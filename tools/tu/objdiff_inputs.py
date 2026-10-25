"""Prepare ELF object pairs for objdiff, without calculating report scores.

Targets contain original disassembly and original TU data. Bases assemble the
verified compiler output against declaration-only scaffold includes: neither
fallback nor standalone assembly earns C credit. IOP pairs use their own pinned
MIPS-I compiler objects and independently assembled original-object references.
All binary inputs remain local.
"""
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from collections import Counter
from bisect import bisect_left
from functools import lru_cache
import hashlib
import ast
import json
import multiprocessing
from pathlib import Path
import re
import struct
import subprocess

import coverage_report as cov
from tu import data_carve
from tu import data_gate
from tu import cinc
from tu.storage_directives import emitted_storage as parse_emitted_storage
from tu import verify_report as verification
from tu.elfinfo import Elf
from tu.toolchain import Toolchain


emitted_storage = lru_cache(maxsize=16)(parse_emitted_storage)
_compiler_only_section = lru_cache(maxsize=32)(data_gate.compiler_only_section)
_compiler_include_views = {}
_object_preparer = None


def _init_object_preparer(preparer):
    global _object_preparer
    _object_preparer = preparer


def _prepare_in_process(obj):
    return _object_preparer(obj)


@lru_cache(maxsize=32)
def source_discovery_view(source):
    """Mask comment/literal contents while preserving every source offset.

    Literal delimiters remain visible so a declaration's trailing whitespace
    cannot consume its initializer. Only the three already supported section
    attribute strings retain their spelling; none contains declaration tokens.
    Decoders always receive the corresponding original initializer slice.
    """
    view=list(source);at=0
    def hide(lo,hi):
        for i in range(lo,hi):
            if view[i]!='\n':view[i]=' '
    while at<len(source):
        if source.startswith('/*',at):
            end=source.find('*/',at+2)
            if end<0:return ''.join('\n' if c=='\n' else ' ' for c in source)
            end+=2;hide(at,end);at=end;continue
        if source.startswith('//',at):
            end=source.find('\n',at+2);end=len(source) if end<0 else end
            # Preprocessed input must have resolved splicing already. Refuse
            # escapes, including GNU continuations with horizontal whitespace,
            # rather than expose comment text as a declaration on the next line.
            if '\\' in source[at:end]:
                return ''.join('\n' if c=='\n' else ' ' for c in source)
            hide(at,end);at=end;continue
        if source[at]=='#':
            start=source.rfind('\n',0,at)+1
            if ''.join(view[start:at]).strip():
                return ''.join('\n' if c=='\n' else ' ' for c in source)
            # A preprocessor filename/metadata token is never a C expression.
            # Parse only enough metadata to blank its complete physical line;
            # unsupported continued or multiline forms refuse all discovery.
            end=at+1
            while end<len(source) and source[end]!='\n':
                if source.startswith('/*',end):
                    close=source.find('*/',end+2)
                    if close<0 or '\n' in source[end:close+2]:
                        return ''.join('\n' if c=='\n' else ' ' for c in source)
                    end=close+2;continue
                if source.startswith('//',end):
                    close=source.find('\n',end+2);close=len(source) if close<0 else close
                    if '\\' in source[end:close]:
                        return ''.join('\n' if c=='\n' else ' ' for c in source)
                    end=close;break
                if source[end] in ('"',"'"):
                    quote=source[end];close=end+1
                    while close<len(source) and source[close]!=quote:
                        if source[close]=='\n' or source[close]=='\\' and source[close+1:close+2] in ('\n','\r'):
                            return ''.join('\n' if c=='\n' else ' ' for c in source)
                        close+=2 if source[close]=='\\' else 1
                    if close>=len(source):return ''.join('\n' if c=='\n' else ' ' for c in source)
                    end=close+1;continue
                if source[end]=='\\':
                    return ''.join('\n' if c=='\n' else ' ' for c in source)
                end+=1
            hide(at,end);at=end;continue
        if source[at] in ('"',"'"):
            quote=source[at];end=at+1
            while end<len(source):
                if source[end]=='\\':end+=2;continue
                if source[end]==quote:break
                end+=1
            if end>=len(source):return ''.join('\n' if c=='\n' else ' ' for c in source)
            if source[at:end+1] not in ('".data"','".sdata"','".rodata"'):hide(at+1,end)
            at=end+1;continue
        if source[at]=='\\':
            # Preprocessed syntax must already have performed line splicing.
            # Do not guess token/comment formation, including GNU horizontal
            # whitespace before a continued newline, from unprocessed escapes.
            return ''.join('\n' if c=='\n' else ' ' for c in source)
        at+=1
    return ''.join(view)


def source_matches(pattern, source, flags=0):
    """Discover syntax lexically, then recover its exact original spelling."""
    pattern=re.compile(pattern,flags) if isinstance(pattern,str) else pattern
    for visible in pattern.finditer(source_discovery_view(source)):
        actual=pattern.match(source,visible.start())
        if actual and actual.end()==visible.end():yield actual


def source_findall(pattern, source, flags=0):
    return [m[0] if not m.re.groups else (m[1] or '') if m.re.groups==1 else tuple(v or '' for v in m.groups())
            for m in source_matches(pattern,source,flags)]


def source_search(pattern, source, flags=0):
    return next(source_matches(pattern,source,flags),None)


@lru_cache(maxsize=32)
def actual_source_asm_lines(preprocessed):
    """Decode actual asm templates with C escapes, after lexical discovery."""
    import tu_audit
    written=set();view=source_discovery_view(preprocessed)
    for match in re.finditer(r'\b(?:__asm__|__asm|asm)\b',view):
        prefix=re.match(r'\s*(?:(?:__volatile__|__volatile|volatile)\s*)?\(\s*',view[match.end():])
        if not prefix:raise RuntimeError('Unsupported lexically present actual asm statement')
        at=match.end()+prefix.end();parts=[]
        while at<len(view) and view[at]=='"':
            end=at+1
            while end<len(preprocessed):
                if preprocessed[end]=='\\':end+=2;continue
                if preprocessed[end]=='"':break
                end+=1
            if end>=len(preprocessed):raise RuntimeError('Unclosed actual asm-template literal')
            parts.append(preprocessed[at:end+1]);at=end+1
            whitespace=re.match(r'\s*',view[at:]);at+=whitespace.end()
        if not parts or at>=len(view) or view[at] not in '):':
            raise RuntimeError('Unsupported actual asm-template expression')
        try:template=b''.join(c_literal_bytes(token) for token in parts).decode('latin1')
        except (ValueError,SyntaxError,UnicodeError) as error:
            raise RuntimeError('Unsupported actual C asm-template literal') from error
        for line in template.splitlines():
            normalized=tu_audit._norm_asm_line(tu_audit._strip_comment(line))
            if normalized:written.add(normalized)
    return frozenset(written)


def compiler_assembly(assembly):
    """Read-only compiler syntax shared with DATA provenance checks."""
    return data_gate._compiler_assembly(assembly)


def compiler_pool_line_numbers(assembly, preprocessed, unit_dir):
    numbers = _compiler_pool_line_numbers(assembly, preprocessed)
    if numbers:
        # External inputs stay live even when their surrounding syntax is reused.
        include = Path(unit_dir) / 'include/labels.inc'
        if (not include.is_file() or hashlib.sha256(include.read_bytes()).hexdigest() !=
            '2cff19a41f1917f2acf5516b3e3f06b69195340442afe65fad70fb0fc694de19'):
            raise RuntimeError('Deferred compiler pool labels include differs')
    return numbers


@lru_cache(maxsize=16)
def _compiler_pool_line_numbers(assembly, preprocessed):
    """Reuse the audit's exact cc1 pool boundary and authored-line veto.

    GCC can defer #NO_APP until after the next function's constant pool. The
    approved source parser records that narrow grammar separately from actual
    top-level asm. This changes only provenance interpretation, never output.
    """
    import tu_audit
    parsed=compiler_assembly(assembly)
    if parsed.errors:raise RuntimeError('cc1 provenance parser errors: '+str(parsed.errors))
    if not parsed.pool_lines:return frozenset()
    lines=assembly.splitlines();first=min(number for number,_ in parsed.pool_lines)
    preceding=[i for i,line in enumerate(lines[:first-1],1) if line.strip() in ('#APP','#NO_APP')]
    if not preceding or lines[preceding[-1]-1].strip()!='#APP':
        raise RuntimeError('Deferred cc1 pool lacks its original APP include boundary')
    begin=preceding[-1]
    prefix=[line.strip() for line in lines[begin:first-1] if line.strip()]
    if (begin!=next((i for i,line in enumerate(lines,1) if line.strip()=='#APP'),None)
        or prefix!=['.include "include/labels.inc"']
        or any(re.match(r'\s*\.ent\b',line) for line in lines[:begin])):
        raise RuntimeError('Unsupported deferred compiler pool APP origin')
    finish=next((i for i in range(first,len(lines)+1) if lines[i-1].strip()=='#NO_APP'),None)
    if not finish or any(not first<=number<finish for number,_ in parsed.pool_lines):
        raise RuntimeError('Unsupported multiple deferred compiler pool regions')
    statements=list(source_matches(r'\b(?:__asm__|__asm|asm)\s*\(\s*("(?:\\.|[^"\\])*")\s*\)\s*;',preprocessed))
    exact=[match for match in statements if c_literal_bytes(match[1])==b'.include "include/labels.inc"\n']
    if len(exact)!=1:raise RuntimeError('Missing unique actual source labels include')
    written=actual_source_asm_lines(preprocessed)
    for _,line in parsed.pool_lines:
        if not tu_audit._app_line_allowed(line) and tu_audit._norm_asm_line(line) in written:
            raise RuntimeError('Authored asm line in deferred cc1 constant pool')
    return frozenset(number for number,_ in parsed.pool_lines)


def outside_app_section_payload(assembly, section, preprocessed='', unit_dir=None):
    """Refuse direct authored payload in the witnessed compiler section."""
    pool_lines=compiler_pool_line_numbers(assembly,preprocessed,unit_dir) if preprocessed and unit_dir is not None else frozenset()
    return _outside_app_section_payload(assembly, section, pool_lines)


@lru_cache(maxsize=32)
def _outside_app_section_payload(assembly, section, pool_lines):
    current=previous=None;app=False
    for number,line in enumerate(assembly.splitlines(),1):
        if line.strip()=='#APP':app=True;continue
        if line.strip()=='#NO_APP':app=False;continue
        line=line.split('#',1)[0]
        selected=re.match(r'\s*\.(data|sdata|rdata|rodata|lit4|lit8|text)\s*$',line)
        explicit=re.match(r'\s*\.section\s+"?(\.[\w.]+)"?(?:\s|,|$)',line)
        if app and re.match(r'\s*\.(?:section|data|sdata|rdata|rodata|lit4|lit8|text)\b',line) and not (selected or explicit):
            raise RuntimeError('Unsupported authored section selection in compiler provenance')
        if selected or explicit:
            previous,current=current,'.'+selected[1] if selected else explicit[1]
            if current=='.rdata':current='.rodata'
            continue
        if re.match(r'\s*\.previous\s*$',line):current,previous=previous,current;continue
        if app and re.match(r'\s*\.(?:pushsection|popsection)\b',line):
            raise RuntimeError('Unsupported authored section stack in compiler provenance')
        if app and number not in pool_lines and current==section and re.match(r'\s*\.(?:byte|half|short|word|float|double|quad|dword|ascii|asciz|string|space|zero|incbin|fill|org|balign|p2align|align)\b',line):
            raise RuntimeError('Authored APP payload in compiler section '+section)
    if app:raise RuntimeError('Unclosed APP provenance region')
    return True


def compiler_only_section(assembly, preprocessed, unit_dir, section):
    # Includes have external file dependencies: always re-check their contents.
    # Without includes the proof depends only on these immutable text inputs.
    outside_app_section_payload(assembly,section,preprocessed,unit_dir)
    if re.search(r'\.include\b', assembly):
        view = _compiler_include_views.get(assembly, unit_dir)
        return data_gate.compiler_only_section(assembly, preprocessed, view, section)
    return _compiler_only_section(assembly, preprocessed, unit_dir, section)


@lru_cache(maxsize=64)
def parsed_elf(data):
    """Reuse read-only parsing by exact bytes, never by a potentially stale path."""
    return Elf(data)


@lru_cache(maxsize=6)
def original_symbol_indexes(data):
    """Index the same original-symbol candidates used by joint assembly."""
    by_address, by_name, names = {}, {}, set()
    for symbol in parsed_elf(data).symbols:
        names.add(symbol.name)
        if symbol.type in (0, 1):
            by_address.setdefault((symbol.shndx, symbol.value), []).append(symbol)
        if symbol.shndx not in (0, 0xfff1):
            spelling = symbol.name.replace('.', '_')
            for name in (symbol.name, spelling, spelling + f'_{symbol.value:08X}'):
                by_name.setdefault(name, set()).add(symbol.value)
    return by_address, by_name, names


def c_literal_bytes(spelling):
    """Decode a narrow C/assembler literal, including C's escaped question mark."""
    if len(spelling) < 2 or spelling[0] not in ('"', "'") or spelling[-1] != spelling[0]:
        raise ValueError('expected a quoted byte string')
    escapes = {'a': 7, 'b': 8, 'f': 12, 'n': 10, 'r': 13, 't': 9, 'v': 11,
               '\\': 92, "'": 39, '"': 34, '?': 63}
    out = bytearray()
    at, end = 1, len(spelling) - 1
    while at < end:
        char = spelling[at]
        at += 1
        if char != '\\':
            if char in (spelling[0], '\n', '\r'):
                raise ValueError('unescaped string delimiter or newline')
            out.extend(char.encode('latin1'))
            continue
        if at == end:
            raise ValueError('unterminated escape')
        char = spelling[at]
        at += 1
        if char in escapes:
            out.append(escapes[char])
        elif char in '01234567':
            digits = char
            while at < end and len(digits) < 3 and spelling[at] in '01234567':
                digits += spelling[at]
                at += 1
            out.append(int(digits, 8))
        elif char == 'x':
            start = at
            while at < end and spelling[at] in '0123456789abcdefABCDEF':
                at += 1
            if at == start:
                raise ValueError('empty hexadecimal escape')
            out.append(int(spelling[start:at], 16))
        elif char == '\n':
            continue
        else:
            raise ValueError('unsupported byte-string escape: ' + char)
    return bytes(out)


def run(argv, cwd):
    subprocess.run(['timeout', '-k', '5', '30', *map(str, argv)], cwd=cwd,
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def allocate_registered_layout(base, assembly, preprocessed, registry, output,
                               linker, run, cwd):
    from tu.elfinfo import Elf
    before = parsed_elf(base.read_bytes())
    compiler_only_section(assembly, preprocessed, cwd, '.sbss')
    storage = emitted_storage(assembly)
    assert storage.get('layoutManagerId', {}).get('directive') == 'comm'
    assert storage['layoutManagerId']['size'] == 4
    assert storage['layoutManagerId']['alignment'] == 4
    assert storage.get('defaultLayout', {}).get('size') == 4
    assert storage['defaultLayout']['section'] == '.sbss'
    assert storage['defaultLayout']['extent_kind'] == 'label-space'
    common = [s for s in before.symbols if s.shndx in (0xfff2, 0xff03)]
    assert len(common) == 1
    assert (common[0].name, common[0].value, common[0].size, common[0].bind,
            common[0].type, common[0].shndx) == ('layoutManagerId', 4, 4, 1, 1, 0xff03)
    runs = registry.get('.sbss', [])
    assert len(runs) == 1
    run_record = runs[0]
    assert run_record['range'] == ['0x004dc6b0', '0x004dc6b8']
    assert set(run_record['c_owned_symbols']) == {'defaultLayout', 'layoutManagerId'}
    spans = run_record['c_input_spans']
    assert sorted((s['section'], tuple(s['object_range']), tuple(s['range']),
                   tuple(s['symbols'])) for s in spans) == [
        ('.sbss', (0, 4), ('0x004dc6b0', '0x004dc6b4'), ('defaultLayout',)),
        ('.scommon', (0, 4), ('0x004dc6b4', '0x004dc6b8'), ('layoutManagerId',))]
    allocated = output.with_name('ordinary-allocated.o')
    run([linker, '-r', '-d', '-EL', '-m', 'elf32lr5900', '-o', allocated, base], cwd)
    native = parsed_elf(allocated.read_bytes())
    # GNU ld's own relocatable link adjusts local GPREL16 addends when it
    # changes the provisional GP in REGINFO. Prove their ABI equivalence;
    # never rewrite either object's words or relocation records ourselves.
    def text_relocations(image):
        text = image.section('.text')
        result = {}
        for rel in image.sections:
            if rel.type != 9 or rel.info != text.index:
                continue
            assert rel.entsize == 8 and rel.size % 8 == 0
            for offset in range(rel.offset, rel.offset + rel.size, 8):
                address, info = struct.unpack_from('<II', image.data, offset)
                symbol = image.symbols[info >> 8]
                name = (image.sections[symbol.shndx].name if symbol.type == 3
                        else symbol.name)
                result[address] = (info & 255, name)
        return result
    reloc_before, reloc_native = text_relocations(before), text_relocations(native)
    assert reloc_before == reloc_native
    before_gp = struct.unpack_from('<I', before.section_bytes(before.section('.reginfo')), 20)[0]
    native_gp = struct.unpack_from('<I', native.section_bytes(native.section('.reginfo')), 20)[0]
    old_text = before.section_bytes(before.section('.text'))
    new_text = native.section_bytes(native.section('.text'))
    assert len(old_text) == len(new_text)
    adjustments = []
    signed = lambda v: v - 65536 if v & 32768 else v
    for at in range(0, len(old_text), 4):
        old, new = struct.unpack_from('<I', old_text, at)[0], struct.unpack_from('<I', new_text, at)[0]
        if old == new:
            continue
        assert reloc_before.get(at, (None,))[0] in (7, 8)  # GPREL16 / LITERAL
        assert old & 0xffff0000 == new & 0xffff0000
        assert signed(old & 65535) + before_gp == signed(new & 65535) + native_gp
        adjustments.append(dict(offset=at, relocation=reloc_before[at],
                                original_word=hex(old), allocated_word=hex(new)))
    owner = [s for s in native.symbols if s.name == 'layoutManagerId']
    assert len(owner) == 1 and owner[0].size == 4
    assert native.sections[owner[0].shndx].name == '.scommon'
    assert owner[0].value == 0
    script = output.with_suffix('.ld')
    script.write_text('SECTIONS { .sbss 0 : { *(.sbss) *(.scommon) } }\n')
    run([linker, '-r', '-EL', '-m', 'elf32lr5900', '-T', script,
         '-o', output, allocated], cwd)
    after = parsed_elf(output.read_bytes())
    assert after.section('.sbss').size == 8
    assert after.section('.sbss').type == 8
    assert not any(s.shndx in (0xfff2, 0xff03) for s in after.symbols)
    functions = lambda e: {(s.name, s.value, s.size, s.type, s.bind)
                            for s in e.symbols if s.type == 2}
    assert functions(before) == functions(after)
    for section in native.sections:
        if not section.flags & 2 or section.name in ('.sbss', '.scommon'):
            continue
        selected = after.section(section.name)
        assert (section.type, section.flags, section.size, section.align) == (
            selected.type, selected.flags, selected.size, selected.align)
        assert native.section_bytes(section) == after.section_bytes(selected), section.name
    positions = {s.name: (after.sections[s.shndx].name, s.value, s.size, s.type, s.bind)
                 for s in after.symbols if s.name in ('defaultLayout', 'layoutManagerId')}
    assert positions == {'defaultLayout': ('.sbss', 0, 0, 0, 0),
                         'layoutManagerId': ('.sbss', 4, 4, 1, 1)}
    return {'normal_common_allocation': str(allocated), 'registered_route': str(script),
            'owner_positions': positions, 'all_functions_and_other_allocated_bytes_unchanged_by_routing': True,
            'ordinary_LD_GP_adjustments': adjustments,
            'provisional_GP_before': hex(before_gp), 'provisional_GP_allocated': hex(native_gp),
            'SOURCE_unchanged': True, 'TARGET_unchanged': True}


def registered_data_symbol_mappings(target, base, original, assembly, preprocessed,
                                    registry, tu, cwd):
    """Pair accepted C DATA names using original identities and affine storage.

    This supplies native objdiff mappings only. Every initializer is resolved
    diagnostically through actual cc1 relocations and compared with retail bytes;
    neither input object nor the native report is rewritten.
    """
    compiled=parsed_elf(base.read_bytes());reference=parsed_elf(target.read_bytes())
    emitted=typed_compiler_data(assembly);mappings={};proofs=[]
    spans=[(section,span) for section,runs in registry.items() if isinstance(runs,list)
           for row in runs for span in row.get('c_input_spans',[])
           if explicit_object_range(span) and len(span.get('range',[]))==2
           and span.get('credit')!='uncredited_dependency' and not span.get('uncredited_dependency')]
    def transport(section,offset):
        values=set()
        for output,span in spans:
            bounds=explicit_object_range(span);coords=[int(v,16) for v in span['range']]
            if span['section']==section and bounds[0]<=offset<bounds[1] and bounds[1]-bounds[0]==coords[1]-coords[0]:
                values.add(coords[0]+offset-bounds[0])
        return next(iter(values)) if len(values)==1 else None
    def address(symbol,addend):
        if 0<symbol.shndx<len(compiled.sections):
            section=compiled.sections[symbol.shndx]
            if section.name!='.text':return transport(section.name,symbol.value+addend)
            functions=[f for f in compiled.symbols if f.type==2 and f.shndx==section.index and f.value<=symbol.value+addend<f.value+f.size]
            if len(functions)!=1:return None
            function=functions[0]
            originals=[s for s in original.symbols if s.name==function.name and s.type==2 and s.size==function.size and s.shndx!=0]
            return originals[0].value+symbol.value+addend-function.value if len(originals)==1 else None
        if symbol.shndx==0xfff1:return symbol.value+addend
        if symbol.shndx==0:
            found=[s.value+addend for s in original.symbols if s.shndx!=0 and symbol.name in
                   (s.name,s.name.replace('.','_'),s.name.replace('.','_')+f'_{s.value:08X}')]
            if len(set(found))==1:return found[0]
        return None
    for section_name,runs in registry.items():
        if not isinstance(runs,list) or section_name not in ('.data','.sdata','.rodata') or not any(r.get('c_storage_aliases') for r in runs):continue
        try:compiler_only_section(assembly,preprocessed,cwd,section_name)
        except (RuntimeError,ValueError):continue
        original_section=original.section(section_name)
        for row in runs:
            for alias in row.get('c_storage_aliases',[]):
                owner=alias.get('storage_owner',{});name=owner.get('name');va=int(alias['address'],16)
                candidates=[s for s in compiled.symbols if s.name==name and s.type==1 and 0<s.shndx<len(compiled.sections)]
                if len(candidates)!=1:continue
                symbol=candidates[0];section=compiled.sections[symbol.shndx];storage=emitted.get(name)
                if section.name!=section_name or symbol.size!=owner.get('size') or not storage:continue
                if (storage['section'],storage['size'])!=(section_name,symbol.size):continue
                if transport(section_name,symbol.value)!=va or transport(section_name,symbol.value+symbol.size-1)!=va+symbol.size-1:continue
                original_name=alias['original_name']
                retail=[s for s in original.symbols if s.name==original_name and s.value==va and s.shndx==original_section.index]
                if retail:
                    if len(retail)!=1 or retail[0].type!=1 or (retail[0].size,retail[0].bind)!=(symbol.size,symbol.bind):continue
                elif original_name!='D_'+f'{va:08X}':continue
                target_names=(original_name,original_name.replace('.','_'),original_name.replace('.','_')+f'_{va:08X}')
                mapped=tu.get('sections',{}).get(section_name)
                if not mapped:continue
                targets=[s for s in reference.symbols if s.name in target_names and s.type==1 and s.size==symbol.size
                         and 0<s.shndx<len(reference.sections) and reference.sections[s.shndx].name==section_name
                         and s.value==va-int(mapped['start'],16)]
                if len(targets)!=1 or (retail and targets[0].bind!=symbol.bind):continue
                value=bytearray(compiled.section_bytes(section)[symbol.value:symbol.value+symbol.size]);resolved=[];valid=True
                for reloc in compiled.sections:
                    if reloc.info!=section.index or reloc.type not in (4,9):continue
                    if reloc.type!=9 or reloc.entsize!=8 or reloc.size%8:
                        valid=False;break
                    for offset in range(reloc.offset,reloc.offset+reloc.size,reloc.entsize):
                        place,info=struct.unpack_from('<II',compiled.data,offset)
                        if not symbol.value<=place<symbol.value+symbol.size:continue
                        if info&255!=2 or info>>8>=len(compiled.symbols) or place+4>symbol.value+symbol.size:valid=False;break
                        relative=place-symbol.value;addend=struct.unpack_from('<I',value,relative)[0]
                        pointer=address(compiled.symbols[info>>8],addend)
                        if pointer is None:valid=False;break
                        struct.pack_into('<I',value,relative,pointer&0xffffffff)
                        resolved.append(dict(offset=relative,original_address=pointer))
                expected=original.section_bytes(original_section)[va-original_section.addr:va-original_section.addr+symbol.size]
                if not valid or bytes(value)!=expected:continue
                mappings[targets[0].name]=name
                proofs.append(dict(target_name=targets[0].name,c_name=name,registered_alias=alias,
                    cc1_storage=storage,original_payload_sha256=hashlib.sha256(expected).hexdigest(),
                    interpreted_real_R_MIPS32=resolved,original_initializer_exact=True,
                    input_objects_unchanged=True))
    return mappings,proofs


def compile_source(root, unit_dir, source, output, ccdir, assembler, flags, skip_asm,
                   verified_assembly=None):
    common = [ccdir / 'ee-gcc', f'-B{ccdir}/', '-nostdinc', '-fno-builtin']
    preprocessed, assembly = Path(str(output) + '.i'), Path(str(output) + '.s')
    if verified_assembly is not None:
        # Preserve the actual compiler context, including file-scope asm.
        # Even empty replacement markers can change GCC 2.96's C emission.
        assembly.write_bytes(verified_assembly.read_bytes())
        preprocessed.write_bytes(Path(str(verified_assembly)[:-2] + '.i').read_bytes())
    else:
        run([*common, '-E', *flags, '-Iinclude', '-I.',
             f'-I{root / "include"}', f'-I{source.parent}', source, '-o', preprocessed], unit_dir)
        run([*common, '-S', *flags, preprocessed, '-o', assembly], unit_dir)
    cwd = unit_dir
    if skip_asm:
        # Assemble unchanged cc1 output with no included fallback bodies.
        # Keep only the extern declarations that the normal cinc rule retains:
        # they carry assembler ABI information, but emit no code or storage.
        body = assembly.read_text()
        declared = set(re.findall(r'^\s*\.extern\s+([A-Za-z_.$][\w.$]*)\s*,', body, re.M))
        declared.update(re.findall(r'^([A-Za-z_.$][\w.$]*):', body, re.M))
        declared.update(re.findall(r'^\s*\.(?:l?comm)\s+([A-Za-z_.$][\w.$]*)\s*,', body, re.M))
        cwd = output.parent / 'compiler-includes'
        cwd.mkdir(exist_ok=True)
        includes = {}
        for name in cinc.INCLUDE.findall(body):
            if not name.endswith('.s'):
                continue
            relative = Path(name)
            verification.require(not relative.is_absolute() and '..' not in relative.parts,
                                 'nonlocal compiler scaffold include: ' + name)
            original = unit_dir / relative
            declarations = [line for line in original.read_text().splitlines()
                            if cinc.EXTERN.match(line) and cinc.EXTERN.match(line)[1] not in declared]
            local = cwd / relative
            local.parent.mkdir(parents=True, exist_ok=True)
            local.write_text('\n'.join(declarations) + '\n')
            includes[name] = dict(original_sha256=verification.digest(original),
                                  declaration_only_sha256=verification.digest(local))
        _compiler_include_views[body] = cwd
        output.with_suffix('.compiler-only.proof.json').write_text(json.dumps(dict(
            compiler_assembly=str(verified_assembly) if verified_assembly else str(assembly),
            compiler_assembly_sha256=verification.digest(assembly),
            preprocessed_sha256=verification.digest(preprocessed),
            compiler_output_unchanged=True, scaffold_includes=includes,
            scaffold_code_and_storage_emitted=False), indent=2) + '\n')
    gflag = next(flag for flag in flags if flag.startswith('-G'))
    run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, '-I.',
         f'-I{unit_dir}', f'-I{unit_dir / "include"}', '-o', output, assembly], cwd)
    return assembly


def build_target(unit_dir, stem, unit):
    candidates = ([f'build/expected/{stem}.o', f'build/hasm/{stem}.o']
                  if unit == 'main' else [f'build/src/{unit}/{stem}.o',
                                         f'build/scaffold/src/{unit}/{stem}.o'])
    path = next((unit_dir / p for p in candidates if (unit_dir / p).is_file()), None)
    verification.require(path is not None, f'{unit}/{stem}: missing verified build object')
    return path


def functions(path):
    return {s.name: s.size for s in parsed_elf(path.read_bytes()).symbols
            if s.type == 2 and s.shndx not in (0, 0xfff1) and s.size}


@lru_cache(maxsize=32)
def typed_compiler_data(assembly):
    """Read cc1 OBJECT directives, including its standard .rdata alias.

    This changes only parser spelling. The actual assembler input and ELF
    bytes stay untouched; consumers also require the actual ELF .rodata.
    """
    from tu.mapcheck import compiler_data_definitions
    parsed=re.sub(r'^\s*\.rdata\s*$', '.rodata',assembly,flags=re.M)
    return compiler_data_definitions(parsed)


def explicit_object_range(span):
    """Accept only two explicit, valid compiler-object coordinates.

    Historical spans without object coordinates cannot prove a source owner.
    Do not infer coordinates from another range or substitute a zero offset.
    """
    coordinates=span.get('object_range') if isinstance(span,dict) else None
    if (not isinstance(coordinates,(list,tuple)) or len(coordinates)!=2 or
        any(type(value) is not int for value in coordinates) or
        not 0<=coordinates[0]<coordinates[1]):return None
    return coordinates


def nested_base_names(root, tu_id, allowed, actual):
    """Original symbol -> C object name for registered GNU nested functions cc1 renumbered.

    cc1 numbers a nested function `name.N` by the private names emitted before
    it, so the C object can call it `name.M`. Only the registered mapping
    (tools/tu_edit.gnu_nested_specs, as tools/tu/mapcheck.py and
    tools/tu/verify_report.py use it) qualifies, with exactly one candidate.
    """
    import tu_edit
    found = {}
    for spec in tu_edit.gnu_nested_specs(root, tu_id).values():
        for item in spec.get('nested_functions') or []:
            original = item.get('original_name')
            if original not in allowed or original in actual:
                continue
            pattern = re.escape(item['source_name']) + r'\.[0-9]+'
            names = [n for n in actual if re.fullmatch(pattern, n) and n not in allowed]
            if len(names) == 1:
                found[original] = names[0]
    return found


def text_relocations(elf, section):
    """Return section-relative REL/RELA sites, refusing malformed tables."""
    offsets = []
    for rel in elf.sections:
        if rel.type not in (4, 9) or rel.info != section.index:
            continue
        width = 12 if rel.type == 4 else 8
        verification.require(rel.entsize == width and rel.size % width == 0 and
                             0 <= rel.offset <= len(elf.data) - rel.size,
                             'malformed text relocation table')
        for at in range(rel.offset, rel.offset + rel.size, width):
            offset = struct.unpack_from('<I', elf.data, at)[0]
            if elf.e_type != 1:
                offset -= section.addr
            verification.require(0 <= offset <= section.size - 4,
                                 'text relocation outside section')
            offsets.append(offset)
    return offsets


def unrelocated_text_functions(original, ordinary, base, scope):
    """Select by verified identity and relocation absence, never operand values.

    A final ELF cannot establish whether a prelink address was symbolic. Keep
    symbolic references for compiler functions carrying any text relocation.
    For functions with no relocations in either actual compiler view, fixed
    original runtime operands provide the appropriate comparison reference.
    This decision reads no compiler instruction bytes or similarity scores.
    """
    selected = []
    for row in scope:
        target = verification.original_symbol(original, row['original_symbol'],
                                               row['size'], row['va'])
        coordinates = []
        for compiled in (ordinary, base):
            symbols = [s for s in compiled.symbols if s.name == target.name and
                       (s.type, s.bind, s.size) == (2, target.bind, target.size) and
                       0 < s.shndx < len(compiled.sections)]
            if len(symbols) != 1:
                break
            symbol = symbols[0]
            section = compiled.sections[symbol.shndx]
            lo = symbol.value - (section.addr if compiled.e_type != 1 else 0)
            verification.require(section.name == '.text' and section.flags & 4 and
                                 0 <= lo < lo + symbol.size <= section.size,
                                 'verified compiler function outside text section')
            sites = text_relocations(compiled, section)
            if any(lo < site + 4 and site < lo + symbol.size for site in sites):
                break
            coordinates.append(lo)
        if len(coordinates) == 2:
            selected.append(dict(name=target.name, address=target.value, size=target.size,
                                 binding=target.bind, compiler_offsets=coordinates))
    return selected


def text_relocation_identities(elf, section):
    """Retain relocation identity even when reassembly renumbers symbols."""
    text_relocations(elf, section)
    verification.require(sum(s.type == 2 for s in elf.sections) == 1,
                         'reference requires one unambiguous symbol table')
    records = []
    for rel in elf.sections:
        if rel.type not in (4, 9) or rel.info != section.index:
            continue
        verification.require(0 < rel.link < len(elf.sections) and elf.sections[rel.link].type == 2,
                             'text relocation has no symbol table')
        for at in range(rel.offset, rel.offset + rel.size, rel.entsize):
            offset, info = struct.unpack_from('<II', elf.data, at)
            symbols = [s for s in elf.symbols if s.index == info >> 8]
            verification.require(len(symbols) == 1, 'ambiguous text relocation symbol')
            symbol = symbols[0]
            identity = (symbol.name, symbol.type, symbol.bind, symbol.value, symbol.size,
                        elf.sections[symbol.shndx].name if 0 < symbol.shndx < len(elf.sections)
                        else symbol.shndx)
            addend = struct.unpack_from('<i', elf.data, at + 8)[0] if rel.type == 4 else None
            records.append((offset, info & 255, identity, addend))
    return records


def numeric_data_addresses(body, original, scope):
    """Decode only authenticated contiguous LUI/ADDU/load DATA constructors.

    The named original object, extent, carry and complete register flow must
    agree with retail bytes. No compiler operand supplies a replacement word.
    """
    instruction = re.compile(r'^(\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+'
                             r'([\da-fA-F]{8})\s+\*/\s*)(\w+)([^\n]*)$', re.M)
    address = re.compile(r'%(hi|lo)\(([A-Za-z_.$][\w.$]*)(?:\s*\+\s*(0x[\da-fA-F]+|\d+))?\)')
    rows = list(instruction.finditer(body))
    edits, proofs = [], []
    for index, high in enumerate(rows[:-2]):
        expr = address.search(high[5])
        if not expr or expr[1] != 'hi':
            continue
        va = int(high[2], 16)
        owners = [r for r in scope if r['address'] <= va and va + 12 <= r['address'] + r['size']]
        if len(owners) != 1:
            continue
        owner = owners[0]
        add, low = rows[index + 1:index + 3]
        low_expr = address.search(low[5])
        if (not low_expr or low_expr[1] != 'lo' or
                (expr[2], expr[3]) != (low_expr[2], low_expr[3]) or
                int(add[2], 16) != va + 4 or int(low[2], 16) != va + 8):
            continue
        # Labels/directives between these three instructions could add an
        # entry point. Only whitespace may separate the constructor's rows.
        if body[high.end():add.start()].strip() or body[add.end():low.start()].strip():
            continue
        symbols = [s for s in original.symbols if s.type == 1 and s.size and
                   s.name.replace('.', '_') == expr[2] and
                   0 < s.shndx < len(original.sections)]
        if len(symbols) != 1:
            continue
        symbol = symbols[0]
        data_section = original.sections[symbol.shndx]
        addend = int(expr[3] or '0', 0)
        # Overlay ELFs put code and DATA in one executable section. Require
        # an actual OBJECT extent disjoint from every original FUNC instead
        # of treating that container's executable flag as a DATA classifier.
        if (not data_section.flags & 2 or
                not data_section.addr <= symbol.value < symbol.value + symbol.size <= data_section.addr + data_section.size or
                not 0 <= addend < symbol.size or
                any(s.type == 2 and s.size and s.shndx == symbol.shndx and
                    symbol.value < s.value + s.size and s.value < symbol.value + symbol.size
                    for s in original.symbols)):
            continue
        section = original.sections[next(s.shndx for s in original.symbols
            if s.name == owner['name'] and s.value == owner['address'] and s.type == 2)]
        words = []
        for line in (high, add, low):
            at = int(line[2], 16)
            raw = original.section_bytes(section)[at - section.addr:at - section.addr + 4]
            verification.require(len(raw) == 4 and raw.hex().lower() == line[3].lower(),
                                 'stale original DATA constructor bytes')
            words.append(int.from_bytes(raw, 'little'))
        hi, middle, lo = words
        reg = (hi >> 16) & 31
        rs, rt, rd = (middle >> 21) & 31, (middle >> 16) & 31, (middle >> 11) & 31
        if (high[4] != 'lui' or hi >> 26 != 15 or (hi >> 21) & 31 or not reg or
                add[4] != 'addu' or middle >> 26 or middle & 0x7ff != 0x21 or rd != reg or
                (rs == reg) == (rt == reg) or not (rt if rs == reg else rs) or
                lo >> 26 not in (0x23, 0x1a, 0x1b) or
                low[4] != {0x23: 'lw', 0x1a: 'ldl', 0x1b: 'ldr'}[lo >> 26] or
                (lo >> 21) & 31 != reg):
            continue
        # The old base must die; otherwise a later low relocation could depend
        # on its HI16. Accept overwrite-by-load, next LUI, or an authenticated
        # tail which never reads the assembler scratch register again.
        if (lo >> 16) & 31 != reg:
            if index + 3 >= len(rows):
                continue
            after = rows[index + 3]
            if (int(after[2], 16) != va + 12 or va + 16 > owner['address'] + owner['size'] or
                    body[low.end():after.start()].strip()):
                continue
            raw = original.section_bytes(section)[va + 12 - section.addr:va + 16 - section.addr]
            verification.require(raw.hex().lower() == after[3].lower(), 'stale original DATA base overwrite')
            word = int.from_bytes(raw, 'little')
            if after[4] != 'lui' or word >> 26 != 15 or (word >> 21) & 31 or (word >> 16) & 31 != reg:
                if reg != 1:
                    continue
                tail = rows[index + 3:]
                previous, at, valid, returned = low, va + 12, True, False
                for line in tail:
                    if at >= owner['address'] + owner['size']:
                        break
                    if int(line[2], 16) != at or body[previous.end():line.start()].strip():
                        valid = False
                        break
                    raw = original.section_bytes(section)[at - section.addr:at - section.addr + 4]
                    verification.require(raw.hex().lower() == line[3].lower(), 'stale original DATA tail')
                    word = int.from_bytes(raw, 'little')
                    op, rs_tail, rt_tail = word >> 26, (word >> 21) & 31, (word >> 16) & 31
                    if op == 15:  # lui: no input register
                        reads = []
                        writes = rt_tail
                    elif op == 0 and word & 63 in (0x21, 0x2d):  # addu/daddu
                        reads = [rs_tail, rt_tail]
                        writes = (word >> 11) & 31
                    elif op == 0 and word & 63 == 8:  # jr
                        if rs_tail != 31 or at != owner['address'] + owner['size'] - 8:
                            valid = False
                            break
                        returned = True
                        reads = [rs_tail]
                        writes = 0
                    elif op in (0x23, 0x1a, 0x1b):  # lw/ldl/ldr
                        reads = [rs_tail] + ([rt_tail] if op != 0x23 else [])
                        writes = rt_tail
                    elif op in (0x2b, 0x2c, 0x2d):  # sw/sdl/sdr
                        reads = [rs_tail, rt_tail]
                        writes = 0
                    else:
                        valid = False
                        break
                    if reg in reads or writes == 31:
                        valid = False
                        break
                    previous, at = line, at + 4
                if not valid or not returned or at != owner['address'] + owner['size']:
                    continue
        value = symbol.value + addend
        signed = (lo & 0xffff) - (0x10000 if lo & 0x8000 else 0)
        if (hi & 0xffff != ((value + 0x8000) >> 16) & 0xffff or
                lo & 0xffff != value & 0xffff or ((hi & 0xffff) << 16) + signed != value):
            continue
        sites = text_relocations(original, section)
        if any(at - section.addr < site + 4 and site < at - section.addr + 4
               for at in (va, va + 8) for site in sites):
            continue
        for line, expression, immediate in ((high, expr, f'0x{hi & 0xffff:X}'),
                                             (low, low_expr, str(signed))):
            edits.append((line.start(5) + expression.start(), line.start(5) + expression.end(), immediate))
        proofs.append(dict(function=owner, address=va, original_words=words,
                           original_object=symbol.name, object_address=symbol.value,
                           object_size=symbol.size, object_binding=symbol.bind, addend=addend,
                           changed_sites=[va, va + 8], original_relocations_absent=True))
    for lo, hi, replacement in reversed(sorted(edits)):
        body = body[:lo] + replacement + body[hi:]
    return body, proofs


def original_text(reference, original_path, start, end, destination, cwd, obj, assembler, flags,
                  ordinary=None, base=None, scope=()):
    """Keep original numeric operands where the retail ELF has no relocation.

    Turning a literal load into an external symbol invents GPREL16, whereas
    cc1 may emit LITERAL for the same final instruction. The retail binary
    cannot establish either type. Disassemble these operands as their actual
    numeric GP offsets instead; native objdiff report defaults handle an
    unrelocated reference against either compiler representation. Keep ordinary
    symbolic branches so objdiff still resolves object-relative jump targets.

    The disassembler also names some plain constants as symbols: `lui $3,
    %hi(D_FFFF)` / `addiu $3, $3, %lo(D_FFFF)` loads 65535, not an address. A
    `D_<address>` operand outside every loaded section of the original is a
    numeric operand, including hardware addresses and packed register values.
    It keeps its original immediate, so the reference carries no invented
    relocation the C object cannot have.
    This local report assembly never replaces build scaffolding or C source.
    """
    original = parsed_elf(original_path.read_bytes())
    scaffold = parsed_elf(reference.read_bytes())
    text = scaffold.section('.text')
    padded_end = (end + text.align - 1) // text.align * text.align
    verification.require(start + text.size <= padded_end,
                         f'{obj["id"]}: reference exceeds TU text alignment')
    section = next((s for s in original.sections if s.type != 8 and s.flags & 2
                    and s.addr <= start and start + text.size <= s.addr + s.size), None)
    verification.require(section is not None, f'{obj["id"]}: missing original text extent')
    labels = {s.name: s for s in scaffold.symbols if s.shndx == text.index and s.type == 2 and s.size}
    for name, va, size in obj['functions']:
        target = verification.original_symbol(original, name, size, va)
        label = labels.get(name) or labels.get(target.name)
        verification.require(label is not None and label.value == va - start and label.size == size,
                             f'{obj["id"]}/{name}: reference function boundary differs from original')
    offset = section.offset + start - section.addr
    if start + text.size > end:
        verification.require(not any(original.data[offset + end - start:offset + text.size]) and
                             not any(s.size and end <= s.value < start + text.size
                                     for s in original.symbols if s.shndx == section.index),
                             f'{obj["id"]}: reference tail is not original alignment padding')
    literal_sections = [s for s in original.sections if s.name in ('.lit4', '.lit8')]
    gp = original.gp
    literal_operands = bool(gp and literal_sections)
    loaded = [s for s in original.sections if s.flags & 2 and s.size]
    assembly = destination.with_suffix('.s')
    body = Path(str(reference) + '.s').read_text()
    expected = bytearray(scaffold.section_bytes(text))
    changes = []
    selected = (unrelocated_text_functions(original, parsed_elf(ordinary.read_bytes()),
                parsed_elf(base.read_bytes()), scope) if ordinary is not None and base is not None else [])
    data_proofs = []
    instruction = re.compile(r'^(\s*/\*\s+\S+\s+([0-9A-Fa-f]{8})\s+'
                             r'([0-9A-Fa-f]{8})\s+\*/\s*)(lwc1|ldc1)\s+(\$f\d+),.*$', re.M)
    constant = re.compile(r'^(\s*/\*\s+\S+\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+\*/\s*)'
                          r'(\S+)(\s.*?)%(hi|lo)\((D_([0-9A-Fa-f]+))\)(.*)$', re.M)

    def include(match):
        path = cwd / match[1]
        def original_word(line):
            va = int(line[2], 16)
            verification.require(start <= va <= start + text.size - 4,
                                 f'{obj["id"]}: disassembly instruction outside original TU')
            data = original.data[offset + va - start:offset + va - start + 4]
            verification.require(data.hex().lower() == line[3].lower(), 'stale original disassembly bytes')
            return va, data, int.from_bytes(data, 'little')

        def numeric_constant(line):
            value = int(line[8], 16)
            if any(s.addr <= value < s.addr + s.size for s in loaded):
                return line[0]
            va, data, word = original_word(line)
            if word >> 26 in (0x0c, 0x0d, 0x0e, 0x0f):          # andi, ori, xori, lui
                immediate = f'0x{word & 0xffff:X}'
            else:
                immediate = str((word & 0xffff) - (0x10000 if word & 0x8000 else 0))
            changes.append(va - start)
            expected[va - start:va - start + 4] = data
            return f'{line[1]}{line[4]}{line[5]}{immediate}{line[9]}'

        def operand(line):
            if not literal_operands:
                return line[0]
            va = int(line[2], 16)
            verification.require(start <= va <= start + text.size - 4,
                                 f'{obj["id"]}: disassembly instruction outside original TU')
            data = original.data[offset + va - start:offset + va - start + 4]
            verification.require(data.hex().lower() == line[3].lower(), 'stale original disassembly bytes')
            word = int.from_bytes(data, 'little')
            immediate = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
            address = gp + immediate
            if (word >> 21) & 31 != 28 or not any(s.addr <= address < s.addr + s.size
                                                 for s in literal_sections):
                return line[0]
            verification.require(word >> 26 == {'lwc1': 0x31, 'ldc1': 0x35}[line[4]] and
                                 (word >> 16) & 31 == int(line[5][2:]),
                                 'literal load mnemonic/register differs from original bytes')
            changes.append(va - start)
            expected[va - start:va - start + 4] = data
            return f'{line[1]}{line[4]} {line[5]}, {immediate}($gp)'
        old = path.read_text()
        new, proofs = numeric_data_addresses(old, original, selected)
        for proof in proofs:
            for va in proof['changed_sites']:
                changes.append(va - start)
                expected[va - start:va - start + 4] = original.data[offset + va - start:offset + va - start + 4]
        data_proofs.extend(proofs)
        new = constant.sub(numeric_constant, instruction.sub(operand, new))
        if new == old:
            return match[0]
        local = destination.parent / 'original-asm' / path.name
        local.parent.mkdir(exist_ok=True)
        local.write_text(new)
        return f'.include {json.dumps(str(local))}'

    body = re.sub(r'\.include\s+"(asm/[^\"]+\.s)"', include, body)
    if not changes:
        return reference
    assembly.write_text(body)
    gflag = next(flag for flag in flags if flag.startswith('-G'))
    run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, '-I.', '-Iinclude',
         '-o', destination, assembly], cwd)
    extracted = parsed_elf(destination.read_bytes())
    verification.require(extracted.section_bytes(extracted.section('.text')) == expected and
                         functions(destination) == functions(reference),
                         f'{obj["id"]}: numeric disassembly changed other instructions or function scope')
    # No synthesized relocation may survive on a numeric original operand.
    relocated = text_relocations(extracted, extracted.section('.text'))
    verification.require(not any(change < site + 4 and site < change + 4
                                 for change in changes for site in relocated),
                         f'{obj["id"]}: original numeric operand received an invented relocation')
    retained = [row for row in text_relocation_identities(scaffold, text)
                if not any(change < row[0] + 4 and row[0] < change + 4 for change in changes)]
    verification.require(Counter(retained) == Counter(text_relocation_identities(extracted, extracted.section('.text'))),
                         f'{obj["id"]}: numeric original reference changed another relocation')
    if data_proofs:
        destination.with_suffix('.data-addresses.json').write_text(json.dumps(dict(
            schema='original-numeric-data-addresses/1',
            original_sha256=hashlib.sha256(original.data).hexdigest(),
            ordinary_sha256=hashlib.sha256(ordinary.read_bytes()).hexdigest(),
            base_sha256=hashlib.sha256(base.read_bytes()).hexdigest(),
            reference_sha256=hashlib.sha256(extracted.data).hexdigest(),
            pairs=data_proofs), indent=2) + '\n')
    return destination


def original_literal_pool(reference, original_path, start, extent, destination, cwd, obj,
                          assembler, flags):
    """Retain anonymous original literal storage without inventing relocations.

    The retail ELF proves the pool bytes and numeric GP operands, but no
    R_MIPS_LITERAL versus R_MIPS_GPREL16 distinction survives the final link.
    Reassembling loads as li.s invents that distinction and makes objdiff reject
    otherwise identical C loads. Keep the original instructions/relocations and
    describe only the original anonymous, finite single-precision pool here.
    """
    if not extent:
        return reference, False
    original = parsed_elf(original_path.read_bytes())
    before = parsed_elf(reference.read_bytes())
    lo, hi = extent
    lit = next((s for s in original.sections
                if s.name == '.lit4' and s.addr <= lo < hi <= s.addr+s.size), None)
    if (lit is None or (hi-lo) % 4 or lo % lit.align
            or any(s.type in (1, 2) and lo <= s.value < hi for s in original.symbols)
            or any(s.name == '.lit4' and s.size for s in before.sections)):
        return reference, False
    source = Path(str(reference) + '.s')
    if not source.is_file():
        source = reference.with_suffix('.s')
    if not source.is_file():
        return reference, False
    expected_pool = original.data[lit.offset+lo-lit.addr:lit.offset+hi-lit.addr]
    values = struct.unpack('<' + 'f' * ((hi-lo)//4), expected_pool)
    if not all(float('-inf') < value < float('inf') for value in values):
        return reference, False
    assembly = destination.with_suffix('.s')
    body = source.read_text()
    pool = '\n.section .lit4\n.align ' + str(lit.align.bit_length()-1) + '\n'
    pool += ''.join(f'.float {value:.20e}\n' for value in values)
    assembly.write_text(body + pool)
    try:
        gflag = next(flag for flag in flags if flag.startswith('-G'))
        run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, '-I.', '-Iinclude',
             '-o', destination, assembly], cwd)
        after = parsed_elf(destination.read_bytes())
        actual_lit = after.section('.lit4')
    except (subprocess.CalledProcessError, StopIteration):
        return reference, False
    if ((actual_lit.type, actual_lit.flags, actual_lit.align) !=
            (lit.type, lit.flags, lit.align)
            or after.section_bytes(actual_lit) != expected_pool):
        return reference, False
    for section in before.sections:
        if not section.flags & 2:
            continue
        candidates = [s for s in after.sections if s.name == section.name]
        if (len(candidates) != 1 or (section.type, section.flags, section.size, section.align) !=
                (candidates[0].type, candidates[0].flags, candidates[0].size, candidates[0].align)
                or before.section_bytes(section) != after.section_bytes(candidates[0])):
            return reference, False
    def relocations(elf):
        return Counter((elf.sections[rel.info].name, offset, info & 0xff,
                        symbol.name, symbol.value, symbol.size, symbol.bind,
                        symbol.type, symbol.other,
                        elf.sections[symbol.shndx].name
                        if 0 < symbol.shndx < len(elf.sections) else symbol.shndx)
                for rel in elf.sections if rel.type == 9
                for i in range(0, rel.size, rel.entsize)
                for offset, info in [struct.unpack_from('<II', elf.data, rel.offset+i)]
                for symbol in [elf.symbols[info >> 8]])
    verification.require(relocations(before) == relocations(after),
                         f'{obj["id"]}: literal storage changed original relocations')
    identities = lambda elf: {(s.name, s.value, s.size, s.type, s.bind)
                               for s in elf.symbols if s.type == 2}
    verification.require(identities(before) == identities(after),
                         f'{obj["id"]}: literal storage changed FUNC identities')
    destination.with_suffix('.proof.json').write_text(json.dumps(dict(
        schema='objdiff-original-literal-pool/2', tu=obj['id'],
        original_sha256=hashlib.sha256(original.data).hexdigest(),
        address=f'0x{lo:08X}', size=hi-lo,
        initializer_sha256=hashlib.sha256(expected_pool).hexdigest(),
        complete_original_pool_exact=True,
        all_original_allocated_bytes_and_relocations_unchanged=True,
        no_invented_text_relocations=True), indent=2)+'\n')
    return destination, True


def integer_literal_arrays(preprocessed):
    """A deliberately narrow source proof: integer arrays of literal values.

    Pointer arrays, casts, arithmetic, macros that remain unresolved, structs
    and arbitrary compiler-emitted bytes are not evidence for this rule.
    Typedefs are resolved from the actual preprocessed source. Plain long is
    deliberately unsupported: its width cannot be inferred from an ELF32 file
    and differs between compiler ABIs used for MIPS.
    """
    widths = {'char': 1, 'short': 2, 'short int': 2, 'int': 4,
              'long long': 8, 'long long int': 8}
    for declaration in source_matches(r'\btypedef\s+([^;{}]+?)\s+(\w+)\s*;', preprocessed):
        spelling = re.sub(r'\b(?:signed|unsigned|const|volatile)\b', '', declaration[1])
        spelling = ' '.join(spelling.split())
        if spelling in widths:
            widths[declaration[2]] = widths[spelling]
    arrays = {}
    pattern = r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*\[\s*(0[xX][0-9a-fA-F]+|\d+)\s*\]\s*=\s*\{([^{}]*)\}\s*;'
    literal = re.compile(r'(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)(?:[uUlL]*)')
    for declaration in source_matches(pattern, preprocessed):
        spelling = re.sub(r'\b(?:signed|unsigned|const|volatile)\b', '', declaration[1])
        width = widths.get(' '.join(spelling.split()))
        if width is None:
            continue
        tokens = [v.strip() for v in declaration[4].split(',')]
        if tokens and not tokens[-1]:
            tokens.pop()
        if len(tokens) != int(declaration[3], 16 if declaration[3].lower().startswith('0x') else 10) or not all(literal.fullmatch(v) for v in tokens):
            continue
        values = [literal.fullmatch(v)[1] for v in tokens]
        values = [int(v, 16 if v.lower().startswith('0x') else 8 if v.startswith('0') else 10)
                  for v in values]
        if all(v < 1 << (width * 8) for v in values):
            arrays[declaration[2]] = b''.join(v.to_bytes(width, 'little') for v in values)
    return arrays


def original_references(original, begin, end):
    """Conservative original-byte check for a proposed unreferenced pad.

    Check original relocations, stored pointers, GP accesses and LUI address
    constructions. False positives retain a pad; they never remove real data.
    """
    addresses = original_reference_addresses(original.data)
    at = bisect_left(addresses, begin)
    return at < len(addresses) and addresses[at] < end


@lru_cache(maxsize=6)
def original_reference_addresses(data):
    """Index precisely the same conservative witnesses once per original ELF."""
    original = parsed_elf(data)
    addresses = set()
    symbols = {}
    for symbol in original.symbols:
        symbols.setdefault(symbol.index, symbol)
    gp = original.gp
    for section in original.sections:
        if section.type == 9:
            for offset in range(section.offset, section.offset + section.size, section.entsize):
                address, info = struct.unpack_from('<II', original.data, offset)
                symbol = symbols.get(info >> 8)
                addresses.add(address)
                if symbol:
                    addresses.add(symbol.value)
        if not section.flags & 2 or section.type == 8:
            continue
        data = original.section_bytes(section)
        if not section.flags & 4:
            addresses.update(struct.unpack_from('<I', data, offset)[0]
                             for offset in range(0, len(data) - 3, 4))
            continue
        upper = {}
        for offset in range(0, len(data) - 3, 4):
            word, = struct.unpack_from('<I', data, offset)
            opcode, rs, rt = word >> 26, word >> 21 & 31, word >> 16 & 31
            immediate = word & 0xffff
            signed = immediate - (0x10000 if immediate & 0x8000 else 0)
            if opcode == 0xf:
                upper[rt] = (immediate << 16, offset)
            elif opcode in (8, 9, 0xd) or 0x20 <= opcode <= 0x3f:
                if rs in upper and offset - upper[rs][1] <= 128:
                    value = upper[rs][0] | immediate if opcode == 0xd else upper[rs][0] + signed
                    addresses.add(value)
                if rs == 28 and gp:
                    addresses.add(gp + signed)
    return tuple(sorted(addresses))


@lru_cache(maxsize=8)
def function_static_source_bindings(preprocessed, compiler, assembly):
    """Witness lexical static declarations and their actual cc1 local names.

    A suffix is read from cc1, never invented or renumbered. The same lexical
    declaration must belong to exactly one source function with a unique direct
    ordinary compiler consumer. Original identity and storage ownership are
    separate mandatory checks in the reference consumers of these witnesses.
    """
    import tu_edit
    try:functions=[f for f in tu_edit.scan(preprocessed) if f.kind=='func']
    except tu_edit.ParseError:return {}
    outside=[];app=False
    for line in assembly.splitlines():
        if line.strip()=='#APP':app=True
        elif line.strip()=='#NO_APP':app=False
        elif not app:outside.append(line)
    ordinary='\n'.join(outside)
    regions={m[1]:m[2] for m in re.finditer(r'^\s*\.ent\s+(\S+)\s*$(.*?)^\s*\.end\s+\1\s*$',ordinary,re.M|re.S)}
    result={}
    for symbol in compiler.symbols:
        suffix=re.fullmatch(r'([A-Za-z_]\w*)\.[0-9]+',symbol.name)
        if not suffix or symbol.bind!=0 or symbol.type not in (0,1) or not 0<symbol.shndx<len(compiler.sections):continue
        if compiler.sections[symbol.shndx].name not in ('.data','.sdata','.rodata','.bss','.sbss'):continue
        name=suffix[1]
        escaped=re.escape(symbol.name)
        consumer=re.compile(r'%(?:hi|lo|gp_rel)\('+escaped+r'(?:\)|[+ -])|'
            r'^\s*(?:la|lb|lbu|lh|lhu|lw|lwu|ld|lq|l\.s|l\.d|sb|sh|sw|sd|sq|s\.s|s\.d)\s+[^\n]*?(?<![\w.$])'+escaped+r'(?![\w.$])',re.M)
        callers=[name for name,body in regions.items() if consumer.search(body)]
        if len(callers)!=1:continue
        owners=[f for f in functions if f.name==callers[0]]
        if len(owners)!=1:continue
        function=owners[0]
        pattern=(r'\bstatic\s+((?:[A-Za-z_]\w*\s+)+)(?:\(\s*\*\s*'+re.escape(name)+
                 r'\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)?\s*\])*)\s*\)\s*\([^;{}]*\)|'+
                 re.escape(name)+r'\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)?\s*\])*))\s*(?==|;)')
        declarations=list(source_matches(pattern,function.text(preprocessed)))
        if len(declarations)!=1:continue
        declaration=declarations[0]
        result[symbol.name]=dict(source_function=function.name,source_lexical_name=name,
            source_declaration=declaration[0].strip(),source_declared_type=declaration[1].strip(),
            source_array_dimensions=declaration[2] or declaration[3] or '',
            source_declaration_offset=function.start+declaration.start(),
            direct_ordinary_compiler_consumers=callers)
    return result


def numeric_union_source_objects(preprocessed, compiler, assembly):
    """Literal first-member integer arrays in complete evidenced unions.

    The existing bounded record splitter/bound evaluator reads field extents.
    Only explicit scalar arrays with known EE widths are admitted; pointers,
    records, casts, expressions, omitted slots and unknown union views refuse.
    A known native-TI view can establish the legitimate 16-byte alignment.
    Whole initializer bytes and absence of compiler DATA relocations are
    required here; original_data still requires a real original OBJECT.
    """
    from data_types import _record_split, _record_bound, RecordLayoutError
    clean=lambda t:' '.join(re.sub(r'\b(?:const|volatile)\b','',t).split())
    widths={'char':1,'signed char':1,'unsigned char':1,'short':2,'short int':2,
        'signed short':2,'unsigned short':2,'unsigned short int':2,'int':4,'signed int':4,
        'unsigned':4,'unsigned int':4,'long':8,'long int':8,'unsigned long':8,
        'unsigned long int':8,'long long':8,'long long int':8,'unsigned long long':8,
        'unsigned long long int':8}
    aliases={}
    for match in source_matches(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):
        aliases.setdefault(match[2],set()).add(clean(match[1]))
    ti={m[1] for m in source_matches(r'\btypedef\s+unsigned\s+int\s+(\w+)\s+__attribute__\s*\(\(\s*mode\s*\(\s*TI\s*\)\s*\)\)\s*;',preprocessed)}
    def scalar(typename,seen=()):
        typename=clean(typename)
        if typename in seen:return None
        if typename in widths:return widths[typename],widths[typename],('unsigned' in typename)
        if typename in ti:return 16,16,True
        choices=aliases.get(typename,set())
        return scalar(next(iter(choices)),seen+(typename,)) if len(choices)==1 else None
    unions={}
    for match in source_matches(r'\btypedef\s+union(?:\s+\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;',preprocessed):
        fields=[]
        try:
            for text in _record_split(match[1],';'):
                field=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*((?:\[[^\[\]]+\]\s*)*)',text)
                layout=scalar(field[1]) if field else None
                if not layout:break
                width,alignment,unsigned=layout;bounds=re.findall(r'\[([^\]]+)\]',field[3])
                if len(bounds)!=1:break
                count=_record_bound(bounds[0]);size=width*count
                if not 0<size<=65536:break
                fields.append(dict(name=field[2],type=clean(field[1]),width=width,alignment=alignment,
                    unsigned=unsigned,count=count,size=size))
            else:
                if fields:
                    alignment=max(f['alignment'] for f in fields)
                    size=(max(f['size'] for f in fields)+alignment-1)//alignment*alignment
                    if fields[0]['size']==size:
                        unions.setdefault(match[2],[]).append(dict(fields=fields,size=size,alignment=alignment))
        except RecordLayoutError:continue
    emitted=typed_compiler_data(assembly);result={}
    for declaration in source_matches(r'\b(?:static\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*=\s*',preprocessed):
        typename,name=declaration[1],declaration[2]
        choices=unions.get(typename,[])
        if len(choices)!=1 or name not in emitted:continue
        layout=choices[0];first=layout['fields'][0]
        at=declaration.end()
        if preprocessed[at:at+1]!='{':continue
        end=at+1;depth=1
        while end<len(preprocessed) and depth:
            depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
        if depth or not re.match(r'\s*;',preprocessed[end:]):continue
        items=_record_split(preprocessed[at+1:end-1],',')
        if len(items)!=1:continue
        member=items[0]
        designator=re.match(r'\.([A-Za-z_]\w*)\s*=\s*',member)
        if designator:
            if designator[1]!=first['name']:continue
            member=member[designator.end():]
        if not (member.startswith('{') and member.endswith('}')):continue
        literals=_record_split(member[1:-1],',')
        if len(literals)!=first['count']:continue
        decoded=bytearray()
        for literal in literals:
            if not re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*',literal):break
            spelling=re.sub(r'[uUlL]+$','',literal)
            value=int(spelling,16 if 'x' in spelling.lower() else 10);bits=first['width']*8
            if not -(1<<(bits-1))<=value<1<<(bits if first['unsigned'] else bits-1):break
            decoded.extend((value%(1<<bits)).to_bytes(first['width'],'little'))
        else:
            symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and
                0<s.shndx<len(compiler.sections) and compiler.sections[s.shndx].type==1]
            if len(symbols)!=1:continue
            symbol=symbols[0];section=compiler.sections[symbol.shndx]
            if (symbol.size,len(decoded),emitted[name]['size'])!=(layout['size'],layout['size'],layout['size']):continue
            if emitted[name]['section']!=section.name or symbol.value%layout['alignment'] or section.align<layout['alignment']:continue
            if any(rel.type==9 and rel.info==section.index and
                any(symbol.value<=struct.unpack_from('<I',compiler.data,p)[0]<symbol.value+symbol.size
                    for p in range(rel.offset,rel.offset+rel.size,8)) for rel in compiler.sections):continue
            raw=compiler.section_bytes(section)[symbol.value:symbol.value+symbol.size]
            if raw!=decoded:continue
            result[name]=dict(raw=raw,integer_offsets=None,source_type=typename,
                kind='literal-only-numeric-union-object',symbol=symbol,
                union_active_numeric_member=dict(member=first['name'],fields=layout['fields'],
                    union_size=layout['size'],union_alignment=layout['alignment'],explicit_slots=len(literals)))
    return result


def numeric_record_union_source_objects(preprocessed, compiler, assembly):
    """Decode complete integer records in an evidenced union's first member.

    All views have explicit pointer-free layouts; bitfields, casts, omitted
    fields/slots and ABI padding refuse. Source values are decoded independently
    of compiler bytes. original_data additionally requires a retail OBJECT.
    """
    import ast
    from data_types import _record_split, _record_bound, RecordLayoutError
    clean=lambda t:' '.join(re.sub(r'\b(?:const|volatile)\b','',t).split())
    widths={'char':1,'signed char':1,'unsigned char':1,'short':2,'short int':2,
        'signed short':2,'unsigned short':2,'unsigned short int':2,'int':4,'signed int':4,
        'unsigned':4,'unsigned int':4,'long':8,'long int':8,'unsigned long':8,
        'unsigned long int':8,'long long':8,'long long int':8,'unsigned long long':8,
        'unsigned long long int':8}
    aliases={};records={}
    for m in source_matches(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):
        aliases.setdefault(m[2],set()).add(clean(m[1]))
    ti={m[1] for m in source_matches(r'\btypedef\s+unsigned\s+int\s+(\w+)\s+__attribute__\s*\(\(\s*mode\s*\(\s*TI\s*\)\s*\)\)\s*;',preprocessed)}
    for m in source_matches(r'\btypedef\s+(struct|union)(?:\s+\w+)?\s*\{',preprocessed):
        at=m.end();end=at;depth=1
        while end<len(preprocessed) and depth:
            depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
        tail=re.match(r'\s*(\w+)\s*;',preprocessed[end:])
        if not depth and tail:records.setdefault(tail[1],set()).add((m[1],preprocessed[at:end-1]))
    def layout(typename,seen=()):
        typename=clean(typename)
        if typename in seen:raise RecordLayoutError('recursive numeric record')
        if typename in widths:
            w=widths[typename];return dict(kind='scalar',size=w,align=w,unsigned='unsigned' in typename)
        if typename in ti:return dict(kind='scalar',size=16,align=16,unsigned=True)
        choices=aliases.get(typename,set())
        if len(choices)==1:return layout(next(iter(choices)),seen+(typename,))
        choices=records.get(typename,set())
        if len(choices)!=1:raise RecordLayoutError('unknown or divergent numeric record')
        kind,body=next(iter(choices));return record_layout(kind,body,seen+(typename,))
    def record_layout(kind,body,seen):
        fields=[];offset=0;align=1
        for spelling in _record_split(body,';'):
            nested=re.fullmatch(r'\s*(struct|union)\s*\{(.*)\}\s*(\w+)\s*',spelling,re.S)
            field=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*((?:\[[^\[\]]+\]\s*)*)',spelling)
            if nested:child=record_layout(nested[1],nested[2],seen);name=nested[3]
            elif field:
                child=layout(field[1],seen);name=field[2];bounds=re.findall(r'\[([^\]]+)\]',field[3])
                if len(bounds)>1:raise RecordLayoutError('multidimensional numeric member')
                if bounds:
                    count=_record_bound(bounds[0]);child=dict(kind='array',child=child,count=count,size=child['size']*count,align=child['align'])
            else:raise RecordLayoutError('pointer, bitfield or unknown numeric member')
            align=max(align,child['align']);place=0 if kind=='union' else offset
            if kind=='struct' and place%child['align']:raise RecordLayoutError('numeric record ABI padding')
            fields.append(dict(name=name,offset=place,layout=child));offset=max(offset,child['size']) if kind=='union' else offset+child['size']
        if not fields or not 0<offset<=65536 or offset%align:raise RecordLayoutError('numeric record tail padding or size')
        if kind=='union' and fields[0]['layout']['size']!=offset:raise RecordLayoutError('union first member incomplete')
        return dict(kind=kind,size=offset,align=align,fields=fields)
    def integer(text):
        text=re.sub(r'(?<=[\da-fA-F])[uUlL]+\b','',text.strip())
        def value(node,depth=0):
            if depth>16:raise RecordLayoutError('integer expression depth')
            if isinstance(node,ast.Constant) and type(node.value) is int:return node.value
            if isinstance(node,ast.UnaryOp) and isinstance(node.op,(ast.UAdd,ast.USub)):
                return value(node.operand,depth+1)*(-1 if isinstance(node.op,ast.USub) else 1)
            if isinstance(node,ast.BinOp) and isinstance(node.op,(ast.BitOr,ast.BitAnd,ast.BitXor,ast.LShift,ast.RShift)):
                a,b=value(node.left,depth+1),value(node.right,depth+1)
                # C's plain int expression must stay defined independently of
                # storage width: refuse signed overflow and oversized shifts.
                if min(a,b)<0 or max(a,b)>0x7fffffff or isinstance(node.op,(ast.LShift,ast.RShift)) and b>=31:
                    raise RecordLayoutError('integer operation range')
                result=a|b if isinstance(node.op,ast.BitOr) else a&b if isinstance(node.op,ast.BitAnd) else a^b if isinstance(node.op,ast.BitXor) else a<<b if isinstance(node.op,ast.LShift) else a>>b
                if result>0x7fffffff:raise RecordLayoutError('signed int expression overflow')
                return result
            raise RecordLayoutError('nonliteral integer expression')
        try:return value(ast.parse(text,mode='eval').body)
        except (SyntaxError,ValueError,TypeError,RecursionError):raise RecordLayoutError('invalid integer initializer') from None
    def decode(node,text):
        if node['kind']=='scalar':
            n=integer(text);bits=node['size']*8
            if not -(1<<(bits-1))<=n<1<<(bits if node['unsigned'] else bits-1):raise RecordLayoutError('integer initializer range')
            return (n%(1<<bits)).to_bytes(node['size'],'little')
        if not (text.startswith('{') and text.endswith('}')):raise RecordLayoutError('explicit aggregate braces required')
        items=_record_split(text[1:-1],',')
        if node['kind']=='array':
            if len(items)!=node['count']:raise RecordLayoutError('array omitted slots')
            return b''.join(decode(node['child'],item) for item in items)
        fields=node['fields'][:1] if node['kind']=='union' else node['fields']
        if len(items)!=len(fields):raise RecordLayoutError('record omitted fields')
        decoded=[]
        for field,item in zip(fields,items):
            designator=re.match(r'\.([A-Za-z_]\w*)\s*=\s*',item)
            if designator:
                if designator[1]!=field['name']:raise RecordLayoutError('record field order')
                item=item[designator.end():]
            decoded.append(decode(field['layout'],item))
        return b''.join(decoded)
    emitted=typed_compiler_data(assembly);result={}
    for declaration in source_matches(r'\b(?:static\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*=\s*',preprocessed):
        typename,name=declaration[1],declaration[2]
        if name not in emitted:continue
        try:
            node=layout(typename)
            if node['kind']!='union' or node['fields'][0]['layout']['kind'] not in ('struct','union'):continue
            at=declaration.end();end=at;depth=0
            if preprocessed[at:at+1]!='{':continue
            while end<len(preprocessed):
                depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
                if not depth:break
            if depth or not re.match(r'\s*;',preprocessed[end:]):continue
            raw=decode(node,preprocessed[at:end])
        except RecordLayoutError:continue
        symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and 0<s.shndx<len(compiler.sections) and compiler.sections[s.shndx].type==1]
        if len(symbols)!=1:continue
        symbol=symbols[0];section=compiler.sections[symbol.shndx]
        if (symbol.size,len(raw),emitted[name]['size'])!=(node['size'],node['size'],node['size']):continue
        if emitted[name]['section']!=section.name or symbol.value%node['align'] or section.align<node['align']:continue
        if any(rel.type==9 and rel.info==section.index and any(symbol.value<=struct.unpack_from('<I',compiler.data,p)[0]<symbol.value+symbol.size
            for p in range(rel.offset,rel.offset+rel.size,8)) for rel in compiler.sections):continue
        if compiler.section_bytes(section)[symbol.value:symbol.value+symbol.size]!=raw:continue
        result[name]=dict(raw=raw,integer_offsets=None,source_type=typename,kind='literal-only-numeric-union-object',symbol=symbol,
            union_active_numeric_member=dict(member=node['fields'][0]['name'],complete_recursive_layout=node,
                full_explicit_initializer=True,union_size=node['size'],union_alignment=node['align']))
    return result


def numeric_character_source_objects(preprocessed, compiler, assembly):
    """Complete literal char-array initializers, independently decoded.

    Explicit capacity, ordinary cc1 OBJECT emission and the entire raw payload
    are required. Pointer/cast/expression initializers and unknown attributes
    refuse; original_data additionally binds exact registered original spans.
    """
    from data_types import _record_split, _record_bound, RecordLayoutError
    emitted=typed_compiler_data(assembly);result={}
    declaration=re.compile(r'\b(?:static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?char\s+(\w+)\s*\[\s*([^\[\]]+)\s*\]\s*'
        r'(?:__attribute__\s*\(\(\s*section\s*\(\s*"\.[\w.]+"\s*\)\s*\)\)\s*)?=\s*')
    for m in source_matches(declaration, preprocessed):
        name=m[1]
        if name not in emitted:continue
        try:count=_record_bound(m[2])
        except RecordLayoutError:continue
        if not 0<count<=65536:continue
        at=m.end();payload=None;form=None
        literal=re.match(r'("(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*)\s*;',preprocessed[at:])
        if literal:
            try:payload=b''.join(c_literal_bytes(token) for token in re.findall(r'"(?:\\.|[^"\\])*"',literal[1]))+b'\0'
            except (ValueError,SyntaxError,UnicodeError):continue
            if len(payload)>count:continue
            payload+=bytes(count-len(payload));form='literal string with explicit char capacity'
        elif preprocessed[at:at+1]=='{':
            end=preprocessed.find('}',at)
            if end<0 or not re.match(r'\s*;',preprocessed[end+1:]):continue
            values=_record_split(preprocessed[at+1:end],',')
            if len(values)!=count:continue
            decoded=[]
            for item in values:
                if not re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*',item):break
                text=re.sub(r'[uUlL]+$','',item);value=int(text,16 if 'x' in text.lower() else 10)
                if not -128<=value<=255:break
                decoded.append(value%256)
            else:payload=bytes(decoded);form='explicit full char literal array'
        if payload is None:continue
        symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and s.size==count and 0<s.shndx<len(compiler.sections) and compiler.sections[s.shndx].type==1]
        if len(symbols)!=1:continue
        symbol=symbols[0];section=compiler.sections[symbol.shndx]
        if (emitted[name]['section'],emitted[name]['size'])!=(section.name,count):continue
        if compiler.section_bytes(section)[symbol.value:symbol.value+count]!=payload:continue
        if any(rel.type==9 and rel.info==section.index and any(symbol.value<=struct.unpack_from('<I',compiler.data,p)[0]<symbol.value+count
            for p in range(rel.offset,rel.offset+rel.size,8)) for rel in compiler.sections):continue
        result[name]=dict(raw=payload,integer_offsets=None,source_type='char array',kind='literal-only-character-array',symbol=symbol,
            character_initializer_proof=dict(form=form,explicit_capacity=count,full_source_initializer_decoded=True,
                cc1_typed_storage=emitted[name],source_bytes_sha256=hashlib.sha256(payload).hexdigest()))
    return result


def numeric_byte_expression_source_objects(preprocessed, compiler, assembly):
    """Complete unsigned-byte arrays made from literal casts/right shifts.

    Decode the closed EE_U32_BYTES/EE_U64_BYTES expression grammar from
    independently evidenced unsigned typedef widths. Only grouping, unsigned
    casts, nonnegative integer literals and defined unsigned right shifts are
    admitted. No identifiers, pointers, omitted slots or inferred capacity.
    original_data still requires the complete real original OBJECT identity.
    """
    from data_types import _record_split, _record_bound, RecordLayoutError
    widths={'unsigned char':8,'unsigned short':16,'unsigned short int':16,
        'unsigned':32,'unsigned int':32,'unsigned long':64,'unsigned long int':64,
        'unsigned long long':64,'unsigned long long int':64}
    aliases={}
    for m in source_matches(r'\btypedef\s+([\w ]+?)\s+(\w+)\s*;',preprocessed):
        aliases.setdefault(m[2],set()).add(' '.join(m[1].split()))
    def unsigned_width(name,seen=()):
        name=' '.join(name.split())
        if name in widths:return widths[name]
        choices=aliases.get(name,set())
        if name in seen or len(choices)!=1:return None
        return unsigned_width(next(iter(choices)),seen+(name,))
    def decode(text):
        token=re.compile(r'\s*(>>|[()]|0[xX][\da-fA-F]+[uUlL]*|(?:0[0-7]*|[1-9]\d*)[uUlL]*|[A-Za-z_]\w*)')
        tokens=[];at=0
        while at<len(text):
            m=token.match(text,at)
            if not m:raise RecordLayoutError('unknown byte expression token')
            tokens.append(m[1]);at=m.end()
        index=0
        def unary(depth):
            nonlocal index
            if depth>32 or index>=len(tokens):raise RecordLayoutError('byte expression depth/end')
            if tokens[index]=='(':
                close=index+1
                while close<len(tokens) and re.fullmatch(r'[A-Za-z_]\w*',tokens[close]):close+=1
                cast=unsigned_width(' '.join(tokens[index+1:close])) if close<len(tokens) and tokens[close]==')' else None
                if cast:
                    index=close+1;value,bits,unsigned=unary(depth+1)
                    return value%(1<<cast),cast,True
                index+=1;value=expression(depth+1)
                if index>=len(tokens) or tokens[index]!=')':raise RecordLayoutError('unbalanced byte expression')
                index+=1;return value
            m=re.fullmatch(r'(0[xX][\da-fA-F]+|0[0-7]*|[1-9]\d*)([uUlL]*)',tokens[index])
            if not m:raise RecordLayoutError('nonliteral byte expression')
            index+=1;suffix=m[2].lower()
            if suffix not in ('','u','l','ul','lu','ll','ull','llu'):raise RecordLayoutError('integer suffix')
            spelling=m[1];value=int(spelling,16 if spelling.lower().startswith('0x') else 8 if len(spelling)>1 and spelling.startswith('0') else 10)
            if value>0xffffffffffffffff:raise RecordLayoutError('EE integer literal width')
            if 'l' in suffix:bits=64;unsigned='u' in suffix
            elif 'u' in suffix:bits=32 if value<=0xffffffff else 64;unsigned=True
            elif value<=0x7fffffff:bits=32;unsigned=False
            elif spelling.lower().startswith('0x') and value<=0xffffffff:bits=32;unsigned=True
            else:bits=64;unsigned=value>0x7fffffffffffffff and spelling.lower().startswith('0x')
            if not unsigned and value>=1<<(bits-1):raise RecordLayoutError('signed literal range')
            return value,bits,unsigned
        def expression(depth):
            nonlocal index
            value,bits,unsigned=unary(depth)
            while index<len(tokens) and tokens[index]=='>>':
                index+=1;shift,_,_=unary(depth+1)
                # Unsigned char/short undergo signed int promotion; that
                # broader arithmetic is deliberately not admitted here.
                if not unsigned or bits<32 or not 0<=shift<bits:raise RecordLayoutError('undefined/unsupported shift')
                value>>=shift
            return value,bits,unsigned
        value,bits,unsigned=expression(0)
        if index!=len(tokens) or not 0<=value<=255:raise RecordLayoutError('byte expression residue/range')
        return value
    emitted=typed_compiler_data(assembly);result={}
    pattern=re.compile(r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*\[([^\[\]]+)\]\s*=\s*\{([^{}]*)\}\s*;',re.S)
    for m in source_matches(pattern, preprocessed):
        typename,name=m[1].strip(),m[2]
        if unsigned_width(typename)!=8 or name not in emitted:continue
        try:
            count=_record_bound(m[3]);items=_record_split(m[4],',')
            if not 0<count<=65536 or len(items)!=count:continue
            payload=bytes(decode(item.strip()) for item in items)
        except (RecordLayoutError,ValueError,RecursionError):continue
        symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and s.size==count and
                 0<s.shndx<len(compiler.sections) and compiler.sections[s.shndx].type==1]
        if len(symbols)!=1:continue
        s=symbols[0];section=compiler.sections[s.shndx]
        if (emitted[name]['section'],emitted[name]['size'])!=(section.name,count):continue
        if compiler.section_bytes(section)[s.value:s.value+count]!=payload:continue
        refused=False
        for rel in compiler.sections:
            if rel.type not in (4,9) or rel.info!=section.index:continue
            stride=12 if rel.type==4 else 8
            if rel.entsize not in (0,stride) or rel.size%stride:refused=True;break
            if any(s.value<=struct.unpack_from('<I',compiler.data,k)[0]<s.value+count
                   for k in range(rel.offset,rel.offset+rel.size,stride)):refused=True;break
        if refused:continue
        result[name]=dict(raw=payload,integer_offsets=None,source_type=typename,
            kind='literal-only-unsigned-byte-expression-array',symbol=s,
            source_byte_expression_proof=dict(explicit_capacity=count,explicit_slots=len(items),
                unsigned_typedef_widths={name:unsigned_width(name) for name in aliases if unsigned_width(name)},
                source_initializer_sha256=hashlib.sha256(m[4].encode()).hexdigest(),
                independently_decoded_bytes_sha256=hashlib.sha256(payload).hexdigest(),
                permitted_operations=['unsigned integer cast','defined unsigned right shift','grouping','integer literal']))
    return result


def literal_integer_expression(text,unsigned64_types=()):
    """Decode bounded literal C integer arithmetic without names or casts.

    Retain EE int32/long64 literal suffixes and usual integer conversions.
    Refuse signed overflow and implementation-defined shifts. A caller may
    admit casts of literal operands to independently resolved unsigned64 types.
    """
    if len(text)>512:raise ValueError('integer expression extent')
    cast='|'.join(re.escape(n).replace(r'\ ',r'\s+') for n in unsigned64_types)
    token=re.compile(r'(?<![\w.])(?:(\(\s*(?:'+(cast or r'(?!)')+r')\s*\))\s*)?((?:0[xX][\da-fA-F]+|0[0-7]*|[1-9]\d*)([uUlL]*))(?![\w.])')
    literals={}
    def spelling(m):
        value=re.sub(r'[uUlL]+$','',m[2]);suffix=m[3].lower()
        if not re.fullmatch(r'(?:u(?:ll|l)?|(?:ll|l)u?|)',suffix):raise ValueError('integer suffix')
        number=int(value,16 if value.lower().startswith('0x') else 8 if value.startswith('0') else 10)
        bits=64 if 'l' in suffix or number>=1<<32 else 32
        unsigned='u' in suffix
        if not unsigned and number>=1<<(bits-1):
            if value.lower().startswith('0x') or value.startswith('0') and len(value)>1:unsigned=True
            elif bits==32:bits=64
            else:raise ValueError('decimal integer range')
        if not 0<=number<1<<bits:raise ValueError('integer literal range')
        if m[1]:bits,unsigned=64,True
        key='__literal_'+str(len(literals));literals[key]=(number,bits,unsigned);return key
    if re.sub(r'[\s()+*<>|&^\-]','',token.sub('',text.strip())):
        raise ValueError('nonliteral integer spelling')
    normalized=token.sub(spelling,text.strip())
    tree=ast.parse(normalized,mode='eval')
    if sum(1 for _ in ast.walk(tree))>128:raise ValueError('integer expression depth')
    def value(node):
        if isinstance(node,ast.Name) and node.id in literals:return literals[node.id]
        elif isinstance(node,ast.UnaryOp) and isinstance(node.op,(ast.UAdd,ast.USub)):
            a,bits,unsigned=value(node.operand);result=a*(-1 if isinstance(node.op,ast.USub) else 1)
        elif isinstance(node,ast.BinOp) and isinstance(node.op,(ast.Add,ast.Sub,ast.Mult,ast.LShift,ast.RShift,ast.BitOr,ast.BitAnd,ast.BitXor)):
            a,abits,au=value(node.left);b,bbits,bu=value(node.right)
            if isinstance(node.op,(ast.LShift,ast.RShift)):
                bits,unsigned=abits,au
                if a<0 or not 0<=b<bits:raise ValueError('nonportable integer shift')
                result=a<<b if isinstance(node.op,ast.LShift) else a>>b
            else:
                bits=max(abits,bbits)
                unsigned=(au or bu) if abits==bbits else au if abits>bbits else bu
                if unsigned:a%=1<<bits;b%=1<<bits
                if isinstance(node.op,(ast.BitOr,ast.BitAnd,ast.BitXor)):
                    if a<0 or b<0:raise ValueError('signed integer bit operation')
                    result=a|b if isinstance(node.op,ast.BitOr) else a&b if isinstance(node.op,ast.BitAnd) else a^b
                else:result=a+b if isinstance(node.op,ast.Add) else a-b if isinstance(node.op,ast.Sub) else a*b
        else:raise ValueError('nonliteral integer expression')
        if unsigned:result%=1<<bits
        elif not -(1<<(bits-1))<=result<1<<(bits-1):raise ValueError('signed integer expression overflow')
        return result,bits,unsigned
    return value(tree.body)[0]


def typed_literal_source_objects(preprocessed, compiler, assembly, templates_only=False):
    """Decode pointer-free C scalar/array/record initializers independently.

    The EE scalar widths and explicit source fields establish layout. Strings
    are admitted only as char arrays; unions admit a complete first member or
    an explicit complete active member. Explicit unsigned bitfields must fill their declared 32/64-bit
    containers. Unknown expressions, pointers, attributes and divergent types
    refuse. Whole decoded bytes, cc1 OBJECT extent/alignment and absence of
    actual compiler DATA relocations are mandatory before original binding.
    """
    import math
    from data_types import RecordLayoutError
    def split(text, delimiter=','):
        parts=[];at=0;depth=0;quote=None;escape=False
        for i,c in enumerate(text):
            if quote:
                if escape:escape=False
                elif c=='\\':escape=True
                elif c==quote:quote=None
                continue
            if c in '\"\'':quote=c
            elif c in '{[(':depth+=1
            elif c in '}])':
                depth-=1
                if depth<0:raise RecordLayoutError('initializer nesting')
            elif c==delimiter and depth==0:
                if text[at:i].strip():parts.append(text[at:i].strip())
                at=i+1
        if quote or depth:raise RecordLayoutError('initializer quote/nesting')
        if text[at:].strip():parts.append(text[at:].strip())
        return parts
    def quoted(text,quote='"'):
        tokens=re.findall(quote+r'(?:\\.|[^'+quote+r'\\])*'+quote,text)
        if not tokens or ''.join(text.split())!=''.join(''.join(t.split()) for t in tokens):
            # Compare gaps, not literal whitespace, which is itself payload.
            if not tokens or re.sub(quote+r'(?:\\.|[^'+quote+r'\\])*'+quote,'',text).strip():raise RecordLayoutError('literal spelling')
        out=bytearray();escapes={'a':7,'b':8,'f':12,'n':10,'r':13,'t':9,'v':11,'\\':92,'?':63,"'":39,'"':34}
        for token in tokens:
            at=1
            while at<len(token)-1:
                c=token[at];at+=1
                if c!='\\':value=ord(c)
                else:
                    if at>=len(token)-1:raise RecordLayoutError('escape end')
                    c=token[at];at+=1
                    if c in escapes:value=escapes[c]
                    elif c in '01234567':
                        digits=c
                        while at<len(token)-1 and len(digits)<3 and token[at] in '01234567':digits+=token[at];at+=1
                        value=int(digits,8)
                    elif c=='x':
                        start=at
                        while at<len(token)-1 and token[at] in '0123456789abcdefABCDEF':at+=1
                        if at==start:raise RecordLayoutError('hex escape')
                        value=int(token[start:at],16)
                    else:raise RecordLayoutError('unknown C escape')
                if not 0<=value<=255:raise RecordLayoutError('literal byte range')
                out.append(value)
        return bytes(out)
    clean=lambda t:' '.join(re.sub(r'\b(?:const|volatile)\b','',t).split())
    widths={'char':(1,False),'signed char':(1,False),'unsigned char':(1,True),
        'short':(2,False),'short int':(2,False),'signed short':(2,False),'unsigned short':(2,True),'unsigned short int':(2,True),
        'int':(4,False),'signed int':(4,False),'unsigned':(4,True),'unsigned int':(4,True),
        'long':(8,False),'long int':(8,False),'unsigned long':(8,True),'unsigned long int':(8,True),
        'long long':(8,False),'long long int':(8,False),'unsigned long long':(8,True),'unsigned long long int':(8,True)}
    aliases={};records={}
    native_ti=Counter(m[1] for m in source_matches(r'\btypedef\s+unsigned\s+int\s+(\w+)\s+__attribute__\s*\(\(\s*mode\s*\(\s*TI\s*\)\s*\)\)\s*;',preprocessed))
    for m in source_matches(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):aliases.setdefault(m[2],set()).add(clean(m[1]))
    for m in source_matches(r'\b(typedef\s+)?(struct|union)(?:\s+(\w+))?\s*\{',preprocessed):
        at=m.end();end=at;depth=1
        while end<len(preprocessed) and depth:depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
        tail=re.match(r'\s*(\w+)?\s*;',preprocessed[end:])
        tagged_object=(templates_only and not m[1] and m[3] and re.match(
            r'\s*\w+\s*(?:\[\s*(?:0[xX][\da-fA-F]+|\d+)\s*\]\s*)*;',preprocessed[end:]))
        if depth or not tagged_object and (not tail or not m[1] and tail[1]):continue
        names=([tail[1]] if m[1] and tail and tail[1] else [])+([m[2]+' '+m[3]] if m[3] else [])
        for n in names:records.setdefault(n,set()).add((m[2],preprocessed[at:end-1]))
    def layout(typename,seen=(),inline=None):
        typename=clean(typename)
        if typename in seen:raise RecordLayoutError('recursive type')
        if inline is None and typename in native_ti:
            if native_ti[typename]!=1 or typename in aliases or typename in records:
                raise RecordLayoutError('ambiguous native-TI typedef')
            # An actual EE native-TI typedef can establish storage/alignment
            # for an inactive union view. It never decodes an opaque payload.
            return dict(kind='native-ti-storage',size=16,align=16,source_typedef=typename)
        if inline is None and typename in widths:
            width,unsigned=widths[typename];return dict(kind='integer',size=width,align=width,unsigned=unsigned,char='char' in typename)
        if inline is None and typename in ('float','double'):return dict(kind='float',size=4 if typename=='float' else 8,align=4 if typename=='float' else 8)
        if inline is None and len(aliases.get(typename,set()))==1:return layout(next(iter(aliases[typename])),seen+(typename,))
        definitions=records.get(typename,set()) if inline is None else {inline}
        if len(definitions)!=1:raise RecordLayoutError('unknown/ambiguous or pointer type')
        kind,body=next(iter(definitions));fields=[];offset=0;alignment=1;bits=None
        for spelling in split(body,';'):
            bitfield=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*:\s*(\d+)\s*',spelling)
            if bitfield:
                child=layout(bitfield[1],seen+(typename,));width=int(bitfield[3])
                if (kind!='struct' or child['kind']!='integer' or not child['unsigned']
                    or child['size'] not in (4,8) or not 0<width<=child['size']*8):
                    raise RecordLayoutError('unsigned bitfield container')
                if bits is None or bits['used']==bits['size']*8:
                    place=(offset+child['align']-1)//child['align']*child['align']
                    bits=dict(offset=place,size=child['size'],used=0,type=clean(bitfield[1]))
                    offset=place+child['size'];alignment=max(alignment,child['align'])
                if bits['type']!=clean(bitfield[1]) or bits['used']+width>bits['size']*8:
                    raise RecordLayoutError('mixed/crossing bitfield container')
                fields.append(dict(name=bitfield[2],offset=bits['offset'],layout=child,
                    bit_offset=bits['used'],bit_width=width));bits['used']+=width
                continue
            if bits and bits['used']!=bits['size']*8:raise RecordLayoutError('incomplete bitfield container')
            bits=None
            m=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)\s*\]\s*)*)',spelling)
            embedded=re.fullmatch(r'\s*(struct|union)\s*\{(.*)\}\s*(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)\s*\]\s*)*)',spelling,re.S)
            if embedded:
                child=layout(typename+'.'+embedded[3],seen+(typename,),inline=(embedded[1],embedded[2]))
                field_name,dimensions=embedded[3],embedded[4]
            elif m:
                child=layout(m[1],seen+(typename,));field_name,dimensions=m[2],m[3]
            else:
                # A full-width unnamed integer bit-field is explicit storage
                # padding and consumes no initializer. Partial bit-fields remain
                # unsupported; no bit packing is guessed.
                padding=re.fullmatch(r'\s*([\w ]+?)\s*:\s*(\d+)\s*',spelling)
                child=layout(padding[1],seen+(typename,)) if padding else None
                if (kind!='struct' or not child or child['kind']!='integer'
                    or int(padding[2])!=child['size']*8):
                    raise RecordLayoutError('pointer/attribute/unknown field')
                place=(offset+child['align']-1)//child['align']*child['align']
                fields.append(dict(name=None,offset=place,layout=child))
                alignment=max(alignment,child['align']);offset=place+child['size']
                continue
            for bound in reversed(re.findall(r'\[\s*([^\]]+)\s*\]',dimensions)):
                count=int(bound,16 if 'x' in bound.lower() else 10)
                if not 0<count<=65536:raise RecordLayoutError('field count')
                child=dict(kind='array',child=child,count=count,size=count*child['size'],align=child['align'])
            place=0 if kind=='union' else (offset+child['align']-1)//child['align']*child['align']
            fields.append(dict(name=field_name,offset=place,layout=child));alignment=max(alignment,child['align'])
            offset=max(offset,child['size']) if kind=='union' else place+child['size']
        if bits and bits['used']!=bits['size']*8:raise RecordLayoutError('incomplete final bitfield container')
        size=(offset+alignment-1)//alignment*alignment
        if not fields or not 0<size<=65536:raise RecordLayoutError('record extent')
        if kind=='union' and fields[0]['layout']['size']!=size:raise RecordLayoutError('union first view incomplete')
        return dict(kind=kind,size=size,align=alignment,fields=fields)
    unsigned64_types=['unsigned long','unsigned long int','unsigned long long','unsigned long long int']
    for name in aliases:
        try:node=layout(name)
        except RecordLayoutError:continue
        if node['kind']=='integer' and node['size']==8 and node['unsigned']:unsigned64_types.append(name)
    def contains_native_ti(node):
        return (node['kind']=='native-ti-storage' or
            node['kind']=='array' and contains_native_ti(node['child']) or
            node['kind'] in ('struct','union') and any(contains_native_ti(f['layout']) for f in node['fields']))
    def decode(node,text):
        text=text.strip();kind=node['kind']
        if kind=='native-ti-storage':raise RecordLayoutError('native-TI view is storage only')
        if kind=='integer':
            if re.fullmatch(r"'(?:\\.|[^'\\])*'",text):
                data=quoted(text,"'")
                if len(data)!=1:raise RecordLayoutError('multi-character literal')
                n=data[0]
            else:
                try:n=literal_integer_expression(text,unsigned64_types)
                except (ValueError,SyntaxError,RecursionError):raise RecordLayoutError('integer expression') from None
            bits=node['size']*8
            if not -(1<<(bits-1))<=n<1<<(bits if node['unsigned'] else bits-1):raise RecordLayoutError('integer conversion range')
            return (n%(1<<bits)).to_bytes(node['size'],'little')
        if kind=='float':
            if not re.fullmatch(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?[fF]?',text):raise RecordLayoutError('float expression')
            value=float(re.sub(r'[fF]$','',text))
            if not math.isfinite(value):raise RecordLayoutError('nonfinite literal')
            try:return struct.pack('<f' if node['size']==4 else '<d',value)
            except (OverflowError,struct.error):raise RecordLayoutError('floating conversion') from None
        if kind=='array' and node['child']['kind']=='integer' and node['child'].get('char') and text.startswith('"'):
            payload=quoted(text)+b'\0'
            if len(payload)>node['count']:raise RecordLayoutError('string capacity')
            return payload+bytes(node['count']-len(payload))
        if not text.startswith('{') or not text.endswith('}'):raise RecordLayoutError('explicit aggregate braces')
        items=split(text[1:-1]);result=bytearray(node['size'])
        if kind=='array':
            if len(items)>node['count']:raise RecordLayoutError('array excess initializer')
            if contains_native_ti(node) and len(items)!=node['count']:
                raise RecordLayoutError('native-TI union array requires every active element')
            for i,item in enumerate(items):
                if item.startswith('.') or item.startswith('['):raise RecordLayoutError('array designator unsupported')
                payload=decode(node['child'],item);start=i*node['child']['size'];result[start:start+len(payload)]=payload
        else:
            selected=node['fields'][:1] if kind=='union' else node['fields']
            if kind=='union' and len(items)==1:
                active=re.match(r'\.(\w+)\s*=\s*',items[0])
                if active:
                    selected=[f for f in node['fields'] if f['name']==active[1]]
                    if len(selected)!=1 or selected[0]['layout']['size']!=node['size']:
                        raise RecordLayoutError('incomplete explicit union initializer')
            fields=[f for f in selected if f['name'] is not None]
            if len(items)>len(fields):raise RecordLayoutError('record excess initializer')
            if kind=='union' and contains_native_ti(node):
                if len(items)!=1 or len(fields)!=1 or fields[0]['layout']['size']!=node['size']:
                    raise RecordLayoutError('native-TI union requires one complete active view')
                active_text=re.sub(r'^\s*\.\w+\s*=\s*','',items[0]).strip()
                active=fields[0]['layout']
                if (active['kind']!='array' or active['child']['kind']!='integer'
                    or not active_text.startswith('{') or not active_text.endswith('}')
                    or len(split(active_text[1:-1]))!=active['count']):
                    raise RecordLayoutError('native-TI union requires explicit complete scalar slots')
            cursor=0
            for item in items:
                designator=re.match(r'\.(\w+)\s*=\s*',item)
                if designator:
                    positions=[i for i,f in enumerate(fields) if f['name']==designator[1]]
                    if len(positions)!=1 or positions[0]<cursor:raise RecordLayoutError('record designator order')
                    cursor=positions[0]
                    item=item[designator.end():]
                if cursor>=len(fields):raise RecordLayoutError('record excess initializer')
                field=fields[cursor];cursor+=1;payload=decode(field['layout'],item)
                if 'bit_width' in field:
                    integer=int.from_bytes(payload,'little')
                    if integer>=1<<field['bit_width']:raise RecordLayoutError('bitfield conversion range')
                    start=field['offset'];end=start+len(payload)
                    combined=int.from_bytes(result[start:end],'little') | integer<<field['bit_offset']
                    result[start:end]=combined.to_bytes(len(payload),'little')
                else:result[field['offset']:field['offset']+len(payload)]=payload
        return bytes(result)
    emitted=typed_compiler_data(assembly);result={}
    declarations=source_matches(r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)?\s*\]\s*)*)\s*(?:__attribute__\s*\(\(\s*(?:section\s*\(\s*"\.(?:data|rodata|sdata)"\s*\)|aligned\s*\(\s*(?:1|2|4|8|16|32|64|128|256)\s*\))\s*\)\)\s*)?=\s*',preprocessed)
    for declaration in declarations:
        typename,name=clean(declaration[1]),declaration[2]
        if name not in emitted and not templates_only:continue
        try:
            node=layout(typename);at=declaration.end();end=at;depth=0;quote=None;escape=False
            while end<len(preprocessed):
                c=preprocessed[end]
                if quote:
                    if escape:escape=False
                    elif c=='\\':escape=True
                    elif c==quote:quote=None
                elif c in '\"\'':quote=c
                elif c=='{':depth+=1
                elif c=='}':depth-=1
                elif c==';' and depth==0:break
                end+=1
            if end==len(preprocessed) or quote or depth:continue
            text=preprocessed[at:end].strip();bounds=re.findall(r'\[\s*([^\]]*)\s*\]',declaration[3])
            for b in reversed(bounds):
                if b:count=int(b,16 if 'x' in b.lower() else 10)
                elif len(bounds)==1 and text.startswith('{'):count=len(split(text[1:-1]))
                elif len(bounds)==1 and node['kind']=='integer' and node.get('char'):count=len(quoted(text))+1
                else:raise RecordLayoutError('inferred array extent')
                if not 0<count<=65536:raise RecordLayoutError('array count')
                node=dict(kind='array',child=node,count=count,size=count*node['size'],align=node['align'])
            if node['size']>65536:continue
            raw=decode(node,text)
        except (RecordLayoutError,ValueError,OverflowError):continue
        if templates_only:
            result[declaration.start()]=dict(name=name,source_type=typename,source_offset=declaration.start(),
                explicit_extent=len(raw),payload=raw,independent_typed_layout=node)
            continue
        symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and s.size==len(raw) and 0<s.shndx<len(compiler.sections) and compiler.sections[s.shndx].type==1]
        if len(symbols)!=1:continue
        symbol=symbols[0];section=compiler.sections[symbol.shndx]
        if (emitted[name]['section'],emitted[name]['size'])!=(section.name,len(raw)) or symbol.value%node['align'] or section.align<node['align']:continue
        explicit_alignment=re.search(r'\baligned\s*\(\s*(\d+)\s*\)',declaration[0])
        if explicit_alignment and (symbol.value%int(explicit_alignment[1]) or section.align<int(explicit_alignment[1])):continue
        if compiler.section_bytes(section)[symbol.value:symbol.value+symbol.size]!=raw:continue
        relocations=[rel for rel in compiler.sections if rel.type in (9,4) and rel.info==section.index]
        if any(rel.entsize not in (0,8 if rel.type==9 else 12) or rel.size%(8 if rel.type==9 else 12)
               or rel.offset<0 or rel.offset+rel.size>len(compiler.data) for rel in relocations):continue
        if any(any(symbol.value<=struct.unpack_from('<I',compiler.data,p)[0]<symbol.value+symbol.size
            for p in range(rel.offset,rel.offset+rel.size,8 if rel.type==9 else 12)) for rel in relocations):continue
        result[name]=dict(raw=raw,integer_offsets=None,source_type=typename,kind='complete-typed-literal-object',symbol=symbol,
            typed_literal_initializer_proof=dict(layout=node,explicit_storage_alignment=int(explicit_alignment[1]) if explicit_alignment else None,complete_decoded_sha256=hashlib.sha256(raw).hexdigest(),
                no_pointers_or_unknown_expressions=True,standard_initializer_zero_fill=True,cc1_storage=emitted[name]))
    if templates_only:
        # A const array compound literal has a complete source type and
        # initializer, but no named ELF object. Reuse the identical independent
        # layout/decoder; the caller still needs actual cc1 bytes, affine
        # placement and original instruction consumers. Quoted source text and
        # comments cannot supply this expression witness.
        masked=source_discovery_view(preprocessed)
        expressions=re.finditer(r'\(\s*const\s+([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s*\[\s*(\d+)\s*\]\s*\)\s*\{',masked)
        for expression in expressions:
            typename=clean(expression[1]);count=int(expression[2]);at=expression.end()-1;end=at;depth=0;quote=None;escape=False
            while end<len(preprocessed):
                char=preprocessed[end]
                if quote:
                    if escape:escape=False
                    elif char=='\\':escape=True
                    elif char==quote:quote=None
                elif char in '\"\'':quote=char
                elif char=='{':depth+=1
                elif char=='}':
                    depth-=1
                    if not depth:end+=1;break
                end+=1
            if depth or quote or not 0<count<=65536:continue
            try:
                child=layout(typename)
                node=dict(kind='array',child=child,count=count,size=count*child['size'],align=child['align'])
                if node['size']>65536:continue
                raw=decode(node,preprocessed[at:end])
            except (RecordLayoutError,ValueError,OverflowError):continue
            result[expression.start()]=dict(name=None,source_type=typename,source_offset=expression.start(),
                source_expression='const array compound literal',explicit_extent=len(raw),payload=raw,
                independent_typed_layout=node)
    return result


@lru_cache(maxsize=8)
def numeric_source_objects(preprocessed, compiler, assembly):
    """Literal-only numeric source objects, with independently emitted storage.

    The initial struct route resolves existing field types and refuses pointers
    and unknown layouts. Separate typed routes decode explicit unsigned
    bitfields, complete unions and bounded literal arithmetic. Compiler bytes
    provide the layout witness, never the initializer hypothesis. A bounded
    record route admits four-byte int fields among pointer fields; it proves
    each scalar's literal value and offset and leaves all pointer words alone.
    """
    emitted = typed_compiler_data(assembly)
    static_bindings=function_static_source_bindings(preprocessed,compiler,assembly)
    scalar = {'char','short','short int','int','long','long int','long long','long long int','float','double'}
    integers = {'int'}
    aliases, records, pointer_types = {}, {}, set()
    enumerators={}
    for declaration in source_matches(r'\benum(?:\s+\w+)?\s*\{([^{}]+)\}',preprocessed):
        values={};value=-1
        for item in declaration[1].split(','):
            if not item.strip():continue
            match=re.fullmatch(r'\s*(\w+)\s*(?:=\s*(0[xX][\da-fA-F]+|\d+))?\s*',item)
            if not match:break
            value=int(match[2],0) if match[2] else value+1;values[match[1]]=value
        else:enumerators.update(values)
    clean = lambda t: ' '.join(re.sub(r'\b(?:const|volatile|signed|unsigned)\b','',t).split())
    for m in source_matches(r'\btypedef\s+([^;{}]+?)\s+(\w+)\s*;',preprocessed):
        if '*' not in m[1] and '(' not in m[1]:aliases[m[2]]=clean(m[1])
    for m in source_matches(r'\btypedef\s+[^;{}]+\(\s*\*\s*(\w+)\s*\)\s*\([^;{}]*\)\s*;',preprocessed):
        pointer_types.add(m[1])
    for m in source_matches(r'\btypedef\s+struct(?:\s+\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;',preprocessed):
        records[m[2]]=m[1]
    for m in source_matches(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}\s*;',preprocessed):
        records['struct '+m[1]]=m[2]
    def numeric(typename, seen=()):
        typename=clean(typename)
        if typename in seen:return False
        if typename in scalar:return True
        if typename in aliases:return numeric(aliases[typename],seen+(typename,))
        if typename not in records:return False
        fields=[f.strip() for f in records[typename].split(';') if f.strip()]
        return bool(fields) and all((m:=re.fullmatch(r'([\w ]+?)\s+\w+((?:\s*\[\s*\w+\s*\])*)',f))
                                   and all(re.fullmatch(r'0[xX][\da-fA-F]+|\d+',b) or enumerators.get(b,0)>0
                                           for b in re.findall(r'\[\s*(\w+)\s*\]',m[2]))
                                   and numeric(m[1],seen+(typename,)) for f in fields)
    def int32(typename,seen=()):
        typename=clean(typename)
        return typename in integers or typename in aliases and typename not in seen and int32(aliases[typename],seen+(typename,))
    def numeric_layout(typename,seen=()):
        """Contractual EE integer layout; unknown members end prefix proof."""
        typename=clean(typename)
        if typename in seen:return None
        if typename in aliases:return numeric_layout(aliases[typename],seen+(typename,))
        widths={'char':1,'short':2,'short int':2,'int':4,'long':8,'long int':8,'long long':8,'long long int':8}
        if typename in widths:return widths[typename],widths[typename],[(0,widths[typename],'')]
        if typename not in records:return None
        offset,alignment,leaves=0,1,[]
        for field in records[typename].split(';'):
            if not field.strip():continue
            match=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*',field)
            child=numeric_layout(match[1],seen+(typename,)) if match else None
            if not child:return None
            size,align,members=child;offset=(offset+align-1)//align*align;alignment=max(alignment,align)
            leaves.extend((offset+place,width,match[2]+('.'+path if path else '')) for place,width,path in members)
            offset+=size
        return (offset+alignment-1)//alignment*alignment,alignment,leaves
    number=r'[-+]?(?:0[xX][0-9a-fA-F]+|(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)[uUlLfF]*'
    token=re.compile(r'\s*(?:[{},]|\.\w+\s*=|'+number+r')')
    result={}
    pattern=r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)?\s*\])*)\s*=\s*'
    for declaration in source_matches(pattern,preprocessed):
        typename,name=clean(declaration[1]),declaration[2]
        lexical_static=None
        if name not in emitted:
            # Scope, lexical offset and a unique actual function consumer are
            # required; matching a numeric suffix by spelling is insufficient.
            choices=[(actual,proof) for actual,proof in static_bindings.items()
                     if proof['source_lexical_name']==name and actual in emitted and
                     proof['source_declaration_offset']==declaration.start()]
            if len(choices)!=1:continue
            name,lexical_static=choices[0]
        pos=declaration.end()
        if pos>=len(preprocessed):continue
        if preprocessed[pos]=='{':
            depth,end=1,pos+1
            while end<len(preprocessed) and depth:
                depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
            if depth:continue
            initializer=preprocessed[pos:end]
        else:
            end=preprocessed.find(';',pos)
            if end<0:continue
            initializer=preprocessed[pos:end].strip()
        symbols=[s for s in compiler.symbols if s.name==name and s.type==1 and 0<s.shndx<len(compiler.sections)
                 and compiler.sections[s.shndx].type==1 and s.size==emitted[name]['size']
                 and compiler.sections[s.shndx].name==emitted[name]['section']]
        if len(symbols)!=1:continue
        symbol=symbols[0];section=compiler.sections[symbol.shndx]
        relocated=set()
        for rel in compiler.sections:
            if rel.type==9 and rel.info==section.index:
                relocated.update(struct.unpack_from('<I',compiler.data,rel.offset+i)[0]-symbol.value for i in range(0,rel.size,rel.entsize)
                                 if symbol.value<=struct.unpack_from('<I',compiler.data,rel.offset+i)[0]<symbol.value+symbol.size)
        raw=compiler.section_bytes(section)[symbol.value:symbol.value+symbol.size]
        at=0
        while at<len(initializer):
            m=token.match(initializer,at)
            if not m:break
            at=m.end()
        if numeric(typename) and at==len(initializer) and not relocated:
            result[name]=dict(raw=raw,integer_offsets=None,source_type=typename,kind='literal-only-numeric-object',symbol=symbol)
            continue
        # A designated numeric prefix remains provable even when a later
        # member is a union or pointer. Never infer offsets after that member.
        if typename in records and not declaration[3] and initializer.startswith('{'):
            prefix_offset=0;offsets={}
            for field in records[typename].split(';'):
                if not field.strip():continue
                match=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*',field)
                layout=numeric_layout(match[1]) if match else None
                if not layout:break
                size,align,members=layout;prefix_offset=(prefix_offset+align-1)//align*align
                value_match=re.search(r'\.'+re.escape(match[2])+r'\s*=\s*',initializer)
                if value_match:
                    start=value_match.end();end=start
                    if initializer[start:start+1]=='{':
                        depth=1;end=start+1
                        while end<len(initializer) and depth:
                            depth+=(initializer[end]=='{')-(initializer[end]=='}');end+=1
                        value=initializer[start:end]
                    else:
                        end=next((i for i in range(start,len(initializer)) if initializer[i] in ',}'),len(initializer))
                        value=initializer[start:end].strip()
                    # Explicit literal integer arithmetic only; no names,
                    # casts, omitted slots or pointer/union values.
                    numbers=[v.strip() for v in re.split(r'[{},]',value) if v.strip()]
                    try:integers_decoded=[literal_integer_expression(v) for v in numbers]
                    except (ValueError,SyntaxError,RecursionError):integers_decoded=[]
                    if len(integers_decoded)==len(members):
                        for literal,integer,(place,width,path) in zip(numbers,integers_decoded,members):
                            if width!=4:continue
                            if not -(1<<31)<=integer<1<<32:continue
                            position=prefix_offset+place;expected=(integer&0xffffffff).to_bytes(4,'little')
                            if position not in relocated and raw[position:position+4]==expected:
                                offsets[position]=dict(member=match[2]+('.'+path if path else ''),offset=position,
                                                       literal=literal,raw=expected,field_width=width)
                prefix_offset+=size
            if offsets:
                result[name]=dict(raw=raw,integer_offsets=offsets,source_type=typename,
                                  kind='literal-numeric-record-prefix',symbol=symbol)
                continue
        # Exact ordinary array of equal-width EE int/function-pointer fields.
        # No arrays, padding, attributes, unions or implicit initializer slots.
        if typename not in records or not re.fullmatch(r'\[\s*\d+\s*\]',declaration[3]):continue
        fields=[]
        for f in records[typename].split(';'):
            if not f.strip():continue
            m=re.fullmatch(r'\s*([\w ]+?)\s+(\*\s*(?:\*\s*)*)?(\w+)\s*',f)
            if not m or not (m[2] or int32(m[1]) or clean(m[1]) in pointer_types):break
            # Explicit EE pointer declarators occupy four bytes. They only
            # establish layout here; their initializer/relocations are never
            # admitted as integer values or changed by the numeric proof.
            fields.append(('explicit-pointer' if m[2] else clean(m[1]),m[3]))
        else:
            count=int(declaration[3].strip()[1:-1]);stride=len(fields)*4
            items=re.findall(r'\{([^{}]+)\}',initializer[1:-1])
            if not fields or len(items)!=count or symbol.size!=stride*count:continue
            offsets={}
            for index,item in enumerate(items):
                values=[v.strip() for v in item.split(',')]
                if len(values)!=len(fields):break
                for field,(ctype,member) in enumerate(fields):
                    if not int32(ctype):continue
                    value=values[field]
                    if not re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*',value):break
                    literal=re.sub(r'[uUlL]+$','',value);value_int=int(literal,16 if 'x' in literal.lower() else 10)
                    offset=index*stride+field*4
                    expected=(value_int&0xffffffff).to_bytes(4,'little')
                    if offset not in relocated and raw[offset:offset+4]==expected:
                        offsets[offset]=dict(member=member,element=index,offset=offset,literal=literal,raw=expected)
            if offsets:result[name]=dict(raw=raw,integer_offsets=offsets,source_type=typename,kind='literal-int-field-layout',symbol=symbol)
    for name,proof in numeric_union_source_objects(preprocessed,compiler,assembly).items():
        if name not in result:result[name]=proof
    for name,proof in numeric_record_union_source_objects(preprocessed,compiler,assembly).items():
        if name not in result:result[name]=proof
    # Prefer the independently decoded char witness over the legacy generic
    # numeric token admission, retaining all downstream original-byte guards.
    result.update(numeric_character_source_objects(preprocessed,compiler,assembly))
    for name,proof in numeric_byte_expression_source_objects(preprocessed,compiler,assembly).items():
        if name not in result:result[name]=proof
    for name,proof in typed_literal_source_objects(preprocessed,compiler,assembly).items():
        if name not in result or result[name]['integer_offsets'] is not None:result[name]=proof
    return result


def original_data(reference, source, original_path, destination, cwd, obj,
                  preprocessed, assembler_prefix, extent, compiler_object=None, compiler_assembly=None,
                  registry=None):
    """Reassemble evidence-proven numeric data and unreferenced zero padding.

    Preserve every section byte and extent. Literal numeric source objects
    require an exact original OBJECT and an exact initializer. Mixed records
    admit only separately proven literal int fields. Actual pointers remain
    symbolic; compiler DATA relocations never qualify for removal.
    Padding identity is handled separately by the joint-reference helper,
    which requires independent original and compiler owner provenance.
    """
    body = source.read_text()
    if '.word' not in body and 'Automatically generated and unreferenced pad' not in body:
        return reference
    original = parsed_elf(original_path.read_bytes())
    before = parsed_elf(reference.read_bytes())
    arrays = integer_literal_arrays(preprocessed) if preprocessed else {}
    numeric_objects = (numeric_source_objects(preprocessed,parsed_elf(compiler_object.read_bytes()),compiler_assembly.read_text())
                       if compiler_object and compiler_assembly else {})
    section_name = re.search(r'^\.section\s+(\.[\w.]+)', body, re.M)
    if section_name is None or section_name[1] in ('.bss', '.sbss'):
        return reference
    section = before.section(section_name[1])
    loaded = original.section(section.name)
    expected = bytearray(before.section_bytes(section))
    mapped_section_extent = extent
    changes, removed, proofs = [], set(), []
    block = re.compile(r'(?:(?:nonmatching\s+(\S+)\s*)?'
                       r'(?:/\* Automatically generated and unreferenced pad \*/\s*)?)'
                       r'dlabel\s+(\S+)\s*\n(.*?)enddlabel\s+\2\s*\n'
                       r'\.size\s+\2,\s*\.\s*-\s*\2', re.S)
    word = re.compile(r'^(\s*/\*\s+\S+\s+([0-9A-Fa-f]{8})\s+'
                      r'([0-9A-Fa-f]{8})\s+\*/\s*\.word\s+)([^\n]+)$', re.M)

    def replace(match):
        name = match[2]
        symbol = next((s for s in before.symbols if s.name == name and s.shndx == section.index), None)
        rows = list(word.finditer(match[3]))
        if symbol is None or not rows:
            return match[0]
        mixed_directives = len(rows) * 4 != symbol.size
        if mixed_directives:
            # Splat can print an integer's leading bytes as asciz+alignment.
            # Only the independently assembled full TU section coordinates
            # identify its object; never derive that start from word counts.
            if (not mapped_section_extent or section.size != mapped_section_extent[1]-mapped_section_extent[0]
                or not 0 <= symbol.value < symbol.value+symbol.size <= section.size):
                return match[0]
            begin = mapped_section_extent[0] + symbol.value
        else:
            begin = int(rows[0][2], 16)
        end = begin + symbol.size
        if not (loaded.addr <= begin < end <= loaded.addr + loaded.size):
            return match[0]
        raw = original.data[loaded.offset + begin - loaded.addr:loaded.offset + end - loaded.addr]
        positions = [int(row[2],16)-begin for row in rows]
        def require_word_positions():
            if (len(set(positions)) != len(positions) or
                any(offset < 0 or offset+4 > symbol.size or
                    (not mixed_directives and offset != i*4) or
                    bytes.fromhex(row[3]) != raw[offset:offset+4]
                    for i,(row,offset) in enumerate(zip(rows,positions))) or
                any(right < left+4 for left,right in zip(sorted(positions),sorted(positions)[1:]))):
                raise RuntimeError(f'{obj["id"]}/{name}: stale original data disassembly')
        if not mixed_directives:
            require_word_positions()
        object_symbols = [s for s in original.symbols if s.type == 1 and s.value == begin
                          and s.size == symbol.size and s.shndx == loaded.index]
        source_proof=numeric_objects.get(name)
        if source_proof is None and len(object_symbols)==1:
            original_name=object_symbols[0].name
            normalized=original_name.replace('.', '_')
            names={original_name, normalized, f'{normalized}_{begin:08X}'}
            candidates=[proof for cname,proof in numeric_objects.items() if cname in names]
            if len(candidates)==1:source_proof=candidates[0]
            if source_proof is None:
                # An accepted exact owner alias can use a semantic C name.
                # Require both independently registered coordinates and the
                # current actual compiler object, not spelling or equal bytes.
                aliases=[]
                for registered in (registry or {}).get(section.name,[]):
                    for alias in registered.get('c_storage_aliases',[]):
                        owner=alias.get('storage_owner',{})
                        proof=numeric_objects.get(owner.get('name'))
                        if not proof or alias.get('original_name')!=original_name:continue
                        cs=proof['symbol'];coords=[begin,end]
                        if (alias.get('address','').lower()!=f'0x{begin:08x}' or
                            owner.get('address','').lower()!=f'0x{begin:08x}' or owner.get('size')!=symbol.size or
                            owner.get('name') not in registered.get('c_owned_symbols',[]) or
                            compiler_object is None):continue
                        compiled=parsed_elf(compiler_object.read_bytes())
                        for span in registered.get('c_input_spans',[]):
                            extent=explicit_object_range(span)
                            if not extent or span.get('section')!=compiled.sections[cs.shndx].name or owner['name'] not in span.get('symbols',[]):continue
                            mapped=[int(v,16) for v in span.get('range',[])]
                            if len(mapped)!=2 or mapped[1]-mapped[0]!=extent[1]-extent[0]:continue
                            if extent[0]<=cs.value<cs.value+cs.size<=extent[1] and [mapped[0]+cs.value-extent[0],mapped[0]+cs.value+cs.size-extent[0]]==coords:
                                aliases.append((proof,dict(original_name=original_name,storage_owner=owner,
                                    c_input_span=span,compiler_symbol_offset=cs.value)))
                if len(aliases)==1:
                    source_proof={**aliases[0][0], 'registered_numeric_alias':aliases[0][1]}
        compiled_exact=bool(source_proof and len(source_proof['raw'])==len(raw) and
                            len(object_symbols)==1 and
                            (source_proof['symbol'].bind,source_proof['symbol'].type,source_proof['symbol'].size)==
                            (object_symbols[0].bind,object_symbols[0].type,object_symbols[0].size) and
                            (source_proof['raw']==raw or source_proof['integer_offsets']))
        registered_numeric_span=None
        complete_kinds=('literal-only-numeric-object','literal-only-character-array','complete-typed-literal-object',
                        'literal-only-numeric-union-object','literal-only-unsigned-byte-expression-array')
        proven_owner_prefix=None
        if (source_proof and source_proof['kind'] in complete_kinds and source_proof['integer_offsets'] is None
            and 0<len(source_proof['raw'])<len(raw) and raw.startswith(source_proof['raw'])):
            # A mapped scaffold label can include inter-object padding. Do not
            # resize it or credit its tail: restore only complete four-byte
            # words inside an independently established original/C owner.
            cs=source_proof['symbol'];owner_end=begin+cs.size
            genuine=[s for s in original.symbols if s.shndx==loaded.index and s.value==begin and s.type==1
                     and (s.size,s.bind)==(cs.size,cs.bind)]
            overlaps=[s for s in original.symbols if s.shndx==loaded.index and s.name and s.type in (0,1,2)
                      and s.value<owner_end and begin<s.value+max(s.size,1)]
            spans=[span for run in (registry or {}).get(section.name,[]) for span in run.get('c_input_spans',[])
                   if name in run.get('c_owned_symbols',[]) and span.get('section')==section.name
                   and name in span.get('symbols',[]) and [int(v,16) for v in span.get('range',[])]==[begin,owner_end]
                   and explicit_object_range(span)==[cs.value,cs.value+cs.size]]
            if len(spans)==1 and (len(genuine)==1 and overlaps==genuine or not overlaps and re.fullmatch(r'D_'+f'{begin:08X}',name)):
                proven_owner_prefix=dict(registered_exact_owner=spans[0],original_owner_extent=[begin,owner_end],
                    original_OBJECT=genuine[0].name if genuine else None,scaffold_extent=[begin,end],
                    untouched_uncredited_tail=len(raw)-cs.size)
                object_symbols=genuine;compiled_exact=True;registered_numeric_span=spans[0] if not genuine else None
        if (not object_symbols and source_proof and source_proof['kind'] in complete_kinds
            and source_proof['raw']==raw and (re.fullmatch(r'D_'+f'{begin:08X}',name) or source_proof['kind']=='literal-only-character-array')
            and (source_proof['symbol'].type,source_proof['symbol'].size)==(symbol.type,symbol.size)
            and (source_proof['symbol'].bind==symbol.bind or
                 source_proof['kind']=='literal-only-character-array' and source_proof['symbol'].bind in (0,1))
            and not any(s.name and s.shndx==loaded.index and s.type in (0,1,2)
                        and s.value<end and begin<s.value+max(s.size,1) for s in original.symbols)):
            cs=source_proof['symbol']
            spans=[span for run in (registry or {}).get(section.name,[]) for span in run.get('c_input_spans',[])
                   if name in run.get('c_owned_symbols',[]) and span['section']==section.name
                   and name in span['symbols'] and [int(v,16) for v in span['range']]==[begin,end]
                   and explicit_object_range(span)==[cs.value,cs.value+cs.size]]
            if len(spans)==1:
                registered_numeric_span=spans[0];compiled_exact=True
        mixed_directive_proof=None
        if mixed_directives:
            # Validate the independently proven owner before considering a
            # mixed scaffold block. Both genuine OBJECTs and previously
            # registered exact anonymous C owners retain complete extents.
            if (not compiled_exact or source_proof['kind'] not in complete_kinds
                or source_proof['integer_offsets'] is not None or (source_proof['raw'] != raw and not proven_owner_prefix)):
                return match[0]
            require_word_positions()
            mixed_directive_proof=dict(section_extent=list(mapped_section_extent),
                assembled_object_offset=symbol.value,assembled_object_size=symbol.size,
                original_OBJECT=object_symbols[0].name if object_symbols else None,original_OBJECT_address=begin,
                complete_source_initializer_sha256=hashlib.sha256(source_proof['raw']).hexdigest(),
                registered_exact_anonymous_owner=registered_numeric_span,
                proven_original_C_owner_prefix=proven_owner_prefix,
                word_offsets=positions,nonword_directives_unchanged=True)
        if ((name in arrays and arrays[name] == raw and len(object_symbols)==1) or compiled_exact):
            def numeric(row):
                target = re.fullmatch(r'([A-Za-z_$][\w.$]*)(?:\s*\+\s*(0x[0-9A-Fa-f]+))?\s*', row[4])
                if target is None:
                    return row[0]
                relative=int(row[2],16)-begin
                if source_proof and source_proof['integer_offsets'] is not None and relative not in source_proof['integer_offsets']:
                    return row[0]
                if source_proof and source_proof['raw'][relative:relative+4] != raw[relative:relative+4]:
                    return row[0]
                synthetic=re.fullmatch(r'(?:D_|func_)([0-9A-Fa-f]+)',target[1])
                if target[1].startswith('func_') and not compiled_exact:return row[0]
                if registered_numeric_span and source_proof['kind'] not in complete_kinds:return row[0]
                identities=[s for s in original.symbols if target[1] in (s.name,s.name.replace('.', '_'),s.name.replace('.', '_')+f'_{s.value:08X}') and s.shndx!=0]
                if not synthetic and len(identities)!=1:return row[0]
                association = int(synthetic[1],16) if synthetic else identities[0].value
                if not compiled_exact and (not synthetic or any(s.type in (1, 2) and s.shndx not in (0, 0xfff1)
                       and s.value <= association < s.value + max(s.size, 1) for s in original.symbols)):
                    return row[0]
                va = int(row[2], 16)
                if any(rel.type == 9 and any(struct.unpack_from('<I', original.data, rel.offset + i)[0] == va
                       for i in range(0, rel.size, rel.entsize)) for rel in original.sections):
                    return row[0]
                value = int.from_bytes(bytes.fromhex(row[3]), 'little')
                verification.require(value == association + (int(target[2], 16) if target[2] else 0),
                                     f'{obj["id"]}/{name}: inferred relocation does not spell original literal')
                local = symbol.value + va - begin
                expected[local:local + 4] = bytes.fromhex(row[3])
                changes.append(local)
                proofs.append(dict(object_name=object_symbols[0].name if object_symbols else None,
                                   original_object_present=bool(object_symbols),
                                   mixed_directive_original_object_proof=mixed_directive_proof,
                                   registered_numeric_source_span=registered_numeric_span,
                                   proven_original_C_owner_prefix=proven_owner_prefix,
                                   object_address=f'0x{begin:08x}', object_size=symbol.size,
                                   source_integer_array=name, initializer_sha256=hashlib.sha256(raw).hexdigest(),
                                   source_numeric_proof=({k:v for k,v in source_proof.items() if k in ('source_type','kind','union_active_numeric_member','registered_numeric_alias','character_initializer_proof','source_byte_expression_proof','typed_literal_initializer_proof')}
                                                         if source_proof else None),
                                   source_integer_field=({k:v for k,v in source_proof['integer_offsets'][relative].items() if k!='raw'}
                                                         if source_proof and source_proof['integer_offsets'] is not None else None),
                                   numeric_address=f'0x{va:08x}', inferred_target=row[4].strip()))
                return f'{row[1]}0x{value:08X}'
            return match[0].replace(match[3], word.sub(numeric, match[3]), 1)
        return match[0]

    rewritten = block.sub(replace, body)
    if rewritten == body:
        return reference
    assembly = destination.with_suffix('.s')
    assembly.write_text(rewritten)
    run([str(assembler_prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64',
         '-G0', '-no-pad-sections', '-mno-fix-r5900', '-I.', '-Iinclude', '-o', destination,
         assembly], cwd)
    after = parsed_elf(destination.read_bytes())
    for old in before.sections:
        if old.flags & 2:
            new = after.section(old.name)
            verification.require((old.type, old.flags, old.size, old.align) ==
                                 (new.type, new.flags, new.size, new.align)
                                 and after.section_bytes(new) ==
                                 (bytes(expected) if old.name == section.name else before.section_bytes(old)),
                                 f'{obj["id"]}: data-reference correction changed unrelated bytes or extent')
    identities = lambda elf: {(s.name, s.value, s.size, s.bind, s.type)
                              for s in elf.symbols if s.type in (1, 2) and s.name not in removed}
    verification.require(identities(before) == identities(after),
                         f'{obj["id"]}: data-reference correction changed original object identities')
    def relocations(elf):
        return {(elf.sections[rel.info].name, offset, info & 0xff,
                 next(s.name for s in elf.symbols if s.index == info >> 8))
                for rel in elf.sections if rel.type == 9
                for i in range(0, rel.size, rel.entsize)
                for offset, info in [struct.unpack_from('<II', elf.data, rel.offset + i)]}
    retained = {r for r in relocations(before) if r[0] != section.name or r[1] not in changes}
    verification.require(relocations(after) == retained,
                         f'{obj["id"]}: correction changed a genuine retained data relocation')
    for rel in after.sections:
        if rel.type == 9 and rel.info == after.section(section.name).index:
            relocated = {struct.unpack_from('<I', after.data, rel.offset + i)[0]
                         for i in range(0, rel.size, rel.entsize)}
            verification.require(not relocated.intersection(changes),
                                 f'{obj["id"]}: integer initializer retained an invented relocation')
    destination.with_suffix('.proof.json').write_text(json.dumps(dict(
        schema='objdiff-original-data-reference/1', tu=obj['id'], source=str(source),
        original_sha256=hashlib.sha256(original.data).hexdigest(),
        source_sha256=hashlib.sha256(body.encode()).hexdigest(), integer_literal_proofs=proofs,
        numeric_offsets=changes, removed_unreferenced_padding_objects=sorted(removed),
        sections_preserved=True, original_initializer_bytes_verified=True), indent=2) + '\n')
    return destination


def source_alignment_padding(part, original, compiled, assembly, registry, tu, scaffold):
    """Remove only fake zero-pad objects after an original and C owner."""
    emitted=typed_compiler_data(assembly);proofs=[]
    section_match=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
    if not section_match or section_match[1] in ('.bss','.sbss'):return part,proofs
    section=original.section(section_match[1])
    block=re.compile(r'(?:nonmatching\s+(D_[\da-fA-F]+)(?:\s*,[^\n]+)?\s*\n\s*)?(?:/\* Automatically generated and unreferenced pad \*/\s*)?dlabel\s+(D_[\da-fA-F]+)\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*\.\s*-\s*\2',re.S)
    def replace(match):
        name=match[2];symbols=[s for s in scaffold.symbols if s.name==name and s.type==1]
        if len(symbols)!=1:return match[0]
        length=symbols[0].size;lo=int(name[2:],16);hi=lo+length
        if not 0<length<=15 or not section.addr<=lo<hi<=section.addr+section.size:return match[0]
        raw=original.data[section.offset+lo-section.addr:section.offset+hi-section.addr]
        if any(raw) or any(s.shndx==section.index and s.name and
                           (s.type in (1,2) and s.value<hi and lo<s.value+max(s.size,1) or lo<=s.value<hi)
                           for s in original.symbols):return match[0]
        owners=[s for s in original.symbols if s.shndx==section.index and s.type==1 and s.size>0 and s.value+s.size==lo]
        if len(owners)!=1:return match[0]
        owner=owners[0];spellings={owner.name,owner.name.replace('.','_'),owner.name.replace('.','_')+f'_{owner.value:08X}'}
        for record in registry.get(section.name,[]):
            for alias in record.get('c_storage_aliases',[]):
                storage=alias.get('storage_owner',{})
                if (alias.get('original_name')==owner.name and int(alias['address'],16)==owner.value
                    and int(storage.get('address','0'),16)==owner.value and storage.get('size')==owner.size):
                    spellings.add(storage['name'])
        candidates=[]
        for s in compiled.symbols:
            if s.name not in spellings or not 0<s.shndx<len(compiled.sections):continue
            csec=compiled.sections[s.shndx];emission=emitted.get(s.name)
            if (s.bind,s.type,s.size)!=(owner.bind,owner.type,owner.size) or csec.name!=section.name or not emission:continue
            if (emission['section'],emission['size'])!=(section.name,owner.size):continue
            owned=[r for r in registry.get(section.name,[]) if int(r['range'][0],16)<=owner.value and lo<=int(r['range'][1],16)
                   and set(r.get('c_owned_symbols',[])).intersection(spellings|{s.name})]
            if len(owned)!=1:continue
            expected=original.data[section.offset+owner.value-section.addr:section.offset+lo-section.addr]
            if compiled.section_bytes(csec)[s.value:s.value+s.size]!=expected:continue
            if any(r.type==9 and r.info==csec.index and any(s.value<=struct.unpack_from('<I',compiled.data,r.offset+i)[0]<s.value+s.size for i in range(0,r.size,r.entsize)) for r in compiled.sections):continue
            candidates.append(s)
        if len(candidates)!=1 or original_references(original,lo,hi):return match[0]
        proofs.append(dict(scaffold_name=name,range=[lo,hi],size=length,original_owner=owner.name,
                           original_owner_address=owner.value,original_owner_size=owner.size,
                           c_owner=candidates[0].name,cc1_typed_owner=emitted[candidates[0].name],
                           all_padding_bytes_zero=True,no_original_symbol_or_references=True,
                           original_and_compiler_owner_initializer_exact=True))
        return match[3]
    return block.sub(replace,part),proofs


def compiler_string_identity(part, original, compiled, assembly, preprocessed,
                             scope, registry, cwd, scaffold):
    """Restore only anonymous cc1 strings whose original consumers agree.

    Splat can give an anonymous string plus alignment an invented OBJECT.
    Keep its complete storage and remove that identity only after source,
    compiler payload, original consumer addresses and unreferenced padding
    independently establish the string. A current named char array can prove
    the storage, but does not establish an original named OBJECT identity.
    No genuine original named OBJECT qualifies for identity removal.
    """
    import tu_audit
    section_match=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
    if not section_match or section_match[1] not in ('.rodata','.sdata'):return part,[]
    section=original.section(section_match[1])
    csection=next((s for s in compiled.sections if s.name==section.name),None)
    text=next((s for s in compiled.sections if s.name=='.text'),None)
    if not csection or not text:return part,[]
    try:compiler_only_section(assembly,preprocessed,cwd,section.name)
    except (RuntimeError,ValueError):return part,[]
    parsed=tu_audit.AsmFile(assembly);payloads=[]
    emitted=typed_compiler_data(assembly)
    fragments=[]
    for row in parsed.top_data:
        if row['section'] not in (section.name,'.rdata' if section.name=='.rodata' else section.name) or row['directive']!='.ascii':
            fragments=[];continue
        if fragments and row['line']!=fragments[-1][0]['line']+1:fragments=[]
        try:fragment=c_literal_bytes(row['operands'])
        except (ValueError,SyntaxError,AttributeError,UnicodeError):
            fragments=[];continue
        fragments.append((row,fragment))
        payload=b''.join(fragment for _,fragment in fragments)
        if payload.endswith(b'\0'):
            if b'\0' not in payload[:-1]:
                payloads.append((payload,dict(fragments[0][0],consecutive_ascii_rows=[r for r,_ in fragments])))
            fragments=[]
    literals=[]
    for token in source_findall(r'"(?:\\.|[^"\\])*"',preprocessed):
        try:literals.append(c_literal_bytes(token)+b'\0')
        except (ValueError,SyntaxError,UnicodeError):pass
    raw=compiled.section_bytes(csection);proofs=[]
    block=re.compile(r'(?:nonmatching\s+([A-Za-z_][\w.]*)\s*\n\s*)?dlabel\s+([A-Za-z_][\w.]*)\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*\.\s*-\s*\2',re.S)
    def replace(match):
        name=match[2]
        first=re.search(r'/\*\s+\S+\s+([\da-fA-F]{8})\s*\*/\s*\.asciz\s+("(?:\\.|[^"\\])*")',match[3])
        word_payload=None
        if not first:
            # Some anonymous strings are printed as numeric words by Splat.
            # Recover their spelling only from contiguous original-byte rows;
            # the same SOURCE/cc1 payload and original consumer proof follows.
            words=list(re.finditer(r'/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+0x[\da-fA-F]+\s*$',match[3],re.M))
            if not words:return match[0]
            lo=int(words[0][1],16)
            if any(int(row[1],16)!=lo+4*i for i,row in enumerate(words)):return match[0]
            bytes_spelled=b''.join(bytes.fromhex(row[2]) for row in words)
            if b'\0' not in bytes_spelled:return match[0]
            word_payload=bytes_spelled[:bytes_spelled.index(b'\0')+1]
        else:lo=int(first[1],16)
        if re.fullmatch(r'D_[\da-fA-F]+',name) and int(name[2:],16)!=lo:return match[0]
        runs=[r for r in registry.get(section.name,[]) if int(r['range'][0],16)<=lo<int(r['range'][1],16) and name in r.get('c_owned_symbols',[])]
        try:payload=word_payload if word_payload is not None else c_literal_bytes(first[2])+b'\0'
        except (ValueError,SyntaxError,UnicodeError):return match[0]
        entries=[s for s in scaffold.symbols if s.name==name and s.type==1]
        if len(entries)!=1:return match[0]
        hi=lo+entries[0].size;end=lo+len(payload)
        if not section.addr<=lo<end<=hi<=section.addr+section.size or not 0<=hi-end<=15:return match[0]
        if any(s.shndx==section.index and s.name and
               (s.type in (1,2) and s.value<hi and lo<s.value+max(s.size,1) or lo<=s.value<hi) for s in original.symbols):return match[0]
        expected=original.data[section.offset+lo-section.addr:section.offset+hi-section.addr]
        if expected[:len(payload)]!=payload or any(expected[len(payload):]) or payload not in literals:return match[0]
        rows=[r for p,r in payloads if p==payload]
        offsets=[i for i in range(len(raw)-len(payload)+1) if raw[i:i+len(payload)]==payload]
        if len(rows)!=1 or len(offsets)!=1:return match[0]
        offset=offsets[0]
        objects=[s for s in compiled.symbols if s.shndx==csection.index and s.type==1 and s.value<offset+len(payload) and offset<s.value+max(s.size,1)]
        # Named char owners need their existing registered storage and source
        # literal proof below. They do not establish a retail named identity.
        named=None
        if objects:
            if len(runs)!=1:return match[0]
            if len(objects)!=1:return match[0]
            candidate=objects[0];storage=emitted.get(candidate.name)
            if (candidate.name,candidate.value,candidate.size)!=(name,offset,len(payload)) or not storage:return match[0]
            if (storage['section'],storage['size'])!=(section.name,len(payload)):return match[0]
            definition=re.compile(r'(?<![\w])(?:static\s+)?const\s+(?:unsigned\s+)?char\s+'+re.escape(name)+r'\s*\[\s*(?:\d+)?\s*\]\s*=\s*("(?:\\.|[^"\\])*")\s*;')
            definitions=list(source_matches(definition, preprocessed))
            if len(definitions)!=1:return match[0]
            try:initializer=c_literal_bytes(definitions[0][1])+b'\0'
            except (ValueError,SyntaxError,UnicodeError):return match[0]
            if initializer!=payload:return match[0]
            named=dict(c_name=candidate.name,binding=candidate.bind,type=candidate.type,size=candidate.size,
                       source_form='const char array with literal initializer',cc1_typed_storage=storage,
                       original_named_object_identity_not_asserted=True)
        consumers=compiler_pool_consumers(original,compiled,offset,section.name,lo,scope,preprocessed,assembly)
        # A DATA pointer is also an original consumer of an anonymous string.
        # Its actual cc1 relocation and accepted affine owner span must resolve
        # to this exact original address; incidental equal integer words do not.
        for rel in compiled.sections:
            if rel.type!=9 or not 0<rel.info<len(compiled.sections):continue
            owner_section=compiled.sections[rel.info]
            if owner_section.name not in ('.data','.sdata','.rodata'):continue
            try:compiler_only_section(assembly,preprocessed,cwd,owner_section.name)
            except (RuntimeError,ValueError):continue
            owner_bytes=compiled.section_bytes(owner_section)
            for place in range(rel.offset,rel.offset+rel.size,8):
                address,info=struct.unpack_from('<II',compiled.data,place)
                symbol=compiled.symbols[info>>8]
                if info&255!=2 or symbol.shndx!=csection.index or address+4>len(owner_bytes):continue
                if symbol.value+struct.unpack_from('<I',owner_bytes,address)[0]!=offset:continue
                spans=[span for run in registry.get(owner_section.name,[]) for span in run.get('c_input_spans',[])
                    if span.get('section')==owner_section.name and not span.get('uncredited_dependency')
                    and span.get('credit')!='uncredited_dependency' and explicit_object_range(span)
                    and explicit_object_range(span)[0]<=address<address+4<=explicit_object_range(span)[1]
                    and len(span.get('range',[]))==2]
                for span in spans:
                    ext=explicit_object_range(span);coords=[int(v,16) for v in span['range']]
                    if coords[1]-coords[0]!=ext[1]-ext[0]:continue
                    va=coords[0]+address-ext[0];sec=original.section(owner_section.name)
                    if not sec.addr<=va<va+4<=sec.addr+sec.size:continue
                    if struct.unpack_from('<I',original.data,sec.offset+va-sec.addr)[0]!=lo:continue
                    consumers.append(dict(compiler_data_section=owner_section.name,compiler_relocation_offset=address,
                        original_pointer_address=va,original_string_address=lo,registered_owner_span=span,
                        ordinary_C_pointer_emission_verified=True))
        if not consumers or hi>end and original_references(original,end,hi):return match[0]
        temporary=f'.Lobjdiff_string_{lo:08X}'
        proofs.append(dict(scaffold_name=name,temporary_reference_name=temporary,original_range=[lo,hi],
                           compiler_offset=offset,string_size=len(payload),padding_size=hi-end,
                           cc1_ascii=rows[0],source_literal_present=True,consumers=consumers,
                           current_named_char_owner=named,
                           payload_sha256=hashlib.sha256(payload).hexdigest(),no_original_symbol=True,
                           no_original_padding_references=True,all_original_storage_bytes_preserved=True))
        return '.local '+name+'\n'+name+':\n'+match[3]
    return block.sub(replace,part),proofs


def compiler_switch_identity(part, original, compiled, assembly, preprocessed,
                             scope, registry, tu, cwd):
    """Restore anonymous switch-table identity, with source and target proof."""
    if 'dlabel jtbl_' not in part:return part, []
    try:emission=compiler_only_section(assembly,preprocessed,cwd,'.rodata')
    except (RuntimeError,ValueError):return part, []
    regions={r['function']:r for r in emission.get('compiler_region_switch_tables',[])}
    rodata=next((s for s in compiled.sections if s.name=='.rodata'),None)
    text=next((s for s in compiled.sections if s.name=='.text'),None)
    if not rodata or not text:return part, []
    cfunctions={s.name:s for s in compiled.symbols if s.type==2 and s.shndx==text.index}
    claimed={f['original_symbol']:f for f in scope}
    relocations={}
    for rel in compiled.sections:
        if rel.type==9 and rel.info==rodata.index:
            for i in range(0,rel.size,rel.entsize):
                off,info=struct.unpack_from('<II',compiled.data,rel.offset+i)
                symbol=next(s for s in compiled.symbols if s.index==info>>8)
                relocations[off]=(info&255,symbol)
    raw=compiled.section_bytes(rodata);proofs=[]
    block=re.compile(r'(?:nonmatching\s+(jtbl_\w+)\s*\n\s*)?dlabel\s+(jtbl_\w+)\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*\.\s*-\s*\2',re.S)
    rows_re=re.compile(r'/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+\.L[\da-fA-F]+\s*$',re.M)
    def replace(match):
        rows=list(rows_re.finditer(match[3]));name=match[2]
        if not rows:return match[0]
        lo=int(rows[0][1],16);hi=lo+len(rows)*4
        section=original.section('.rodata')
        if not section.addr<=lo<hi<=section.addr+section.size:return match[0]
        expected=original.data[section.offset+lo-section.addr:section.offset+hi-section.addr]
        if any(int(r[1],16)!=lo+4*i or bytes.fromhex(r[2])!=expected[i*4:i*4+4] for i,r in enumerate(rows)):return match[0]
        if any(s.type in(1,2) and s.shndx==section.index and s.value<hi and lo<s.value+max(s.size,1) for s in original.symbols):return match[0]
        runs=[r for r in registry.get('.rodata',[]) if int(r['range'][0],16)<=lo and hi<=int(r['range'][1],16)
              and (name in r.get('c_owned_symbols',[]) or any(
                  (span.get('support_only') is True or span.get('anonymous_emission') is True and span.get('credit')=='verified')
                  and not span.get('symbols') and not span.get('uncredited_dependency')
                  and span.get('section')=='.rodata' and explicit_object_range(span)
                  and len(span.get('range',[]))==2 and int(span['range'][0],16)<=lo<hi<=int(span['range'][1],16)
                  and explicit_object_range(span)[1]-explicit_object_range(span)[0]==int(span['range'][1],16)-int(span['range'][0],16)
                  for span in r.get('c_input_spans',[])))]
        if len(runs)!=1:return match[0]
        functions=set(runs[0].get('generated_by') or regions).intersection(regions).intersection(claimed).intersection(cfunctions)
        candidates=[]
        for function in functions:
            function_bytes_exact=False
            f=cfunctions[function];claim=claimed[function]
            if regions[function]['word_count']<len(rows):continue
            original_function=verification.original_symbol(original,function,claim['size'],claim['va'])
            if (f.bind,f.type,f.size)!=(original_function.bind,original_function.type,original_function.size):continue
            code=compiled.section_bytes(text)[f.value:f.value+f.size]
            original_text=original.sections[original_function.shndx]
            original_code=original.data[original_text.offset+original_function.value-original_text.addr:
                                        original_text.offset+original_function.value-original_text.addr+f.size]
            masks={}
            for rel in compiled.sections:
                if rel.type!=9 or rel.info!=text.index:continue
                for i in range(0,rel.size,rel.entsize):
                    place,info=struct.unpack_from('<II',compiled.data,rel.offset+i)
                    if f.value<=place<f.value+f.size:
                        kind=info&255
                        if kind not in (4,5,6,7,8):break
                        masks[place-f.value]=0xfc000000 if kind==4 else 0xffff0000
                else:continue
                break
            else:
                if all((struct.unpack_from('<I',code,i)[0]&masks.get(i,0xffffffff))==
                       (struct.unpack_from('<I',original_code,i)[0]&masks.get(i,0xffffffff))
                       for i in range(0,len(code),4)):
                    function_bytes_exact=True
                else:continue
            if not function_bytes_exact:continue
            for off in sorted(relocations):
                if off+len(expected)>len(raw):continue
                if name not in runs[0].get('c_owned_symbols',[]):
                    transports=[span for span in runs[0].get('c_input_spans',[]) if explicit_object_range(span)
                        and not span.get('symbols') and not span.get('uncredited_dependency')
                        and (span.get('support_only') is True or span.get('anonymous_emission') is True and span.get('credit')=='verified')
                        and span.get('section')=='.rodata' and len(span.get('range',[]))==2
                        and explicit_object_range(span)[1]-explicit_object_range(span)[0]==int(span['range'][1],16)-int(span['range'][0],16)
                        and explicit_object_range(span)[0]<=off<off+len(expected)<=explicit_object_range(span)[1]
                        and int(span['range'][0],16)+off-explicit_object_range(span)[0]==lo]
                    if len(transports)!=1:continue
                if any(s.type==1 and s.shndx==rodata.index and s.value<off+len(expected) and off<s.value+max(s.size,1) for s in compiled.symbols):continue
                targets=[]
                for index in range(len(rows)):
                    place=off+index*4;rel=relocations.get(place)
                    if not rel or rel[0]!=2 or rel[1].shndx!=text.index:break
                    value=struct.unpack_from('<I',raw,place)[0]+rel[1].value
                    if not f.value<=value<f.value+f.size:break
                    targets.append(claim['va']+value-f.value)
                else:
                    if b''.join(v.to_bytes(4,'little') for v in targets)==expected:
                        candidates.append((function,off,targets,hashlib.sha256(original_code).hexdigest(),hashlib.sha256(code).hexdigest()))
        if len(candidates)!=1:return match[0]
        function,off,targets,original_code_sha,compiler_code_sha=candidates[0]
        proofs.append(dict(scaffold_name=name,original_range=[lo,hi],compiler_offset=off,
                           generated_by=function,word_count=len(rows),original_targets=targets,
                           compiler_region=regions[function],registered_run=runs[0]['range'],
                           temporary_reference_name=f'.Lobjdiff_switch_{lo:08X}',
                           original_function_sha256=original_code_sha,
                           compiler_function_sha256=compiler_code_sha,
                           owner_nonrelocated_instructions_exact=True,
                           no_original_object=True,all_real_R_MIPS32_targets_exact=True))
        return '.local '+name+'\n'+name+':\n'+match[3]
    return block.sub(replace,part),proofs


def compiler_nobits_alias_identity(part, original, compiled, assembly,
                                   preprocessed, registry, tu, tu_id):
    """Discard proven Splat interior aliases, never original symbols.

    Each owner needs a full registered compiler span, actual C array and
    cc1 label-space allocation, and independent original owner boundaries.
    Historical alias metadata is checked when available; missing metadata
    is not invented. Original bytes, real names and allocation gaps remain.
    """
    if not re.search(r'^\.section\s+\.bss\s*$',part,re.M):return part,[]
    section=original.section('.bss');csection=next((s for s in compiled.sections if s.name=='.bss'),None)
    if not csection:return part,[]
    if section.type!=8 or csection.type!=8:return part,[]
    definitions=emitted_storage(assembly);proofs=[]
    pattern=re.compile(r'nonmatching\s+(D_[\da-fA-F]+)\s*,\s*(0x[\da-fA-F]+|\d+)\s*\n\s*dlabel\s+\1\s*\n(\s*/\*\s+([\da-fA-F]{8})\s*\*/\s*\.space\s+(0x[\da-fA-F]+|\d+)\s*)')
    segments=list(pattern.finditer(part))
    for run_entry in registry.get('.bss',[]):
        lo,hi=[int(v,16) for v in run_entry['range']];size=hi-lo
        if not int(tu['sections']['.bss']['start'],16)<=lo<hi<=int(tu['sections']['.bss']['end'],16):continue
        for owner_name in run_entry.get('c_owned_symbols',[]):
            originals=[s for s in original.symbols if s.shndx==section.index and s.name==owner_name and s.value==lo]
            if len(originals)!=1 or (originals[0].bind,originals[0].type,originals[0].size)!=(0,0,0):continue
            if any(s.name and s.shndx==section.index and lo<s.value<hi for s in original.symbols):continue
            next_names=[s.name for s in original.symbols if s.name and s.shndx==section.index and s.value==hi]
            trailing_gap=None
            if not next_names:
                # This independently reviewed stack has exactly 32 bytes of
                # original alignment before the next real owner. Keep that
                # gap entirely outside the compiler-owned stack allocation.
                next_symbols=[s for s in original.symbols if s.name and s.shndx==section.index and s.value>=hi]
                if (tu_id!='main/tu110' or owner_name!='MYwaveTransThStack' or size!=16384 or
                    not next_symbols or min(s.value for s in next_symbols)!=hi+32 or
                    hi+32!=int(tu['sections']['.bss']['end'],16)):continue
                next_names=[s.name for s in next_symbols if s.value==hi+32];trailing_gap=[hi,hi+32]
            current=[s for s in compiled.symbols if s.shndx==csection.index and s.name==owner_name]
            if len(current)!=1 or (current[0].bind,current[0].type,current[0].size)!=(0,0,0):continue
            storage=definitions.get(owner_name)
            if not storage or (storage['section'],storage['size'],storage['extent_kind'])!=('.bss',size,'label-space'):continue
            declarations=source_findall(r'\bstatic\s+((?:(?:unsigned|signed)\s+)?[A-Za-z_]\w*)\s+'+re.escape(owner_name)+r'\s*\[\s*(0[xX][\da-fA-F]+|\d+)\s*\]\s*;',preprocessed)
            source=list({(' '.join(ctype.split()),extent) for ctype,extent in declarations})
            if len(source)!=1:continue
            count=int(source[0][1],16 if 'x' in source[0][1].lower() else 10)
            if count<=0 or size%count:continue
            spans=[s for s in run_entry.get('c_input_spans',[]) if s['section']=='.bss' and
                   [int(v,16) for v in s['range']]==[lo,hi] and
                   explicit_object_range(s)==[current[0].value,current[0].value+size] and owner_name in s['symbols']]
            if len(spans)!=1 or current[0].value+size>csection.size:continue
            aliases={a['original_name']:a for a in run_entry.get('c_storage_aliases',[])}
            for match in segments:
                name=match[1];address=int(name[2:],16);length=int(match[2],0)
                limit=trailing_gap[1] if trailing_gap else hi
                if not lo<address<hi or not address<address+length<=limit or int(match[4],16)!=address or int(match[5],0)!=length:continue
                registered_alias=aliases.get(name)
                if registered_alias:
                    other=registered_alias.get('storage_owner',{})
                    if (int(registered_alias['address'],16)!=address or other.get('name')!=owner_name or
                        int(other.get('address','0'),16)!=lo or other.get('size')!=size):continue
                if len(list(pattern.finditer(match[0])))!=1 or match[0] not in part:continue
                temporary=f'.Lobjdiff_bss_{address:08X}'
                part=part.replace(match[0],temporary+':\n'+match[3],1)
                proofs.append(dict(scaffold_name=name,temporary_reference_name=temporary,
                    original_alias_address=address,alias_extent=length,original_owner=owner_name,
                    original_owner_identity=dict(binding=0,type=0,size=0,address=lo),
                    original_allocation=[lo,hi],next_original_symbol_names=next_names,
                    retained_uncredited_alignment_gap=trailing_gap,
                    registered_c_input_span=spans[0],registered_alias=registered_alias,
                    cc1_storage=storage,source_array_type=source[0][0],source_array_count=count,
                    compatible_tentative_source_declarations=len(declarations),
                    no_original_interior_symbols=True,original_owner_retained=True,all_nobits_extents_preserved=True))
    return part,proofs


def registered_nobits_foreign_aliases(body, reference, original, compiled,
                                      assembly, preprocessed, registry, tu,
                                      cwd, destination):
    """Remove only invented GLOBAL identities inside fully proven C storage.

    Temporary local labels retain every section-relative consumer. Original
    identities, source objects, owner extents and allocation gaps are retained;
    the caller additionally requires identical allocated bytes and relocations.
    """
    definitions=emitted_storage(assembly);proofs=[];replacements={}
    used=set()
    for rel in reference.sections:
        if rel.type!=9:continue
        for offset in range(rel.offset,rel.offset+rel.size,8):
            _,info=struct.unpack_from('<II',reference.data,offset);used.add(info>>8)
    pattern=re.compile(r'^\s*dlabel\s+(D_[0-9A-Fa-f]{8})\s*\n(\s*/\*\s+([0-9A-Fa-f]{8})\s*\*/\s*\.space\s+(0x[0-9A-Fa-f]+|\d+))',re.M)
    for match in pattern.finditer(body):
        name=match[1];address=int(match[3],16);length=int(match[4],0)
        if address!=int(name[2:],16) or length<=0:continue
        sections=list(re.finditer(r'^\s*\.section\s+(\.[\w.]+)',body[:match.start()],re.M))
        if not sections:continue
        section_name=sections[-1][1]
        if section_name not in ('.bss','.sbss'):continue
        section=original.section(section_name);extent=tu.get('sections',{}).get(section_name)
        csection=next((s for s in compiled.sections if s.name==section_name),None)
        rsection=next((s for s in reference.sections if s.name==section_name),None)
        if not extent or not csection or not rsection or any(s.type!=8 for s in (section,csection,rsection)):continue
        start,end=(int(extent[k],16) for k in ('start','end'))
        if not start<=address<address+length<=end:continue
        current=[s for s in reference.symbols if s.name==name and s.shndx==rsection.index]
        if len(current)!=1 or (current[0].bind,current[0].type,current[0].size)!=(1,1,0):continue
        if current[0].index in used or current[0].value!=address-start:continue
        if any(s.name==name or (s.name and s.shndx==section.index and s.value==address) for s in original.symbols):continue
        owners=[]
        for run_entry in registry.get(section_name,[]):
            for span in run_entry.get('c_input_spans',[]):
                coordinates=explicit_object_range(span)
                if coordinates is None or span.get('section')!=section_name:continue
                lo,hi=(int(v,16) for v in span['range'])
                if not start<=lo<=address<address+length<=hi<=end or hi-lo!=coordinates[1]-coordinates[0]:continue
                for owner in span.get('symbols',[]):
                    if owner not in run_entry.get('c_owned_symbols',[]):continue
                    real=[s for s in original.symbols if s.name==owner and s.shndx==section.index and s.value==lo]
                    actual=[s for s in compiled.symbols if s.name==owner and s.shndx==csection.index]
                    if len(real)!=1 or len(actual)!=1:continue
                    if (real[0].bind,real[0].type,real[0].size)!=(0,0,0) or (actual[0].bind,actual[0].type,actual[0].size)!=(0,0,0):continue
                    if any(s.name and s.shndx==section.index and lo<s.value<hi for s in original.symbols):continue
                    storage=definitions.get(owner)
                    if not storage or (storage['section'],storage['size'],storage['extent_kind'])!=(section_name,hi-lo,'label-space'):continue
                    if coordinates!=[actual[0].value,actual[0].value+storage['size']] or coordinates[1]>csection.size:continue
                    declarations=set(source_findall(r'\bstatic\s+((?:(?:unsigned|signed)\s+)?[A-Za-z_]\w*)\s+'+re.escape(owner)+r'\s*(\[[^;]+\])?\s*;',preprocessed))
                    if len(declarations)!=1:continue
                    owners.append(dict(owner=owner,original_range=[lo,hi],registered_c_input_span=span,
                        source_declaration=next(iter(declarations)),cc1_outside_APP_storage=storage,
                        original_owner_identity=dict(name=owner,address=lo,binding=0,type=0,size=0)))
        unique={json.dumps(o,sort_keys=True):o for o in owners}
        if len(unique)!=1:continue
        temporary=f'.Lobjdiff_owned_nobits_{address:08X}'
        replacements[name]=temporary
        proofs.append(dict(scaffold_name=name,temporary_reference_name=temporary,
            original_alias_address=address,whole_alias_allocation=[address,address+length],
            owner_proof=next(iter(unique.values())),no_original_alias_identity=True,
            no_nominal_reference_relocation_consumer=True,all_real_original_identities_retained=True))
    if not proofs:return body,[]
    for name,temporary in replacements.items():
        body=re.sub(r'^\s*dlabel\s+'+re.escape(name)+r'\s*$',lambda _:'.local '+temporary+'\n'+temporary+':',body,flags=re.M)
        body=re.sub(r'^\s*enddlabel\s+'+re.escape(name)+r'\s*$','',body,flags=re.M)
        body=re.sub(r'^\s*\.size\s+'+re.escape(name)+r'\s*,[^\n]*$','',body,flags=re.M)
    def rename(text):
        for name,temporary in replacements.items():
            text=re.sub(r'(?<![\w.$])'+re.escape(name)+r'(?![\w.$])',temporary,text)
        return text
    def include(match):
        source=Path(match[1]);source=source if source.is_absolute() else cwd/source
        old=source.read_text();new=rename(old)
        if old==new:return match[0]
        path=destination.parent/'joint-foreign-asm'/source.name
        path.parent.mkdir(exist_ok=True);path.write_text(new)
        return '.include '+json.dumps(str(path))
    return rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,body)),proofs


def registered_anonymous_char_identities(body, reference, original, compiled,
                                        assembly, preprocessed, registry, tu, cwd, destination,
                                        scope=()):
    """Use the shared source/consumer proof for anonymous identities only.

    Complete named C owners remain available for the separate exact identity
    transport. No legacy byte-equality-only anonymous route can bypass the
    strict instruction/DATA-pointer consumer and source initializer checks.
    """
    return registered_source_pool_identities(body,reference,original,compiled,
        assembly,preprocessed,registry,tu,cwd,scope)


def original_reference_signature(elf):
    """Allocated storage and real relocation identity/addend words, unchanged."""
    allocated={s.name:(s.type,s.flags,s.align,s.size,elf.section_bytes(s)) for s in elf.sections if s.flags&2}
    relocations=[]
    for rel in elf.sections:
        if rel.type!=9:continue
        for offset in range(rel.offset,rel.offset+rel.size,8):
            address,info=struct.unpack_from('<II',elf.data,offset);symbol=elf.symbols[info>>8]
            section=elf.sections[rel.info]
            symbol_section=elf.sections[symbol.shndx].name if 0<symbol.shndx<len(elf.sections) else symbol.shndx
            relocations.append((section.name,address,info&255,symbol.name,symbol.value,symbol.bind,symbol.type,
                symbol.size,symbol_section,elf.section_bytes(section)[address:address+4]))
    return allocated,sorted(relocations,key=str)


def compiler_original_static_name(part, original, compiled, assembly,
                                  preprocessed, scope, registry, tu):
    """Restore a genuine original dot-name now emitted by scoped C storage.

    This only transports a real LOCAL identity shared by retail and cc1. Its
    SOURCE declaration, claimed function consumer and registered input span
    must agree. No allocation, original size, initializer or relocation changes
    are made; the ordinary joint original linker still checks all bytes.
    """
    section_match=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
    if not section_match:return part,[]
    section_name=section_match[1]
    if section_name not in ('.data','.sdata','.rodata','.bss','.sbss'):return part,[]
    original_section=original.section(section_name)
    current=next((s for s in compiled.sections if s.name==section_name),None)
    if not current or original_section.type!=current.type:return part,[]
    storage=emitted_storage(assembly);typed=typed_compiler_data(assembly)
    bindings=function_static_source_bindings(preprocessed,compiled,assembly)
    claimed={f['original_symbol']:f for f in scope};proofs=[]
    def replace(match):
        name=match[1]
        address=re.search(r'/\*\s+(?:\S+\s+)?([0-9A-Fa-f]{8})(?:\s|\s*\*/)',part[match.end():])
        if not address:return match[0]
        va=int(address[1],16)
        originals=[s for s in original.symbols if s.shndx==original_section.index and s.value==va
                   and s.bind==0 and s.type in (0,1) and re.fullmatch(r'[A-Za-z_]\w*\.[0-9]+',s.name)
                   and name in (s.name.replace('.','_'),s.name.replace('.','_')+f'_{va:08X}')]
        if len(originals)!=1:return match[0]
        old=originals[0];actual_name=old.name;binding=bindings.get(actual_name)
        if not binding or binding['source_function'] not in claimed:return match[0]
        if any(s.name in (name,name+'.NON_MATCHING') for s in original.symbols):return match[0]
        actual=[s for s in compiled.symbols if s.name==actual_name and s.shndx==current.index
                and (s.bind,s.type,s.size)==(old.bind,old.type,old.size)]
        if len(actual)!=1:return match[0]
        symbol=actual[0];allocation=storage.get(actual_name)
        if old.type==1:
            emission=typed.get(actual_name)
            if not emission or (emission['section'],emission['size'])!=(section_name,old.size) or old.size<=0:return match[0]
            size=old.size
        else:
            if not allocation or allocation['section']!=section_name or allocation['extent_kind']!='label-space' or allocation['size']<=0:return match[0]
            size=allocation['size']
            # A size-less original NOBITS symbol's allocation is bounded by the
            # next genuine original owner or the original TU end, never zeros.
            if current.type!=8:return match[0]
            next_addresses=[s.value for s in original.symbols if s.name and s.shndx==original_section.index and s.value>va]
            next_boundary=min(next_addresses+[int(tu['sections'][section_name]['end'],16)])
            if va+size!=next_boundary:return match[0]
        if not int(tu['sections'][section_name]['start'],16)<=va<va+size<=int(tu['sections'][section_name]['end'],16):return match[0]
        if symbol.value+size>current.size:return match[0]
        spans=[span for run in registry.get(section_name,[]) for span in run.get('c_input_spans',[])
               if (object_range:=explicit_object_range(span)) is not None
               and actual_name in run.get('c_owned_symbols',[]) and span['section']==section_name and actual_name in span['symbols']
               and int(span['range'][0],16)<=va and va+size<=int(span['range'][1],16)
               and object_range[0]<=symbol.value and symbol.value+size<=object_range[1]
               and va-int(span['range'][0],16)==symbol.value-object_range[0]]
        if len(spans)!=1:return match[0]
        declaration=claimed[binding['source_function']]
        of=verification.original_symbol(original,binding['source_function'],declaration['size'],declaration['va'])
        cf=[s for s in compiled.symbols if s.name==binding['source_function'] and s.type==2 and s.shndx not in (0,0xfff1)]
        if len(cf)!=1 or (cf[0].bind,cf[0].type,cf[0].size)!=(of.bind,of.type,of.size):return match[0]
        proofs.append(dict(scaffold_name=name,temporary_reference_name=actual_name,
            original_name=actual_name,original_address=va,binding=old.bind,type=old.type,size=old.size,
            source_scope=binding,cc1_storage=allocation,cc1_typed_object=typed.get(actual_name),
            registered_c_input_span=spans[0],compiler_offset=symbol.value,
            original_allocation=[va,va+size],original_local_identity_retained=True,
            storage_and_real_relocations_unchanged=True))
        return 'dlabel '+actual_name
    part=re.sub(r'^dlabel\s+(\S+)\s*$',replace,part,flags=re.M)
    for proof in proofs:
        part=re.sub(r'(?<![\w.$])'+re.escape(proof['scaffold_name'])+r'(?![\w.$])',
                    lambda _:proof['temporary_reference_name'],part)
    return part,proofs


def compiler_function_static_identity(part, original, compiled, assembly,
                                      preprocessed, scope, registry, tu):
    """Restore real local static names lost by Splat's numeric suffix stripping.

    This route requires the same original and cc1 identity, a typed declaration
    inside its claimed C function, and an exact registered storage allocation.
    It cannot manufacture an original symbol or infer ownership from zeros.
    """
    if not re.search(r'^\.section\s+\.bss\s*$',part,re.M):return part,[]
    import tu_edit
    original_section=original.section('.bss')
    current_section=next((s for s in compiled.sections if s.name=='.bss'),None)
    if not current_section or original_section.type!=8 or current_section.type!=8:return part,[]
    try:functions=[item for item in tu_edit.scan(preprocessed) if item.kind=='func']
    except tu_edit.ParseError:return part,[]
    claimed={f['original_symbol']:f for f in scope};storage=emitted_storage(assembly)
    outside=[];app=False
    for line in assembly.splitlines():
        if line.strip()=='#APP':app=True
        elif line.strip()=='#NO_APP':app=False
        elif not app:outside.append(line)
    ordinary='\n'.join(outside);proofs=[]
    blocks=re.compile(r'nonmatching\s+(\w+)\s*,\s*(0x[\da-fA-F]+|\d+)\s*\n\s*dlabel\s+\1\s*\n(\s*/\*\s+([\da-fA-F]{8})\s*\*/\s*\.space\s+(0x[\da-fA-F]+|\d+)\s*)')
    def replace(match):
        name=match[1];address=int(match[4],16);size=int(match[2],0)
        if size<=0 or int(match[5],0)!=size:return match[0]
        originals=[s for s in original.symbols if s.shndx==original_section.index and s.value==address
                   and re.fullmatch(re.escape(name)+r'\.[0-9]+',s.name) and (s.bind,s.type,s.size)==(0,0,0)]
        if len(originals)!=1:return match[0]
        original_symbol=originals[0];actual_name=original_symbol.name
        if any(s.name in (name,name+'.NON_MATCHING') for s in original.symbols):return match[0]
        if any(s.name and s.shndx==original_section.index and address<s.value<address+size for s in original.symbols):return match[0]
        if not any(s.name and s.shndx==original_section.index and s.value==address+size for s in original.symbols):return match[0]
        actual=[s for s in compiled.symbols if s.shndx==current_section.index and s.name==actual_name
                and (s.bind,s.type,s.size)==(0,0,0)]
        allocation=storage.get(actual_name)
        if len(actual)!=1 or not allocation or (allocation['section'],allocation['size'],allocation['extent_kind'])!=('.bss',size,'label-space'):return match[0]
        symbol=actual[0]
        if symbol.value+size>current_section.size:return match[0]
        if not int(tu['sections']['.bss']['start'],16)<=address<address+size<=int(tu['sections']['.bss']['end'],16):return match[0]
        spans=[span for run in registry.get('.bss',[]) for span in run.get('c_input_spans',[])
               if (object_range:=explicit_object_range(span)) is not None
               and actual_name in run.get('c_owned_symbols',[]) and span['section']=='.bss' and actual_name in span['symbols']
               and int(span['range'][0],16)<=address and address+size<=int(span['range'][1],16)
               and object_range[0]<=symbol.value and symbol.value+size<=object_range[1]
               and address-int(span['range'][0],16)==symbol.value-object_range[0]]
        if len(spans)!=1:return match[0]
        declarations=[]
        for function in functions:
            if function.name not in claimed:continue
            rows=source_findall(r'\bstatic\s+([A-Za-z_]\w*)\s+'+re.escape(name)+r'\s*;',function.text(preprocessed))
            for ctype in rows:
                if not source_search(r'\btypedef\s+struct\s*\{[^{}]*\}\s*'+re.escape(ctype)+r'\s*;',preprocessed,re.S):continue
                region=re.search(r'^\s*\.ent\s+'+re.escape(function.name)+r'\s*$(.*?)^\s*\.end\s+'+re.escape(function.name)+r'\s*$',ordinary,re.M|re.S)
                if not region or not re.search(r'%hi\('+re.escape(actual_name)+r'\)',region[1]) or not re.search(r'%lo\('+re.escape(actual_name)+r'\)',region[1]):continue
                declaration=claimed[function.name]
                of=verification.original_symbol(original,function.name,declaration['size'],declaration['va'])
                cf=[s for s in compiled.symbols if s.name==function.name and s.type==2 and s.shndx not in (0,0xfff1)]
                if len(cf)!=1 or (cf[0].bind,cf[0].type,cf[0].size)!=(of.bind,of.type,of.size):continue
                declarations.append((function.name,ctype))
        if len(declarations)!=1:return match[0]
        function,ctype=declarations[0]
        proofs.append(dict(scaffold_name=name,temporary_reference_name=actual_name,
            original_name=actual_name,original_address=address,binding=0,type=0,size=0,
            source_function=function,source_declared_type=ctype,cc1_storage=allocation,
            registered_c_input_span=spans[0],compiler_offset=symbol.value,
            original_allocation=[address,address+size],original_next_owner_boundary=True,
            no_original_stripped_alias_or_marker=True,all_nobits_extents_preserved=True))
        return 'dlabel '+actual_name+'\n'+match[3]
    return blocks.sub(replace,part),proofs


def splat_data_marker_identity(part, original, scaffold, reference, macro_path):
    """Remove Splat-only DATA marker aliases, preserving their parent storage.

    The pinned macro must emit only a symbol alias. Actual assembled parent and
    marker identities, and absence of any relocation consumer, are mandatory.
    This does not claim the parent, invent its original type, or remove padding.
    """
    if not macro_path.is_file():return part,[]
    macro=macro_path.read_text()
    definition=re.search(r'^\s*\.macro\s+nonmatching\s+label,\s*size=1\s*\n(.*?)^\s*\.endm\s*$',macro,re.M|re.S)
    expected=['.global "\\label\\().NON_MATCHING"',
              '.type "\\label\\().NON_MATCHING", @object',
              '.size "\\label\\().NON_MATCHING", \\size',
              '"\\label\\().NON_MATCHING":']
    if not definition or [line.strip() for line in definition[1].splitlines() if line.strip()]!=expected:return part,[]
    proofs=[]
    marker_re=re.compile(r'^\s*nonmatching\s+([A-Za-z_.$][\w.$]*)(?:\s*,\s*(0x[\da-fA-F]+|\d+))?\s*$',re.M)
    def replace(match):
        parent=match[1];name=parent+'.NON_MATCHING';size=int(match[2],0) if match[2] else 1
        if any(s.name==name for s in original.symbols):return match[0]
        # Between marker and label only whitespace/comments are allowed, so
        # the macro and dlabel necessarily denote the same allocation point.
        following=part[match.end():]
        adjacent=re.match(r'(?:\s|/\*.*?\*/)*dlabel\s+'+re.escape(parent)+r'\s*(?:\n|$)',following,re.S)
        if not adjacent:return match[0]
        markers=[s for s in scaffold.symbols if s.name==name and 0<s.shndx<len(scaffold.sections)]
        parents=[s for s in scaffold.symbols if s.name==parent and 0<s.shndx<len(scaffold.sections)]
        if len(markers)!=1 or len(parents)!=1:return match[0]
        marker,label=markers[0],parents[0];section=scaffold.sections[marker.shndx]
        if not section.flags&2 or section.flags&4 or section.type not in (1,8):return match[0]
        if (marker.bind,marker.type,marker.size)!=(1,1,size):return match[0]
        if (marker.shndx,marker.value)!=(label.shndx,label.value):return match[0]
        if not any(s.name==section.name and s.flags&2 and not s.flags&4 for s in original.sections):return match[0]
        for elf in (original,scaffold,reference):
            indices={s.index for s in elf.symbols if s.name==name}
            for relocation in elf.sections:
                if relocation.type not in (4,9):continue
                stride=relocation.entsize or (12 if relocation.type==4 else 8)
                if any(struct.unpack_from('<I',elf.data,relocation.offset+i+4)[0]>>8 in indices
                       for i in range(0,relocation.size,stride)):return match[0]
        # No textual operand can silently turn the removed marker into an
        # undefined relocation when this source is assembled jointly.
        if re.search(r'(?<![\w.$])'+re.escape(name)+r'(?![\w.$])',part):return match[0]
        proofs.append(dict(marker_name=name,parent_name=parent,section=section.name,
            section_offset=marker.value,marker_binding=1,marker_type=1,marker_size=size,
            parent_binding=label.bind,parent_type=label.type,parent_size=label.size,
            macro_sha256=hashlib.sha256(macro_path.read_bytes()).hexdigest(),
            macro_emits_only_symbol_alias=True,no_original_marker=True,
            no_relocation_consumers=True,parent_and_all_storage_preserved=True))
        return ''
    return marker_re.sub(replace,part),proofs


def original_synthetic_call_destination(symbol, reference, original, tu, body, cwd, scope):
    """Resolve a scaffold-only call name from every original J/JAL instruction.

    This proves an encoded destination, not a callee identity. DATA pointers,
    other relocation kinds, addends and unsupported consumers stay unresolved.
    """
    synthetic=re.fullmatch(r'func_([\da-fA-F]+)',symbol.name)
    if not synthetic:return None
    destination=int(synthetic[1],16);text=reference.section('.text');start=int(tu['text']['start'],16)
    original_text=original.section('.text');uses=[]
    sources=[('joint-input',body)]
    for path in re.findall(r'\.include\s+"([^\"]+\.s)"',body):
        source=Path(path);source=source if source.is_absolute() else cwd/source
        if not source.is_file():return None
        sources.append((str(source),source.read_text()))
    for relocation in reference.sections:
        if relocation.type not in (4,9):continue
        stride=relocation.entsize or (12 if relocation.type==4 else 8)
        for index in range(0,relocation.size,stride):
            offset,info=struct.unpack_from('<II',reference.data,relocation.offset+index)
            if info>>8!=symbol.index:continue
            if relocation.type!=9 or relocation.info!=text.index or info&255!=4:return None
            address=start+offset
            if not original_text.addr<=address<=original_text.addr+original_text.size-4:return None
            word=struct.unpack_from('<I',original.data,original_text.offset+address-original_text.addr)[0]
            raw=struct.unpack_from('<I',reference.data,text.offset+offset)[0]
            if word>>26 not in (2,3) or raw>>26!=word>>26 or raw&0x3ffffff:return None
            if ((address+4)&0xf0000000)|((word&0x3ffffff)<<2)!=destination:return None
            owners=[f for f in scope if f['va']<=address<f['va']+f['size']]
            if len(owners)!=1:return None
            owner=owners[0]
            verification.original_symbol(original,owner['original_symbol'],owner['size'],owner['va'])
            pattern=re.compile(r'^\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*(j|jal)\s+'+re.escape(symbol.name)+r'\s*$',re.M)
            witnesses=[(path,match) for path,source in sources for match in pattern.finditer(source)
                       if int(match[1],16)==address and bytes.fromhex(match[2])==word.to_bytes(4,'little')
                       and match[3]==('jal' if word>>26==3 else 'j')]
            if len(witnesses)!=1:return None
            path,match=witnesses[0]
            uses.append(dict(original_site=address,original_word=f'{word:08X}',
                original_caller=owner['original_symbol'],scaffold_source=path,
                scaffold_source_sha256=hashlib.sha256(dict(sources)[path].encode()).hexdigest(),
                relocation='R_MIPS26',addend=0))
    if not uses:return None
    return dict(scaffold_name=symbol.name,encoded_destination=destination,uses=uses,
                all_original_call_destinations_exact=True,no_callee_identity_inferred=True)


def unused_scaffold_external(symbol, reference, original, body, cwd):
    """Keep a declared unused extern without inventing a linked address.

    Splat .extern directives survive reconstruction of numeric/literal loads.
    They allocate no storage and need no linker definition when no REL/RELA
    entry consumes them. Actual original names and textual uses refuse.
    """
    if (symbol.shndx,symbol.bind,symbol.type,symbol.other)!=(0,1,0,0) or symbol.size<=0:return None
    if any(s.name==symbol.name for s in original.symbols):return None
    for relocation in reference.sections:
        if relocation.type not in (4,9):continue
        stride=12 if relocation.type==4 else 8
        if relocation.entsize not in (0,stride) or relocation.size%stride:return None
        if any(struct.unpack_from('<I',reference.data,relocation.offset+i+4)[0]>>8==symbol.index
               for i in range(0,relocation.size,stride)):return None
    sources=[('joint-input',body)]
    for filename in re.findall(r'\.include\s+"([^\"]+\.s)"',body):
        path=Path(filename);path=path if path.is_absolute() else cwd/path
        if not path.is_file():return None
        sources.append((str(path),path.read_text()))
    declaration=re.compile(r'^\s*\.extern\s+'+re.escape(symbol.name)+r'\s*,\s*(0x[\da-fA-F]+|\d+)\s*$',re.M)
    use=re.compile(r'(?<![\w.$])'+re.escape(symbol.name)+r'(?![\w.$])');witnesses=[]
    for path,source in sources:
        declarations=list(declaration.finditer(source))
        if any(int(m[1],0)!=symbol.size for m in declarations):return None
        if use.search(declaration.sub('',source)):return None
        witnesses.extend(dict(source=path,source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                              declared_size=symbol.size) for m in declarations)
    if not witnesses:return None
    return dict(name=symbol.name,size=symbol.size,explicit_scaffold_extern_declarations=witnesses,
                no_original_named_identity=True,no_REL_or_RELA_consumers=True,
                undefined_symbol_preserved=True,no_address_inferred=True)


def compiler_character_pointer_references(part, reference, original, compiled,
        assembly, preprocessed, registry, tu, scope, cwd):
    """Restore omitted char-pointer expressions from actual C relocations.

    Admit only complete source char-pointer arrays (or a record containing
    exactly one such array). Original matching code or an already accepted
    affine input span must locate the emitted array; every character destination
    must equal the original bytes. Numeric DATA without this proof stays literal.
    """
    section_match=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
    if not section_match or section_match[1] not in ('.data','.rodata','.sdata'):return part,[]
    section_name=section_match[1]
    csec=next((s for s in compiled.sections if s.name==section_name),None)
    text=next((s for s in compiled.sections if s.name=='.text'),None)
    if not csec or not text:return part,[]
    try:compiler_only_section(assembly,preprocessed,cwd,section_name)
    except (RuntimeError,ValueError):return part,[]
    sequences=[]
    records={}
    for row in source_matches(r'\btypedef\s+struct(?:\s+\w+)?\s*\{\s*(?:const\s+)?char\s*\*\s*(?:const\s+)?\w+\s*\[\s*(\d+)\s*\]\s*;\s*\}\s*(\w+)\s*;',preprocessed):
        records[row[2]]=int(row[1])
    direct=r'(?:const\s+)?char\s*\*\s*(?:const\s+)?'
    types=direct+'|'+('|'.join(re.escape(n) for n in records) if records else r'(?!)')
    declaration=re.compile(r'\b(?:static\s+)?('+types+r')\s*(\w+)\s*\[\s*(\d*)\s*\]\s*=\s*\{')
    chars={n:p for n,p in numeric_source_objects(preprocessed,compiled,assembly).items()
           if p['kind']=='literal-only-character-array'
           or p['source_type'] in ('char','signed char','unsigned char')}
    atom=re.compile(r'\s*(?:("(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*)|([A-Za-z_]\w*)(?:\s*\+\s*(0[xX][\da-fA-F]+|\d+))?)\s*')
    for row in source_matches(declaration, preprocessed):
        at=row.end();end=at;depth=1;quote=False;escape=False
        while end<len(preprocessed) and depth:
            c=preprocessed[end]
            if quote:
                if escape:escape=False
                elif c=='\\':escape=True
                elif c=='"':quote=False
            elif c=='"':quote=True
            elif c=='{':depth+=1
            elif c=='}':depth-=1
            end+=1
        if depth or not re.match(r'\s*;',preprocessed[end:]):continue
        initializer=preprocessed[at:end-1]
        # Flatten aggregate braces outside literals only. Braces inside source
        # text remain ordinary character payload and are never rewritten.
        tokens=re.findall(r'"(?:\\.|[^"\\])*"|[^"{}]+|[{}]',initializer)
        initializer=''.join(v for v in tokens if v not in ('{','}'));items=[];cursor=0
        while cursor<len(initializer):
            m=atom.match(initializer,cursor)
            if not m:break
            if m[1]:
                try:payload=b''.join(c_literal_bytes(v) for v in re.findall(r'"(?:\\.|[^"\\])*"',m[1]))+b'\0'
                except (ValueError,UnicodeError):break
                item=dict(payload=payload,source_literal=m[1])
            else:
                proof=chars.get(m[2]);addend=int(m[3],0) if m[3] else 0
                if not proof or not 0<=addend<len(proof['raw']):break
                item=dict(payload=proof['raw'][addend:],source_char_object=m[2],source_addend=addend,
                          destination_symbol=proof['symbol'])
            items.append(item);cursor=m.end()
            if cursor==len(initializer):break
            if initializer[cursor]!=',':break
            cursor+=1
            if not initializer[cursor:].strip():cursor=len(initializer);break
        count=(int(row[3]) if row[3] else len(items))*records.get(row[1].strip(),1)
        if cursor==len(initializer) and 0<len(items)==count<=1024:
            sequences.append(dict(source_name=row[2],source_type=row[1].strip(),items=items,source_offset=row.start()))
    if not sequences:return part,[]
    symbols={s.index:s for s in compiled.symbols};relocs={}
    for rel in compiled.sections:
        if rel.type==9:
            relocs[rel.info]={struct.unpack_from('<II',compiled.data,p)[0]:(struct.unpack_from('<II',compiled.data,p)[1]&255,symbols[struct.unpack_from('<II',compiled.data,p)[1]>>8])
                for p in range(rel.offset,rel.offset+rel.size,8)}
    pointers=relocs.get(csec.index,{});code=compiled.section_bytes(text);text_relocs=relocs.get(text.index,{})
    claimed={f['original_symbol']:f for f in scope};anchors=[]
    for place,(kind,symbol) in text_relocs.items():
        if kind!=5 or symbol.shndx!=csec.index:continue
        high=struct.unpack_from('<I',code,place)[0]
        if high>>26!=15:continue
        for lowplace in range(place+4,min(place+132,len(code)),4):
            lowrel=text_relocs.get(lowplace)
            if not lowrel or lowrel[0]!=6 or lowrel[1].index!=symbol.index:continue
            low=struct.unpack_from('<I',code,lowplace)[0]
            if low>>21&31!=high>>16&31 or low>>26 not in (8,9,*range(32,64)):continue
            offset=((high&65535)<<16)+(low&65535)-(65536 if low&32768 else 0)+symbol.value
            owners=[s for s in compiled.symbols if s.type==2 and s.shndx==text.index and s.name in claimed and s.value<=place<lowplace<s.value+s.size]
            if len(owners)!=1:continue
            f=owners[0];claim=claimed[f.name];of=verification.original_symbol(original,f.name,claim['size'],claim['va'])
            if (f.bind,f.type,f.size)!=(of.bind,of.type,of.size):continue
            os=original.sections[of.shndx];base=os.offset+of.value-os.addr-f.value
            oh,ol=struct.unpack_from('<I',original.data,base+place)[0],struct.unpack_from('<I',original.data,base+lowplace)[0]
            if (high&0xffff0000,low&0xffff0000)!=(oh&0xffff0000,ol&0xffff0000):continue
            address=((oh&65535)<<16)+(ol&65535)-(65536 if ol&32768 else 0)
            anchors.append(dict(compiler_offset=offset,original_address=address,function=f.name,hi_offset=place,lo_offset=lowplace))
    for run in registry.get(section_name,[]):
        for span in run.get('c_input_spans',[]):
            extent=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
            if (not extent or len(coords)!=2 or span.get('section')!=section_name
                or span.get('anonymous_emission') is not True or span.get('symbols')
                or span.get('uncredited_dependency') or span.get('credit')=='uncredited_dependency'
                or coords[1]-coords[0]!=extent[1]-extent[0]
                or not 0<=extent[0]<extent[1]<=csec.size):continue
            for offset,(kind,_) in pointers.items():
                if kind==2 and extent[0]<=offset<offset+4<=extent[1]:
                    anchors.append(dict(compiler_offset=offset,original_address=coords[0]+offset-extent[0],
                        accepted_anonymous_C_input_span=span))
    raw=compiled.section_bytes(csec);proofs=[]
    word=re.compile(r'^(\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+)(0x[\da-fA-F]+)\s*$',re.M)
    def replace(row):
        address=int(row[2],16);value=int(row[4],16)
        retail=original.section(section_name)
        if not retail.addr<=address<address+4<=retail.addr+retail.size or bytes.fromhex(row[3])!=value.to_bytes(4,'little'):return row[0]
        if original.section_bytes(retail)[address-retail.addr:address-retail.addr+4]!=bytes.fromhex(row[3]):return row[0]
        choices=[]
        for anchor in anchors:
            start=anchor['compiler_offset'];lo=anchor['original_address']
            for source in sequences:
                extent=len(source['items'])*4;index=(address-lo)//4
                if not lo<=address<lo+extent or (address-lo)%4 or not 0<=start<start+extent<=len(raw):continue
                if not retail.addr<=lo<lo+extent<=retail.addr+retail.size:continue
                runs=[run for run in registry.get(section_name,[]) if int(run['range'][0],16)<=lo and lo+extent<=int(run['range'][1],16)]
                if len(runs)!=1:continue
                targets=[]
                for i,item in enumerate(source['items']):
                    relocation=pointers.get(start+i*4)
                    if not relocation or relocation[0]!=2 or not 0<relocation[1].shndx<len(compiled.sections):break
                    symbol=relocation[1];dest=compiled.sections[symbol.shndx]
                    if dest.name not in ('.rodata','.sdata'):break
                    offset=symbol.value+struct.unpack_from('<I',raw,start+i*4)[0];payload=item['payload']
                    if compiled.section_bytes(dest)[offset:offset+len(payload)]!=payload:break
                    named=item.get('destination_symbol')
                    if named and (named.shndx,named.value+item['source_addend'])!=(dest.index,offset):break
                    pointer=struct.unpack_from('<I',original.data,retail.offset+lo-retail.addr+i*4)[0]
                    os=original.section(dest.name)
                    if not os.addr<=pointer<pointer+len(payload)<=os.addr+os.size or original.section_bytes(os)[pointer-os.addr:pointer-os.addr+len(payload)]!=payload:break
                    targets.append(dict(value=pointer,section=dest.name,compiler_offset=offset,payload_sha256=hashlib.sha256(payload).hexdigest(),source={k:v for k,v in item.items() if k not in ('payload','destination_symbol')}))
                if len(targets)==len(source['items']) and targets[index]['value']==value:
                    choices.append((source,anchor,runs[0],targets,index))
        unique={(s['source_offset'],a['compiler_offset'],a['original_address']):(s,a,r,t,i) for s,a,r,t,i in choices}
        if len(unique)!=1:return row[0]
        source,anchor,run,targets,index=next(iter(unique.values()));target=targets[index]
        mapped=tu.get('sections',{}).get(target['section'])
        if not mapped:return row[0]
        start=int(mapped['start'],16);candidates=[]
        for symbol in reference.symbols:
            if symbol.type!=1 or symbol.size<=0 or not 0<symbol.shndx<len(reference.sections) or reference.sections[symbol.shndx].name!=target['section']:continue
            if start+symbol.value<=value<start+symbol.value+symbol.size:candidates.append(symbol)
        if len(candidates)!=1:return row[0]
        symbol=candidates[0];addend=value-start-symbol.value;expression=symbol.name+(f' + 0x{addend:X}' if addend else '')
        proofs.append(dict(original_pointer_address=address,original_pointer_value=value,restored_expression=expression,
            source_array=source['source_name'],source_type=source['source_type'],pointer_index=index,
            complete_source_pointer_count=len(targets),original_owner_mapping=anchor,
            registered_owner_run=run['range'],compiler_R_MIPS_32_verified=True,original_char_payloads=targets,
            no_numeric_or_unknown_source_words_admitted=True))
        return row[1]+expression
    return word.sub(replace,part),proofs


def registered_pointer_template_references(part, reference, original, compiled,
        assembly, preprocessed, registry, tu, scope, cwd):
    """Restore literal pointer fields only from the strict complete template proof.

    The source pointer type, actual cc1 R32, unique registered original span,
    original table words and closed original instruction consumer are all
    established by registered_pointer_pool_templates. Pointee storage earns no
    credit here; an existing positive reference owner supplies its expression.
    """
    section_match=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
    if not section_match:return part,[]
    name=section_match[1];proofs=[]
    templates=registered_pointer_pool_templates(original,compiled,assembly,preprocessed,registry,scope,cwd)
    word=re.compile(r'^(\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+)(0x[\da-fA-F]+)\s*$',re.M)
    def replace(row):
        address=int(row[2],16);value=int(row[4],16);raw=bytes.fromhex(row[3])
        if raw!=value.to_bytes(4,'little'):return row[0]
        choices=[]
        for template,va,offset,span,consumers in templates:
            if template['section']!=name or not va<=address<va+len(template['payload']) or (address-va)%4:continue
            index=(address-va)//4
            fields=[p for p in template['source']['actual_R_MIPS32_proofs'] if p['index']==index]
            if len(fields)!=1 or fields[0]['original_pointer']!=value:continue
            field=fields[0]
            retail=original.section(name)
            if (not retail.addr<=address<address+4<=retail.addr+retail.size
                or original.section_bytes(retail)[address-retail.addr:address-retail.addr+4]!=raw):continue
            if field.get('source_kind')=='foreign_char':
                # The complete typed template already proves this exact
                # undefined C R32 symbol and original address. Restoring its
                # expression does not claim storage for the foreign pointee.
                expression=field.get('source_char_name','')
                if (not field.get('exact_actual_R_MIPS32_symbol')
                    or not field.get('no_pointee_storage_credit')
                    or not re.fullmatch(r'D_[\da-fA-F]{8}',expression)
                    or int(expression[2:],16)!=value):continue
                choices.append((template,va,offset,span,consumers,field,expression))
                continue
            dest=field.get('compiler_destination_section')
            mapped=tu.get('sections',{}).get(dest)
            if not mapped:continue
            start=int(mapped['start'],16)
            owners=[s for s in reference.symbols if s.type==1 and s.size>0
                and 0<s.shndx<len(reference.sections) and reference.sections[s.shndx].name==dest
                and start+s.value<=value<start+s.value+s.size]
            if len(owners)!=1:continue
            owner=owners[0];addend=value-start-owner.value
            expression=owner.name+(f' + 0x{addend:X}' if addend else '')
            choices.append((template,va,offset,span,consumers,field,expression))
        if len(choices)!=1:return row[0]
        template,va,offset,span,consumers,field,expression=choices[0]
        proofs.append(dict(original_pointer_address=address,original_pointer_value=value,
            restored_expression=expression,source_array=template['source']['name'],
            source_type=template['source']['source_type'],pointer_index=field['index'],
            complete_source_pointer_count=len(template['payload'])//4,
            registered_pointer_template=template['label'],registered_owner_span=span,
            compiler_R_MIPS_32_verified=True,actual_C_pointer_field=field,
            original_consumer_proofs=consumers,original_bytes_retained=True,
            no_numeric_or_unknown_source_words_admitted=True,no_pointee_storage_credit=True))
        return row[1]+expression
    return word.sub(replace,part),proofs


def compiler_switch_target_labels(reference, original, switch_proofs, tu):
    """Locate only invented global jlabels of already proved switch targets."""
    text=original.section('.text');mapped=tu.get('text')
    if not mapped:return {}
    start=int(mapped['start'],16);result={}
    for proof in switch_proofs:
        if not proof.get('all_real_R_MIPS32_targets_exact') or not proof.get('owner_nonrelocated_instructions_exact'):continue
        for address in set(proof['original_targets']):
            if any(s.name and s.shndx==text.index and s.value==address for s in original.symbols):continue
            name=f'.L{address:08X}'
            if any(s.name==name for s in original.symbols):continue
            symbols=[s for s in reference.symbols if s.name==name and 0<s.shndx<len(reference.sections)
                and reference.sections[s.shndx].name=='.text' and start+s.value==address
                and (s.bind,s.type,s.size)==(1,1,0)]
            if len(symbols)!=1 or sum(s.name==name for s in reference.symbols)!=1:continue
            symbol=symbols[0]
            result[name]=dict(scaffold_name=name,address=address,previous_identity=[symbol.bind,symbol.type,symbol.size],
                restored_identity=[0,0,0],proved_switch=proof['scaffold_name'],generated_by=proof['generated_by'],
                no_real_original_or_colocated_identity=True,all_real_R_MIPS32_targets_exact=True,
                raw_words_and_addresses_retained=True,no_storage_or_credit=True)
    return result


def joint_original(reference, text_reference, data, original_path, destination,
                   cwd, obj, tu, assembler, flags, prefix, compiler_object=None,
                   compiler_assembly=None, preprocessed='', scope=(), registry=None):
    """Assemble one original TU, retaining its actual local DATA identities.

    Separately assembled Splat dlabels are global so text can refer to them.
    That changes MIPS local SECTION/GP relocations. Joint assembly restores
    only uniquely evidenced original identities. A normal linker then resolves
    every relocation at the original section addresses; all allocated bytes
    must equal the original before this reference can be used.
    """
    original, before = parsed_elf(original_path.read_bytes()), parsed_elf(reference.read_bytes())
    symbols_by_address, values_by_name, original_names = original_symbol_indexes(original.data)
    source = text_reference.with_suffix('.s')
    if not source.is_file():source=Path(str(text_reference)+'.s')
    if not source.is_file():return reference, []
    body=source.read_text();identities=[];switch_proofs=[];padding_proofs=[];string_proofs=[];nobits_proofs=[];static_proofs=[];marker_proofs=[];pointer_proofs=[];source_pool_proofs=[]
    for path, scaffold_source in data:
        source=path.with_suffix('.s')
        if not source.is_file():source=scaffold_source
        if not source.is_file():return reference, []
        part=source.read_text()
        part=re.sub(r'^\s*\.include\s+"macro.inc"\s*$', '',part,flags=re.M)
        part=re.sub(r'^(\.section\s+\.[\w.]+),[^\n]*$',r'\1',part,flags=re.M)
        if compiler_object and compiler_assembly:
            part,typed_pointer_rows=registered_pointer_template_references(part,before,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,registry or {},tu,scope,cwd)
            pointer_proofs.extend(typed_pointer_rows)
            part,pointer_rows=compiler_character_pointer_references(part,before,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,registry or {},tu,scope,cwd)
            pointer_proofs.extend(pointer_rows)
            part,pad_proofs=source_alignment_padding(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),registry or {},tu,parsed_elf(path.read_bytes()))
            padding_proofs.extend(pad_proofs)
            part,literal_proofs=compiler_string_identity(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},cwd,parsed_elf(path.read_bytes()))
            string_proofs.extend(literal_proofs)
            part,alias_proofs=compiler_nobits_alias_identity(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,registry or {},tu,obj['id'])
            nobits_proofs.extend(alias_proofs)
            part,local_proofs=compiler_function_static_identity(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu)
            static_proofs.extend(local_proofs)
            part,natural_proofs=compiler_original_static_name(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu)
            static_proofs.extend(natural_proofs)
        if compiler_object and compiler_assembly and 'dlabel jtbl_' in part:
            part,table_proofs=compiler_switch_identity(part,original,parsed_elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu,cwd)
            switch_proofs.extend(table_proofs)
        section=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
        if not section:return reference, []
        native=original.section(section[1])
        part,aliases=splat_data_marker_identity(part,original,parsed_elf(path.read_bytes()),before,cwd/'include/macro.inc')
        marker_proofs.extend(aliases)
        identity_source=part
        for label in re.finditer(r'^dlabel\s+(\S+)\s*$',identity_source,re.M):
            name=label[1]
            address=re.search(r'/\*\s+(?:\S+\s+)?([0-9A-Fa-f]{8})(?:\s|\s*\*/)',identity_source[label.end():])
            if not address:continue
            va=int(address[1],16)
            matches=[s for s in symbols_by_address.get((native.index,va), ())
                     if name in (s.name,s.name.replace('.','_'),s.name.replace('.','_')+f'_{va:08X}')]
            if len(matches)!=1:continue
            symbol=matches[0]
            definition=f'.{"local" if symbol.bind==0 else "globl"} {name}\n'
            if symbol.type==1:definition+=f'.type {name}, @object\n'
            definition+=name+':'
            part=re.sub(r'^dlabel\s+'+re.escape(name)+r'\s*$',lambda _:definition,part,flags=re.M)
            removed_marker=False
            if name+'.NON_MATCHING' not in original_names:
                part,count=re.subn(r'^\s*nonmatching\s+'+re.escape(name)+r'(?:\s*,\s*[^\n]+)?\s*$', '',part,flags=re.M)
                removed_marker=bool(count)
            # This legacy assembler retains the first size directive. Splat's
            # enddlabel sizes the mapped allocation, including alignment, so
            # replace only this restored symbol's metadata before setting its
            # genuine original size. Storage directives remain untouched.
            part=re.sub(r'^\s*enddlabel\s+'+re.escape(name)+r'\s*$', '',part,flags=re.M)
            part=re.sub(r'^\s*\.size\s+'+re.escape(name)+r'\s*,[^\n]*$', '',part,flags=re.M)
            part+=f'\n.size {name}, {symbol.size}\n'
            identities.append(dict(target_name=name,original_name=symbol.name,address=va,
                                   binding=symbol.bind,type=symbol.type,size=symbol.size,
                                   removed_invented_marker=removed_marker))
            # A genuine empty DATA object may share the next positive object's
            # address. Splat names only the empty alias; retain both retail
            # identities without assigning any storage to the empty object.
            if (not obj['functions'] and symbol.type == 1 and symbol.size == 0
                    and compiler_object and compiler_assembly):
                compiled = parsed_elf(compiler_object.read_bytes())
                emitted = typed_compiler_data(compiler_assembly.read_text())
                empty = [s for s in compiled.symbols if s.name == symbol.name
                         and (s.bind, s.type, s.size) == (symbol.bind, 1, 0)
                         and 0 < s.shndx < len(compiled.sections)
                         and compiled.sections[s.shndx].name == native.name]
                positive = [s for s in symbols_by_address.get((native.index, va), ())
                            if s.type == 1 and s.size > 0 and s.bind == symbol.bind]
                owners = [r for r in (registry or {}).get(native.name, [])
                          if name in r.get('c_owned_symbols', [])
                          and int(r['range'][0], 16) <= va
                          and len(positive) == 1
                          and va + positive[0].size <= int(r['range'][1], 16)]
                if len(empty) == len(positive) == len(owners) == 1:
                    owner = positive[0]
                    peers = [s for s in compiled.symbols if s.name == owner.name
                             and (s.bind, s.type, s.size, s.shndx, s.value) ==
                             (owner.bind, 1, owner.size, empty[0].shndx, empty[0].value)]
                    emission = emitted.get(owner.name)
                    existing = re.search(r'^(?:dlabel\s+|\s*)' + re.escape(owner.name)
                                         + r'(?::|\s*$)', identity_source, re.M)
                    if (len(peers) == 1 and emission and not existing
                            and (emission.get('section'), emission.get('size')) ==
                            (native.name, owner.size)):
                        part += (f'\n.{"local" if owner.bind == 0 else "globl"} {owner.name}\n'
                                 f'.type {owner.name}, @object\n.equ {owner.name}, {name}\n'
                                 f'.size {owner.name}, {owner.size}\n')
                        identities.append(dict(target_name=owner.name, original_name=owner.name,
                                               address=va, binding=owner.bind, type=1,
                                               size=owner.size, restored_colocated_empty_alias=name,
                                               registered_run=owners[0]['range']))
        body+='\n'+part
    switch_label_candidates={}
    switch_label_proofs={}
    if switch_proofs or string_proofs or nobits_proofs or static_proofs:
        replacements={p['scaffold_name']:p['temporary_reference_name'] for p in switch_proofs+string_proofs+nobits_proofs+static_proofs}
        def rename(text):
            for old,new in replacements.items():
                text=re.sub(r'(?<![\w.$])'+re.escape(old)+r'(?![\w.$])',lambda _:new,text)
            def local_target(match):
                name=match[1]
                if name not in switch_label_candidates:return match[0]
                switch_label_proofs[name]=switch_label_candidates[name]
                # This contractual assembler gives a local unsized label its
                # actual NOTYPE0 identity; it does not accept @notype syntax.
                return f'.local {name}\n{name}:'
            text=re.sub(r'^\s*jlabel\s+(\.L[\da-fA-F]{8})\s*$',local_target,text,flags=re.M)
            return text
        def include(match):
            path=Path(match[1]);path=path if path.is_absolute() else cwd/path
            old=path.read_text();new=rename(old)
            if new==old:return match[0]
            local=destination.parent/'joint-asm'/path.name
            local.parent.mkdir(exist_ok=True);local.write_text(new)
            return '.include '+json.dumps(str(local))
        body=rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,body))
    assembly=destination.with_suffix('.s');assembly.write_text(body)
    try:
        run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
             '-I.','-Iinclude','-o',destination,assembly],cwd)
        after=parsed_elf(destination.read_bytes())
        switch_label_candidates=compiler_switch_target_labels(after,original,switch_proofs,tu)
        if switch_label_candidates:
            body=rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,body))
            if switch_label_proofs:
                previous=after
                assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
                if original_reference_signature(previous)!=original_reference_signature(after):return reference,[]
                others=lambda e:{(s.name,s.value,s.size,s.bind,s.type,s.other,s.shndx) for s in e.symbols
                    if s.name not in switch_label_proofs}
                if others(previous)!=others(after):return reference,[]
        # Record the independent typed/source/raw/original-consumer witness
        # before older generic anonymous metadata routes can consume its label.
        if compiler_object and compiler_assembly:
            body,source_pool_proofs=registered_source_pool_identities(body,after,original,
                parsed_elf(compiler_object.read_bytes()),compiler_assembly.read_text(),
                preprocessed,registry or {},tu,cwd,scope)
            if source_pool_proofs:
                assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
        anonymous_char_proofs=[]
        if compiler_object and compiler_assembly:
            rewritten,anonymous_char_proofs=registered_anonymous_char_identities(body,after,original,
                parsed_elf(compiler_object.read_bytes()),compiler_assembly.read_text(),preprocessed,registry,tu,cwd,destination,scope)
            if anonymous_char_proofs:
                old=after;body=rewritten;assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
                if original_reference_signature(old)!=original_reference_signature(after):return reference,[]
                removed={p['scaffold_name'] for p in anonymous_char_proofs}
                temporaries={p['temporary_reference_name'] for p in anonymous_char_proofs}
                symbols=lambda e:{(s.name,s.value,s.size,s.bind,s.type,s.other,
                    e.sections[s.shndx].name if 0<s.shndx<len(e.sections) else s.shndx) for s in e.symbols if s.name}
                if {s for s in symbols(old) if s[0] not in removed}!={s for s in symbols(after) if s[0] not in temporaries}:return reference,[]
                if any((s.bind,s.type,s.size)!=(0,0,0) for s in after.symbols if s.name in temporaries):return reference,[]
        registered_nobits_proofs=[]
        if compiler_object and compiler_assembly:
            rewritten,registered_nobits_proofs=registered_nobits_foreign_aliases(body,after,original,
                parsed_elf(compiler_object.read_bytes()),compiler_assembly.read_text(),preprocessed,registry,tu,cwd,destination)
            if registered_nobits_proofs:
                old=after;assembly.write_text(rewritten)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
                if original_reference_signature(old)!=original_reference_signature(after):return reference,[]
                removed={p['scaffold_name'] for p in registered_nobits_proofs}
                temporaries={p['temporary_reference_name'] for p in registered_nobits_proofs}
                symbols=lambda e:{(s.name,s.value,s.size,s.bind,s.type,s.other,
                    e.sections[s.shndx].name if 0<s.shndx<len(e.sections) else s.shndx) for s in e.symbols if s.name}
                if {s for s in symbols(old) if s[0] not in removed}!={s for s in symbols(after) if s[0] not in temporaries}:return reference,[]
                if any((s.bind,s.type,s.size)!=(0,0,0) for s in after.symbols if s.name in temporaries):return reference,[]
        storage_extent_proofs=[]
        if compiler_object and compiler_assembly:
            from types import SimpleNamespace
            helpers=SimpleNamespace(typed_compiler_data=typed_compiler_data,explicit_object_range=explicit_object_range,original_references=original_references)
            compiled=parsed_elf(compiler_object.read_bytes());cc1=compiler_assembly.read_text()
            body=assembly.read_text()
            body,found=reconstruct(body,after,original,compiled,cc1,preprocessed,registry or {},tu,helpers)
            storage_extent_proofs.extend(found)
            body,found=remove_synthetic_padding(body,after,original,compiled,cc1,registry or {},tu,helpers)
            storage_extent_proofs.extend(found)
            body,found=nobits_registered_extents(body,after,original,compiled,cc1,registry or {},tu,helpers)
            storage_extent_proofs.extend(found)
            body,found=registered_initialized_extents(body,after,original,compiled,cc1,preprocessed,registry or {},tu,cwd)
            storage_extent_proofs.extend(found)
            if storage_extent_proofs:
                previous=after;assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
                if original_reference_signature(previous)!=original_reference_signature(after):return reference,[]
                sizes={p['symbol']:p['reconstructed_object_size'] for p in storage_extent_proofs if 'reconstructed_object_size' in p}
                removed={n for p in storage_extent_proofs if p.get('retained_all_storage') for n in (p['symbol'],p['symbol']+'.NON_MATCHING')}
                added={a['name'] for p in storage_extent_proofs for a in p.get('additional_registered_owners',[])}
                for proof in storage_extent_proofs:
                    for owner in proof.get('additional_registered_owners',[]):
                        aliases=[s for s in after.symbols if s.name==owner['name']]
                        if len(aliases)!=1:return reference,[]
                        alias=aliases[0]
                        if not 0<alias.shndx<len(after.sections) or after.sections[alias.shndx].name!=proof['section']:return reference,[]
                        start=int(tu['sections'][proof['section']]['start'],16)
                        if (alias.value+start,alias.bind,alias.type,alias.size)!=(owner['address'],owner['binding'],owner['type'],owner['size']):return reference,[]
                signature=lambda e,adjust:{(s.name,s.value,adjust.get(s.name,s.size),s.bind,s.type,s.other,s.shndx) for s in e.symbols if s.name not in removed and s.name not in added}
                if signature(previous,sizes)!=signature(after,{}):return reference,[]
        named_literal_proofs=[];literal_word_proofs=[];nobits_zero_size_proofs=[]
        if compiler_object and compiler_assembly:
            compiled=parsed_elf(compiler_object.read_bytes());cc1=compiler_assembly.read_text()
            body=assembly.read_text()
            body,found=registered_source_pool_identities(body,after,original,compiled,
                cc1,preprocessed,registry or {},tu,cwd,scope)
            source_pool_proofs.extend(found)
            body,named_literal_proofs=registered_named_literal_identities(body,after,before,original,compiled,
                cc1,preprocessed,registry or {},tu,cwd)
            body,literal_word_proofs=registered_literal_word_references(body,original,compiled,
                cc1,preprocessed,registry or {},tu,cwd,scope)
            body,nobits_zero_size_proofs=registered_nobits_zero_sizes(body,after,original,compiled,
                cc1,registry or {},tu,identities)
            if source_pool_proofs or named_literal_proofs or literal_word_proofs or nobits_zero_size_proofs:
                assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=parsed_elf(destination.read_bytes())
        funcs=lambda e:{(s.name,s.value,s.size,s.bind,s.type) for s in e.symbols if s.type==2}
        if funcs(before)!=funcs(after):return reference, []
        for proof in switch_label_proofs.values():
            labels=[s for s in after.symbols if s.name==proof['scaffold_name']]
            # Ordinary local .L labels are omitted by the contractual
            # assembler, just as cc1's anonymous case labels are omitted.
            # The preceding byte/relocation signature proves their addresses.
            if not labels:
                proof['assembler_local_symbol_elided']=True
                proof['allocated_bytes_and_real_relocations_unchanged']=True
                continue
            if len(labels)!=1:return reference,[]
            label=labels[0]
            if (not 0<label.shndx<len(after.sections) or after.sections[label.shndx].name!='.text'
                or (label.bind,label.type,label.size)!=(0,0,0)
                or label.value+int(tu['text']['start'],16)!=proof['address']):return reference,[]
            proof['assembler_local_symbol_elided']=False
            proof['allocated_bytes_and_real_relocations_unchanged']=True
        for old in before.sections:
            if not old.flags&2:continue
            # The modern scaffold assembler adds its ABI declaration section.
            # The original and contractual assembler have no such section;
            # this is ELF metadata, never original code or DATA coverage.
            if old.name=='.MIPS.abiflags' and old.type==0x7000002a and not any(s.name==old.name for s in original.sections):continue
            new=after.section(old.name)
            if (old.type,old.flags,old.size,old.align)!=(new.type,new.flags,new.size,new.align):return reference, []
        for proof in identities:
            symbols=[s for s in after.symbols if s.name==proof['target_name']]
            if len(symbols)!=1:return reference, []
            s=symbols[0]
            start=int(tu['sections'][after.sections[s.shndx].name]['start'],16)
            size=proof['size']
            annotation=next((p for p in storage_extent_proofs if p['symbol']==proof['target_name'] and p.get('original_size_remains_unspecified')),None)
            if annotation:
                if (proof['binding'],proof['type'],size)!=(0,0,0):return reference,[]
                size=annotation['reconstructed_object_size']
            if any(p['target_name']==proof['target_name'] for p in nobits_zero_size_proofs):size=0
            if (s.value+start,s.bind,s.type,s.size)!=(proof['address'],proof['binding'],proof['type'],size):return reference, []
        # Link the reference normally at original addresses. This checks every
        # changed local addend together with all unchanged foreign relocations.
        definitions=[];call_proofs=[];unused_external_proofs=[]
        for symbol in after.symbols:
            if symbol.shndx!=0 or not symbol.name:continue
            if symbol.name=='_gp':continue
            values=set(values_by_name.get(symbol.name, ()))
            synthetic=re.fullmatch(r'D_([\da-fA-F]+)',symbol.name)
            if not values and synthetic:values={int(synthetic[1],16)}
            if not values:
                call=original_synthetic_call_destination(symbol,after,original,tu,body,cwd,scope)
                if call:
                    values={call['encoded_destination']};call_proofs.append(call)
            if not values:
                unused=unused_scaffold_external(symbol,after,original,body,cwd)
                if unused:
                    unused_external_proofs.append(unused)
                    continue
            if len(values)!=1:return reference, []
            definitions.append(f'{symbol.name} = 0x{values.pop():08X};')
        placements=[]
        for section in after.sections:
            if not section.flags&2 or not section.size:continue
            if section.name=='.text':start=int(tu['text']['start'],16)
            elif section.name in tu.get('sections',{}):start=int(tu['sections'][section.name]['start'],16)
            else:return reference, []
            placements.append(f'{section.name} 0x{start:08X} : {{ *({section.name}) }}')
        script=destination.with_suffix('.ld');script.write_text(f'_gp = 0x{original.gp:08X};\n'+ '\n'.join(definitions)+'\nSECTIONS {\n'+'\n'.join(placements)+'\n}\n')
        linked=destination.with_suffix('.elf')
        run([str(prefix)+'ld','-EL','-m','elf32lr5900','-T',script,'-o',linked,destination],cwd)
        final=parsed_elf(linked.read_bytes())
        for section in final.sections:
            if not section.flags&2 or not section.size:continue
            original_section=original.section(section.name)
            if not original_section.addr<=section.addr<=section.addr+section.size<=original_section.addr+original_section.size:return reference, []
            if section.type!=8:
                expected=original.data[original_section.offset+section.addr-original_section.addr:
                                       original_section.offset+section.addr-original_section.addr+section.size]
                if final.section_bytes(section)!=expected:return reference, []
        destination.with_suffix('.proof.json').write_text(json.dumps(dict(
            schema='objdiff-original-joint-reference/1',tu=obj['id'],original_sha256=hashlib.sha256(original.data).hexdigest(),
            original_data_identities=identities,allocated_extents_unchanged=True,
            reconstructed_storage_extent_proofs=storage_extent_proofs,
            compiler_switch_table_identity_proofs=switch_proofs,
            source_alignment_padding_proofs=padding_proofs,
            compiler_string_identity_proofs=string_proofs,
            compiler_character_pointer_reference_proofs=pointer_proofs,
            compiler_nobits_alias_identity_proofs=nobits_proofs,
            registered_nobits_foreign_alias_identity_proofs=registered_nobits_proofs,
            registered_anonymous_char_identity_proofs=anonymous_char_proofs,
            registered_source_pool_identity_proofs=source_pool_proofs,
            compiler_switch_target_label_identity_proofs=list(switch_label_proofs.values()),
            registered_named_literal_identity_proofs=named_literal_proofs,
            registered_literal_word_reference_proofs=literal_word_proofs,
            registered_nobits_zero_size_proofs=nobits_zero_size_proofs,
            compiler_function_static_identity_proofs=static_proofs,
            splat_data_marker_identity_proofs=marker_proofs,
            original_synthetic_call_destination_proofs=call_proofs,
            unused_scaffold_external_proofs=unused_external_proofs,
            compiler_object_sha256=hashlib.sha256(compiler_object.read_bytes()).hexdigest() if compiler_object else None,
            compiler_assembly_sha256=hashlib.sha256(compiler_assembly.read_bytes()).hexdigest() if compiler_assembly else None,
            reference_assembly_sha256=hashlib.sha256(assembly.read_bytes()).hexdigest(),
            reference_object_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
            linked_reference_sha256=hashlib.sha256(linked.read_bytes()).hexdigest(),
            normal_link_all_original_allocated_bytes_exact=True),indent=2)+'\n')
        return destination,identities
    except (subprocess.CalledProcessError,StopIteration,KeyError):
        return reference, []


def strip_debug(prefix, source, destination, cwd, iop_reference=False):
    # Old GCC emits ECOFF debug records that objdiff's MIPS reader cannot parse
    # reliably. Remove debug records only; verify every allocated section and
    # function identity survives unchanged. No instruction/data normalization.
    before = parsed_elf(source.read_bytes())
    # IRX stores module-relative data addresses/relocations. Make its data
    # sections section-relative for objdiff, keeping text addresses and every
    # FUNC identity. Target-only IOP references have no base and earn zero.
    options = (['--remove-section=.rel.text', '--remove-section=.rel.data', '--strip-all',
                *[f'--change-section-address={s.name}=0' for s in before.sections
                  if s.flags & 2 and not s.flags & 4],
                *[f'--keep-symbol={name}' for name in functions(source)]] if iop_reference else [])
    run([str(prefix) + 'objcopy', '--strip-debug', *options, source, destination], cwd)
    after = parsed_elf(destination.read_bytes())
    allocated = lambda elf: {s.name: (s.type, s.flags,
                             0 if iop_reference and not s.flags & 4 else s.addr,
                             s.size, elf.section_bytes(s))
                             for s in elf.sections if s.flags & 2}
    verification.require(allocated(before) == allocated(after) and
                         functions(source) == functions(destination),
                         f'{source}: debug stripping changed report inputs')
    identity = lambda elf: {(s.name, s.value, s.size, s.bind, s.type)
                           for s in elf.symbols if s.type == 2 and s.size}
    verification.require(identity(before) == identity(after), f'{source}: FUNC identity changed')


"""Reconstruct anonymous char OBJECT extents without changing section storage.

Splat's dlabel bucket reaches the next label, which can include alignment after
the string. It is not an original symbol size. This helper requires independently
decoded C character storage, its registered placement and original bytes, and
retains every original section byte, relocation, label and section extent.
"""
import ast
import hashlib
import re
import struct


def character_storage(preprocessed, compiled, assembly, oi):
    from data_types import _record_split, _record_bound, RecordLayoutError
    emitted = oi.typed_compiler_data(assembly)
    pattern = re.compile(r'\b(?:static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?char\s+(\w+)\s*\[\s*([^\[\]]*)\s*\]\s*=\s*')
    result = {}
    for m in source_matches(pattern, preprocessed):
        name, bound = m[1], m[2].strip()
        if name not in emitted:
            continue
        count = None
        if bound:
            try:
                count = _record_bound(bound)
            except RecordLayoutError:
                continue
        at = m.end()
        literal = re.match(r'("(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*)\s*;', preprocessed[at:])
        payload = None
        if literal:
            try:
                payload = b''.join(c_literal_bytes(t) for t in re.findall(r'"(?:\\.|[^"\\])*"', literal[1])) + b'\0'
            except (ValueError, SyntaxError, UnicodeError):
                continue
        elif preprocessed[at:at+1] == '{':
            end = preprocessed.find('}', at)
            if end < 0 or not re.match(r'\s*;', preprocessed[end+1:]):
                continue
            values = _record_split(preprocessed[at+1:end], ',')
            decoded = []
            for item in values:
                try:
                    if re.fullmatch(r"'(?:\\.|[^'\\])+'", item):
                        value = c_literal_bytes(item)
                        if len(value) != 1:
                            break
                        value = value[0]
                    elif re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*', item):
                        token = re.sub(r'[uUlL]+$', '', item)
                        value = int(token, 16 if 'x' in token.lower() else 10)
                    else:
                        break
                    if not -128 <= value <= 255:
                        break
                    decoded.append(value % 256)
                except (ValueError, SyntaxError):
                    break
            else:
                payload = bytes(decoded)
        if payload is None:
            continue
        if count is None:
            count = len(payload)
        if not 0 < len(payload) <= count <= 65536:
            continue
        payload += bytes(count-len(payload))
        symbols = [s for s in compiled.symbols if s.name == name and s.type == 1 and s.size == count and 0 < s.shndx < len(compiled.sections)]
        if len(symbols) != 1:
            continue
        symbol = symbols[0]
        section = compiled.sections[symbol.shndx]
        if section.type != 1 or emitted[name]['section'] != section.name or emitted[name]['size'] != count:
            continue
        if compiled.section_bytes(section)[symbol.value:symbol.value+count] != payload:
            continue
        if any(rel.type == 9 and rel.info == section.index and any(symbol.value <= struct.unpack_from('<I', compiled.data, p)[0] < symbol.value+count for p in range(rel.offset, rel.offset+rel.size, 8)) for rel in compiled.sections):
            continue
        result[name] = (symbol, payload, {'capacity': count, 'bound_inferred_from_initializer': not bool(bound), 'source_initializer_sha256': hashlib.sha256(payload).hexdigest(), 'cc1_emission': emitted[name]})
    return result


def reconstruct(body, reference, original, compiled, assembly, preprocessed, registry, tu, oi):
    """Correct only invented char bucket sizes; leave every storage directive."""
    storage = character_storage(preprocessed, compiled, assembly, oi)
    # Pointer-free literal arrays and records have the same independent
    # initializer proof as character arrays. Genuine retail sizes remain
    # excluded below; only invented scaffold bucket sizes are reconstructed.
    for name,proof in numeric_source_objects(preprocessed,compiled,assembly).items():
        if name not in storage and proof['integer_offsets'] is None:
            storage[name]=(proof['symbol'],proof['raw'],{k:v for k,v in proof.items() if k not in ('symbol','raw')})
    proofs = []
    block = re.compile(r'dlabel\s+(D_[0-9A-Fa-f]{8})\s*\n(.*?)enddlabel\s+\1\s*\n\.size\s+\1,\s*\.\s*-\s*\1', re.S)
    def replace(match):
        name = match[1]
        if name not in storage:
            return match[0]
        cs, payload, source_proof = storage[name]
        symbols = [s for s in reference.symbols if s.name == name and s.type == 1 and 0 < s.shndx < len(reference.sections)]
        if len(symbols) != 1:
            return match[0]
        rs = symbols[0]
        section = reference.sections[rs.shndx]
        if section.name not in ('.data', '.sdata', '.rodata') or section.name != compiled.sections[cs.shndx].name or not 0 < cs.size < rs.size:
            return match[0]
        mapped = tu.get('sections', {}).get(section.name)
        if not mapped:
            return match[0]
        lo = int(mapped['start'], 16) + rs.value
        hi = lo + rs.size
        end = lo + cs.size
        if lo != int(name[2:], 16) or hi > int(mapped['end'], 16):
            return match[0]
        retail = original.section(section.name)
        if any(s.name and s.shndx == retail.index and s.value < hi and lo < s.value + max(s.size, 1) for s in original.symbols):
            return match[0]
        raw = original.section_bytes(retail)[lo-retail.addr:hi-retail.addr]
        if len(raw) != rs.size or raw[:cs.size] != payload or any(raw[cs.size:]):
            return match[0]
        spans = []
        for run in registry.get(section.name, []):
            if name not in run.get('c_owned_symbols', []):
                continue
            for span in run.get('c_input_spans', []):
                extent = oi.explicit_object_range(span)
                if not extent or span.get('section') != section.name or name not in span.get('symbols', []):
                    continue
                coords = [int(v, 16) for v in span['range']]
                if coords[1]-coords[0] == extent[1]-extent[0] and extent[0] <= cs.value and cs.value+cs.size <= extent[1] and coords[0]+cs.value-extent[0] == lo:
                    spans.append(span)
        if not spans:
            # Older accepted carves place the entire emitted section as one
            # dense block. Its generated extent and every payload byte must
            # agree; no absent object_range is interpreted as zero.
            runs = registry.get(section.name, [])
            csec = compiled.sections[cs.shndx]
            if len(runs) == 1 and not runs[0].get('c_input_spans') and name in runs[0].get('c_owned_symbols', []):
                run = runs[0]
                generated = [int(v, 16) for v in run.get('generated', [])]
                if len(generated) == 2 and generated[0] == int(mapped['start'],16) and generated[1]-generated[0] == csec.size and generated[0]+cs.value == lo:
                    original_payload = original.section_bytes(retail)[generated[0]-retail.addr:generated[1]-retail.addr]
                    if compiled.section_bytes(csec) == original_payload and not any(s.type==9 and s.info==csec.index and s.size for s in compiled.sections):
                        spans.append({'legacy_complete_section': True, 'generated': run['generated'], 'actual_compiler_section_size': csec.size, 'complete_original_payload_sha256': hashlib.sha256(original_payload).hexdigest()})
        if len(spans) != 1 or oi.original_references(original, end, hi):
            return match[0]
        # The source bound is independently decoded; no section length, byte,
        # relocation expression, original symbol or source definition changes.
        proofs.append({'symbol': name, 'section': section.name, 'address': lo, 'synthetic_bucket_size': rs.size, 'reconstructed_object_size': cs.size, 'retained_zero_tail': [end, hi], 'source': source_proof, 'registered_span': spans[0], 'no_retail_identity': True, 'no_tail_reference': True})
        return 'dlabel '+name+'\n'+match[2]+'.size '+name+', '+str(cs.size)+'\n'
    return block.sub(replace, body), proofs


def remove_synthetic_padding(body, reference, original, compiled, assembly, registry, tu, oi):
    """Remove invented identities on short unreferenced zero alignment gaps.

    An original positive-size OBJECT fixes the beginning of each gap. The C
    owner must have that same identity and emitted size, and be registered.
    Pointer owners qualify too: their relocations and all bytes are untouched
    and the normal joint link verifies their complete original values.
    """
    proofs = []
    emitted = oi.typed_compiler_data(assembly)
    block = re.compile(r'(?:nonmatching\s+(D_[\da-fA-F]+)(?:\s*,[^\n]+)?\s*\n\s*)?(?:/\* Automatically generated and unreferenced pad \*/\s*)?dlabel\s+(D_[\da-fA-F]+)\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*\.\s*-\s*\2',re.S)
    def replace(m):
        name=m[2]
        symbols=[s for s in reference.symbols if s.name==name and s.type==1 and 0<s.shndx<len(reference.sections)]
        if len(symbols)!=1:
            return m[0]
        sym=symbols[0];sec=reference.sections[sym.shndx]
        if sec.name not in ('.data','.rodata','.sdata') or not 0<sym.size<=4095:
            return m[0]
        lo=int(name[2:],16);hi=lo+sym.size
        mapped=tu.get('sections',{}).get(sec.name)
        if not mapped or int(mapped['start'],16)+sym.value!=lo or hi>int(mapped['end'],16):
            return m[0]
        retail=original.section(sec.name)
        # A larger inferred gap must also be bounded by a genuine following
        # OBJECT. This is not an assertion about a missing C storage owner.
        if sym.size>15 and not any(s.name and s.shndx==retail.index and s.type==1 and s.size>0 and s.value==hi for s in original.symbols):
            return m[0]
        raw=original.section_bytes(retail)[lo-retail.addr:hi-retail.addr]
        if len(raw)!=sym.size or any(raw) or any(s.name and s.shndx==retail.index and s.value<hi and lo<s.value+max(s.size,1) for s in original.symbols):
            return m[0]
        owners=[s for s in original.symbols if s.shndx==retail.index and s.type==1 and s.size>0 and s.value+s.size==lo]
        if len(owners)!=1:
            return m[0]
        owner=owners[0]
        names={owner.name,owner.name.replace('.','_'),owner.name.replace('.','_')+f'_{owner.value:08X}'}
        candidates=[s for s in compiled.symbols if s.name in names and (s.bind,s.type,s.size)==(owner.bind,owner.type,owner.size) and 0<s.shndx<len(compiled.sections) and compiled.sections[s.shndx].name==sec.name and s.name in emitted and (emitted[s.name]['section'],emitted[s.name]['size'])==(sec.name,owner.size)]
        if len(candidates)!=1:
            return m[0]
        cs=candidates[0]
        runs=[r for r in registry.get(sec.name,[]) if cs.name in r.get('c_owned_symbols',[]) and int(r['range'][0],16)<=owner.value and lo<=int(r['range'][1],16)]
        if len(runs)!=1 or oi.original_references(original,lo,hi):
            return m[0]
        # A relocation targeting the synthetic gap would require keeping its
        # identity; this check is independent of the original reference scan.
        if any(s.type==9 and any(struct.unpack_from('<II',reference.data,p)[1]>>8==sym.index for p in range(s.offset,s.offset+s.size,8)) for s in reference.sections):
            return m[0]
        proofs.append({'symbol':name,'section':sec.name,'range':[lo,hi],'original_owner':owner.name,'original_owner_size':owner.size,'compiler_owner':cs.name,'cc1_emission':emitted[cs.name],'retained_all_storage':True,'no_original_identity_or_reference':True})
        return m[3]
    return block.sub(replace,body),proofs


def nobits_registered_extents(body, reference, original, compiled, assembly, registry, tu, oi):
    """Give unspecified NOBITS labels their accepted compiler storage extent.

    This does not claim that retail st_size was positive. Retail NOTYPE/SIZE0
    provides the identity; accepted storage spans provide the comparison view.
    Existing positive retail sizes and all section storage remain untouched.
    """
    emitted=emitted_storage(assembly)
    proofs=[]
    for rs in reference.symbols:
        if (rs.bind,rs.type,rs.size)!=(0,0,0) or not rs.name or not 0<rs.shndx<len(reference.sections):
            continue
        sec=reference.sections[rs.shndx]
        mapped=tu.get('sections',{}).get(sec.name)
        if sec.type!=8 or not mapped:
            continue
        retail=original.section(sec.name)
        lo=int(mapped['start'],16)+rs.value
        originals=[s for s in original.symbols if s.shndx==retail.index and s.value==lo and s.name and (s.name==rs.name or s.name.replace('.','_')==rs.name or s.name.replace('.','_')+f'_{lo:08X}'==rs.name)]
        if len(originals)!=1 or (originals[0].bind,originals[0].type,originals[0].size)!=(0,0,0):
            continue
        candidates=[s for s in compiled.symbols if s.name==rs.name and (s.bind,s.type,s.size)==(0,0,0) and 0<s.shndx<len(compiled.sections) and compiled.sections[s.shndx].name==sec.name]
        definition=emitted.get(rs.name)
        if len(candidates)!=1 or not definition or definition['section']!=sec.name or definition['extent_kind']!='label-space':
            continue
        cs=candidates[0];size=definition['size'];hi=lo+size
        if not 0<size or rs.value+size>sec.size or cs.value+size>compiled.sections[cs.shndx].size:
            continue
        # Do not replace an interior inferred extent that also contains
        # alignment. A final owner has no following real symbol and qualifies.
        following=[s.value for s in original.symbols if s.shndx==retail.index and s.name and s.type in (0,1) and lo<s.value<int(mapped['end'],16)]
        cfollowing=[s.value for s in compiled.symbols if s.shndx==cs.shndx and s.name and s.type in (0,1) and s.value>cs.value]
        if following and (min(following)!=hi or not cfollowing or min(cfollowing)!=cs.value+size):
            continue
        spans=[]
        for run in registry.get(sec.name,[]):
            if rs.name not in run.get('c_owned_symbols',[]):
                continue
            for span in run.get('c_input_spans',[]):
                extent=oi.explicit_object_range(span)
                addresses=[int(x,16) for x in span.get('range',[])]
                if (extent and len(addresses)==2 and span.get('section')==sec.name and rs.name in span.get('symbols',[])
                    and span.get('credit')!='uncredited_dependency' and not span.get('uncredited_dependency')
                    and extent[1]-extent[0]==addresses[1]-addresses[0]
                    and extent[0]<=cs.value and cs.value+size<=extent[1]
                    and addresses[0]+cs.value-extent[0]==lo):
                    spans.append(span)
        if len(spans)!=1:
            continue
        if any(s.name and s.shndx==retail.index and s is not originals[0] and
               ((s.size>0 and s.value<hi and lo<s.value+s.size) or lo<s.value<hi) for s in original.symbols):
            continue
        pattern=re.compile(r'^\s*\.size\s+'+re.escape(rs.name)+r'\s*,\s*0\s*$',re.M)
        if len(pattern.findall(body))!=1:
            continue
        body=pattern.sub('.size '+rs.name+', '+str(size),body)
        proofs.append({'symbol':rs.name,'section':sec.name,'address':lo,'retail_st_size':0,
                       'retail_identity':[originals[0].name,lo,0,0,0],
                       'reconstructed_object_size':size,'cc1_storage':definition,'registered_span':spans[0],
                       'comparison_extent_from_accepted_storage':True,'original_size_remains_unspecified':True,
                       'no_new_source_or_storage_credit':True,'all_section_storage_retained':True})
    return body,proofs


def registered_initialized_extents(body, reference, original, compiled, assembly, preprocessed, registry, tu, cwd):
    """Split invented storage buckets using previously accepted C owners.

    This reconstructs comparison identities, never original named identities.
    Complete ordinary compiler emission and accepted affine owner placement
    establish capacity; the enclosing normal link proves all relocated values.
    No section bytes, extents or real original symbols are changed.
    """
    typed=typed_compiler_data(assembly);proofs=[];eligible=set()
    for section in ('.data','.rodata','.sdata'):
        try:compiler_only_section(assembly,preprocessed,cwd,section)
        except (RuntimeError,ValueError):continue
        eligible.add(section)
    pattern=re.compile(r'dlabel\s+(D_[\da-fA-F]{8})\s*\n(.*?)enddlabel\s+\1\s*\n\.size\s+\1,\s*\.\s*-\s*\1',re.S)
    def replace(m):
        name=m[1];targets=[s for s in reference.symbols if s.name==name and s.type==1 and 0<s.shndx<len(reference.sections)]
        if len(targets)!=1:return m[0]
        target=targets[0];section=reference.sections[target.shndx];mapped=tu.get('sections',{}).get(section.name)
        if section.name not in eligible or not mapped:return m[0]
        start=int(mapped['start'],16);lo=start+target.value;hi=lo+target.size
        if lo!=int(name[2:],16) or hi>int(mapped['end'],16):return m[0]
        retail=original.section(section.name)
        if any(s.name and s.shndx==retail.index and s.value<hi and lo<s.value+max(1,s.size) for s in original.symbols):return m[0]
        owners=[]
        for symbol in compiled.symbols:
            if symbol.type!=1 or symbol.size<=0 or symbol.name not in typed or not 0<symbol.shndx<len(compiled.sections):continue
            csec=compiled.sections[symbol.shndx]
            if csec.name!=section.name or (typed[symbol.name]['section'],typed[symbol.name]['size'])!=(section.name,symbol.size):continue
            spans=[]
            for run in registry.get(section.name,[]):
                if symbol.name not in run.get('c_owned_symbols',[]):continue
                for span in run.get('c_input_spans',[]):
                    extent=explicit_object_range(span);coords=[int(x,16) for x in span.get('range',[])]
                    if (extent and len(coords)==2 and span.get('section')==section.name and symbol.name in span.get('symbols',[])
                        and not span.get('uncredited_dependency') and span.get('credit')!='uncredited_dependency'
                        and coords[1]-coords[0]==extent[1]-extent[0]
                        and extent[0]<=symbol.value and symbol.value+symbol.size<=extent[1]):
                        address=coords[0]+symbol.value-extent[0]
                        if lo<=address and address+symbol.size<=hi:spans.append((address,span))
            if len(spans)==1:owners.append((spans[0][0],symbol,spans[0][1]))
        owners.sort(key=lambda p:p[0])
        if not owners or owners[0][0]!=lo or owners[0][1].name!=name:return m[0]
        if len(owners)==1 and owners[0][1].size>=target.size:return m[0]
        raw=original.section_bytes(retail)[lo-retail.addr:hi-retail.addr]
        cursor=lo;gaps=[]
        for address,symbol,span in owners:
            if address<cursor:return m[0]
            if address>cursor:gaps.append((cursor,address))
            cursor=address+symbol.size
        if cursor<hi:gaps.append((cursor,hi))
        for a,b in gaps:
            if any(raw[a-lo:b-lo]) or original_references(original,a,b):return m[0]
        extra=[];directives=[]
        for address,symbol,span in owners[1:]:
            if any(s.name==symbol.name for s in reference.symbols):return m[0]
            directives.extend([f'.{"local" if symbol.bind==0 else "globl"} {symbol.name}',f'.type {symbol.name}, @object',
                f'.equ {symbol.name}, {name} + {address-lo}',f'.size {symbol.name}, {symbol.size}'])
            extra.append(dict(name=symbol.name,address=address,binding=symbol.bind,type=symbol.type,size=symbol.size,registered_span=span,cc1_storage=typed[symbol.name]))
        first=owners[0]
        proofs.append(dict(symbol=name,section=section.name,reconstructed_object_size=first[1].size,synthetic_bucket_size=target.size,
            registered_span=first[2],cc1_storage=typed[name],additional_registered_owners=extra,retained_unowned_zero_gaps=gaps,
            no_original_named_identity=True,compiler_only_section_verified=True,original_relocated_payload_verified_by_enclosing_normal_link=True))
        return 'dlabel '+name+'\n'+m[2]+f'.size {name}, {first[1].size}\n'+'\n'.join(directives)+'\n'
    return pattern.sub(replace,body),proofs


def registered_anonymous_storage(body, reference, original, compiled,
                                 assembly, preprocessed, registry, tu, cwd, scope=()):
    """Compatibility entry point with the same strict source/consumer proof."""
    return registered_source_pool_identities(body,reference,original,compiled,
        assembly,preprocessed,registry,tu,cwd,scope)


def source_literal_templates(preprocessed, compiled, assembly):
    """Decode complete primitive array initializers, including embedded NULs.

    These are source witnesses, not storage claims. A caller must independently
    bind their bytes to cc1 directives, an actual object and a registered span.
    """
    return list(typed_literal_source_objects(preprocessed,compiled,assembly,templates_only=True).values())


def cc1_literal_templates(assembly, preprocessed, compiled):
    """Pair anonymous cc1 literal directives with independently decoded C arrays."""
    import tu_audit
    parsed=tu_audit.AsmFile(assembly)
    rows={row['line']:row for row in parsed.top_data}
    sources=source_literal_templates(preprocessed,compiled,assembly);result=[];lines=assembly.splitlines()
    for index,line in enumerate(lines):
        label=re.fullmatch(r'\s*(\$LC\d+):\s*',line)
        if not label:continue
        payload=bytearray();used=[];section=None
        for number in range(index+2,len(lines)+1):
            if not lines[number-1].strip():continue
            row=rows.get(number)
            if not row or row['directive'] not in ('.byte','.half','.word','.ascii'):break
            if section is None:section=row['section']
            if row['section']!=section:break
            try:
                if row['directive']=='.ascii':fragment=c_literal_bytes(row['operands'])
                else:
                    width={'.byte':1,'.half':2,'.word':4}[row['directive']]
                    values=[literal_integer_expression(value) for value in row['operands'].split(',')]
                    if any(not -(1<<(width*8-1))<=value<1<<(width*8) for value in values):break
                    fragment=b''.join((value%(1<<(width*8))).to_bytes(width,'little') for value in values)
            except (ValueError,SyntaxError,UnicodeError):break
            payload.extend(fragment);used.append(row)
        if not payload:continue
        choices=[s for s in sources if s['payload']==bytes(payload)]
        # String expressions also materialize ordinary anonymous cc1 strings.
        if not choices:
            for match in source_matches(r'"(?:\\.|[^"\\])*"',preprocessed):
                try:decoded=c_literal_bytes(match[0])+b'\0'
                except (ValueError,SyntaxError,UnicodeError):continue
                if decoded==bytes(payload):
                    choices.append(dict(name=None,source_type='string literal',source_offset=match.start(),
                                        explicit_extent=len(payload),payload=bytes(payload)))
        if not choices:continue
        source=dict(choices[0],identical_source_offsets=[choice['source_offset'] for choice in choices])
        alignment=re.fullmatch(r'\s*\.align\s+(\d+)\s*',lines[index-1]) if index else None
        result.append(dict(label=label[1],section='.rodata' if section=='.rdata' else section,
                           payload=bytes(payload),directives=used,source=source,
                           alignment=1<<int(alignment[1]) if alignment and int(alignment[1])<=8 else None))
    return result


def compiler_data_coordinates(assembly, compiled, preprocessed='', unit_dir=None):
    """Read initialized cc1 directive coordinates and explicit ALIGN gaps.

    An entire section counter must agree with the real ELF before any of its
    labels or gaps can witness a reference identity. Unknown directives refuse.
    """
    import tu_audit
    from data_types import _record_split, RecordLayoutError
    parsed=tu_audit.AsmFile(assembly)
    rows={row['line']:row for row in parsed.top_data+[row for region in parsed.regions for row in region['data']]}
    counters={name:0 for name in ('.data','.sdata','.rodata','.lit4','.lit8')}
    labels={};gaps=[];invalid=set();section=None;previous=None;app=False
    try:pool_lines=compiler_pool_line_numbers(assembly,preprocessed,unit_dir) if preprocessed and unit_dir is not None else frozenset()
    except RuntimeError:return {},[]
    for number,line in enumerate(assembly.splitlines(),1):
        if re.match(r'\s*#APP\b',line):app=True;continue
        if re.match(r'\s*#NO_APP\b',line):app=False;continue
        if app and number not in pool_lines:continue
        selected=re.match(r'\s*\.(data|sdata|rdata|rodata|lit4|lit8|text)\s*$',line)
        explicit=re.match(r'\s*\.section\s+(\.[\w.]+)',line)
        if selected or explicit:
            previous,section=section,('.'+selected[1] if selected else explicit[1])
            if section=='.rdata':section='.rodata'
            continue
        if re.match(r'\s*\.previous\s*$',line):section,previous=previous,section;continue
        if section not in counters:continue
        align=re.fullmatch(r'\s*\.align\s+(\d+)\s*',line)
        if align:
            exponent=int(align[1])
            if exponent>8:invalid.add(section);continue
            before=counters[section];after=(before+(1<<exponent)-1)//(1<<exponent)*(1<<exponent)
            if after>before:gaps.append(dict(section=section,range=[before,after],alignment=1<<exponent,line=number))
            counters[section]=after;continue
        label=re.fullmatch(r'\s*([\w.$]+):\s*',line)
        if label:labels[label[1]]=(section,counters[section]);continue
        row=rows.get(number)
        if not row:continue
        try:
            directive=row['directive']
            if directive=='.ascii':size=len(c_literal_bytes(row['operands']))
            elif directive in ('.space','.zero'):size=literal_integer_expression(row['operands'])
            elif directive in ('.byte','.half','.short','.word','.float','.double','.dword','.quad'):
                width={'.byte':1,'.half':2,'.short':2,'.word':4,'.float':4,'.double':8,'.dword':8,'.quad':16}[directive]
                size=width*len(_record_split(row['operands'],','))
            else:invalid.add(section);continue
            if not 0<=size<=1048576:invalid.add(section);continue
            counters[section]+=size
        except (ValueError,SyntaxError,UnicodeError,RecordLayoutError):invalid.add(section)
    for name,count in counters.items():
        actual=[s for s in compiled.sections if s.name==name]
        if len(actual)!=1 or actual[0].size!=count:invalid.add(name)
    return ({name:value for name,value in labels.items() if value[0] not in invalid},
            [gap for gap in gaps if gap['section'] not in invalid])


def compiler_pool_consumers(original, compiled, offset, section_name, va, scope, preprocessed='', assembly=''):
    """Prove matching consumers in a closed, unmodified HI/LO basic-block slice.

    Calls, branches, delay slots and incoming interior edges cannot be crossed.
    Computed jumps require the actual bounded cc1 switch idiom and every real
    table relocation/original target; unsupported indirect control refuses.
    """
    text=compiled.section('.text');code=compiled.section_bytes(text);relocations={}
    for rel in compiled.sections:
        if rel.type!=9 or rel.info!=text.index or rel.entsize!=8 or rel.size%8:continue
        for at in range(rel.offset,rel.offset+rel.size,8):
            place,info=struct.unpack_from('<II',compiled.data,at)
            if info>>8>=len(compiled.symbols) or place in relocations:return []
            relocations[place]=(info&255,compiled.symbols[info>>8])
    claimed={row['original_symbol']:row for row in scope};proofs=[];controls={}
    def control(word):
        opcode=word>>26;function=word&63
        return (opcode in (1,2,3,4,5,6,7,20,21,22,23)
            or opcode==0 and function in (8,9,12,13,48,49,50,51,52,54)
            or opcode in (16,17,18) and (word>>21&31)==8
            or opcode==16 and (word>>21&31)==16)
    def nonlikely_branch(word):
        return (word>>26 in (4,5) or word>>26 in (6,7) and word>>16&31==0
                or word>>26==1 and word>>16&31 in (0,1))
    def may_write(word,gpr):
        opcode=word>>26;rt=word>>16&31;rd=word>>11&31;function=word&63
        if opcode==0:
            if function in (8,12,13,15,17,19):return False
            # R5900 MULT/MULTU optionally write rd as well as HI/LO.
            if function in (24,25):return rd==gpr
            if function in (26,27):return rd!=0
            if function in (0,2,3,4,6,7,9,10,11,16,18,20,22,23,32,33,34,35,36,37,38,39,42,43,44,45,46,47,56,58,59,60,62,63):return rd==gpr
            return True
        if control(word):return True
        if opcode in (40,41,42,43,44,45,46,47,49,53,57,61,63):return False
        if opcode in (8,9,10,11,12,13,14,15,24,25,26,27,32,33,34,35,36,37,38,39,48,52,55,56,60):return rt==gpr
        if opcode==17:
            operation=word>>21&31
            if operation in (0,2):return rt==gpr
            return operation not in (4,6,8,16,17,20,21)
        return True
    def typed_opaque_call(function,retail,at,rs,oword):
        """Bind the one evidenced runtime callback form to its actual C type."""
        if not preprocessed or not source_discovery_view(preprocessed).strip() or at-12<function.value:return None
        if sum(struct.unpack_from('<I',code,p)[0]&0xfc00003f==9 for p in range(function.value,function.value+function.size,4))!=1:return None
        import tu_edit
        try:source=tu_edit.TU(preprocessed)
        except tu_edit.ParseError:return None
        blocks=[b for b in source.blocks if b.state=='c' and b.name==function.name]
        if len(blocks)!=1:return None
        body=preprocessed[blocks[0].core_start:blocks[0].core_end];choices=[]
        for match in source_matches(r'\b(\w+)\s*\*\s*(\w+)\s*=\s*&\s*(\w+)\s*;',body):
            typename,alias,name=match.groups()
            types=list(source_matches(r'\btypedef\s+int\s*\(\s*\*\s*'+re.escape(typename)+r'\s*\)\s*\(\s*int\s*\)\s*;',preprocessed))
            calls=list(source_matches(r'\(\s*\*\s*'+re.escape(alias)+r'\s*\)\s*\(\s*command\s*\)\s*;',body))
            declarations=list(source_matches(r'\bstatic\s+'+re.escape(typename)+r'\s+'+re.escape(name)+r'\s*;',preprocessed))
            if len(types)!=1 or len(calls)!=1 or not declarations:continue
            owners=[s for s in compiled.symbols if s.name==name and 0<s.shndx<len(compiled.sections)]
            originals=[s for s in original.symbols if s.name==name and 0<s.shndx<len(original.sections)]
            if len(owners)!=1 or len(originals)!=1:continue
            owner=owners[0];old=originals[0];section=compiled.sections[owner.shndx]
            load=struct.unpack_from('<I',code,at-12)[0];branch=struct.unpack_from('<I',code,at-8)[0]
            load_delay=struct.unpack_from('<I',code,at-4)[0];call_delay=struct.unpack_from('<I',code,at+4)[0]
            if (load>>26!=35 or (load>>21&31,load>>16&31)!=(28,rs)
                or branch>>26!=4 or (branch>>21&31,branch>>16&31)!=(rs,0)
                or (branch&65535)!=4 or (owner.bind,owner.type,owner.size)!=(0,0,0)
                or (old.bind,old.type,old.size)!=(0,0,0) or section.name!='.sbss'
                or original.sections[old.shndx].name!='.sbss'
                or owner.value+4>section.size or control(load_delay) or may_write(load_delay,rs)
                or control(call_delay)):continue
            refs=[r for r in data_carve.section_references(compiled,section)
                  if r['offset']==at-12 and r['type']==7 and r['target']==owner.value]
            gps=[s.value for s in original.symbols if s.name=='_gp' and s.shndx!=0]
            old_load=oword(at-12)
            if (len(refs)!=1 or len(set(gps))!=1 or old_load&0xffff0000!=load&0xffff0000
                or gps[0]+(old_load&65535)-(65536 if old_load&32768 else 0)!=old.value):continue
            choices.append(dict(source_callback_type=types[0][0],source_address_alias=match[0],
                source_typed_call=calls[0][0],source_offset=blocks[0].core_start+calls[0].start(),
                actual_callback_storage=name,compiler_callback_offset=owner.value,
                original_callback_address=old.value,actual_GPREL_load=at-12,
                actual_nonnull_branch=at-8,targets_unknown=True,normal_return_not_assumed=True))
        return choices[0] if len(choices)==1 else None
    def fixed_header_print_call(function,retail,at,gpr):
        """Revision-bound normal-return certificate for two literal headers.

        This is deliberately not a general DB_println or ABI summary. The
        independent original-runtime inspection covers these exact eight-ASCII
        no-format headers, bounded vsprintf output and no frame-pointer escape.
        Ordinary valid-C buffer lifetime and ABI frame discipline are explicit
        premises; arbitrary formats, destinations and calls remain unsupported.
        """
        if (gpr!=16 or not preprocessed or (retail.name,retail.value,retail.size,retail.bind,retail.type)!=
            ('ACT_info',0x306090,1252,0,2) or at-8<function.value):return None
        site=retail.value+at-function.value
        headers={0x306300:(0x4d1760,b'     SCL\0'),0x30647c:(0x4d17c8,b'  CENTER\0')}
        if site not in headers:return None
        if hashlib.sha256(original.data).hexdigest()!='412d448de31505c03ad474cc63d500b3b4f8c48d49d32bd24c89fe5b21af0fb2':return None
        callees=[s for s in original.symbols if s.name=='DB_println' and s.shndx!=0]
        if len(callees)!=1:return None
        callee=callees[0]
        if (callee.value,callee.size,callee.bind,callee.type)!=(0x260938,172,1,2):return None
        cs=original.sections[callee.shndx];body=original.section_bytes(cs)[callee.value-cs.addr:callee.value-cs.addr+callee.size]
        if hashlib.sha256(body).hexdigest()!='1c1b7bfd4486b54949a8f435a06f8b048ef927ba170578b3249aac42706b413b':return None
        call=struct.unpack_from('<I',code,at)[0];rel=relocations.get(at)
        if (call!=0x0c000000 or not rel or rel[0]!=4 or rel[1].name!='DB_println' or rel[1].shndx!=0):return None
        os=original.sections[retail.shndx];obase=os.offset+retail.value-os.addr-function.value
        old_call=struct.unpack_from('<I',original.data,obase+at)[0]
        if (((site+4)&0xf0000000)|((old_call&0x3ffffff)<<2))!=callee.value or old_call>>26!=3:return None
        high,low=[struct.unpack_from('<I',code,p)[0] for p in (at-8,at-4)]
        old_high,old_low=[struct.unpack_from('<I',original.data,obase+p)[0] for p in (at-8,at-4)]
        if (high&0xffff0000,low&0xffff0000)!=(0x3c040000,0x24840000):return None
        if (old_high&0xffff0000,old_low&0xffff0000)!=(high&0xffff0000,low&0xffff0000):return None
        header,payload=headers[site]
        if ((old_high&65535)<<16)+(old_low&65535)-(65536 if old_low&32768 else 0)!=header:return None
        rh,rl=relocations.get(at-8),relocations.get(at-4)
        if not rh or not rl or (rh[0],rl[0],rh[1].index)!=(5,6,rl[1].index):return None
        symbol=rh[1]
        if not 0<symbol.shndx<len(compiled.sections):return None
        section=compiled.sections[symbol.shndx]
        offset=symbol.value+((high&65535)<<16)+(low&65535)-(65536 if low&32768 else 0)
        # Bind the complete literal declaration and actual raw OBJECT. This
        # certificate supplies no storage credit or claimed retail C name.
        definitions=list(source_matches(r'\b(?:static\s+)?(?:const\s+)?char\s+D_'+f'{header:08X}'+r'\s*\[\s*(?:9)?\s*\]\s*=\s*("(?:\\.|[^"\\])*")\s*;',preprocessed))
        if len(definitions)!=1:return None
        try:source_payload=c_literal_bytes(definitions[0][1])+b'\0'
        except (ValueError,SyntaxError,UnicodeError):return None
        owners=[s for s in compiled.symbols if s.name==f'D_{header:08X}' and s.shndx==section.index
                and s.value==offset and s.type==1 and s.size==9]
        if len(owners)!=1 or source_payload!=payload or compiled.section_bytes(section)[offset:offset+9]!=payload:return None
        if data_carve.va_bytes(original,header,9)!=payload:return None
        closure=[]
        for name in ('xglFontGetFlags','vsprintf','vfprintf','_vfprintf_r','_mbtowc_r','__sprint','__sfvwrite','memmove',
            '__sinit','std','localeconv','_localeconv_r','xglFontPrint','set_xyz','set_ot','xglFontPrintDirectCore'):
            symbols=[s for s in original.symbols if s.name==name and s.type==2 and s.shndx!=0]
            if len(symbols)!=1:return None
            s=symbols[0];sec=original.sections[s.shndx]
            raw=original.section_bytes(sec)[s.value-sec.addr:s.value-sec.addr+s.size]
            closure.append(dict(name=name,address=s.value,size=s.size,binding=s.bind,sha256=hashlib.sha256(raw).hexdigest()))
        return dict(kind='fixed-eight-ASCII-header-DB_println-normal-return',original_revision_sha256=hashlib.sha256(original.data).hexdigest(),
            caller_original_identity=[retail.name,retail.value,retail.size,retail.bind,retail.type],original_call_site=site,
            actual_compiler_R26_target='DB_println',callee_original_identity=['DB_println',callee.value,callee.size,1,2],
            callee_sha256=hashlib.sha256(body).hexdigest(),original_header_address=header,header_hex=payload.hex(),
            actual_source_header=definitions[0][0],compiler_header_offset=offset,source_raw_retail_header_exact=True,
            saved_s0_frame_offset=256,saved_ra_frame_offset=264,frame_size=368,
            formatter_output_at_most=9,formatter_buffer_size=256,header_has_no_format_conversion=True,
            preserved_GPRs_on_normal_return=[16],return_not_assumed=True,exception_paths_not_assumed_to_continue=True,
            valid_C_buffer_lifetime_and_original_ABI_frame_discipline_required=True,
            original_semantic_closure=closure,new_storage_or_credit=False)
    def fptodp_call_preservation(function,retail,at,gpr):
        """Exact four-function, pointer-free-entry floating conversion closure.

        None of its instructions writes s0/s2. The two nonleaf frames save RA
        at+32, while the only exposed local conversion objects occupy0..23.
        __unpack_f writes only its16B output; __pack_d never stores. Thus the
        actual closure returns with its saved linkage and preserves these GPRs
        on normal continuation, independently of the floating argument value.
        """
        if gpr not in (16,18):return None
        if hashlib.sha256(original.data).hexdigest()!='412d448de31505c03ad474cc63d500b3b4f8c48d49d32bd24c89fe5b21af0fb2':return None
        rel=relocations.get(at);word=struct.unpack_from('<I',code,at)[0]
        if not rel or rel[0]!=4 or rel[1].shndx!=0 or rel[1].name!='fptodp' or word!=0x0c000000:return None
        os=original.sections[retail.shndx];old=struct.unpack_from('<I',original.data,os.offset+retail.value-os.addr-function.value+at)[0]
        site=retail.value+at-function.value
        if old>>26!=3 or (((site+4)&0xf0000000)|((old&0x3ffffff)<<2))!=0x32bb88:return None
        expected=[('fptodp',0x32bb88,60,'1af414975b5126bf021fa57c767d449502c23c7c67418e25b02ad8627eb9ca67'),
            ('__unpack_f',0x32dbf8,92,'c2ed8f498eb76593ae6d3a5c65266f30df6f6130445b7deaab30af2a0b66cfe5'),
            ('__make_dp',0x32c4b8,44,'0d12c4f5e5618c6712aea39e067c0fe4db0bdd4383c95e68e1767c3524c57bb5'),
            ('__pack_d',0x32dc88,280,'7302732fbbbdb045483c7e172cfdde32bb49c418260dfa778960a101e54b98d0')]
        closure=[]
        for name,address,size,sha in expected:
            symbols=[s for s in original.symbols if s.name==name and s.shndx!=0]
            if len(symbols)!=1:return None
            s=symbols[0]
            if (s.value,s.size,s.bind,s.type)!=(address,size,1,2):return None
            sec=original.sections[s.shndx];raw=original.section_bytes(sec)[address-sec.addr:address-sec.addr+size]
            if hashlib.sha256(raw).hexdigest()!=sha:return None
            closure.append(dict(name=name,address=address,size=size,sha256=sha))
        return dict(kind='original-fptodp-four-function-normal-return',actual_compiler_R26_target='fptodp',
            original_call_site=site,preserved_GPRs_on_normal_return=[16,18],original_closure=closure,
            no_instruction_in_closure_writes_preserved_GPRs=True,direct_call_graph=[['fptodp','__unpack_f'],
                ['fptodp','__make_dp'],['__make_dp','__pack_d']],unknown_indirect_calls_absent=True,
            nonleaf_frame_size=48,saved_RA_offset=32,exposed_local_objects=[0,24],
            unpack_output_size=16,pack_has_no_stores=True,entry_argument_float_f12_not_a_pointer=True,
            return_not_assumed=True,valid_C_object_and_original_ABI_frame_discipline_required=True,
            new_storage_or_credit=False)
    def fixed_font_packet_call(function,retail,at,gpr):
        """Three revision/site-bound font calls, conditional normal return only.

        These are the two bounded menu-label tables and the fixed XYZ scalar
        packet. This certificate is not an arbitrary-format or generic ABI
        summary. Actual retail frame/packet/callee inspection establishes the
        preservation fact under ordinary valid-C output-buffer lifetime and
        original ABI frame discipline; unknown control never gains an entry.
        """
        if gpr not in (16,18) or not preprocessed or not assembly:return None
        expected_revision='412d448de31505c03ad474cc63d500b3b4f8c48d49d32bd24c89fe5b21af0fb2'
        if hashlib.sha256(original.data).hexdigest()!=expected_revision:return None
        sites={('PauseMenuPage2',0x250da0,1744,0,2):(0x648,'a9c4d42e7cbbf85fe8edf06fa43a6861f61b49c432f4df0d32d2e1e294f20063',0x367a50,7),
            ('PauseMenuPage3',0x251470,2476,0,2):(0xdd4,'3616b9bf3ea79a09d40d5da954237723c171b0c6457eaffbf5bc80d262ec6929',0x367a70,10),
            ('On_Ladder',0x2cf380,4108,1,2):(0xfa4,'e558576e400ab4c8768922306ba5d7eac6f8b7c551d95d623ff9e76d38535477',0x4cacd8,0)}
        identity=(retail.name,retail.value,retail.size,retail.bind,retail.type)
        spec=sites.get(identity)
        if not spec or at!=spec[0] and not (identity[0]=='On_Ladder' and at==0xf58):return None
        rel=relocations.get(at);call=struct.unpack_from('<I',code,at)[0]
        if not rel or rel[0]!=4 or rel[1].shndx!=0 or rel[1].name!='xglFontDebugPrintf' or call!=0x0c000000:return None
        os=original.sections[retail.shndx];obase=os.offset+retail.value-os.addr-function.value
        old=struct.unpack_from('<I',original.data,obase+at)[0];site=retail.value+at-function.value
        if old>>26!=3 or (((site+4)&0xf0000000)|((old&0x3ffffff)<<2))!=0x219298:return None
        import tu_edit
        try:source=tu_edit.TU(preprocessed)
        except tu_edit.ParseError:return None
        blocks=[b for b in source.blocks if b.state=='c' and b.name==function.name]
        if len(blocks)!=1:return None
        body=preprocessed[blocks[0].core_start:blocks[0].core_end]
        if hashlib.sha256(body.encode()).hexdigest()!=spec[1]:return None
        def materialized(high_at,low_at,register,address):
            high,low=[struct.unpack_from('<I',code,p)[0] for p in (high_at,low_at)]
            old_high,old_low=[struct.unpack_from('<I',original.data,obase+p)[0] for p in (high_at,low_at)]
            rh,rl=relocations.get(high_at),relocations.get(low_at)
            if (high>>26!=15 or high>>16&31!=register or low>>26!=9 or low>>21&31!=register
                or not rh or not rl or (rh[0],rl[0],rh[1].index)!=(5,6,rl[1].index)
                or not 0<rh[1].shndx<len(compiled.sections)
                or (high&0xffff0000,low&0xffff0000)!=(old_high&0xffff0000,old_low&0xffff0000)
                or ((old_high&65535)<<16)+(old_low&65535)-(65536 if old_low&32768 else 0)!=address):return None
            target=rh[1];where=target.value+((high&65535)<<16)+(low&65535)-(65536 if low&32768 else 0)
            return compiled.sections[target.shndx],where,dict(hi_offset=high_at,lo_offset=low_at,
                compiler_target_symbol=target.name,compiler_target_section=compiled.sections[target.shndx].name,
                compiler_target_offset=where,original_target_address=address)
        format_address,count=spec[2:];formats=[]
        if count:
            table_name='tbl_'+str(count==10 and 1 or 0)+'_'+f'{format_address:08X}'
            declarations=list(source_matches(r'\bstatic\s+const\s+char\s*\*\s*'+table_name+r'\s*\[\s*'+str(count)+r'\s*\]\s*=\s*\{([^{}]+)\}\s*;',preprocessed))
            if len(declarations)!=1:return None
            names=[name.strip() for name in declarations[0][1].split(',')]
            if len(names)!=count or any(not re.fullmatch(r'D_[0-9A-F]{8}',name) for name in names):return None
            owners=[s for s in compiled.symbols if s.name==table_name and s.bind==0 and s.type==1
                and s.size==4*count and 0<s.shndx<len(compiled.sections)]
            if len(owners)!=1:return None
            table_owner=owners[0];csection=compiled.sections[table_owner.shndx];cbytes=compiled.section_bytes(csection)
            emitted=typed_compiler_data(assembly).get(table_name)
            if emitted!={'section':csection.name,'size':4*count,'extent_kind':'typed-object'}:return None
            table_lines=list(re.finditer(r'^'+re.escape(table_name)+r':\s*\n((?:\s*\.word\s+[^\n]+\n)+)',assembly,re.M))
            if len(table_lines)!=1 or re.findall(r'\.word\s+(\S+)',table_lines[0][1])!=names:return None
            coordinates=materialized(0x600 if count==7 else 0xd8c,at-20,20,format_address)
            alternate=materialized(0x600 if count==7 else 0xd8c,at-44,20,format_address)
            if (not coordinates or coordinates[:2]!=(csection,table_owner.value)
                or not alternate or alternate[:2]!=(csection,table_owner.value)
                or struct.unpack_from('<I',code,at-20)[0]>>16&31!=2
                or struct.unpack_from('<I',code,at-4)[0]!=0x8c460000):return None
            fields={}
            for relsec in compiled.sections:
                if relsec.type!=9 or relsec.info!=csection.index or relsec.entsize!=8:continue
                for p in range(relsec.offset,relsec.offset+relsec.size,8):
                    place,info=struct.unpack_from('<II',compiled.data,p)
                    if info>>8>=len(compiled.symbols):return None
                    fields.setdefault(place,[]).append((info&255,compiled.symbols[info>>8]))
            table=data_carve.va_bytes(original,format_address,4*count)
            for i,name in enumerate(names):
                address=struct.unpack_from('<I',table,4*i)[0]
                if name!=f'D_{address:08X}':return None
                try:_,(_,tokens,payload)=data_carve._character_definition(preprocessed,name,allow_initializer_references=True)
                except (data_carve.CarveError,tu_edit.ParseError):return None
                if not 1<len(payload)<=19 or any(byte<32 or byte==37 for byte in payload[:-1]):return None
                if data_carve.va_bytes(original,address,len(payload))!=payload:return None
                owners=[s for s in compiled.symbols if s.name==name and s.type==1 and s.size==len(payload) and 0<s.shndx<len(compiled.sections)]
                if len(owners)!=1 or compiled.section_bytes(compiled.sections[owners[0].shndx])[owners[0].value:owners[0].value+len(payload)]!=payload:return None
                try:emitted_payload=data_carve._character_compiler_bytes(assembly,name,len(payload),compiled.sections[owners[0].shndx].name)
                except data_carve.CarveError:return None
                if emitted_payload!=payload:return None
                field=table_owner.value+4*i;rows=fields.get(field,[]);owner=owners[0]
                if (len(rows)!=1 or rows[0][0]!=2 or rows[0][1].shndx!=owner.shndx
                    or rows[0][1].value+struct.unpack_from('<I',cbytes,field)[0]!=owner.value):return None
                formats.append(dict(source_name=name,original_address=address,payload_hex=payload.hex(),
                    actual_compiler_table_R32_offset=field,actual_compiler_pointee_offset=owner.value,
                    percent_conversion_absent=True))
            argument_proof=dict(actual_table_object=[table_owner.name,table_owner.value,table_owner.size,0,1],
                actual_table_base_HI_LO=coordinates[2],actual_a2_table_load=at-4,
                alternate_branch_likely_table_base_HI_LO=alternate[2],
                table_index_from_source_i=True)
        else:
            numeric_header=at==0xf58
            payload=b'NUM = %3d,%3d,%8f\0' if numeric_header else b'XYZ = %4f,%4f,%4f\0'
            format_address=0x4cacc0 if numeric_header else 0x4cacd8
            if data_carve.va_bytes(original,format_address,len(payload))!=payload:return None
            pattern=(r'\bxglFontDebugPrintf\s*\(\s*4\s*,\s*2\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*ladder_id\s*,\s*direction\s*,\s*height\s*\)\s*;' if numeric_header else
                r'\bxglFontDebugPrintf\s*\(\s*4\s*,\s*10\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*move_x\s*,\s*move_y\s*,\s*move_z\s*\)\s*;')
            calls=list(source_matches(pattern,body))
            if len(calls)!=1 or c_literal_bytes(calls[0][1])+b'\0'!=payload:return None
            high_at,low_at,capture=(0xf34,0xf40,0xf44) if numeric_header else (0xf48,0xf5c,0xf84)
            coordinates=materialized(high_at,low_at,16,format_address)
            if not coordinates:return None
            section,where,argument_proof=coordinates
            if compiled.section_bytes(section)[where:where+len(payload)]!=payload:return None
            labels,_=compiler_data_coordinates(assembly,compiled)
            templates=[t for t in cc1_literal_templates(assembly,preprocessed,compiled)
                if t['payload']==payload and labels.get(t['label'])==(section.name,where)]
            if len(templates)!=1:return None
            argument_proof['actual_cc1_literal_label']=templates[0]['label']
            for relsec in compiled.sections:
                if relsec.type!=9 or relsec.info!=section.index or relsec.entsize!=8:continue
                for p in range(relsec.offset,relsec.offset+relsec.size,8):
                    target,info=struct.unpack_from('<II',compiled.data,p)
                    if where<=target<where+len(payload):return None
            if struct.unpack_from('<I',code,capture)[0]!=0x0200302d:return None
            argument_proof.update(actual_a2_capture=capture,actual_raw_literal_hex=payload.hex(),
                actual_typed_source_literal=calls[0][1])
            if numeric_header:
                conversion=fptodp_call_preservation(function,retail,0xf30,16)
                if not conversion:return None
                argument_proof['HI_delay_normal_return_call']=conversion
            else:
                first=fixed_font_packet_call(function,retail,0xf58,16)
                conversions=[fptodp_call_preservation(function,retail,p,16) for p in (0xf60,0xf6c,0xf78)]
                if not first or not all(conversions):return None
                argument_proof['actual_normal_return_preserved_calls']=[first,*conversions]
            formats.append(dict(original_address=format_address,payload_hex=payload.hex(),numeric_float_arguments=1 if numeric_header else 3,
                numeric_integer_arguments=2 if numeric_header else 0,
                meaningful_packet_bytes=[0,32],pointer_varargs_absent=True))
        expected=[('xglFontDebugPrintf',0x219298,220,'b74cfda305b80992216086fc66d4aefc963f968cb848a1b248eec749985a19d4'),
            ('xglFontPrintSub',0x218d70,652,'59057a14f2c5c4d39c016a96e5d9e633aa599aae6a838cc888bff37e502cf0d2'),
            ('set_xyz',0x218bc0,256,'93e21d14fb54cc1e6443f0b110ef93864054e7f60e7fd30b48fcff2b2a2d3cc1'),
            ('set_ot',0x218b60,92,'6dbf5d355cdfa1d7c75043686ec877ce8ddc9cdbe12be163adf19e72a1cc6f54'),
            ('dptofp',0x32c4e8,84,'ebb3b8d9fb237b0a8bec6c42bf958416d70989a5ce8bfe6b36abc33d5c83fd65'),
            ('__unpack_d',0x32dda0,172,'e8f1d3445b57cf5a224cb91d2c0fef7b368846095c97b3d2fe021f7cfb865c13'),
            ('__make_fp',0x32dc58,44,'d2ec47e3e5490b4d74dd892d42587a3217b5c034f0283252627a1c6acacba07b'),
            ('__pack_f',0x32df70,208,'ae91002ecdedf68d15cc677a7c314adfa90d56a2ebaa258fbd5836ac011d8b10')]
        closure=[]
        for name,address,size,sha in expected:
            symbols=[s for s in original.symbols if s.name==name and s.shndx!=0]
            if len(symbols)!=1:return None
            s=symbols[0]
            binding=0 if name in ('xglFontPrintSub','set_xyz','set_ot') else 1
            if (s.value,s.size,s.bind,s.type)!=(address,size,binding,2):return None
            sec=original.sections[s.shndx];raw=original.section_bytes(sec)[address-sec.addr:address-sec.addr+size]
            if hashlib.sha256(raw).hexdigest()!=sha:return None
            closure.append(dict(name=name,address=address,size=size,sha256=sha))
        return dict(kind='fixed-font-packet-normal-return',original_revision_sha256=expected_revision,
            caller_original_identity=list(identity),original_call_site=site,actual_compiler_R26_target='xglFontDebugPrintf',
            exact_source_function_body_sha256=spec[1],source_format_and_original_bytes=formats,
            actual_compiler_format_argument_proof=argument_proof,
            preserved_GPRs_on_normal_return=[16,18],return_not_assumed=True,exception_paths_not_assumed_to_continue=True,
            wrapper_frame_size=352,packet_bytes=[0,256],saved_s0_frame_offset=256,saved_ra_frame_offset=264,
            print_sub_frame_size=32,print_sub_saved_offsets=[0,8,16,24],packet_and_format_cursors_read_only=True,
            selected_formats_have_no_SPcode_path=True,float_path_emits_scalar_seven_byte_command=True,
            dptofp_frame_size=64,dptofp_saved_RA_offset=48,unpack_output_bytes=[0,24],
            make_fp_local_bytes=[0,16],make_fp_saved_RA_offset=16,pack_f_has_no_stores=True,
            packet_or_frame_pointer_escape_absent_in_actual_closure=True,
            valid_C_output_buffer_lifetime_and_original_ABI_frame_discipline_required=True,
            original_semantic_closure=closure,new_storage_or_credit=False)
    def function_control(function,retail):
        """Enumerate all potential interior entries, or decline the function."""
        key=function.index
        if key in controls:return controls[key]
        controls[key]=None
        start,end=function.value,function.value+function.size
        if start%4 or function.size%4 or end>len(code):return None
        osection=original.sections[retail.shndx]
        obase=osection.offset+retail.value-osection.addr-start
        word=lambda at:struct.unpack_from('<I',code,at)[0]
        oword=lambda at:struct.unpack_from('<I',original.data,obase+at)[0]
        # Both CFGs must carry the same actual instructions. Only relocation
        # fields may differ; the switch table targets are checked separately.
        for at in range(start,end,4):
            rel=relocations.get(at);kind=rel[0] if rel else None
            if rel and kind not in (4,5,6,7,8):return None
            mask=0xfc000000 if kind==4 else 0xffff0000 if rel else 0xffffffff
            if word(at)&mask!=oword(at)&mask:return None
        edges=[];dispatches=[];opaque_calls=[]
        for at in range(start,end,4):
            w=word(at);opcode=w>>26;rs=w>>21&31;rt=w>>16&31;fn=w&63
            if opcode in (4,5,6,7,20,21,22,23) or opcode==1 and rt in (0,1,2,3,16,17,18,19) or opcode in (16,17,18) and rs==8:
                target=at+4+((w&65535)-(65536 if w&32768 else 0))*4
                edges.append((at,target));continue
            if opcode in (2,3):
                rel=relocations.get(at)
                original_target=((retail.value+at-start+4)&0xf0000000)|((oword(at)&0x3ffffff)<<2)
                original_internal=retail.value<=original_target<retail.value+retail.size
                if rel:
                    if rel[0]!=4:return None
                    target=((w&0x3ffffff)<<2)+rel[1].value if rel[1].shndx==text.index else None
                else:
                    target=start+original_target-retail.value if original_internal else None
                internal=target is not None and start<=target<end
                if original_internal!=internal or internal and original_target!=retail.value+target-start:return None
                if internal:edges.append((at,target))
                continue
            if opcode==0 and fn==8 and rs==31:
                if w!=((31<<21)|8):return None
                continue
            if opcode==0 and fn==9:
                # Canonical JALR-ra is an opaque call, not a computed jump
                # dispatch. No callee target or return is asserted. A possible
                # return falls through after its delay slot with ALL GPRs
                # killed. The constructor checker below refuses every crossing
                # of this call and every HI/LOW in its delay slot.
                if not 0<rs<31 or w!=((rs<<21)|(31<<11)|9) or at+8>=end:return None
                typed=typed_opaque_call(function,retail,at,rs,oword)
                if not typed:return None
                opaque_calls.append(dict(offset=at,target_register=rs,possible_return=at+8,
                    all_GPRs_killed=True,callee_targets_unknown=True,return_not_assumed=True,actual_typed_C_call=typed))
                if at+8<end:edges.append((at,at+8))
                continue
            if opcode==0 and fn in (12,13,48,49,50,51,52,54):
                # An architectural exception resumes at its EPC or successor.
                # It remains a hard barrier for the constructor slice below;
                # it supplies no arbitrary computed intra-function entry.
                edges.append((at,at+4));continue
            if opcode==0 and fn==8:
                # Exact GCC bounded switch: sltiu; beq out; delay; sll;
                # lui; addu; lw; jr; nop. The complete real table bounds all
                # possible targets, including paths that re-enter this slice.
                if at-28<start or at+8>end or word(at+4)!=0 or not rs or w!=((rs<<21)|8):return None
                branch,delay,shift,hi,add,load=[word(at+d) for d in (-24,-20,-16,-12,-8,-4)]
                saved_index=None;branch_at=at-24
                # cc1 can schedule ordinary saved-register stores between the
                # actual bound test and branch. They change neither bound nor
                # indexed value. Require a unique test and only exact SP stores;
                # the whole-function entry guards still cover the wider range.
                bounds=[]
                for begin in range(max(start,at-44),at-24,4):
                    candidate=word(begin)
                    if candidate>>26!=11:continue
                    if any(word(p)>>26 not in (43,63) or word(p)>>21&31!=29
                           for p in range(begin+4,at-24,4)):continue
                    bounds.append((begin,candidate))
                if len(bounds)==1:
                    begin,bound=bounds[0]
                elif not bounds and at-48>=start and word(at-24)>>26==11 and word(at-20)>>26==20:
                    # The two actual menu dispatches precompute s0=s2*4
                    # before a fixed label call, then bounds-check the still
                    # preserved s2. On the dispatch path BEQL's increment slot
                    # is annulled. Both facts require this exact call proof.
                    begin=at-48;shift=word(begin);bound=word(at-24)
                    branch_at=at-20;branch=word(branch_at);delay=word(at-16)
                    proofs=[fixed_font_packet_call(function,retail,at-32,gpr) for gpr in (16,18)]
                    if not all(proofs):return None
                    if (shift!=0x00128080 or word(at-44)!=0x02021021
                        or word(at-40) not in (0x24040020,0x24040010)
                        or word(at-36)!=0x8c460000 or word(at-28)!=0x0220282d
                        or bound>>21&31!=18):return None
                    saved_index=dict(shift_offset=begin,bound_offset=at-24,
                        preserved_index_register=16,preserved_source_register=18,
                        actual_normal_return_call=proofs[0])
                else:return None
                n=bound&65535;source=bound>>21&31;test=bound>>16&31;index=shift>>11&31;base=hi>>16&31
                if (bound>>26!=11 or not 0<n<=1024 or not source or not test or source==test
                    or branch>>26 not in (4,20) or (branch>>21&31,branch>>16&31)!=(test,0)
                    or control(delay) or branch>>26==4 and may_write(delay,source)
                    or shift>>26 or shift&63 or shift>>21&31 or (shift>>16&31,shift>>6&31)!=(source,2) or not index
                    or hi>>26!=15 or not base
                    or add>>26 or add&63!=33 or add>>11&31!=base or {add>>21&31,add>>16&31}!={base,index}
                    or base==index or load>>26!=35 or (load>>21&31,load>>16&31)!=(base,rs)):
                    return None
                exit_target=branch_at+4+((branch&65535)-(65536 if branch&32768 else 0))*4
                if begin<=exit_target<=at+4:return None
                rh,rl=relocations.get(at-12),relocations.get(at-4)
                if not rh or not rl or (rh[0],rl[0],rh[1].index)!=(5,6,rl[1].index):return None
                symbol=rh[1]
                if not 0<symbol.shndx<len(compiled.sections):return None
                table=compiled.sections[symbol.shndx];raw=compiled.section_bytes(table)
                table_offset=((hi&65535)<<16)+(load&65535)-(65536 if load&32768 else 0)+symbol.value
                original_table=((oword(at-12)&65535)<<16)+(oword(at-4)&65535)-(65536 if oword(at-4)&32768 else 0)
                original_tables=[s for s in original.sections if s.flags&2 and s.type!=8 and s.addr<=original_table<original_table+4*n<=s.addr+s.size]
                if len(original_tables)!=1 or not 0<=table_offset<table_offset+4*n<=len(raw):return None
                table_rels={}
                for rel in compiled.sections:
                    if rel.type!=9 or rel.info!=table.index or rel.entsize!=8 or rel.size%8:continue
                    for p in range(rel.offset,rel.offset+rel.size,8):
                        place,info=struct.unpack_from('<II',compiled.data,p)
                        if info>>8>=len(compiled.symbols):return None
                        table_rels.setdefault(place,[]).append((info&255,compiled.symbols[info>>8]))
                targets=[];osection=original_tables[0]
                for i in range(n):
                    place=table_offset+4*i;rows=table_rels.get(place,[])
                    if len(rows)!=1 or rows[0][0]!=2 or rows[0][1].shndx!=text.index:return None
                    target=struct.unpack_from('<I',raw,place)[0]+rows[0][1].value
                    if not start<=target<end or target%4:return None
                    old=struct.unpack_from('<I',original.data,osection.offset+original_table-osection.addr+4*i)[0]
                    if old!=retail.value+target-start:return None
                    targets.append(target);edges.append((at,target))
                dispatches.append(dict(range=[begin,at+8],table_section=table.name,
                    compiler_table_offset=table_offset,original_table_address=original_table,
                    bounded_entry_count=n,actual_targets=targets,
                    exact_SP_stores_between_bound_and_branch=(at-28-begin)//4 if saved_index is None else 0,
                    precomputed_index_across_proved_normal_return_call=saved_index,
                    branch_likely_delay_annulled_on_dispatch_path=branch>>26==20))
                continue
            # Calls through registers, non-ABI returns, traps, coprocessor
            # transfers and unsupported REGIMM operations have unknown paths.
            if control(w):return None
        for dispatch in dispatches:
            begin,finish=dispatch['range']
            # A bound test in a preceding control transfer's delay slot does
            # not dominate the later dispatch. In particular a callee may
            # clobber both source and test, then return after the bound test.
            if begin>=4 and control(word(begin-4)):return None
            if retail.value+begin-start>=osection.addr+4 and control(oword(begin-4)):return None
            if any(begin<target<finish for _,target in edges):return None
        result=dict(incoming_edges=edges,proved_switch_dispatches=dispatches,
                    all_nonrelocated_function_instructions_exact=True,
                    unsupported_indirect_jumps_absent=True,opaque_calls=opaque_calls)
        controls[key]=result;return result
    for place,(kind,symbol) in relocations.items():
        if kind!=5 or not 0<symbol.shndx<len(compiled.sections):continue
        if compiled.sections[symbol.shndx].name!=section_name or place+4>len(code):continue
        high=struct.unpack_from('<I',code,place)[0]
        if high>>26!=15 or not high>>16&31:continue
        for lowplace in range(place+4,min(place+1028,len(code)),4):
            lowrel=relocations.get(lowplace)
            if not lowrel or (lowrel[0],lowrel[1].index)!=(6,symbol.index):continue
            low=struct.unpack_from('<I',code,lowplace)[0];imm=low&65535
            if low>>21&31!=high>>16&31 or low>>26 not in (8,9,*range(32,64)):continue
            if ((high&65535)<<16)+imm-(65536 if imm&32768 else 0)+symbol.value!=offset:continue
            # A direct JAL's unconditional delay slot executes before the
            # callee. Its sole pre-slot GPR write is $ra. This narrow form
            # permits cc1's actual argument construction; all other control
            # transfers and every instruction after a call delay still refuse.
            call_delay_low=(lowplace>=place+8 and struct.unpack_from('<I',code,lowplace-4)[0]>>26==3
                            and high>>16&31!=31)
            preceding=struct.unpack_from('<I',code,lowplace-4)[0]
            branch_delay_low=lowplace>=place+8 and nonlikely_branch(preceding)
            taken_entry=None
            if place>=4:
                branch=struct.unpack_from('<I',code,place-4)[0]
                if nonlikely_branch(branch):
                    entry=place+((branch&65535)-(65536 if branch&32768 else 0))*4
                    if place+8<entry<=lowplace:
                        escape=struct.unpack_from('<I',code,entry-8)[0]
                        slot=struct.unpack_from('<I',code,entry-4)[0]
                        exit_target=entry-4+((escape&65535)-(65536 if escape&32768 else 0))*4
                        before_escape=struct.unpack_from('<I',code,entry-12)[0]
                        if (escape>>26==4 and escape>>16&1023==0 and not control(slot) and not control(before_escape)
                            and not place<exit_target<=lowplace):taken_entry=entry
            # The selected branch successor can be a separate basic block.
            # Its other lexical predecessor must end in an unconditional exit;
            # no fallthrough carrying a clobbered HI can reach this LOW.
            slice_start=taken_entry if taken_entry is not None else place+4
            owners=[s for s in compiled.symbols if s.type==2 and s.shndx==text.index
                    and s.name in claimed and s.value<=place<lowplace<s.value+s.size]
            if len(owners)!=1:continue
            function=owners[0];claim=claimed[function.name]
            retail=verification.original_symbol(original,function.name,claim['size'],claim['va'])
            if (function.bind,function.type,function.size)!=(retail.bind,retail.type,retail.size):continue
            preserved_calls=[];blocked=False
            for p in range(slice_start,lowplace,4):
                if (call_delay_low or branch_delay_low) and p==lowplace-4:continue
                instruction=struct.unpack_from('<I',code,p)[0]
                if instruction>>26==3:
                    certificate=(fptodp_call_preservation(function,retail,p,high>>16&31)
                        or fixed_font_packet_call(function,retail,p,high>>16&31))
                    if certificate:preserved_calls.append(certificate);continue
                if control(instruction) or may_write(instruction,high>>16&31):blocked=True;break
            if blocked:continue
            cfg=function_control(function,retail)
            if not cfg:continue
            high_preceding=struct.unpack_from('<I',code,place-4)[0] if place>function.value else 0
            call_delay_high=(fixed_header_print_call(function,retail,place-4,high>>16&31)
                or fptodp_call_preservation(function,retail,place-4,high>>16&31)) if high_preceding>>26==3 else None
            # A canonical nonlikely branch executes its HI delay slot on both
            # successors. Only that exact predecessor may enter the later
            # straight-line slice; all other interior entries remain closed.
            branch_delay_high=(place>function.value and nonlikely_branch(high_preceding)
                and (place<function.value+8 or not control(struct.unpack_from('<I',code,place-8)[0])))
            if (control(high_preceding) and not branch_delay_high and not call_delay_high
                or any(place<target<=lowplace and not (branch_delay_high and origin==place-4)
                       for origin,target in cfg['incoming_edges'])):continue
            section=original.sections[retail.shndx];base=section.offset+retail.value-section.addr-function.value
            oh,ol=struct.unpack_from('<I',original.data,base+place)[0],struct.unpack_from('<I',original.data,base+lowplace)[0]
            if (high&0xffff0000,low&0xffff0000)!=(oh&0xffff0000,ol&0xffff0000):continue
            original_va=((oh&65535)<<16)+(ol&65535)-(65536 if ol&32768 else 0)
            if va is not None and original_va!=va:continue
            proofs.append(dict(function=function.name,hi_offset=place,lo_offset=lowplace,
                               compiler_pool_offset=offset,original_pool_address=original_va,
                               hi_register_unmodified_until_low=True,
                               closed_straight_line_slice=True,
                               direct_JAL_delay_slot_low=call_delay_low,
                               nonlikely_conditional_branch_delay_slot_low=branch_delay_low,
                               hi_is_not_delay_slot=not branch_delay_high and not call_delay_high,
                               nonlikely_conditional_branch_delay_slot_hi=branch_delay_high,
                               selected_taken_successor=taken_entry,
                               unselected_lexical_path_has_unconditional_exit=taken_entry is not None,
                               normal_return_call_delay_slot_hi=call_delay_high,
                               conditional_normal_return_preserved_calls=preserved_calls,
                               no_unproved_incoming_interior_edge=True,
                               function_control_proof=cfg))
    return proofs


def registered_pointer_pool_templates(original, compiled, assembly, preprocessed, registry, scope, cwd):
    """Prove anonymous char-pointer arrays using source and actual relocations."""
    import tu_audit
    from data_types import _record_split, RecordLayoutError
    chars=numeric_source_objects(preprocessed,compiled,assembly);sources=[];initializers=[]
    coordinates,_=compiler_data_coordinates(assembly,compiled,preprocessed,cwd)
    declaration=re.compile(r'\b(?:const\s+)?char\s*\*\s*(?:const\s+)?(\w+)\s*\[\s*(\d*)\s*\]\s*=\s*\{([^{}]*)\}\s*;',re.S)
    for match in source_matches(declaration, preprocessed):
        try:items=_record_split(match[3],',')
        except RecordLayoutError:continue
        if not 0<len(items)==(int(match[2]) if match[2] else len(items))<=1024:continue
        initializers.append(dict(name=match[1],source_offset=match.start(),items=items,
                                 source_layout='complete char-pointer array'))
    records=re.compile(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}\s*(\w+)\s*\[\s*(\d+)\s*\]\s*=\s*\{',re.S)
    for match in source_matches(records, preprocessed):
        fields=[field.strip() for field in match[2].split(';') if field.strip()]
        if not fields or any(not re.fullmatch(r'(?:const\s+)?char\s*\*\s*(?:const\s+)?\w+',field) for field in fields):continue
        count=int(match[4]);at=match.end();end=at;depth=1;quote=False;escape=False
        while end<len(preprocessed) and depth:
            char=preprocessed[end]
            if quote:
                if escape:escape=False
                elif char=='\\':escape=True
                elif char=='"':quote=False
            elif char=='"':quote=True
            elif char=='{':depth+=1
            elif char=='}':depth-=1
            end+=1
        if depth or not re.match(r'\s*;',preprocessed[end:]) or not 0<count*len(fields)<=1024:continue
        try:
            rows=_record_split(preprocessed[at:end-1],',');items=[]
            if len(rows)!=count:continue
            for row in rows:
                if not row.strip().startswith('{') or not row.strip().endswith('}'):break
                values=_record_split(row.strip()[1:-1],',')
                if len(values)!=len(fields):break
                items.extend(values)
            else:
                initializers.append(dict(name=match[3],source_offset=match.start(),items=items,
                    source_layout=dict(record_name=match[1],char_pointer_fields=fields,record_count=count,
                                       pointer_width=4,record_size=4*len(fields),no_internal_padding=True)))
        except RecordLayoutError:continue
    for initializer in initializers:
        items=initializer['items']
        decoded=[]
        for item in items:
            item=item.strip()
            if item=='0':decoded.append(dict(kind='null'));continue
            token=re.fullmatch(r'(\w+)(?:\s*\+\s*(0x[\da-fA-F]+|\d+))?',item)
            if token:
                source=chars.get(token[1]);addend=int(token[2],0) if token[2] else 0
                if not source and not addend and re.fullmatch(r'D_[\da-fA-F]{8}',token[1]):
                    declarations=source_findall(r'\bextern\s+(?:const\s+)?(?:signed\s+|unsigned\s+)?char\s+'
                        +re.escape(token[1])+r'\s*\[\s*\]\s*;',preprocessed)
                    if declarations:
                        decoded.append(dict(kind='foreign_char',name=token[1],addend=0,
                            registered_scaffold_declaration=declarations[0],no_pointee_storage_credit=True));continue
                if (not source or source.get('integer_offsets') is not None
                    or source['source_type'] not in ('char','signed char','unsigned char','char array')
                    or not 0<=addend<len(source['raw'])):break
                decoded.append(dict(kind='named_char',name=token[1],symbol=source['symbol'],addend=addend,
                                    payload=source['raw'][addend:]));continue
            # Parenthesized literal macros retain precisely the same pointer
            # initializer. Admit only balanced outer parentheses around one
            # adjacent-literal sequence, never casts or other expressions.
            literal_sequence=re.fullmatch(r'(\(*\s*)((?:"(?:\\.|[^"\\])*"\s*)+)(\)*)',item)
            if (not literal_sequence
                or literal_sequence[1].count('(')!=len(literal_sequence[3])):break
            literals=re.findall(r'"(?:\\.|[^"\\])*"',literal_sequence[2])
            try:payload=b''.join(c_literal_bytes(token) for token in literals)+b'\0'
            except (ValueError,SyntaxError,UnicodeError):break
            decoded.append(dict(kind='string_literal',payload=payload))
        else:
            sources.append(dict(name=initializer['name'],source_offset=initializer['source_offset'],items=decoded,
                                source_layout=initializer['source_layout']))
    def affine(section,offset):
        values=set()
        for output,runs in registry.items():
            for run in runs:
                for span in run.get('c_input_spans',[]):
                    ext=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                    if (ext and len(coords)==2 and span.get('section')==section
                        and not span.get('uncredited_dependency') and span.get('credit')!='uncredited_dependency'
                        and ext[1]-ext[0]==coords[1]-coords[0] and ext[0]<=offset<ext[1]):
                        values.add(coords[0]+offset-ext[0])
        return next(iter(values)) if len(values)==1 else None
    relocs={}
    for rel in compiled.sections:
        if rel.type!=9 or rel.entsize!=8 or rel.size%8:continue
        for at in range(rel.offset,rel.offset+rel.size,8):
            offset,info=struct.unpack_from('<II',compiled.data,at)
            if info>>8<len(compiled.symbols):relocs.setdefault((rel.info,offset),[]).append((info&255,compiled.symbols[info>>8]))
    literal_labels={row['label']:row['payload'] for row in cc1_literal_templates(assembly,preprocessed,compiled)}
    rows={row['line']:row for row in tu_audit.AsmFile(assembly).top_data};lines=assembly.splitlines();result=[]
    for index,line in enumerate(lines):
        label=re.fullmatch(r'\s*(\$LC\d+):\s*',line)
        if not label:continue
        directives=[]
        for number in range(index+2,len(lines)+1):
            row=rows.get(number)
            if not row or row['directive']!='.word':break
            directives.append(row)
        if not directives:continue
        name=directives[0]['section'];name='.rodata' if name=='.rdata' else name
        try:compiler_only_section(assembly,preprocessed,cwd,name)
        except (RuntimeError,ValueError):continue
        section=compiled.section(name);raw=compiled.section_bytes(section);retail=original.section(name)
        choices=[];size=len(directives)*4
        for source in sources:
            if len(source['items'])!=len(directives):continue
            if any(item['kind']=='null' and row['operands'].strip()!='0'
                   or item['kind'] in ('named_char','foreign_char') and re.sub(r'\s+|\+','',row['operands'])!=item['name']+(str(item['addend']) if item['addend'] else '')
                   or item['kind']=='string_literal' and literal_labels.get(row['operands'].strip())!=item['payload']
                   for item,row in zip(source['items'],directives)):continue
            for run in registry.get(name,[]):
                for span in run.get('c_input_spans',[]):
                    ext=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                    if (not ext or len(coords)!=2 or span.get('section')!=name
                        or not (span.get('anonymous_emission') is True and span.get('credit')=='verified' or span.get('support_only') is True)
                        or span.get('symbols') or span.get('uncredited_dependency')
                        or ext[1]-ext[0]!=coords[1]-coords[0] or ext[1]>section.size):continue
                    for offset in range(ext[0],ext[1]-size+1,4):
                        if coordinates.get(label[1])!=(name,offset):continue
                        va=coords[0]+offset-ext[0];values=[];witnesses=[]
                        for i,item in enumerate(source['items']):
                            place=offset+i*4;word=struct.unpack_from('<I',raw,place)[0];rel=relocs.get((section.index,place),[])
                            if item['kind']=='null':
                                if word or rel:break
                                values.append(0);continue
                            if len(rel)!=1 or rel[0][0]!=2:break
                            symbol=rel[0][1]
                            if item['kind']=='foreign_char':
                                pointer=int(item['name'][2:],16)
                                if (symbol.shndx!=0 or symbol.name!=item['name'] or word
                                    or not any(s.flags&2 and s.addr<=pointer<s.addr+s.size for s in original.sections)):break
                                values.append(pointer);witnesses.append(dict(index=i,compiler_relocation_offset=place,
                                    original_pointer=pointer,source_kind='foreign_char',source_char_name=item['name'],
                                    registered_scaffold_declaration=item['registered_scaffold_declaration'],
                                    exact_actual_R_MIPS32_symbol=True,no_pointee_storage_credit=True));continue
                            if not 0<symbol.shndx<len(compiled.sections):break
                            dest=compiled.sections[symbol.shndx];at=symbol.value+word;pointer=affine(dest.name,at)
                            try:compiler_only_section(assembly,preprocessed,cwd,dest.name)
                            except (RuntimeError,ValueError):break
                            if pointer is None or at+len(item['payload'])>dest.size:break
                            if compiled.section_bytes(dest)[at:at+len(item['payload'])]!=item['payload']:break
                            if item['kind']=='named_char':
                                owner=item['symbol']
                                if (dest.index,at)!=(owner.shndx,owner.value+item['addend']):break
                            original_dest=original.section(dest.name)
                            if original.section_bytes(original_dest)[pointer-original_dest.addr:pointer-original_dest.addr+len(item['payload'])]!=item['payload']:break
                            values.append(pointer);witnesses.append(dict(index=i,compiler_relocation_offset=place,
                                compiler_destination_section=dest.name,compiler_destination_offset=at,original_pointer=pointer,
                                source_kind=item['kind'],source_char_name=item.get('name'),payload_sha256=hashlib.sha256(item['payload']).hexdigest()))
                        else:
                            payload=b''.join(value.to_bytes(4,'little') for value in values)
                            if original.section_bytes(retail)[va-retail.addr:va-retail.addr+size]!=payload:continue
                            consumers=compiler_pool_consumers(original,compiled,offset,name,va,scope,preprocessed,assembly)
                            if consumers:choices.append((source,va,offset,span,consumers,payload,witnesses))
        if len(choices)!=1:continue
        source,va,offset,span,consumers,payload,witnesses=choices[0]
        alignment=re.fullmatch(r'\s*\.align\s+(\d+)\s*',lines[index-1]) if index else None
        template=dict(label=label[1],section=name,payload=payload,directives=directives,
            source=dict(name=source['name'],source_type='complete char-pointer array',source_offset=source['source_offset'],
                        explicit_extent=size,source_layout=source['source_layout'],actual_R_MIPS32_proofs=witnesses),
            alignment=1<<int(alignment[1]) if alignment and int(alignment[1])<=8 else None)
        result.append((template,va,offset,span,consumers))
    return result


def registered_assembler_double_pools(original, compiled, assembly, preprocessed, registry, scope, cwd):
    """Bind standard li.d assembler pools to actual C double literals.

    A unique verified anonymous input span, exact source/cc1 binary64 value,
    actual LD consumer and retail HI/LO address establish each eight-byte pool.
    This does not invent a C or retail object for an assembler temporary.
    """
    import math
    import tu_edit
    import tu_audit
    if not re.search(r'^\s*li\.d\s+',assembly,re.M):return []
    try:compiler_only_section(assembly,preprocessed,cwd,'.rodata')
    except (RuntimeError,ValueError):return []
    try:source=tu_edit.TU(preprocessed)
    except tu_edit.ParseError:return []
    values={};view=source_discovery_view(preprocessed)
    number=r'(?<![\w.])(?:\d+\.\d*|\.\d+)(?:[eE][-+]?\d+)?(?![\w.])'
    for block in source.blocks:
        if block.state!='c':continue
        bits=[]
        for token in re.compile(number).finditer(view,block.core_start,block.core_end):
            try:
                value=float(preprocessed[token.start():token.end()])
                if math.isfinite(value):bits.append(struct.pack('<d',value))
            except (ValueError,OverflowError,struct.error):continue
        values[block.name]=Counter(bits)
    authored=actual_source_asm_lines(preprocessed);pseudos={};function=None;app=False
    pattern=re.compile(r'\s*li\.d\s+\$(\d+),\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)\s*(?:#.*)?$')
    for line_number,line in enumerate(assembly.splitlines(),1):
        if line.strip()=='#APP':app=True;continue
        if line.strip()=='#NO_APP':app=False;continue
        if app:continue
        ent=re.fullmatch(r'\s*\.ent\s+(\S+)\s*',line)
        end=re.fullmatch(r'\s*\.end\s+(\S+)\s*',line)
        if ent:function=ent[1];continue
        if end:function=None;continue
        match=pattern.fullmatch(line)
        if not match or not function or not 0<int(match[1])<31:continue
        if tu_audit._norm_asm_line(tu_audit._strip_comment(line)) in authored:continue
        try:
            value=float(match[2])
            if not math.isfinite(value):continue
            payload=struct.pack('<d',value)
        except (ValueError,OverflowError,struct.error):continue
        pseudos.setdefault((function,payload),[]).append(dict(line=line_number,destination=int(match[1]),spelling=match[2]))
    result=[];section=compiled.section('.rodata');retail=original.section('.rodata');raw=compiled.section_bytes(section)
    refs=data_carve.section_references(compiled,section);text=compiled.section('.text');code=compiled.section_bytes(text)
    for run in registry.get('.rodata',[]):
        for span in run.get('c_input_spans',[]):
            extent=explicit_object_range(span);coords=span.get('range',[])
            if (not extent or len(coords)!=2 or extent[1]-extent[0]!=8 or span.get('section')!='.rodata'
                or span.get('symbols') or span.get('anonymous_emission') is not True or span.get('credit')!='verified'
                or span.get('uncredited_dependency')):continue
            va,hi=map(lambda v:int(v,16),coords);offset=extent[0]
            if hi-va!=8 or not 0<=offset<offset+8<=len(raw) or not retail.addr<=va<hi<=retail.addr+retail.size:continue
            payload=raw[offset:offset+8]
            if original.section_bytes(retail)[va-retail.addr:hi-retail.addr]!=payload:continue
            if any(s.shndx==section.index and s.type not in (3,4)
                   and (offset<=s.value<offset+8 or s.size>0 and s.value<offset+8 and offset<s.value+s.size)
                   for s in compiled.symbols):continue
            if any(rel.type in (4,9) and rel.info==section.index and any(offset<struct.unpack_from('<I',compiled.data,p)[0]+4
                   and struct.unpack_from('<I',compiled.data,p)[0]<offset+8 for p in range(rel.offset,rel.offset+rel.size,rel.entsize or 8))
                   for rel in compiled.sections):continue
            consumers=compiler_pool_consumers(original,compiled,offset,'.rodata',va,scope,preprocessed,assembly);choices=[]
            for consumer in consumers:
                name=consumer['function'];pseudo=pseudos.get((name,payload),[])
                if not pseudo or values.get(name,Counter())[payload]!=len(pseudo):continue
                load=struct.unpack_from('<I',code,consumer['lo_offset'])[0]
                high=struct.unpack_from('<I',code,consumer['hi_offset'])[0]
                if (load>>26!=55 or high>>16&31!=1
                    or load>>16&31 not in {p['destination'] for p in pseudo}):continue
                actual=[r for r in refs if r['target']==offset and r['offset'] in (consumer['hi_offset'],consumer['lo_offset'])]
                if sorted((r['offset'],r['type']) for r in actual)!=[(consumer['hi_offset'],5),(consumer['lo_offset'],6)]:continue
                choices.append((consumer,pseudo))
            if not choices:continue
            template=dict(label='assembler_li_d_'+str(offset),section='.rodata',payload=payload,directives=[],
                source=dict(name=None,source_type='double literal via standard li.d',explicit_extent=8,
                    literal_binary64=payload.hex(),function=choices[0][0]['function'],
                    actual_cc1_li_d=choices[0][1],actual_LD_HI_LO_pairs=[p[0] for p in choices],
                    source_literal_and_cc1_value_exact=True,no_named_C_or_original_object_claim=True))
            result.append((template,va,offset,span,[p[0] for p in choices]))
    return result


def registered_source_pool_identities(body, reference, original, compiled, assembly,
                                      preprocessed, registry, tu, cwd, scope):
    """Remove only invented identities over source-proved anonymous C pools.

    Registered affine placement, independent source/cc1 payloads and original
    instruction consumers establish every pool. All bytes, zero gaps, section
    extents and genuine original identities remain in the reference.
    """
    templates=cc1_literal_templates(assembly,preprocessed,compiled);located=[];proofs=[]
    coordinates,alignment_gaps=compiler_data_coordinates(assembly,compiled,preprocessed,cwd)
    pointer_pools=registered_pointer_pool_templates(original,compiled,assembly,preprocessed,registry,scope,cwd)
    for template in templates:
        name=template['section'];payload=template['payload']
        if name not in ('.rodata','.sdata'):continue
        try:compiler_only_section(assembly,preprocessed,cwd,name)
        except (RuntimeError,ValueError):continue
        section=compiled.section(name);retail=original.section(name);raw=compiled.section_bytes(section)
        candidates=[]
        for run in registry.get(name,[]):
            for span in run.get('c_input_spans',[]):
                extent=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                if (not extent or len(coords)!=2 or span.get('section')!=name
                    or not (span.get('anonymous_emission') is True and span.get('credit')=='verified' or span.get('support_only') is True)
                    or span.get('symbols') or span.get('uncredited_dependency')
                    or coords[1]-coords[0]!=extent[1]-extent[0] or extent[1]>section.size):continue
                for at in range(extent[0],extent[1]-len(payload)+1):
                    if coordinates.get(template['label'])!=(name,at):continue
                    if raw[at:at+len(payload)]!=payload:continue
                    va=coords[0]+at-extent[0]
                    if original.section_bytes(retail)[va-retail.addr:va-retail.addr+len(payload)]!=payload:continue
                    if any(s.type==1 and s.shndx==section.index and s.value<at+len(payload)
                           and at<s.value+max(1,s.size) for s in compiled.symbols):continue
                    if any(rel.type in (4,9) and rel.info==section.index and any(at<=struct.unpack_from('<I',compiled.data,p)[0]<at+len(payload)
                           for p in range(rel.offset,rel.offset+rel.size,rel.entsize or (12 if rel.type==4 else 8))) for rel in compiled.sections):continue
                    consumers=compiler_pool_consumers(original,compiled,at,name,va,scope,preprocessed,assembly)
                    consumers.extend(dict(pointer_template=t['label'],original_table_address=table_va,actual_C_R_MIPS32=proof)
                        for t,table_va,off,s,used in pointer_pools for proof in t['source'].get('actual_R_MIPS32_proofs',[])
                        if proof.get('original_pointer')==va and proof.get('compiler_destination_section')==name
                        and proof.get('compiler_destination_offset')==at)
                    if consumers:candidates.append((va,at,span,consumers))
        if len(candidates)==1:located.append((template,*candidates[0]))
        elif not candidates and template['source'].get('source_type')!='string literal':
            # A function-local C template need not be a recovered storage run.
            # Its original coordinate must come from an actual closed original
            # address constructor, never from a byte search or label spelling.
            position=coordinates.get(template['label'])
            if not position or position[0]!=name:continue
            at=position[1]
            if raw[at:at+len(payload)]!=payload:continue
            explicit_claim=False
            for run in registry.get(name,[]):
                for span in run.get('c_input_spans',[]):
                    if span.get('section')!=name:continue
                    extent=explicit_object_range(span)
                    if not extent or extent[0]<at+len(payload) and at<extent[1]:explicit_claim=True
            if explicit_claim:continue
            if len(template['source'].get('identical_source_offsets',[]))!=1:continue
            if any(s.type==1 and s.shndx==section.index and s.value<at+len(payload)
                   and at<s.value+max(1,s.size) for s in compiled.symbols):continue
            if any(rel.type in (4,9) and rel.info==section.index and any(at<=struct.unpack_from('<I',compiled.data,p)[0]<at+len(payload)
                   for p in range(rel.offset,rel.offset+rel.size,rel.entsize or (12 if rel.type==4 else 8))) for rel in compiled.sections):continue
            consumers=compiler_pool_consumers(original,compiled,at,name,None,scope,preprocessed,assembly)
            addresses={p['original_pool_address'] for p in consumers}
            if len(addresses)!=1:continue
            va=next(iter(addresses));mapped=tu.get('sections',{}).get(name)
            if not mapped:continue
            start=int(mapped['start'],16)
            items=[s for s in reference.symbols if s.type==1 and 0<s.shndx<len(reference.sections)
                and reference.sections[s.shndx].name==name and start+s.value==va
                and len(payload)<=s.size<=len(payload)+15]
            if len(items)!=1:continue
            item=items[0];hi=va+item.size
            if any(s.shndx==retail.index and s.type not in (3,4) and
                   (va<=s.value<hi or s.size>0 and s.value<hi and va<s.value+s.size)
                   for s in original.symbols):continue
            # Historical registrations name generated templates without object
            # coordinates. Admit that distinct witness only when the actual
            # source name and original constructor function are registered.
            # A present (even empty/malformed) modern span list never falls back.
            overlaps=[r for r in registry.get(name,[]) if int(r['range'][0],16)<hi and va<int(r['range'][1],16)]
            if not overlaps:continue
            legacy=None
            if overlaps:
                if len(overlaps)!=1:continue
                run=overlaps[0];generated=run.get('generated',[])
                if ('c_input_spans' in run or template['source'].get('name') not in run.get('c_owned_symbols',[])
                    or len(generated)!=2):continue
                generated=list(map(lambda value:int(value,16),generated))
                if not int(run['range'][0],16)<=generated[0]<=va<va+len(payload)<=generated[1]<=int(run['range'][1],16):continue
                consumers=[p for p in consumers if p['function'] in run.get('generated_by',[])]
                if not consumers:continue
                legacy=dict(range=run['range'],generated=run['generated'],
                    c_owned_source_name=template['source']['name'],generated_by=run['generated_by'],
                    no_modern_object_coordinate_claim=True)
            if not retail.addr<=va<va+len(payload)<=hi<=retail.addr+retail.size:continue
            if original.section_bytes(retail)[va-retail.addr:va-retail.addr+len(payload)]!=payload:continue
            witness=dict(kind='legacy-registered-template-and-closed-consumer',registered_run=True,
                original_item=[va,hi],compiler_payload=[at,at+len(payload)],
                original_item_symbol=item.name,original_item_symbol_size=item.size,
                reference_sha256=hashlib.sha256(reference.data).hexdigest(),
                explicit_object_coordinates_from_registry=False,legacy_source_registration=legacy,
                no_source_storage_credit=True)
            located.append((template,va,at,witness,consumers))
    located.extend(pointer_pools)
    located.extend(registered_assembler_double_pools(original,compiled,assembly,preprocessed,registry,scope,cwd))
    pattern=re.compile(r'(?:nonmatching\s+(\S+)(?:\s*,[^\n]*)?\s*\n\s*)?dlabel\s+(D_[\da-fA-F]{8})\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*[^\n]*',re.S)
    replacements={}
    def replace(match):
        name=match[2];symbols=[s for s in reference.symbols if s.name==name and s.type==1 and 0<s.shndx<len(reference.sections)]
        if len(symbols)!=1:return match[0]
        target=symbols[0];section=reference.sections[target.shndx];mapped=tu.get('sections',{}).get(section.name)
        if not mapped:return match[0]
        lo=int(mapped['start'],16)+target.value;hi=lo+target.size
        if lo!=int(name[2:],16):return match[0]
        retail=original.section(section.name)
        if any(s.name and s.shndx==retail.index and s.value<hi and lo<s.value+max(1,s.size) for s in original.symbols):return match[0]
        pools=[row for row in located if row[0]['section']==section.name and lo<=row[1]<row[1]+len(row[0]['payload'])<=hi]
        pools.sort(key=lambda row:row[1])
        if not pools or pools[0][1]!=lo:return match[0]
        cursor=lo;gaps=[]
        for template,va,offset,span,consumers in pools:
            if va<cursor:return match[0]
            if cursor<va:gaps.append((cursor,va))
            cursor=va+len(template['payload'])
        if cursor<hi:gaps.append((cursor,hi))
        raw=original.section_bytes(retail)[lo-retail.addr:hi-retail.addr]
        if any(not 0<b-a<=15 or any(raw[a-lo:b-lo]) or original_references(original,a,b) for a,b in gaps):return match[0]
        gap_proofs=[]
        for a,b in gaps:
            preceding=[row for row in pools if row[1]+len(row[0]['payload'])==a]
            if len(preceding)!=1:return match[0]
            previous=preceding[0];compiler_end=previous[2]+len(previous[0]['payload'])
            emitted=[gap for gap in alignment_gaps if gap['section']==section.name
                     and gap['range']==[compiler_end,compiler_end+b-a]]
            linker=[piece['linker_padding'] for piece in mapped.get('pieces',[]) if piece.get('linker_padding')
                    and [int(v,16) for v in piece['linker_padding'].get('range',[])]==[a,b]
                    and piece['linker_padding'].get('kind')=='unreferenced_original_alignment']
            if len(emitted)==1:gap_proofs.append(dict(original_range=[a,b],cc1_alignment=emitted[0]))
            elif len(linker)==1:gap_proofs.append(dict(original_range=[a,b],ordinary_linker_padding=linker[0]))
            elif b==hi:
                # A split original pool can precede a separately allocated C
                # owner even when cc1 inserted another pool between them.
                # Require that real next owner and its own emitted alignment.
                owners=[]
                typed=typed_compiler_data(assembly)
                for cname,source,owner_va,owner_span in registered_literal_source_spans(original,compiled,assembly,preprocessed,registry,cwd):
                    symbol=source['symbol'];emission=typed.get(cname)
                    if (owner_va==b and compiled.sections[symbol.shndx].name==section.name and emission
                        and owner_span.get('credit')=='verified'):
                        # Derive the next owner's actual preceding cc1 ALIGN.
                        label=re.search(r'^'+re.escape(cname)+r':\s*$',assembly,re.M)
                        prefix=assembly[:label.start()].splitlines()[-5:] if label else []
                        align=[int(m[1]) for line in prefix for m in [re.fullmatch(r'\s*\.align\s+(\d+)\s*',line)] if m]
                        if len(align)==1 and align[0]<=8 and (a+(1<<align[0])-1)//(1<<align[0])*(1<<align[0])==b:
                            owners.append(dict(name=cname,original_address=b,cc1_alignment=1<<align[0],registered_span=owner_span))
                if len(owners)==1:gap_proofs.append(dict(original_range=[a,b],following_registered_C_owner=owners[0]))
                elif not owners:
                    # A partial compiler template does not prove the original
                    # item's capacity or alignment. Keep this exact zero suffix
                    # as unknown original storage, outside every source claim.
                    named_spans=[span for run in registry.get(section.name,[]) for span in run.get('c_input_spans',[])
                        if span.get('symbols') and len(span.get('range',[]))==2
                        and int(span['range'][0],16)<b and a<int(span['range'][1],16)]
                    if named_spans:return match[0]
                    gap_proofs.append(dict(original_range=[a,b],kind='unknown_original_zero_suffix',
                        credited=False,credited_bytes=0,original_capacity_proved=False,alignment_proved=False,
                        existing_original_rejection_index_no_hit=True))
                else:return match[0]
            else:return match[0]
        temporary=f'.Lobjdiff_source_pool_{lo:08X}';replacements[name]=temporary
        proofs.append(dict(scaffold_name=name,temporary_reference_name=temporary,original_range=[lo,hi],
            pools=[dict(compiler_label=t['label'],compiler_offset=off,original_address=va,size=len(t['payload']),
                        payload_sha256=hashlib.sha256(t['payload']).hexdigest(),source={k:v for k,v in t['source'].items() if k!='payload'},
                        cc1_directives=t['directives'],registered_span=span if span.get('kind')!='legacy-registered-template-and-closed-consumer' else None,
                        original_span_witness=span if span.get('kind')=='legacy-registered-template-and-closed-consumer' else None,original_consumers=consumers)
                   for t,va,off,span,consumers in pools],retained_uncredited_zero_gaps=gaps,alignment_proofs=gap_proofs,
            no_original_named_identity=True,all_section_storage_retained=True,new_source_credit=False))
        return '.local '+name+'\n'+name+':\n'+match[3]
    rewritten=pattern.sub(replace,body)
    def rename(text):
        for old,new in replacements.items():text=re.sub(r'(?<![\w.$])'+re.escape(old)+r'(?![\w.$])',lambda _:new,text)
        return text
    def include(match):
        path=Path(match[1]);path=path if path.is_absolute() else cwd/path
        old=path.read_text();new=rename(old)
        return new if new!=old else match[0]
    rewritten=rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,rewritten))
    return rewritten,proofs


def registered_literal_source_spans(original, compiled, assembly, preprocessed, registry, cwd):
    """Bind complete decoded named literal owners to exact accepted placements."""
    decoded=numeric_source_objects(preprocessed,compiled,assembly);result=[]
    for name,proof in decoded.items():
        if proof.get('integer_offsets') is not None:continue
        symbol=proof['symbol'];section=compiled.sections[symbol.shndx]
        if section.name not in ('.data','.sdata','.rodata') or len(proof['raw'])!=symbol.size:continue
        try:compiler_only_section(assembly,preprocessed,cwd,section.name)
        except (RuntimeError,ValueError):continue
        spans=[]
        for run in registry.get(section.name,[]):
            if name not in run.get('c_owned_symbols',[]):continue
            for span in run.get('c_input_spans',[]):
                extent=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                if (not extent or len(coords)!=2 or span.get('section')!=section.name
                    or name not in span.get('symbols',[]) or span.get('uncredited_dependency')
                    or span.get('credit')=='uncredited_dependency' or coords[1]-coords[0]!=extent[1]-extent[0]
                    or not extent[0]<=symbol.value<symbol.value+symbol.size<=extent[1]):continue
                va=coords[0]+symbol.value-extent[0];retail=original.section(section.name)
                if not retail.addr<=va<va+symbol.size<=retail.addr+retail.size:continue
                if original.section_bytes(retail)[va-retail.addr:va-retail.addr+symbol.size]!=proof['raw']:continue
                spans.append((va,span))
        if len(spans)==1:result.append((name,proof,spans[0][0],spans[0][1]))
    return result


def registered_named_literal_identities(body, reference, scaffold, original, compiled,
                                        assembly, preprocessed, registry, tu, cwd):
    """Retain actual named compiler storage without asserting a retail name.

    Older string/pool routes hid even complete named C owners. Restore their
    independently proved compiler identity over the same bytes; all genuine
    original symbols and any unowned tail remain untouched.
    """
    proofs=[]
    for name,source,va,span in registered_literal_source_spans(original,compiled,assembly,preprocessed,registry,cwd):
        symbol=source['symbol'];section=compiled.sections[symbol.shndx];retail=original.section(section.name)
        hi=va+symbol.size
        if any(s.name and s.shndx==retail.index and s.value<hi and va<s.value+max(1,s.size) for s in original.symbols):continue
        existing=[s for s in reference.symbols if s.name==name and 0<s.shndx<len(reference.sections)]
        if len(existing)>1:continue
        mapped=tu.get('sections',{}).get(section.name)
        if not mapped:continue
        targets=[s for s in scaffold.symbols if s.name==name and s.type==1 and 0<s.shndx<len(scaffold.sections)
                 and scaffold.sections[s.shndx].name==section.name and s.value+int(mapped['start'],16)==va
                 and s.size>=symbol.size]
        if len(targets)!=1:continue
        bucket=targets[0]
        if bucket.size>symbol.size:
            tail=original.section_bytes(retail)[hi-retail.addr:va+bucket.size-retail.addr]
            if any(tail) or original_references(original,hi,va+bucket.size):continue
        if existing:
            current=existing[0]
            if reference.sections[current.shndx].name!=section.name or current.value+int(mapped['start'],16)!=va:continue
            if (current.bind,current.type,current.size)==(symbol.bind,symbol.type,symbol.size):continue
            label=name
        else:
            labels=[label for label in (f'.Lobjdiff_string_{va:08X}',f'.Lobjdiff_registered_char_{va:08X}')
                    if re.search(r'^\s*'+re.escape(label)+r':\s*$',body,re.M)]
            if len(labels)!=1:continue
            label=labels[0]
        directives=[f'.{"local" if symbol.bind==0 else "globl"} {name}',f'.type {name}, @object']
        if label!=name:directives.append(f'.equ {name}, {label}')
        directives.append(f'.size {name}, {symbol.size}')
        # The contractual assembler retains its first size directive.
        body=re.sub(r'^\s*\.size\s+'+re.escape(name)+r'\s*,[^\n]*$', '',body,flags=re.M)
        body+='\n'+'\n'.join(directives)+'\n'
        proofs.append(dict(c_name=name,section=section.name,original_address=va,compiler_offset=symbol.value,
            compiler_identity=[symbol.bind,symbol.type,symbol.size],registered_span=span,
            source_kind=source['kind'],initializer_sha256=hashlib.sha256(source['raw']).hexdigest(),
            original_named_identity_not_asserted=True,retained_uncredited_tail=[hi,va+bucket.size],
            all_storage_and_genuine_original_identities_retained=True,new_source_credit=False))
    return body,proofs


def registered_literal_word_references(body, original, compiled, assembly, preprocessed, registry, tu, cwd, scope):
    """Keep numeric words numeric only inside independently decoded C owners."""
    zones=[dict(name=name,payload=proof['raw'],original_address=va,compiler_offset=proof['symbol'].value,
                section=compiled.sections[proof['symbol'].shndx],registered_span=span,source_kind=proof['kind'])
           for name,proof,va,span in registered_literal_source_spans(original,compiled,assembly,preprocessed,registry,cwd)]
    coordinates,alignment_gaps=compiler_data_coordinates(assembly,compiled,preprocessed,cwd)
    # A numeric scaffold word may straddle the end of an authenticated partial
    # character owner. Prove its C bytes independently; the remaining zero
    # bytes stay original, unknown, and outside the C extent and credit.
    for zone in zones:
        payload=zone['payload'];va=zone['original_address'];end=va+len(payload)
        width=(-end)%4
        if not width:continue
        registrations=[run.get('typed_character_prefix_proof') for run in registry.get(zone['section'].name,[])
            if zone['name'] in run.get('c_owned_symbols',[])
            and run.get('typed_character_prefix_proof') is not None]
        if len(registrations)!=1 or not isinstance(registrations[0],dict):continue
        registration=registrations[0];bounds=registration.get('original_item_range')
        if (registration.get('schema')!='typed-character-prefix-proof/1'
            or registration.get('name')!=zone['name'] or registration.get('range')!=[va,end]
            or not isinstance(bounds,list) or len(bounds)!=2 or bounds[0]!=va or not end<end+width<=bounds[1]
            or registration.get('original_sha256')!=hashlib.sha256(original.data).hexdigest()
            or registration.get('payload_sha256')!=hashlib.sha256(payload).hexdigest()):continue
        source_path=Path(cwd).parent.parent/tu.get('path','')
        if (not source_path.is_file() or source_path.stat().st_size>1024*1024
            or hashlib.sha256(source_path.read_bytes()).hexdigest()!=registration.get('source_sha256')):continue
        try:
            _,(_,source_tokens,source_payload)=data_carve._character_definition(source_path.read_text(),zone['name'],True)
            _,(_,cpp_tokens,cpp_payload)=data_carve._character_definition(preprocessed,zone['name'],True)
            emitted=data_carve._character_compiler_bytes(assembly,zone['name'],len(payload),zone['section'].name)
        except (data_carve.CarveError,ValueError,IndexError):continue
        if source_tokens!=cpp_tokens or source_payload!=payload or cpp_payload!=payload or emitted!=payload:continue
        section=zone['section'];offset=zone['compiler_offset']+len(payload);retail=original.section(section.name)
        gaps=[gap for gap in alignment_gaps if gap['section']==section.name
              and gap['range'][0]==offset and offset+width<=gap['range'][1]]
        if len(gaps)!=1:continue
        tail=compiled.section_bytes(section)[offset:offset+width]
        if len(tail)!=width or any(tail) or original.section_bytes(retail)[end-retail.addr:end-retail.addr+width]!=tail:continue
        if any(s.shndx==section.index and s.type not in (3,4)
               and (offset<=s.value<offset+width or s.size>0 and s.value<offset+width and offset<s.value+s.size)
               for s in compiled.symbols):continue
        if any(s.shndx==retail.index and s.type not in (3,4)
               and (va<=s.value<end+width or s.size>0 and s.value<end+width and va<s.value+s.size)
               for s in original.symbols):continue
        if original_references(original,end,end+width):continue
        zone['payload']=payload+tail
        zone['partial_character_terminal_word']=dict(registration=registration,
            actual_source_sha256=registration['source_sha256'],C_extent=[va,end],
            retained_unknown_zero_suffix=[end,end+width],unknown_suffix_C_bytes=0,
            unknown_suffix_credit=0,original_capacity_proved=False,source_cpp_cc1_prefix_exact=True)
    pointer_pools=registered_pointer_pool_templates(original,compiled,assembly,preprocessed,registry,scope,cwd)
    for template in cc1_literal_templates(assembly,preprocessed,compiled):
        name=template['section'];position=coordinates.get(template['label'])
        if not position or position[0]!=name or name not in ('.data','.sdata','.rodata'):continue
        try:compiler_only_section(assembly,preprocessed,cwd,name)
        except (RuntimeError,ValueError):continue
        section=compiled.section(name);offset=position[1];payload=template['payload']
        if compiled.section_bytes(section)[offset:offset+len(payload)]!=payload:continue
        spans=[]
        for run in registry.get(name,[]):
            for span in run.get('c_input_spans',[]):
                ext=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                if (ext and len(coords)==2 and span.get('section')==name and not span.get('symbols')
                    and (span.get('anonymous_emission') is True and span.get('credit')=='verified' or span.get('support_only') is True)
                    and not span.get('uncredited_dependency') and ext[1]-ext[0]==coords[1]-coords[0]
                    and ext[0]<=offset<offset+len(payload)<=ext[1]):spans.append((coords[0]+offset-ext[0],span))
        if len(spans)!=1:continue
        va,span=spans[0];consumers=compiler_pool_consumers(original,compiled,offset,name,va,scope,preprocessed,assembly)
        consumers.extend(dict(pointer_template=t['label'],original_table_address=table_va,actual_C_R_MIPS32=proof)
            for t,table_va,off,s,used in pointer_pools for proof in t['source'].get('actual_R_MIPS32_proofs',[])
            if proof.get('original_pointer')==va and proof.get('compiler_destination_section')==name
            and proof.get('compiler_destination_offset')==offset)
        if not consumers:continue
        padding=[gap for gap in alignment_gaps if gap['section']==name and gap['range'][0]==offset+len(payload)]
        if len(padding)==1 and 0<padding[0]['range'][1]-padding[0]['range'][0]<=15:
            end=padding[0]['range'][1];tail=compiled.section_bytes(section)[offset+len(payload):end]
            if any(tail):continue
            payload+=tail
        retail=original.section(name)
        if original.section_bytes(retail)[va-retail.addr:va-retail.addr+len(payload)]!=payload:continue
        zones.append(dict(name=template['label'],payload=payload,original_address=va,compiler_offset=offset,
            section=section,registered_span=span,source_kind='source-proved-anonymous-literal',source=template['source'],
            original_consumers=consumers,cc1_alignment=padding[0] if len(padding)==1 else None))
    proofs=[]
    pattern=re.compile(r'^(\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+)'
                       r'([A-Za-z_.$][\w.$]*)(?:\s*\+\s*(0x[\da-fA-F]+|\d+))?\s*$',re.M)
    _,original_values,_=original_symbol_indexes(original.data)
    def replace(match):
        va=int(match[2],16);raw=bytes.fromhex(match[3]);value=int.from_bytes(raw,'little')
        name=match[4];addend=int(match[5],0) if match[5] else 0
        numeric=re.fullmatch(r'D_([\da-fA-F]+)',name)
        values={int(numeric[1],16)} if numeric else set(original_values.get(name,()))
        if len(values)!=1 or value!=next(iter(values))+addend:return match[0]
        candidates=[row for row in zones if row['original_address']<=va and va+4<=row['original_address']+len(row['payload'])
                    and row['payload'][va-row['original_address']:va-row['original_address']+4]==raw]
        if len(candidates)!=1:return match[0]
        source=candidates[0];name=source['name'];section=source['section'];span=source['registered_span']
        offset=source['compiler_offset']+va-source['original_address']
        if compiled.section_bytes(section)[offset:offset+4]!=raw:return match[0]
        if any(rel.type in (4,9) and rel.info==section.index and any(offset<struct.unpack_from('<I',compiled.data,p)[0]+4 and struct.unpack_from('<I',compiled.data,p)[0]<offset+4
            for p in range(rel.offset,rel.offset+rel.size,rel.entsize or (12 if rel.type==4 else 8))) for rel in compiled.sections):return match[0]
        if any(rel.type in (4,9) and any(va<struct.unpack_from('<I',original.data,p)[0]+4 and struct.unpack_from('<I',original.data,p)[0]<va+4
            for p in range(rel.offset,rel.offset+rel.size,rel.entsize or (12 if rel.type==4 else 8))) for rel in original.sections):return match[0]
        proofs.append(dict(original_word_address=va,compiler_offset=offset,value=value,c_owner=name,
            registered_span=span,source_kind=source['source_kind'],full_source_initializer_decoded=True,
            actual_compiler_relocation_absent=True,original_bytes_retained=True,
            original_consumers=source.get('original_consumers'),cc1_alignment=source.get('cc1_alignment'),
            partial_character_terminal_word=source.get('partial_character_terminal_word'),
            no_alignment_storage_credit=True))
        return match[1]+f'0x{value:08X}'
    return pattern.sub(replace,body),proofs


def registered_nobits_zero_sizes(body, reference, original, compiled, assembly, registry, tu, identities):
    """Retain genuine NOTYPE/SIZE0 identities, keeping C extents as proof only."""
    storage=emitted_storage(assembly);proofs=[]
    for identity in identities:
        if (identity['binding'],identity['type'],identity['size'])!=(0,0,0):continue
        name=identity['target_name'];va=identity['address']
        targets=[s for s in reference.symbols if s.name==name and (s.bind,s.type)==(0,0)
                 and s.size>0 and 0<s.shndx<len(reference.sections)]
        symbols=[s for s in compiled.symbols if s.name==name and (s.bind,s.type,s.size)==(0,0,0)
                 and 0<s.shndx<len(compiled.sections)]
        definition=storage.get(name)
        if len(targets)!=1 or len(symbols)!=1 or not definition or definition.get('extent_kind')!='label-space':continue
        target,symbol=targets[0],symbols[0];section=compiled.sections[symbol.shndx]
        if section.type!=8 or reference.sections[target.shndx].name!=section.name or definition['section']!=section.name:continue
        if target.size!=definition['size']:continue
        # This route concerns a sub-alignment declaration whose unspecified
        # ELF size makes native comparison include alignment. Larger accepted
        # storage annotations and partial allocations are not reinterpreted.
        if not 0<definition['size']<section.align:continue
        cnext=min([s.value for s in compiled.symbols if s.name and s.shndx==symbol.shndx
                   and s.type in (0,1) and s.value>symbol.value]+[section.size])
        rsection=reference.sections[target.shndx]
        rnext=min([s.value for s in reference.symbols if s.name and s.shndx==target.shndx
                   and s.type in (0,1) and s.value>target.value]+[rsection.size])
        native_extent=(definition['size']+section.align-1)//section.align*section.align
        if cnext-symbol.value!=native_extent or rnext-target.value!=native_extent:continue
        next_symbols=[s for s in compiled.symbols if s.name and s.shndx==symbol.shndx and s.value==cnext
                      and s.name in storage and storage[s.name].get('alignment')==section.align]
        if len(next_symbols)!=1:continue
        if original_references(original,va+definition['size'],va+native_extent):continue
        originals=[s for s in original.symbols if s.name==identity['original_name'] and s.value==va
                   and (s.bind,s.type,s.size)==(0,0,0) and original.sections[s.shndx].name==section.name]
        if len(originals)!=1:continue
        spans=[]
        for run in registry.get(section.name,[]):
            if name not in run.get('c_owned_symbols',[]):continue
            for span in run.get('c_input_spans',[]):
                ext=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                if (ext and len(coords)==2 and span.get('section')==section.name and name in span.get('symbols',[])
                    and not span.get('uncredited_dependency') and span.get('credit')!='uncredited_dependency'
                    and ext[1]-ext[0]==coords[1]-coords[0] and ext[0]<=symbol.value
                    and symbol.value+definition['size']<=ext[1] and coords[0]+symbol.value-ext[0]==va):spans.append(span)
        if len(spans)!=1:continue
        sizes=re.compile(r'^\s*\.size\s+'+re.escape(name)+r'\s*,[^\n]*$',re.M)
        if len(sizes.findall(body))!=1:continue
        body=sizes.sub('.size '+name+', 0',body)
        proofs.append(dict(target_name=name,original_name=identity['original_name'],original_address=va,
            original_identity=[0,0,0],actual_compiler_identity=[0,0,0],source_storage=definition,
            registered_span=spans[0],source_extent_remains_separate_from_symbol_size=True,
            native_inferred_comparison_extent=native_extent,uncredited_alignment_bytes=native_extent-definition['size'],
            next_cc1_storage_alignment=storage[next_symbols[0].name],
            all_section_storage_retained=True,new_storage_or_padding_credit=False))
    return body,proofs




def registered_current_data_layout(root, unit_dir, obj, tu, base, assembly, preprocessed,
                                   evidence, destination, prefix, reference=None):
    """Ordinary LD placement of verified C inputs, never imported DATA bytes.

    The compiler object stays immutable. This comparison view uses the same
    selected input sections and genuine ALIGN operations as the verified link.
    It retains the complete original TU footprint, including zero-input fill.
    """
    checked=evidence['objects'][obj['id']]
    if obj['unit']!='main' or not checked.get('complete'):return None
    manifest=verification.read(unit_dir/'tu-manifest.carved.json')
    current=next(t for t in manifest['tus'] if t['id']==obj['id'])
    verification.require(tu['name']==current['name'] and tu['text']==current['text'] and all(
        all(entry[k]==current['sections'][name][k] for k in ('start','end'))
        for name,entry in tu.get('sections',{}).items()),'current DATA view TU coordinates differ from verified manifest')
    route=current.get('c_link_object')
    if not route or not route.endswith('.carved.o'):return None
    compiled=parsed_elf(base.read_bytes())
    # This route is needed only when the original comparison still invents
    # OBJECT identities over registered anonymous compiler literal emissions.
    # It is independent of scores, a TU name or a requested matching result.
    if reference is not None:
        candidate=parsed_elf(reference.read_bytes())
        original=parsed_elf((unit_dir/'orig'/verification.read(root/cov.ORIGINALS)['units']['main']['file']).read_bytes())
        found=False
        registry=data_carve.load_registry(root)['tus'].get(obj['id'],{})
        for secname,runs in registry.items():
            entry=tu.get('sections',{}).get(secname)
            if not entry:continue
            sections=[s for s in compiled.sections if s.name==secname and s.type==1 and s.flags&2]
            if len(sections)!=1:continue
            csection=sections[0]
            # The closed placement adapter handles an anonymous pool family
            # whose current compiler extent differs from its original physical
            # footprint. Named storage and already coordinate-identical views
            # keep their existing proof routes.
            if csection.size==int(entry['end'],16)-int(entry['start'],16) or any(
                    s.type==1 and s.shndx==csection.index for s in compiled.symbols):continue
            start=int(entry['start'],16)
            spans=[span for r in runs for span in r.get('c_input_spans',[]) if
                   span.get('anonymous_emission') is True and span.get('credit')=='verified' and
                   not span.get('symbols') and not any(span.get(k) for k in ('support_only','uncredited_dependency'))]
            for symbol in candidate.symbols:
                if not (symbol.type==1 and symbol.name.startswith('D_') and 0<symbol.shndx<len(candidate.sections)
                        and candidate.sections[symbol.shndx].name==secname):continue
                va=start+symbol.value
                if not any(int(s['range'][0],16)<=va<int(s['range'][1],16) for s in spans):continue
                retail=original.section(secname)
                if any(s.shndx==retail.index and s.type not in (3,4) and
                       (va<=s.value<va+symbol.size or s.size and s.value<va+symbol.size and va<s.value+s.size)
                       for s in original.symbols):continue
                found=True
        if not found:return None
    raw_path,_=verification.build_object(root,root/'build',obj)
    selected_path=unit_dir/route
    original_path=unit_dir/'orig'/verification.read(root/cov.ORIGINALS)['units']['main']['file']
    artifact_paths=(raw_path,selected_path,unit_dir/'build/main.rom.elf.map',
                    unit_dir/'build/main.rom.elf',unit_dir/'tu-manifest.carved.json',original_path)
    source_paths=(root/obj['source'],root/'config/objects/data-carves.json',root/'config/objects/main.objects.json')
    pins={}
    for paths,authority in ((artifact_paths,evidence['artifacts']),(source_paths,evidence['inputs']['files'])):
        for path in paths:
            actual=verification.digest(path)
            verification.require(authority.get(path.relative_to(root).as_posix())==actual,
                                 'current DATA view lacks fresh verified authority/input')
            pins[str(path)]=actual
    registry_path=data_carve.registry_input_path(root)
    verification.require(verification.digest(registry_path)==pins[str(root/'config/objects/data-carves.json')],
                         'ordinary selected registry differs from tracked verified DATA authority')
    pins[str(registry_path)]=verification.digest(registry_path)
    compiled=parsed_elf(base.read_bytes());raw=parsed_elf(raw_path.read_bytes());selected=parsed_elf(selected_path.read_bytes())
    def signature(image):
        rows=[]
        for sec in image.sections:
            if not sec.flags&2 or sec.name=='.reginfo':continue
            rows.append((sec.name,sec.type,sec.flags,sec.align,sec.size,image.section_bytes(sec)))
        rel=[]
        for section in image.sections:
            if section.type not in (4,9):continue
            for off,kind,index,addend in data_carve._rel_entries(image,section):
                s=image.symbols[index]
                rel.append((image.sections[section.info].name,off,kind,addend,s.name,s.bind,s.type,s.value,s.size,
                            image.sections[s.shndx].name if 0<s.shndx<len(image.sections) else s.shndx))
        identities=sorted((s.name,s.bind,s.type,s.size,s.value,s.other,
            image.sections[s.shndx].name if 0<s.shndx<len(image.sections) else s.shndx)
            for s in image.symbols if s.type in (1,2) and s.shndx!=0)
        return sorted(rows),sorted(rel,key=repr),identities
    verification.require(signature(compiled)==signature(raw),'native C emission differs from verified ordinary input')
    verification.require(functions(base)=={f['name']:f['size'] for f in checked['functions']},
                         'current DATA view includes a function outside verified C scope')
    registry=data_carve.load_registry(root)
    plans=data_carve.split_plans(root,'main',unit_dir,obj['id'],raw,registry)
    plans={sec:(p['pieces'] if isinstance(p,dict) else p) for sec,p in plans.items()}
    verification.require(data_carve.split_object(raw.data,plans)==selected.data,
                         'current DATA input differs from complete ordinary split')
    map_path=unit_dir/'build/main.rom.elf.map';map_text=map_path.read_text()
    placed=verification.parse_link_map(map_text)[0]
    fills=[(int(a,16),int(a,16)+int(n,16)) for a,n in re.findall(
        r'^ \*fill\*\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\b',map_text,re.M)]
    original=parsed_elf((unit_dir/'orig'/verification.read(root/cov.ORIGINALS)['units']['main']['file']).read_bytes())
    linked=parsed_elf((unit_dir/'build/main.rom.elf').read_bytes())
    families=('.data','.rodata','.sdata','.lit4','.lit8')
    script=['SECTIONS {','.text 0 : { '+json.dumps(str(selected_path))+'(.text) }']
    layout={};transport=[]
    for family in families:
        mapped=tu.get('sections',{}).get(family)
        if not mapped:continue
        lo,hi=(int(mapped[k],16) for k in ('start','end'))
        if lo==hi:continue
        inputs=[]
        for sec in selected.sections:
            if not sec.size or not (sec.name==family or sec.name.startswith(family+'.carve.')):continue
            place=placed.get((route,sec.name))
            verification.require(place is not None and place[1]==sec.size and sec.type==1 and sec.flags&2,
                                 'current C selected section placement is missing or malformed')
            start,size=place
            verification.require(lo<=start<start+size<=hi and start%sec.align==0,
                                 'current C input lies outside its original TU footprint')
            inputs.append((start,start+size,sec))
        inputs.sort(key=lambda row:row[0])
        if not inputs:return None
        blocks=[];cursor=lo
        script.append(f'{family} 0 : {{')
        gaps={tuple(int(v,16) for v in p['linker_padding']['range']):p['linker_padding']
              for p in current['sections'].get(family,{}).get('pieces',[]) if p.get('linker_padding')}
        def gap(a,b):
            proof=gaps.get((a,b))
            verification.require(proof is not None and proof.get('credited_bytes')==0 and
                proof.get('grants_source_eligibility') is False and proof.get('original_capacity_proved') is False and
                proof.get('kind') in ('compiler_alignment_fill','external_input_alignment_fill') and
                (a,b) in fills and not any(size>0 and start<b and a<start+size for start,size in placed.values()),
                'DATA view gap is not exact proved zero-input linker fill')
            alignment=proof['alignment']
            verification.require((a+alignment-1)&-alignment==b and lo%alignment==0 and
                not any(data_carve.va_bytes(original,a,b-a)) and
                data_carve.va_bytes(linked,a,b-a)==data_carve.va_bytes(original,a,b-a),
                'DATA view ALIGN does not reproduce genuine original-coordinate fill')
            script.append(f'. = ALIGN({alignment});')
            blocks.append(dict(kind='linker_alignment',range=[a,b],proof=proof))
        for start,end,sec in inputs:
            verification.require(start>=cursor,'overlapping current C selected inputs')
            if cursor<start:gap(cursor,start)
            script.append(json.dumps(str(selected_path))+'('+sec.name+')')
            blocks.append(dict(kind='C_input',range=[start,end],section=sec.name,alignment=sec.align))
            cursor=end
        if cursor<hi:gap(cursor,hi)
        script.append('}')
        layout[family]=dict(range=[lo,hi],blocks=blocks)
        for registration in registry['tus'].get(obj['id'],{}).get(family,[]):
            for span in registration.get('c_input_spans',[]):
                ext=explicit_object_range(span);coords=[int(v,16) for v in span.get('range',[])]
                if ext and len(coords)==2 and span.get('section')==family and ext[1]-ext[0]==coords[1]-coords[0]:
                    transport.append(dict(section=family,raw=ext,original=coords,span=span))
    if not layout:return None
    # Nonempty unexplained allocated sections cannot slip through as orphans.
    verification.require(all(s.name=='.text' or s.name=='.reginfo' or s.name in families or
        any(s.name.startswith(f+'.carve.') for f in families) or not s.size or not s.flags&2
        for s in selected.sections),'unsupported allocated current DATA view section')
    # These are ELF/debug/ABI declaration records, never code or DATA input
    # storage. Keep .reginfo: its GP value participates in real GPREL semantics.
    # The ordinary data-placement link must not merge stale GP debug tables.
    script.append('/DISCARD/ : { *(.MIPS.abiflags) *(.pdr) *(.gnu.attributes) *(.mdebug*) *(.comment) *(.note*) *(.gptab.*) }')
    script.append('}')
    script_path=destination.with_suffix('.ld');script_path.write_text('\n'.join(script)+'\n')
    run([str(prefix)+'ld','-EL','-m','elf32lr5900','-r','-T',script_path,'-o',destination,selected_path],unit_dir)
    image=parsed_elf(destination.read_bytes())
    for family,row in layout.items():
        sec=image.section(family);lo,hi=row['range']
        verification.require(sec.size==hi-lo,'ordinary placement changed the original DATA denominator')
    verification.require(functions(destination)==functions(base),'ordinary DATA placement changed C function identities')
    # Resolve the ordinary comparison object at the actual original addresses.
    # All relocations, including DATA-to-DATA coordinates, must reproduce the
    # independently verified linked bytes. No unresolved-name address guessing.
    definitions=[f'_gp = 0x{original.gp:08X};'];placements=[]
    for symbol in image.symbols:
        if symbol.shndx!=0 or not symbol.name or symbol.name=='_gp':continue
        values={s.value for s in linked.symbols if s.name==symbol.name and s.shndx!=0}
        verification.require(len(values)==1,'current C placement has unresolved/ambiguous linked symbol')
        definitions.append(f'{symbol.name} = 0x{values.pop():08X};')
    for sec in image.sections:
        if not sec.flags&2 or not sec.size or sec.name=='.reginfo':continue
        start=int(tu['text']['start'],16) if sec.name=='.text' else layout[sec.name]['range'][0]
        placements.append(f'{sec.name} 0x{start:08X} : {{ *({sec.name}) }}')
    resolved_script=destination.with_suffix('.resolve.ld');resolved_path=destination.with_suffix('.resolve.elf')
    resolved_script.write_text('\n'.join(definitions)+'\nSECTIONS {\n'+'\n'.join(placements)+'\n}\n')
    run([str(prefix)+'ld','-EL','-m','elf32lr5900','-T',resolved_script,'-o',resolved_path,destination],unit_dir)
    resolved=parsed_elf(resolved_path.read_bytes());resolved_proofs=[]
    for sec in resolved.sections:
        if not sec.flags&2 or not sec.size or sec.type==8 or sec.name=='.reginfo':continue
        payload=resolved.section_bytes(sec)
        verification.require(payload==data_carve.va_bytes(original,sec.addr,sec.size)==data_carve.va_bytes(linked,sec.addr,sec.size),
                             'ordinary C placement resolved payload differs from original/verified link')
        resolved_proofs.append(dict(section=sec.name,range=[sec.addr,sec.addr+sec.size],sha256=hashlib.sha256(payload).hexdigest()))
    for path,digest in pins.items():
        verification.require(verification.digest(Path(path))==digest,'current DATA view authority/input changed')
    pins.update({str(p):verification.digest(p) for p in (base,assembly)})
    return dict(schema='native-current-registered-C-placement/1',layout=layout,transport=transport,
                compiler_sha256=hashlib.sha256(compiled.data).hexdigest(),original_sha256=hashlib.sha256(original.data).hexdigest(),
                input_pins=pins,ordinary_comparison_object=str(destination),resolved_payload_proofs=resolved_proofs,
                no_new_source_or_storage_credit=True)


def current_layout_reference(body, reference, original, compiled, assembly, preprocessed,
                             registry, tu, cwd, scope, placement):
    """Recover only invented reference metadata over decoded current C pools.

    Real original identities and every byte/extent remain. The alternative to
    the old straight-line consumer recognizer is the actual verified C input
    placement, whose ordinary full split and linked semantics were checked.
    """
    verification.require(placement['compiler_sha256']==hashlib.sha256(compiled.data).hexdigest() and
        placement['original_sha256']==hashlib.sha256(original.data).hexdigest(),
        'current reference view input identity differs')
    for path,digest in placement['input_pins'].items():
        verification.require(verification.digest(Path(path))==digest,'current reference placement input changed')
    coordinates,gaps=compiler_data_coordinates(assembly,compiled,preprocessed,cwd)
    zones=[]
    for template in cc1_literal_templates(assembly,preprocessed,compiled):
        name=template['section'];position=coordinates.get(template['label'])
        if name not in placement['layout'] or not position or position[0]!=name:continue
        if not compiler_only_section(assembly,preprocessed,cwd,name):continue
        offset=position[1];payload=template['payload'];sec=compiled.section(name)
        if compiled.section_bytes(sec)[offset:offset+len(payload)]!=payload:continue
        owners=[r for r in placement['transport'] if r['section']==name and
            r['span'].get('anonymous_emission') is True and r['span'].get('credit')=='verified' and
            not r['span'].get('symbols') and not any(r['span'].get(k) for k in ('support_only','uncredited_dependency')) and
            r['raw'][0]<=offset<offset+len(payload)<=r['raw'][1]]
        if len(owners)!=1:continue
        owner=owners[0];va=owner['original'][0]+offset-owner['raw'][0]
        if data_carve.va_bytes(original,va,len(payload))!=payload:continue
        if any(s.type==1 and s.shndx==sec.index and s.value<offset+len(payload) and offset<s.value+max(s.size,1)
               for s in compiled.symbols):continue
        if any(rel.type in (4,9) and rel.info==sec.index and any(offset<p+4 and p<offset+len(payload)
               for p,k,n,a in data_carve._rel_entries(compiled,rel)) for rel in compiled.sections):continue
        zones.append(dict(section=name,range=[va,va+len(payload)],raw=[offset,offset+len(payload)],
                          payload=payload,source=template['source'],span=owner['span']))
    # Some anonymous constructors are genuine pointer arrays or assembler
    # double pools. Their established source/real-REL proof remains mandatory.
    for template,va,offset,span,consumers in (registered_pointer_pool_templates(original,compiled,assembly,preprocessed,registry,scope,cwd)+
                                           registered_assembler_double_pools(original,compiled,assembly,preprocessed,registry,scope,cwd)):
        zones.append(dict(section=template['section'],range=[va,va+len(template['payload'])],
                          raw=[offset,offset+len(template['payload'])],payload=template['payload'],
                          source=template['source'],span=span,existing_consumers=consumers))
    identity_proofs=[];word_proofs=[];renames={}
    pattern=re.compile(r'(?:nonmatching\s+(\S+)(?:\s*,[^\n]*)?\s*\n\s*)?dlabel\s+(D_[\da-fA-F]{8})\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*[^\n]*',re.S)
    def replace(match):
        name=match[2];symbols=[s for s in reference.symbols if s.name==name and s.type==1 and 0<s.shndx<len(reference.sections)]
        if len(symbols)!=1:return match[0]
        symbol=symbols[0];family=reference.sections[symbol.shndx].name
        if family not in placement['layout']:return match[0]
        start=placement['layout'][family]['range'][0];lo=start+symbol.value;hi=lo+symbol.size
        if lo!=int(name[2:],16):return match[0]
        retail=original.section(family)
        if any(s.shndx==retail.index and s.type not in (3,4) and
               (lo<=s.value<hi or s.size and s.value<hi and lo<s.value+s.size) for s in original.symbols):return match[0]
        pieces=sorted((z for z in zones if z['section']==family and lo<=z['range'][0]<z['range'][1]<=hi),key=lambda z:z['range'])
        if not pieces or pieces[0]['range'][0]!=lo:return match[0]
        cursor=lo;proved_gaps=[]
        for zone in pieces:
            a,b=zone['range']
            if a!=cursor:return match[0]
            cursor=b
            following=[z['range'][0] for z in pieces if z['range'][0]>=cursor]
            end=min(following) if following else hi
            if end>cursor:
                raw_end=zone['raw'][1];amount=end-cursor
                events=[g for g in gaps if g['section']==family and g['range'][0]<=raw_end<raw_end+amount<=g['range'][1]]
                if len(events)!=1 or any(data_carve.va_bytes(original,cursor,amount)) or original_references(original,cursor,end):return match[0]
                proved_gaps.append(dict(original_range=[cursor,end],actual_cc1_alignment=events[0]));cursor=end
        if cursor!=hi:return match[0]
        temporary=f'.Lobjdiff_current_pool_{lo:08X}';renames[name]=temporary
        identity_proofs.append(dict(scaffold_name=name,original_range=[lo,hi],source_pools=[
            dict(range=z['range'],raw=z['raw'],source={k:v for k,v in z['source'].items() if k!='payload'},span=z['span'],payload_sha256=hashlib.sha256(z['payload']).hexdigest()) for z in pieces],
            real_alignment_gaps=proved_gaps,no_original_identity=True,all_storage_retained=True,no_new_credit=True))
        return '.local '+name+'\n'+name+':\n'+match[3]
    body=pattern.sub(replace,body)
    word=re.compile(r'^(\s*/\*\s+\S+\s+([\da-fA-F]{8})\s+([\da-fA-F]{8})\s*\*/\s*\.word\s+)(D_[\da-fA-F]+)(?:\s*\+\s*(0x[\da-fA-F]+|\d+))?\s*$',re.M)
    def numeric(match):
        va=int(match[2],16);raw=bytes.fromhex(match[3]);value=int(match[4][2:],16)+int(match[5] or '0',0)
        candidates=[z for z in zones if not z.get('existing_consumers') and z['range'][0]<=va<va+4<=z['range'][1] and
                    z['payload'][va-z['range'][0]:va-z['range'][0]+4]==raw]
        if len(candidates)!=1 or value!=int.from_bytes(raw,'little') or data_carve.va_bytes(original,va,4)!=raw:return match[0]
        zone=candidates[0];sec=original.section(zone['section'])
        if any(rel.type in (4,9) and rel.info==sec.index and any(va<p+4 and p<va+4 for p,k,n,a in data_carve._rel_entries(original,rel)) for rel in original.sections):return match[0]
        word_proofs.append(dict(original_address=va,value=value,source={k:v for k,v in zone['source'].items() if k!='payload'},registered_span=zone['span'],
                               no_actual_compiler_or_original_relocation=True,all_bytes_retained=True))
        return match[1]+f'0x{value:08X}'
    body=word.sub(numeric,body)
    def rename(text):
        for old,new in renames.items():text=re.sub(r'(?<![\w.$])'+re.escape(old)+r'(?![\w.$])',lambda m:new,text)
        return text
    def include(match):
        p=Path(match[1]);p=p if p.is_absolute() else cwd/p
        old=p.read_text();new=rename(old)
        return new if new!=old else match[0]
    body=rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,body))
    return body,dict(identities=identity_proofs,literal_words=word_proofs)


def prepare(root, evidence, only_objects=None, *, iop_only=False, project_dir=None):
    build = root / 'build'
    project = Path(project_dir) if project_dir is not None else build / 'objdiff'
    verification.require(project.resolve().is_relative_to(root / 'build') or
                         project.resolve().is_relative_to(root / '.work'), 'report project must be private')
    project.mkdir(parents=True, exist_ok=True)
    objects, targets, _, _, _ = cov.tracked_scope(root)
    if iop_only:
        verification.require(only_objects is None, 'IOP-only and scoped EE object preparation are exclusive')
        objects = []
    if only_objects is not None:
        requested=set(only_objects)
        verification.require(requested and requested<={o['id'] for o in objects},'unknown scoped native object')
        objects=[o for o in objects if o['id'] in requested]
    tc = Toolchain(root) if objects else None
    prefix = Path(tc.dir('ps2dev-binutils')) / 'mips64r5900el-ps2-elf-' if tc else None
    main = verification.read(build / 'main/tu-manifest.json') if objects else {}
    main_tus = {t['name']: t for t in main.get('tus', [])}
    data_registry=verification.read(data_carve.registry_input_path(root)).get('tus',{}) if objects else {}
    mapcheck_path=build/'main/build/main.mapcheck.json'
    mapcheck=verification.read(mapcheck_path) if objects and mapcheck_path.is_file() else {}
    overlays = {unit: verification.read(build / unit / 'compile-manifest.json')
                for unit in cov.OVERLAY_UNITS} if objects else {}
    originals = verification.read(root / cov.ORIGINALS)['units']
    expected = {}

    def prepare_object(obj):
        unit, stem = obj['unit'], Path(obj['source']).stem
        unit_dir, name = build / unit, f'{unit}/{stem}'
        destination = project / name
        destination.mkdir(parents=True, exist_ok=True)
        checked = evidence['objects'][obj['id']]
        target_text = destination / 'text.o'
        if unit == 'main':
            tu = main_tus[stem]
            contract = tu.get('contract', {})
            ccdir = Path(contract.get('cc_dir', '.'))
            assembler = Path(contract.get('assembler_path', '.'))
            flags = contract.get('flags', [])
            data_sources = [unit_dir / (piece.get('file') or
                            f'asm/main/data/{piece["piece"]}.{section[1:]}.s')
                            for section, entry in main['sections'].items()
                            for piece in entry['order'] if piece.get('tu') == stem]
            if obj['functions']:
                target_text = build_target(unit_dir, stem, unit)
            else:
                empty_text_source=target_text.with_suffix('.s')
                empty_text_source.write_text('.include "include/labels.inc"\n.section .text\n')
                run([str(prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64',
                     '-I.', '-Iinclude', '-o', target_text, empty_text_source], unit_dir)
        else:
            tus = overlays[unit]['units'][unit]['tus']
            tu = next(t for t in tus if t['name'] == stem)
            ccdir = Path(tu['compiler']['dir'])
            assembler = Path(tu['assembler']['path'])
            flags = tu['flags']
            data_sources = sorted((unit_dir / f'asm/data/{unit}').glob(stem + '.*.s'))
            if tu['kind'] == 'c':
                compile_source(root, unit_dir, unit_dir / f'scaffold/src/{unit}/{stem}.c',
                               target_text, ccdir, assembler, flags, skip_asm=False)
            else:
                target_text = build_target(unit_dir, stem, unit)
        data, data_assembly = [], []
        # Source type evidence is available before reconstructing original data.
        # The compiled object itself remains unmodified and is compared natively.
        base, assembly = destination / 'base.o', None
        data_only=(unit=='main' and not obj['functions'] and not checked['functions'] and
                   tu.get('kind')=='data-only' and tu.get('mode')=='c' and
                   (root/obj['source']).is_file() and any(
                       runs for section,runs in data_registry.get(obj['id'],{}).items()
                       if section in data_carve.SECTIONS and section!='.text'))
        compile_base=bool(checked['functions']) or data_only
        if compile_base:
            verified_object, verified_assembly = (verification.build_object(root, build, obj)
                                                   if checked['functions'] else (None, None))
            assembly = compile_source(root, unit_dir, root / obj['source'], base,
                                      ccdir, assembler, flags, skip_asm=True,
                                      verified_assembly=verified_assembly)
        if checked['functions']:
            start, end = ([int(tu['text'][k], 16) for k in ('start', 'end')] if unit == 'main'
                          else [int(v, 16) for v in tu['text']])
            reference = destination / 'original.text.o'
            target_text = original_text(target_text, unit_dir / 'orig' / originals[unit]['file'],
                                        start, end, reference, unit_dir, obj, assembler, flags,
                                        ordinary=verified_object, base=base, scope=checked['functions'])
        literal_pool = False
        if unit == 'main' and checked['functions']:
            entry = tu.get('sections', {}).get('.lit4', {})
            extent = tuple(int(entry[k],16) for k in ('start','end')) if entry else None
            target_text, literal_pool = original_literal_pool(target_text,
                unit_dir/'orig'/originals[unit]['file'], start, extent,
                destination/'literal.text.o', unit_dir, obj, assembler, flags)
        for source in data_sources:
            if literal_pool and source.stem.endswith('.lit4'):
                continue  # The exact anonymous original pool is already in the reference.
            output = destination / (source.stem + '.o')
            run([str(prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64',
                 '-G0', '-no-pad-sections', '-mno-fix-r5900', '-I.', '-Iinclude',
                 '-o', output, source], unit_dir)
            if unit == 'main':
                entry = tu.get('sections', {}).get('.' + source.stem.rsplit('.', 1)[-1], {})
                extent = tuple(int(entry[k], 16) for k in ('start', 'end')) if entry else None
                output = original_data(output, source, unit_dir / 'orig' / originals[unit]['file'],
                                       destination / (source.stem + '.original.o'), unit_dir, obj,
                                       Path(str(base) + '.i').read_text() if assembly else '', prefix, extent,
                                       compiler_object=base if assembly else None, compiler_assembly=assembly,
                                       registry=data_registry.get(obj['id'],{}))
            data.append(output)
            data_assembly.append((output,source))
        target = destination / 'target.o'
        stripped = destination / 'text.nodebug.o'
        strip_debug(prefix, target_text, stripped, unit_dir)
        run([str(prefix) + 'ld', '-EL', '-m', 'elf32lr5900', '-r',
             '-o', target, stripped, *data], unit_dir)
        joint_identities=[];current_placement=None
        if unit=='main' and compile_base:
            joint,joint_identities=joint_original(target,target_text,data_assembly,
                unit_dir/'orig'/originals[unit]['file'],destination/'joint.o',
                unit_dir,obj,tu,assembler,flags,prefix,compiler_object=base,
                compiler_assembly=assembly,preprocessed=Path(str(base)+'.i').read_text(),
                scope=checked['functions'],registry=data_registry.get(obj['id'],{}))
            if joint!=target:
                strip_debug(prefix,joint,target,unit_dir)
            comparison=destination/'base.placed.o'
            current_placement=registered_current_data_layout(root,unit_dir,obj,tu,base,assembly,
                Path(str(base)+'.i').read_text(),evidence,comparison,prefix,reference=target)
            if current_placement:
                original=parsed_elf((unit_dir/'orig'/originals[unit]['file']).read_bytes())
                body,cleanup=current_layout_reference((destination/'joint.s').read_text(),parsed_elf(target.read_bytes()),
                    original,parsed_elf(base.read_bytes()),assembly.read_text(),Path(str(base)+'.i').read_text(),
                    data_registry.get(obj['id'],{}),tu,unit_dir,checked['functions'],current_placement)
                verification.require(cleanup['identities'],'current DATA placement lacks a source-pool identity witness')
                source=destination/'current-reference.s';source.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',target,source],unit_dir)
                linked_reference=destination/'current-reference.elf'
                run([str(prefix)+'ld','-EL','-m','elf32lr5900','-T',destination/'joint.ld',
                     '-o',linked_reference,target],unit_dir)
                resolved=parsed_elf(linked_reference.read_bytes())
                for sec in resolved.sections:
                    if sec.flags&2 and sec.size and sec.type!=8:
                        verification.require(resolved.section_bytes(sec)==data_carve.va_bytes(original,sec.addr,sec.size),
                                             'current source-pool reference does not resolve to complete original bytes')
                for path,digest in current_placement['input_pins'].items():
                    verification.require(verification.digest(Path(path))==digest,'current DATA view input changed during reference proof')
                (destination/'current-data-layout.proof.json').write_text(json.dumps(dict(
                    placement=current_placement,source_pool_reference=cleanup,
                    normal_link_all_original_allocated_bytes_exact=True),indent=2)+'\n')
                strip_debug(prefix,target,target,unit_dir)
        unit_config = dict(name=name, target_path=str(target), metadata=dict(
            source_path=obj['source'], progress_categories=[unit], complete=checked['complete']))
        allowed = {f['original_symbol']: f['size'] for f in checked['functions']}
        if compile_base:
            emitted = verification.compiler_functions(assembly.read_text())
            actual = functions(base)
            nested = nested_base_names(root, obj['id'], allowed, actual)
            # Every remaining definition must be emitted C in the verified scope,
            # with its verified size; the include view cannot supply any body.
            verification.require(actual == {nested.get(n, n): size for n, size in allowed.items()}
                                 and set(actual) <= emitted,
                                 f'{name}: compiler-only emission differs from verified C scope')
            stripped_base = destination / 'base.nodebug.o'
            comparison_base=base
            if current_placement:
                comparison_base=destination/'base.placed.o'
            if obj['id']=='main/tu237':
                comparison_base=destination/'base.allocated.o'
                common_proof=allocate_registered_layout(base,assembly.read_text(),Path(str(base)+'.i').read_text(),
                    data_registry.get(obj['id'],{}),comparison_base,str(prefix)+'ld',run,unit_dir)
                (destination/'common-allocation.proof.json').write_text(json.dumps(common_proof,indent=2)+'\n')
            strip_debug(prefix, comparison_base, stripped_base, unit_dir)
            unit_config['base_path'] = str(stripped_base)
            # The target keeps the map name (e.g. `_WrapperDestruct_00A0DB58`,
            # which tells same-named original locals apart) while the C object
            # defines the original symbol. Pair them with objdiff's own manual
            # mapping (target name -> base name) instead of leaving them unmatched.
            mappings = {f['name']: nested.get(f['original_symbol'], f['original_symbol'])
                        for f in checked['functions']
                        if f['name'] != nested.get(f['original_symbol'], f['original_symbol'])}
            if joint_identities:
                emitted_data={**emitted_storage(assembly.read_text()),**typed_compiler_data(assembly.read_text())}
                compiled=parsed_elf(base.read_bytes())
                original_target=parsed_elf(target.read_bytes())
                source_sha=hashlib.sha256((root/obj['source']).read_bytes()).hexdigest()
                original=parsed_elf((unit_dir/'orig'/originals[unit]['file']).read_bytes())
                mapping_proofs=[]
                for identity in joint_identities:
                    name=identity['original_name']
                    if identity['type']!=1 or identity['size']<=0:continue
                    names=[s for s in compiled.symbols if s.name==name and 0<s.shndx<len(compiled.sections)
                           and (s.bind,s.type,s.size)==(identity['binding'],identity['type'],identity['size'])]
                    emission=emitted_data.get(name)
                    if len(names)!=1 or not emission:continue
                    symbol=names[0];section=compiled.sections[symbol.shndx]
                    address=identity['address'];extent=identity['size']
                    original_sections=[s for s in original.sections if s.name==section.name and s.addr<=address and address+extent<=s.addr+s.size]
                    owners=[r for r in data_registry.get(obj['id'],{}).get(section.name,[]) if
                            int(r['range'][0],16)<=address and address+extent<=int(r['range'][1],16) and
                            set(r.get('c_owned_symbols',[])).intersection((name,identity['target_name']))]
                    if len(original_sections)!=1 or len(owners)!=1 or section.type!=1:continue
                    if (emission.get('section'),emission.get('size'))!=(section.name,extent):continue
                    if any(rel.type==9 and rel.info==section.index and any(symbol.value<=struct.unpack_from('<I',compiled.data,rel.offset+i)[0]<symbol.value+extent for i in range(0,rel.size,rel.entsize)) for rel in compiled.sections):continue
                    old=original_sections[0]
                    if compiled.section_bytes(section)[symbol.value:symbol.value+extent]!=original.data[old.offset+address-old.addr:old.offset+address-old.addr+extent]:continue
                    mappings[identity['target_name']]=name
                    mapping_proofs.append(dict(target_name=identity['target_name'],c_name=name,
                        original_identity=identity,source_sha256=source_sha,
                        cc1_storage=emission,registered_run=owners[0]['range'],
                        original_and_compiler_initializer_exact=True,no_compiler_data_relocations=True))
                transports=evidence.get('data_name_transport_proofs',{}).get(stem,
                    mapcheck.get('c_tu_audit',{}).get(stem,{}).get('compiler_data_name_transport_proofs',[]))
                for proof in transports:
                    if proof.get('source_sha256')!=source_sha:continue
                    matches=[p for p in joint_identities if p['original_name']==proof['original_symbol']
                             and p['address']==int(proof['original_address'],16)
                             and (p['binding'],p['type'],p['size'])==
                             (proof['original_binding'],proof['original_type'],proof['original_size'])]
                    names=[s for s in compiled.symbols if s.name==proof['c_symbol'] and s.shndx not in (0,0xfff1)
                           and (s.bind,s.type,s.size)==(proof['c_binding'],proof['c_type'],proof['c_size'])]
                    emission=emitted_data.get(proof['c_symbol'])
                    if len(matches)!=1 or len(names)!=1 or not emission:continue
                    target_names=[s for s in original_target.symbols if s.name==matches[0]['target_name']
                                  and 0<s.shndx<len(original_target.sections)]
                    if len(target_names)!=1 or not 0<names[0].shndx<len(compiled.sections):continue
                    # Native 3.8.1 computes paired symbol order within the same
                    # section name. A .data/.sdata pair has equal broad DATA kind
                    # but cannot be compared there; preserve both real sections
                    # and let native section comparison expose the difference.
                    if original_target.sections[target_names[0].shndx].name!=compiled.sections[names[0].shndx].name:continue
                    if (emission.get('section'),emission.get('size'))!=(proof['compiler_storage']['section'],proof['compiler_storage']['size']):continue
                    if (proof['original_binding'],proof['original_type'],proof['original_size'])!=(proof['c_binding'],proof['c_type'],proof['c_size']):continue
                    mappings[matches[0]['target_name']]=proof['c_symbol']
                    mapping_proofs.append(dict(target_name=matches[0]['target_name'],c_name=proof['c_symbol'],
                        original_identity=matches[0],source_sha256=source_sha,
                        current_cc1_storage=emission,registered_mapcheck_transport=proof))
                joint_proof=destination/'joint.proof.json'
                if joint_proof.is_file():
                    provenance=json.loads(joint_proof.read_text())
                    provenance['native_symbol_mapping_proofs']=mapping_proofs
                    provenance['current_source_sha256']=source_sha
                    joint_proof.write_text(json.dumps(provenance,indent=2)+'\n')
            data_mappings,data_mapping_proofs=registered_data_symbol_mappings(target,base,
                parsed_elf((unit_dir/'orig'/originals[unit]['file']).read_bytes()),assembly.read_text(),Path(str(base)+'.i').read_text(),
                data_registry.get(obj['id'],{}),tu,unit_dir)
            mappings.update(data_mappings)
            if obj['id']=='main/tu237':
                aliases=[a for r in data_registry[obj['id']]['.sbss'] for a in r.get('c_storage_aliases',[])
                         if a['original_name']=='D_004DC6B4' and a['storage_owner']==
                         {'name':'layoutManagerId','address':'0x004DC6B4','size':4}]
                verification.require(len(aliases)==1,'layout: missing registered COMMON alias')
                mappings['D_004DC6B4']='layoutManagerId'
            (destination/'registered-mappings.proof.json').write_text(json.dumps(data_mapping_proofs,indent=2)+'\n')
            if mappings:
                unit_config['symbol_mappings'] = mappings
        target_functions = functions(target)
        verification.require(len(target_functions) == len(obj['functions']) and
                             sum(target_functions.values()) == sum(f[2] for f in obj['functions']),
                             f'{name}: target function scope differs from tracked TU')
        # The native report names a function by its target symbol, which is the
        # map name or the original name depending on how the target was built.
        credited = {name: f['size'] for f in checked['functions']
                    for name in (f['name'], f['original_symbol'])}
        return unit_config, dict(functions=target_functions, allowed=credited,
                                 complete=checked['complete'], data_only=data_only)

    if 'fork' in multiprocessing.get_all_start_methods():
        # These Python-heavy proofs need separate interpreters for parallelism.
        # Fork inherits the validated, read-only context and the worker closure;
        # each object still writes only its own destination. Keep the existing
        # four-worker bound and ordered results, including exception propagation.
        with ProcessPoolExecutor(max_workers=4,
                                 mp_context=multiprocessing.get_context('fork'),
                                 initializer=_init_object_preparer,
                                 initargs=(prepare_object,)) as pool:
            prepared = list(pool.map(_prepare_in_process, objects))
    else:
        with ThreadPoolExecutor(max_workers=4) as pool:
            prepared = list(pool.map(prepare_object, objects))
    units = []
    for unit, checked in prepared:
        verification.require(unit['name'] not in expected, 'duplicate TU name')
        units.append(unit)
        expected[unit['name']] = checked
    if only_objects is None:
        from tu import iop_objdiff
        iop = evidence.get('iop') or {}
        verification.require(iop.get('build_report'),
                             'verified IOP build missing; run verify_report --finish --iop-report PATH')
        report_path = root / iop['build_report']
        verification.require(verification.digest(report_path) == iop.get('build_report_sha256'),
                             'verified IOP build report changed')
        iop_units, iop_expected, iop_proof = iop_objdiff.prepare(root, project, report_path)
        verification.require(not set(iop_expected).intersection(expected), 'duplicate IOP object name')
        units.extend(iop_units)
        expected.update(iop_expected)
        (project / 'iop-proof.json').write_text(json.dumps(iop_proof, indent=2) + '\n')
    config = dict(build_target=False, build_base=False, units=units,
                  progress_categories=[dict(id=u, name=targets.get(u, u)) for u in cov.UNITS])
    (project / 'objdiff.json').write_text(json.dumps(config, indent=1) + '\n')
    return project, expected


def validate(report, expected):
    """Reject scope drift and scaffold credit; leave native JSON unchanged."""
    units = {u['name']: u for u in report['units']}
    verification.require(set(units) == set(expected), 'native report dropped or added units')
    for name, checked in expected.items():
        unit = units[name]
        actual = {f['name']: int(f['size']) for f in unit.get('functions', [])}
        verification.require(actual == checked['functions'], f'{name}: native function scope drift')
        for function in unit.get('functions', []):
            verification.require(not function.get('fuzzy_match_percent', 0) or
                                 function['name'] in checked['allowed'],
                                 f'{name}/{function["name"]}: scaffolding received matching credit')
            if function['name'] in checked['allowed']:
                verification.require(function.get('fuzzy_match_percent') == 100,
                                     f'{name}/{function["name"]}: verified C is not a native 100% match')
        # Keep objdiff's native BSS measure. It describes uninitialized storage
        # matching, never recovered C declarations or source/link completeness.
        if not checked['allowed'] and not checked.get('data_only', False):
            verification.require(not any(int(s.get('size', 0)) and s['name'] not in ('.bss', '.sbss')
                                         and s.get('fuzzy_match_percent') == 100
                                         for s in unit.get('sections', [])),
                                 f'{name}: initialized scaffold data received matching credit')
        verification.require(bool(unit.get('metadata', {}).get('complete', False)) == checked['complete'],
                             f'{name}: unexpected complete metadata')
        if checked['complete']:
            measures=unit.get('measures',{})
            for kind in (('code','data') if name.startswith('main/') else ('code',)):
                total=int(measures.get('total_'+kind,0));matched=int(measures.get('matched_'+kind,0))
                verification.require(matched==total and (not total or measures.get('matched_'+kind+'_percent')==100),
                                     f'{name}: complete TU is not an exact native {kind} match')

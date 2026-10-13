"""Prepare ELF object pairs for objdiff, without calculating report scores.

Targets contain original disassembly and original TU data. Bases are fresh C
compilations with SKIP_ASM: neither fallback nor standalone assembly earns C
credit. IOP originals have no base. All binary inputs remain local.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import ast
import json
from pathlib import Path
import re
import struct
import subprocess

import coverage_report as cov
from tu import data_carve
from tu import verify_report as verification
from tu.elfinfo import Elf
from tu.toolchain import Toolchain


def run(argv, cwd):
    subprocess.run(['timeout', '-k', '5', '30', *map(str, argv)], cwd=cwd,
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def allocate_registered_layout(base, assembly, preprocessed, registry, output,
                               linker, run, cwd):
    from tu.elfinfo import Elf
    from tu.storage_directives import emitted_storage
    from tu.data_gate import compiler_only_section
    before = Elf(base.read_bytes())
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
    native = Elf(allocated.read_bytes())
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
    after = Elf(output.read_bytes())
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
    from tu.data_gate import compiler_only_section
    compiled=Elf(base.read_bytes());reference=Elf(target.read_bytes())
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


def compile_source(root, unit_dir, source, output, ccdir, assembler, flags, skip_asm):
    common = [ccdir / 'ee-gcc', f'-B{ccdir}/', '-nostdinc', '-fno-builtin']
    preprocessed, assembly = Path(str(output) + '.i'), Path(str(output) + '.s')
    run([*common, '-E', *flags, *(['-DSKIP_ASM'] if skip_asm else []), '-Iinclude', '-I.',
         f'-I{root / "include"}', f'-I{source.parent}', source, '-o', preprocessed], unit_dir)
    run([*common, '-S', *flags, preprocessed, '-o', assembly], unit_dir)
    gflag = next(flag for flag in flags if flag.startswith('-G'))
    run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, '-I.',
         '-Iinclude', '-o', output, assembly], unit_dir)
    return assembly


def build_target(unit_dir, stem, unit):
    candidates = ([f'build/expected/{stem}.o', f'build/hasm/{stem}.o']
                  if unit == 'main' else [f'build/src/{unit}/{stem}.o',
                                         f'build/scaffold/src/{unit}/{stem}.o'])
    path = next((unit_dir / p for p in candidates if (unit_dir / p).is_file()), None)
    verification.require(path is not None, f'{unit}/{stem}: missing verified build object')
    return path


def functions(path):
    return {s.name: s.size for s in Elf(path.read_bytes()).symbols
            if s.type == 2 and s.shndx not in (0, 0xfff1) and s.size}


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


def original_text(reference, original_path, start, end, destination, cwd, obj, assembler, flags):
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
    original = Elf(original_path.read_bytes())
    scaffold = Elf(reference.read_bytes())
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
    literal_operands = bool(original.gp and literal_sections)
    loaded = [s for s in original.sections if s.flags & 2 and s.size]
    assembly = destination.with_suffix('.s')
    body = Path(str(reference) + '.s').read_text()
    expected = bytearray(scaffold.section_bytes(text))
    changes = []
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
            address = original.gp + immediate
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
        new = constant.sub(numeric_constant, instruction.sub(operand, old))
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
    extracted = Elf(destination.read_bytes())
    verification.require(extracted.section_bytes(extracted.section('.text')) == expected and
                         functions(destination) == functions(reference),
                         f'{obj["id"]}: numeric disassembly changed other instructions or function scope')
    # No synthesized relocation may survive on a numeric original operand.
    for rel in extracted.sections:
        if rel.type == 9 and rel.info == extracted.section('.text').index:
            relocated = {struct.unpack_from('<I', extracted.data, rel.offset + i)[0]
                         for i in range(0, rel.size, rel.entsize)}
            verification.require(not relocated.intersection(changes),
                                 f'{obj["id"]}: original numeric operand received an invented relocation')
    return destination


def original_literal_pool(reference, original_path, start, extent, destination, cwd, obj,
                          assembler, flags):
    """Recover the assembler's literal association from original GP loads.

    Old ee-as creates .lit4 and R_MIPS_LITERAL for li.s. Splat instead emits
    independently named data and a numeric GP load, losing that association.
    Reconstruct it only when the original has no OBJECT/FUNC in the pool and
    the contractual assembler reproduces the entire original pool, in order,
    with every load resolving to its exact original address/instruction.
    No C output supplies these constants or bytes; a rejected hypothesis leaves
    the ordinary reference unchanged.
    """
    if not extent:
        return reference, False
    original = Elf(original_path.read_bytes())
    before = Elf(reference.read_bytes())
    lo, hi = extent
    lit = next((s for s in original.sections if s.name == '.lit4' and s.addr <= lo < hi <= s.addr+s.size), None)
    if lit is None or any(s.type in (1, 2) and lo <= s.value < hi for s in original.symbols):
        return reference, False
    if any(s.name == '.lit4' and s.size for s in before.sections):
        return reference, False
    text = before.section('.text')
    source = Path(str(reference) + '.s')
    if not source.is_file():
        source = reference.with_suffix('.s')
    if not source.is_file():
        return reference, False
    loads = {}
    body = source.read_text()
    row = re.compile(r'^(\s*/\*\s+\S+\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+\*/\s*)'
                     r'lwc1\s+(\$f\d+),\s*(-?\d+)\(\$gp\)\s*$', re.M)

    def include(match):
        path = Path(match[1])
        path = path if path.is_absolute() else cwd/path
        def load(line):
            address = original.gp + int(line[5]) if original.gp else -1
            if not lo <= address <= hi-4:
                return line[0]
            va = int(line[2], 16)
            data = bytes.fromhex(line[3])
            code = next((s for s in original.sections if s.flags & 4 and s.addr <= va < s.addr+s.size), None)
            if code is None or original.data[code.offset+va-code.addr:code.offset+va-code.addr+4] != data:
                return line[0]
            word, = struct.unpack('<I', data)
            if word >> 26 != 0x31 or word >> 21 & 31 != 28 or word >> 16 & 31 != int(line[4][2:]):
                return line[0]
            value, = struct.unpack_from('<f', original.data, lit.offset+address-lit.addr)
            if not float('-inf') < value < float('inf'):
                return line[0]
            loads[va-start] = dict(address=address, word=word)
            return f'{line[1]}li.s {line[4]}, {value:.20e}'
        old = path.read_text()
        new = row.sub(load, old)
        if new == old:
            return match[0]
        local = destination.parent/'original-literals'/path.name
        local.parent.mkdir(exist_ok=True)
        local.write_text(new)
        return '.include '+json.dumps(str(local))
    rewritten = re.sub(r'\.include\s+"([^\"]+\.s)"', include, body)
    if not loads:
        return reference, False
    assembly = destination.with_suffix('.s')
    assembly.write_text(rewritten)
    try:
        gflag = next(flag for flag in flags if flag.startswith('-G'))
        run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, '-I.', '-Iinclude',
             '-o', destination, assembly], cwd)
        after = Elf(destination.read_bytes())
        actual_lit = after.section('.lit4')
    except (subprocess.CalledProcessError, StopIteration):
        return reference, False
    expected_pool = original.data[lit.offset+lo-lit.addr:lit.offset+hi-lit.addr]
    if ((actual_lit.type, actual_lit.flags, actual_lit.align) != (lit.type, lit.flags, lit.align)
            or after.section_bytes(actual_lit) != expected_pool or functions(destination) != functions(reference)):
        return reference, False
    current = after.section_bytes(after.section('.text'))
    previous = before.section_bytes(text)
    if len(current) != len(previous):
        return reference, False
    for section in before.sections:
        if section.flags & 2 and section.name != '.text':
            candidates = [s for s in after.sections if s.name == section.name]
            if (len(candidates) != 1 or (section.type,section.flags,section.size) !=
                    (candidates[0].type,candidates[0].flags,candidates[0].size) or
                    before.section_bytes(section) != after.section_bytes(candidates[0])):
                return reference, False
    def retained_relocations(elf):
        return {(elf.sections[rel.info].name, offset, info & 0xff,
                 next(s.name for s in elf.symbols if s.index == info >> 8))
                for rel in elf.sections if rel.type == 9
                for i in range(0, rel.size, rel.entsize)
                for offset, info in [struct.unpack_from('<II', elf.data, rel.offset+i)]
                if elf.sections[rel.info].name != '.text' or offset not in loads}
    if retained_relocations(before) != retained_relocations(after):
        return reference, False
    relocations = {}
    # ELF MIPS REL literal addends include the assembler's provisional GP.
    # Read that normal ABI metadata; never modify the emitted object.
    reginfo = after.section('.reginfo')
    assembler_gp, = struct.unpack_from('<I', after.data, reginfo.offset+20)
    for rel in after.sections:
        if rel.type == 9 and rel.info == after.section('.text').index:
            for i in range(0, rel.size, rel.entsize):
                offset, info = struct.unpack_from('<II', after.data, rel.offset+i)
                symbol = next(s for s in after.symbols if s.index == info >> 8)
                if info & 0xff == 8 and symbol.shndx == actual_lit.index:
                    relocations[offset] = symbol.value
    for offset in range(0, len(current), 4):
        if offset not in loads:
            if current[offset:offset+4] != previous[offset:offset+4]:
                return reference, False
            continue
        if offset not in relocations:
            return reference, False
        word, = struct.unpack_from('<I', current, offset)
        addend = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
        address = lo+relocations[offset]+addend+assembler_gp
        resolved = word & 0xffff0000 | ((address-original.gp) & 0xffff)
        if address != loads[offset]['address'] or resolved != loads[offset]['word']:
            return reference, False
    identities = lambda elf: {(s.name,s.value,s.size,s.type,s.bind) for s in elf.symbols if s.type == 2}
    verification.require(identities(before) == identities(after), f'{obj["id"]}: literal reconstruction changed FUNC identities')
    destination.with_suffix('.proof.json').write_text(json.dumps(dict(
        schema='objdiff-original-literal-pool/1', tu=obj['id'], original_sha256=hashlib.sha256(original.data).hexdigest(),
        address=f'0x{lo:08X}', size=hi-lo, initializer_sha256=hashlib.sha256(expected_pool).hexdigest(),
        loads={hex(k):v for k,v in loads.items()}, complete_original_pool_exact=True,
        all_original_load_addresses_and_instructions_exact=True), indent=2)+'\n')
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
    for declaration in re.finditer(r'\btypedef\s+([^;{}]+?)\s+(\w+)\s*;', preprocessed):
        spelling = re.sub(r'\b(?:signed|unsigned|const|volatile)\b', '', declaration[1])
        spelling = ' '.join(spelling.split())
        if spelling in widths:
            widths[declaration[2]] = widths[spelling]
    arrays = {}
    pattern = r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*\[\s*(0[xX][0-9a-fA-F]+|\d+)\s*\]\s*=\s*\{([^{}]*)\}\s*;'
    literal = re.compile(r'(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)(?:[uUlL]*)')
    for declaration in re.finditer(pattern, preprocessed):
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
    for section in original.sections:
        if section.type == 9:
            for offset in range(section.offset, section.offset + section.size, section.entsize):
                address, info = struct.unpack_from('<II', original.data, offset)
                symbol = next((s for s in original.symbols if s.index == info >> 8), None)
                if begin <= address < end or symbol and begin <= symbol.value < end:
                    return True
        if not section.flags & 2 or section.type == 8:
            continue
        data = original.section_bytes(section)
        if not section.flags & 4:
            if any(begin <= struct.unpack_from('<I', data, offset)[0] < end
                   for offset in range(0, len(data) - 3, 4)):
                return True
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
                    if begin <= value < end:
                        return True
                if rs == 28 and original.gp and begin <= original.gp + signed < end:
                    return True
    return False


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
        declarations=list(re.finditer(pattern,function.text(preprocessed)))
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
    for match in re.finditer(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):
        aliases.setdefault(match[2],set()).add(clean(match[1]))
    ti={m[1] for m in re.finditer(r'\btypedef\s+unsigned\s+int\s+(\w+)\s+__attribute__\s*\(\(\s*mode\s*\(\s*TI\s*\)\s*\)\)\s*;',preprocessed)}
    def scalar(typename,seen=()):
        typename=clean(typename)
        if typename in seen:return None
        if typename in widths:return widths[typename],widths[typename],('unsigned' in typename)
        if typename in ti:return 16,16,True
        choices=aliases.get(typename,set())
        return scalar(next(iter(choices)),seen+(typename,)) if len(choices)==1 else None
    unions={}
    for match in re.finditer(r'\btypedef\s+union(?:\s+\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;',preprocessed):
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
    for declaration in re.finditer(r'\b(?:static\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*=\s*',preprocessed):
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
    for m in re.finditer(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):
        aliases.setdefault(m[2],set()).add(clean(m[1]))
    ti={m[1] for m in re.finditer(r'\btypedef\s+unsigned\s+int\s+(\w+)\s+__attribute__\s*\(\(\s*mode\s*\(\s*TI\s*\)\s*\)\)\s*;',preprocessed)}
    for m in re.finditer(r'\btypedef\s+(struct|union)(?:\s+\w+)?\s*\{',preprocessed):
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
    for declaration in re.finditer(r'\b(?:static\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*=\s*',preprocessed):
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
    for m in declaration.finditer(preprocessed):
        name=m[1]
        if name not in emitted:continue
        try:count=_record_bound(m[2])
        except RecordLayoutError:continue
        if not 0<count<=65536:continue
        at=m.end();payload=None;form=None
        literal=re.match(r'("(?:\\.|[^"\\])*"(?:\s*"(?:\\.|[^"\\])*")*)\s*;',preprocessed[at:])
        if literal:
            try:payload=b''.join(ast.literal_eval(token).encode('latin1') for token in re.findall(r'"(?:\\.|[^"\\])*"',literal[1]))+b'\0'
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
    for m in re.finditer(r'\btypedef\s+([\w ]+?)\s+(\w+)\s*;',preprocessed):
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
    for m in pattern.finditer(preprocessed):
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


def typed_literal_source_objects(preprocessed, compiler, assembly):
    """Decode pointer-free C scalar/array/record initializers independently.

    The EE scalar widths and explicit source fields establish layout. Strings
    are admitted only as char arrays; unions admit only their complete first
    member. Unknown expressions, pointers, attributes and divergent types
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
    for m in re.finditer(r'\btypedef\s+([^;{}()]+?)\s+(\w+)\s*;',preprocessed):aliases.setdefault(m[2],set()).add(clean(m[1]))
    for m in re.finditer(r'\b(typedef\s+)?(struct|union)(?:\s+(\w+))?\s*\{',preprocessed):
        at=m.end();end=at;depth=1
        while end<len(preprocessed) and depth:depth+=(preprocessed[end]=='{')-(preprocessed[end]=='}');end+=1
        tail=re.match(r'\s*(\w+)?\s*;',preprocessed[end:])
        if depth or not tail or (not m[1] and tail[1]):continue
        names=([tail[1]] if m[1] and tail[1] else [])+([m[2]+' '+m[3]] if m[3] else [])
        for n in names:records.setdefault(n,set()).add((m[2],preprocessed[at:end-1]))
    def layout(typename,seen=()):
        typename=clean(typename)
        if typename in seen:raise RecordLayoutError('recursive type')
        if typename in widths:
            width,unsigned=widths[typename];return dict(kind='integer',size=width,align=width,unsigned=unsigned,char='char' in typename)
        if typename in ('float','double'):return dict(kind='float',size=4 if typename=='float' else 8,align=4 if typename=='float' else 8)
        if len(aliases.get(typename,set()))==1:return layout(next(iter(aliases[typename])),seen+(typename,))
        definitions=records.get(typename,set())
        if len(definitions)!=1:raise RecordLayoutError('unknown/ambiguous or pointer type')
        kind,body=next(iter(definitions));fields=[];offset=0;alignment=1
        for spelling in split(body,';'):
            m=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)\s*\]\s*)*)',spelling)
            if not m:
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
            child=layout(m[1],seen+(typename,))
            for bound in reversed(re.findall(r'\[\s*([^\]]+)\s*\]',m[3])):
                count=int(bound,16 if 'x' in bound.lower() else 10)
                if not 0<count<=65536:raise RecordLayoutError('field count')
                child=dict(kind='array',child=child,count=count,size=count*child['size'],align=child['align'])
            place=0 if kind=='union' else (offset+child['align']-1)//child['align']*child['align']
            fields.append(dict(name=m[2],offset=place,layout=child));alignment=max(alignment,child['align'])
            offset=max(offset,child['size']) if kind=='union' else place+child['size']
        size=(offset+alignment-1)//alignment*alignment
        if not fields or not 0<size<=65536:raise RecordLayoutError('record extent')
        if kind=='union' and fields[0]['layout']['size']!=size:raise RecordLayoutError('union first view incomplete')
        return dict(kind=kind,size=size,align=alignment,fields=fields)
    def decode(node,text):
        text=text.strip();kind=node['kind']
        if kind=='integer':
            if re.fullmatch(r"'(?:\\.|[^'\\])*'",text):
                data=quoted(text,"'")
                if len(data)!=1:raise RecordLayoutError('multi-character literal')
                n=data[0]
            else:
                if not re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|0[0-7]*|[1-9]\d*)[uUlL]*',text):raise RecordLayoutError('integer expression')
                value=re.sub(r'[uUlL]+$','',text);sign=-1 if value.startswith('-') else 1;value=value.lstrip('+-')
                n=sign*int(value,16 if value.lower().startswith('0x') else 8 if value.startswith('0') else 10)
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
            for i,item in enumerate(items):
                if item.startswith('.') or item.startswith('['):raise RecordLayoutError('array designator unsupported')
                payload=decode(node['child'],item);start=i*node['child']['size'];result[start:start+len(payload)]=payload
        else:
            fields=[f for f in (node['fields'][:1] if kind=='union' else node['fields']) if f['name'] is not None]
            if len(items)>len(fields):raise RecordLayoutError('record excess initializer')
            for field,item in zip(fields,items):
                designator=re.match(r'\.(\w+)\s*=\s*',item)
                if designator:
                    if designator[1]!=field['name']:raise RecordLayoutError('record designator order')
                    item=item[designator.end():]
                payload=decode(field['layout'],item);result[field['offset']:field['offset']+len(payload)]=payload
        return bytes(result)
    emitted=typed_compiler_data(assembly);result={}
    declarations=re.finditer(r'\b(?:static\s+)?(?:const\s+)?([\w ]+?)\s+(\w+)\s*((?:\[\s*(?:0[xX][\da-fA-F]+|\d+)?\s*\]\s*)*)\s*(?:__attribute__\s*\(\(\s*(?:section\s*\(\s*"\.(?:data|rodata|sdata)"\s*\)|aligned\s*\(\s*(?:1|2|4|8|16|32|64|128|256)\s*\))\s*\)\)\s*)?=\s*',preprocessed)
    for declaration in declarations:
        typename,name=clean(declaration[1]),declaration[2]
        if name not in emitted:continue
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
    return result


def numeric_source_objects(preprocessed, compiler, assembly):
    """Literal-only numeric source objects, with independently emitted storage.

    Struct admission resolves its existing field types recursively and refuses
    pointers, unions, bitfields, casts and expressions. Compiler bytes provide
    the layout witness, never the initializer hypothesis. A separate bounded
    record route admits four-byte int fields among pointer fields; it proves
    each scalar's literal value and offset and leaves all pointer words alone.
    """
    emitted = typed_compiler_data(assembly)
    static_bindings=function_static_source_bindings(preprocessed,compiler,assembly)
    scalar = {'char','short','short int','int','long','long int','long long','long long int','float','double'}
    integers = {'int'}
    aliases, records, pointer_types = {}, {}, set()
    enumerators={}
    for declaration in re.finditer(r'\benum(?:\s+\w+)?\s*\{([^{}]+)\}',preprocessed):
        values={};value=-1
        for item in declaration[1].split(','):
            if not item.strip():continue
            match=re.fullmatch(r'\s*(\w+)\s*(?:=\s*(0[xX][\da-fA-F]+|\d+))?\s*',item)
            if not match:break
            value=int(match[2],0) if match[2] else value+1;values[match[1]]=value
        else:enumerators.update(values)
    clean = lambda t: ' '.join(re.sub(r'\b(?:const|volatile|signed|unsigned)\b','',t).split())
    for m in re.finditer(r'\btypedef\s+([^;{}]+?)\s+(\w+)\s*;',preprocessed):
        if '*' not in m[1] and '(' not in m[1]:aliases[m[2]]=clean(m[1])
    for m in re.finditer(r'\btypedef\s+[^;{}]+\(\s*\*\s*(\w+)\s*\)\s*\([^;{}]*\)\s*;',preprocessed):
        pointer_types.add(m[1])
    for m in re.finditer(r'\btypedef\s+struct(?:\s+\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;',preprocessed):
        records[m[2]]=m[1]
    for m in re.finditer(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}\s*;',preprocessed):
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
    for declaration in re.finditer(pattern,preprocessed):
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
                    # Ordinary integer literals only; no casts/expressions,
                    # omitted slots, designators or pointer/union values.
                    numbers=re.findall(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*',value)
                    residue=re.sub(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*','',value)
                    if not re.sub(r'[\s{},]','',residue) and len(numbers)==len(members):
                        for literal,(place,width,path) in zip(numbers,members):
                            if width!=4:continue
                            integer=re.sub(r'[uUlL]+$','',literal);integer=int(integer,16 if 'x' in integer.lower() else 10)
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
            m=re.fullmatch(r'\s*([\w ]+?)\s+(\w+)\s*',f)
            if not m or not (int32(m[1]) or clean(m[1]) in pointer_types):break
            fields.append((clean(m[1]),m[2]))
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
        if name not in result:result[name]=proof
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
    original = Elf(original_path.read_bytes())
    before = Elf(reference.read_bytes())
    arrays = integer_literal_arrays(preprocessed) if preprocessed else {}
    numeric_objects = (numeric_source_objects(preprocessed,Elf(compiler_object.read_bytes()),compiler_assembly.read_text())
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
                        compiled=Elf(compiler_object.read_bytes())
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
    after = Elf(destination.read_bytes())
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
    from tu.data_gate import compiler_only_section
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
        try:fragment=ast.literal_eval(row['operands']).encode('latin1')
        except (ValueError,SyntaxError,AttributeError,UnicodeError):
            fragments=[];continue
        fragments.append((row,fragment))
        payload=b''.join(fragment for _,fragment in fragments)
        if payload.endswith(b'\0'):
            if b'\0' not in payload[:-1]:
                payloads.append((payload,dict(fragments[0][0],consecutive_ascii_rows=[r for r,_ in fragments])))
            fragments=[]
    literals=[]
    for token in re.findall(r'"(?:\\.|[^"\\])*"',preprocessed):
        try:literals.append(ast.literal_eval(token).encode('latin1')+b'\0')
        except (ValueError,SyntaxError,UnicodeError):pass
    symbols={s.index:s for s in compiled.symbols};relocations={}
    for rel in compiled.sections:
        if rel.type==9 and rel.info==text.index:
            for i in range(0,rel.size,rel.entsize):
                place,info=struct.unpack_from('<II',compiled.data,rel.offset+i)
                relocations[place]=(info&255,symbols[info>>8])
    claimed={f['original_symbol']:f for f in scope};functions=[s for s in compiled.symbols if s.type==2 and s.shndx==text.index and s.name in claimed]
    code=compiled.section_bytes(text);raw=compiled.section_bytes(csection);proofs=[]
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
        try:payload=word_payload if word_payload is not None else ast.literal_eval(first[2]).encode('latin1')+b'\0'
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
        # An existing named C owner is compared through its own storage
        # identity. Only compiler-anonymous pools need identity removal.
        if objects:return match[0]
        named=None
        if objects:
            if len(runs)!=1:return match[0]
            if len(objects)!=1:return match[0]
            candidate=objects[0];storage=emitted.get(candidate.name)
            if (candidate.name,candidate.value,candidate.size)!=(name,offset,len(payload)) or not storage:return match[0]
            if (storage['section'],storage['size'])!=(section.name,len(payload)):return match[0]
            definition=re.compile(r'(?<![\w])(?:static\s+)?const\s+(?:unsigned\s+)?char\s+'+re.escape(name)+r'\s*\[\s*(?:\d+)?\s*\]\s*=\s*("(?:\\.|[^"\\])*")\s*;')
            definitions=list(definition.finditer(preprocessed))
            if len(definitions)!=1:return match[0]
            try:initializer=ast.literal_eval(definitions[0][1]).encode('latin1')+b'\0'
            except (ValueError,SyntaxError,UnicodeError):return match[0]
            if initializer!=payload:return match[0]
            named=dict(c_name=candidate.name,binding=candidate.bind,type=candidate.type,size=candidate.size,
                       source_form='const char array with literal initializer',cc1_typed_storage=storage,
                       original_named_object_identity_not_asserted=True)
        consumers=[]
        for place,(kind,symbol) in relocations.items():
            if kind!=5 or symbol.shndx!=csection.index:continue
            high=struct.unpack_from('<I',code,place)[0]
            if high>>26!=15:continue
            for lowplace in range(place+4,min(place+132,len(code)),4):
                lowrel=relocations.get(lowplace)
                if not lowrel or lowrel[0]!=6 or lowrel[1].index!=symbol.index:continue
                low=struct.unpack_from('<I',code,lowplace)[0];immediate=low&65535
                if low>>21&31!=high>>16&31 or low>>26 not in (8,9,*range(32,64)):continue
                if ((high&65535)<<16)+immediate-(65536 if immediate&32768 else 0)+symbol.value!=offset:continue
                owners=[f for f in functions if f.value<=place<lowplace<f.value+f.size]
                if len(owners)!=1:continue
                f=owners[0];claim=claimed[f.name]
                of=verification.original_symbol(original,f.name,claim['size'],claim['va'])
                if (f.bind,f.type,f.size)!=(of.bind,of.type,of.size):continue
                os=original.sections[of.shndx];base=os.offset+of.value-os.addr-f.value
                oh,ol=struct.unpack_from('<I',original.data,base+place)[0],struct.unpack_from('<I',original.data,base+lowplace)[0]
                if (high&0xffff0000,low&0xffff0000)!=(oh&0xffff0000,ol&0xffff0000):continue
                if ((oh&65535)<<16)+(ol&65535)-(65536 if ol&32768 else 0)!=lo:continue
                consumers.append(dict(function=f.name,cc1_hi_offset=place,cc1_lo_offset=lowplace,original_string_address=lo))
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
    from tu.data_gate import compiler_only_section
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
              and name in r.get('c_owned_symbols',[])]
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
    from storage_directives import emitted_storage
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
            declarations=re.findall(r'\bstatic\s+((?:(?:unsigned|signed)\s+)?[A-Za-z_]\w*)\s+'+re.escape(owner_name)+r'\s*\[\s*(0[xX][\da-fA-F]+|\d+)\s*\]\s*;',preprocessed)
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
    from storage_directives import emitted_storage
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
                    declarations=set(re.findall(r'\bstatic\s+((?:(?:unsigned|signed)\s+)?[A-Za-z_]\w*)\s+'+re.escape(owner)+r'\s*(\[[^;]+\])?\s*;',preprocessed))
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
                                        assembly, preprocessed, registry, tu, cwd, destination):
    """Remove Splat's invented GLOBAL identity over a complete C char array.

    This does not assert an original OBJECT, name or binding from the candidate.
    It retains an internal address label and every storage directive/consumer.
    A complete registered individual C owner, independent SOURCE initializer,
    ordinary cc1 emission, original bytes and absent retail identities are
    mandatory. Shorter arrays and any nominal relocation consumer refuse.
    """
    from data_types import _record_split, _record_bound, RecordLayoutError
    typed=typed_compiler_data(assembly);proofs=[];replacements={}
    reloc_symbols=set()
    for rel in reference.sections:
        if rel.type==9:
            reloc_symbols.update(struct.unpack_from('<II',reference.data,p)[1]>>8
                for p in range(rel.offset,rel.offset+rel.size,8))
    pattern=re.compile(r'(?:nonmatching\s+(\S+)\s*\n\s*)?dlabel\s+(\S+)\s*\n(.*?)enddlabel\s+\2\s*\n\.size\s+\2,\s*\.\s*-\s*\2',re.S)
    def replace(match):
        name=match[2];symbols=[s for s in reference.symbols if s.name==name and 0<s.shndx<len(reference.sections)]
        if len(symbols)!=1:return match[0]
        target=symbols[0];section=reference.sections[target.shndx]
        if section.name not in ('.data','.sdata','.rodata') or (target.bind,target.type)!=(1,1) or target.size<=0 or target.index in reloc_symbols:return match[0]
        mapped=tu.get('sections',{}).get(section.name)
        if not mapped:return match[0]
        lo=int(mapped['start'],16)+target.value;hi=lo+target.size;retail=original.section(section.name)
        if not int(mapped['start'],16)<=lo<hi<=int(mapped['end'],16):return match[0]
        if any(s.name and s.shndx==retail.index and s.type in (0,1,2)
            and s.value<hi and lo<s.value+max(s.size,1) for s in original.symbols):return match[0]
        if any(s.name in (name,name+'.NON_MATCHING') and s.shndx not in (0,0xfff1) for s in original.symbols):return match[0]
        actual=[s for s in compiled.symbols if s.name==name and (s.bind,s.type,s.size)==(0,1,target.size) and 0<s.shndx<len(compiled.sections)
            and compiled.sections[s.shndx].name==section.name]
        if len(actual)!=1:return match[0]
        cs=actual[0];emission=typed.get(name)
        if not emission or (emission['section'],emission['size'])!=(section.name,target.size):return match[0]
        declarations=list(re.finditer(r'\bstatic\s+(?:const\s+)?(?:signed\s+|unsigned\s+)?char\s+'+re.escape(name)+r'\s*\[\s*([^\[\]]+)\s*\]\s*'
            r'(?:__attribute__\s*\(\(\s*section\s*\(\s*"'+re.escape(section.name)+r'"\s*\)\s*\)\)\s*)?=\s*',preprocessed))
        if len(declarations)!=1:return match[0]
        declaration=declarations[0]
        try:count=_record_bound(declaration[1])
        except RecordLayoutError:return match[0]
        if count!=target.size:return match[0]
        at=declaration.end();initializer=None;spelling=''
        if preprocessed[at:at+1]=='"':
            literal=re.match(r'"(?:\\.|[^"\\])*"\s*;',preprocessed[at:])
            if not literal:return match[0]
            spelling=literal[0][:-1].strip()
            try:initializer=ast.literal_eval(spelling).encode('latin1')+b'\0'
            except (ValueError,SyntaxError,UnicodeError):return match[0]
            if len(initializer)>count:return match[0]
            initializer+=bytes(count-len(initializer))
        elif preprocessed[at:at+1]=='{':
            end=preprocessed.find('}',at)
            if end<0 or not re.match(r'\s*;',preprocessed[end+1:]):return match[0]
            spelling=preprocessed[at:end+1];items=_record_split(spelling[1:-1],',')
            if len(items)!=count:return match[0]
            decoded=[]
            for item in items:
                if not re.fullmatch(r'[-+]?(?:0[xX][\da-fA-F]+|\d+)[uUlL]*',item):return match[0]
                text=re.sub(r'[uUlL]+$','',item);value=int(text,16 if 'x' in text.lower() else 10)
                if not -128<=value<=255:return match[0]
                decoded.append(value%256)
            initializer=bytes(decoded)
        if initializer is None:return match[0]
        expected=original.data[retail.offset+lo-retail.addr:retail.offset+hi-retail.addr]
        raw=compiled.section_bytes(compiled.sections[cs.shndx])[cs.value:cs.value+cs.size]
        if not len(expected)==len(raw)==len(initializer)==target.size or expected!=raw or expected!=initializer:return match[0]
        if any(rel.type==9 and rel.info==cs.shndx and any(cs.value<=struct.unpack_from('<I',compiled.data,p)[0]<cs.value+cs.size
            for p in range(rel.offset,rel.offset+rel.size,8)) for rel in compiled.sections):return match[0]
        spans=[]
        for run in registry.get(section.name,[]):
            if name not in run.get('c_owned_symbols',[]):continue
            for span in run.get('c_input_spans',[]):
                extent=explicit_object_range(span)
                if not extent or span.get('section')!=section.name or name not in span.get('symbols',[]):continue
                coordinates=span.get('range')
                if not isinstance(coordinates,(list,tuple)) or len(coordinates)!=2 or any(not isinstance(v,str) or not re.fullmatch(r'0x[\da-fA-F]+',v) for v in coordinates):continue
                r=[int(v,16) for v in coordinates]
                if not r[0]<r[1]:continue
                if r[1]-r[0]==extent[1]-extent[0] and extent[0]<=cs.value<cs.value+cs.size<=extent[1] and r[0]+cs.value-extent[0]==lo:spans.append(span)
        if len(spans)!=1:return match[0]
        temporary=f'.Lobjdiff_registered_char_{lo:08X}';replacements[name]=temporary
        proofs.append(dict(scaffold_name=name,temporary_reference_name=temporary,original_range=[lo,hi],
            registered_individual_c_owner=name,registered_c_input_span=spans[0],compiler_offset=cs.value,
            explicit_source_array_extent=count,source_initializer=spelling,cc1_typed_storage=emission,
            original_payload_sha256=hashlib.sha256(expected).hexdigest(),no_retail_named_identity=True,
            no_nominal_relocation_consumer=True,original_object_identity_not_asserted=True,
            complete_storage_preserved_and_no_new_SOURCE_credit=True))
        return '.local '+name+'\n'+name+':\n'+match[3]
    rewritten=pattern.sub(replace,body)
    rewritten,extra=registered_anonymous_storage(rewritten,reference,original,compiled,assembly,preprocessed,registry,tu,cwd)
    proofs.extend(extra)
    replacements.update({p["scaffold_name"]:p["temporary_reference_name"] for p in extra})
    if not proofs:return body,[]
    def rename(text):
        for name,temporary in replacements.items():text=re.sub(r'(?<![\w.$])'+re.escape(name)+r'(?![\w.$])',lambda _:temporary,text)
        return text
    if destination is None:return rename(rewritten),proofs
    def include(match):
        path=Path(match[1]);path=path if path.is_absolute() else cwd/path
        old=path.read_text();new=rename(old)
        if old==new:return match[0]
        local=destination.parent/'joint-anonymous-asm'/path.name
        local.parent.mkdir(exist_ok=True);local.write_text(new)
        return '.include '+json.dumps(str(local))
    return rename(re.sub(r'\.include\s+"([^\"]+\.s)"',include,rewritten)),proofs


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
    from storage_directives import emitted_storage
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
    from storage_directives import emitted_storage
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
            rows=re.findall(r'\bstatic\s+([A-Za-z_]\w*)\s+'+re.escape(name)+r'\s*;',function.text(preprocessed))
            for ctype in rows:
                if not re.search(r'\btypedef\s+struct\s*\{[^{}]*\}\s*'+re.escape(ctype)+r'\s*;',preprocessed,re.S):continue
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
    original, before = Elf(original_path.read_bytes()), Elf(reference.read_bytes())
    source = text_reference.with_suffix('.s')
    if not source.is_file():source=Path(str(text_reference)+'.s')
    if not source.is_file():return reference, []
    body=source.read_text();identities=[];switch_proofs=[];padding_proofs=[];string_proofs=[];nobits_proofs=[];static_proofs=[];marker_proofs=[]
    for path, scaffold_source in data:
        source=path.with_suffix('.s')
        if not source.is_file():source=scaffold_source
        if not source.is_file():return reference, []
        part=source.read_text()
        part=re.sub(r'^\s*\.include\s+"macro.inc"\s*$', '',part,flags=re.M)
        part=re.sub(r'^(\.section\s+\.[\w.]+),[^\n]*$',r'\1',part,flags=re.M)
        if compiler_object and compiler_assembly:
            part,pad_proofs=source_alignment_padding(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),registry or {},tu,Elf(path.read_bytes()))
            padding_proofs.extend(pad_proofs)
            part,literal_proofs=compiler_string_identity(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},cwd,Elf(path.read_bytes()))
            string_proofs.extend(literal_proofs)
            part,alias_proofs=compiler_nobits_alias_identity(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,registry or {},tu,obj['id'])
            nobits_proofs.extend(alias_proofs)
            part,local_proofs=compiler_function_static_identity(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu)
            static_proofs.extend(local_proofs)
            part,natural_proofs=compiler_original_static_name(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu)
            static_proofs.extend(natural_proofs)
        if compiler_object and compiler_assembly and 'dlabel jtbl_' in part:
            part,table_proofs=compiler_switch_identity(part,original,Elf(compiler_object.read_bytes()),
                compiler_assembly.read_text(),preprocessed,scope,registry or {},tu,cwd)
            switch_proofs.extend(table_proofs)
        section=re.search(r'^\.section\s+(\.[\w.]+)',part,re.M)
        if not section:return reference, []
        native=original.section(section[1])
        part,aliases=splat_data_marker_identity(part,original,Elf(path.read_bytes()),before,cwd/'include/macro.inc')
        marker_proofs.extend(aliases)
        identity_source=part
        for label in re.finditer(r'^dlabel\s+(\S+)\s*$',identity_source,re.M):
            name=label[1]
            address=re.search(r'/\*\s+(?:\S+\s+)?([0-9A-Fa-f]{8})(?:\s|\s*\*/)',identity_source[label.end():])
            if not address:continue
            va=int(address[1],16)
            matches=[s for s in original.symbols if s.shndx==native.index and s.value==va
                     and s.type in (0,1) and name in (s.name,s.name.replace('.','_'),s.name.replace('.','_')+f'_{va:08X}')]
            if len(matches)!=1:continue
            symbol=matches[0]
            definition=f'.{"local" if symbol.bind==0 else "globl"} {name}\n'
            if symbol.type==1:definition+=f'.type {name}, @object\n'
            definition+=name+':'
            part=re.sub(r'^dlabel\s+'+re.escape(name)+r'\s*$',lambda _:definition,part,flags=re.M)
            removed_marker=False
            if not any(s.name==name+'.NON_MATCHING' for s in original.symbols):
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
        body+='\n'+part
    if switch_proofs or string_proofs or nobits_proofs or static_proofs:
        replacements={p['scaffold_name']:p['temporary_reference_name'] for p in switch_proofs+string_proofs+nobits_proofs+static_proofs}
        def rename(text):
            for old,new in replacements.items():
                text=re.sub(r'(?<![\w.$])'+re.escape(old)+r'(?![\w.$])',lambda _:new,text)
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
        after=Elf(destination.read_bytes())
        anonymous_char_proofs=[]
        if compiler_object and compiler_assembly:
            rewritten,anonymous_char_proofs=registered_anonymous_char_identities(body,after,original,
                Elf(compiler_object.read_bytes()),compiler_assembly.read_text(),preprocessed,registry,tu,cwd,destination)
            if anonymous_char_proofs:
                old=after;body=rewritten;assembly.write_text(body)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=Elf(destination.read_bytes())
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
                Elf(compiler_object.read_bytes()),compiler_assembly.read_text(),preprocessed,registry,tu,cwd,destination)
            if registered_nobits_proofs:
                old=after;assembly.write_text(rewritten)
                run([assembler,'-EL','-m5900','-mabi=eabi',next(f for f in flags if f.startswith('-G')),
                     '-I.','-Iinclude','-o',destination,assembly],cwd)
                after=Elf(destination.read_bytes())
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
            compiled=Elf(compiler_object.read_bytes());cc1=compiler_assembly.read_text()
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
                after=Elf(destination.read_bytes())
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
        funcs=lambda e:{(s.name,s.value,s.size,s.bind,s.type) for s in e.symbols if s.type==2}
        if funcs(before)!=funcs(after):return reference, []
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
            if (s.value+start,s.bind,s.type,s.size)!=(proof['address'],proof['binding'],proof['type'],size):return reference, []
        # Link the reference normally at original addresses. This checks every
        # changed local addend together with all unchanged foreign relocations.
        definitions=[];call_proofs=[];unused_external_proofs=[]
        for symbol in after.symbols:
            if symbol.shndx!=0 or not symbol.name:continue
            if symbol.name=='_gp':continue
            values={s.value for s in original.symbols if s.shndx not in (0,0xfff1)
                    and symbol.name in (s.name,s.name.replace('.','_'),s.name.replace('.','_')+f'_{s.value:08X}')}
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
        final=Elf(linked.read_bytes())
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
            compiler_nobits_alias_identity_proofs=nobits_proofs,
            registered_nobits_foreign_alias_identity_proofs=registered_nobits_proofs,
            registered_anonymous_char_identity_proofs=anonymous_char_proofs,
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
    before = Elf(source.read_bytes())
    # IRX stores module-relative data addresses/relocations. Make its data
    # sections section-relative for objdiff, keeping text addresses and every
    # FUNC identity. Target-only IOP references have no base and earn zero.
    options = (['--remove-section=.rel.text', '--remove-section=.rel.data', '--strip-all',
                *[f'--change-section-address={s.name}=0' for s in before.sections
                  if s.flags & 2 and not s.flags & 4],
                *[f'--keep-symbol={name}' for name in functions(source)]] if iop_reference else [])
    run([str(prefix) + 'objcopy', '--strip-debug', *options, source, destination], cwd)
    after = Elf(destination.read_bytes())
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
    for m in pattern.finditer(preprocessed):
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
                payload = b''.join(ast.literal_eval(t).encode('latin1') for t in re.findall(r'"(?:\\.|[^"\\])*"', literal[1])) + b'\0'
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
                        value = ast.literal_eval(item)
                        if not isinstance(value, str) or len(value) != 1:
                            break
                        value = ord(value)
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
    from storage_directives import emitted_storage
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
    from tu.data_gate import compiler_only_section
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
                                 assembly, preprocessed, registry, tu, cwd):
    proofs=[]
    for section in ('.rodata','.sdata'):
        body,rows=registered_anonymous_storage_section(body,reference,original,compiled,
            assembly,preprocessed,registry,tu,cwd,section)
        proofs.extend(rows)
    return body,proofs


def registered_anonymous_storage_section(body, reference, original, compiled,
                                 assembly, preprocessed, registry, tu, cwd, section_name):
    """Restore anonymous C constant pools from accepted affine storage proofs.

    No original named identity is removed. Every byte and padding byte stays
    present, with the ordinary joint link checking the complete result.
    """
    from tu.data_gate import compiler_only_section
    try:
        compiler_only_section(assembly, preprocessed, cwd, section_name)
    except (RuntimeError, ValueError):
        return body, []
    section = next((s for s in compiled.sections if s.name == section_name), None)
    mapped = tu.get('sections', {}).get(section_name)
    if not section or not mapped:
        return body, []
    retail = original.section(section_name)
    nominal = set()
    for rel in reference.sections:
        if rel.type == 9:
            nominal.update(struct.unpack_from('<II', reference.data, p)[1] >> 8
                           for p in range(rel.offset, rel.offset + rel.size, 8))
    proofs = []
    pattern = re.compile(r'dlabel\s+(D_[\da-fA-F]{8})\s*\n(.*?)enddlabel\s+\1\s*\n\.size\s+\1,\s*\.\s*-\s*\1', re.S)
    def replace(match):
        name = match[1]
        targets = [s for s in reference.symbols if s.name == name and s.type == 1
                   and 0 < s.shndx < len(reference.sections)
                   and reference.sections[s.shndx].name == section_name]
        if len(targets) != 1 or targets[0].index in nominal:
            return match[0]
        target = targets[0]
        lo = int(mapped['start'], 16) + target.value
        hi = lo + target.size
        if lo != int(name[2:], 16) or hi > int(mapped['end'], 16):
            return match[0]
        if any(s.name and s.shndx == retail.index and s.value < hi
               and lo < s.value + max(s.size, 1) for s in original.symbols):
            return match[0]
        spans = []
        for run in registry.get(section_name, []):
            if name not in run.get('c_owned_symbols', []):
                continue
            candidates = list(run.get('c_input_spans', []))
            if not candidates:
                # Historical dense carves predate individual span records.
                # Their entire ordinary C section must equal the original
                # registered generated range; only its anonymous tail qualifies.
                generated = [int(v, 16) for v in run.get('generated', [])]
                offset = lo - generated[0] if len(generated) == 2 else -1
                if (len(generated) == 2 and generated[1]-generated[0] == section.size
                    and int(mapped['start'],16) <= generated[0] <= lo < generated[1] <= hi
                    and 0 <= offset < section.size
                    and compiled.section_bytes(section) == original.section_bytes(retail)[generated[0]-retail.addr:generated[1]-retail.addr]
                    and not any(s.name == name for s in compiled.symbols)):
                    candidates.append(dict(section=section_name,range=[hex(lo),hex(generated[1])],
                        object_range=[offset,section.size],symbols=[],anonymous_emission=True,credit='verified',
                        reconstructed_from_accepted_dense_generated_range=run['generated']))
            for span in candidates:
                extent = explicit_object_range(span)
                coords = [int(v, 16) for v in span.get('range', [])]
                if (not extent or len(coords) != 2 or coords[0] != lo
                    or span.get('section') != section_name
                    or span.get('anonymous_emission') is not True
                    or span.get('credit') != 'verified' or span.get('symbols')
                    or not 0 <= extent[0] < extent[1] <= section.size
                    or coords[1] - lo != extent[1] - extent[0]
                    or not 0 <= hi - coords[1] <= 15):
                    continue
                if any(s.type == 1 and s.shndx == section.index
                       and s.value < extent[1] and extent[0] < s.value + s.size
                       for s in compiled.symbols):
                    continue
                if any(rel.type == 9 and rel.info == section.index
                       and any(extent[0] <= struct.unpack_from('<I', compiled.data, p)[0] < extent[1]
                               for p in range(rel.offset, rel.offset + rel.size, 8))
                       for rel in compiled.sections):
                    continue
                expected = original.section_bytes(retail)[lo-retail.addr:hi-retail.addr]
                payload = compiled.section_bytes(section)[extent[0]:extent[1]]
                if (expected[:len(payload)] != payload or any(expected[len(payload):])
                    or original_references(original, coords[1], hi)):
                    continue
                spans.append(span)
        if len(spans) != 1:
            return match[0]
        temporary = '.Lobjdiff_registered_pool_' + name[2:]
        proofs.append(dict(scaffold_name=name, temporary_reference_name=temporary,
                           original_range=[lo, hi], registered_c_input_span=spans[0],
                           compiler_only_section_verified=True,
                           no_retail_named_identity=True, no_nominal_relocation_consumer=True,
                           complete_storage_preserved_and_no_new_SOURCE_credit=True))
        return '.local ' + name + '\n' + name + ':\n' + match[2]
    return pattern.sub(replace, body), proofs


def prepare(root, evidence):
    build, project = root / 'build', root / 'build/objdiff'
    project.mkdir(exist_ok=True)
    objects, targets, _, _, _ = cov.tracked_scope(root)
    tc = Toolchain(root)
    prefix = Path(tc.dir('ps2dev-binutils')) / 'mips64r5900el-ps2-elf-'
    main = verification.read(build / 'main/tu-manifest.json')
    main_tus = {t['name']: t for t in main['tus']}
    data_registry=verification.read(data_carve.registry_input_path(root)).get('tus',{})
    mapcheck_path=build/'main/build/main.mapcheck.json'
    mapcheck=verification.read(mapcheck_path) if mapcheck_path.is_file() else {}
    overlays = {unit: verification.read(build / unit / 'compile-manifest.json')
                for unit in cov.OVERLAY_UNITS}
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
                run([str(prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64',
                     '-o', target_text, '/dev/null'], unit_dir)
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
        if checked['functions']:
            assembly = compile_source(root, unit_dir, root / obj['source'], base,
                                      ccdir, assembler, flags, skip_asm=True)
        if checked['functions']:
            start, end = ([int(tu['text'][k], 16) for k in ('start', 'end')] if unit == 'main'
                          else [int(v, 16) for v in tu['text']])
            reference = destination / 'original.text.o'
            target_text = original_text(target_text, unit_dir / 'orig' / originals[unit]['file'],
                                        start, end, reference, unit_dir, obj, assembler, flags)
        literal_pool = False
        if unit == 'main' and checked['functions']:
            entry = tu.get('sections', {}).get('.lit4', {})
            extent = tuple(int(entry[k],16) for k in ('start','end')) if entry else None
            target_text, literal_pool = original_literal_pool(target_text,
                unit_dir/'orig'/originals[unit]['file'], start, extent,
                destination/'literal.text.o', unit_dir, obj, assembler, flags)
        for source in data_sources:
            if literal_pool and source.stem.endswith('.lit4'):
                continue  # The exact entire original pool now comes from the original li.s loads.
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
        joint_identities=[]
        if unit=='main' and checked['functions']:
            joint,joint_identities=joint_original(target,target_text,data_assembly,
                unit_dir/'orig'/originals[unit]['file'],destination/'joint.o',
                unit_dir,obj,tu,assembler,flags,prefix,compiler_object=base,
                compiler_assembly=assembly,preprocessed=Path(str(base)+'.i').read_text(),
                scope=checked['functions'],registry=data_registry.get(obj['id'],{}))
            if joint!=target:
                strip_debug(prefix,joint,target,unit_dir)
        unit_config = dict(name=name, target_path=str(target), metadata=dict(
            source_path=obj['source'], progress_categories=[unit], complete=checked['complete']))
        allowed = {f['original_symbol']: f['size'] for f in checked['functions']}
        if allowed:
            emitted = verification.compiler_functions(assembly.read_text())
            actual = functions(base)
            nested = nested_base_names(root, obj['id'], allowed, actual)
            # Removing inline scaffold blocks can change scheduling/alignment.
            # Native objdiff measures that difference; the unmodified published
            # build remains the authority for exact linked sizes and bytes.
            verification.require(set(actual) == {nested.get(n, n) for n in allowed}
                                 and set(actual) <= emitted,
                                 f'{name}: SKIP_ASM emission differs from verified C scope')
            stripped_base = destination / 'base.nodebug.o'
            comparison_base=base
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
                from storage_directives import emitted_storage
                emitted_data={**emitted_storage(assembly.read_text()),**typed_compiler_data(assembly.read_text())}
                compiled=Elf(base.read_bytes())
                original_target=Elf(target.read_bytes())
                source_sha=hashlib.sha256((root/obj['source']).read_bytes()).hexdigest()
                original=Elf((unit_dir/'orig'/originals[unit]['file']).read_bytes())
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
                Elf((unit_dir/'orig'/originals[unit]['file']).read_bytes()),assembly.read_text(),Path(str(base)+'.i').read_text(),
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
                                 complete=checked['complete'])

    with ThreadPoolExecutor(max_workers=4) as pool:
        prepared = list(pool.map(prepare_object, objects))
    units = []
    for unit, checked in prepared:
        verification.require(unit['name'] not in expected, 'duplicate TU name')
        units.append(unit)
        expected[unit['name']] = checked
    iop = verification.read(root / cov.IOP_OBJECTS)
    for unit, entry in iop['units'].items():
        target = tc.game_dir() / entry['file']
        verification.require(verification.digest(target) == entry['sha256'], f'{unit}: wrong original')
        original_functions = functions(target)
        verification.require(len(original_functions) == entry['functions'], f'{unit}: original scope differs')
        destination = project / unit
        destination.mkdir(exist_ok=True)
        stripped = destination / 'target.o'
        strip_debug(prefix, target, stripped, root, iop_reference=True)
        units.append(dict(name=unit, target_path=str(stripped),
                          metadata=dict(complete=False, progress_categories=[unit])))
        expected[unit] = dict(functions=original_functions, allowed={}, complete=False)
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
        # Keep objdiff's native BSS measure. It describes uninitialized storage
        # matching, never recovered C declarations or source/link completeness.
        if not checked['allowed']:
            verification.require(not any(int(s.get('size', 0)) and s['name'] not in ('.bss', '.sbss')
                                         and s.get('fuzzy_match_percent') == 100
                                         for s in unit.get('sections', [])),
                                 f'{name}: initialized scaffold data received matching credit')
        verification.require(bool(unit.get('metadata', {}).get('complete', False)) == checked['complete'],
                             f'{name}: unexpected complete metadata')

#!/usr/bin/env python3
"""TU-mode acceptance audit: compiler provenance, original symbols, eligibility.

With the whole-file SHA-256 gate this is the normative TU acceptance check
(AGENTS.md, "TU-mode acceptance gate"). It is fail-closed: the audit compiles
the TU itself and every check that cannot be performed is a failure.

What it does, for one TU source and the read-only original ELF of its unit:

  0. Contract: the compile contract comes from config/tu-build.json and
     config/toolchains-tu.json (id, cc1/cpp0/driver and assembler digests,
     flags including -G). Every pin is verified; a contract file or flag that
     differs from the manifest is a failure.
  1. Compile: cpp -> cc1 -> as under that contract, in a private output
     directory (each step 30 s, own process group, minimal environment). Any
     message from any step is a failure. The audit uses its own outputs, never
     supplied ones, so a stale or hand-edited .s/.o cannot be certified.
  2. Build binding: the build's object (--obj, required) must equal the audit's
     object in every allocated section, relocation and symbol; the build's .s
     (--build-s) must equal the audit's .s apart from the recorded .i path.
  3. Preprocessing: every file cpp opened (tu.i line markers) must be admitted
     source or a checked scaffold header, every include must resolve to the
     file cpp used, and each claimed definition must be the one cpp kept - the
     single definition of that name in tu.i, attributed to the TU source inside
     the audited block's lines, and not inside a preprocessor conditional.
  4. Per claimed C function: (a) exactly one cc1 region .ent/.end outside
     top-level asm, in .text, under the TU source's .file, at the block's
     position, without .include, inline asm (outside the hardware class),
     data in .text or any .word other than a cc1 jump table; (b) the original
     .symtab FUNC symbol and the object symbol agree on name, binding, type,
     size and offset from the TU start, with no lost alias; (c) the admission
     of the function's class accepts the prelude plus the function text.
  5. ACCEPTED_ASM functions (exact_asm): the tracked .s under src/<unit>/... is
     included once, hashes to the accepted ee-asm record of config/units, and
     passes match.ee_assembly; every function it defines passes (b).
  6. Scaffold: include/labels.inc must be the pinned include_asm.labels_inc of
     config/toolchains-tu.json and define only the scaffold macros (a macro may
     not shadow a mnemonic); the INCLUDE_ASM/ACCEPTED_ASM templates must stay in
     .text and INCLUDE_RODATA in a data section; INCLUDE_RODATA payloads carry
     data only; no other top-level asm; no scaffold `.extern` of a symbol the C
     defines; scaffold `.word` sites of config/tu-build.json block their
     function.
  7. Object-wide: no cc1 function or named data outside the original TU, no
     symbol that is neither original, scaffold-defined nor claimed; C-owned
     data sections must define every original data symbol of their range; cc1
     COMMON symbols must carry the original size and a compatible alignment.

Usage (docs/tu-tools.md):
  tu_audit.py --unit ov02 --unit-dir UNIT --tu-c UNIT/src/ov02/title.c \\
      --tu-id ov02/tu002 --obj UNIT/build/src/ov02/title.o --out-dir FRESH
"""
import argparse
import collections
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import signal
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
# This private copy lives outside both the shared tools package and the subject
# tree. Its helper imports are pinned to the canonical repository; `ROOT` below
# remains the tree under audit and is rebound only by `resolve_root()`.
_AUDIT_PATH = Path(__file__).resolve()
TOOL_ROOT = (_AUDIT_PATH.parents[1] if _AUDIT_PATH.parent.name == "tools"
             else _AUDIT_PATH.parents[3])
sys.path.insert(0, str(TOOL_ROOT))
from tools import tu_edit  # noqa: E402
from tools import asm_contract_lint  # noqa: E402

# The tree under audit. Every read of the subject's own content -- its `src`,
# its `config` (TU manifest, accepted records, source classes, asm contracts,
# adaptations), its archives and its originals -- goes through this, and it is
# resolved from the audit's subject by `resolve_root()` before anything is read.
# It is a module global rather than an argument because the audit is one process
# auditing one tree; `resolve_root()` is the only writer.
ROOT = TOOL_ROOT

SCHEMA = 'tu-audit/2'
TIMEOUT = 30
CLASSES = ('ordinary', 'hardware', 'runtime-verbatim', 'native-ti', 'gnu-nested-function', 'ee-mmi',
           'ee-asm-leaf', 'ee-fpu', 'ee-vu-clip', 'ee-assert-break')
# Classes whose functions may carry admitted inline assembly and are reported
# `exact_c_with_asm` when they do: `hardware` (the ee-vu-cop2 route), `ee-mmi`
# (constrained inline EE MMI, user decision 2026-10-10, docs/ee-mmi.md), and
# `ee-asm-leaf` / `ee-fpu` (original hand-written assembly delivered as C with
# asm, user decision 2026-10-10 on main/tu100, docs/ee-hand-asm.md), and
# `ee-vu-clip` (the same decision extended on 2026-10-11 to the main/tu109 VU0
# lane-test blocks, docs/ee-hand-asm.md), and `ee-assert-break` (the assert
# macros' bare `break` trap through include/debug_break.h, user decision
# 2026-10-11 on main/tu121, docs/ee-hand-asm.md).
INLINE_ASM_CLASSES = ('hardware', 'ee-mmi', 'ee-asm-leaf', 'ee-fpu', 'ee-vu-clip', 'ee-assert-break')
SOURCE_CLASS = {'game_c': 'ordinary', 'sdk_c': 'ordinary', 'runtime_c': 'ordinary', 'hand_asm': 'ordinary'}
TU_MAPS = ('.work/tu-migration/map-main/tu-map-main.json', '.work/tu-migration/map-ovl/tu-map-ovl.json')
BIND = {0: 'LOCAL', 1: 'GLOBAL', 2: 'WEAK'}
STT_NOTYPE, STT_OBJECT, STT_FUNC, STT_SECTION, STT_FILE = 0, 1, 2, 3, 4
SHN_COMMON, SHN_MIPS_SCOMMON = 0xfff2, 0xff03
MARKER_NAMES = ('gcc2_compiled.', '__gnu_compiled_c')
ASM_CONTRACT_FIELDS = {'purpose', 'inputs', 'outputs', 'clobbers', 'memory', 'state', 'privilege', 'control_flow'}
CINC_TOOL = 'tools/tu/cinc.py'
SOURCE_CLASSES_FILE = 'config/tu/source-classes.json'
SOURCE_CLASSES_SCHEMA = 'tu-source-classes/1'
DATA_DIRECTIVES = {'.word', '.half', '.hword', '.short', '.byte', '.dword', '.2byte', '.4byte', '.8byte', '.int',
                   '.long', '.quad', '.octa', '.ascii', '.asciz', '.string', '.float', '.single', '.double',
                   '.space', '.skip', '.fill', '.zero', '.incbin', '.inst', '.insn', '.gpword', '.gpdword',
                   '.org', '.dc', '.dc.b', '.dc.w', '.dc.l', '.dc.a'}
WORD_LIKE = {'.word', '.4byte', '.long', '.int', '.gpword'}
TEXT_SECTIONS = ('.text', '.vutext')
DATA_SECTIONS = ('.data', '.rodata', '.rdata', '.sdata', '.lit4', '.lit8', '.bss', '.sbss')
RODATA_LIKE = ('.rodata', '.rdata', '.lit4', '.lit8')
# Only these macros may exist in the scaffold macro include: a GAS macro is
# resolved before machine instructions, so any other name can shadow a mnemonic.
SCAFFOLD_MACROS = frozenset(('glabel', 'endlabel', 'alabel', 'ehlabel', 'jlabel', 'dlabel', 'enddlabel',
                             'nonmatching'))
# Sections that only record debug/compiler paths; excluded from the object
# comparison because the recorded name of the preprocessed input differs
# between the build's output directory and the audit's private one.
DEBUG_SECTIONS = ('.mdebug', '.mdebug.eabi64', '.comment', '.pdr', '.gptab.sdata', '.gptab.sbss')


class AuditError(ValueError):
    """An input that makes the audit impossible (reported as a failing audit)."""


# ---------------------------------------------------------------------------
# Which tree is being audited
# ---------------------------------------------------------------------------
# The audit must read the subject's own `src` and `config`, never a second tree
# that happens to hold the same paths. It used to take them from the script's
# location, which is wrong in exactly the case that matters: `tools/worker.py`
# builds a sandbox whose `src`, `include` and `build` are private and whose
# `tools/` and `config/` are links to the shared repository, and then runs the
# SHARED `tools/tu_audit.py` over it. `Path(__file__).parents[1]` then named the
# published tree, so
#
#   * `record_fidelity` hashed and read the PUBLISHED TU file instead of the
#     candidate. `rel(tu_c)` could not name the candidate relative to that tree
#     either, so the check never even recognised that the record points at the
#     file under audit: it silently compared the candidate's body against the
#     published body of the same function and reported `verbatim`. A candidate
#     that rewrote a *different* accepted C function of the same TU passed with
#     `grants_acceptance` (measured, tu-audit-root-resolution).
#   * `c_record_index`/`accepted_asm_records` globbed the PUBLISHED
#     `config/units`, so a re-cut accepted `.s` could never be presented with
#     its narrowed record: the audit only ever saw the wide published one.
#   * `published_source_classes`, the asm contracts it pins, `lookup_tu`, the
#     adaptation files, the retired-source archive and
#     `match.runtime_verbatim_provenance` (which re-reads the source it is given
#     a root for) all read published state while claiming to audit a candidate.
#
# So the root comes from the subject: `--root` when given, else the repository
# tree that encloses `--unit-dir` -- the unit build directory whose `src` link
# decides which sources are compiled in the first place. A `--unit-dir` outside
# any repository tree, or a TU source outside the resolved one, is a failure
# with a named remedy, not a quiet fallback to wherever the script lives.
ROOT_MARKERS = ('config/tu-build.json', 'config/toolchains-tu.json', 'src')


def enclosing_root(path, what):
    """The nearest enclosing repository tree of `path`."""
    here = Path(os.path.abspath(path))
    for candidate in (here, *here.parents):
        if all((candidate / marker).exists() for marker in ROOT_MARKERS):
            return candidate
    raise AuditError(f'{what} {path} is not inside a repository tree (no enclosing directory has '
                     f'{", ".join(ROOT_MARKERS)}); pass --root to name the tree to audit')


def resolve_root(args):
    """Point the audit at the tree it was given, and report which one that is."""
    global ROOT
    if args.root:
        root = Path(os.path.abspath(args.root))
        missing = [m for m in ROOT_MARKERS if not (root / m).exists()]
        if missing:
            raise AuditError(f'--root {root} is not a repository tree: {missing} missing')
        basis = '--root'
    else:
        root = enclosing_root(args.unit_dir, '--unit-dir')
        basis = '--unit-dir'
    ROOT = root
    # The record indexes are keyed by nothing but ROOT, so a process that
    # re-points the root re-reads them instead of serving another tree's.
    global _C_RECORD_INDEX, _ACCEPTED_ASM_INDEX
    _C_RECORD_INDEX = _ACCEPTED_ASM_INDEX = None
    tu_c = Path(os.path.abspath(args.tu_c))
    inside = (rel(tu_c) != str(tu_c))
    return dict(path=str(root), basis=basis, tool_root=str(TOOL_ROOT),
                tool_outside_root=not str(TOOL_ROOT).startswith(str(root) + os.sep) and TOOL_ROOT != root,
                tu_source_inside_root=inside,
                detail=None if inside else
                       (f'the TU source {tu_c} is not inside the audited tree {root}, so its accepted '
                        'records, adaptations and source classes cannot be resolved against it; audit the '
                        'candidate where it is built, or pass --root'))


def sha256_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def sha256_text(text):
    return hashlib.sha256(text.encode('utf-8', 'surrogateescape')).hexdigest()


def rel(path, base=None):
    # `base` defaults to the audited tree at CALL time: binding ROOT in the
    # signature would freeze the tool's own location into every path the audit
    # reports, and into `record_fidelity`'s "does this record point at the file
    # I am auditing?" test, which is how the check went blind in a sandbox.
    base = ROOT if base is None else base
    # Lexically first, so a file reached through one of the tree's own links
    # (`config/` and `.work/` are links in a worker sandbox) is still named
    # relative to the tree it was reached through; then by real path, which is
    # what a directory argument spelled through a link needs.
    for here, there in ((Path(os.path.abspath(path)), Path(os.path.abspath(base))),
                        (Path(path).resolve(), Path(base).resolve())):
        try:
            return str(here.relative_to(there))
        except ValueError:
            continue
    return str(path)


# ---------------------------------------------------------------------------
# Relocatable ELF32 LE reader (objects)
# ---------------------------------------------------------------------------

class RelObject:
    """Sections, symbols and relocations of an ELF32 little-endian MIPS object."""

    def __init__(self, data):
        self.data = data
        if data[:6] != b'\x7fELF\x01\x01':
            raise AuditError('object is not ELF32 little-endian')
        (etype, machine, _v, _entry, _phoff, shoff, _flags, _eh, _phes, _phn, shentsize, shnum,
         shstrndx) = struct.unpack_from('<HHIIIIIHHHHHH', data, 16)
        if etype != 1 or machine != 8:
            raise AuditError('object is not a MIPS relocatable (ET_REL)')
        if shentsize < 40 or shoff + shnum * shentsize > len(data):
            raise AuditError('object section table out of range')
        raw = [struct.unpack_from('<10I', data, shoff + i * shentsize) for i in range(shnum)]
        names = data[raw[shstrndx][4]:raw[shstrndx][4] + raw[shstrndx][5]]
        self.sections = []
        for i, (name, kind, flags, addr, offset, size, link, info, align, entsize) in enumerate(raw):
            end = names.find(b'\0', name)
            self.sections.append(dict(index=i, name=names[name:end].decode('ascii', 'replace'), type=kind,
                                      flags=flags, addr=addr, offset=offset, size=size, link=link, info=info,
                                      align=align, entsize=entsize))
        self.symbols = []
        for sec in self.sections:
            if sec['type'] != 2:
                continue
            strtab = self.sections[sec['link']]
            strings = data[strtab['offset']:strtab['offset'] + strtab['size']]
            for k in range(sec['size'] // 16):
                name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', data, sec['offset'] + k * 16)
                end = strings.find(b'\0', name)
                section = self.sections[shndx]['name'] if 0 < shndx < len(self.sections) else None
                self.symbols.append(dict(index=k, name=strings[name:end].decode('utf-8', 'surrogateescape'),
                                         value=value, size=size, type=info & 15, binding=info >> 4, other=other,
                                         shndx=shndx, section=section))

    def section(self, name):
        found = [s for s in self.sections if s['name'] == name]
        return found[0] if len(found) == 1 else None

    def bytes_of(self, sec):
        if sec['type'] == 8:  # NOBITS
            return b''
        return self.data[sec['offset']:sec['offset'] + sec['size']]

    def functions(self):
        return [s for s in self.symbols if s['type'] == STT_FUNC and s['shndx'] != 0]

    def relocations(self):
        """[(section, offset, type, symbol name)] over all REL sections."""
        out = []
        for sec in self.sections:
            if sec['type'] != 9 or sec['entsize'] not in (8, 0):
                continue
            target = self.sections[sec['info']]['name'] if sec['info'] < len(self.sections) else '?'
            symtab = self.sections[sec['link']] if sec['link'] < len(self.sections) else None
            syms = [s for s in self.symbols if symtab and s['index'] is not None] if symtab else []
            by_index = {s['index']: s for s in syms}
            for k in range(sec['size'] // 8):
                offset, info = struct.unpack_from('<II', self.data, sec['offset'] + k * 8)
                sym = by_index.get(info >> 8)
                out.append((target, offset, info & 0xff, sym['name'] if sym else str(info >> 8)))
        return out


def compare_objects(fresh, build):
    """Differences between two objects, ignoring debug/path-only sections."""
    diffs = []
    a, b = RelObject(Path(fresh).read_bytes()), RelObject(Path(build).read_bytes())
    names_a = [s['name'] for s in a.sections if s['name'] not in DEBUG_SECTIONS and s['name']]
    names_b = [s['name'] for s in b.sections if s['name'] not in DEBUG_SECTIONS and s['name']]
    if set(names_a) != set(names_b):
        diffs.append(f'section sets differ: {sorted(set(names_a) ^ set(names_b))}')
    for name in sorted(set(names_a) & set(names_b)):
        sa, sb = a.section(name), b.section(name)
        if sa is None or sb is None:
            diffs.append(f'{name}: duplicate section name')
            continue
        if (sa['type'], sa['flags'], sa['size'], sa['align']) != (sb['type'], sb['flags'], sb['size'], sb['align']):
            diffs.append(f'{name}: header differs (size {sa["size"]} vs {sb["size"]})')
        if a.bytes_of(sa) != b.bytes_of(sb):
            diffs.append(f'{name}: contents differ')
    def symkey(o):
        return sorted((s['name'], s['value'], s['size'], s['type'], s['binding'], s['other'],
                       (s['section'] or '') if s['section'] not in DEBUG_SECTIONS else '')
                      for s in o.symbols if s['type'] != STT_FILE)
    if symkey(a) != symkey(b):
        only_a = [s for s in symkey(a) if s not in symkey(b)][:6]
        only_b = [s for s in symkey(b) if s not in symkey(a)][:6]
        diffs.append(f'symbol tables differ: audit-only {only_a}, build-only {only_b}')
    if sorted(a.relocations()) != sorted(b.relocations()):
        diffs.append('relocations differ')
    return diffs


_FILE = re.compile(r'^\t?\.file\t?\s*(\d+)\s+"[^"]*"', re.M)


def compare_assembly(fresh_text, build_text):
    """Differences between two cc1 outputs, ignoring the recorded input paths.

    cc1 records `.file 1` for the preprocessed input and `.file 2` for the TU
    source itself, each spelled the way the caller named it. The audit compiles
    in its own output directory, so both spellings legitimately differ from the
    build edge's even when the emitted code is identical. `.file 1` was already
    normalised; `.file 2` was not, which is why `--build-s` could only be passed
    where the build happened to spell the source exactly as the audit does (main;
    the overlay edges spell it relative to the unit directory). Normalising every
    `.file N` path - keeping its number, so a different *set* of files still
    differs - lets `--build-s` run for every unit (docs/tu-build.md, "Layout
    requirement").

    What the paths could have caught is covered elsewhere and not by a debug
    string: the object comparison checks sections, relocations and symbols, and
    the preprocessing check verifies that every file cpp opened is admitted
    source and that each claimed definition comes from the TU source itself.
    """
    norm = lambda t: _FILE.sub(lambda m: f'\t.file\t{m.group(1)} "<input>"', t)
    if norm(fresh_text) == norm(build_text):
        return []
    a, b = norm(fresh_text).splitlines(), norm(build_text).splitlines()
    for i, (x, y) in enumerate(zip(a, b), 1):
        if x != y:
            return [f'line {i}: audit {x.strip()[:60]!r} vs build {y.strip()[:60]!r}']
    return [f'length differs: {len(a)} vs {len(b)} lines']


# ---------------------------------------------------------------------------
# cc1 assembly parser
# ---------------------------------------------------------------------------

def _strip_comment(line):
    """Remove a trailing '#' comment outside string literals."""
    quote, k = False, 0
    while k < len(line):
        c = line[k]
        if c == '\\' and quote:
            k += 2
            continue
        if c == '"':
            quote = not quote
        elif c == '#' and not quote:
            return line[:k]
        k += 1
    return line


_SECTION_ALIASES = {'.text': '.text', '.data': '.data', '.rdata': '.rodata', '.sdata': '.sdata', '.bss': '.bss',
                    '.sbss': '.sbss', '.lit4': '.lit4', '.lit8': '.lit8', '.rodata': '.rodata'}
_ALLOWED_APP = [re.compile(p) for p in (
    r'\.section\s+\.text', r'\.section\s+\.rodata', r'\.section\s+\.data', r'\.section\s+\.sdata',
    r'\.set\s+noat', r'\.set\s+noreorder', r'\.set\s+reorder', r'\.set\s+at',
    # state restoration after accepted .s bodies that switch it (tu-ovl-phase2 review)
    r'\.set\s+macro', r'\.set\s+nomacro',
    # plain alignment, which the approved ACCEPTED_ASM scaffold emits (`.align 3`:
    # accepted blocks start 8-byte aligned while the tracked .s starts at .align 2).
    # Only the padding form: `.align n, fill` carries a fill operand, does not match
    # here and stays a data directive.
    r'\.align\s+\d+')]
_SCAFFOLD_INC = re.compile(r'\.include\s+"include/(?:labels|macro)\.inc"')
_EXTERN_LINE = re.compile(r'\.extern\s+[A-Za-z_.$][\w.$]*\s*,\s*\d+')


def _app_line_allowed(line):
    return bool(_SCAFFOLD_INC.fullmatch(line) or any(p.fullmatch(line) for p in _ALLOWED_APP)
                or re.fullmatch(r'\.include\s+"[^"]+\.s"', line))


# cc1's anonymous constant pool (varasm): what GCC 2.96 writes for the string,
# float and address-template constants of the NEXT function before that
# function starts. It opens with a bare section switch into a data section (a
# pool never continues in .text), holds only `.align N`, `$LCn:` labels and data
# directives with plain operands (one statement per line: no `;`, no
# parentheses), and cc1 then leaves app mode (`#NO_APP`) and returns to `.text`
# for the function. The grammar is only half of the rule: `AsmFile` records the
# lines it admits as `pool_lines`, and `top_level_asm` still reports every one
# of them that a top-level asm statement of the preprocessed unit writes
# (`asm_statement_lines`), so hand-written asm cannot pass as a pool.
_POOL_OPEN = re.compile(r'\.(?:rdata|sdata|lit4|lit8)')
_POOL_OPERAND = r'(?:-?(?:0x[0-9a-fA-F]+|\d+)|\$LC\d+|[A-Za-z_][\w.]*)(?:\s*[-+]\s*\d+)?'
_POOL_LINE = re.compile(
    r'\.(?:rdata|sdata|lit4|lit8)|\.section\s+\.(?:rodata|sdata|lit4|lit8)|\.align\s+\d+|\$LC\d+:'
    r'|\.ascii\s+"(?:[^"\\]|\\.)*"'
    r'|\.(?:word|half|byte|dword|space|gpword)\s+' + _POOL_OPERAND + r'(?:\s*,\s*' + _POOL_OPERAND + r')*')
_POOL_LABEL = re.compile(r'\$LC\d+:')


def _norm_asm_line(line):
    return ' '.join(line.split())


def asm_statement_lines(i_text):
    """{normalized line} of every asm statement string in a preprocessed unit.

    GCC writes a top-level asm statement verbatim into the assembly, one output
    line per line of its string, so any line of the .s that equals one of these
    may come from hand-written asm. Inline asm templates and asm labels are
    included too: over-inclusion can only report a cc1 line as stray, never admit
    a hand-written one."""
    out = set()
    for m in re.finditer(r'\b(?:__asm__|__asm|asm)\b(?:\s*(?:__volatile__|__volatile|volatile))?\s*\(', i_text):
        pos, parts = m.end(), []
        while True:
            ws = re.match(r'(?:\s+|^#[^\n]*\n)*', i_text[pos:], re.M)
            pos += ws.end()
            lit = re.match(r'"((?:\\.|[^"\\\n])*)"', i_text[pos:])
            if not lit:
                break
            try:
                parts.append(lit.group(1).encode('latin-1', 'replace').decode('unicode_escape'))
            except UnicodeDecodeError:
                parts.append(lit.group(1))
            pos += lit.end()
        for line in ''.join(parts).split('\n'):
            line = _norm_asm_line(_strip_comment(line))
            if line:
                out.add(line)
    return out


class AsmFile:
    """Regions, top-level asm blocks and data of a cc1 .s file."""

    def __init__(self, text):
        self.regions, self.top_app, self.top_data, self.sequence = [], [], [], []
        self.files, self.errors, self.defined_data, self.comms, self.externs = {}, [], [], [], []
        self.section, self.previous, self.current_file = '.text', '.text', None
        self.region, self.app = None, None
        # cc1 constant pools that closed a top-level #APP block (_cc1_pool_end),
        # and the line of the deferred #NO_APP that then closes nothing
        self.pool_lines, self.deferred_no_app = [], set()
        lines = text.splitlines()
        index = 0
        while index < len(lines):
            number, raw = index + 1, lines[index]
            index += 1
            stripped = raw.strip()
            if stripped == '#APP':
                if self.app is not None:
                    self.errors.append(f'line {number}: nested #APP')
                self.app = dict(line=number, lines=[], function=self.region['name'] if self.region else None)
                continue
            if stripped == '#NO_APP':
                if self.app is None and number in self.deferred_no_app:
                    self.deferred_no_app.discard(number)
                    continue
                if self.app is None:
                    self.errors.append(f'line {number}: #NO_APP without #APP')
                    continue
                (self.region['app'] if self.region else self.top_app).append(self.app)
                self.app = None
                continue
            code = _strip_comment(raw).strip()
            if self.app is not None:
                if not code:
                    continue
                # cc1 writes no final #NO_APP: an unterminated top-level block
                # ends at the first line that is not part of a scaffold template.
                # cc1 leaves app mode only when the next function starts, so the
                # constant pool it writes for that function before it lands inside
                # the block the preceding INCLUDE_ASM expansions opened; the block
                # ends where the pool starts, and the deferred #NO_APP closes nothing.
                pool_end = (self._cc1_pool_end(lines, index)
                            if self.region is None and not _app_line_allowed(code) else None)
                if pool_end is not None:
                    self.top_app.append(self.app)
                    self.app = None
                    self.deferred_no_app.add(pool_end)
                    for k in range(index - 1, pool_end - 1):
                        pool_code = _strip_comment(lines[k]).strip()
                        if pool_code:
                            self.pool_lines.append((k + 1, pool_code))
                elif self.region is None and not _app_line_allowed(code) and not self._app_open_legitimately(lines, index):
                    self.top_app.append(self.app)
                    self.app = None
                else:
                    self.app['lines'].append((number, code))
                    self._app_state(code, number)
                    continue
            if code:
                self._line(number, code)
        if self.region is not None:
            self.errors.append(f'unterminated region {self.region["name"]}')
        if self.app is not None:
            if self.region is None:
                self.top_app.append(self.app)
            else:
                self.errors.append('unterminated #APP')

    @staticmethod
    def _cc1_pool_end(lines, index):
        """Line number of the #NO_APP ending a cc1 constant pool that starts at line `index`, or None.

        A pool opens with a bare switch into a data section, holds only pool
        lines (_POOL_LINE) and at least one `$LCn:` label up to the next
        #NO_APP, and that #NO_APP is directly followed by cc1's `.text` for the
        function whose start ended app mode. Anything else - another #APP, a
        function start, a line outside the grammar, no `.text` - is not a pool,
        and the block is judged as before."""
        first = _strip_comment(lines[index - 1]).strip()
        if not _POOL_OPEN.fullmatch(first):
            return None
        labels = 0
        for k in range(index - 1, len(lines)):
            s = lines[k].strip()
            if s == '#NO_APP':
                following = next((_strip_comment(x).strip() for x in lines[k + 1:]
                                  if _strip_comment(x).strip()), None)
                return k + 1 if labels and following == '.text' else None
            if s == '#APP':
                return None
            code = _strip_comment(lines[k]).strip()
            if not code:
                continue
            if not _POOL_LINE.fullmatch(code):
                return None
            labels += bool(_POOL_LABEL.fullmatch(code))
        return None

    def _app_open_legitimately(self, lines, index):
        """True while a later #NO_APP still closes this block (inline asm)."""
        for raw in lines[index - 1:]:
            s = raw.strip()
            if s == '#NO_APP':
                return True
            if s == '#APP':
                return False
        return False

    def _app_state(self, code, number):
        m = re.match(r'\.section\s+"?([^\s",]+)', code)
        if m:
            self.previous, self.section = self.section, _SECTION_ALIASES.get(m.group(1), m.group(1))
        elif code in _SECTION_ALIASES:
            self.previous, self.section = self.section, _SECTION_ALIASES[code]
        m = re.match(r'\.include\s+"([^"]+)"', code)
        if m and self.region is None:
            self.sequence.append(('include', m.group(1), number, self.section))

    def _line(self, number, code):
        m = re.match(r'\.file\s+(\d+)\s+"([^"]*)"', code)
        if m:
            self.files[int(m.group(1))] = m.group(2)
            self.current_file = m.group(2)
            return
        m = re.match(r'\.section\s+"?([^\s",]+)', code)
        if m:
            self.previous, self.section = self.section, _SECTION_ALIASES.get(m.group(1), m.group(1))
        elif code in ('.previous',):
            self.previous, self.section = self.section, self.previous
        elif code in _SECTION_ALIASES:
            self.previous, self.section = self.section, _SECTION_ALIASES[code]
        elif code.startswith(('.pushsection', '.popsection', '.subsection')):
            self.errors.append(f'line {number}: unsupported section directive {code[:40]}')
        m = re.match(r'\.ent\s+(\S+)', code)
        if m:
            if self.region is not None:
                self.errors.append(f'line {number}: .ent {m.group(1)} inside {self.region["name"]}')
            self.region = dict(name=m.group(1), start=number, end=None, section=self.section,
                               file=self.current_file, labels=[], app=[], data=[], includes=[])
            self.sequence.append(('c', m.group(1), number, self.section))
            return
        m = re.match(r'\.end\s+(\S+)', code)
        if m:
            if self.region is None or self.region['name'] != m.group(1):
                self.errors.append(f'line {number}: unmatched .end {m.group(1)}')
            else:
                self.region['end'] = number
                self.regions.append(self.region)
                self.region = None
            return
        label = re.match(r'"?([A-Za-z_.$][\w.$]*)"?:', code)
        if label:
            if self.region is not None:
                self.region['labels'].append((label.group(1), self.section))
            elif not label.group(1).startswith('$') and label.group(1) not in MARKER_NAMES:
                self.defined_data.append(label.group(1))
        directive = code.split(None, 1)[0]
        operands = code[len(directive):].strip()
        if directive == '.include':
            if self.region is not None:
                self.region['includes'].append((number, operands))
            else:
                self.errors.append(f'line {number}: .include outside #APP')
        m = re.match(r'\.(l?comm)\s+([A-Za-z_.$][\w.$]*)\s*,\s*(\d+)(?:\s*,\s*(\d+))?', code)
        if m:
            self.defined_data.append(m.group(2))
            self.comms.append(dict(kind=m.group(1), name=m.group(2), size=int(m.group(3)),
                                   align=int(m.group(4)) if m.group(4) else None, line=number))
        m = re.match(r'\.extern\s+([A-Za-z_.$][\w.$]*)', code)
        if m:
            self.externs.append(m.group(1))
        # `.align n, fill` emits fill bytes; plain alignment does not.
        fill = operands.split(',')[1].strip() if operands.count(',') >= 1 else ''
        emits = directive in DATA_DIRECTIVES or (directive in ('.align', '.p2align', '.balign') and fill)
        if emits:
            entry = dict(line=number, directive=directive, operands=operands, section=self.section)
            (self.region['data'] if self.region else self.top_data).append(entry)


# ---------------------------------------------------------------------------
# Preprocessed unit (tu.i)
# ---------------------------------------------------------------------------

class Preprocessed:
    """Line markers of a cpp output: which file and line every offset came from."""

    MARKER = re.compile(r'^#\s*(\d+)\s+"([^"]*)"(.*)$', re.M)

    def __init__(self, text):
        self.text = text
        self.markers = []  # (offset after the marker line, file, line)
        for m in self.MARKER.finditer(text):
            end = text.find('\n', m.end())
            end = len(text) if end < 0 else end + 1
            self.markers.append((end, m.group(2), int(m.group(1))))

    def files(self):
        out = []
        for _, name, _line in self.markers:
            if name.startswith('<') or name in out:
                continue
            out.append(name)
        return out

    def attribute(self, offset):
        """(file, line) of a text offset, following the markers."""
        best = None
        for start, name, line in self.markers:
            if start <= offset:
                best = (start, name, line)
            else:
                break
        if best is None:
            return None, None
        start, name, line = best
        return name, line + self.text.count('\n', start, offset)


# ---------------------------------------------------------------------------
# Scaffold headers and macro includes
# ---------------------------------------------------------------------------

def _blank_comments(text):
    return re.sub(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0][0] in '"\'' else ''.join('\n' if c == '\n' else ' ' for c in m[0]),
                  text, flags=re.S)


def _logical_lines(text):
    return re.sub(r'\\\r?\n', ' ', _blank_comments(text)).splitlines()


def _asm_template(body, params):
    """Decode __asm__(...) built from string literals and FOLDER/#NAME tokens."""
    m = re.fullmatch(r'\s*__asm__\s*\((.*)\)\s*;?\s*', body, re.S)
    if not m:
        return None
    out, rest = [], m.group(1).strip()
    while rest:
        lit = re.match(r'"((?:\\.|[^"\\])*)"\s*', rest)
        if lit:
            out.append(bytes(lit.group(1), 'ascii').decode('unicode_escape'))
            rest = rest[lit.end():]
            continue
        tok = re.match(r'(#?)\s*([A-Za-z_]\w*)\s*', rest)
        if tok and tok.group(2) in params:
            out.append('<' + params[tok.group(2)] + '>')
            rest = rest[tok.end():]
            continue
        return None
    return ''.join(out)


def _template_ok(macro, template):
    """The expansion must stay in .text (code) or a data section (rodata)."""
    want_text = macro in ('INCLUDE_ASM', 'ACCEPTED_ASM')
    section, included = None, False
    for raw in template.split('\n'):
        line = raw.strip()
        if not line:
            continue
        if line == '.include "<F>/<N>.s"':
            if section is None:
                return False, 'the payload is included before a section is selected'
            if want_text and section not in TEXT_SECTIONS:
                return False, f'{macro} includes code into {section}'
            if not want_text and section in TEXT_SECTIONS:
                return False, f'{macro} includes data into {section}'
            included = True
            continue
        m = re.fullmatch(r'\.section\s+([^\s,]+)', line)
        if m:
            if section is None and ((m.group(1) in TEXT_SECTIONS) != want_text):
                return False, f'{macro} starts in {m.group(1)}'
            section = m.group(1)
            continue
        if any(p.fullmatch(line) for p in _ALLOWED_APP):
            continue
        return False, f'unexpected line in the {macro} template: {line[:40]}'
    return (included, None if included else f'the {macro} template includes nothing')


def scaffold_header_ok(text):
    """(ok, reason): a header that only defines the INCLUDE_ASM family."""
    for line in _logical_lines(text):
        s = line.strip()
        if not s:
            continue
        if re.fullmatch(r'#\s*(?:if|ifdef|ifndef|elif)\b[^"\'{};]*', s) or re.fullmatch(r'#\s*(?:else|endif)\s*', s):
            continue
        if re.fullmatch(r'#\s*define\s+[A-Za-z_]\w*(?:\s+\d+)?\s*', s):
            continue
        if re.fullmatch(r'#\s*include\s+"(?:include_asm\.h|common\.h)"\s*', s):
            continue
        m = re.fullmatch(r'#\s*define\s+(INCLUDE_ASM|ACCEPTED_ASM|INCLUDE_RODATA)\s*\(\s*([A-Za-z_]\w*)\s*,'
                         r'\s*([A-Za-z_]\w*)\s*\)(.*)', s, re.S)
        if m:
            if not m.group(4).strip():
                continue
            if re.fullmatch(r'\s*(?:INCLUDE_ASM|ACCEPTED_ASM|INCLUDE_RODATA)\s*\(\s*' + re.escape(m.group(2))
                            + r'\s*,\s*' + re.escape(m.group(3)) + r'\s*\)\s*', m.group(4)):
                continue  # ACCEPTED_ASM delegating to INCLUDE_ASM with the same parameters
            template = _asm_template(m.group(4), {m.group(2): 'F', m.group(3): 'N'})
            if template is None:
                return False, f'{m.group(1)} is not a plain __asm__ template'
            ok, reason = _template_ok(m.group(1), template)
            if not ok:
                return False, reason
            continue
        template = _asm_template(s, {})
        if template is not None and all(not a.strip() or _SCAFFOLD_INC.fullmatch(a.strip())
                                        for a in template.split('\n')):
            continue
        return False, f'unexpected line in the scaffold header: {s[:60]}'
    return True, None


DEBUG_BREAK_HEADER_LINES = ['#ifndef DEBUG_BREAK_H', '#define DEBUG_BREAK_H',
                            '#define DEBUG_BREAK() __asm__ __volatile__("break")', '#endif']


def debug_break_header_ok(text):
    """(ok, reason): include/debug_break.h, the one header that spells the
    `ee-assert-break` trap (docs/ee-hand-asm.md): its guard and the single
    DEBUG_BREAK() definition, nothing else. A function only gains the trap by
    declaring `ee-assert-break`; `audit_c` refuses its #APP block otherwise."""
    code = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', text, flags=re.S)
    lines = [re.sub(r'\s+', ' ', line).strip() for line in code.splitlines()]
    lines = [line for line in lines if line]
    if lines != DEBUG_BREAK_HEADER_LINES:
        return False, 'not the debug_break.h DEBUG_BREAK() definition'
    return True, None


def macro_include_ok(text):
    """(ok, reason): only the scaffold macros, guards and comments at top level."""
    depth, names = 0, []
    for raw in text.splitlines():
        s = _strip_comment(raw).strip()
        if not s:
            continue
        m = re.match(r'\.macro\s+([A-Za-z_.$][\w.$]*)', s)
        if m:
            depth += 1
            names.append(m.group(1))
            continue
        if s.startswith('.endm'):
            depth -= 1
            continue
        if depth:
            continue
        if re.fullmatch(r'\.(?:ifndef|ifdef)\s+\w+|\.endif|\.else|\.internal\s+\w+|\.set\s+\w+\s*,\s*\d+', s):
            continue
        return False, f'top-level content in the macro include: {s[:60]}'
    if depth:
        return False, 'unbalanced .macro/.endm'
    extra = sorted(set(names) - SCAFFOLD_MACROS)
    if extra:
        return False, f'macros outside the scaffold set (a macro can shadow a mnemonic): {extra}'
    return True, None


def asm_data_only(text):
    """(ok, reason): an INCLUDE_RODATA payload holds data, never code."""
    section = None
    for number, raw in enumerate(text.splitlines(), 1):
        line = _strip_comment(raw).strip()
        if not line or re.fullmatch(r'"?[A-Za-z_.$][\w.$]*"?:', line):
            continue
        if line.startswith('.'):
            directive = line.split(None, 1)[0]
            if directive in ('.section', '.text', '.data', '.rdata', '.sdata', '.rodata', '.bss', '.sbss',
                             '.lit4', '.lit8'):
                name = line.split()[1] if directive == '.section' else directive
                if _SECTION_ALIASES.get(name, name) in TEXT_SECTIONS:
                    return False, f'line {number}: payload switches to {name}'
                section = name
                continue
            if directive in DATA_DIRECTIVES or directive in ('.align', '.globl', '.global', '.type', '.size',
                                                             '.set', '.local', '.weakext', '.extern'):
                continue
            return False, f'line {number}: unsupported directive {directive}'
        return False, f'line {number}: instruction in a data payload: {line[:40]}'
    del section
    return True, None


# ---------------------------------------------------------------------------
# Admission (tools/match.py)
# ---------------------------------------------------------------------------

def admission(cls, tu_class=None):
    """The admission for a function of class `cls` inside a TU of class `tu_class`.

    `tu_class` matters for exactly one pair. A `hardware` function is admitted
    as the TU's prelude plus its own body (see `audit_c`), and in a TU declared
    `native-ti` that prelude carries the file-scope
    `typedef unsigned int N __attribute__((mode(TI)));`, which plain
    `match.hardware_c` refuses through `ordinary_c`. `match.hardware_native_ti_c`
    is the two already-authorized admissions composed -- the calibrated hardware
    table for the inline statements, the native-TI grammar (itself `ordinary_c`
    plus that one typedef and several extra refusals) for everything else -- and
    it admits no construct that one of the two routes does not admit on its own.
    The composition is reachable ONLY here: a `hardware` function in an
    `ordinary` TU still gets `hardware_c`, so the typedef stays refused there,
    and a `native-ti` function never gains inline assembly (`audit_c` refuses an
    `#APP` block in any non-`hardware` function, and `native_ti.source` refuses
    the `asm` token). See docs/native-ti.md.
    """
    from tools import match
    if cls == 'ordinary':
        return match.ordinary_c
    if cls == 'hardware':
        return match.hardware_native_ti_c if tu_class == 'native-ti' else match.hardware_c
    if cls == 'runtime-verbatim':
        return match.runtime_verbatim_c
    if cls == 'native-ti':
        from tools import native_ti
        return native_ti.source
    if cls == 'gnu-nested-function':
        # This admits evidenced GNU nested definitions project-wide. It does not alter the TU contract or
        # the ordinary admission used for file-scope text and headers.
        return match.gnu_nested_c
    if cls == 'ee-mmi':
        # Constrained inline EE MMI (user decision 2026-10-10, docs/ee-mmi.md): the closed MMI grammar
        # for the inline blocks, ordinary (or native-TI) C for everything else. Not a precedent for
        # non-MMI assembly; the COP2/VU route stays `hardware`.
        return match.mmi_native_ti_c if tu_class == 'native-ti' else match.mmi_c
    if cls == 'ee-asm-leaf':
        # Original hand-written block-copy loop as C with asm (user decision 2026-10-10,
        # docs/ee-hand-asm.md): a leaf whose only statement is one admitted loop block.
        return match.asm_leaf_native_ti_c if tu_class == 'native-ti' else match.asm_leaf_c
    if cls == 'ee-fpu':
        # Inline COP1 transfer/convert blocks (user decision 2026-10-10, docs/ee-hand-asm.md).
        return match.fpu_native_ti_c if tu_class == 'native-ti' else match.fpu_c
    if cls == 'ee-vu-clip':
        # Original hand-written VU0 lane-test block with its set/clear join as the whole body
        # (user decision 2026-10-11 on main/tu109, docs/ee-hand-asm.md).
        return match.vu_clip_native_ti_c if tu_class == 'native-ti' else match.vu_clip_c
    if cls == 'ee-assert-break':
        # The assert macros' bare debugger trap, spelled only as DEBUG_BREAK() of
        # include/debug_break.h (user decision 2026-10-11 on main/tu121, docs/ee-hand-asm.md).
        return match.assert_break_native_ti_c if tu_class == 'native-ti' else match.assert_break_c
    raise AuditError(f'unknown source class {cls!r}')


def included_names(text):
    return re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]', text, re.M)


def asm_contract_problems(path):
    """Read an asm contract and list what is wrong with it, never raising.

    The asm contract is OPTIONAL review documentation (user direction
    2026-09-27): the bytes are proven by the whole-file gate and this audit, and
    the automatic guard against illegitimate inline assembly is the source-class
    admission (`admission`, tools/match.py `hardware_c`), which stays enforced.
    Returns (data or None, [problem, ...]); `data` is None only when the file is
    missing or not a JSON object.  Callers record the problems as notes.
    """
    problems = []
    p = Path(path)
    if not p.is_file():
        return None, [f'asm contract {rel(p) if p.is_absolute() else p} is missing']
    try:
        data = json.loads(p.read_text())
    except (OSError, ValueError) as e:
        return None, [f'asm contract {rel(p.resolve())} is unreadable: {e}']
    if not isinstance(data, dict):
        return None, [f'asm contract {rel(p.resolve())} is not a JSON object']
    missing = [k for k in ASM_CONTRACT_FIELDS if not (isinstance(data.get(k), str) and data[k].strip())]
    if missing:
        problems.append(f'asm contract lacks {sorted(missing)}')
    evidence = data.get('evidence')
    if not (isinstance(evidence, dict) and set(evidence) == {'path', 'sha256'}):
        problems.append('asm contract evidence must be {path, sha256}')
    else:
        target = ROOT / str(evidence['path'])
        if not target.is_file() or sha256_file(target) != evidence['sha256']:
            problems.append('asm contract evidence changed or missing')
    lines = data.get('top_level_asm') or []
    if not isinstance(lines, list) or any(not isinstance(x, str) for x in lines):
        problems.append('asm contract top_level_asm must be a list of exact assembly lines (ignored)')
        data = dict(data, top_level_asm=[])
    return data, problems


def check_asm_contract(path):
    """Strict form of `asm_contract_problems` (raises on the first problem).

    Kept for callers that want a hard validation of a document they are about to
    write; the audit itself only uses the non-raising form.
    """
    data, problems = asm_contract_problems(path)
    if problems:
        raise AuditError(problems[0])
    return data


def load_asm_contract(path, notes, label=None):
    """Non-raising load for the audit: problems go to `notes`, never to a check."""
    data, problems = asm_contract_problems(path)
    for p in problems:
        notes.append(f'{label or rel(Path(path).resolve())}: {p}')
    return data


def owned_header_source_class(full, default, root, metadata):
    """Select only a pinned, defining-owner native-TI declaration header.

    This changes no body class or grammar. Unlisted headers retain their
    importing TU class; listed text still passes native_ti.source in full.
    """
    try:
        key = Path(full).resolve().relative_to(Path(root).resolve()).as_posix()
    except ValueError:
        return default, {}
    staged_copy = {}
    registered = metadata.get('owned_header_classes') or {}
    parts = Path(key).parts
    if parts[:3] == ('build', 'main', 'include'):
        canonical_key = (Path('include') / Path(*parts[3:])).as_posix()
        if canonical_key in registered:
            staged_copy = dict(path=key, canonical_header=canonical_key)
            key = canonical_key
    entry = registered.get(key)
    if entry is None:
        return default, {}
    if metadata.get('schema') != SOURCE_CLASSES_SCHEMA or entry.get('source_class') != 'native-ti':
        raise ValueError('owned header dialect must be the registered native-TI class')
    owner = entry.get('owner_tu')
    if ((metadata.get('tus') or {}).get(owner) or {}).get('source_class') != 'native-ti':
        raise ValueError('owned native-TI header lacks a native-TI defining TU')
    rows = json.loads((Path(root) / 'config/tu-build.json').read_text()).get('tus') or []
    row = next((r for r in rows if r.get('id') == owner), None)
    if row is None:
        raise ValueError('owned native-TI header has no original TU identity')
    source = Path(row['path'])
    expected = (Path('include') / source.parent.relative_to('src') / source.with_suffix('.h').name).as_posix()
    if key != expected:
        raise ValueError('owned native-TI header is not its normal defining-owner header')
    origin = entry.get('source_owner') or {}
    if origin.get('path') not in (source.as_posix(), source.with_suffix('.h').as_posix()):
        raise ValueError('owned native-TI declaration source is outside its defining TU')
    source_path = Path(root) / origin['path']
    if (sha256_file(source_path) != origin.get('sha256')
            or sha256_file(Path(root) / key) != entry.get('header_sha256')
            or sha256_file(full) != entry.get('header_sha256')):
        raise ValueError('owned native-TI source/header pin changed')
    exports = json.loads((Path(root) / 'config/header-exports.json').read_text()).get('exports') or []
    names = set(entry.get('names') or [])
    if not names or not any(r.get('tu') == owner and names <= set(r.get('names') or [])
            and (r.get('source_owner') or {}).get('path') == origin['path']
            and (r.get('source_owner') or {}).get('sha256') == origin['sha256'] for r in exports):
        raise ValueError('owned native-TI header lacks its existing source-bound type export')
    return 'native-ti', dict(owner_tu=owner, source_owner=origin, header_sha256=entry['header_sha256'], names=sorted(names), staged_copy=staged_copy)


def emitted_c_category(cls, source_statements, emitted_blocks):
    """Report admitted inlined helper ASM as ASM, never pure C coverage."""
    return ('exact_c_with_asm' if cls in INLINE_ASM_CLASSES
            and (source_statements or emitted_blocks) else 'exact_c')


def published_source_classes(tu_id):
    """The source class a published TU's functions were accepted under.

    `--function-class`/`--asm-contract` state a class at the moment a worker
    submits; nothing of that survives publication, so a fresh audit of the whole
    TU would read an `exact_c_with_asm` function as ordinary C and refuse its
    inline assembly.  `config/tu/source-classes.json` is where the class
    survives: per TU, and per function inside it, the class and the asm contract
    it was accepted with, pinned by SHA-256.  The audit reads it whenever it
    knows which TU it is auditing, so a whole-TU audit reproduces the classes
    without anyone having to restate them, and a later worker who did not
    allocate the function cannot lose them.

    Returns (tu_class, {name: class}, {name: contract path}, provenance) and
    raises when the file or the class is wrong: an unreadable declaration is
    never silently treated as "ordinary".  Asm contracts are optional review
    documentation (user direction 2026-09-27): an absent, lost or re-hashed one
    is reported in the provenance (`asm_contracts_missing`,
    `asm_contract_notes`) and never raises.
    """
    empty = (None, {}, {}, None)
    path = ROOT / SOURCE_CLASSES_FILE
    if not tu_id or not path.is_file():
        return empty
    data = json.loads(path.read_text())
    if data.get('schema') != SOURCE_CLASSES_SCHEMA:
        raise AuditError(f'{SOURCE_CLASSES_FILE} is not schema {SOURCE_CLASSES_SCHEMA}')
    entry = (data.get('tus') or {}).get(tu_id)
    prov = dict(path=SOURCE_CLASSES_FILE, sha256=sha256_file(path), declared=bool(entry))
    discovered = tu_edit.gnu_nested_specs(ROOT, tu_id)
    if not entry and not discovered:
        return None, {}, {}, prov
    entry = entry or {}

    # The asm contract is optional review documentation that never blocks (user
    # direction 2026-09-27): an absent, lost, unsafe or re-hashed contract is
    # recorded here as a note and the audit goes on.  The source class itself
    # (and hence the hardware admission) stays mandatory and validated.
    missing_contracts = {}
    contract_notes = []

    def one(where, spec):
        cls = spec.get('source_class')
        if cls not in CLASSES:
            raise AuditError(f'{SOURCE_CLASSES_FILE} {where}: invalid source_class {cls!r}')
        contract = spec.get('asm_contract')
        key = where[len(tu_id) + 1:] if where != tu_id else '*'
        if contract is None:
            if cls == 'hardware':
                contract_notes.append(f'{where}: hardware class declared without an asm_contract '
                                      '(optional review documentation)')
            return cls, None
        try:
            if not isinstance(contract, dict):
                raise AuditError('asm_contract is not {path, sha256}')
            rel_path = safe_relative(contract.get('path'), 'asm contract')
        except (AuditError, TypeError, ValueError) as e:
            contract_notes.append(f'{where}: unusable asm_contract declaration ignored: {e}')
            return cls, None
        target = ROOT / rel_path
        if not target.is_file():
            # A contract whose file was lost (e.g. the attempt directory it lived
            # in was cleaned up).  Its function's bytes are still verified: record
            # it and waive it visibly.
            missing_contracts[key] = rel_path
            return cls, None
        digest = sha256_file(target)
        if digest != contract.get('sha256'):
            contract_notes.append(f'{where}: {rel_path} is sha256 {digest[:12]}..., the declaration pins '
                                  f'{str(contract.get("sha256"))[:12]}... (recorded, not blocking)')
        return cls, rel_path

    tu_class, tu_contract = (one(tu_id, entry) if entry.get('source_class') else (None, None))
    if tu_class == 'gnu-nested-function':
        raise AuditError(f'{SOURCE_CLASSES_FILE} {tu_id}: gnu-nested-function is a function-level class '
                         'attached to an enclosing function, not a whole-TU class')
    classes, contracts, nested_functions = {}, {}, {}
    for name, spec in sorted((entry.get('functions') or {}).items()):
        cls, contract = one(f'{tu_id} {name}', spec)
        classes[name] = cls
        if contract:
            contracts[name] = contract
        nested = spec.get('nested_functions')
        if nested is not None:
            if cls != 'gnu-nested-function':
                raise AuditError(f'{SOURCE_CLASSES_FILE} {tu_id} {name}: nested_functions requires '
                                 'source_class gnu-nested-function')
            nested_functions[name] = nested
        elif cls == 'gnu-nested-function':
            if name not in discovered:
                raise AuditError(f'{SOURCE_CLASSES_FILE} {tu_id} {name}: gnu-nested-function needs '
                                  'an evidenced nested_functions mapping')
    for name, spec in discovered.items():
        nested_functions.setdefault(name, spec.get('nested_functions'))
    seen = set()
    for name, nested in nested_functions.items():
        if not isinstance(nested, list) or not nested:
            raise AuditError(f'{tu_id} {name}: nested_functions must be a nonempty list')
        lexical_names = {name} | {item.get('source_name') for item in nested if isinstance(item, dict)}
        for item in nested:
            required = ('source_name', 'original_name', 'parent', 'binding', 'type', 'va', 'size')
            if not isinstance(item, dict) or any(k not in item for k in required):
                raise AuditError(f'{tu_id} {name}: incomplete nested function identity')
            if (not isinstance(item['source_name'], str) or not re.fullmatch(r'[A-Za-z_]\w*', item['source_name'])
                    or not isinstance(item['original_name'], str) or not item['original_name']
                    or not isinstance(item['parent'], str)
                    or item['parent'] not in lexical_names or item['parent'] == item['source_name']
                    or item['binding'] != 'LOCAL' or item['type'] != 'FUNC'
                    or not isinstance(item['va'], str) or not re.fullmatch(r'0x[0-9a-f]{8}', item['va'])
                    or type(item['size']) is not int or item['size'] <= 0):
                raise AuditError(f'{tu_id} {name}: invalid nested function identity or lexical parent')
            if item['original_name'] in seen:
                raise AuditError(f'{tu_id}: duplicate nested identity {item["original_name"]}')
            seen.add(item['original_name'])
    discovery_path = ROOT / tu_edit.GNU_NESTED_REGISTRY
    if discovery_path.is_file() and discovered:
        prov['nested_discovery'] = dict(path=tu_edit.GNU_NESTED_REGISTRY, sha256=sha256_file(discovery_path))
    prov.update(tu_source_class=tu_class, functions={n: classes[n] for n in sorted(classes)},
                asm_contracts={n: dict(path=contracts[n], sha256=sha256_file(ROOT / contracts[n]))
                               for n in sorted(contracts)})
    if missing_contracts:
        prov['asm_contracts_missing'] = dict(sorted(missing_contracts.items()))
    if contract_notes:
        prov['asm_contract_notes'] = contract_notes
    if nested_functions:
        prov['nested_functions'] = nested_functions
    if tu_contract:
        contracts[None] = tu_contract
    return tu_class, classes, contracts, prov


# ---------------------------------------------------------------------------
# Toolchain steps
# ---------------------------------------------------------------------------

def run(argv, cwd):
    env = dict(PATH='/usr/bin:/bin', LC_ALL='C')
    proc = subprocess.Popen([str(a) for a in argv], cwd=str(cwd), env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, start_new_session=True)
    try:
        out, _ = proc.communicate(timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate()
        raise AuditError(f'timeout after {TIMEOUT}s: {argv[0]}')
    return proc.returncode, out.decode('utf-8', 'replace')


def registry_contract(contract_id):
    path = ROOT / 'config/toolchains-tu.json'
    if not path.is_file():
        raise AuditError('config/toolchains-tu.json is missing')
    data = json.loads(path.read_bytes())
    entry = (data.get('contracts') or {}).get(contract_id)
    if not entry:
        raise AuditError(f'contract {contract_id!r} is not in config/toolchains-tu.json')
    return entry, data


# KOS-MOS 2026-09-27: opt-in, mirrors tools/tu/toolchain.py; Opok to approve.
# A source-built tool of config/toolchain-identity.json (the la29 ee-as, the plain
# 2.9 ee-as, ps2dev binutils, dvp-as) is not bit-reproducible across host compilers.
# tools/tu/toolchain.py accepts such a rebuilt payload when XENO_TOOLCHAIN_ALLOW
# names its tool id; the audit does the same for the contract ASSEMBLER only, and
# every report says so (`contract_relaxed`, top level and in `summary`): the
# whole-file gate still decides whether the rebuilt assembler emits the original bytes.
TOOLCHAIN_IDENTITY = 'config/toolchain-identity.json'


def toolchain_allow():
    """The tool ids XENO_TOOLCHAIN_ALLOW names (the same set tools/tu/toolchain.py reads)."""
    return set(filter(None, os.environ.get('XENO_TOOLCHAIN_ALLOW', '').split(',')))


def toolchain_tool_id(payload):
    """The config/toolchain-identity.json tool id whose payload the contract path
    `payload` (under .work/toolchains) names, or None when no tool owns it."""
    try:
        tools = json.loads((ROOT / TOOLCHAIN_IDENTITY).read_bytes())['tools']
    except (OSError, ValueError, KeyError, TypeError):
        return None
    payload = Path(payload).as_posix()
    for tool_id, entry in tools.items():
        names = ([entry['path']] if 'path' in entry
                 else [f'{entry.get("dir")}/{name}' for name in entry.get('files', {})])
        if any(payload.endswith('/' + name) for name in names):
            return tool_id
    return None


def resolve_contract(args, tu_record):
    """The TU's compile contract from the manifests, with every pin verified.

    Returns (contract, issues, relaxed). An override that differs from the manifest and
    any unverifiable pin is an issue, i.e. a failing audit; `relaxed` lists the
    assembler pin mismatch XENO_TOOLCHAIN_ALLOW admitted ([{tool, path, expected, actual}]).
    """
    issues, relaxed = [], []
    manifest = (tu_record or {}).get('contract') or {}
    if not manifest.get('id'):
        raise AuditError('config/tu-build.json has no contract for this TU (pass --tu-id)')
    entry, registry = registry_contract(manifest['id'])
    include_asm = registry.get('include_asm') or {}
    compiler_dir = ROOT / entry['compiler']['dir']
    contract = dict(id=manifest['id'], source='config/tu-build.json + config/toolchains-tu.json',
                    compiler=dict(dir=entry['compiler']['dir'], sha256=dict(entry['compiler']['sha256'])),
                    driver=entry.get('driver', 'ee-gcc'), driver_flags=list(entry.get('driver_flags', [])),
                    flags=list(entry['flags']),
                    assembler=dict(path=entry['assembler']['path'], sha256=entry['assembler']['sha256'],
                                   argv=entry['assembler'].get('argv', '-EL -m5900 -mabi=eabi <G> -I .'),
                                   flags=list(entry['assembler'].get('flags', []))),
                    include_asm=include_asm)
    if list(manifest.get('flags') or []) != contract['flags']:
        issues.append(f'config/tu-build.json flags {manifest.get("flags")} differ from contract '
                      f'{contract["id"]} flags {contract["flags"]}')
    if manifest.get('cc1_sha256') and manifest['cc1_sha256'] != contract['compiler']['sha256'].get('cc1'):
        issues.append('config/tu-build.json cc1 digest differs from the contract registry')
    if manifest.get('assembler_sha256') and manifest['assembler_sha256'] != contract['assembler']['sha256']:
        issues.append('config/tu-build.json assembler digest differs from the contract registry')
    for name, digest in contract['compiler']['sha256'].items():
        path = compiler_dir / name
        if not path.is_file() or sha256_file(path) != digest:
            issues.append(f'compiler payload pin mismatch: {rel(path)}')
    asm_path = ROOT / contract['assembler']['path']
    asm_found = sha256_file(asm_path) if asm_path.is_file() else None
    if asm_found != contract['assembler']['sha256']:
        # KOS-MOS 2026-09-27: opt-in, mirrors tools/tu/toolchain.py; Opok to approve.
        # Only the tool id XENO_TOOLCHAIN_ALLOW names is relaxed; a missing payload
        # or any other tool's mismatch fails the contract check as before.
        tool_id = toolchain_tool_id(contract['assembler']['path'])
        if asm_found and tool_id in toolchain_allow():
            relaxed.append(dict(tool=tool_id, path=rel(asm_path),
                                expected=contract['assembler']['sha256'], actual=asm_found))
        else:
            issues.append(f'assembler pin mismatch: {rel(asm_path)}')
    # Overrides are compared with the manifest contract, never trusted.
    override = {}
    if args.contract:
        override = json.loads(Path(args.contract).read_text())
    if args.cc_dir:
        override.setdefault('compiler', {})['dir'] = args.cc_dir
    if args.assembler:
        override.setdefault('assembler', {})['path'] = args.assembler
    if args.flags:
        override['flags'] = args.flags.split()
    if override:
        got_dir = (override.get('compiler') or {}).get('dir')
        if got_dir and Path(got_dir).resolve() != compiler_dir.resolve():
            issues.append(f'compiler directory override {got_dir} is not the contract compiler')
        if 'flags' in override and list(override['flags']) != contract['flags']:
            issues.append(f'flag override {override["flags"]} is not the contract {contract["flags"]}')
        got_as = (override.get('assembler') or {}).get('path')
        if got_as and Path(got_as).resolve() != asm_path.resolve():
            issues.append(f'assembler override {got_as} is not the contract assembler')
        argv = (override.get('assembler') or {}).get('argv')
        if argv and argv.split() != contract['assembler']['argv'].split():
            issues.append(f'assembler argv override {argv!r} is not the contract argv')
    contract['include_dirs'] = (args.include_dir or (override.get('include_dirs') if override else None)
                                or ['include'])
    contract['overrides'] = override or None
    return contract, issues, relaxed


def gflag(contract):
    g = [f for f in contract['flags'] if re.fullmatch(r'-G\d+', f)]
    if len(g) != 1:
        raise AuditError('the contract flags must carry exactly one -G flag')
    return g[0]


def assembler_argv(contract, out, src, extra_includes=()):
    spec = contract['assembler']['argv'].replace('<G>', gflag(contract)).split()
    argv = [ROOT / contract['assembler']['path'], *spec]
    for inc in extra_includes:
        argv += ['-I', str(inc)]
    return argv + ['-o', str(out), str(src)]


# ---------------------------------------------------------------------------
# The assembly step of the real build edge
# ---------------------------------------------------------------------------
#
# cpp and cc1 are spelled the same wherever a TU is built, but the **assembly**
# step is not.  A main C TU is assembled from a per-TU overlay directory that
# `tools/tu/cinc.py` fills first: it drops every `.extern SYM, N` line of the
# scaffold `.s` bodies the TU includes whose SYM the TU's own compiler output
# already declares or defines (tu-asmcompat review finding F3), and the overlay
# is searched before the unit files.  Assembling the same cc1 output from the
# unit directory instead -- the bare cpp/cc1/as sequence this audit used to run --
# produces a **different object**: a symbol whose scaffold `.extern` line the
# build dropped is entered in the symbol table where the C output declares it
# rather than where the scaffold line did, so `.symtab` and `.strtab` move and
# every symbol index in `.rel.text` moves with them.  No instruction changes and
# the link is unaffected, but the bytes are not identical, and
# `build_object_matches_audit` -- the check that binds the audited source to the
# object the whole-file gate actually links -- then failed on TUs whose source
# was exactly right.  Fourteen of main's sixty-three published C TUs were in
# that state (main/tu211 `sef` among them), and with them every
# `tools/worker.py` submission on those TUs, because `tu_citable_forms()`
# requires the audit's top-level `ok`.
#
# So the recipe is not assumed: the rule that produces `--obj` is read from the
# unit's own `build.ninja` and reproduced.  The table below mirrors
# `tools/tu/ninja_main.py` (rules `ccas`, `ccas_c`) and `tools/tu/cc_tu.sh`
# (rule `cc`); it is not a second source of truth, because the audit's object is
# still compared with the build's byte for byte, so a recipe that drifted from
# the generator would show up as a failure and can never mask a real mismatch.
# Nothing here normalises a difference away: the comparison is unchanged.
#
# The audit builds its **own** overlay from its **own** cc1 output instead of
# reusing the build's `build/cinc/<tu>`: the compile stays self-contained, so a
# stale or hand-edited overlay in the build tree can never certify anything, and
# the build's overlay is still validated -- transitively, because the two
# objects are compared.


def _ninja_document(unit_dir):
    """{'rules': {name: vars}, 'edges': {output: vars incl. 'rule'}} of build.ninja."""
    path = Path(unit_dir) / 'build.ninja'
    if not path.is_file():
        return None
    rules, edges, section = {}, {}, None
    for line in path.read_text(errors='replace').splitlines():
        if line[:1] in (' ', '\t'):
            if section is not None and '=' in line:
                key, _, value = line.strip().partition('=')
                section[key.strip()] = value.strip()
            continue
        match = re.match(r'rule (\S+)\s*$', line)
        if match:
            section = rules.setdefault(match.group(1), {})
            continue
        match = re.match(r'build ([^:]+):\s*(\S+)(.*)', line)
        if match:
            section = dict(rule=match.group(2), inputs=match.group(3).split('|', 1)[0].split())
            for out in match.group(1).split():
                edges[out] = section
            continue
        section = None
    return dict(rules=rules, edges=edges)


def build_assembly_recipe(unit_dir, obj):
    """How the unit's build assembles this TU, read from its build.ninja edge.

    Returns None when there is no build edge to read (no build.ninja, no --obj,
    or an output no edge produces); the caller then keeps the contract's bare
    `as -I .` in the unit directory and the report says so.
    """
    if obj is None:
        return None
    unit_dir = Path(unit_dir).resolve()
    document = _ninja_document(unit_dir)
    if document is None:
        return None
    try:
        target = os.path.relpath(Path(obj).resolve(), unit_dir)
    except ValueError:
        return None
    edge = document['edges'].get(target)
    if edge is None:
        return None
    rule = edge['rule']
    if rule == 'alloccommon':
        # Native MAIN first assembles the C TU, then uses the standard
        # relocatable link to allocate COMMON. Reproduce both steps rather
        # than comparing the final object with an unallocated compiler object.
        inputs = edge.get('inputs') or []
        command = (document['rules'].get(rule) or {}).get('command', '')
        expected = ('timeout -k 5 30 $ld -r -d -EL -m elf32lr5900 -o $obj $in && '
                    'timeout -k 5 30 cp $in.s $obj.s && timeout -k 5 30 cp $in.i $obj.i')
        if len(inputs) != 1 or command != expected or edge.get('obj') != target:
            raise AuditError('unsupported native MAIN COMMON-allocation recipe')
        raw = unit_dir / inputs[0]
        raw_edge = document['edges'].get(inputs[0]) or {}
        if raw_edge.get('rule') != 'ccas_c':
            raise AuditError('native MAIN COMMON allocation must follow the C compiler edge')
        linker_lines = re.findall(r'^ld\s*=\s*(.+)$', (unit_dir/'build.ninja').read_text(), re.M)
        if len(linker_lines) != 1:
            raise AuditError('native MAIN linker is not uniquely declared')
        linker_argv = linker_lines[0].split()
        if linker_argv[:4] == ['timeout', '-k', '5', '30']:
            linker_argv = linker_argv[4:]
        if len(linker_argv) != 1 or not Path(linker_argv[0]).is_absolute() or not Path(linker_argv[0]).is_file():
            raise AuditError('native MAIN linker declaration is not one absolute executable')
        recipe = build_assembly_recipe(unit_dir, raw)
        if recipe is None:
            raise AuditError('native MAIN raw compiler recipe is missing')
        recipe['common_allocation'] = dict(raw_object=str(raw), output_object=str(Path(obj).resolve()),
            linker=linker_argv[0], linker_sha256=sha256_file(linker_argv[0]))
        return recipe
    if rule == 'ccas_c':
        # tools/tu/ninja_main.py: cinc.py --root . --dst $overlay, then
        # `cd $overlay && ee-as ... -I . -I <unit_dir> -I <unit_dir>/include`
        overlay = edge.get('overlay')
        if not overlay:
            return None
        return dict(rule=rule, cinc=True, cwd=None,
                    build_overlay=unit_dir / overlay,
                    includes=[unit_dir, unit_dir / 'include'])
    if rule == 'ccas':
        # tools/tu/ninja_main.py: `ee-as ... -I . -I include` in the unit directory
        return dict(rule=rule, cinc=False, cwd=unit_dir, build_overlay=None,
                    includes=[unit_dir / 'include'])
    if rule == 'cc':
        # tools/tu/cc_tu.sh: `ee-as ... -I . -I <unit_dir> -I <root>` in the unit
        # directory; those two directories are the last arguments of the rule.
        command = (document['rules'].get('cc') or {}).get('command', '').split()
        if len(command) < 2:
            return None
        return dict(rule=rule, cinc=False, cwd=unit_dir, build_overlay=None,
                    includes=[unit_dir, Path(command[-1])])
    return None


def compile_tu(contract, tu_rel, unit_dir, out_dir, asm_cwd=None, asm_includes=(), recipe=None):
    """cpp -> cc1 -> [cinc] -> as under the contract and the build's recipe.

    Returns (paths, log, messages, asm_cwd, asm_includes): the last two are the
    directory the assembler ran in and the extra `-I` directories it was given,
    which the rest of the audit needs to resolve the same included `.s` files
    the assembler resolved.
    """
    cc = ROOT / contract['compiler']['dir']
    driver = cc / contract['driver']
    flags, incs = contract['flags'], []
    for d in contract['include_dirs']:
        incs += ['-I', d]
    stem = Path(out_dir) / Path(tu_rel).stem
    steps = [
        ('cpp', [driver, f'-B{cc}/', *contract['driver_flags'], '-E', *flags, *incs, str(tu_rel),
                 '-o', f'{stem}.i'], unit_dir),
        ('cc1', [driver, f'-B{cc}/', *contract['driver_flags'], '-S', *flags, f'{stem}.i', '-o', f'{stem}.s'],
         unit_dir),
    ]
    if recipe is not None:
        if recipe['cinc']:
            overlay = Path(out_dir) / 'cinc'
            # The audit's OWN cinc, not the audited tree's: this is tooling that
            # produces the audit's overlay, so it must not be supplied by the subject.
            steps.append(('cinc', [sys.executable, '-B', str(TOOL_ROOT / CINC_TOOL),
                                   '--i', f'{stem}.i', '--s', f'{stem}.s',
                                   '--root', str(unit_dir), '--dst', str(overlay),
                                   '--report', f'{stem}.cinc.json'], unit_dir))
            asm_cwd = overlay
        else:
            # `None` when the build assembles in the unit directory, which is the
            # audit's default too: the rest of the audit reads `asm_cwd` as "a
            # staged copy of the included .s files", and the unit files are not a
            # staged copy of themselves.
            asm_cwd = (Path(recipe['cwd']).resolve()
                       if Path(recipe['cwd']).resolve() != Path(unit_dir).resolve() else None)
        asm_includes = recipe['includes']
    allocation = (recipe or {}).get('common_allocation')
    assembled = f'{stem}.raw.o' if allocation else f'{stem}.o'
    steps.append(('as', assembler_argv(contract, assembled, f'{stem}.s', asm_includes),
                  asm_cwd or unit_dir))
    if allocation:
        if sha256_file(allocation['linker']) != allocation['linker_sha256']:
            raise AuditError('native MAIN linker changed during the audit')
        steps.append(('allocate_common', [allocation['linker'], '-r', '-d', '-EL', '-m', 'elf32lr5900',
                                         '-o', f'{stem}.o', assembled], unit_dir))
    log, messages = [], []
    for name, argv, cwd in steps:
        rc, out = run(argv, cwd)
        log.append(dict(step=name, argv=[rel(a) if str(a).startswith('/') else str(a) for a in argv],
                        cwd=rel(cwd), rc=rc, output=out[-2000:]))
        if rc:
            raise AuditError(f'{name} failed ({rc}): {out[-400:]}')
        if out.strip():
            # Keep every message: a partially recovered TU can emit many legitimate
            # scaffold-exemption warnings, and truncating the capture cut the last
            # one mid-word so it failed the exemption match (2026-09-19).
            messages.append(dict(step=name, output=out.strip()[:65536]))
    return ((Path(f'{stem}.i'), Path(f'{stem}.s'), Path(f'{stem}.o')), log, messages,
            asm_cwd, list(asm_includes))


# ---------------------------------------------------------------------------
# Originals, manifests, accepted records
# ---------------------------------------------------------------------------

def original_elf(unit):
    from tools.inventory import ELF
    targets = json.loads((ROOT / 'config/targets.json').read_text())
    if unit not in targets['units']:
        raise AuditError(f'unknown unit {unit!r}')
    pin = targets['units'][unit]
    path = ROOT / 'game/extracted-iso' / pin['path']
    if not path.is_file():
        raise AuditError(f'original ELF missing: {rel(path)}')
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != pin['sha256']:
        raise AuditError(f'original ELF differs from config/targets.json: {rel(path)}')
    return ELF(data, unit), dict(unit=unit, path=rel(path), sha256=pin['sha256'])


def lookup_tu(tu_id, unit):
    """(start, end, function names, source, record) for a TU id."""
    build = ROOT / 'config/tu-build.json'
    tried = []
    if build.is_file():
        data = json.loads(build.read_text())
        objs = data.get('tus') or data.get('objects') or []
        for t in (objs if isinstance(objs, list) else objs.values()):
            if tu_id in (t.get('id'), t.get('map_id')):
                text = t.get('text') or {}
                funcs = [f['name'] if isinstance(f, dict) else f for f in t.get('functions', [])]
                return (int(text['start'], 16), int(text['end'], 16), funcs or None, 'config/tu-build.json', t)
        tried.append('config/tu-build.json')
    for path in TU_MAPS:
        p = ROOT / path
        if not p.is_file():
            continue
        data = json.loads(p.read_text())
        tus = data.get('tus') or data.get('overlays', {}).get(unit, {}).get('tus', [])
        for t in tus:
            if t.get('id') == tu_id:
                start = t['text']['start'] if isinstance(t.get('text'), dict) else t['start']
                end = t['text']['end'] if isinstance(t.get('text'), dict) else t['end']
                return int(start, 16), int(end, 16), [f['name'] for f in t['functions']], path, t
        tried.append(path)
    raise AuditError(f'TU id {tu_id!r} not found in {tried}')


_ACCEPTED_ASM_INDEX = None


def accepted_asm_records():
    """{function name: (record path, record)} for accepted standalone ee-asm units."""
    global _ACCEPTED_ASM_INDEX
    if _ACCEPTED_ASM_INDEX is None:
        index = {}
        for path in sorted((ROOT / 'config/units').glob('*.json')):
            try:
                record = json.loads(path.read_bytes())
            except ValueError:
                continue
            if record.get('source_language') != 'ee-asm':
                continue
            for f in record.get('functions', []):
                index[f['name']] = (rel(path), record)
        _ACCEPTED_ASM_INDEX = index
    return _ACCEPTED_ASM_INDEX


# ---------------------------------------------------------------------------
# Audit
# ---------------------------------------------------------------------------

class Checks:
    def __init__(self):
        self.rows = []

    def add(self, check, ok, **detail):
        self.rows.append(dict(check=check, ok=bool(ok), **detail))
        return ok


def function_classes(args):
    per = {}
    for spec in args.function_class or []:
        name, _, cls = spec.partition('=')
        if cls not in CLASSES:
            raise AuditError(f'invalid --function-class {spec!r}')
        per[name] = cls
    return per


def ledger_claims(path):
    """Claimed names from a TU ledger."""
    data = json.loads(Path(path).read_text())
    if isinstance(data, list):
        return [str(n) for n in data]
    if isinstance(data.get('claims'), list):
        return [str(n) for n in data['claims']]
    funcs = data.get('functions')
    if isinstance(funcs, dict):
        return [n for n, v in funcs.items() if not isinstance(v, dict) or v.get('state') in ('c', 'accepted_asm')]
    if isinstance(funcs, list):
        return [f['name'] for f in funcs if f.get('state') in ('c', 'accepted_asm')]
    raise AuditError(f'unrecognised ledger format: {path}')


def symbol_check(name, obj_funcs, pool, start):
    reasons, detail = [], {}
    orig = [s for s in pool if s['name'] == name]
    objs = obj_funcs.get(name, [])
    if len(orig) != 1:
        reasons.append(f'{len(orig)} original FUNC symbols named {name} in range')
    if len(objs) != 1:
        reasons.append(f'{len(objs)} FUNC symbols named {name} in the object')
    if len(orig) == 1 and len(objs) == 1:
        o, s = orig[0], objs[0]
        detail = dict(original=dict(va=o['va'], binding=BIND.get(o['binding'], o['binding']), type='FUNC',
                                    size=o['size']),
                      object=dict(offset=f'0x{s["value"]:x}', binding=BIND.get(s['binding'], s['binding']),
                                  type='FUNC', size=s['size'], section=s['section']))
        if s['section'] not in TEXT_SECTIONS:
            reasons.append(f'object symbol is in {s["section"]}, not .text')
        if o['binding'] != s['binding']:
            reasons.append(f'binding differs: original {BIND.get(o["binding"])} vs object {BIND.get(s["binding"])}')
        if o['size'] != s['size']:
            reasons.append(f'size differs: original {o["size"]} vs object {s["size"]}')
        if start is None:
            reasons.append('TU range unknown: offset not verified')
        elif int(o['va'], 16) - start != s['value']:
            reasons.append(f'offset differs: original +0x{int(o["va"], 16) - start:x} vs object +0x{s["value"]:x}')
        aliases = sorted({a['name'] for a in pool if a['va'] == o['va'] and a['name'] != name})
        lost = [a for a in aliases if not any(x['value'] == s['value'] for x in obj_funcs.get(a, []))]
        if aliases:
            detail['aliases'] = aliases
        if lost:
            reasons.append(f'original alias symbols missing from the object: {lost}')
    return reasons, detail


def nested_alias_names(specs, obj_funcs):
    """Candidate local names emitted for the authorized nested functions."""
    aliases = set()
    for entries in (specs or {}).values():
        for item in entries:
            prefix = re.escape(item['source_name']) + r'\.\d+'
            aliases.update(name for name in obj_funcs if re.fullmatch(prefix, name))
    return aliases


def audit_nested_c(item, parent, tu, prelude, tu_rel, header_set, regions, obj_funcs, pool, start,
                    tu_class=None):
    """Audit a compiler-emitted nested definition against its original identity."""
    original_name = item['original_name']
    reasons, checks = [], {}
    original = [s for s in pool if s['name'] == original_name]
    aliases = sorted(name for name in obj_funcs
                      if re.fullmatch(re.escape(item['source_name']) + r'\.\d+', name))
    if len(aliases) > 1 and len(original) == 1 and start is not None:
        aliases = [name for name in aliases if any(s['value'] == int(original[0]['va'], 16) - start
                                                  for s in obj_funcs[name])]
    if len(original) != 1:
        reasons.append(f'{len(original)} original FUNC symbols named {original_name} in range')
    if len(aliases) != 1:
        reasons.append(f'{len(aliases)} emitted local symbols match {item["source_name"]}.N')
    actual = aliases[0] if len(aliases) == 1 else None
    found = regions.get(actual, []) if actual else []
    prov = []
    if len(found) != 1:
        prov.append(f'{len(found)} cc1 regions (.ent/.end) for emitted nested symbol {actual or "<none>"}')
    elif found[0]['section'] not in TEXT_SECTIONS:
        prov.append(f'nested region starts in section {found[0]["section"]}')
    elif found[0]['file'] != tu_rel:
        prov.append(f'nested region is under .file {found[0]["file"]!r}, not {tu_rel}')
    elif not any(label == actual for label, _ in found[0]['labels']):
        prov.append(f'no {actual}: label inside nested region')
    checks['provenance'] = dict(ok=not prov, reasons=prov, emitted_name=actual)
    reasons += prov
    if len(original) == 1 and actual:
        expected = original[0]
        candidate = obj_funcs[actual][0]
        detail = dict(original=dict(name=original_name, va=expected['va'],
                                    binding=BIND.get(expected['binding'], expected['binding']),
                                    type='FUNC', size=expected['size']),
                      object=dict(name=actual, offset=f'0x{candidate["value"]:x}',
                                  binding=BIND.get(candidate['binding'], candidate['binding']),
                                  type='FUNC', size=candidate['size'], section=candidate['section']))
        symbol_reasons = []
        original_binding = BIND.get(expected['binding'], expected['binding'])
        # ELF symbols carry numeric STT_* values; nested-function mappings
        # use the corresponding readable label, just like bindings above.
        original_type = {STT_FUNC: 'FUNC'}.get(expected['type'], expected['type'])
        if original_binding != item['binding']:
            symbol_reasons.append(f'original binding differs from registered mapping: mapping {item["binding"]} '
                                  f'vs original {original_binding}')
        if original_type != item['type']:
            symbol_reasons.append(f'original type differs from registered mapping: mapping {item["type"]} '
                                  f'vs original {original_type}')
        if candidate['section'] not in TEXT_SECTIONS:
            symbol_reasons.append(f'object symbol is in {candidate["section"]}, not .text')
        if BIND.get(candidate['binding'], candidate['binding']) != item['binding']:
            symbol_reasons.append(f'binding differs: original {item["binding"]} vs '
                                  f'object {BIND.get(candidate["binding"], candidate["binding"])}')
        if candidate['size'] != item['size']:
            symbol_reasons.append(f'size differs: original {item["size"]} vs object {candidate["size"]}')
        if expected['size'] != item['size']:
            symbol_reasons.append(f'mapping size differs from original: {item["size"]} vs {expected["size"]}')
        if expected['va'] != item['va']:
            symbol_reasons.append(f'mapping VA differs from original: {item["va"]} vs {expected["va"]}')
        if start is None:
            symbol_reasons.append('TU range unknown: offset not verified')
        elif int(expected['va'], 16) - start != candidate['value']:
            symbol_reasons.append(f'offset differs: original +0x{int(expected["va"], 16) - start:x} '
                                  f'vs object +0x{candidate["value"]:x}')
        checks['symbol'] = dict(ok=not symbol_reasons, reasons=symbol_reasons, **detail)
        reasons += symbol_reasons
    else:
        checks['symbol'] = dict(ok=False, reasons=['nested symbol identity is incomplete'])
    text = prelude + tu.block_text(parent) + '\n'
    eligibility = []
    try:
        admission('gnu-nested-function', tu_class)(text, Path(tu_rel), header_set)
    except ValueError as error:
        eligibility.append(str(error))
    checks['eligibility'] = dict(ok=not eligibility, source_class='gnu-nested-function',
                                 reasons=eligibility)
    reasons += eligibility
    checks['record_fidelity'] = dict(ok=True, status='nested_parent', parent=parent.key,
                                     detail='the local symbol is covered by its parent C definition')
    return dict(name=original_name, block=parent.name, state='c', claimed=True, category='exact_c',
                ok=not reasons, reasons=reasons, checks=checks)


RODATA_ANON = RODATA_LIKE
# Sections whose anonymous cc1 constants (no original symbol names them) are
# covered only inside a declared carve run (config/tu/data-carves.json) and only
# when the whole compiled section, its relocations resolved at the runs'
# original addresses, equals the original bytes (`carved_section_bytes`). Under
# -G8 the small string literals and address templates go to .sdata, which
# otherwise holds named data, so .sdata is not blanket-anonymous like .rodata.
CARVED_ANON = ('.sdata',)


def carved_section_bytes(args, section, elf, obj):
    """dict(equal, ...): one carved C section, relocations resolved, vs the original bytes.

    Every byte of the compiled section is placed where the build places it: the
    declared runs, split by `tools/tu/data_carve.py` exactly as the build splits
    the object (`split_plans`), or the one run from its start. Each R_MIPS_32 is
    resolved at the original address of its target - a run of a carved section
    of this object, this TU's .text, or an original symbol of that name - and any
    other relocation, or a target that cannot be placed, makes the section
    unequal. Nothing is masked: an unresolved word is a failure, not a skip."""
    from tools.tu import data_carve as dc
    try:
        registry = dc.load_registry(ROOT)
        declared = (registry.get('tus') or {}).get(args.tu_id) or {}
        cobj = dc.Elf(bytes(obj.data))
        args_unit_dir = Path(args.unit_dir).resolve()
        orig = dc.Elf(bytes(elf.data))
        names = {'.rodata': ('.rodata', '.rdata')}
        plans = dc.split_plans(ROOT, args.unit, args_unit_dir, args.tu_id, cobj, registry)

        def pieces_of(sec_name):
            base = next((k for k, v in names.items() if sec_name in v), sec_name)
            runs = declared.get(base)
            if not runs:
                return None
            if base in plans:
                return plans[base]['pieces']
            csec_ = next(x for x in cobj.sections if x.name == sec_name)
            return [dict(offset=0, length=csec_.size, end=csec_.size, lo=dc.hx(runs[0]['range'][0]))]

        csec = next((x for x in cobj.sections if x.name in names.get(section, (section,)) and x.size), None)
        if csec is None:
            return dict(equal=False, reason=f'the C object emits no {section}')
        pieces = pieces_of(csec.name)
        data = bytearray(cobj.section_bytes(csec))
        text = next((x for x in cobj.sections if x.name == '.text'), None)
        text_start = dc.tu_context(ROOT, args.unit, args_unit_dir, args.tu_id)['text_start']
        by_name = {}
        for x in orig.symbols:
            if x.name and 0 < x.shndx < 0xff00:
                by_name.setdefault(x.name, set()).add(x.value)
        unresolved = []
        for rs in cobj.sections:
            if rs.type not in (4, 9) or rs.info != csec.index:
                continue
            for off, rtype, symi, add in dc._rel_entries(cobj, rs):
                sym = cobj.symbols[symi] if symi < len(cobj.symbols) else None
                where = f'{section}+0x{off:x}'
                if rtype != 2 or sym is None or off + 4 > len(data):
                    unresolved.append(f'{where}: relocation type {rtype}')
                    continue
                addend = add if add is not None else struct.unpack_from('<i', data, off)[0]
                target = (0 if sym.type == STT_SECTION else sym.value) + addend
                value = None
                if text is not None and sym.shndx == text.index:
                    value = text_start + target
                elif 0 < sym.shndx < len(cobj.sections) and sym.shndx != 0xfff1:
                    tp = pieces_of(cobj.sections[sym.shndx].name)
                    if tp:
                        try:
                            k = dc.piece_of(tp, target, where)
                            value = tp[k]['lo'] + target - tp[k]['offset']
                        except dc.CarveError:
                            value = None
                elif sym.shndx == 0 and len(by_name.get(sym.name, ())) == 1:
                    value = next(iter(by_name[sym.name])) + addend
                if value is None:
                    unresolved.append(f'{where}: {sym.name or cobj.sections[sym.shndx].name}+0x{target:x} '
                                      'has no original address')
                    continue
                struct.pack_into('<I', data, off, value & 0xffffffff)
        differing = []
        for k, p in enumerate(pieces):
            end = pieces[k + 1]['offset'] if k + 1 < len(pieces) else len(data)
            want = dc.va_bytes(orig, p['lo'], end - p['offset'])
            if want is None:
                return dict(equal=False, reason=f'run 0x{p["lo"]:08x} is not inside an allocated original section')
            got = bytes(data[p['offset']:end])
            differing += [f'0x{p["lo"] + i:08x}' for i in range(0, len(got), 4) if got[i:i + 4] != want[i:i + 4]]
        return dict(equal=not unresolved and not differing, pieces=len(pieces), size=len(data),
                    unresolved=unresolved[:8], differing_words=differing[:8])
    except (dc.CarveError, OSError, KeyError, ValueError, StopIteration) as exc:
        return dict(equal=False, reason=f'{type(exc).__name__}: {exc}')


def data_gate_row(args, tu_c):
    """Return current compiler-data evidence bound to this exact TU build.

    Carve declarations identify intended compiler-owned runs; they do not by
    themselves authorize object symbols. The linked C-data gate must have
    proved the current source/object/assembly and linked placement first.
    """
    path = getattr(args, 'data_gate_report', None)
    if not path:
        return None
    report = json.loads(Path(path).read_text())
    if report.get('result') != 'pass':
        raise AuditError('data gate report is not passing')
    rows = [row for row in report.get('objects', []) if row.get('tu') == args.tu_id]
    if len(rows) != 1:
        raise AuditError(f'data gate report has {len(rows)} rows for {args.tu_id}')
    row = rows[0]
    candidate_record = None
    manifest_path = getattr(args, 'candidate_manifest', None)
    if manifest_path:
        manifest_path = Path(manifest_path).resolve()
        manifest = json.loads(manifest_path.read_text())
        manifest_sha = sha256_file(manifest_path)
        if report.get('manifest_sha256') != manifest_sha:
            raise AuditError('candidate manifest hash differs from linked data-gate report')
        candidates = [item for item in manifest.get('candidates', []) if item.get('tu') == args.tu_id]
        if len(candidates) != 1:
            raise AuditError(f'candidate manifest has {len(candidates)} records for {args.tu_id}')
        candidate = candidates[0]
        candidate_path = Path(candidate['record']).resolve()
        candidate_sha = sha256_file(candidate_path)
        if candidate_sha != candidate.get('sha256'):
            raise AuditError('candidate record hash differs from candidate manifest')
        candidate_record = json.loads(candidate_path.read_text())
        if candidate_record.get('tu') != args.tu_id:
            raise AuditError('candidate record TU identity differs from candidate manifest')
        row = dict(row, _audit_candidate_record=dict(path=str(candidate_path), sha256=candidate_sha,
                                                     manifest_path=str(manifest_path),
                                                     manifest_sha256=manifest_sha),
                   _audit_location_dispositions=candidate_record.get('location_dispositions') or [])
    if row.get('source_sha256') != sha256_file(tu_c):
        raise AuditError('data gate report source hash differs from the audited TU')
    if not args.obj:
        raise AuditError('data gate report requires the raw build object')
    raw = Path(args.obj).resolve()
    selected = Path(getattr(args, 'data_gate_object', None) or raw).resolve()
    if row.get('object_sha256') != sha256_file(selected):
        raise AuditError('data gate report object hash differs from the selected linked-data object')
    if selected != raw:
        chain = validate_selected_data_object(args, raw, selected, tu_c, row)
        row = dict(row, _audit_selected_object_chain=chain)
    # Version 2 reports pin the compiler input and the linked data selection
    # separately. Function emission is still checked against the fresh raw
    # object; initialized spans must be read from the object the data gate used.
    if report.get('schema') == 'linked-c-data-audit/2':
        compiler_path = Path(row.get('compiler_raw_object_path', '/nonexistent')).resolve()
        data_path = Path(row.get('linked_data_object_path', '/nonexistent')).resolve()
        if compiler_path != raw or not data_path.is_relative_to(Path(args.unit_dir).resolve()):
            raise AuditError('linked data-gate compiler/selection paths differ from this unit build')
        if not data_path.is_file() or row.get('linked_data_object_sha256') != sha256_file(data_path):
            raise AuditError('linked data-gate selected object hash differs from the current build')
        code_input = raw
        allocation = row.get('compiler_allocation_proof')
        if allocation:
            if not candidate_record or args.unit != 'main':
                raise AuditError('native COMMON chain requires the frozen MAIN candidate manifest')
            code_input = Path(allocation.get('allocated_object_path', '/nonexistent')).resolve()
            verification = Path(allocation.get('verification_path', '/nonexistent')).resolve()
            linker = Path(allocation.get('linker_path', '/nonexistent')).resolve()
            if (not code_input.is_relative_to(Path(args.unit_dir).resolve()) or
                    Path(allocation.get('raw_object_path', '/nonexistent')).resolve() != raw or
                    allocation.get('raw_object_sha256') != sha256_file(raw) or
                    not code_input.is_file() or allocation.get('allocated_object_sha256') != sha256_file(code_input) or
                    not verification.is_file() or allocation.get('verification_sha256') != sha256_file(verification) or
                    sha256_file(verification) != sha256_file(code_input) or
                    not linker.is_file() or allocation.get('linker_sha256') != sha256_file(linker)):
                raise AuditError('native COMMON chain object/tool hashes differ from the current build')
            for key, path in (('compiler_object', raw), ('object', code_input)):
                pin = candidate_record.get(key) or {}
                if Path(pin.get('path', '/nonexistent')).resolve() != path or pin.get('sha256') != sha256_file(path):
                    raise AuditError('native COMMON chain differs from frozen candidate object pins')
            recipe = (build_assembly_recipe(args.unit_dir, code_input) or {}).get('common_allocation') or {}
            if (Path(recipe.get('raw_object', '/nonexistent')).resolve() != raw or
                    Path(recipe.get('output_object', '/nonexistent')).resolve() != code_input or
                    Path(recipe.get('linker', '/nonexistent')).resolve() != linker or
                    recipe.get('linker_sha256') != sha256_file(linker)):
                raise AuditError('native COMMON chain differs from the supported current build recipe')
            expected_argv = [str(linker), '-r', '-d', '-EL', '-m', 'elf32lr5900',
                             '-o', str(verification), str(raw)]
            if allocation.get('argv') != expected_argv:
                raise AuditError('native COMMON chain verification command differs from the exact recipe')
            with tempfile.TemporaryDirectory(prefix='audit-common-', dir=code_input.parent) as directory:
                independently_allocated = Path(directory) / 'allocated.o'
                argv = [str(linker), '-r', '-d', '-EL', '-m', 'elf32lr5900',
                        '-o', str(independently_allocated), str(raw)]
                status, log = run(argv, Path(args.unit_dir))
                if status or not independently_allocated.is_file() or sha256_file(independently_allocated) != sha256_file(code_input):
                    raise AuditError('independent native COMMON transformation differs: ' + log[:300])
            row = dict(row, _audit_compiler_allocation=dict(raw_object=str(raw),
                raw_sha256=sha256_file(raw), allocated_object=str(code_input),
                allocated_sha256=sha256_file(code_input), linker_sha256=sha256_file(linker),
                independent_native_reproduction=True))
        compare_code_identity(code_input, data_path)
        row = dict(row, _audit_linked_data_object=str(data_path))
    build_s = Path(args.build_s) if getattr(args, 'build_s', None) else Path(args.obj).with_suffix('.s')
    if build_s.is_file() and row.get('assembly_sha256') != sha256_file(build_s):
        raise AuditError('data gate report assembly hash differs from the build assembly')
    linked = unit_dir = Path(args.unit_dir)
    linked = unit_dir / 'build' / f'{args.unit}.rom.elf'
    if linked.is_file() and row.get('linked_elf_sha256') and row.get('linked_elf_sha256') != sha256_file(linked):
        raise AuditError('data gate report linked ELF hash differs from the current unit ELF')
    return row


def compare_code_identity(raw_path, selected_path):
    """Require allocation/carving to preserve every compiler-emitted code identity."""
    raw = RelObject(Path(raw_path).read_bytes())
    selected = RelObject(Path(selected_path).read_bytes())
    raw_text = [(s['name'], s['size'], raw.bytes_of(s)) for s in raw.sections
                if s['name'] == '.text' or s['name'].startswith('.text.')]
    selected_text = [(s['name'], s['size'], selected.bytes_of(s)) for s in selected.sections
                     if s['name'] == '.text' or s['name'].startswith('.text.')]
    if raw_text != selected_text:
        raise AuditError('selected data object changes compiler-emitted .text sections')
    def funcs(obj):
        return sorted((s.get('name'), s.get('binding'), s.get('type'), s.get('size'), s.get('value'),
                       s.get('section')) for s in obj.symbols
                      if s.get('section') == '.text' and s.get('type') == STT_FUNC)
    if funcs(raw) != funcs(selected):
        raise AuditError('selected data object changes compiler-emitted function symbols')
    def text_relocs(obj):
        return sorted(row[:3] for row in obj.relocations() if 'text' in row[0])
    if text_relocs(raw) != text_relocs(selected):
        raise AuditError('selected data object changes compiler-emitted text relocations')


def allocation_output_path(args, raw):
    matches = []
    for report_path in getattr(args, 'allocation_report', None) or []:
        report = json.loads(Path(report_path).read_text())
        input_row, output_row = report.get('input') or {}, report.get('output') or {}
        input_path = Path(input_row.get('path', '/nonexistent')).resolve()
        # Some private allocators operate on a hash-pinned snapshot of the
        # Ninja raw object. Permit that exact copy only when both the snapshot
        # and current build input hash to the allocator's recorded input SHA.
        same_input = (input_path == raw or
                      (input_path.is_file() and raw.is_file() and
                       input_row.get('sha256') == sha256_file(raw) == sha256_file(input_path)))
        if same_input:
            matches.append((Path(output_row.get('path', '/nonexistent')).resolve(), report_path, report,
                            input_row, output_row))
    if len(matches) != 1:
        raise AuditError(f'{args.tu_id}: found {len(matches)} allocator outputs for the raw object')
    return matches[0]


def validate_selected_data_object(args, raw, selected, tu_c, row):
    """Bind linked-data evidence to raw C plus exact allocator/carve outputs.

    Function emission remains audited against `raw`. The linked-data report may
    instead pin a deterministic allocated/carved object; each edge is checked
    against its sidecar and current source/assembly before data is credited.
    """
    allocation_reports = getattr(args, 'allocation_report', None) or []
    carve_reports = getattr(args, 'data_carve_report', None) or []
    if not carve_reports:
        raise AuditError('--data-gate-object requires a current --data-carve-report')
    carved_arg = getattr(args, 'data_carve_object', None)
    if carved_arg and Path(carved_arg).resolve() != selected.resolve():
        raise AuditError('--data-carve-object differs from the linked-data report object')
    allocated = raw
    allocation_proof = None
    if allocation_reports:
        allocated, report_path, report, input_row, output_row = allocation_output_path(args, raw)
        if report.get('schema') != 'overlay-bss-allocation/1' or report.get('unit') != args.unit:
            raise AuditError('unsupported or mismatched overlay allocation report')
        if (input_row.get('sha256') != sha256_file(raw) or
                output_row.get('sha256') != sha256_file(allocated) or not allocated.is_file()):
            raise AuditError('overlay allocation report raw/output hash differs from current objects')
        source_hash = sha256_file(tu_c)
        file_pins = report.get('files') or {}
        build_s = Path(args.build_s).resolve() if args.build_s else raw.with_name(raw.name + '.s')
        if not build_s.is_file():
            raise AuditError('overlay allocation proof requires the current raw cc1 assembly')
        source_pinned = any(item.get('sha256') == source_hash and
                            Path(item.get('path', '')).resolve() == tu_c.resolve()
                            for item in file_pins.values())
        # Some overlay allocators record per-TU provenance on the recovered
        # rows rather than in the generic `files` map. Accept that shape only
        # when a row for this exact TU binds both current inputs by absolute
        # path and SHA. The raw/output object pins and code-identity checks
        # below remain mandatory.
        if not source_pinned:
            evidence_pins = [candidate.get('evidence') or {}
                             for candidate in report.get('recovered_location_candidates', [])
                             if (candidate.get('evidence') or {}).get('original_tu') == args.tu_id]
            source_pinned = any(
                evidence.get('source_sha256') == source_hash and
                Path(evidence.get('source_path', '/nonexistent')).resolve() == tu_c.resolve() and
                evidence.get('assembly_sha256') == sha256_file(build_s) and
                Path(evidence.get('assembly_path', '/nonexistent')).resolve() == build_s
                for evidence in evidence_pins)
        if not source_pinned:
            raise AuditError('overlay allocation report source/assembly pins differ from audited TU inputs')
        asm_pins = {(candidate.get('evidence') or {}).get('assembly_sha256')
                    for candidate in report.get('recovered_location_candidates', [])
                    if (candidate.get('evidence') or {}).get('original_tu') == args.tu_id}
        asm_pins.discard(None)
        if asm_pins and sha256_file(build_s) not in asm_pins:
            raise AuditError('overlay allocation report assembly pin differs from current cc1 output')
        compare_code_identity(raw, allocated)
        allocation_proof = dict(report=str(Path(report_path).resolve()),
                                 report_sha256=sha256_file(report_path),
                                 raw_object=str(raw), raw_sha256=sha256_file(raw),
                                 allocated_object=str(allocated), allocated_sha256=sha256_file(allocated),
                                 source_sha256=source_hash, assembly_sha256=sha256_file(build_s))
    carve_matches = []
    for report_path in carve_reports:
        report_path = Path(report_path).resolve()
        carve = json.loads(report_path.read_text())
        if carve.get('tu') != args.tu_id:
            continue
        input_path = Path(carve.get('object', ''))
        output_path = Path(carve.get('out', ''))
        if not input_path.is_absolute():
            input_path = Path(args.unit_dir).resolve() / input_path
        if not output_path.is_absolute():
            output_path = Path(args.unit_dir).resolve() / output_path
        if input_path.resolve() == allocated and output_path.resolve() == selected:
            carve_matches.append((report_path, carve))
    if len(carve_matches) != 1:
        raise AuditError(f'{args.tu_id}: found {len(carve_matches)} carve reports for the allocated/selected object pair')
    carve_path, carve_report = carve_matches[0]
    if not allocated.is_file() or not selected.is_file():
        raise AuditError('allocated or selected carved object is missing')
    helper_path = getattr(args, 'data_carve_tool', None)
    helper_sha = getattr(args, 'data_carve_tool_sha256', None)
    if not helper_path or not helper_sha:
        raise AuditError('selected-object proof requires pinned --data-carve-tool and --data-carve-tool-sha256')
    root = Path(args.root).resolve() if args.root else ROOT
    helper_path = Path(helper_path).resolve()
    try:
        helper_path.relative_to(root)
    except ValueError as exc:
        raise AuditError('pinned subject data-carve helper must resolve inside --root') from exc
    if sha256_file(helper_path) != helper_sha:
        raise AuditError('pinned subject data-carve helper SHA-256 differs')
    mod_name = '_tu_audit_subject_data_carve_' + helper_sha[:16]
    dc = sys.modules.get(mod_name)
    if dc is None:
        spec = importlib.util.spec_from_file_location(mod_name, helper_path)
        if spec is None or spec.loader is None:
            raise AuditError('cannot load pinned subject data-carve helper')
        dc = importlib.util.module_from_spec(spec)
        sys.modules[mod_name] = dc
        spec.loader.exec_module(dc)
    registry = dc.load_registry(root)
    derived = dc.derive(root, args.unit, Path(args.unit_dir).resolve(), args.tu_id, tu_c, allocated)
    if derived.get('problems'):
        raise AuditError('current data-carve derivation has problems: ' + '; '.join(derived['problems']))
    declared = (registry.get('tus') or {}).get(args.tu_id) or {}
    derived_runs = dc.normalize_runs(derived.get('runs', {}))
    declared_runs = dc.normalize_runs(declared)
    # Overlay allocation materializes native NOBITS owners after raw C
    # compilation. `derive` therefore covers initialized sections only; accept
    # a declared .bss/.sbss run only when the fresh linked-data row and the
    # current allocator output prove every named owner and its exact offset.
    if allocation_proof:
        allocation_report = allocation_proof['report']
        allocation = json.loads(Path(allocation_report).read_text())
        output = allocation.get('output') or {}
        allocated_elf = RelObject(allocated.read_bytes())
        allocated_symbols = [s for s in allocated_elf.symbols
                             if s.get('name') and s.get('type') in (STT_NOTYPE, STT_OBJECT)]
        allocator_rows = output.get('verified_symbols') or []
        for section in ('.bss', '.sbss'):
            runs = declared_runs.get(section) or []
            if not runs:
                continue
            storage_rows = [item for item in row.get('verified_storage', [])
                            if item.get('section') == section and item.get('linked') is True]
            for run in runs:
                run_lo, run_hi = int(run['range'][0], 16), int(run['range'][1], 16)
                spans = run.get('c_input_spans') or []
                for name in run.get('c_owned_symbols', []):
                    proofs = [item for item in storage_rows if item.get('name') == name]
                    alloc_rows = [item for item in allocator_rows if item.get('name') == name]
                    c_symbols = [item for item in allocated_symbols if item.get('name') == name]
                    if len(proofs) != 1 or len(alloc_rows) != 1 or len(c_symbols) != 1:
                        raise AuditError(f'{args.tu_id}: allocated {section} owner {name} lacks unique linked, '
                                         'allocator, or selected-object identity')
                    proof, alloc_row, c_symbol = proofs[0], alloc_rows[0], c_symbols[0]
                    try:
                        address, size = int(proof['address'], 16), proof['size']
                        alignment = proof['alignment']
                        owner_alignment = alloc_row['alignment']
                        map_lo, map_hi = (int(v, 16) for v in proof['tu_section_range'])
                        allocated_offset = int(alloc_row['allocated_offset'], 16)
                        span_matches = []
                        for span in spans:
                            if name not in span.get('symbols', []):
                                continue
                            span_lo, span_hi = (int(v, 16) for v in span['range'])
                            object_lo, object_hi = span['object_range']
                            mapped_offset = object_lo + address - span_lo
                            if (span.get('section') == c_symbol.get('section') and
                                    span_lo <= address and address + size <= span_hi and
                                    object_lo <= mapped_offset and mapped_offset + size <= object_hi):
                                span_matches.append(mapped_offset)
                    except (KeyError, TypeError, ValueError, IndexError):
                        raise AuditError(f'{args.tu_id}: malformed allocated {section} proof for {name}')
                    if (size <= 0 or alignment <= 0 or alignment & (alignment - 1) or
                            type(owner_alignment) is not int or owner_alignment <= 0 or
                            owner_alignment & (owner_alignment - 1) or
                            address % owner_alignment or allocated_offset % owner_alignment or
                            address < run_lo or address + size > run_hi or
                            map_lo > address or address + size > map_hi or
                            len(span_matches) != 1 or span_matches[0] != allocated_offset or
                            alloc_row.get('section') != c_symbol.get('section') or
                            alloc_row.get('proven_size') != size or
                            alloc_row.get('bind') != proof.get('bind') or
                            alloc_row.get('type') != {'NOTYPE': STT_NOTYPE, 'OBJECT': STT_OBJECT}.get(
                                proof.get('type')) or
                            c_symbol.get('value') != allocated_offset or
                            c_symbol.get('binding') != proof.get('bind') or
                            c_symbol.get('type') != {'NOTYPE': STT_NOTYPE, 'OBJECT': STT_OBJECT}.get(proof.get('type')) or
                            c_symbol.get('size') not in (0, size)):
                        raise AuditError(f'{args.tu_id}: allocated {section} owner {name} differs from its exact '
                                         'linked proof, input span, or current allocator output')
            declared_runs.pop(section, None)
            derived_runs.pop(section, None)
    def core_runs(run_map):
        return {sec: [{k: v for k, v in run.items() if k != 'c_storage_aliases'}
                      for run in runs] for sec, runs in run_map.items()}
    if core_runs(derived_runs) != core_runs(declared_runs):
        raise AuditError('current data-carve registry differs from fresh compiler derivation')
    plans = dc.split_plans(root, args.unit, Path(args.unit_dir).resolve(), args.tu_id,
                           dc.Elf(allocated.read_bytes()), registry)
    expected_sections = {sec: [dict(section=dc.split_section_name(sec, i), offset=hex(piece['offset']),
                                    length=hex(piece['length']), va=dc.h8(piece['lo']),
                                    run_end=dc.h8(piece['hi']))
                               for i, piece in enumerate(plan['pieces'])]
                         for sec, plan in plans.items()}
    recorded_sections = {sec: [{k: piece.get(k) for k in ('section', 'offset', 'length', 'va', 'run_end')}
                               for piece in value.get('pieces', [])]
                         for sec, value in (carve_report.get('sections') or {}).items()}
    if recorded_sections != expected_sections:
        raise AuditError('carve report sections differ from current fresh plans')
    expected = dc.split_object(allocated.read_bytes(), {sec: plan['pieces'] for sec, plan in plans.items()})
    if expected != selected.read_bytes():
        raise AuditError('selected carved object differs from deterministic data-carve output')
    compare_code_identity(raw, allocated)
    compare_code_identity(allocated, selected)
    return dict(raw_object=str(raw), raw_sha256=sha256_file(raw),
                allocation=allocation_proof, carve_report=str(carve_path),
                carve_report_sha256=sha256_file(carve_path),
                allocated_object=str(allocated), allocated_sha256=sha256_file(allocated),
                selected_object=str(selected), selected_sha256=sha256_file(selected),
                data_carve_tool=dict(path=str(helper_path), sha256=helper_sha))


def verified_storage_field_aliases(args, section, runs, data_row, storage_rows):
    """Validate interior address witnesses without adding C definitions."""
    aliases = [item for item in data_row.get('storage_owner_aliases',[]) if item.get('section') == section]
    if not aliases:
        return []
    from tools.tu import data_carve as dc, data_gate as dg
    unit_dir = Path(args.unit_dir).resolve()
    ctx = dc.tu_context(ROOT,args.unit,unit_dir,args.tu_id)
    source = Path(getattr(args,'tu_c',None) or ROOT/'src'/args.unit/(ctx['name']+'.c'))
    assembly = Path(args.build_s) if getattr(args,'build_s',None) else Path(str(args.obj)+'.s')
    selected = Path(data_row.get('_audit_linked_data_object','/nonexistent'))
    linked_path = unit_dir/'build'/f'{args.unit}.rom.elf'
    candidate_pin = data_row.get('_audit_candidate_record') or {}
    candidate_path = Path(candidate_pin.get('path','/nonexistent'))
    if (not source.is_file() or not assembly.is_file() or not selected.is_file() or not linked_path.is_file() or
            not candidate_path.is_file() or sha256_file(candidate_path) != candidate_pin.get('sha256') or
            sha256_file(source) != data_row.get('source_sha256') or
            sha256_file(assembly) != data_row.get('assembly_sha256') or
            sha256_file(selected) != data_row.get('linked_data_object_sha256') or
            sha256_file(linked_path) != data_row.get('linked_elf_sha256')):
        raise AuditError('interior storage aliases lack current source/compiler/selected/link/candidate pins')
    candidate = json.loads(candidate_path.read_text())
    raw = RelObject(Path(args.obj).read_bytes())
    input_path = Path((data_row.get('_audit_compiler_allocation') or {}).get('allocated_object',args.obj))
    allocation_input = RelObject(input_path.read_bytes())
    selected_obj = RelObject(selected.read_bytes())
    linked = dc.Elf(linked_path.read_bytes())
    original = dc.Elf(Path(ctx['orig']).read_bytes())
    items = dc.section_items(ctx,section)
    emission = dg.emitted_storage(assembly.read_text())
    checked, identities = [], set()
    for alias in aliases:
        try:
            address, owner_address = int(alias['address'],16),int(alias['owner_address'],16)
            extent, size, offset = alias['alias_extent'],alias['owner_size'],alias['owner_offset']
            name = alias['owner_name']
            names = set(alias['scaffold_names'])
        except (KeyError,TypeError,ValueError):
            raise AuditError('malformed interior storage field alias')
        if (not names or type(extent) is not int or extent <= 0 or type(size) is not int or size <= 0 or
                type(offset) is not int or offset <= 0 or not isinstance(name,str)):
            raise AuditError('malformed interior storage field alias extent or owner')
        owner_proofs = [proof for proof in storage_rows if proof.get('name') == name and
                        int(proof.get('address','0'),16) == owner_address and proof.get('size') == size]
        owner = owner_proofs[0] if len(owner_proofs) == 1 else {}
        raw_symbols = [symbol for symbol in raw.symbols if symbol.get('name') == name]
        input_symbols = [symbol for symbol in allocation_input.symbols if symbol.get('name') == name]
        selected_symbols = [symbol for symbol in selected_obj.symbols if symbol.get('name') == name]
        definition = emission.get(name,{})
        field = dg.source_storage_member_extent(source.read_text(),name,size,offset)
        current_candidates = [location for location in candidate.get('recovered_location_candidates',[])
                              if location.get('section') == section and int(location.get('address','0'),16) == address and
                              names == set(location.get('names',[])) and
                              (location.get('storage_owner') or {}).get('name') == name and
                              int((location.get('storage_owner') or {}).get('address','0'),16) == owner_address and
                              (location.get('storage_owner') or {}).get('size') == size and
                              location.get('alias_extent') == extent and location.get('alias_field') == field]
        mapped_items = [item for item in items if item['start'] == address and address+extent <= item['end'] and
                        names <= set(item.get('labels',[]))]
        mapped_inputs = [int(part['object_range'][0])+owner_address-int(part['range'][0],16)
                         for run in runs for part in run.get('c_input_spans',[])
                         if name in part.get('symbols',[]) and part.get('object_range') and
                         int(part['range'][0],16) <= owner_address and
                         owner_address+size <= int(part['range'][1],16)]
        expected_type = {'NOTYPE':STT_NOTYPE,'OBJECT':STT_OBJECT}.get(owner.get('type'))
        linked_owners = [symbol for symbol in linked.symbols if symbol.name == name and symbol.value == owner_address and
                         symbol.bind == owner.get('bind') and symbol.type == expected_type and symbol.size in (0,size) and
                         0 < symbol.shndx < len(linked.sections) and linked.sections[symbol.shndx].type == 8]
        original_at_alias = [symbol for symbol in original.symbols if symbol.value == address and
                             0 < symbol.shndx < len(original.sections) and original.sections[symbol.shndx].name == section]
        if (not names or len(owner_proofs) != 1 or type(extent) is not int or extent <= 0 or
                type(size) is not int or type(offset) is not int or offset <= 0 or
                address != owner_address+offset or offset+extent > size or
                alias.get('mapped_span') != [address,address+extent] or
                alias.get('linked') is not True or alias.get('source_field') != field or not field or field['size'] != extent or
                alias.get('owner_object_sha256') != data_row.get('linked_data_object_sha256') or
                alias.get('source_sha256') != data_row.get('source_sha256') or
                alias.get('assembly_sha256') != data_row.get('assembly_sha256') or
                alias.get('linked_elf_sha256') != data_row.get('linked_elf_sha256') or
                len(current_candidates) != 1 or len(mapped_items) != 1 or len(mapped_inputs) != 1 or
                len(raw_symbols) != 1 or len(input_symbols) != 1 or len(selected_symbols) != 1 or len(linked_owners) != 1 or
                definition.get('size') != size or definition.get('section') not in (section,'COMMON') or
                raw_symbols[0].get('binding') != owner.get('bind') or raw_symbols[0].get('type') != expected_type or
                raw_symbols[0].get('size') not in (0,size) or
                input_symbols[0].get('value') != mapped_inputs[0] or
                input_symbols[0].get('section') != section or
                selected_symbols[0].get('value') != alias.get('c_object_offset') or
                selected_symbols[0].get('section') != alias.get('c_object_section') or
                any(symbol.type == STT_OBJECT and symbol.size > 0 for symbol in original_at_alias)):
            raise AuditError(f'{args.tu_id}: unproven interior storage field alias at 0x{address:08x}')
        for scaffold_name in names:
            key = (address,scaffold_name)
            if key in identities or scaffold_name == name:
                raise AuditError('duplicate interior storage alias or alias counted as its owner')
            identities.add(key)
        checked.append(dict(alias))
    return checked


def c_owned_sections(args, contract, tu_record, data_row=None):
    """{section: {lo, hi, alo, ahi, exact, source}} for C-owned data sections."""
    owners, manifest_ranges, partial = {}, {}, {}
    if tu_record and isinstance(tu_record.get('data_ownership'), dict):
        for sec, own in tu_record['data_ownership'].items():
            if isinstance(own, dict):
                if own.get('owner') == 'c' and sec != '.text':
                    owners[sec] = own.get('range')
                    partial[sec] = bool(own.get('partial'))
                if own.get('window'):
                    manifest_ranges[sec] = own
            elif own == 'c' and sec != '.text':
                owners[sec] = None
    # A declared C-owned data run (config/tu/data-carves.json, tools/tu/data_carve.py):
    # the jump tables and literals the TU's C functions generate, carved out of the
    # scaffold piece. Declared under this root (a worker's or reviewer's private
    # copy, or the published one), it is newer than config/tu-build.json, which
    # carries it only after publication regenerates the manifest.
    # Several runs of one section (the C-generated items of the TU are separated by
    # scaffold items) own the span from the first run's start to the last run's end
    # for the size check, and only the runs themselves for the original symbols.
    carved, run_ranges = set(), {}
    verified_storage_by_section = {}
    storage_field_aliases_by_section = {}
    generated_aliases_by_section, symbol_aliases_by_section, anonymous_ranges_by_section = {}, {}, {}
    carve_path = ROOT / 'config/tu/data-carves.json'
    if getattr(args, 'tu_id', None) and carve_path.is_file():
        for sec, runs in ((json.loads(carve_path.read_text()).get('tus') or {}).get(args.tu_id) or {}).items():
            if runs:
                owners[sec] = [runs[0]['range'][0], runs[-1]['range'][1]]
                partial[sec] = True
                carved.add(sec)
                if len(runs) > 1:
                    run_ranges[sec] = [(int(r['range'][0], 16), int(r['range'][1], 16)) for r in runs]
                if data_row is not None:
                    declared = {name for run in runs for name in run.get('c_owned_symbols', [])}
                    verified = {name for item in data_row.get('verified_locations', [])
                                if item.get('section') == sec and
                                any(int(run['range'][0], 16) <= int(item['address'], 16) <
                                    int(run['range'][1], 16) for run in runs)
                                for name in item.get('names', [])}
                    # A disassembler label is not a retail ELF identity. The
                    # gate records the real C symbol separately when a literal
                    # has a semantic name but no original ELF symbol.
                    for span in data_row.get('verified_location_spans', []):
                        if (span.get('section') == sec and not span.get('original_identities') and
                                span.get('object_sha256') == data_row.get(
                                    'linked_data_object_sha256', data_row.get('object_sha256')) and
                                any(int(run['range'][0], 16) <= int(span['address'], 16) and
                                    int(span['address'], 16) + span['length'] <= int(run['range'][1], 16)
                                    for run in runs) and span.get('object_symbols')):
                            verified.difference_update(span.get('names', []))
                            verified.update(span['object_symbols'])
                    symbol_aliases = [dict(alias) for alias in data_row.get('symbol_aliases', [])
                                      if alias.get('section') == sec]
                    storage_rows = [dict(item) for item in data_row.get('verified_storage', [])
                                    if item.get('section') == sec and item.get('linked') is True]
                    if sec in ('.bss', '.sbss'):
                        valid_storage = []
                        for storage in storage_rows:
                            try:
                                address = int(storage['address'], 16)
                                size, alignment = storage['size'], storage['alignment']
                                map_lo, map_hi = (int(v, 16) for v in storage['tu_section_range'])
                                count, status = storage['relocation_count'], storage['relocation_status']
                            except (KeyError, TypeError, ValueError, IndexError):
                                continue
                            relocation_ok = ((type(count) is int and count > 0 and
                                              status == 'cc1_function_relocation') or
                                             (count == 0 and status ==
                                              'no_retained_relocation_exact_linked_identity'))
                            original_ids = storage.get('original_identities') or []
                            original_ok = all(
                                identity.get('binding') in (storage.get('bind'), BIND.get(storage.get('bind'))) and
                                ({STT_NOTYPE: 'NOTYPE', STT_OBJECT: 'OBJECT'}.get(identity.get('type'),
                                    identity.get('type')) == storage.get('type')) and
                                identity.get('size') in (0, size)
                                for identity in original_ids)
                            if (type(size) is int and size > 0 and type(alignment) is int and alignment > 0 and
                                    alignment & (alignment - 1) == 0 and storage.get('bind') in (0, 1, 2) and
                                    storage.get('type') in ('NOTYPE', 'OBJECT') and
                                    any(int(run['range'][0], 16) <= address and
                                        address + size <= int(run['range'][1], 16) for run in runs) and
                                    map_lo <= address and address + size <= map_hi and relocation_ok and original_ok):
                                valid_storage.append(storage)
                        valid_by_name = {item['name']: item for item in valid_storage}
                        declared_storage = {name for run in runs for name in run.get('c_owned_symbols', [])}
                        if set(valid_by_name) != declared_storage:
                            raise AuditError(f'{args.tu_id} {sec}: declared NOBITS owners differ from linked '
                                             f'storage proof (declared={sorted(declared_storage)}, '
                                             f'verified={sorted(valid_by_name)})')
                        verified.update(valid_by_name)
                        verified_storage_by_section[sec] = valid_storage
                        field_aliases = verified_storage_field_aliases(args,sec,runs,data_row,valid_storage)
                        verified.difference_update(name for alias in field_aliases for name in alias['scaffold_names'])
                        storage_field_aliases_by_section[sec] = field_aliases
                    alias_by_scaffold = {}
                    alias_objects = set()
                    for alias in symbol_aliases:
                        scaffold_name = alias.get('scaffold_name')
                        object_symbol = alias.get('object_symbol')
                        address = int(alias.get('address'), 16)
                        padded_owner_spans = set()
                        for span in data_row.get('verified_location_spans',[]):
                            owner = span.get('storage_owner') or {}
                            if (span.get('section') != sec or owner.get('name') != object_symbol or
                                    owner.get('size') != alias.get('size') or
                                    span.get('length',0) <= alias.get('size',0) or alias.get('size',0) <= 0 or
                                    owner.get('address','').lower() != alias.get('address','').lower() or
                                    owner.get('binding') != alias.get('binding') or owner.get('type') != alias.get('type') or
                                    owner.get('object_sha256') != data_row.get('object_sha256') or
                                    owner.get('source_sha256') != data_row.get('source_sha256') or
                                    owner.get('assembly_sha256') != data_row.get('assembly_sha256') or
                                    owner.get('linked_elf_sha256') != data_row.get('linked_elf_sha256')):
                                continue
                            from tools.tu import data_carve as dc
                            raw_owner = RelObject(Path(args.obj).read_bytes())
                            selected_owner = RelObject(Path(data_row['_audit_linked_data_object']).read_bytes())
                            raw_symbols = [symbol for symbol in raw_owner.symbols if symbol.get('name') == object_symbol and
                                           symbol.get('section') == owner.get('section') and
                                           symbol.get('value') == owner.get('c_object_offset') and
                                           symbol.get('size') == alias['size'] and symbol.get('binding') ==
                                           {'LOCAL':0,'GLOBAL':1,'WEAK':2}.get(alias.get('binding')) and
                                           symbol.get('type') == {'NOTYPE':STT_NOTYPE,'OBJECT':STT_OBJECT}.get(alias.get('type'))]
                            selected_symbols = [symbol for symbol in selected_owner.symbols if symbol.get('name') == object_symbol and
                                                symbol.get('section') == span.get('c_object_section') and
                                                symbol.get('value') == span.get('c_object_offset') and
                                                symbol.get('size') == alias['size'] and symbol.get('binding') ==
                                                {'LOCAL':0,'GLOBAL':1,'WEAK':2}.get(alias.get('binding')) and
                                                symbol.get('type') == {'NOTYPE':STT_NOTYPE,'OBJECT':STT_OBJECT}.get(alias.get('type'))]
                            if len(raw_symbols) != 1 or len(selected_symbols) != 1:
                                continue
                            raw_symbol, selected_symbol = raw_symbols[0],selected_symbols[0]
                            raw_payload = raw_owner.bytes_of(raw_owner.sections[raw_symbol['shndx']])
                            selected_payload = selected_owner.bytes_of(selected_owner.sections[selected_symbol['shndx']])
                            lo,size = raw_symbol['value'],raw_symbol['size']
                            selected_lo = selected_symbol['value']
                            ctx = dc.tu_context(ROOT,args.unit,Path(args.unit_dir),args.tu_id)
                            original = dc.Elf(Path(ctx['orig']).read_bytes())
                            raw_relocations = [row for row in raw_owner.relocations() if row[0] == raw_symbol['section'] and
                                               lo <= row[1] < lo+size]
                            if (not raw_relocations and raw_payload[lo:lo+size] == dc.va_bytes(original,address,size) and
                                    selected_payload[selected_lo:selected_lo+size] == raw_payload[lo:lo+size] and
                                    not any(selected_payload[selected_lo+size:selected_lo+span['length']])):
                                padded_owner_spans.add((span.get('section'),span.get('address'),span.get('c_object_offset')))
                        # R22 producers may represent a same-address alias in
                        # the TU-level symbol_aliases list while the matching
                        # span carries the original identity and selected C
                        # object identity. Accept that normalized form only
                        # when every identity field is independently pinned by
                        # the exact span and selected-object hash.
                        direct_span_alias = any(
                            span.get('address', '').lower() == alias.get('address', '').lower() and
                            object_symbol in span.get('object_symbols', []) and
                            object_symbol in span.get('names', []) and
                            span.get('object_sha256') == data_row.get(
                                'linked_data_object_sha256', data_row.get('object_sha256')) and
                            (span.get('length') == alias.get('size') or
                             (span.get('section'),span.get('address'),span.get('c_object_offset')) in padded_owner_spans) and
                            any(identity.get('name') == scaffold_name and
                                identity.get('address', '').lower() == alias.get('address', '').lower() and
                                identity.get('binding') == alias.get('binding') and
                                identity.get('type') == alias.get('type') and
                                identity.get('size') == alias.get('size')
                                for identity in span.get('original_identities', []))
                            for span in data_row.get('verified_location_spans', [])
                            if span.get('section') == sec)
                        direct_identity_alias = (
                            direct_span_alias and
                            alias.get('binding') in ('LOCAL', 'GLOBAL', 'WEAK') and
                            alias.get('type') in ('NOTYPE', 'OBJECT'))
                        if (not direct_identity_alias and sec in ('.bss', '.sbss') and
                                alias.get('binding') == 'LOCAL' and alias.get('type') == 'NOTYPE'):
                            # A zero-size retail local has one compiler .space
                            # owner, while Splat supplies a public marker name.
                            # Prove that exact alias without counting another
                            # compiler definition or inferring punctuation.
                            storage = verified_storage_by_section.get(sec, [])
                            proofs = [item for item in storage if item.get('name') == object_symbol and
                                      int(item.get('address', '0'),16) == address and
                                      item.get('size') == alias.get('size') and item.get('bind') == 0 and
                                      item.get('type') == 'NOTYPE' and item.get('linked') is True and
                                      item.get('object_sha256') == data_row.get(
                                          'linked_data_object_sha256',data_row.get('object_sha256')) and
                                      any(identity.get('name') == object_symbol and
                                          identity.get('binding') == 0 and identity.get('type') == 0 and
                                          identity.get('size') == 0 for identity in item.get('original_identities',[]))]
                            if len(proofs) == 1:
                                from tools.tu import data_carve as dc, data_gate as dg
                                raw_storage = RelObject(Path(args.obj).read_bytes())
                                symbols = [symbol for symbol in raw_storage.symbols
                                           if symbol.get('name') == object_symbol and symbol.get('section') == sec and
                                           symbol.get('binding') == 0 and symbol.get('type') == STT_NOTYPE and
                                           symbol.get('size') == 0]
                                build_s = Path(args.build_s) if getattr(args,'build_s',None) else Path(str(args.obj)+'.s')
                                emission = dg.emitted_storage(build_s.read_text()).get(object_symbol,{})
                                ctx = dc.tu_context(ROOT,args.unit,Path(args.unit_dir),args.tu_id)
                                items = [item for item in dc.section_items(ctx,sec) if item['start'] == address and
                                         item['end']-item['start'] == alias.get('size') and
                                         scaffold_name in item.get('labels',[])]
                                direct_identity_alias = (len(symbols) == 1 and len(items) == 1 and
                                    emission.get('extent_kind') == 'label-space' and emission.get('section') == sec and
                                    emission.get('size') == alias.get('size') and
                                    any(symbols[0]['value'] == int(part['object_range'][0]) + address-int(part['range'][0],16)
                                        for run in runs for part in run.get('c_input_spans',[])
                                        if object_symbol in part.get('symbols',[]) and part.get('object_range') and
                                        int(part['range'][0],16) <= address and
                                        address+alias['size'] <= int(part['range'][1],16)))
                        duplicate_object = object_symbol in alias_objects and not direct_identity_alias
                        if (not scaffold_name or not object_symbol or scaffold_name in alias_by_scaffold or
                                duplicate_object or scaffold_name == object_symbol or
                                (scaffold_name not in declared and not direct_identity_alias) or
                                object_symbol not in verified or
                                not any(int(run['range'][0], 16) <= address < int(run['range'][1], 16)
                                        for run in runs) or
                                not any(item.get('section') == sec and int(item['address'], 16) == address and
                                        object_symbol in item.get('names', [])
                                        for item in data_row.get('verified_locations', [])) and
                                not direct_identity_alias):
                            raise AuditError(f'{args.tu_id} {sec}: malformed or unproven scaffold/C symbol alias '
                                             f'in linked data-gate evidence: {alias}')
                        alias_by_scaffold[scaffold_name] = alias
                        alias_objects.add(object_symbol)
                    # GNU cc1 emits switch tables as anonymous section payload;
                    # the original linker/map may give those bytes a local
                    # `jtbl_<VA>` label which has no counterpart in the C object.
                    # Admit only the exact declared jtbl label whose complete
                    # address is covered by the fresh linked-data gate's
                    # compiler-owned range. This is hash-bound to the current
                    # source/object/assembly/linked ELF below; ordinary named
                    # data still has to appear in verified_locations.
                    generated_aliases = set()
                    gate_ranges = data_row.get('compiler_owned_ranges', {}).get(sec, [])
                    for name in declared - verified:
                        match = re.fullmatch(r'jtbl_([0-9A-Fa-f]{8})', name)
                        if not match:
                            continue
                        address = int(match.group(1), 16)
                        in_carve = any(int(run['range'][0], 16) <= address < int(run['range'][1], 16)
                                       for run in runs)
                        in_compiler_range = any(int(span[0]) <= address < int(span[1])
                                                for span in gate_ranges)
                        if in_carve and in_compiler_range:
                            generated_aliases.add(name)
                    alias_carve_names = ({name for name in alias_by_scaffold if name in declared} |
                                         {alias['object_symbol'] for alias in alias_by_scaffold.values()
                                          if alias['object_symbol'] in declared})
                    uncredited_scaffold_names = {
                        uncredited_dependency_label(dep) for dep in data_row.get('uncredited_compiler_dependencies', [])
                        if dep.get('section') == sec and dep.get('credited') is False and
                        dep.get('disposition') == 'uncredited_scaffold_dependency' and
                        dep.get('kind') in ('inline_assembly_data', 'compiler_region_switch_words') and
                        uncredited_dependency_label(dep) and
                        (dep.get('kind') != 'compiler_region_switch_words' or
                         valid_switch_word_dependency_shape(dep))}
                    expected_declared = ((verified - alias_objects - set(alias_by_scaffold)) |
                                         alias_carve_names | generated_aliases | uncredited_scaffold_names)
                    if declared != expected_declared:
                        raise AuditError(f'{args.tu_id} {sec}: declared C-owned symbols differ from '
                                          f'linked data-gate evidence (declared={sorted(declared)}, '
                                          f'verified={sorted(verified)}, aliases={sorted(alias_by_scaffold)})')
                    anonymous = [x for x in data_row.get('anonymous_constants', [])
                                 if x.get('section') == sec and x.get('range')]
                    anonymous_ranges = [(int(x['range'][0]), int(x['range'][1])) for x in anonymous]
                    generated_aliases_by_section[sec] = generated_aliases
                    symbol_aliases_by_section[sec] = list(alias_by_scaffold.values())
                    anonymous_ranges_by_section[sec] = anonymous_ranges
                else:
                    anonymous_ranges = []
    for spec in args.c_owned or []:
        sec, _, rng = spec.partition('=')
        if sec in carved:
            continue          # the declared run supersedes an import-era split of the section
        owners[sec] = rng.split(':') if rng else owners.get(sec)
    out = {}
    for sec, rng in owners.items():
        if rng:
            lo, hi = int(rng[0], 16), int(rng[1], 16)
            out[sec] = dict(lo=lo, hi=hi, alo=lo, ahi=hi, exact=True,
                            source='config/tu/data-carves.json' if sec in carved else 'explicit',
                            partial=partial.get(sec, False), runs=run_ranges.get(sec),
                            verified_names=sorted({name for item in (data_row or {}).get('verified_locations', [])
                                                   if item.get('section') == sec and lo <= int(item['address'], 16) < hi
                                                   for name in item.get('names', []) if name not in {
                                                       n for alias in storage_field_aliases_by_section.get(sec,[]) for n in alias['scaffold_names']}}),
                            verified_storage_field_aliases=storage_field_aliases_by_section.get(sec,[]),
                            verified_locations=[item for item in (data_row or {}).get('verified_locations', [])
                                                if item.get('section') == sec],
                            verified_storage=verified_storage_by_section.get(sec, []),
                            anonymous_ranges=anonymous_ranges_by_section.get(sec, []),
                            generated_aliases=sorted(generated_aliases_by_section.get(sec, set())),
                            symbol_aliases=symbol_aliases_by_section.get(sec, []),
                            uncredited_dependencies=[item for item in (data_row or {}).get(
                                'uncredited_compiler_dependencies', []) if item.get('section') == sec],
                            compiler_owned_ranges=[list(map(int, span)) for span in
                                                   (data_row or {}).get('compiler_owned_ranges', {}).get(sec, [])])
            continue
        entry = dict(lo=None, hi=None, alo=None, ahi=None, exact=False, source=None,
                     partial=partial.get(sec, False))
        if sec in manifest_ranges:
            own = manifest_ranges[sec]
            ev = own.get('evidence') or own['window']
            entry.update(lo=int(own['window'][0], 16), hi=int(own['window'][1], 16), alo=int(ev[0], 16),
                         ahi=int(ev[1], 16), source='config/tu-build.json window/evidence')
        else:
            data = (tu_record or {}).get('data') or {}
            if sec in data:
                d = data[sec]
                entry.update(lo=int(d['lower_bound_start'], 16), hi=int(d['upper_bound_end'], 16),
                             alo=int(d['anchored_start'], 16), ahi=int(d['anchored_end'], 16),
                             source='tu-map bounds')
            for d in (tu_record or {}).get('data_contributions') or []:
                if d.get('family') == sec:
                    entry.update(lo=int(d['start_window'][0], 16), hi=int(d['end_window'][1], 16),
                                 alo=int(d['start_window'][1], 16), ahi=int(d['end_window'][0], 16),
                                 source='tu-map windows')
        out[sec] = entry
    return out


def overlay_bss_owned_names(args, elf, obj):
    """Overlay BSS statics a TU owns, re-proved from the audited object.

    config/objects/data-carves.json `overlay_bss_records` lists the local BSS
    storage that tools/tu/overlay_link.py allocates for a recovered overlay TU.
    A recorded owner is admitted only when the audited object defines it once
    in .bss/.sbss/.scommon (or COMMON) with the recorded linkage and the
    original ELF carries the same identity, with the same binding, at the
    recorded owner address. A function-scope static is compared without
    cc1's `.N` counter (the original `cld0.8` is the object's `cld0.0`: the
    counter depends on how much of the TU is recovered), and a `D_<address>`
    placeholder is admitted only where the original has no symbol at all.
    Returns (names, reasons).
    """
    def static_base(symbol_name):
        return re.sub(r'\.\d+$', '', symbol_name)

    path = ROOT / 'config/objects/data-carves.json'
    tu_id = getattr(args, 'tu_id', None)
    if not tu_id or not path.is_file():
        return set(), []
    unit_records = (json.loads(path.read_bytes()).get('overlay_bss_records') or {}).get(args.unit) or {}
    accepted_tus = (tu_id, tu_id.split('_')[0])
    rows = [row for row in unit_records.get('recovered_location_candidates', [])
            if row.get('section') in ('.bss', '.sbss')
            and (row.get('evidence') or {}).get('original_tu') in accepted_tus]
    names, reasons = set(), []
    for row in rows:
        owner = row.get('storage_owner') or {}
        name = owner.get('name')
        if not name or not owner.get('address'):
            reasons.append(f'{row.get("address")}: record has no storage owner name/address')
            continue
        if name in names:
            continue
        defs = [s for s in obj.symbols if s['name'] == name and s['shndx'] != 0 and
                (s['section'] in ('.bss', '.sbss', '.scommon') or s['shndx'] in (SHN_COMMON, SHN_MIPS_SCOMMON))]
        if len(defs) != 1:
            reasons.append(f'{name}: {len(defs)} BSS definitions in the audited object')
            continue
        recorded_local = str(owner.get('linkage', '')).lower() in ('local', 'static', 'internal')
        if recorded_local != (defs[0]['binding'] == 0):
            reasons.append(f'{name}: object binding {defs[0]["binding"]} differs from recorded '
                           f'{owner.get("linkage")} linkage')
            continue
        address = int(owner['address'], 16)
        at_address = [s for s in elf.symbols if s['name'] and s['section_index'] != 0
                      and int(s['va'], 16) == address]
        if not at_address and name == f'D_{address:08X}':
            names.add(name)
            continue
        originals = [s for s in at_address if static_base(s['name']) == static_base(name)]
        if len(originals) != 1 or originals[0]['binding'] != defs[0]['binding']:
            original_names = sorted({s['name'] for s in at_address}) or ['no symbol']
            reasons.append(f'{name}: the original names 0x{address:08x} '
                           f'{", ".join(original_names)}')
            continue
        names.add(name)
    return names, reasons


def original_data_symbols(elf, lo, hi):
    return sorted((s for s in elf.symbols if s['type'] in (STT_NOTYPE, STT_OBJECT)
                   and 0 < s['section_index'] < 0xff00 and elf.sections[s['section_index']]['flags'] & 2
                   and lo <= int(s['va'], 16) < hi and s['name'] and s['name'] not in MARKER_NAMES
                   and not s['name'].startswith(('$', '.L'))), key=lambda s: int(s['va'], 16))


def valid_switch_word_dependency_shape(dep):
    """Validate the report's structural claim without granting data credit."""
    words = dep.get('word_spans') or []
    try:
        start = int(words[0]['scaffold_range'][0])
        end = int(words[-1]['scaffold_range'][1])
        first_object_offset = words[0]['object_offset']
        count = dep['compiler_word_count']
        label = words[0]['scaffold_name']
        match = re.fullmatch(r'jtbl_([0-9A-Fa-f]{8})', label)
        return (dep.get('credited') is False and
                dep.get('disposition') == 'uncredited_scaffold_dependency' and
                dep.get('kind') == 'compiler_region_switch_words' and
                dep.get('section') in ('.rodata', '.data') and
                type(count) is int and count > 0 and len(words) == count and
                match is not None and int(match.group(1), 16) == start and
                end - start == count * 4 and
                dep.get('c_object_section_size', 0) >= first_object_offset + count * 4 and
                all(word.get('scaffold_name') == label and
                    word.get('scaffold_range') == [start + i * 4, start + (i + 1) * 4] and
                    word.get('object_offset') == first_object_offset + i * 4 and
                    word.get('relocation_offset') == first_object_offset + i * 4 and
                    word.get('length') == 4 and
                    type(word.get('raw_rel_addend')) is int and
                    re.fullmatch(r'[0-9a-f]{64}', word.get('payload_sha256', '')) is not None
                    for i, word in enumerate(words)) and
                re.fullmatch(r'[0-9a-f]{64}', dep.get('selected_carve_sha256', '')) is not None and
                bool(dep.get('selected_carve_path')) and bool(dep.get('selected_carve_section')))
    except (KeyError, TypeError, ValueError, IndexError):
        return False


def uncredited_dependency_label(dep):
    if dep.get('kind') == 'compiler_region_switch_words':
        words = dep.get('word_spans') or []
        return words[0].get('scaffold_name') if words else None
    return dep.get('scaffold_name')


def linked_span_data_check(section, rng, elf, obj, data_row, args=None):
    """Check C data against the exact carved-object spans pinned by R22."""
    spans = [dict(span) for span in (data_row or {}).get('verified_location_spans', [])
             if span.get('section') == section]
    spans = [span for span in spans if rng['lo'] <= int(span.get('address', '0'), 16) < rng['hi']]
    if not spans:
        return False, dict(reasons=['linked-data report has no verified object spans in the declared run'],
                           range_source=rng.get('source')), set()
    selected_sha = (data_row or {}).get('linked_data_object_sha256', (data_row or {}).get('object_sha256'))
    if any(span.get('object_sha256') != selected_sha for span in spans):
        return False, dict(reasons=['verified object span hash differs from the selected linked-data object']), set()
    by_section = {item['name']: item for item in obj.sections}
    symbols = [item for item in obj.symbols if item['type'] in (STT_NOTYPE, STT_OBJECT)
               and item['name'] and not item['name'].startswith(('$', '.L'))
               and item['name'] not in MARKER_NAMES]
    aliases = [dict(alias) for alias in (data_row or {}).get('symbol_aliases', [])
               if alias.get('section') == section]
    aliases_by_original = {alias.get('scaffold_name'): alias for alias in aliases}
    originals = original_data_symbols(elf, rng['lo'], rng['hi'])
    runs = rng.get('runs')
    if runs:
        originals = [item for item in originals if any(lo <= int(item['va'], 16) < hi for lo, hi in runs)]
    reasons, accounted, matched = [], set(), set()
    dependency_names = set()
    dependency_proofs = []
    alias_address_witnesses = []
    raw_path = Path(args.obj).resolve() if args is not None else None
    raw_obj = RelObject(raw_path.read_bytes()) if raw_path is not None else None
    build_s = (Path(args.build_s).resolve() if args is not None and getattr(args, 'build_s', None)
               else raw_path.with_name(raw_path.name + '.s') if raw_path is not None else None)
    linked_path = (Path(args.unit_dir) / 'build' / f'{args.unit}.rom.elf').resolve() if args is not None else None
    linked_data = linked_path.read_bytes() if linked_path is not None and linked_path.is_file() else None
    empty_objects = {}
    if raw_obj is not None and build_s is not None and build_s.is_file() and linked_data is not None:
        from tools.inventory import ELF as InventoryELF
        linked_image = InventoryELF(linked_data, args.unit)
        outside = re.sub(r'^\s*#APP\s*$.*?^\s*#NO_APP\s*$', '',
                         build_s.read_text(errors='replace'), flags=re.M | re.S)
        for original in originals:
            if original['type'] != STT_OBJECT or original['size'] != 0:
                continue
            name, address = original['name'], int(original['va'], 16)
            owners = [s for s in originals if int(s['va'], 16) == address and
                      s['type'] == STT_OBJECT and s['size'] > 0]
            if len(owners) != 1:
                continue
            owner = owners[0]
            proofs = [s for s in spans if int(s.get('address', '0'), 16) == address and
                      name in s.get('object_symbols', []) and
                      owner['name'] in s.get('object_symbols', []) and owner['name'] in s.get('names', [])]
            selected = [s for s in symbols if s['name'] == name]
            selected_owner = [s for s in symbols if s['name'] == owner['name']]
            raw = [s for s in raw_obj.symbols if s['name'] == name]
            raw_owner = [s for s in raw_obj.symbols if s['name'] == owner['name']]
            if any(len(values) != 1 for values in (proofs, selected, selected_owner, raw, raw_owner)):
                continue
            span, selected, selected_owner, raw, raw_owner = (
                proofs[0], selected[0], selected_owner[0], raw[0], raw_owner[0])
            def identity(symbol, witness):
                return (symbol['type'], symbol['binding'], symbol['size']) == (
                    witness['type'], witness['binding'], witness['size'])
            linked_exact = all(len([s for s in linked_image.symbols
                                   if s['name'] == witness['name'] and int(s['va'], 16) == address and
                                   s.get('section') == section and identity(s, witness)]) == 1
                               for witness in (original, owner))
            compiler_span = span.get('compiler_input_span') or {}
            if (identity(selected, original) and identity(raw, original) and
                    identity(selected_owner, owner) and identity(raw_owner, owner) and linked_exact and
                    selected['section'] == selected_owner['section'] == span.get('c_object_section') and
                    selected['value'] == selected_owner['value'] == span.get('c_object_offset') and
                    raw['section'] == raw_owner['section'] == compiler_span.get('section') and
                    raw['value'] == raw_owner['value'] == compiler_span.get('offset') and
                    span.get('length') == owner['size'] and
                    data_row.get('object_sha256') == sha256_file(raw_path) and
                    data_row.get('assembly_sha256') == sha256_file(build_s) and
                    data_row.get('source_sha256') == sha256_file(Path(args.tu_c)) and
                    data_row.get('linked_elf_sha256') == sha256_file(linked_path) and
                    len(re.findall(r'^\s*\.type\s+' + re.escape(name) + r'\s*,\s*@object\s*$', outside, re.M)) == 1 and
                    len(re.findall(r'^\s*\.size\s+' + re.escape(name) + r'\s*,\s*0\s*$', outside, re.M)) == 1 and
                    len(re.findall(r'^' + re.escape(name) + r':\s*$', outside, re.M)) == 1):
                empty_objects[name] = dict(name=name, address=original['va'], size=0,
                                          credited_bytes=0, positive_owner=owner['name'])

    def elf_va_bytes(image, address, length):
        for ph in image.program_headers:
            lo, size = int(ph['va'], 16), ph['file_size']
            if ph['type'] == 1 and lo <= address and address + length <= lo + size:
                off = ph['file_offset'] + address - lo
                return image.data[off:off + length]
        return None

    # A report can also account an exact scaffold-backed compiler dependency
    # without crediting it as recovered C data. Validate the raw cc1 bytes,
    # selected carved-object bytes, original and linked bytes, plus every
    # current sidecar hash before admitting its original symbol to the
    # object-wide accounting set.
    for dep in (data_row or {}).get('uncredited_compiler_dependencies', []):
        if dep.get('section') != section or dep.get('kind') not in (
                'inline_assembly_data', 'compiler_region_switch_words'):
            continue
        if dep.get('kind') == 'compiler_region_switch_words':
            try:
                words = dep['word_spans']
                start = int(words[0]['scaffold_range'][0])
                length = len(words) * 4
                end = start + length
                raw_section = raw_obj.section(section)
                selected_path = Path(dep['selected_carve_path']).resolve()
                linker_path = Path(dep['linker_script_path']).resolve()
                selected_obj = RelObject(selected_path.read_bytes())
                selected_section = selected_obj.section(dep['selected_carve_section'])
                raw_bytes = raw_obj.bytes_of(raw_section)
                selected_bytes = selected_obj.bytes_of(selected_section)
                original_bytes = elf_va_bytes(elf, start, length)
                from tools.inventory import ELF as InventoryELF
                linked_image = InventoryELF(linked_data, args.unit) if linked_data is not None else None
                linked_bytes = elf_va_bytes(linked_image, start, length) if linked_image is not None else None
                relocations = {(sec, offset, kind) for sec, offset, kind, _name in raw_obj.relocations()}
                selected_relocations = {(sec, offset, kind) for sec, offset, kind, _name
                                        in selected_obj.relocations()}
                dep_label = uncredited_dependency_label(dep)
                first_offset = words[0]['object_offset']
                raw_table = raw_bytes[first_offset:first_offset + length]
                selected_offsets = [offset for offset in range(max(0, len(selected_bytes) - length + 1))
                                    if selected_bytes[offset:offset + length] == raw_table]
                selected_offset = selected_offsets[0] if len(selected_offsets) == 1 else None
                exact = (
                    valid_switch_word_dependency_shape(dep) and
                    raw_section is not None and
                    raw_section['size'] == dep.get('c_object_section_size') and
                    raw_section['align'] == dep.get('c_object_section_alignment') and
                    dep.get('object_sha256') == data_row.get('object_sha256') == sha256_file(raw_path) and
                    dep.get('source_sha256') == data_row.get('source_sha256') == sha256_file(Path(args.tu_c)) and
                    dep.get('assembly_sha256') == data_row.get('assembly_sha256') == sha256_file(build_s) and
                    dep.get('linked_elf_sha256') == data_row.get('linked_elf_sha256') ==
                    sha256_file(linked_path) and
                    dep.get('original_elf_sha256') == hashlib.sha256(elf.data).hexdigest() and
                    dep.get('selected_carve_sha256') == sha256_file(selected_path) and
                    dep.get('linker_script_sha256') == sha256_file(linker_path) and
                    selected_section is not None and selected_offset is not None and
                    original_bytes == linked_bytes and
                    all((section, word['relocation_offset'], 2) in relocations and
                        (selected_section['name'], selected_offset + i * 4, 2) in selected_relocations and
                        int.from_bytes(raw_bytes[word['object_offset']:word['object_offset'] + 4], 'little') ==
                        word['raw_rel_addend'] and
                        hashlib.sha256(raw_bytes[word['object_offset']:word['object_offset'] + 4]).hexdigest() ==
                        word['payload_sha256'] and
                        int.from_bytes(original_bytes[i * 4:(i + 1) * 4], 'little') ==
                        int(word['original_linked_word'], 16)
                        for i, word in enumerate(words)) and
                    not any(max(start, int(span.get('address', '0'), 16)) <
                            min(end, int(span.get('address', '0'), 16) + int(span.get('length', 0)))
                            for span in spans))
            except (KeyError, TypeError, ValueError, OSError, AttributeError, IndexError):
                exact = False
            if not exact:
                reasons.append(f'{uncredited_dependency_label(dep) or "switch-table dependency"}: uncredited compiler table '
                               'does not match exact current raw/selected/original/link evidence')
                continue
            dependency_names.add(dep_label)
            dependency_proofs.append(dict(name=dep_label, length=length, credited=False,
                                          raw_sha256=dep['object_sha256'],
                                          selected_sha256=dep['selected_carve_sha256']))
            continue
        try:
            lo, hi = map(int, dep['scaffold_range'])
            off, length = int(dep['c_object_offset']), int(dep['length'])
            csec = raw_obj.section(dep['c_object_section'])
            selected_sec = obj.section(dep['selected_section'])
            selected_off = int(dep['selected_section_offset'])
            alloc_path = Path(dep['allocation_report_path']).resolve()
            carve_path = Path(dep['carve_sidecar_path']).resolve()
            selected_path = Path(dep['selected_object_path']).resolve()
            allocation_reports = [Path(p).resolve() for p in (getattr(args, 'allocation_report', None) or [])]
            carve_reports = [Path(p).resolve() for p in (getattr(args, 'data_carve_report', None) or [])]
            payload = raw_obj.bytes_of(csec)[off:off + length]
            selected_bytes = obj.bytes_of(selected_sec)[selected_off:selected_off + length]
            original_bytes = elf_va_bytes(elf, lo, length)
            from tools.inventory import ELF as InventoryELF
            linked_image = InventoryELF(linked_data, args.unit) if linked_data is not None else None
            linked_bytes = elf_va_bytes(linked_image, lo, length) if linked_image is not None else None
            label = dep['compiler_assembly_label']
            asm_text = build_s.read_text(errors='replace') if build_s and build_s.is_file() else ''
            label_pos = asm_text.find(label + ':')
            app_pos = asm_text.rfind('#APP', 0, label_pos) if label_pos >= 0 else -1
            no_app_pos = asm_text.find('#NO_APP', label_pos) if label_pos >= 0 else -1
            alloc_report = json.loads(alloc_path.read_text())
            allocation_input = alloc_report.get('input') or {}
            allocation_output = alloc_report.get('output') or {}
            carve_report = json.loads(carve_path.read_text())
            allocated_path = Path(allocation_output.get('path', '/nonexistent')).resolve()
            original_names = [s for s in original_data_symbols(elf, lo, hi)
                              if s['name'] == dep['scaffold_name'] and int(s['va'], 16) == lo]
            exact = (
                dep.get('credited') is False and dep.get('disposition') == 'uncredited_scaffold_dependency' and
                dep.get('section') == section and hi - lo == length and lo < hi and
                dep.get('c_object_section') == section and csec is not None and
                off >= 0 and off + length <= csec['size'] and selected_sec is not None and
                selected_path == Path(getattr(args, 'data_carve_object', None) or
                                      getattr(args, 'data_gate_object', '')).resolve() and
                selected_path == Path(args.data_gate_object).resolve() and
                dep.get('selected_object_sha256') == sha256_file(selected_path) == data_row.get('object_sha256') and
                dep.get('raw_object_sha256') == sha256_file(raw_path) and
                dep.get('source_sha256') == data_row.get('source_sha256') == sha256_file(Path(args.tu_c)) and
                dep.get('assembly_sha256') == data_row.get('assembly_sha256') == sha256_file(build_s) and
                dep.get('linked_elf_sha256') == data_row.get('linked_elf_sha256') == sha256_file(linked_path) and
                alloc_path in allocation_reports and sha256_file(alloc_path) == dep.get('allocation_report_sha256') and
                allocation_input.get('sha256') == sha256_file(raw_path) and
                allocation_output.get('sha256') == dep.get('allocated_object_sha256') and
                Path(carve_report.get('object', '/nonexistent')).resolve() == allocated_path and
                Path(carve_report.get('out', '/nonexistent')).resolve() == selected_path and
                carve_path in carve_reports and sha256_file(carve_path) == dep.get('carve_sidecar_sha256') and
                dep.get('selected_section_offset') == selected_off and
                hashlib.sha256(payload).hexdigest() == dep.get('payload_sha256') and
                selected_bytes == payload and original_bytes == payload and linked_bytes == payload and
                len(original_names) == 1 and original_names[0]['size'] in (0, length) and
                label_pos >= 0 and app_pos >= 0 and no_app_pos >= label_pos and
                not any(max(lo, int(span.get('address', '0'), 16)) <
                        min(hi, int(span.get('address', '0'), 16) + int(span.get('length', 0)))
                        for span in spans))
        except (KeyError, TypeError, ValueError, OSError, AttributeError, IndexError):
            exact = False
        if not exact:
            reasons.append(f'{dep.get("scaffold_name", "inline assembly data")}: uncredited dependency does not match exact current raw/selected/original/link evidence')
            continue
        dependency_names.add(dep['scaffold_name'])
        accounted.add(dep['scaffold_name'])
        matched.add(dep['scaffold_name'])
        dependency_proofs.append(dict(name=dep['scaffold_name'], offset=off, length=length,
                                      credited=False, raw_sha256=dep['raw_object_sha256'],
                                      selected_sha256=dep['selected_object_sha256']))
    # A gate may prove a real named C object whose retail bytes have no named
    # original ELF symbol (for example a local literal array). Such a name is
    # creditable only through this exact selected-object span; the linked-data
    # report also binds its bytes and final linked address.
    for span in spans:
        try:
            offset = int(span['c_object_offset'])
            length = int(span['length'])
            section_size = int(span['c_object_section_size'])
            section_align = int(span['c_object_section_alignment'])
        except (KeyError, TypeError, ValueError):
            reasons.append('malformed selected-object span for a named C data object')
            continue
        csec = by_section.get(span.get('c_object_section'))
        address = int(span.get('address', '0'), 16)
        if (csec is None or csec['size'] != section_size or csec['align'] != section_align or
                offset < 0 or length <= 0 or offset + length > section_size or
                not any(lo <= address and address + length <= hi
                        for lo, hi in (runs or [(rng['lo'], rng['hi'])]))):
            reasons.append('selected-object span lies outside its exact section or declared run')
            continue
        for object_name in span.get('object_symbols', []):
            if object_name in empty_objects and int(empty_objects[object_name]['address'], 16) == address:
                continue
            candidates = [symbol for symbol in symbols if symbol['name'] == object_name]
            if (len(candidates) != 1 or candidates[0].get('section') != csec['name'] or
                    candidates[0].get('value') != offset or candidates[0].get('size', 0) <= 0 or
                    candidates[0].get('size', 0) > length):
                reasons.append(f'{object_name}: no unique selected-object symbol matches its verified span')
                continue
            matched.add(object_name)
            accounted.add(object_name)
        # Anonymous cc1 strings/tables can have a scaffold label without a
        # named ELF symbol. Their exact bytes and relocations are proved by
        # this current, hash-bound data gate; do not require a fabricated C
        # symbol merely to account for that label.
        if not any(int(original['va'], 16) == address for original in originals):
            matched.update(span.get('names', []))
    for original in originals:
        original_name = original['name']
        address = int(original['va'], 16)
        if original_name in empty_objects:
            accounted.add(original_name)
            matched.add(original_name)
            continue
        if original_name in dependency_names:
            # The linker scaffold remains the owner; the C switch words are
            # accepted only as a physical dependency of their compiler-emitted
            # function and receive no source-data credit.
            continue
        alias = aliases_by_original.get(original_name)
        object_name = alias.get('object_symbol') if alias else original_name
        candidates = [item for item in symbols if item['name'] == object_name]
        if len(candidates) != 1:
            reasons.append(f'original {original_name} @{original["va"]} has no unique selected C object {object_name}')
            continue
        symbol = candidates[0]
        proof_spans = [span for span in spans
                       if int(span.get('address', '0'), 16) == address and
                       object_name in (span.get('object_symbols') or []) and
                       object_name in (span.get('names') or [])]
        if len(proof_spans) != 1:
            reasons.append(f'{original_name} @{original["va"]}: no unique linked-data span for {object_name}')
            continue
        span = proof_spans[0]
        csec = by_section.get(span.get('c_object_section'))
        try:
            offset = int(span['c_object_offset'])
            length = int(span['length'])
            span_size = int(span['c_object_section_size'])
            span_align = int(span['c_object_section_alignment'])
        except (KeyError, TypeError, ValueError):
            reasons.append(f'{original_name}: malformed linked object span')
            continue
        if (csec is None or csec['size'] != span_size or csec['align'] != span_align or
                offset < 0 or length <= 0 or offset + length > csec['size'] or
                symbol['section'] != csec['name'] or symbol['value'] != offset or
                symbol['size'] != original['size'] or symbol['type'] != original['type'] or
                symbol['binding'] != original['binding']):
            reasons.append(f'{original_name} -> {object_name}: selected object identity/span differs from original')
            continue
        if alias:
            linked_matches = []
            linked_other = []
            if linked_data is not None:
                from tools.inventory import ELF as InventoryELF
                linked_image = InventoryELF(linked_data, args.unit)
                named_matches = [item for item in linked_image.symbols
                                 if item.get('name') == object_name and
                                 int(item.get('va', '0'), 16) == address]
                original_section = elf.sections[original['section_index']]
                for item in named_matches:
                    index = item.get('section_index', 0)
                    exact = (item.get('size') == original['size'] and
                             item.get('type') == original['type'] and
                             item.get('binding') == original['binding'] and
                             item.get('section') == original.get('section') and
                             0 < index < len(linked_image.sections) and
                             linked_image.sections[index]['type'] == original_section['type'] and
                             linked_image.sections[index]['flags'] & 2)
                    if exact:
                        linked_matches.append(item)
                    else:
                        linked_other.append(item)
            address_witnesses = [item for item in linked_other
                                 if item.get('section_index') == 0xfff1 and item.get('section') is None and
                                 item.get('binding') == 1 and item.get('type') == STT_NOTYPE and
                                 item.get('size') == 0]
            if (alias.get('section') != section or alias.get('scaffold_name') != original_name or
                    alias.get('object_symbol') != object_name or
                    int(alias.get('address', '0'), 16) != address or alias.get('size') != original['size'] or
                    alias.get('binding') != BIND.get(original['binding']) or
                    alias.get('type') != {STT_NOTYPE: 'NOTYPE', STT_OBJECT: 'OBJECT'}.get(original['type']) or
                    alias.get('binding') != BIND.get(symbol['binding']) or
                    alias.get('type') != {STT_NOTYPE: 'NOTYPE', STT_OBJECT: 'OBJECT'}.get(symbol['type']) or
                    alias.get('size') != symbol['size'] or len(linked_matches) != 1 or
                    linked_matches[0].get('size') != original['size'] or
                    linked_matches[0].get('type') != original['type'] or
                    linked_matches[0].get('binding') != original['binding'] or
                    len(address_witnesses) != len(linked_other) or
                    data_row.get('linked_elf_sha256') != sha256_file(linked_path)):
                reasons.append(f'{original_name} -> {object_name}: linked alias identity is not exact')
                continue
            alias_address_witnesses.extend(dict(name=item['name'], address=item['va'],
                                                binding='GLOBAL', type='NOTYPE', size=0,
                                                section_index=0xfff1, credited=False,
                                                original_name=original_name,
                                                disposition='absolute_linker_address_witness')
                                           for item in address_witnesses)
        accounted.add(original_name)
        # The exact selected-object span proves this original identity even
        # when the C object uses a different, verified alias name.  Keep both
        # names in the accounting set; alias identity, size, binding, type,
        # address and linked-symbol checks above remain mandatory.
        matched.add(original_name)
        accounted.add(object_name)
        matched.add(object_name)
    for empty in empty_objects.values():
        if empty['positive_owner'] not in accounted:
            reasons.append(f'{empty["name"]}: colocated positive owner did not pass its ordinary linked span checks')
    expected_c_names = {name for item in (data_row or {}).get('verified_locations', [])
                        if item.get('section') == section and rng['lo'] <= int(item['address'], 16) < rng['hi']
                        for name in item.get('names', [])}
    expected_c_names |= {alias.get('object_symbol') for alias in aliases
                         if rng['lo'] <= int(alias.get('address', '0'), 16) < rng['hi']}
    if expected_c_names - matched:
        reasons.append(f'linked data names lack matching carved-object spans: {sorted(expected_c_names - matched)}')
    return not reasons, dict(reasons=reasons, range_source=rng.get('source'),
                             original_symbols=len(originals), matched=len(accounted),
                             selected_spans=len(spans), selected_object_size=sum(by_section[s['c_object_section']]['size']
                                                                               for s in spans
                                                                               if s.get('c_object_section') in by_section),
                             uncredited_dependencies=dependency_proofs,
                             alias_address_witnesses=alias_address_witnesses,
                             verified_empty_objects=list(empty_objects.values()),
                             detail='each claimed original identity is pinned to its exact selected-object span'), accounted


def data_section_check(section, rng, elf, obj, scaffold_names):
    """(ok, detail) for one C-owned data section, and the names it accounts for."""
    reasons, accounted = [], set()
    if rng.get('lo') is None:
        return False, dict(reasons=['C-owned section without a known original range (pass '
                                    '--c-owned SECTION=START:END)']), accounted
    storage_rows = rng.get('verified_storage') or []
    if section in ('.bss', '.sbss') and storage_rows:
        reasons, accounted = [], set()
        expected_type = {'NOTYPE': STT_NOTYPE, 'OBJECT': STT_OBJECT}
        for storage in storage_rows:
            candidates = [symbol for symbol in obj.symbols if symbol.get('name') == storage.get('name')]
            if len(candidates) != 1:
                reasons.append(f'{storage.get("name")}: verified storage owner is not defined once by the C object')
                continue
            symbol = candidates[0]
            common_symbol = symbol.get('shndx') in (SHN_COMMON, SHN_MIPS_SCOMMON)
            allowed_sections = (section, '.scommon') if section == '.sbss' else ('.bss', '.scommon')
            if (symbol.get('binding') != storage.get('bind') or
                    symbol.get('type') != expected_type.get(storage.get('type')) or
                    symbol.get('size') not in (0, storage.get('size')) or
                    (symbol.get('section') not in allowed_sections and not common_symbol)):
                reasons.append(f'{storage.get("name")}: emitted identity differs from hash-bound linked storage')
                continue
            accounted.add(storage['name'])
        storage_by_address = {int(item['address'], 16): item for item in storage_rows}
        originals = original_data_symbols(elf, rng['lo'], rng['hi'])
        if rng.get('runs'):
            originals = [item for item in originals
                         if any(lo <= int(item['va'], 16) < hi for lo, hi in rng['runs'])]
        for original in originals:
            address = int(original['va'], 16)
            storage = storage_by_address.get(address)
            if storage is None:
                reasons.append(f'original {original["name"]} @{original["va"]} is not covered by a '
                               'hash-bound C storage owner')
                continue
            if original['size'] > 0 and (original['binding'] != storage.get('bind') or
                                         original['size'] != storage.get('size') or
                                         original['type'] != expected_type.get(storage.get('type'))):
                reasons.append(f'{original["name"]}: meaningful original ELF identity differs from linked '
                               'C storage owner')
                continue
            accounted.add(original['name'])
        declared_storage = {item.get('name') for item in storage_rows}
        if not declared_storage.issubset(set(rng.get('verified_names') or [])):
            reasons.append('verified storage names are not present in the C-owned data evidence')
        return not reasons, dict(reasons=reasons, range_source=rng['source'],
                                 original_symbols=len(originals), matched=len(accounted),
                                 object_size=sum(int(item.get('size', 0)) for item in storage_rows),
                                 piece_start=f'0x{rng["lo"]:08x}', verified_storage=storage_rows,
                                 detail='C storage emission and final linked placement are hash-bound by the '
                                        'linked-data gate'), accounted
    names = {'.rodata': ('.rodata', '.rdata')}.get(section, (section,))
    secs = [x for x in obj.sections if x['name'] in names]
    if len(secs) != 1:
        detail = (f'the object has no {section} section, so the C-owned range '
                  f'0x{rng["lo"]:08x}..0x{rng["hi"]:08x} does not come from this TU\'s C'
                  if not secs else f'the object has {len(secs)} {section} sections')
        return False, dict(reasons=[detail], partial=rng.get('partial'),
                           range_source=rng.get('source')), accounted
    sec = secs[0]
    lo, hi = rng['lo'], rng['hi']
    originals = original_data_symbols(elf, lo, hi)
    runs = rng.get('runs')
    if runs:
        # several runs: the original symbols of the runs, not of the scaffold items
        # between them; the build places each run's slice at its own address
        # (tools/tu/data_carve.py split), so there is no single piece start to check
        originals = [o for o in originals if any(a <= int(o['va'], 16) < b for a, b in runs)]
    defined = [x for x in obj.symbols if x['shndx'] == sec['index'] and x['type'] in (STT_NOTYPE, STT_OBJECT)
               and x['name'] and not x['name'].startswith(('$', '.L')) and x['name'] not in scaffold_names
               and x['name'] not in MARKER_NAMES]
    by_name = {}
    for x in defined:
        by_name.setdefault(x['name'], []).append(x)
    deltas, matched = set(), []
    for alias in rng.get('symbol_aliases', []):
        name = alias['object_symbol']
        cands = by_name.get(name, [])
        if len(cands) != 1:
            reasons.append(f'scaffold alias {alias["scaffold_name"]} -> {name} is not defined once by the C '
                           f'object in {section}')
            continue
        x = cands[0]
        expected_binding = {'LOCAL': 0, 'GLOBAL': 1, 'WEAK': 2}.get(alias.get('binding'))
        expected_type = {'NOTYPE': STT_NOTYPE, 'OBJECT': STT_OBJECT, 'FUNC': STT_FUNC}.get(alias.get('type'))
        try:
            address = int(alias['address'], 16)
            size = int(alias['size'])
        except (KeyError, TypeError, ValueError):
            reasons.append(f'scaffold alias {alias["scaffold_name"]} -> {name} has malformed address/size')
            continue
        original_alias_ok = True
        if alias.get('evidence') == 'exact_same_address_original_and_linked_C_OBJECT':
            original_alias_ok = any(
                o['name'] == alias.get('scaffold_name') and int(o['va'], 16) == address and
                o['binding'] == expected_binding and o['type'] == expected_type and o['size'] == size
                for o in originals)
        if (alias.get('section') != section or expected_binding is None or expected_type is None or
                x['binding'] != expected_binding or x['type'] != expected_type or x['size'] != size or
                not original_alias_ok):
            reasons.append(f'{alias["scaffold_name"]} -> {name}: object {BIND.get(x["binding"])}/{x["size"]}/'
                           f'type {x["type"]} vs gate alias {alias.get("binding")}/{size}/type '
                           f'{alias.get("type")} in {section}' +
                           ('' if original_alias_ok else '; original ELF identity mismatch'))
            continue
        accounted.add(name)
        deltas.add(address - x['value'])
        matched.append((x['value'], x['value'] + x['size'], name))
    for o in originals:
        cands = by_name.get(o['name'], [])
        if len(cands) != 1:
            direct_alias = next((alias for alias in rng.get('symbol_aliases', [])
                                 if alias.get('evidence') == 'exact_same_address_original_and_linked_C_OBJECT' and
                                 alias.get('scaffold_name') == o['name'] and
                                 int(alias.get('address', '0'), 16) == int(o['va'], 16) and
                                 alias.get('binding') == BIND.get(o['binding']) and
                                 alias.get('type') == {STT_NOTYPE: 'NOTYPE', STT_OBJECT: 'OBJECT'}.get(o['type']) and
                                 alias.get('size') == o['size']), None)
            alias_candidates = by_name.get(direct_alias.get('object_symbol'), []) if direct_alias else []
            if len(alias_candidates) == 1:
                x = alias_candidates[0]
                if (x['binding'] == o['binding'] and x['type'] == o['type'] and x['size'] == o['size'] and
                        direct_alias.get('section') == section):
                    accounted.add(o['name'])
                    continue
            if o['name'] in rng.get('generated_aliases', []):
                address = int(o['va'], 16)
                if any(int(span[0]) <= address < int(span[1]) for span in
                       (rng.get('compiler_owned_ranges') or [])):
                    accounted.add(o['name'])
                    continue
            reasons.append(f'original {o["name"]} @{o["va"]} ({o["size"]} bytes) is not defined once by the C '
                           f'object in {section}')
            continue
        x = cands[0]
        accounted.add(o['name'])
        if x['binding'] != o['binding'] or x['size'] != o['size'] or x['type'] != o['type']:
            reasons.append(f'{o["name"]}: object {BIND.get(x["binding"])}/{x["size"]}/type {x["type"]} vs original '
                           f'{BIND.get(o["binding"])}/{o["size"]}/type {o["type"]}')
        deltas.add(int(o['va'], 16) - x['value'])
        matched.append((x['value'], x['value'] + x['size'], o['name']))
    expected_names = {o['name'] for o in originals} | set(rng.get('verified_names') or [])
    if rng.get('source') == 'config/tu/data-carves.json' and rng.get('verified_names'):
        by_name_defined = {name: values for name, values in by_name.items()}
        loc_address = {name: int(item['address'], 16)
                       for item in rng.get('verified_locations', [])
                       for name in item.get('names', [])}
        for name in rng['verified_names']:
            if len(by_name_defined.get(name, [])) != 1:
                address = loc_address.get(name)
                anonymous = any(a <= address < b for a, b in rng.get('anonymous_ranges', [])
                                ) if address is not None else False
                if not anonymous:
                    reasons.append(f'linked-data-gate symbol {name} is neither defined once nor covered by '
                                   f'compiler-proven anonymous constants in {section}')
            else:
                accounted.add(name)
    extra = sorted(x['name'] for x in defined if x['name'] not in expected_names)
    if extra:
        reasons.append(f'named data the original range lacks: {extra[:10]}')
    piece = None
    if runs:
        deltas = set()
    if len(deltas) > 1:
        reasons.append(f'symbols disagree on the piece start: {sorted(hex(d) for d in deltas)[:4]}')
    elif deltas:
        piece = deltas.pop()
    if piece is None and rng['exact'] and sec['size'] > hi - lo:
        reasons.append(f'the object\'s {section} ({sec["size"]} bytes) does not fit the C-owned range '
                       f'0x{lo:08x}..0x{hi:08x}')
    if piece is not None:
        if rng['exact'] and piece != lo:
            reasons.append(f'piece starts at 0x{piece:08x}, range starts at 0x{lo:08x}')
        if piece < lo or piece + sec['size'] > hi:
            reasons.append(f'piece 0x{piece:08x}+0x{sec["size"]:x} does not fit 0x{lo:08x}..0x{hi:08x}')
        if rng['alo'] is not None and (piece > rng['alo'] or piece + sec['size'] < rng['ahi']):
            reasons.append('piece does not cover the anchored span of the manifest')
    gaps, pos = [], 0
    for a, b, _ in sorted(matched):
        if a > pos:
            gaps.append((pos, a))
        pos = max(pos, b)
    if pos < sec['size']:
        gaps.append((pos, sec['size']))
    data = obj.bytes_of(sec)
    bad = []
    for a, b in gaps:
        zero = not data or not any(data[a:b])
        if (b - a) < 16 and zero:
            continue
        if section in RODATA_ANON:
            continue
        if section in CARVED_ANON and (rng.get('anon_bytes') or {}).get('equal'):
            continue
        bad.append(f'+0x{a:x}..+0x{b:x}' + ('' if zero else ' (nonzero)'))
    if bad:
        reasons.append(f'uncovered bytes that are not alignment padding: {bad[:6]}')
    detail = dict(reasons=reasons, range_source=rng['source'], original_symbols=len(originals),
                  matched=len(matched), object_size=sec['size'],
                  piece_start=f'0x{piece:08x}' if piece is not None else None)
    if rng.get('anon_bytes') is not None:
        detail['anon_bytes'] = rng['anon_bytes']
    return not reasons, detail, accounted


# ---------------------------------------------------------------------------
# Accepted C records: a claimed body must still be its record's body
# ---------------------------------------------------------------------------
# A record's source can be corrected after a TU was imported. The whole-file
# gate cannot see it when both spellings compile to the same bytes (main/tu092
# `xglCdArcCheck`), so every claimed C function whose accepted record resolves
# to a body is compared with that body verbatim. The only allowed difference is
# a per-TU adaptation named in the TU's adaptations file, and when the entry
# carries the exact old/new text the difference must be exactly that.
_C_RECORD_INDEX = None
_ADAPTATION_FILES = ('adaptations.json', 'merge-adaptations.json', 'tu-adaptations.json')
# Unpublished workflow evidence: config/adaptations/<unit>.json holds the declared
# differences between a TU body and its accepted record (the build never reads them).
_ADAPTATION_UNIT_FILE = 'config/adaptations/%s.json'
_ADAPTATION_PROSE = ('reason', 'detail', 'before', 'after', 'resolution', 'note', 'area')


def c_record_index():
    """{(unit, va): [(record path, record, function entry)]} over accepted C records."""
    global _C_RECORD_INDEX
    if _C_RECORD_INDEX is None:
        index = {}
        for path in sorted((ROOT / 'config/units').glob('*.json')):
            try:
                record = json.loads(path.read_bytes())
            except ValueError:
                continue
            if record.get('source_language') == 'ee-asm':
                continue
            source = (record.get('source') or {}).get('path', '')
            if not source.endswith('.c'):
                continue
            for f in record.get('functions', []):
                index.setdefault((record.get('unit'), str(f.get('va', '')).lower()), []).append(
                    (rel(path), record, f))
        _C_RECORD_INDEX = index
    return _C_RECORD_INDEX


def record_body(record, name, source_rel=None, expected_sha=None):
    """(text, error): the function's current definition text in the record's source."""
    source = ROOT / (source_rel or relative_source(record))
    want = expected_sha or record['source']['sha256']
    if not source.is_file():
        return None, f'record source missing: {rel(source)}'
    digest = sha256_file(source)
    if digest != want:
        return None, (f'record source {rel(source)} no longer matches the sha256 of its record '
                      f'({digest[:12]}... vs {want[:12]}...)')
    try:
        tu = tu_edit.TU(source.read_text(encoding='utf-8', errors='surrogateescape'))
    except tu_edit.ParseError as e:
        return None, f'record source {rel(source)} cannot be split: {e}'
    block = tu.by_name.get(name)
    if block is None or block.state != 'c':
        return None, f'record source {rel(source)} has no C definition of {name}'
    return tu.text[block.core_start:block.core_end], None


def safe_relative(path, what='record source'):
    if '..' in Path(path).parts or Path(path).is_absolute():
        raise AuditError(f'unsafe {what} path {path!r}')
    return path


def relative_source(record, tu_id=None):
    """The file this record's body is read from, for the TU being audited.

    A record that spans two original translation units (the accepted unit was one
    allocation, the published TUs are two objects) carries a `tu_sources` entry per
    TU; without `tu_id`, or without a matching entry, the record's own `source` is
    used.
    """
    source = record.get('source') or {}
    for entry in source.get('tu_sources') or []:
        if tu_id is not None and entry.get('tu') == tu_id and entry.get('path'):
            return safe_relative(entry['path'])
    return safe_relative(source['path'])


def relative_source_sha(record, tu_id=None):
    source = record.get('source') or {}
    for entry in source.get('tu_sources') or []:
        if tu_id is not None and entry.get('tu') == tu_id and entry.get('path'):
            return entry.get('sha256') or source['sha256']
    return source['sha256']


def retired_origin(record):
    """The archived pre-migration source of a retired record, when it is still here.

    Retiring the pre-migration layout (task tu-retire-legacy-src, 2026-09-12) moved
    every duplicated legacy source to `.work/archive/pre-tu-source-20260912/` and
    repointed its record at the published TU file. The archive is local and
    gitignored, so it is used when present and its absence is reported, never
    silently treated as agreement.
    """
    retired = ((record.get('source') or {}).get('retired_from') or {})
    path, digest = retired.get('archived_to'), retired.get('sha256')
    if not path or not digest:
        return None
    try:
        path = safe_relative(path, 'archived record source')
    except AuditError:
        return None
    return dict(path=path, sha256=digest, present=(ROOT / path).is_file())


def load_adaptations(paths):
    """Normalise the per-TU adaptation declarations of the importers/integrator."""
    entries = []
    for path in paths:
        p = Path(path)
        if not p.is_file():
            continue
        try:
            data = json.loads(p.read_text())
        except ValueError:
            continue
        raw = data.get('adaptations') if isinstance(data, dict) else data
        if isinstance(raw, list):
            for k, e in enumerate(raw):
                if not isinstance(e, dict):
                    continue
                symbols = {e[f] for f in ('symbol', 'function') if isinstance(e.get(f), str)}
                for f in ('symbols', 'functions'):
                    if isinstance(e.get(f), list):
                        symbols |= {x for x in e[f] if isinstance(x, str)}
                prose = ' '.join(str(e.get(f, '')) for f in _ADAPTATION_PROSE)
                entries.append(dict(source=rel(p), index=k, tu=str(e.get('tu') or e.get('id') or ''),
                                    symbols=symbols, prose=prose, old=e.get('old'), new=e.get('new'),
                                    kind=e.get('kind'), reason=(e.get('reason') or e.get('detail') or '')[:200]))
        elif isinstance(data, dict):
            # {unit: {tu: {replace: [{function, old, new, reason}], ...}}}
            for unit, tus in data.items():
                if not isinstance(tus, dict):
                    continue
                for tu_name, spec in tus.items():
                    if not isinstance(spec, dict):
                        continue
                    reason = str(spec.get('reason', ''))[:200]
                    for k, r in enumerate(spec.get('replace', []) or []):
                        entries.append(dict(source=rel(p), index=k, tu=f'{unit}/{tu_name}',
                                            symbols={r['function']} if r.get('function') else set(),
                                            prose=str(r.get('reason', '')) + ' ' + reason,
                                            old=r.get('old'), new=r.get('new'), kind='replace',
                                            reason=str(r.get('reason', reason))[:200]))
                    if not spec.get('replace'):
                        entries.append(dict(source=rel(p), index=-1, tu=f'{unit}/{tu_name}', symbols=set(),
                                            prose=reason, old=None, new=None, kind='tu', reason=reason))
    return entries


def discover_adaptations(paths, tu_c, unit_dir):
    """Explicit files, the ones beside the TU or its unit, and their declared inputs.

    A merged tree keeps its own decisions and names the deliverables it merged in
    `inputs`; each of those keeps the record of what that importer adapted, so the
    declared inputs are followed (bounded, without cycles).
    """
    found, seen = [], set()

    def add(path):
        resolved = str(Path(path).resolve())
        if resolved not in seen and Path(resolved).is_file():
            seen.add(resolved)
            found.append(resolved)
            return True
        return False

    for path in paths or []:
        add(path)
    # The declarations for a unit live outside the built tree (workflow evidence).
    unit_name = Path(tu_c).resolve().parent.name
    for base in (Path.cwd(), Path(unit_dir).resolve().parent):
        add(Path(base) / (_ADAPTATION_UNIT_FILE % unit_name))
    # A candidate carries its own exact accepted-body replacements beside its
    # TU. They follow inherited unit declarations and are transported/pinned
    # with the same candidate through review and serial publication.
    add(Path(tu_c).with_suffix('.adaptations.json'))
    for start in (Path(tu_c).parent, Path(unit_dir)):
        here = Path(start).resolve()
        for _ in range(4):
            for name in _ADAPTATION_FILES:
                add(here / name)
            if here.parent == here:
                break
            here = here.parent
    index = 0
    while index < len(found) and index < 64:
        current = Path(found[index])
        index += 1
        try:
            data = json.loads(current.read_text())
        except (ValueError, OSError):
            continue
        if not isinstance(data, dict):
            continue
        for entry in data.get('inputs') or []:
            if not isinstance(entry, str) or entry.startswith('/') or '..' in Path(entry).parts:
                continue
            base = ROOT / entry
            for name in _ADAPTATION_FILES:
                add(base / name if base.is_dir() else base.parent / name)
    return found


def _tu_matches(entry_tu, tu_id, tu_rel):
    if not entry_tu or entry_tu.lower() in ('none', 'range', 'all', '-'):
        return None  # a declaration that names no TU
    keys = {k.lower() for k in (tu_id or '', tu_rel, Path(tu_rel).stem) if k}
    value = entry_tu.lower()
    return any(k and (k in value or value in k or Path(k).stem == Path(value).stem) for k in keys)


def adaptations_for(entries, tu_id, tu_rel, function):
    """Declarations that name this function for this TU.

    An entry that carries exact `old`/`new` text is applied to a record body, so
    it must name the function it rewrites: a prose mention of a *different*
    function (a reason that explains why the TU's other users of the same object
    need the canonical declaration) would otherwise apply that entry's
    replacement to them as well and fail as `adaptation_text_not_found`. Only a
    prose-only declaration, which never rewrites anything, keeps the mention
    fallback.
    """
    hits = []
    for e in entries:
        match = _tu_matches(e['tu'], tu_id, tu_rel)
        named = function in e['symbols']
        has_text = bool(e.get('old') and e.get('new'))
        mentioned = (not has_text
                     and re.search(r'\b' + re.escape(function) + r'\b', e['prose']) is not None)
        if (named and match is not False) or (mentioned and match is True):
            hits.append(e)
    return hits


def record_digest(record_path):
    """The sha256 of the accepted record file the audit actually read.

    The audited tree supplies the records (see resolve_root), and a worker
    sandbox may legitimately stage a narrowed one for a re-cut accepted group.
    Reporting the digest of the file that was read is what keeps that from
    becoming credit nobody can check: the reviewer re-runs the audit against
    the published tree and sees at a glance whether the record it judged is the
    published record or a candidate for one.
    """
    try:
        return sha256_file(ROOT / record_path)
    except OSError:
        return None


def record_claims_tu(record, tu_id):
    """Does this accepted record say the TU under audit is what carries its body?

    Only such a record makes a statement about this TU's C set. A record whose
    source is still a pre-migration file of the legacy per-function pipeline
    describes an acceptance the TU has NOT imported yet -- main/tu107 delivers
    `nmlPacketAddTransMicrocode` as INCLUDE_ASM while
    config/units/math-main-nml-packet-trans-microcode.json holds its accepted C,
    and the per-TU ledger records exactly that (state `asm`, no records). Treating
    it as a function this TU lost would turn a recorded, legitimate backlog into a
    failure, so those are reported as `not_carried` instead.
    """
    source = record.get('source') or {}
    if tu_id and source.get('tu') == tu_id:
        return True
    return any(entry.get('tu') == tu_id for entry in source.get('tu_sources') or [])


def tu_accepted_c_functions(unit, tu_id, start, end):
    """([carried], [not carried]) accepted C records of the TU spanning [start, end).

    Keyed by the original address, so a record whose functions were split across
    two published TUs contributes only the ones this TU carries, and a name that
    occurs in more than one unit cannot be borrowed from the wrong one. Each entry
    is (record path, record, function entry, address).
    """
    carried, other = [], []
    if start is None:
        return carried, other
    for (record_unit, va), hits in c_record_index().items():
        if record_unit != unit or not va:
            continue
        try:
            address = int(va, 16)
        except ValueError:
            continue
        if not start <= address < end:
            continue
        for record_path, record, entry in hits:
            row = (record_path, record, entry, address)
            (carried if record_claims_tu(record, tu_id) else other).append(row)
    key = lambda row: (row[3], row[0])
    return sorted(carried, key=key), sorted(other, key=key)


# ---------------------------------------------------------------------------
# Comment-neutral record fidelity (user direction 2026-09-19)
# ---------------------------------------------------------------------------
# A change that only removes, rewords or adds C comments never blocks anything
# and never counts as an error. The published source is edited by hand (on
# 2026-09-16 workflow citations were removed from comments of
# src/main/game_camera.c, which moved `MoveHokan`'s body off its record's
# per-function body pin). A hash cannot tell a comment from code, so when a
# body no longer hashes to its pin the pinned text is recovered -- from the
# repository history of the record's source, the pre-migration archive or a
# C file the record itself names -- verified against the pin, and compared
# with the TU body as C token streams with comments treated as whitespace
# (tools/tu_edit.py `same_code`). Equal streams pass as
# `comment_only_drift_*`; a code difference, or a pin nothing in reach
# reproduces, is judged exactly as before.
_PRE_TU_ARCHIVE = '.work/archive/pre-tu-source-20260912'
_GIT_REVISION_LIMIT = 64
_GIT_TIMEOUT = 30
_WITNESS_CACHE = {}


def _git(repo, argv, stdin=None):
    """stdout bytes of a bounded, read-only git command, or None."""
    try:
        proc = subprocess.run(['git', '-C', str(repo), *argv], input=stdin, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, timeout=_GIT_TIMEOUT, start_new_session=True)
    except (OSError, subprocess.SubprocessError):
        return None
    return proc.stdout if proc.returncode == 0 else None


def _git_versions(source_rel):
    """[(label, bytes)]: the index copy and up to _GIT_REVISION_LIMIT committed
    versions of `source_rel`, newest first, from the repository of the audited
    tree and the tool's own (a worker sandbox has no history of its own)."""
    found, paths = [], set()
    for tree in (ROOT, TOOL_ROOT):
        top = _git(tree, ['rev-parse', '--show-toplevel'])
        if not top:
            continue
        top = Path(top.decode('utf-8', 'surrogateescape').strip())
        try:
            git_path = Path(os.path.abspath(Path(tree) / source_rel)).relative_to(top).as_posix()
        except ValueError:
            continue
        # A private root may be nested in TOOL_ROOT's checkout without having
        # its own .git. Git then reports the same top for both roots, but their
        # source paths differ: the private path has no pinned history while the
        # canonical path does. Deduplicate paths, not repository roots.
        if (top, git_path) in paths:
            continue
        paths.add((top, git_path))
        revs = _git(top, ['log', '--format=%H', f'-n{_GIT_REVISION_LIMIT}', '--all', '--', git_path]) or b''
        specs = [f':{git_path}'] + [f'{r}:{git_path}' for r in revs.decode().split()]
        out = _git(top, ['cat-file', '--batch'], ''.join(s + '\n' for s in specs).encode())
        if out is None:
            continue
        pos, seen = 0, set()
        for spec in specs:
            eol = out.find(b'\n', pos)
            if eol < 0:
                break
            header = out[pos:eol].split()
            pos = eol + 1
            if len(header) != 3 or header[1] != b'blob':
                continue                      # `<spec> missing`, or not a file
            size = int(header[2])
            data, pos = out[pos:pos + size], pos + size + 1
            if header[0] in seen:
                continue
            seen.add(header[0])
            where = 'index' if spec.startswith(':') else spec.split(':', 1)[0][:12]
            found.append((f'git {where}:{git_path}', data))
    return found


def _archive_versions(record_path, record, source_rel):
    """[(label, bytes)] of the pre-migration archive files this record reaches."""
    paths = [f'{_PRE_TU_ARCHIVE}/{source_rel}']
    retired = ((record.get('source') or {}).get('retired_from') or {})
    if str(retired.get('archived_to', '')).endswith('.c'):
        paths.append(retired['archived_to'])
    found, seen = [], set()
    for base in (ROOT, TOOL_ROOT):
        try:
            manifest = json.loads((Path(base) / _PRE_TU_ARCHIVE / 'manifest.json').read_bytes())
        except (OSError, ValueError):
            manifest = {}
        for f in manifest.get('files') or []:
            if isinstance(f, dict) and f.get('pinned_by') == record_path and str(f.get('path', '')).endswith('.c'):
                paths.append(f'{_PRE_TU_ARCHIVE}/{f["path"]}')
        for path in paths:
            try:
                path = safe_relative(path, 'archived record source')
            except AuditError:
                continue
            full = Path(base) / path
            if full.is_file() and str(full.resolve()) not in seen:
                seen.add(str(full.resolve()))
                found.append((f'archive {path}', full.read_bytes()))
    return found


def _evidence_versions(record, source_rel):
    """[(label, bytes)] of the other C files the record itself names."""
    names, stack = set(), [record]
    while stack:
        node = stack.pop()
        if isinstance(node, dict):
            stack.extend(node.values())
        elif isinstance(node, list):
            stack.extend(node)
        elif isinstance(node, str) and node.endswith('.c') and '\n' not in node and node != source_rel:
            names.add(node)
    found = []
    for name in sorted(names):
        try:
            path = safe_relative(name, 'record evidence')
        except AuditError:
            continue
        if (ROOT / path).is_file():
            found.append((f'evidence {path}', (ROOT / path).read_bytes()))
    return found


def recover_pinned_body(record_path, record, name, source_rel, body_pin):
    """(text, witness): the body `body_pin` pins, recovered and verified, or (None, witness).

    Candidates are tried in order -- the repository versions of the record's
    source (a version whose sha256 is the record's own `source.sha256` first),
    then the pre-migration archive, then C files the record names -- and each
    is split with tu_edit.TU and sliced `core_start:core_end` exactly as
    record_body() does. Only a slice that hashes to the pin is returned.
    """
    key = (str(ROOT), record_path, name, source_rel, body_pin)
    if key in _WITNESS_CACHE:
        return _WITNESS_CACHE[key]
    source = record.get('source') or {}
    pins = {source.get('sha256')} | {e.get('sha256') for e in source.get('tu_sources') or []
                                     if isinstance(e, dict)}
    versions = _git_versions(source_rel)
    versions.sort(key=lambda v: hashlib.sha256(v[1]).hexdigest() not in pins)   # stable: pinned first
    versions += _archive_versions(record_path, record, source_rel)
    versions += _evidence_versions(record, source_rel)
    tried = []
    answer = None, dict(recovered=False, tried=tried)
    for label, data in versions:
        tried.append(label)
        try:
            tu = tu_edit.TU(data.decode('utf-8', 'surrogateescape'))
        except (tu_edit.ParseError, ValueError):
            continue
        block = tu.by_name.get(name)
        if block is None or block.state != 'c':
            continue
        text = tu.text[block.core_start:block.core_end]
        if sha256_text(text) == body_pin:
            answer = text, dict(recovered=True, witness=label, file_sha256=hashlib.sha256(data).hexdigest(),
                                file_matches_record_pin=hashlib.sha256(data).hexdigest() in pins)
            break
    _WITNESS_CACHE[key] = answer
    return answer


def recover_pinned_file(record_path, record, source_rel, want):
    """(text, label) of a version of the record's source whose sha256 is `want`, or (None, None)."""
    for label, data in _git_versions(source_rel) + _archive_versions(record_path, record, source_rel):
        if hashlib.sha256(data).hexdigest() == want:
            return data.decode('utf-8', 'surrogateescape'), label
    return None, None


def verified_record_storage_views(expected, body, tu, record, context):
    """Prove a byte-address view of a recovered, record-pinned data object."""
    if not context or not context.get('data_row'):
        return []
    args, row = context['args'], context['data_row']
    tokens = tu_edit.c_tokens(tu.text)
    directives, directive = [], None
    for token in tokens:
        if token == tu_edit.DIRECTIVE_END:
            if directive is not None:
                directives.append(directive)
            directive = None
        elif directive is not None:
            directive.append(token)
        elif token == '#':
            directive = []
    macros, depth = {}, 0
    for directive in directives:
        if not directive:
            continue
        if directive[0] in ('if', 'ifdef', 'ifndef'):
            depth += 1
        elif directive[0] == 'endif':
            depth -= 1
        elif directive[0] in ('define', 'undef') and len(directive) >= 2:
            name = directive[1]
            macros.setdefault(name, []).append((depth, directive))
    expected_tokens, body_tokens = tu_edit.c_tokens(expected), tu_edit.c_tokens(body)
    views = []
    for macro, definitions in macros.items():
        if len(definitions) != 1 or definitions[0][0] != 0 or macro not in body_tokens:
            continue
        definition = definitions[0][1]
        # No function-like macro, offset, arithmetic, or evaluation is admitted.
        for owner, reference in (record.get('references') or {}).items():
            if owner not in expected_tokens or definition != ['define', macro] + tu_edit.c_tokens(
                    '((unsigned char *)(void *)' + owner + ')'):
                continue
            if owner in macros or not isinstance(reference, dict) or reference.get('unit') != args.unit:
                continue
            # The cast is an address view only for an authored array.  A
            # pointer or scalar cast would introduce a different evaluation.
            declarations = []
            for item in tu.items:
                if item.kind != 'decl':
                    continue
                declaration = tu_edit.c_tokens(tu.text[item.start:item.end])
                if owner not in declaration:
                    continue
                index = declaration.index(owner)
                tail = declaration[index + 1:]
                if (declaration.count(owner) == 1 and 'extern' not in declaration[:index] and
                        '*' not in declaration[:index] and len(tail) >= 5 and
                        tail[0] == '[' and re.fullmatch(r'[1-9][0-9]*', tail[1]) and
                        tail[2:4] == [']', '=']):
                    declarations.append(declaration)
            if len(declarations) != 1:
                continue
            views.append((macro, owner, reference))
    if not views or len(views) > 16 or len({owner for _, owner, _ in views}) != len(views):
        return []
    substitutions = {owner: macro for macro, owner, _ in views}
    if [substitutions.get(token, token) for token in expected_tokens] != body_tokens:
        return []
    from tools.tu import data_carve as dc
    raw_path, source, assembly = Path(args.obj), Path(args.tu_c), Path(args.build_s)
    selected_path = Path(row.get('_audit_linked_data_object', '/nonexistent'))
    linked_path = Path(args.unit_dir) / 'build' / (args.unit + '.rom.elf')
    candidate_pin = row.get('_audit_candidate_record') or {}
    candidate_path = Path(candidate_pin.get('path', '/nonexistent'))
    current_hashes = dict(source_sha256=sha256_file(source), assembly_sha256=sha256_file(assembly),
                          object_sha256=sha256_file(raw_path),
                          linked_data_object_sha256=sha256_file(selected_path),
                          linked_elf_sha256=sha256_file(linked_path))
    if (any(row.get(key) != value for key, value in current_hashes.items()) or
            not candidate_path.is_file() or sha256_file(candidate_path) != candidate_pin.get('sha256') or
            source.read_text() != tu.text):
        return []
    candidate = json.loads(candidate_path.read_text())
    raw, selected = RelObject(raw_path.read_bytes()), RelObject(selected_path.read_bytes())
    ctx = dc.tu_context(ROOT, args.unit, Path(args.unit_dir), args.tu_id)
    original, linked = dc.Elf(Path(ctx['orig']).read_bytes()), dc.Elf(linked_path.read_bytes())
    authored_asm = asm_statement_lines(tu.text)
    asm_text = assembly.read_text()
    outside_app, app = [], False
    for line in asm_text.splitlines():
        if line.strip() == '#APP':
            app = True
        elif line.strip() == '#NO_APP':
            app = False
        elif not app:
            outside_app.append(line)
    outside_app = '\n'.join(outside_app)
    proofs = []
    for macro, owner, reference in views:
        address, size = int(reference['va'], 16), reference['size']
        spans = [span for span in row.get('verified_location_spans', [])
                 if span.get('address', '').lower() == reference['va'].lower() and
                 span.get('names') == [owner] and span.get('object_symbols') == [owner] and
                 span.get('length') == size and span.get('range') == [address, address + size] and
                 span.get('object_sha256') == current_hashes['linked_data_object_sha256']]
        raw_symbols = [symbol for symbol in raw.symbols if symbol['name'] == owner]
        selected_symbols = [symbol for symbol in selected.symbols if symbol['name'] == owner]
        if (type(size) is not int or size <= 0 or len(spans) != 1 or len(raw_symbols) != 1 or
                len(selected_symbols) != 1 or reference.get('symbols') != [owner] or
                any(re.search(r'\b' + re.escape(owner) + r'\b', line) for line in authored_asm)):
            return []
        span, raw_symbol, selected_symbol = spans[0], raw_symbols[0], selected_symbols[0]
        section = span['section']
        original_symbols = [symbol for symbol in original.symbols if symbol.name == owner and
                            symbol.value == address and symbol.size == size and symbol.type == STT_OBJECT and
                            symbol.bind == raw_symbol['binding'] and 0 < symbol.shndx < len(original.sections) and
                            original.sections[symbol.shndx].name == section]
        linked_symbols = [symbol for symbol in linked.symbols if symbol.name == owner and
                          symbol.value == address and symbol.size == size and symbol.type == STT_OBJECT and
                          symbol.bind == raw_symbol['binding'] and 0 < symbol.shndx < len(linked.sections) and
                          linked.sections[symbol.shndx].name == section]
        locations = [location for location in candidate.get('recovered_location_candidates', [])
                     if location.get('unit') == args.unit and location.get('section') == section and
                     location.get('address', '').lower() == reference['va'].lower() and location.get('names') == [owner]]
        raw_section, selected_section = raw.section(raw_symbol['section']), selected.section(selected_symbol['section'])
        input_span = span.get('compiler_input_span') or {}
        if (len(original_symbols) != 1 or len(linked_symbols) != 1 or len(locations) != 1 or
                raw_symbol['type'] != STT_OBJECT or raw_symbol['size'] != size or
                selected_symbol['type'] != STT_OBJECT or selected_symbol['size'] != size or
                selected_symbol['binding'] != raw_symbol['binding'] or
                raw_symbol['section'] != input_span.get('section') or raw_symbol['value'] != input_span.get('offset') or
                raw_section['size'] != input_span.get('section_size') or raw_section['type'] != 1 or
                selected_symbol['section'] != span['c_object_section'] or selected_symbol['value'] != span['c_object_offset'] or
                selected_section['type'] != 1 or selected_section['size'] != span['c_object_section_size'] or
                not re.search(r'^\s*' + re.escape(owner) + r':\s*$', outside_app, re.MULTILINE) or
                not re.search(r'^\s*\.size\s+' + re.escape(owner) + r'\s*,\s*' + str(size) + r'\s*$', outside_app, re.MULTILINE)):
            return []
        raw_bytes = raw.bytes_of(raw_section)[raw_symbol['value']:raw_symbol['value'] + size]
        selected_bytes = selected.bytes_of(selected_section)[selected_symbol['value']:selected_symbol['value'] + size]
        original_bytes, linked_bytes = dc.va_bytes(original, address, size), dc.va_bytes(linked, address, size)
        if (len(raw_bytes) != size or raw_bytes != selected_bytes or raw_bytes != original_bytes or
                raw_bytes != linked_bytes or hashlib.sha256(raw_bytes).hexdigest() != reference.get('sha256') or
                any(sec == raw_symbol['section'] and raw_symbol['value'] <= off < raw_symbol['value'] + size
                    for sec, off, _kind, _name in raw.relocations())):
            return []
        proofs.append(dict(macro=macro, owner=owner, address=reference['va'], size=size,
                           record_reference_sha256=reference['sha256'], candidate=candidate_pin,
                           kind='exact_recovered_object_byte_address_view', **current_hashes))
    return proofs


def record_fidelity(block, tu, unit, tu_id, tu_rel, original_va, entries, tu_repo=None,
                    storage_view_context=None):
    """(ok, detail) for one claimed C function against its accepted record.

    Since the pre-migration layout was retired (task tu-retire-legacy-src), a
    record's `source` is usually the published TU file itself. Comparing that file
    with itself proves nothing, so this check says so - `record_is_tu_source` - and
    falls back to the archived pre-migration body when the local archive still has
    it, which is the only place a real comparison is still available.

    What the check establishes is about ONE FUNCTION: that the TU's definition is
    still the body the record was accepted for. The record's whole-file
    `source.sha256` is not that; it is a property of the published baseline, it
    changes whenever any other function of the TU is edited, and in a candidate it
    can never hold. It is reported here (`source_pin`) and never decides.
    `accepted_c_functions` in `audit` is the other half: it runs this comparison
    for the accepted C functions of the TU that were NOT claimed, so an accepted
    neighbour cannot be dropped or reverted to assembly without the audit noticing.
    """
    body = tu.text[block.core_start:block.core_end]
    index = c_record_index()
    hits = list(index.get((unit, str(original_va).lower()), [])) if original_va else []
    if not hits and original_va is None:
        hits = [h for key, v in index.items() if key[0] == unit for h in v if h[2].get('name') == block.key]
    if not hits:
        return True, dict(status='no_accepted_record',
                          detail='no accepted config/units record resolves to this function; nothing to '
                                 'compare (a TU-mode recovery has no legacy record)')
    if len(hits) != 1:
        return False, dict(status='ambiguous_record', records=sorted(h[0] for h in hits),
                           detail='several accepted records claim this address')
    record_path, record, entry = hits[0]
    source_rel = relative_source(record, tu_id)
    self_reference = tu_repo is not None and source_rel == tu_repo
    origin = retired_origin(record) if self_reference else None
    if self_reference:
        # The record points at the file under audit, so `record_body` would read
        # the accepted body out of the very file it is judging. Two different
        # questions have to be kept apart here, and only one of them is this
        # check's:
        #
        #  * "is the TU's definition still the accepted function?" -- what the
        #    check is for. The witness is the archived pre-migration body, which
        #    the record names and pins by its own sha256 (`retired_from`). It is
        #    independent of the TU file, so it answers the question in a published
        #    tree and in a candidate tree alike, and it answers it per function.
        #  * "does the record still describe the file it points at?" -- the
        #    whole-file `source.sha256` pin. That is integrator bookkeeping about a
        #    published tree, and in a candidate it is unsatisfiable BY
        #    CONSTRUCTION: a candidate differs from the published baseline, which
        #    is what makes it a candidate. Deciding this check on that pin made an
        #    untouched neighbour refuse a byte-exact recovery of a different
        #    function of the same TU (ov01/tu028: `adjAngle`'s record pins
        #    src/ov01/m_ef_create_msp_02.c, so every re-treatment of
        #    `fnMSP02_PR000` failed on `adjAngle`), and it returned before the
        #    archived comparison below, which is the comparison that means
        #    something -- leaving that branch unreachable for every candidate.
        #
        # So the pin is measured and reported on every result (`source_pin`), the
        # archived body decides, and the TU-level `accepted_records` report lists
        # every record whose pin no longer matches the file it points at, which is
        # where an integrator reads the bookkeeping signal for a published tree.
        digest = sha256_file(ROOT / source_rel)
        want = relative_source_sha(record, tu_id)
        pin = dict(source=source_rel, source_pin='matches' if digest == want else 'differs',
                   source_sha256=digest, record_source_sha256=want)
        # A per-function body pin, if the record carries one, is a witness of the
        # accepted body that costs no second file and does not move when another
        # function of the TU is edited. A record born in TU mode has no archived
        # pre-migration source (its own note says so), so this is the witness it
        # can be given; the audit consumes it wherever it is present so adding one
        # needs no further tool change.
        body_pin = entry.get('body_sha256') if isinstance(entry, dict) else None
        if body_pin:
            pin['record_body_sha256'] = body_pin
            if sha256_text(body) == body_pin:
                return True, dict(status='verbatim_vs_record_body_pin', record=record_path,
                                  detail='the record points at this TU file; the body was compared with '
                                         'the per-function body pin the record carries and is that body '
                                         'verbatim', **pin)
            # The hash cannot tell a comment edit from a code edit: recover the
            # pinned text and compare code (see recover_pinned_body).
            declared = adaptations_for(entries, tu_id, tu_rel, block.key)
            pinned, witness = recover_pinned_body(record_path, record, block.key, source_rel, body_pin)
            if not declared and pinned is not None and tu_edit.same_code(pinned, body):
                return True, dict(status='comment_only_drift_vs_record_body_pin', record=record_path,
                                  body_sha256=sha256_text(body), pinned_body=witness,
                                  detail='the TU body does not hash to the per-function body pin of its '
                                         'record, but the pinned body (recovered from '
                                         f'{witness["witness"]} and verified against the pin) is the same '
                                         'C token stream once comments are treated as whitespace: the '
                                         'difference is comments only, which never counts as drift', **pin)
            drift = {}
            if pinned is not None:
                drift['diff'] = [line for line in difflib.unified_diff(
                    pinned.splitlines(), body.splitlines(), 'record body pin', 'TU', lineterm='', n=1)][:12]
            if not declared:
                return False, dict(status='undeclared_drift_vs_record_body_pin', record=record_path,
                                   body_sha256=sha256_text(body), pinned_body=witness, **drift,
                                   detail='the TU body does not hash to the per-function body pin of its '
                                          'accepted record and no adaptation in the TU\'s adaptations '
                                          'file names this function ('
                                          + (f'the pinned body, recovered from {witness["witness"]}, differs '
                                             'in code, not only in comments' if pinned is not None else
                                             'no repository version, archive or record evidence in reach '
                                             'reproduces the pin, so a comment-only difference cannot be '
                                             'told apart from a code change') + ')', **pin)
            # A hash cannot be rewritten by an `old`->`new` declaration, so the
            # difference is reported as declared and not as verified, exactly as a
            # prose-only declaration is against a body.
            return True, dict(status='declared_adaptation_vs_record_body_pin', record=record_path,
                              body_sha256=sha256_text(body), pinned_body=witness, **drift,
                              adaptations=[dict(source=x['source'], tu=x['tu'], kind=x['kind'],
                                                reason=x['reason']) for x in declared],
                              detail='the TU body does not hash to the per-function body pin of its '
                                     'record; the difference is covered by a declared per-TU adaptation, '
                                     'which a hash cannot verify', **pin)
        if origin is None or not origin['present']:
            # No independent witness of the accepted body exists in this tree: a
            # record accepted directly in TU mode never had a pre-migration source,
            # and the archive is local and gitignored. Say exactly what was and was
            # not established rather than passing quietly or refusing something no
            # tree can satisfy. What still holds this function down is the TU-level
            # `accepted_records` check (it must still be a C definition of this TU),
            # the original-symbol check (b) and the whole-file rebuild.
            archive = ((f"the pre-migration body this record was accepted from is in the archive "
                        f"({origin['path']}, sha256 {origin['sha256'][:12]}...), which is not in this "
                        f"tree; restore it to compare bodies again")
                       if origin else
                       'this record was accepted against the TU file itself (no pre-migration source '
                       'is recorded), so no tree holds a second copy of the accepted body; giving the '
                       'record entry a `body_sha256` (or archiving the accepted file under '
                       '`source.retired_from.archived_to`) restores the comparison, and this check '
                       'already consumes both')
            if digest == want:
                return True, dict(
                    status='record_is_tu_source', record=record_path, archived=(origin or {}).get('path'),
                    detail='the accepted record points at this TU file, so the body comparison is '
                           'trivially satisfied and establishes nothing beyond the sha256 pin, which was '
                           'checked. ' + archive + '.', **pin)
            return True, dict(
                status='record_is_tu_source_unwitnessed', record=record_path,
                archived=(origin or {}).get('path'),
                detail='the accepted record points at this TU file and the file differs from the '
                       'record\'s whole-file pin, which is what a candidate is; that pin is a property '
                       'of the published baseline, not of this function, so it is reported rather than '
                       'enforced here. No independent copy of the accepted body is available: '
                       + archive + '.', **pin)
        expected, error = record_body(record, block.key, origin['path'], origin['sha256'])
        if expected is None:
            return False, dict(status='unresolved_archived_record', record=record_path,
                               archived=origin['path'], detail=error, **pin)
        if expected == body:
            return True, dict(status='verbatim_vs_archived_record', record=record_path,
                              archived=origin['path'],
                              detail='the record points at this TU file; the body was compared with the '
                                     'archived pre-migration source instead, and is that body verbatim',
                              **pin)
        declared = adaptations_for(entries, tu_id, tu_rel, block.key)
        if not declared and tu_edit.same_code(expected, body):
            return True, dict(status='comment_only_drift_vs_archived_record', record=record_path,
                              archived=origin['path'],
                              detail='the record points at this TU file; the body was compared with the '
                                     'archived pre-migration source instead and differs from it only in '
                                     'comments (the same C token stream), which never counts as drift',
                              **pin)
        diff = [line for line in difflib.unified_diff(expected.splitlines(), body.splitlines(),
                                                      'archived record', 'TU', lineterm='', n=1)][:12]
        if not declared:
            return False, dict(status='undeclared_drift_vs_archived_record', record=record_path,
                               archived=origin['path'], diff=diff,
                               detail='the TU body differs from the archived pre-migration source of its '
                                      'record and no adaptation in the TU\'s adaptations file names this '
                                      'function', **pin)
        texts = [e for e in declared if e.get('old') and e.get('new')]
        rebuilt = expected
        for e in texts:
            if e['old'] not in rebuilt:
                return False, dict(status='adaptation_text_not_found', record=record_path,
                                   archived=origin['path'], diff=diff,
                                   adaptations=[dict(source=x['source'], reason=x['reason']) for x in declared],
                                   detail=f'the declared replacement {e["old"][:60]!r} is not in the '
                                          'archived record body', **pin)
            rebuilt = rebuilt.replace(e['old'], e['new'])
        storage_views = (verified_record_storage_views(rebuilt, body, tu, record, storage_view_context)
                         if not tu_edit.same_code(rebuilt, body) else [])
        if texts and not tu_edit.same_code(rebuilt, body) and not storage_views:
            return False, dict(status='adaptation_does_not_explain', record=record_path,
                               archived=origin['path'], diff=diff,
                               adaptations=[dict(source=x['source'], reason=x['reason']) for x in declared],
                               detail='applying the declared replacements to the archived record body does '
                                      'not produce the TU body', **pin)
        return True, dict(status=('declared_adaptation_and_storage_view_verified_vs_archived_record' if storage_views
                                  else 'declared_adaptation_verified_vs_archived_record' if texts
                                  else 'declared_adaptation_vs_archived_record'),
                          record=record_path, archived=origin['path'],
                          storage_views=storage_views,
                          adaptations=[dict(source=x['source'], reason=x['reason']) for x in declared],
                          detail='the record points at this TU file; the difference against its archived '
                                 'pre-migration source is covered by declared per-TU adaptations' +
                                 (' and independently verified byte-address storage views' if storage_views else ''),
                          **pin)
    want = relative_source_sha(record, tu_id)
    expected, error = record_body(record, block.key, source_rel, want)
    recovered = None
    if expected is None and (ROOT / source_rel).is_file():
        # The record's source no longer hashes to its pin. When the pinned
        # version is still in reach and the file has changed only in comments
        # since, the record still describes it: read the body from that version.
        pinned_text, label = recover_pinned_file(record_path, record, source_rel, want)
        if pinned_text is not None and tu_edit.same_code(
                pinned_text, (ROOT / source_rel).read_text(encoding='utf-8', errors='surrogateescape')):
            try:
                pinned_tu = tu_edit.TU(pinned_text)
                pinned_block = pinned_tu.by_name.get(block.key)
            except tu_edit.ParseError:
                pinned_block = None
            if pinned_block is not None and pinned_block.state == 'c':
                expected = pinned_tu.text[pinned_block.core_start:pinned_block.core_end]
                recovered = dict(witness=label, detail='the record source differs from its pin only in comments; '
                                                      'the body was read from the pinned version')
    if expected is None:
        return False, dict(status='unresolved_record', record=record_path, detail=error)
    extra = dict(record_source_comment_drift=recovered) if recovered else {}
    if expected == body:
        return True, dict(status='verbatim', record=record_path, **extra)
    declared = adaptations_for(entries, tu_id, tu_rel, block.key)
    if not declared and tu_edit.same_code(expected, body):
        return True, dict(status='comment_only_drift', record=record_path, **extra,
                          detail='the TU body differs from its accepted record only in comments (the same C '
                                 'token stream), which never counts as drift')
    diff = [line for line in difflib.unified_diff(expected.splitlines(), body.splitlines(),
                                                  'record', 'TU', lineterm='', n=1)][:12]
    if not declared:
        return False, dict(status='undeclared_drift', record=record_path, diff=diff,
                           detail='the TU body differs from its accepted record and no adaptation in the TU\'s '
                                  'adaptations file names this function')
    texts = [e for e in declared if e.get('old') and e.get('new')]
    if texts:
        rebuilt = expected
        for e in texts:
            if e['old'] not in rebuilt:
                return False, dict(status='adaptation_text_not_found', record=record_path, diff=diff,
                                   adaptations=[dict(source=e['source'], reason=e['reason']) for e in declared],
                                   detail=f'the declared replacement {e["old"][:60]!r} is not in the record body')
            rebuilt = rebuilt.replace(e['old'], e['new'])
        if not tu_edit.same_code(rebuilt, body):
            return False, dict(status='adaptation_does_not_explain', record=record_path, diff=diff,
                               adaptations=[dict(source=e['source'], reason=e['reason']) for e in declared],
                               detail='applying the declared replacements to the record body does not produce '
                                      'the TU body')
        return True, dict(status='declared_adaptation_verified', record=record_path,
                          adaptations=[dict(source=e['source'], reason=e['reason']) for e in texts])
    return True, dict(status='declared_adaptation', record=record_path, diff=diff,
                      adaptations=[dict(source=e['source'], tu=e['tu'], kind=e['kind'], reason=e['reason'])
                                   for e in declared],
                      detail='the difference is covered by a declared per-TU adaptation (prose declaration: the '
                             'entry names this function but carries no exact old/new text)')


def audit(args):
    checks = Checks()
    # Before anything is read: the tree this audit is about (see resolve_root).
    root = resolve_root(args)
    unit_dir = Path(args.unit_dir).resolve()
    tu_c = Path(args.tu_c).resolve()
    tu_rel = Path(os.path.relpath(tu_c, unit_dir)).as_posix()
    result = dict(schema=SCHEMA, tool=dict(path='tools/tu_audit.py', sha256=sha256_file(__file__),
                                           root=str(TOOL_ROOT)),
                  unit=args.unit, tu_id=args.tu_id, root=root, unit_dir=rel(unit_dir), tu_source=tu_rel,
                  ok=False)
    checks.add('root_resolution', root['tu_source_inside_root'], **root)
    if not tu_c.is_file():
        raise AuditError(f'TU source missing: {tu_c}')

    # -- manifest, range, function list ------------------------------------------
    start = end = names = tu_record = None
    if args.tu_id:
        if not args.tu_id.startswith(args.unit + '/'):
            raise AuditError(f'TU id {args.tu_id!r} does not belong to unit {args.unit!r}')
        start, end, names, source, tu_record = lookup_tu(args.tu_id, args.unit)
        result['range_source'] = source
    if args.range:
        a, b = args.range.split(':')
        start, end = int(a, 16), int(b, 16)
        result['range_source'] = 'argument'
    if args.functions:
        names = [n for part in args.functions for n in part.split(',') if n]
    elif args.skeleton:
        names = tu_edit.skeleton_functions(tu_edit.load(args.skeleton))
    if start is not None:
        result['range'] = [f'0x{start:08x}', f'0x{end:08x}']
    checks.add('range_known', start is not None,
               detail=None if start is not None else 'no --tu-id/--range: offsets and the symbol set of the TU '
                                                     'cannot be verified')

    # The TU source is split before anything is compiled: a structurally
    # refused TU is reported as such rather than through a later toolchain error.
    tu_text = tu_c.read_text(encoding='utf-8', errors='surrogateescape')
    tu = tu_edit.TU(tu_text, str(tu_c), names, unit_dir)

    # -- contract ------------------------------------------------------------------
    contract, contract_issues, contract_relaxed = resolve_contract(args, tu_record)
    result['contract'] = json.loads(json.dumps(contract).replace(str(ROOT) + '/', ''))
    # KOS-MOS 2026-09-27: `contract_relaxed` is [] when every pin held (see resolve_contract).
    result['contract_relaxed'] = contract_relaxed
    checks.add('contract', not contract_issues, issues=contract_issues, relaxed=contract_relaxed,
               detail=None if not contract_issues else 'the compile contract is not the TU contract of '
                                                       'config/tu-build.json / config/toolchains-tu.json')
    # The classes the published TU was accepted under (see
    # published_source_classes): the audit reads them from the repository, so a
    # whole-TU audit needs no --source-class/--function-class/--asm-contract to
    # reproduce what a worker declared at submission time.
    pub_tu_class, pub_classes, pub_contracts, pub_prov = published_source_classes(args.tu_id)
    if pub_prov is not None:
        result['source_classes'] = pub_prov
    manifest_class = SOURCE_CLASS.get(((tu_record or {}).get('contract') or {})
                                      .get('source_class'), 'ordinary')
    if args.source_class and pub_tu_class and args.source_class != pub_tu_class:
        raise AuditError(f'--source-class {args.source_class} contradicts {SOURCE_CLASSES_FILE}, '
                         f'which records {pub_tu_class} for {args.tu_id}')
    source_class = args.source_class or pub_tu_class or manifest_class
    result['source_class'] = dict(
        value=source_class,
        basis=('argument' if args.source_class else
               (SOURCE_CLASSES_FILE if pub_tu_class else 'config/tu-build.json')))

    # -- the audit compiles the TU itself -------------------------------------------
    # The assembly step is the build's, read from the unit's build.ninja edge for
    # --obj (build_assembly_recipe). `--asm-cwd`/`--asm-include` predate that and
    # may now only restate it, exactly as `--contract`/`--flags` may only restate
    # the compile contract: a caller naming a different staged directory is a
    # failure, not an override.
    asm_cwd = Path(args.asm_cwd).resolve() if args.asm_cwd else None
    asm_includes = [Path(p).resolve() for p in (args.asm_include or [])]
    recipe = build_assembly_recipe(unit_dir, args.obj)
    if recipe is not None:
        staged = recipe['build_overlay'] or recipe['cwd']
        if asm_cwd is not None and asm_cwd != staged:
            raise AuditError(f'--asm-cwd {rel(asm_cwd)} is not the directory the build assembles '
                             f'{rel(Path(args.obj).resolve())} from ({rel(staged)}, rule '
                             f'{recipe["rule"]} of {rel(unit_dir / "build.ninja")})')
        allowed = [Path(i).resolve() for i in recipe['includes']]
        unknown = [rel(p) for p in asm_includes if p not in allowed]
        if unknown:
            raise AuditError(f'--asm-include {unknown} is not in the include list the build '
                             f'assembles with ({[rel(p) for p in allowed]})')
    tmp = None
    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
        if out_dir.exists() and any(out_dir.iterdir()):
            raise AuditError('--out-dir must be empty or absent')
        out_dir.mkdir(parents=True, exist_ok=True)
    else:
        tmp = tempfile.TemporaryDirectory(prefix='tu_audit_')
        out_dir = Path(tmp.name)
    try:
        (tu_i, tu_s, tu_o), compile_log, messages, asm_cwd, asm_includes = compile_tu(
            contract, tu_rel, unit_dir, out_dir, asm_cwd, asm_includes, recipe)
        result['compile'] = compile_log
        # What the assembly step was, and where it came from. The audit's own
        # overlay (`cwd`) is built by CINC_TOOL from the audit's own cc1 output;
        # `build_overlay` is the directory the build edge used, recorded so the
        # two are visibly the same recipe.
        result['assembly'] = dict(
            rule=(recipe or {}).get('rule'), derived_from=rel(unit_dir / 'build.ninja'),
            cinc=bool((recipe or {}).get('cinc')),
            cinc_tool=(dict(path=CINC_TOOL, sha256=sha256_file(TOOL_ROOT / CINC_TOOL))
                       if (recipe or {}).get('cinc') else None),
            build_overlay=(rel((recipe or {})['build_overlay'])
                           if (recipe or {}).get('build_overlay') else None),
            cwd=rel(asm_cwd) if asm_cwd else rel(unit_dir),
            includes=[rel(Path(i)) for i in asm_includes])
        if (recipe or {}).get('common_allocation'):
            result['assembly']['common_allocation'] = recipe['common_allocation']
        checks.add('build_assembly_recipe', recipe is not None or not args.obj,
                   **result['assembly'],
                   detail=("the audit assembles this TU the way the build edge that produced "
                           "--obj does" if recipe is not None else
                           'no build.ninja edge produces --obj, so the assembly step could not be '
                           'reproduced from the build; the object comparison stays the binding and '
                           'a recipe difference would fail it'))
        result['toolchain_messages'] = messages
        result['inputs'] = {k: dict(path=rel(p), sha256=sha256_file(p)) for k, p in
                            (('tu_c', tu_c), ('audit_tu_i', tu_i), ('audit_tu_s', tu_s), ('audit_tu_o', tu_o))}
        return _audit_body(args, result, checks, unit_dir, tu_c, tu_rel, contract, source_class, tu_record,
                           start, end, names, tu_i, tu_s, tu_o, asm_cwd, asm_includes, tu, tu_text,
                           pub_classes, pub_contracts,
                           (pub_prov or {}).get('nested_functions') or {},
                           (pub_prov or {}).get('asm_contracts_missing') or {})
    finally:
        if tmp is not None:
            tmp.cleanup()


def _audit_body(args, result, checks, unit_dir, tu_c, tu_rel, contract, source_class, tu_record,
                start, end, names, tu_i, tu_s, tu_o, asm_cwd, asm_includes, tu, tu_text,
                pub_classes=None, pub_contracts=None, pub_nested=None, pub_missing=None):
    pub_missing = pub_missing or {}
    pub_classes = pub_classes or {}
    pub_contracts = pub_contracts or {}
    pub_nested = pub_nested or {}
    # -- binding to the build outputs ------------------------------------------------
    if args.obj:
        build_obj = Path(args.obj).resolve()
        diffs = compare_objects(tu_o, build_obj) if build_obj.is_file() else ['build object missing']
        checks.add('build_object_matches_audit', not diffs, build_object=rel(build_obj), differences=diffs[:8],
                   sha256=sha256_file(build_obj) if build_obj.is_file() else None,
                   detail='allocated sections, relocations and symbols of the build object equal the audit\'s own '
                          'compile (debug/path-only sections excluded)' if not diffs else
                          'the build object is not what this source compiles to')
    else:
        checks.add('build_object_matches_audit', False, waived=True,
                   detail='no --obj: the audited source is not bound to the object the whole-file gate linked')
    if args.build_s:
        build_s = Path(args.build_s).resolve()
        diffs = (compare_assembly(tu_s.read_text(errors='replace'), build_s.read_text(errors='replace'))
                 if build_s.is_file() else ['build assembly missing'])
        checks.add('build_assembly_matches_audit', not diffs, build_assembly=rel(build_s), differences=diffs[:4])

    # Every toolchain message is an error (config/toolchains-tu.json
    # include_asm.rules). The single exemption is cc1's note that a function
    # this TU declares static is not defined in C: its definition is the
    # INCLUDE_ASM/ACCEPTED_ASM body of the same TU. Every such line is listed.
    scaffold_block_names = {b.name for b in tu.blocks if b.state != 'c'} | {b.key for b in tu.blocks
                                                                            if b.state != 'c'}
    kept, exempt = [], []
    for message in result.get('toolchain_messages') or []:
        for line in message['output'].splitlines():
            if not line.strip():
                continue
            m = re.search(r"warning: `([A-Za-z_][\w.$]*)' (?:used but never defined|declared .?static.? but "
                          r"never defined)", line)
            if m and m.group(1) in scaffold_block_names:
                exempt.append(line.strip())
            elif re.fullmatch(r'\S+: (?:In function .*|At top level:)', line.strip()):
                continue
            else:
                kept.append(dict(step=message['step'], line=line.strip()))
    checks.add('toolchain_messages', not kept, messages=kept[:20], exempt=exempt[:20],
               detail=None if not kept else 'config/toolchains-tu.json include_asm.rules: any assembler or '
                                            'compiler message from a TU is an error (the only exemption is a '
                                            'static function whose definition is this TU\'s scaffold body)')
    elf, orig_info = original_elf(args.unit)
    result['inputs']['original'] = orig_info
    obj = RelObject(tu_o.read_bytes())
    asm = AsmFile(tu_s.read_text(encoding='utf-8', errors='surrogateescape'))
    pre = Preprocessed(tu_i.read_text(encoding='utf-8', errors='replace'))
    # Per-function classes: the repository's declaration first, the command line
    # for functions it does not classify. Where both name a function they must
    # agree - the command line may restate a published class, never change it.
    cli_classes = function_classes(args)
    disagree = [f'{n}: {SOURCE_CLASSES_FILE} records {pub_classes[n]}, --function-class says {c}'
                for n, c in sorted(cli_classes.items()) if n in pub_classes and pub_classes[n] != c]
    if disagree:
        raise AuditError('; '.join(disagree))
    classes = dict(pub_classes)
    classes.update(cli_classes)
    gnu_scope_errors = []
    if source_class == 'gnu-nested-function':
        gnu_scope_errors.append(f'{args.tu_id}: the GNU nested-function class must be attached to '
                                 'an enclosing function, not the whole TU')
    for name, cls in sorted(classes.items()):
        if cls != 'gnu-nested-function':
            continue
        if name not in pub_nested:
            gnu_scope_errors.append(f'{args.tu_id} {name}: the evidenced nested_functions mapping is missing')
            continue
        parents = [b for b in tu.blocks if name in (b.name, b.key) and b.state == 'c']
        if len(parents) == 1:
            actual = sorted((d['name'], d['parent']) for d in tu_edit.gnu_nested_definitions(tu.block_text(parents[0]))
                            if d['depth'] > 0)
            expected = sorted((item['source_name'], item['parent']) for item in pub_nested[name])
            if actual != expected:
                gnu_scope_errors.append(f'{args.tu_id} {name}: lexical nested definitions differ from the registered mapping')
    checks.add('gnu_nested_scope', not gnu_scope_errors, errors=gnu_scope_errors,
               detail=None if not gnu_scope_errors else
                'nested C requires original identities, lexical ownership and complete paired claims')
    # The asm contract is optional review documentation that never blocks (user
    # direction 2026-09-27): whatever is wrong with one is a note in the report.
    contract_notes = list((result.get('source_classes') or {}).get('asm_contract_notes') or [])
    asm_contract = (load_asm_contract(args.asm_contract, contract_notes, '--asm-contract')
                    if args.asm_contract else None)
    if asm_contract is not None:
        result['asm_contract'] = dict(path=rel(args.asm_contract),
                                      sha256=sha256_file(args.asm_contract),
                                      top_level_asm=asm_contract.get('top_level_asm') or [])
    # One parsed contract per function that has its own, plus the TU-wide one
    # (the declaration's, else --asm-contract) for every other function.
    contracts = {n: load_asm_contract(ROOT / rp, contract_notes, rp)
                 for n, rp in pub_contracts.items() if n is not None}
    contracts = {n: c for n, c in contracts.items() if c is not None}
    default_contract = asm_contract
    if pub_contracts.get(None) is not None:
        default_contract = (load_asm_contract(ROOT / pub_contracts[None], contract_notes, pub_contracts[None])
                            or asm_contract)
    missing_docs = (result.get('source_classes') or {}).get('asm_contracts_missing') or {}
    checks.add('asm_contract_documentation', True, informational=True,
               notes=contract_notes[:50], missing=missing_docs,
               detail=None if not (contract_notes or missing_docs) else
               'asm contracts are optional review documentation (user direction 2026-09-27): '
               'these notes never fail the audit')
    checks.add('assembly_parse', not asm.errors, errors=asm.errors)

    # -- claims ------------------------------------------------------------------------
    requested = []
    if args.claim:
        requested += [n for part in args.claim for n in part.split(',') if n]
    if args.ledger:
        requested += ledger_claims(args.ledger)
    listed_empty = {e.get('name') if isinstance(e, dict) else e
                    for e in ((tu_record or {}).get('scaffold') or {}).get('empty_functions') or []}
    for b in tu.blocks:
        if b.state == 'c' and (b.key in listed_empty or b.name in listed_empty):
            b.empty = True

    def auditable(b):
        return b.role == 'function' and b.state in ('c', 'accepted_asm')

    nested_by_original = {
        item['original_name']: dict(item, owner=parent)
        for parent, entries in pub_nested.items() for item in entries
    }
    nested_by_parent = {parent: entries for parent, entries in pub_nested.items()}
    claimed_blocks, unresolved, requested_nested = [], [], set()
    if requested:
        for n in requested:
            hit = [b for b in tu.blocks if auditable(b) and n in (b.name, b.key)]
            if len(hit) == 1:
                if hit[0] not in claimed_blocks:
                    claimed_blocks.append(hit[0])
            elif n in nested_by_original:
                parent_name = nested_by_original[n]['owner']
                hit = [b for b in tu.blocks if auditable(b) and parent_name in (b.name, b.key)]
                if len(hit) == 1:
                    requested_nested.add(n)
                    if hit[0] not in claimed_blocks:
                        claimed_blocks.append(hit[0])
                else:
                    unresolved.append(n)
            else:
                unresolved.append(n)
    else:
        claimed_blocks = [b for b in tu.blocks if auditable(b) and not (b.state == 'c' and b.empty)]
    pair_errors = []
    for block in claimed_blocks:
        entries = nested_by_parent.get(block.key) or nested_by_parent.get(block.name) or []
        if not entries or classes.get(block.key, source_class) != 'gnu-nested-function':
            continue
        if requested and (block.key not in requested and block.name not in requested):
            pair_errors.append(f'{block.key}: parent and nested function must be claimed together')
        for item in entries:
            if requested and item['original_name'] not in requested_nested and item['original_name'] not in requested:
                pair_errors.append(f'{block.key}: missing paired nested claim {item["original_name"]}')
    checks.add('gnu_nested_claim_pair', not pair_errors, errors=pair_errors,
               detail=None if not pair_errors else
               'the authorized GNU nested function and its parent are one recovery claim')
    checks.add('claims_resolved', not unresolved, requested=requested, unresolved=unresolved,
               audited=[b.key for b in claimed_blocks],
               detail=None if not unresolved else 'a claimed name is not an auditable C/ACCEPTED_ASM function '
                                                  'of this TU (INCLUDE_ASM scaffold, helper or unknown)')
    nested_claims = []
    for block in claimed_blocks:
        if classes.get(block.key, source_class) != 'gnu-nested-function':
            continue
        nested_claims.extend(nested_by_parent.get(block.key) or nested_by_parent.get(block.name) or [])
    result['claims'] = dict(source='--claim/--ledger' if requested else
                            'default: non-empty C and ACCEPTED_ASM blocks', requested=requested,
                            functions=[b.key for b in claimed_blocks] +
                                      [item['original_name'] for item in nested_claims])
    claimed_names = {b.key for b in claimed_blocks} | {item['original_name'] for item in nested_claims}

    # -- scaffold macro include and headers ------------------------------------------
    include_asm_pin = (contract.get('include_asm') or {}).get('labels_inc') or {}
    used_macro_incs = sorted({m.group(1) for blk in asm.top_app for _, line in blk['lines']
                              for m in [re.fullmatch(r'\.include\s+"([^"]+)"', line)]
                              if m and m.group(1).endswith('.inc')})
    for inc in used_macro_incs:
        path = (asm_cwd or unit_dir) / inc
        if not path.is_file():
            path = unit_dir / inc
        ok, reason = (False, 'missing') if not path.is_file() else macro_include_ok(path.read_text(errors='replace'))
        digest = sha256_file(path) if path.is_file() else None
        pinned = include_asm_pin.get('sha256')
        if ok and pinned and digest != pinned:
            ok, reason = False, (f'not the pinned config/toolchains-tu.json include_asm.labels_inc '
                                 f'({pinned[:12]}...): {digest[:12] if digest else None}...')
        elif ok and not pinned:
            ok, reason = False, 'config/toolchains-tu.json has no include_asm.labels_inc pin'
        checks.add(f'scaffold_macros:{inc}', ok, sha256=digest, pinned=include_asm_pin.get('sha256'),
                   reason=reason)

    include_dirs = contract['include_dirs']
    used_files = [p for p in pre.files() if p != tu_rel]
    header_results, admitted = [], {}

    def resolve_used(name, including_file):
        """The file cpp opened for `#include "name"` in including_file."""
        candidates = [Path(including_file).parent / name] + [Path(d) / name for d in include_dirs]
        for cand in candidates:
            full = cand if cand.is_absolute() else unit_dir / cand
            if full.is_file():
                return cand.as_posix() if not cand.is_absolute() else str(cand)
        return None

    def admit(path_rel, depth=0):
        key = path_rel
        if key in admitted:
            return admitted[key]
        admitted[key] = True
        full = Path(path_rel) if Path(path_rel).is_absolute() else unit_dir / path_rel
        if not full.is_file():
            header_results.append(dict(path=path_rel, ok=False, reason='missing'))
            admitted[key] = False
            return False
        text = full.read_text(errors='replace')
        resolved = {n: resolve_used(n, path_rel) for n in included_names(text)}
        ok, reason, kind = True, None, 'c'
        header_class, header_class_provenance = source_class, {}
        try:
            class_metadata = json.loads((ROOT / SOURCE_CLASSES_FILE).read_text())
            header_class, header_class_provenance = owned_header_source_class(
                full, source_class, ROOT, class_metadata)
            admission(header_class)(text, Path(path_rel), {n for n, p in resolved.items() if p is not None})
        except ValueError as e:
            scaffold_ok, scaffold_reason = scaffold_header_ok(text)
            if scaffold_ok:
                kind = 'scaffold'
            elif Path(path_rel).name == 'debug_break.h' and debug_break_header_ok(text)[0]:
                kind = 'debug-break'
            else:
                ok, reason = False, f'{e}' + (f'; not a scaffold header: {scaffold_reason}' if scaffold_reason else '')
        for n, p in resolved.items():
            if p is None:
                ok, reason = False, f'unresolved include {n!r}'
            elif p not in used_files and p != tu_rel:
                ok, reason = False, f'include {n!r} resolves to {p}, which cpp did not open'
            elif depth < 24 and not admit(p, depth + 1):
                ok, reason = False, f'included header {n!r} refused'
        header_results.append(dict(path=path_rel, sha256=sha256_file(full), kind=kind, ok=ok, reason=reason,
                                   source_class=header_class, source_class_provenance=header_class_provenance))
        admitted[key] = ok
        return ok

    tu_includes = {n: resolve_used(n, tu_rel) for n in included_names(tu_text)}
    for n, p in tu_includes.items():
        if p is not None:
            admit(p)
    unadmitted = [p for p in used_files if p not in admitted]
    checks.add('headers_admitted', all(p is not None for p in tu_includes.values())
               and all(h['ok'] for h in header_results) and not unadmitted,
               unresolved=[n for n, p in tu_includes.items() if p is None], not_admitted=unadmitted,
               files_used=len(used_files),
               detail=None if not unadmitted else 'cpp opened files that no #include of the audited text '
                                                  'resolves to')
    result['headers'] = header_results
    header_set = {n for n, p in tu_includes.items() if p is not None}

    # -- the audited text is what cpp kept ---------------------------------------------
    binding = []
    try:
        i_tu = tu_edit.TU(pre.text)
        i_defs = {}
        for b in i_tu.blocks:
            if b.state == 'c':
                i_defs.setdefault(b.name, []).append(b)
    except tu_edit.ParseError as e:
        i_defs = None
        binding.append(f'the preprocessed unit could not be split: {e}')
    if i_defs is not None:
        for b in claimed_blocks:
            if b.state != 'c':
                continue
            defs = i_defs.get(b.name, [])
            if len(defs) != 1:
                binding.append(f'{b.key}: {len(defs)} definitions in the preprocessed unit')
                continue
            where, line = pre.attribute(defs[0].core_start)
            lo = tu_edit.line_of(tu_text, b.core_start)
            hi = tu_edit.line_of(tu_text, b.end)
            if where != tu_rel:
                binding.append(f'{b.key}: the compiled definition comes from {where}, not {tu_rel}')
            elif not (lo <= line <= hi):
                binding.append(f'{b.key}: the compiled definition is at {tu_rel}:{line}, outside the audited '
                               f'block ({lo}-{hi})')
            if b.conditional:
                binding.append(f'{b.key}: the audited definition sits inside a preprocessor conditional')
    checks.add('source_is_what_cpp_compiled', not binding, reasons=binding,
               detail=None if not binding else 'the claimed definition is not the one the compiler used')

    # -- tu.s corresponds to the TU blocks ----------------------------------------------
    expected_seq = []
    for entry in sorted([*tu.blocks, *[i for i in tu.items if i.kind == 'rodata']], key=lambda x: x.start):
        if getattr(entry, 'state', None) == 'c':
            expected_seq.append(('c', entry.name))
        else:
            expected_seq.append(('include', f'{entry.asm_dir}/{entry.name}.s'))
    helpers = {b.name for b in tu.blocks if b.role == 'helper'}
    got_seq = [(k, v) for k, v, _n, _s in asm.sequence if not (k == 'include' and _SCAFFOLD_INC.fullmatch(
        f'.include "{v}"'))]
    emitted_helpers = [n for k, n in got_seq if k == 'c' and n in helpers]
    # GCC emits a nested definition as its own region before its lexical
    # parent. It is not a second top-level TU block. Only the durable admitted
    # mappings qualify; audit_nested_c still proves each original identity,
    # compiler region and lexical/static-chain admission independently.
    nested_emissions = nested_alias_names(pub_nested, {r['name']: [] for r in asm.regions})
    got_cmp = [e for e in got_seq if not (e[0] == 'c' and e[1] in helpers | nested_emissions)]
    exp_cmp = [e for e in expected_seq if not (e[0] == 'c' and e[1] in helpers)]
    checks.add('assembly_matches_source', got_cmp == exp_cmp,
               nested_emissions_checked_separately=sorted(nested_emissions),
               expected=[f'{k}:{v}' for k, v in exp_cmp][:200] if got_cmp != exp_cmp else None,
               found=[f'{k}:{v}' for k, v in got_cmp][:200] if got_cmp != exp_cmp else None,
               detail=None if got_cmp == exp_cmp else 'the compiler output does not follow the TU blocks')
    checks.add('no_emitted_helpers', not emitted_helpers, emitted=emitted_helpers,
               detail=None if not emitted_helpers else 'an emitted helper is code the original TU lacks')

    # -- top-level asm --------------------------------------------------------------------
    allowed_contract_lines = set()
    for _c in list(contracts.values()) + [default_contract]:
        allowed_contract_lines.update((_c or {}).get('top_level_asm') or [])
    stray, waived = [], []
    for block in asm.top_app:
        for number, line in block['lines']:
            if _app_line_allowed(line) or _EXTERN_LINE.fullmatch(line):
                continue
            (waived if line in allowed_contract_lines else stray).append(f'line {number}: {line}')
    # A cc1 constant pool that ended a top-level #APP block is compiler output only
    # when no asm statement of the unit writes any of its lines: a hand-written
    # `asm(".rdata\n$LC9: ...")` has the pool's shape but its lines are in tu.i.
    # Lines a scaffold template may write anyway (`.align N`, `.section .sdata`)
    # are allowed top-level asm on their own and are not matched.
    written = asm_statement_lines(pre.text) if asm.pool_lines else set()
    for number, line in asm.pool_lines:
        if not _app_line_allowed(line) and _norm_asm_line(line) in written:
            (waived if line in allowed_contract_lines else stray).append(f'line {number}: {line}')
    checks.add('top_level_asm', not stray, lines=stray[:50], waived_by_contract=waived[:20],
               **({'cc1_pool_lines': len(asm.pool_lines)} if asm.pool_lines else {}),
               detail=None if not stray else 'only the INCLUDE_ASM/ACCEPTED_ASM/INCLUDE_RODATA expansions, the '
                                             'scaffold macro include, cc1 .extern lines and a cc1 constant pool '
                                             'no asm statement writes may be top-level asm')
    top_text_data = [d for d in asm.top_data if d['section'] in TEXT_SECTIONS]
    checks.add('no_data_in_text', not top_text_data,
               entries=[f'line {d["line"]}: {d["directive"]} {d["operands"][:30]}' for d in top_text_data][:10],
               detail=None if not top_text_data else 'cc1 emitted data into .text outside every function')

    # -- included scaffold, accepted asm and rodata payloads ---------------------------------
    included, scaffold_names = {}, set()
    def effective_path(rel_path):
        if asm_cwd is not None and (asm_cwd / rel_path).is_file():
            return asm_cwd / rel_path, True
        for inc in asm_includes:
            if (inc / rel_path).is_file():
                return inc / rel_path, False
        return unit_dir / rel_path, False

    entries = [(b.name, b.asm_dir, b.state, b) for b in tu.blocks if b.state != 'c']
    entries += [(i.name, i.asm_dir, 'rodata', None) for i in tu.items if i.kind == 'rodata']
    for name, folder, kind, block in entries:
        rel_path = f'{folder}/{name}.s'
        path, staged = effective_path(rel_path)
        text = path.read_text(errors='replace') if path.is_file() else None
        base = unit_dir / rel_path
        included[name if kind != 'rodata' else f'rodata:{name}'] = dict(
            kind=kind, path=path, rel=rel_path, staged=staged, text=text, block=block,
            base_text=base.read_text(errors='replace') if base.is_file() else None,
            defines=tu_edit.asm_file_functions(path) if text is not None else None)
        for m in re.finditer(r'^\s*(?:glabel|alabel|dlabel|ehlabel|jlabel|\.globl|\.global)\s+"?([A-Za-z_.$][\w.$]*)',
                             text or '', re.M):
            scaffold_names.add(m.group(1))
        for m in re.finditer(r'^\s*"?([A-Za-z_.$][\w.$]*)"?:', text or '', re.M):
            scaffold_names.add(m.group(1))
        for m in re.finditer(r'^\s*\.(?:l?comm)\s+([A-Za-z_.$][\w.$]*)', text or '', re.M):
            scaffold_names.add(m.group(1))
        # the scaffold `nonmatching NAME` macro defines NAME.NON_MATCHING
        for m in re.finditer(r'^\s*nonmatching\s+"?([A-Za-z_.$][\w.$]*)', text or '', re.M):
            scaffold_names.add(m.group(1) + '.NON_MATCHING')
    checks.add('included_files_present', all(v['text'] is not None for v in included.values()),
               missing=[v['rel'] for v in included.values() if v['text'] is None])

    # staged (cinc) copies may only drop .extern lines the C declares or defines
    staged_issues = []
    c_declared = set(asm.externs) | set(asm.defined_data) | {r['name'] for r in asm.regions}
    for v in included.values():
        if not v['staged'] or v['text'] is None or v['base_text'] is None:
            continue
        base_lines, lines = v['base_text'].splitlines(), v['text'].splitlines()
        dropped = [l for l in base_lines if l not in lines]
        if [l for l in lines if l not in base_lines]:
            staged_issues.append(f'{v["rel"]}: the staged copy adds lines')
        for line in dropped:
            m = re.fullmatch(r'\s*\.extern\s+([A-Za-z_.$][\w.$]*)\s*,\s*\d+\s*', line)
            if not m:
                staged_issues.append(f'{v["rel"]}: the staged copy drops {line.strip()[:40]!r}')
            elif m.group(1) not in c_declared:
                staged_issues.append(f'{v["rel"]}: dropped .extern {m.group(1)} that the C does not declare')
    if asm_cwd is not None:
        checks.add('staged_asm_overlay', not staged_issues, issues=staged_issues[:10],
                   detail='the staged copies differ from the unit files only by .extern lines the C output '
                          'declares or defines' if not staged_issues else None)

    extern_hits = []
    c_defined = {r['name'] for r in asm.regions} | set(asm.defined_data)
    for v in included.values():
        for m in re.finditer(r'^\s*\.extern\s+"?([A-Za-z_.$][\w.$]*)', v['text'] or '', re.M):
            if m.group(1) in c_defined:
                extern_hits.append(dict(symbol=m.group(1), asm=v['rel']))
    checks.add('no_extern_of_c_defined_symbols', not extern_hits, hits=extern_hits[:20],
               detail=None if not extern_hits else 'a symbol cc1 defines is .extern-declared in the TU asm (GAS '
                                                   'makes it external)')
    rodata_bad = []
    for key, v in included.items():
        if v['kind'] != 'rodata' or v['text'] is None:
            continue
        ok, reason = asm_data_only(v['text'])
        if not ok:
            rodata_bad.append(f'{v["rel"]}: {reason}')
    checks.add('include_rodata_payloads', not rodata_bad, issues=rodata_bad[:10],
               detail=None if not rodata_bad else 'an INCLUDE_RODATA payload contains code or switches to .text')

    # -- scaffold .word sites block their function -------------------------------------------
    word_sites = {}
    for entry in ((tu_record or {}).get('scaffold') or {}).get('word_directives') or []:
        if isinstance(entry, dict) and entry.get('function'):
            word_sites.setdefault(entry['function'], []).append(entry.get('va'))
    scaffold_word_functions = sorted(word_sites)

    # -- object symbols ------------------------------------------------------------------------
    obj_funcs = {}
    for s in obj.functions():
        obj_funcs.setdefault(s['name'], []).append(s)
    orig_funcs = [s for s in elf.symbols if s['type'] == STT_FUNC and s['section_index'] != 0]
    in_range = [s for s in orig_funcs if start <= int(s['va'], 16) < end] if start is not None else []
    pool = in_range if start is not None else orig_funcs
    if start is not None:
        nested_aliases = nested_alias_names(pub_nested, obj_funcs)
        nested_originals = set(nested_by_original)
        claimed_syms = set()
        for b in claimed_blocks:
            claimed_syms.add(b.key)
            if b.state == 'accepted_asm':
                claimed_syms |= set((included.get(b.name) or {}).get('defines') or [])
        obj_named = {n: v for n, v in obj_funcs.items() if len(v) == 1 and n in claimed_syms}
        deltas = sorted({int(s['va'], 16) - start - obj_named[s['name']][0]['value']
                         for s in in_range if s['name'] in obj_named})
        checks.add('range_alignment', deltas in ([0], []), deltas=deltas[:8], checked=len(obj_named))
        orig_names = {s['name'] for s in in_range}
        c_region_names = {r['name'] for r in asm.regions}
        extra_c = sorted(n for n in obj_funcs
                         if n not in orig_names and n in c_region_names and n not in nested_aliases)
        checks.add('no_extra_compiled_functions', not extra_c, extra=extra_c,
                   nested_aliases=sorted(nested_aliases))
        result['scaffold'] = dict(
            extra_func_symbols=sorted(n for n in obj_funcs
                                     if n not in orig_names and n not in c_region_names and n not in nested_aliases),
            nested_aliases=sorted(nested_aliases),
            missing_original_functions=sorted(n for n in orig_names
                                              if n not in obj_funcs and n not in nested_originals),
            word_sites=scaffold_word_functions)

    # -- C-owned data and COMMON ------------------------------------------------------------------
    data_row = None
    validated_alignment_data_row = None
    data_checks_start = len(checks.rows)
    data_audit_error = None
    try:
        data_row = data_gate_row(args, tu_c)
        validated_alignment_data_row = data_row
        owned = c_owned_sections(args, contract, tu_record, data_row)
        data_obj = obj
        selected_data_path = (getattr(args, 'data_carve_object', None) or
                              getattr(args, 'data_gate_object', None))
        if (data_row or {}).get('_audit_linked_data_object'):
            selected_data_path = data_row['_audit_linked_data_object']
        if selected_data_path:
            selected_data_path = Path(selected_data_path).resolve()
            data_obj = RelObject(selected_data_path.read_bytes())
        if args.data_gate_report:
            checks.add('linked_data_gate', data_row is not None,
                       report=rel(Path(args.data_gate_report).resolve()),
                       verified_locations=len((data_row or {}).get('verified_locations', [])))
            if (data_row or {}).get('_audit_candidate_record'):
                result['linked_data_candidate'] = data_row['_audit_candidate_record']
            if (data_row or {}).get('_audit_selected_object_chain'):
                result['selected_data_object_chain'] = data_row['_audit_selected_object_chain']
        result['data_ownership'] = {k: {kk: (f'0x{vv:08x}' if isinstance(vv, int) else vv) for kk, vv in v.items()}
                                    for k, v in owned.items()}
        owned_names = set()
        for section, rng in owned.items():
            if section in CARVED_ANON and rng.get('source') == 'config/tu/data-carves.json':
                rng['anon_bytes'] = carved_section_bytes(args, section, elf, data_obj)
            has_gate_spans = any(span.get('section') == section
                                 for span in (data_row or {}).get('verified_location_spans', []))
            if (selected_data_path and section not in ('.bss', '.sbss') and data_row is not None and
                    (rng.get('source') == 'config/tu/data-carves.json' or has_gate_spans)):
                ok, detail, accounted = linked_span_data_check(section, rng, elf, data_obj, data_row, args)
            else:
                ok, detail, accounted = data_section_check(section, rng, elf, obj, scaffold_names)
            owned_names |= accounted
            checks.add(f'c_owned_data:{section}', ok, **detail)
        comm_reasons, comm_names = [], set()
        orig_data_all = [s for s in elf.symbols if s['type'] in (STT_NOTYPE, STT_OBJECT)
                         and 0 < s['section_index'] < 0xff00 and elf.sections[s['section_index']]['flags'] & 2]
        for c in asm.comms:
            comm_names.add(c['name'])
            if c['kind'] != 'comm':
                continue
            sym = [x for x in obj.symbols if x['name'] == c['name'] and x['shndx'] in (SHN_COMMON, SHN_MIPS_SCOMMON)]
            cands = [o for o in orig_data_all if o['name'] == c['name'] and o['binding'] == 1]
            if len(cands) != 1:
                comm_reasons.append(f'{c["name"]}: {len(cands)} original global data symbols')
                continue
            o = cands[0]
            size = sym[0]['size'] if sym else c['size']
            align = sym[0]['value'] if sym else (c['align'] or 1)
            if size != o['size']:
                comm_reasons.append(f'{c["name"]}: COMMON size {size} vs original {o["size"]}')
            if align and int(o['va'], 16) % align:
                comm_reasons.append(f'{c["name"]}: COMMON alignment {align} does not divide {o["va"]}')
        checks.add('c_common_symbols', not comm_reasons, reasons=comm_reasons,
                   checked=sorted(c['name'] for c in asm.comms if c['kind'] == 'comm'))
        overlay_bss_names, overlay_bss_reasons = overlay_bss_owned_names(args, elf, obj)
        if overlay_bss_names or overlay_bss_reasons:
            checks.add('overlay_bss_storage', not overlay_bss_reasons, reasons=overlay_bss_reasons,
                       owned=sorted(overlay_bss_names))

        # -- no symbol beyond the original TU, the scaffold and the claims -------------------------
        if start is not None:
            allowed = ({s['name'] for s in in_range} | scaffold_names | owned_names | comm_names
                       | overlay_bss_names
                       | {r['name'] for r in asm.regions if r['name'] in {s['name'] for s in in_range}})
            allowed |= nested_alias_names(pub_nested, obj_funcs)
            for section, rng in owned.items():
                if rng.get('lo') is not None:
                    allowed |= {o['name'] for o in original_data_symbols(elf, rng['lo'], rng['hi'])}
            extra_syms = sorted({s['name'] for s in obj.symbols
                                 if s['shndx'] != 0 and s['type'] in (STT_NOTYPE, STT_OBJECT, STT_FUNC)
                                 and s['name'] and s['name'] not in MARKER_NAMES
                                 and not s['name'].startswith(('$', '.L')) and s['name'] not in allowed})
            checks.add('no_extra_object_symbols', not extra_syms, extra=extra_syms[:20],
                       detail=None if not extra_syms else 'the object defines names that are neither original, '
                                                          'scaffold-provided nor C-owned data')

    except (AuditError, OSError, ValueError, KeyError) as error:
        # Data scaffolding is permitted during function matching. A selected
        # object that changes emitted code remains a fatal provenance error.
        if (getattr(args, 'require_data_audit', False) or any(
                text in str(error) for text in ('changes compiler-emitted .text',
                                                'changes compiler-emitted function',
                                                'changes compiler-emitted text relocation'))):
            raise
        data_audit_error = f'{type(error).__name__}: {error}'
        data_row = None
    deferred_data_checks = []
    if not getattr(args, 'require_data_audit', False):
        for check in checks.rows[data_checks_start:]:
            if check['ok']:
                continue
            if check['check'] == 'no_extra_object_symbols':
                extra_functions = [s['name'] for s in obj.symbols
                                   if s['type'] == STT_FUNC and
                                   s['name'] in check.get('extra', [])]
                if extra_functions:
                    continue
            deferred_data_checks.append(dict(check))
            check.update(ok=True, informational=True, deferred=True)
    result['data_audit'] = dict(
        required=getattr(args, 'require_data_audit', False),
        pending=bool(data_audit_error or deferred_data_checks),
        error=data_audit_error, deferred_checks=deferred_data_checks,
        grants_data_recovery=False if data_audit_error or deferred_data_checks else True,
        note='Function acceptance permits original data scaffolding with zero data credit; '
             'explicit data recovery requires its own passing ownership and byte verification.')
    result['data_required'] = bool(getattr(args, 'require_data_audit', False))

    # -- eligibility text -------------------------------------------------------------------------
    admission_items, deferred_attributes, verified_placements = matching_data_section_attributes(
        tu, rel(tu_c), obj, elf, required=getattr(args, 'require_data_audit', False),
        data_row=data_row if result['data_audit']['grants_data_recovery'] else None,
        build_object=Path(args.obj) if args.obj else None)
    if verified_placements:
        checks.add('verified_original_data_placement', True, evidence=verified_placements)
    hardware_items, hardware_alignment = verified_vu_data_alignment(
        args, tu, rel(tu_c), obj, elf, validated_alignment_data_row,
        tu_s.read_text(encoding='utf-8', errors='surrogateescape'),
        tu_i.read_text(encoding='utf-8', errors='replace'))
    admission_items.update(hardware_items)
    if hardware_alignment:
        checks.add('verified_vu_data_alignment', True, evidence=hardware_alignment)
    packet_items, packet_alignment = verified_original_packet_alignment(
        args, tu, rel(tu_c), obj, elf, validated_alignment_data_row,
        tu_s.read_text(encoding='utf-8', errors='surrogateescape'),
        tu_i.read_text(encoding='utf-8', errors='replace'))
    admission_items.update(packet_items)
    if packet_alignment:
        checks.add('verified_original_packet_alignment', True, evidence=packet_alignment)
    if deferred_attributes:
        result['data_audit'].update(pending=True, grants_data_recovery=False,
                                    deferred_source_attributes=deferred_attributes)
        checks.add('inherited_data_section_attributes', True, informational=True,
                   deferred=True, attributes=deferred_attributes)
    prelude_end = tu.prelude_end()
    prelude = ''.join(admission_items.get(i.start, tu_text[i.start:i.end]) + '\n' for i in tu.items
                      if i.end <= prelude_end and i.kind != 'rodata')
    free = ''.join(admission_items.get(i.start, tu_text[i.start:i.end]) + '\n' for i in tu.free_items()
                   if i.start >= prelude_end and i.kind != 'rodata')
    free += ''.join(tu.block_text(b) + '\n' for b in tu.blocks if b.role == 'helper')
    try:
        admission(source_class)(prelude + free, Path(tu_rel), header_set)
        checks.add('free_text_admitted', True)
    except ValueError as e:
        checks.add('free_text_admitted', False, reason=str(e))

    # -- accepted-record fidelity inputs -------------------------------------------------------------
    adaptation_files = discover_adaptations(args.adaptations, tu_c, unit_dir)
    adaptations = load_adaptations(adaptation_files)
    result['adaptations'] = dict(files=[rel(f) for f in adaptation_files], entries=len(adaptations))

    # -- the accepted C functions this TU carries ----------------------------------------------------
    # `record_fidelity` runs per CLAIMED function, so an accepted neighbour the
    # candidate silently drops was examined by nothing at all: a worker claims its
    # own slice, and reverting an accepted C function to INCLUDE_ASM re-emits the
    # ORIGINAL bytes, so the whole-file rebuild is byte-identical and cannot see it
    # either. Measured on ov01/tu028 (task tu-audit-record-fidelity-candidate): a
    # candidate that turned the accepted `adjAngle` back into INCLUDE_ASM and
    # claimed only `fnMSP02_PR000` audited ok=true with grants_acceptance=true.
    #
    # So every accepted C record whose function lies in this TU's range is resolved
    # here, claimed or not: it must still be a C definition of this TU, and if it
    # was not claimed its body is put through the same comparison the claimed ones
    # get. `stale_pins` is the other, non-fatal half: the records whose whole-file
    # `source.sha256` no longer matches the file they point at. That is integrator
    # bookkeeping over a published tree -- a candidate diverges from it by
    # construction -- so it is reported for whoever publishes, never charged to a
    # function that is untouched (see record_fidelity).
    accepted_here, accepted_elsewhere = tu_accepted_c_functions(args.unit, args.tu_id, start, end)
    accepted_rows, accepted_bad, stale_pins = [], [], []
    for record_path, record, entry, address in accepted_here:
        name, va = entry.get('name'), f'0x{address:08x}'
        row = dict(function=name, va=va, record=record_path, record_sha256=record_digest(record_path))
        block = tu.by_name.get(name)
        if block is None:
            row.update(state='absent', ok=False, status='accepted_function_absent',
                       detail=f'{name} has an accepted C record ({record_path}) but this TU defines no '
                              f'block for it; an accepted function cannot be dropped by a candidate')
        elif block.state != 'c':
            row.update(state=block.state, ok=False, status='accepted_function_not_c',
                       detail=f'{name} has an accepted C record ({record_path}) but this TU delivers it '
                              f'as {block.state}; the scaffolding re-emits the original bytes, so the '
                              'whole-file rebuild cannot see the loss')
        elif name in claimed_names:
            row.update(state='c', ok=True, claimed=True, status='claimed',
                       detail='claimed by this audit: the body comparison is reported with the function')
        else:
            ok, detail = record_fidelity(block, tu, args.unit, args.tu_id, tu_rel, entry.get('va'),
                                         adaptations, rel(tu_c), dict(args=args, data_row=data_row))
            row.update(state='c', ok=ok, claimed=False, **detail)
        accepted_rows.append(row)
        if not row['ok']:
            accepted_bad.append(f'{name}: {row.get("detail")}')
        try:
            source_rel = relative_source(record, args.tu_id)
            on_disk = sha256_file(ROOT / source_rel)
        except (AuditError, KeyError, OSError):
            source_rel, on_disk = None, None
        pinned = relative_source_sha(record, args.tu_id)
        if source_rel is not None and on_disk != pinned:
            stale_pins.append(dict(record=record_path, source=source_rel, on_disk=on_disk, pinned=pinned))
    result['accepted_records'] = dict(
        functions=accepted_rows, stale_pins=stale_pins,
        not_carried=[dict(function=e.get('name'), va=f'0x{address:08x}', record=record_path,
                          state=(tu.by_name[e['name']].state if e.get('name') in tu.by_name else 'absent'),
                          detail='an accepted C record for a function of this TU\'s range whose source is '
                                 'not this TU: an acceptance the TU has not imported, which the per-TU '
                                 'ledger records as well')
                     for record_path, _record, e, address in accepted_elsewhere])
    checks.add('accepted_c_functions', not accepted_bad, reasons=accepted_bad,
               functions=[r['function'] for r in accepted_rows],
               unclaimed_verified=[r['function'] for r in accepted_rows
                                   if r['ok'] and not r.get('claimed') and r['state'] == 'c'],
               detail=None if not accepted_bad else
               'an accepted C function of this TU is no longer the accepted C function')

    # -- asm contract truthfulness ------------------------------------------------------------------
    # `load_asm_contract` above notes whether the eight fields are non-empty
    # and whether the evidence pin still hashes.  Neither says whether the prose is
    # TRUE about the code it is attached to, and that -- not bytes -- is what
    # returned most of the 2026-09-12 ee-vu-cop2 re-treatment submissions: a
    # contract for a different function, the exact_asm-era document for the
    # standalone .s, a document left stale when the allocation was narrowed, a
    # claim false about the delivered code, an omitted alignment precondition.
    # tools/asm_contract_lint.py derives the facts from the very blocks this
    # audit compiled and reports a claim the source contradicts.  It is
    # contradiction-driven: silence is not a finding, and a clean result means
    # "no claim is contradicted", never "the contract is true".  Since the
    # contract became optional review documentation (user direction
    # 2026-09-27) the lint rows are informational: contradictions are shown to
    # the reviewer, never an audit failure.
    lint_groups = {}
    for b in claimed_blocks:
        if b.state != 'c' or classes.get(b.key, source_class) != 'hardware':
            continue
        if b.key in pub_missing:
            continue
        parsed = contracts.get(b.key, default_contract)
        if parsed is None:
            continue
        path = (pub_contracts.get(b.key) or pub_contracts.get(None)
                or (rel(args.asm_contract) if args.asm_contract else '<none>'))
        lint_groups.setdefault(path, (parsed, {}))[1][b.key] = tu.block_text(b)
    for path, (parsed, bodies) in sorted(lint_groups.items()):
        try:
            lint_report = asm_contract_lint.check_bodies(
                parsed, bodies, unit=args.unit, root=ROOT, source_label=rel(tu_c),
                file_scope_text=prelude + free)
        except Exception as e:  # noqa: BLE001 - documentation lint never blocks
            checks.add(f'asm_contract_lint:{path}', True, informational=True, error=str(e),
                       functions=sorted(bodies),
                       detail='the asm contract could not be checked against the assembly it '
                              'describes (informational: contracts never block)')
            continue
        contradictions = [f'{f["check"]}: {f["detail"]}' for f in lint_report.contradictions]
        # Informational (user direction 2026-09-27): a contradicted contract is a
        # review note about the document, not a failure of the exact source.
        checks.add(f'asm_contract_lint:{path}', True, informational=True,
                   contradicted=bool(contradictions), functions=sorted(bodies),
                   contradictions=contradictions[:20],
                   undecided=sorted({f['check'] for f in lint_report.findings
                                     if f['severity'] == asm_contract_lint.UNDECIDABLE}),
                   notes=sorted({f['check'] for f in lint_report.findings
                                 if f['severity'] == asm_contract_lint.NOTE}),
                   not_checked=asm_contract_lint.NOT_CHECKED,
                   detail=None if not contradictions else
                   'the asm contract is contradicted by the inline assembly it describes: '
                   + '; '.join(contradictions[:4]))

    # -- per function --------------------------------------------------------------------------------
    regions = {}
    for r in asm.regions:
        regions.setdefault(r['name'], []).append(r)
    functions = []
    for b in claimed_blocks:
        if b.state == 'c':
            parent_result = audit_c(b, tu, prelude, tu_rel, header_set, regions, scaffold_names, obj_funcs,
                                    pool, start, classes.get(b.key, source_class),
                                    (None if b.key in pub_missing else contracts.get(b.key, default_contract)),
                                    word_sites,
                                    args.unit, args.tu_id, adaptations, rel(tu_c),
                                    tu_class=source_class, contract_lost=pub_missing.get(b.key),
                                    storage_view_context=dict(args=args, data_row=data_row))
            functions.append(parent_result)
            # The nested helper has no independent C block, but it does have a
            # real LOCAL FUNC symbol and a distinct acceptance record. Audit it
            # beside the parent so the pair is visible in the immutable report.
            entries = nested_by_parent.get(b.key) or nested_by_parent.get(b.name) or []
            for item in entries:
                nested_result = audit_nested_c(item, b, tu, prelude, tu_rel, header_set, regions,
                                               obj_funcs, pool, start,
                                               tu_class=source_class)
                functions.append(nested_result)
        else:
            functions.append(audit_accepted_asm(b, included, asm, obj_funcs, pool, start, word_sites, args.unit))
    result['functions'] = functions
    result['unit_checks'] = checks.rows
    result['summary'] = dict(
        claimed=len(functions), passed=sum(f['ok'] for f in functions), failed=sum(not f['ok'] for f in functions),
        record_fidelity=dict(collections.Counter(
            (f.get('checks', {}).get('record_fidelity') or {}).get('status', 'n/a') for f in functions)),
        accepted_records=dict(
            functions=len(accepted_rows),
            unclaimed=sum(1 for r in accepted_rows if not r.get('claimed') and r['state'] == 'c'),
            failed=sum(1 for r in accepted_rows if not r['ok']),
            stale_pins=len(stale_pins), not_carried=len(accepted_elsewhere),
            status=dict(collections.Counter(r.get('status', 'n/a') for r in accepted_rows))),
        unit_failures=[c['check'] for c in checks.rows if not c['ok']],
        contract_relaxed=result['contract_relaxed'],
        categories={c: sum(1 for f in functions if f['ok'] and f['category'] == c)
                    for c in ('exact_c', 'exact_c_with_asm', 'exact_asm')})
    result['ok'] = all(c['ok'] for c in checks.rows) and all(f['ok'] for f in functions)
    # An audit that claims nothing grants nothing, and says so instead of failing.
    result['grants_acceptance'] = bool(result['ok'] and functions and
                                     (not result['data_required'] or
                                      not result['data_audit']['pending']))
    if not functions:
        result['summary']['note'] = ('no claimed function: the TU checks passed but this audit grants no '
                                     'acceptance')
    return result


def verified_vu_data_alignment(args, tu, tu_rel, obj, elf, data_row, assembly, preprocessed):
    """Admit one evidenced VU store's DATA alignment in admission text only.

    The linked DATA gate has already validated the current source, compiler,
    assembly, carving and linked ELF hashes. This is not an ordinary-C
    attribute exception: the exact original sqc2 access requires a 16-byte
    address, and the same typed storage and capacity must be emitted by cc1.
    Missing evidence leaves the original attribute for normal rejection.
    """
    if (args.unit != 'main' or args.tu_id != 'main/tu208' or
            tu_rel != 'src/main/set_path.c' or not data_row):
        return {}, []
    tokens = tu_edit.c_tokens
    declaration = 'static SetPathDummyActor dmyActor_12 __attribute__((aligned(16)));'
    declarations = [item for item in tu.free_items() if item.kind == 'decl' and
                    not item.conditional and tokens(item.text(tu.text)) == tokens(declaration)]
    if len(declarations) != 1:
        return {}, []
    actor_type = '''typedef struct SetPathDummyActor {
        unsigned char unmodeled_00[0x50]; Vector4 rotation; Vector4 scale;
        unsigned char unmodeled_70[0x86 - 0x70]; short enabled;
        unsigned char unmodeled_88[0xa70 - 0x88]; } SetPathDummyActor;'''
    vector_type = 'typedef struct Vector4 { float x; float y; float z; float w; } Vector4;'
    def contains(haystack, needle):
        return any(haystack[i:i + len(needle)] == needle for i in range(len(haystack) - len(needle) + 1))
    if not contains(tokens(preprocessed), tokens(actor_type)) or not contains(tokens(preprocessed), tokens(vector_type)):
        return {}, []
    block = tu.by_name.get('seffectDebugCf')
    consumer = '__asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(&dmyActor_12.scale) : "memory");'
    if not block or block.state != 'c' or not contains(tokens(tu.block_text(block)), tokens(consumer)):
        return {}, []
    from tools.tu.storage_directives import emitted_storage
    storage = emitted_storage(assembly).get('dmyActor_12') or {}
    if (storage.get('section') != '.bss' or storage.get('size') != 2672 or
            storage.get('alignment') != 16 or storage.get('extent_kind') != 'label-space'):
        return {}, []
    symbols = [s for s in obj.symbols if s['name'] == 'dmyActor_12' and s['shndx'] != 0]
    section = obj.section('.bss')
    if (len(symbols) != 1 or not section or section['type'] != 8 or section['align'] < 16 or
            any(symbols[0][key] != value for key, value in
                dict(value=240, size=0, binding=0, type=STT_NOTYPE, section='.bss').items())):
        return {}, []
    rows = [r for r in data_row.get('verified_storage', []) if r.get('name') == 'dmyActor_12']
    if len(rows) != 1:
        return {}, []
    row = rows[0]
    if (not row.get('linked') or any(row.get(key) != value for key, value in
            dict(address='0x00585400', size=2672, section='.bss', bind=0,
                 type='NOTYPE', alignment=16).items())):
        return {}, []
    mapping = row.get('allocated_carve_mapping') or {}
    piece = mapping.get('piece') or {}
    if (mapping.get('raw_object_sha256') != data_row.get('object_sha256') or
            mapping.get('allocated_offset') != '0xf0' or
            mapping.get('address') != '0x00585400' or mapping.get('size') != 2672 or
            piece.get('offset') != '0xf0' or piece.get('length') != '0xa70' or
            piece.get('va') != '0x00585400' or piece.get('run_end') != '0x00585e70'):
        return {}, []
    original = [s for s in elf.symbols if s['name'] == 'dmyActor.12']
    caller = [s for s in elf.symbols if s['name'] == 'seffectDebugCf' and s['type'] == STT_FUNC]
    if (len(original) != 1 or len(caller) != 1 or
            any(original[0].get(key) != value for key, value in
                dict(va='0x00585400', size=0, binding=0, type=STT_NOTYPE).items())):
        return {}, []
    witnesses = {0x2dd1b4: 0x3c100058, 0x2dd1f4: 0x26105400,
                 0x2dd1f8: 0x24060a70, 0x2dd208: 0x26030060, 0x2dd20c: 0xf8600000}
    caller_lo = int(caller[0]['va'], 16)
    for address, word in witnesses.items():
        if not caller_lo <= address < caller_lo + caller[0]['size']:
            return {}, []
        segments = [p for p in elf.program_headers if p['type'] == 1 and
                    int(p['va'], 16) <= address and address + 4 <= int(p['va'], 16) + p['file_size']]
        if len(segments) != 1:
            return {}, []
        offset = segments[0]['file_offset'] + address - int(segments[0]['va'], 16)
        if elf.data[offset:offset + 4] != struct.pack('<I', word):
            return {}, []
    item = declarations[0]
    evidence = dict(name='dmyActor_12', original_name='dmyActor.12',
                    original_address='0x00585400', original_symbol_size=0,
                    storage_size=2672, field='scale', field_offset=96, alignment=16,
                    source_sha256=data_row['source_sha256'],
                    compiler_sha256=data_row['object_sha256'],
                    assembly_sha256=data_row['assembly_sha256'],
                    linked_elf_sha256=data_row['linked_elf_sha256'],
                    original_words={f'0x{k:08x}': f'0x{v:08x}' for k, v in witnesses.items()},
                    scope='admission text only; original source and emitted storage unchanged')
    return {item.start: 'static SetPathDummyActor dmyActor_12;'}, [evidence]


def verified_original_packet_alignment(args, tu, tu_rel, obj, elf, data_row,
                                       assembly, preprocessed):
    """One original-placement DATA alignment hypothesis, not hardware16.

    The SDK copies six 16-byte records with two ld instructions per record,
    requiring eight-byte source alignment. The stronger literal alignment16
    is a Source storage hypothesis accepted only with this exact original
    OBJECT, complete initializer and fresh linked DATA evidence. Nothing in
    ordinary-C function or general attribute admission is widened.
    """
    if (args.unit != 'main' or args.tu_id != 'main/tu132' or
            tu_rel != 'src/main/yajima_test.c' or not data_row):
        return {}, []
    tokens = tu_edit.c_tokens
    words = ((32769, 1342390272, 348654, 0), (458752, 0, 71, 0),
             (68, 0, 66, 0), (0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0))
    initializer = '{ ' + ', '.join('{ ' + ', '.join(
        f'.{field} = {value}' for field, value in zip(('x', 'y', 'z', 'w'), row)) + ' }'
        for row in words) + ' }'
    attribute = '__attribute__((aligned(16)))'
    declaration = 'static UmlDispVertex TestEnv_42[6] ' + attribute + ' = ' + initializer + ';'
    items = [item for item in tu.free_items() if item.kind == 'decl' and
             not item.conditional and tokens(item.text(tu.text)) == tokens(declaration)]
    if len(items) != 1:
        return {}, []
    def contains(text, fragment):
        haystack, needle = tokens(text), tokens(fragment)
        return any(haystack[i:i + len(needle)] == needle for i in range(len(haystack) - len(needle) + 1))
    vertex = 'typedef struct UmlDispVertex { int x; int y; int z; int w; } UmlDispVertex;'
    block = tu.by_name.get('UmlDispTestSub')
    if (not contains(preprocessed, vertex) or not block or block.state != 'c' or
            not contains(tu.block_text(block), 'sceVif1PkAddDirectDataN(*packet, TestEnv_42, 6);')):
        return {}, []
    emission = re.compile(r'\.align\s+4\s+\.type\s+TestEnv_42\s*,\s*@object\s+'
                          r'\.size\s+TestEnv_42\s*,\s*96\s+TestEnv_42:')
    # #APP cannot establish compiler-owned storage or alignment.
    outside = re.sub(r'^\s*#APP\s*$.*?^\s*#NO_APP\s*$', '', assembly, flags=re.M | re.S)
    if len(emission.findall(outside)) != 1:
        return {}, []
    symbols = [s for s in obj.symbols if s['name'] == 'TestEnv_42' and s['shndx'] != 0]
    section = obj.section('.data')
    if (len(symbols) != 1 or not section or section['align'] < 16 or
            any(symbols[0].get(k) != v for k, v in
                dict(value=1536, size=96, type=STT_OBJECT, binding=0, section='.data').items())):
        return {}, []
    original = [s for s in elf.symbols if s['name'] == 'TestEnv.42']
    if (len(original) != 1 or any(original[0].get(k) != v for k, v in
            dict(va='0x0036a630', size=96, type=STT_OBJECT, binding=0, section='.data').items())):
        return {}, []
    payload = struct.pack('<24i', *(v for row in words for v in row))
    original_bytes = elf.data[original[0]['file_offset']:original[0]['file_offset'] + 96]
    if obj.bytes_of(section)[1536:1632] != payload or original_bytes != payload:
        return {}, []
    locations = [r for r in data_row.get('verified_location_spans', [])
                 if 'TestEnv_42' in r.get('names', []) and r.get('section') == '.data']
    if len(locations) != 1:
        return {}, []
    location = locations[0]
    input_span = location.get('compiler_input_span') or {}
    if (location.get('range') != [0x36a630, 0x36a690] or location.get('length') != 96 or
            input_span.get('section') != '.data' or input_span.get('offset') != 1536 or
            location.get('c_object_section_alignment', 0) < 16 or
            location.get('original_identities') != [dict(name='TestEnv.42', address='0x0036A630',
                                                       binding='LOCAL', type='OBJECT', size=96)]):
        return {}, []
    witnesses = {0x2552dc: 0x3c020037, 0x2552e0: 0x2442a630,
                 0x2552ec: 0x0040282d, 0x25530c: 0x24060006,
                 0x25535c: 0x08082d3c, 0x20b510: 0xdce30000,
                 0x20b518: 0xdce40008, 0x20b524: 0x24e70010}
    for address, word in witnesses.items():
        segments = [p for p in elf.program_headers if p['type'] == 1 and
                    int(p['va'], 16) <= address and address + 4 <= int(p['va'], 16) + p['file_size']]
        if len(segments) != 1:
            return {}, []
        offset = segments[0]['file_offset'] + address - int(segments[0]['va'], 16)
        if elf.data[offset:offset + 4] != struct.pack('<I', word):
            return {}, []
    evidence = dict(name='TestEnv_42', original_name='TestEnv.42', address='0x0036a630',
                    size=96, elements=6, element_size=16, requested_alignment=16,
                    SDK_required_alignment=8, alignment_role='original-placement Source hypothesis',
                    original_payload_sha256=hashlib.sha256(payload).hexdigest(),
                    source_sha256=data_row['source_sha256'], compiler_sha256=data_row['object_sha256'],
                    assembly_sha256=data_row['assembly_sha256'], linked_elf_sha256=data_row['linked_elf_sha256'],
                    original_words={hex(k): hex(v) for k, v in witnesses.items()},
                    no_general_attribute_or_hardware_exception=True)
    item = items[0]
    return {item.start: item.text(tu.text).replace(attribute, ' ')}, [evidence]


def matching_data_section_attributes(tu, tu_rel, obj, elf, required=False, data_row=None, build_object=None):
    """Separate unchanged published data placement from function admission.

    This affects admission text only, never the compiled source or object.
    A complete DATA-only gate can admit one literal section attribute on an
    exact positive original OBJECT; its original size, binding, section and
    full linked compiler span remain mandatory. Matching may otherwise defer
    one literal data-section attribute on a published, unchanged top-level
    object declaration emitted in the requested data section. Original data
    identity remains a deferred data check, not a function-admission condition.
    Function attributes, new declarations, other attributes and headers remain
    subject to their ordinary source-class checks.
    """
    verified_only = required
    complete_data_only = bool(data_row and not tu.functions() and build_object and build_object.is_file() and
                              data_row.get('source_sha256') == sha256_file(ROOT / tu_rel) and
                              data_row.get('object_sha256') == sha256_file(build_object))
    if required and not complete_data_only:
        return {}, [], []
    published = TOOL_ROOT / tu_rel
    if not published.resolve().is_relative_to(TOOL_ROOT.resolve()) or not published.is_file():
        return {}, [], []
    published_text = published.read_text(errors='surrogateescape')
    published_declarations = {
        tuple(tu_edit.c_tokens(item.text(published_text)))
        for item in tu_edit.parse(published_text).free_items()
        if item.kind == 'decl' and not item.conditional
    }
    attribute = re.compile(
        r'\b__attribute__?\s*\(\s*\(\s*section\s*\(\s*'
        r'"(?P<section>\.(?:data|rodata|rdata|sdata|bss|sbss|lit4|lit8))"'
        r'\s*\)\s*\)\s*\)')
    replacements, deferred, verified = {}, [], []
    for item in tu.free_items():
        if item.kind != 'decl' or item.conditional:
            continue
        text = item.text(tu.text)
        found = list(attribute.finditer(text))
        if len(found) != 1:
            continue
        tokens = tu_edit.c_tokens(text)
        if ((tuple(tokens) not in published_declarations and not complete_data_only) or
                'typedef' in tokens or 'extern' in tokens):
            continue
        attrs = [i for i, token in enumerate(tokens) if token in ('__attribute', '__attribute__')]
        if len(attrs) != 1:
            continue
        pos = attrs[0]
        section = found[0]['section']
        expected = [tokens[pos], '(', '(', 'section', '(', '"' + section + '"', ')', ')', ')']
        if tokens[pos:pos + len(expected)] != expected or ('=' in tokens and pos > tokens.index('=')):
            continue
        names = {name for kind, name in tu_edit.decl_keys(text) if kind == 'ordinary'}
        if len(names) != 1:
            continue
        name = next(iter(names))
        original = [s for s in elf.symbols if s['name'] == name and s['type'] == STT_OBJECT]
        emitted = [s for s in obj.symbols if s['name'] == name and s['shndx'] != 0]
        if len(emitted) != 1:
            continue
        after = emitted[0]
        if (after['type'] != STT_OBJECT or after['size'] <= 0 or
                after.get('section') not in DATA_SECTIONS or
                _SECTION_ALIASES.get(after['section']) != _SECTION_ALIASES.get(section)):
            continue
        before = original[0] if len(original) == 1 else None
        original_identity_equal = bool(
            before and before['size'] == after['size'] and
            before['binding'] == after['binding'] and
            before.get('section') in DATA_SECTIONS and
            _SECTION_ALIASES.get(before['section']) == _SECTION_ALIASES.get(section))
        match = found[0]
        spans = [span for span in (data_row or {}).get('verified_location_spans', [])
                 if name in span.get('names', []) and name in span.get('object_symbols', [])]
        if complete_data_only and original_identity_equal and before['size'] > 0 and len(spans) == 1:
            span = spans[0]
            compiler_span = span.get('compiler_input_span') or {}
            address = int(before['va'], 16)
            if (span.get('section') == section and span.get('range') == [address, address + before['size']] and
                    span.get('length') == before['size'] and
                    compiler_span.get('section') == after['section'] and
                    compiler_span.get('offset') == after['value']):
                replacements[item.start] = text[:match.start()] + ' ' + text[match.end():]
                verified.append(dict(name=name, section=section, size=before['size'],
                                     address=before['va'], source_sha256=data_row['source_sha256'],
                                     object_sha256=data_row['object_sha256'],
                                     linked_elf_sha256=data_row['linked_elf_sha256'],
                                     evidence='complete_linked_data_only_original_positive_OBJECT'))
                continue
        if verified_only:
            continue
        replacements[item.start] = text[:match.start()] + ' ' + text[match.end():]
        deferred.append(dict(name=name, section=section, size=after['size'],
                             original_identity_equal=original_identity_equal,
                             published_source=rel(published),
                             published_sha256=sha256_file(published),
                             declaration_sha256=hashlib.sha256(text.encode()).hexdigest(),
                             grants_source_or_data_recovery=False,
                             reason='Unchanged inherited data placement; pending strict data/source audit.'))
    return replacements, deferred, verified


def audit_c(b, tu, prelude, tu_rel, header_set, regions, scaffold_names, obj_funcs, pool, start, cls,
            asm_contract, word_sites, unit=None, tu_id=None, adaptations=(), tu_repo=None,
            tu_class=None, contract_lost=None, storage_view_context=None):
    name = b.name
    reasons, checks = [], {}
    prov = []
    found = regions.get(name, [])
    if len(found) != 1:
        prov.append(f'{len(found)} cc1 regions (.ent/.end) for {name}')
    else:
        r = found[0]
        if r['section'] not in TEXT_SECTIONS:
            prov.append(f'region starts in section {r["section"]}')
        if r['file'] != tu_rel:
            prov.append(f'region is under .file {r["file"]!r}, not {tu_rel}')
        if not any(lbl == name for lbl, _ in r['labels']):
            prov.append(f'no {name}: label inside its region')
        if r['includes']:
            prov.append('.include inside the function region')
        for app in r['app']:
            bad = [line for _, line in app['lines']
                   if line.split(None, 1)[0] in DATA_DIRECTIVES or line.startswith(('.include', '.section'))]
            if cls not in INLINE_ASM_CLASSES:
                prov.append(f'inline asm (#APP at line {app["line"]}) in a {cls} function')
            elif cls == 'ee-assert-break' and [line.strip() for _, line in app['lines']
                                               if line.strip()] != ['break']:
                prov.append(f'an ee-assert-break #APP block (line {app["line"]}) must be exactly `break`')
            elif bad:
                prov.append(f'inline asm emits data/sections: {bad[:3]}')
        for d in r['data']:
            if d['section'] in TEXT_SECTIONS:
                prov.append(f'{d["directive"]} {d["operands"][:40]} in .text at line {d["line"]}')
            elif d['directive'] in WORD_LIKE and not all(
                    re.fullmatch(r'\$L\d+', op.strip()) for op in d['operands'].split(',')):
                prov.append(f'{d["directive"]} {d["operands"][:40]} at line {d["line"]} is not a cc1 jump table')
        checks['jump_table_words'] = sum(1 for d in r['data'] if d['directive'] in WORD_LIKE
                                         and d['section'] not in TEXT_SECTIONS)
    if name in scaffold_names or b.key in scaffold_names:
        prov.append(f'{name} is also defined by an included scaffold .s')
    if b.key in word_sites:
        prov.append(f'config/tu-build.json records scaffold .word sites for {b.key}: acceptance is blocked')
    checks['provenance'] = dict(ok=not prov, reasons=prov)
    reasons += prov
    sym_reasons, sym_detail = symbol_check(b.key, obj_funcs, pool, start)
    checks['symbol'] = dict(ok=not sym_reasons, reasons=sym_reasons, **sym_detail)
    reasons += sym_reasons
    elig, count = [], None
    text = prelude + tu.block_text(b) + '\n'
    composed = cls in INLINE_ASM_CLASSES and tu_class == 'native-ti'
    try:
        count = admission(cls, tu_class)(text, Path(tu_rel), header_set)
    except ValueError as e:
        elig.append(f'{cls + "+native-ti" if composed else cls} admission refused: {e}')
    if cls == 'runtime-verbatim' and not elig:
        try:
            from tools import match
            match.runtime_verbatim_provenance(ROOT, dict(source=dict(path=tu_rel), headers=[]))
        except (ValueError, OSError) as e:
            elig.append(f'runtime-verbatim provenance: {e}')
    elig_notes = []
    if cls == 'hardware' and count and asm_contract is None:
        # Optional review documentation (user direction 2026-09-27): noted, never
        # a reason.  The hardware admission above stays the enforced guard.
        elig_notes.append('hardware inline asm without an asm contract' +
                          (f' (declared contract {contract_lost} is missing)' if contract_lost else ''))
    checks['eligibility'] = dict(ok=not elig, source_class=cls, tu_source_class=tu_class,
                                 asm_contract_lost=contract_lost, notes=elig_notes,
                                 admission=(cls + '+native-ti' if composed else cls), reasons=elig,
                                 hardware_statements=count if cls in INLINE_ASM_CLASSES else None,
                                 emitted_inline_blocks=len(found[0]['app']) if len(found) == 1 else None)
    reasons += elig
    original = [x for x in pool if x['name'] == b.key]
    fid_ok, fid = record_fidelity(b, tu, unit, tu_id, tu_rel,
                                  original[0]['va'] if len(original) == 1 else None, adaptations,
                                  tu_repo, storage_view_context)
    if fid.get('record'):
        fid['record_sha256'] = record_digest(fid['record'])
    checks['record_fidelity'] = dict(ok=fid_ok, **fid)
    if not fid_ok:
        reasons.append(f'record fidelity: {fid.get("detail")}')
    return dict(name=b.key, block=name, state='c', claimed=True,
                category=emitted_c_category(cls, count, len(found[0]['app']) if len(found) == 1 else 0),
                ok=not reasons, reasons=reasons, checks=checks)


def audit_accepted_asm(b, included, asm, obj_funcs, pool, start, word_sites, unit):
    from tools import match
    reasons, checks = [], {}
    info = included.get(b.name)
    rel_path = f'{b.asm_dir}/{b.name}.s'
    parts = Path(b.asm_dir).parts
    if not (len(parts) >= 2 and parts[0] == 'src' and parts[1] == unit) or 'nonmatchings' in parts:
        reasons.append(f'ACCEPTED_ASM folder {b.asm_dir} is not tracked source under src/{unit}/')
    if not info or info['text'] is None:
        reasons.append(f'{rel_path} missing')
        return dict(name=b.key, state='accepted_asm', claimed=True, category='exact_asm', ok=False,
                    reasons=reasons, checks=checks)
    includes = [v for k, v, _n, _s in asm.sequence if k == 'include']
    if includes.count(rel_path) != 1:
        reasons.append(f'{rel_path} is included {includes.count(rel_path)} times by the compiler output')
    defined = list(info['defines'] or [])
    digest = hashlib.sha256((info['text'] or '').encode()).hexdigest()
    index = accepted_asm_records()
    records = {index[n][0] for n in defined if n in index}
    checks['accepted_record'] = dict(functions=defined, records=sorted(records), sha256=digest,
                                     record_sha256={r: record_digest(r) for r in sorted(records)})
    if len(records) != 1:
        reasons.append(f'{rel_path} does not resolve to exactly one accepted ee-asm record: {sorted(records)}')
    else:
        record_path = sorted(records)[0]
        record = index[defined[0]][1]
        expected = record['source']['sha256']
        checks['accepted_record'].update(record=record_path, expected_sha256=expected)
        if digest != expected:
            reasons.append(f'{rel_path} is not the accepted source of {record_path} (sha256 {digest[:12]}... '
                           f'vs {expected[:12]}...)')
        missing = [f['name'] for f in record['functions'] if f['name'] not in defined]
        extra = [n for n in defined if n not in {f['name'] for f in record['functions']}]
        if missing or extra:
            reasons.append(f'{rel_path} defines {defined}, the record has '
                           f'{[f["name"] for f in record["functions"]]}')
        if relative_source(record) == rel_path:
            # The record was repointed at this tracked .s when the pre-migration
            # layout was retired, so the hash comparison above is the record's own
            # pin rather than a second witness. Say so instead of reading it as
            # agreement between two sources.
            origin = retired_origin(record)
            checks['accepted_record'].update(
                record_is_tu_source=True, archived=(origin or {}).get('path'),
                note='the accepted record points at this tracked .s, so the sha256 comparison checks the '
                     'record pin and nothing else; the pre-migration copy it was accepted from is in the '
                     'archive' + (f" ({origin['path']})" if origin else ' (no provenance recorded)'))
            if origin and origin['present']:
                archived = hashlib.sha256((ROOT / origin['path']).read_bytes()).hexdigest()
                checks['accepted_record']['archived_sha256'] = archived
                if archived != digest:
                    reasons.append(f'{rel_path} differs from the archived pre-migration copy '
                                   f'{origin["path"]} (sha256 {digest[:12]}... vs {archived[:12]}...)')
    try:
        match.ee_assembly(info['text'], [dict(name=n) for n in defined])
        checks['ee_assembly'] = dict(ok=True, functions=defined)
    except ValueError as e:
        reasons.append(f'ee_assembly refused {rel_path}: {e}')
        checks['ee_assembly'] = dict(ok=False, reason=str(e))
    sym = {}
    for n in defined or [b.key]:
        if n in word_sites:
            reasons.append(f'config/tu-build.json records scaffold .word sites for {n}')
        r, d = symbol_check(n, obj_funcs, pool, start)
        sym[n] = dict(ok=not r, reasons=r, **d)
        reasons += [f'{n}: {x}' for x in r]
    checks['symbols'] = sym
    return dict(name=b.key, state='accepted_asm', covers=defined, claimed=True, category='exact_asm',
                ok=not reasons, reasons=reasons, checks=checks)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--unit', required=True, help='main, ov01, ov02, ov10, ov11 or ov12')
    ap.add_argument('--tu-c', required=True, help='TU C source')
    ap.add_argument('--unit-dir', required=True, help='directory the TU is compiled from (cwd, -I .)')
    ap.add_argument('--root', help='the repository tree to audit: its src, config, records, adaptations and '
                                   'originals (default: the tree enclosing --unit-dir, never the tree this '
                                   'script happens to live in)')
    ap.add_argument('--tu-id', help='TU id of config/tu-build.json (or the TU map id)')
    ap.add_argument('--obj', help='the build object this TU links into; compared with the audit\'s own compile')
    ap.add_argument('--build-s', '--tu-s', dest='build_s', help='the build cc1 output, compared with the audit\'s')
    ap.add_argument('--tu-i', dest='ignored_tu_i', help=argparse.SUPPRESS)
    ap.add_argument('--out-dir', help='empty directory for the audit\'s own compile (default: a temporary one)')
    ap.add_argument('--compile', action='store_true', help=argparse.SUPPRESS)
    ap.add_argument('--no-reassemble', action='store_true', help=argparse.SUPPRESS)
    ap.add_argument('--asm-cwd', help='directory the build assembles from (staged .extern overlay)')
    ap.add_argument('--asm-include', action='append', help='extra -I directory for the assembler')
    ap.add_argument('--range', help='START:END text range (hex VAs)')
    ap.add_argument('--functions', action='append', help='original function names in order')
    ap.add_argument('--skeleton', help='all-INCLUDE_ASM skeleton giving the function list')
    ap.add_argument('--claim', action='append', help='audit only these functions')
    ap.add_argument('--ledger', help='TU ledger JSON naming the claimed functions')
    ap.add_argument('--source-class', choices=CLASSES, help='default: config/tu-build.json contract.source_class')
    ap.add_argument('--function-class', action='append', help='NAME=CLASS override')
    ap.add_argument('--asm-contract', help='asm contract JSON (hardware inline asm, enumerated top-level asm)')
    ap.add_argument('--contract', help='compile contract JSON; must equal the TU contract of the manifests')
    ap.add_argument('--cc-dir')
    ap.add_argument('--assembler')
    ap.add_argument('--flags')
    ap.add_argument('--include-dir', action='append')
    ap.add_argument('--c-owned', action='append', help='SECTION[=START:END] owned by the C TU')
    ap.add_argument('--data-gate-report', help='passing linked C-data report, hash-bound to this TU build')
    ap.add_argument('--require-data-audit', action='store_true',
                    help='require complete data recovery checks in addition to function emission; '
                         'default: data_required=false; original scaffolding is permitted for '
                         'function acceptance with zero data credit')
    ap.add_argument('--candidate-manifest', help='manifest whose hash is recorded by the linked C-data report')
    ap.add_argument('--data-gate-object', help='object hash-pinned by linked C-data evidence; defaults to --obj')
    ap.add_argument('--allocation-report', action='append',
                    help='hash-bound raw-to-allocated overlay object report (repeatable)')
    ap.add_argument('--data-carve-report', action='append',
                    help='deterministic allocated-to-carved object report (repeatable)')
    ap.add_argument('--data-carve-object', help='selected carved object; defaults to --data-gate-object')
    ap.add_argument('--data-carve-tool', help='subject-root data_carve.py implementation used by the build')
    ap.add_argument('--data-carve-tool-sha256', help='SHA-256 pin for --data-carve-tool')
    ap.add_argument('--adaptations', action='append',
                    help='per-TU adaptation declarations (default: adaptations.json / merge-adaptations.json '
                         'beside the TU or its unit directory)')
    ap.add_argument('--json-out', help='write the audit JSON here (default stdout)')
    args = ap.parse_args(argv)
    try:
        result = audit(args)
    except (AuditError, tu_edit.ParseError, tu_edit.EditError, OSError, KeyError, ValueError) as e:
        result = dict(schema=SCHEMA, ok=False, unit=args.unit, tu_id=args.tu_id,
                      grants_acceptance=False, data_required=args.require_data_audit,
                      tool=dict(path='tools/tu_audit.py', sha256=sha256_file(__file__)),
                      error=f'{type(e).__name__}: {e}')
    except Exception as e:  # fail closed: an audit that cannot finish is a failing audit
        import traceback
        result = dict(schema=SCHEMA, ok=False, unit=args.unit, tu_id=args.tu_id,
                      grants_acceptance=False, data_required=args.require_data_audit,
                      tool=dict(path='tools/tu_audit.py', sha256=sha256_file(__file__)),
                      error=f'internal error {type(e).__name__}: {e}', traceback=traceback.format_exc()[-2000:])
    text = json.dumps(result, indent=1) + '\n'
    if args.json_out:
        Path(args.json_out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json_out).write_text(text)
    else:
        sys.stdout.write(text)
    if not result['ok']:
        summary = result.get('summary') or {}
        print(f'tu_audit: FAIL {result.get("error", "")} {summary.get("unit_failures", "")} '
              f'{[f["name"] for f in result.get("functions", []) if not f["ok"]]}', file=sys.stderr)
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Check an ``asm_contract`` against the assembly it claims to describe.

An ``asm_contract`` is the document that authorizes a function's constrained
inline assembly (AGENTS.md "Source and acceptance"; docs/ps2-capabilities.md).
``tools/tu_audit.py`` checks that its eight prose fields are non-empty and that
its ``evidence`` pin still hashes.  Neither says whether the prose is TRUE about
the code it is attached to, and on 2026-09-12 two independent review rounds
returned submissions for exactly that, in every case with byte-exact objects:

* a contract describing a completely different function
  (``xglQuaternion2Matrix`` shipped with ``xglQuaternionInterpolateLinear``'s);
* the ``exact_asm``-era document describing the standalone ``.s`` a re-treatment
  had just replaced (``MoveHokan``, ``TCAMERA_setView``);
* contracts left stale after the allocation was narrowed (main/tu219's "eight
  of the routines" for seven delivered, main/tu100's seven-function document);
* a claim that is false about the delivered code rather than merely stale
  (locals "never read back from memory" that the next C call reads);
* omitted 16-byte alignment preconditions for ``lqc2``/``sqc2`` against stack
  locals whose C types guarantee only 4.

This module derives the facts from the source and compares them with the
contract.  It is deliberately **contradiction-driven**: a fact the contract does
not mention is not a finding (the two contracts the reviewer named as exemplary,
main/tu210 ``svRotMatrixScale`` and the reworked main/tu256 ``TCAMERA_setView``,
both omit things a completeness rule would demand).  What it reports is a claim
the source contradicts.

What it CANNOT check, and says so in every report rather than staying silent:

* ``purpose`` and ``privilege`` are prose about intent and about hardware
  eligibility.  No pattern match decides whether a function really is entitled
  to run ``ei``, or whether the stated purpose is the operation the original
  performs.  Those stay a human reviewer's job.
* whether an alignment precondition is actually SATISFIED.  The linter reports
  that a precondition exists and whether the contract states it; the latent
  correctness question is the project's separate ``tu-align-vector4-lqc2-audit``.
* the semantic truth of ``memory``/``state`` beyond the mechanical claims below:
  lane semantics, ACC/Q producer-consumer order, aliasing arguments.
* instruction counts and extents ("30 COP2 instructions", "34 instructions /
  136 bytes").  Off-by-one prose about counts is cosmetic and flagging it would
  bury the real findings.

So a clean run means "no contract claim is contradicted by the source", never
"this contract is true".  Every report carries that sentence.

Usage:
    python3 -B tools/asm_contract_lint.py --contract C.json --source TU.c \\
        --function NAME [--function NAME ...] [--unit main] [--json-out R.json]
    python3 -B tools/asm_contract_lint.py --contract C.json --asm-source F.s \\
        --function NAME [--unit main]
    python3 -B tools/asm_contract_lint.py --sweep [--json-out R.json]

Exit status: 0 no contradiction, 1 at least one contradiction, 2 usage/parse.
"""
import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

PROSE_FIELDS = ('purpose', 'inputs', 'outputs', 'clobbers', 'memory', 'state',
                'privilege', 'control_flow')

# Fields whose truth this tool does not attempt to decide at all.
UNCHECKABLE_FIELDS = ('purpose', 'privilege')


class LintError(ValueError):
    """The linter could not run: a malformed input, not a contract finding."""


# ---------------------------------------------------------------------------
# Lexical helpers (comments and string literals)
# ---------------------------------------------------------------------------

def blank_comments(text):
    """Replace comments with spaces, preserving offsets and newlines."""
    out, i, n = [], 0, len(text)
    while i < n:
        ch = text[i]
        if ch == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join('\n' if c == '\n' else ' ' for c in text[i:j]))
            i = j
        elif ch == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif ch in '"\'':
            j, esc = i + 1, False
            while j < n:
                if esc:
                    esc = False
                elif text[j] == '\\':
                    esc = True
                elif text[j] == ch:
                    j += 1
                    break
                elif text[j] == '\n' and ch == '"':
                    break
                j += 1
            out.append(text[i:j])
            i = j
        else:
            out.append(ch)
            i += 1
    return ''.join(out)


def split_top(text, sep):
    """Split on `sep` at parenthesis depth 0, outside string/char literals."""
    parts, depth, cur, instr, inch, esc = [], 0, '', False, False, False
    for ch in text:
        if instr or inch:
            cur += ch
            if esc:
                esc = False
            elif ch == '\\':
                esc = True
            elif instr and ch == '"':
                instr = False
            elif inch and ch == "'":
                inch = False
            continue
        if ch == '"':
            instr, cur = True, cur + ch
        elif ch == "'":
            inch, cur = True, cur + ch
        elif ch in '([':
            depth, cur = depth + 1, cur + ch
        elif ch in ')]':
            depth, cur = depth - 1, cur + ch
        elif ch == sep and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    return parts


_ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '\\': '\\', '"': '"', "'": "'", '0': '\0'}


def literal_prefix(item):
    """(decoded, rest) for a run of adjacent string literals, else None."""
    bodies, i, n = [], 0, len(item)
    while True:
        while i < n and item[i] in ' \t\r\n':
            i += 1
        if i >= n or item[i] != '"':
            break
        j, body, ok = i + 1, '', False
        while j < n:
            ch = item[j]
            if ch == '\\':
                if j + 1 >= n:
                    return None
                body += _ESCAPES.get(item[j + 1], item[j + 1])
                j += 2
                continue
            if ch == '"':
                ok = True
                break
            body += ch
            j += 1
        if not ok:
            return None
        bodies.append(body)
        i = j + 1
    if not bodies:
        return None
    return ''.join(bodies), item[i:]


# ---------------------------------------------------------------------------
# Inline assembly statements
# ---------------------------------------------------------------------------

_ASM_KEYWORD = re.compile(r'\b(?:__asm__|__asm|asm)\b')
_ASM_HEAD = re.compile(r'\s*(?:__volatile__|__volatile|volatile)?\s*(?:goto\s*)?\(')
# `register T v asm("$8");` is a GNU storage declaration, not a statement.
_REG_BIND = re.compile(r'\bregister\b[^;{}()]{1,200}?\b(?:asm|__asm|__asm__)\s*\(\s*'
                       r'"(?P<reg>\$[A-Za-z0-9]{1,4})"\s*\)')

_CONSTRAINT = re.compile(r'^[=+&%#*?!]*[a-zA-Z0-9]+$')


class Block:
    """One inline assembly statement of a function."""

    def __init__(self, start, end, lines, outputs, inputs, clobbers, volatile, is_goto):
        self.start, self.end = start, end
        self.lines = lines
        self.outputs, self.inputs = outputs, inputs
        self.clobbers = clobbers
        self.volatile, self.is_goto = volatile, is_goto

    @property
    def constraints(self):
        return [c for c, _ in self.outputs] + [c for c, _ in self.inputs]

    @property
    def mnemonics(self):
        found = []
        for line in self.lines:
            token = line.split(None, 1)[0] if line.split() else ''
            if token and not token.startswith(('.', '#')) and not token.endswith(':'):
                found.append(token)
        return found


def _operand_section(section):
    """[(constraint, expression)] for one operand section; [] when empty."""
    result = []
    for item in split_top(section, ','):
        if not item.strip():
            continue
        parsed = literal_prefix(item)
        if parsed is None:
            result.append(('?', item.strip()))
            continue
        constraint, rest = parsed
        expr = rest.strip()
        if expr.startswith('[') and ']' in expr:            # [name] "c" (expr)
            expr = expr.split(']', 1)[1].strip()
        result.append((constraint.strip(), expr.strip().strip('()').strip()))
    return result


def _clobber_section(section):
    names = []
    for item in split_top(section, ','):
        if not item.strip():
            continue
        parsed = literal_prefix(item)
        names.append(parsed[0].strip() if parsed else item.strip())
    return names


def asm_statements(text):
    """Every inline asm statement in `text`, in source order.

    Generic: this is not the COP2 allowlist of tools/match.py, because a
    contract has to be checked against whatever the source actually wrote,
    including a spelling the admission table would refuse.
    """
    masked = blank_comments(text)
    # Blank `register ... asm("$N")` bindings so they are not read as statements.
    for m in _REG_BIND.finditer(masked):
        masked = masked[:m.start()] + ''.join(
            '\n' if c == '\n' else ' ' for c in masked[m.start():m.end()]) + masked[m.end():]
    blocks = []
    for m in _ASM_KEYWORD.finditer(masked):
        head = _ASM_HEAD.match(masked, m.end())
        if not head:
            continue
        depth, i, n = 0, head.end() - 1, len(masked)
        while i < n:
            ch = masked[i]
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if not depth:
                    break
            i += 1
        if i >= n:
            raise LintError(f'unbalanced parentheses in an inline asm statement at offset {m.start()}')
        body = text[head.end():i]
        sections = split_top(body, ':')
        decoded = literal_prefix(sections[0])
        if decoded is None or decoded[1].strip():
            raise LintError('an inline asm template is not a string literal at offset %d' % m.start())
        lines = [ln.strip() for ln in decoded[0].split('\n') if ln.strip()]
        outputs = _operand_section(sections[1]) if len(sections) >= 2 else []
        inputs = _operand_section(sections[2]) if len(sections) >= 3 else []
        clobbers = _clobber_section(sections[3]) if len(sections) >= 4 else []
        blocks.append(Block(m.start(), i + 1, lines, outputs, inputs, clobbers,
                            'volatile' in masked[m.end():head.end()],
                            'goto' in masked[m.end():head.end()]))
    return blocks


def register_pins(text):
    """[(register, offset)] for every `register T v asm("$R")` declaration."""
    masked = blank_comments(text)
    return [(m.group('reg'), m.start()) for m in _REG_BIND.finditer(masked)]


# ---------------------------------------------------------------------------
# Assembly-level facts
# ---------------------------------------------------------------------------

BRANCH = re.compile(r'^(b(?:eq|ne|gez|gtz|lez|ltz|eqz|nez|c[0-3][tf]|al|gezal|ltzal)?'
                    r'(?:l)?|j|jal|jr|jalr|jalx)(?:\.[a-z]+)?$')
CALL = re.compile(r'^(jal|jalr|jalx|bal|bgezal|bltzal)$')

# Mnemonics whose presence is the reason a contract exists at all: COP2/VU0
# macro mode, the COP1/COP2 transfer pairs, the R5900 quadword move and the
# synchronization/interrupt primitives.  Scalar compiler mnemonics (jr, addiu,
# lui, sw, nop) are deliberately NOT here: a good contract legitimately
# discusses compiler-emitted scalar code as context, and flagging that would
# have flagged both exemplary contracts.
HARDWARE_MNEMONICS = {
    'lqc2', 'sqc2', 'lq', 'sq', 'qmfc2', 'qmtc2', 'cfc2', 'ctc2', 'mfc1', 'mtc1',
    'vcallms', 'vcallmsr', 'vsqrt', 'vrsqrt', 'vdiv', 'vwaitq', 'vrget', 'vrnext',
    'vrinit', 'vrxor', 'vnop', 'vclipw', 'vilwr', 'viswr', 'vlqi', 'vsqi',
    'vmove', 'vmr32', 'vabs', 'vadd', 'vaddq', 'vaddi', 'vsub', 'vsubq', 'vsubi',
    'vmul', 'vmulq', 'vmuli', 'vmadd', 'vmsub', 'vmula', 'vmsuba', 'vmadda',
    'vmax', 'vmini', 'vftoi0', 'vftoi4', 'vftoi12', 'vftoi15', 'vitof0', 'vitof4',
    'vitof12', 'vitof15', 'vopmula', 'vopmsub', 'viadd', 'visub', 'viaddi',
    'movz', 'movn', 'sync', 'ei', 'di', 'pcpyld', 'pcpyud', 'pcpyh', 'por', 'pextlw',
}


def base_mnemonic(token):
    """`vmulax.xyzw` -> `vmulax`, `sync.l` -> `sync`."""
    return token.split('.', 1)[0].lower()


def is_hardware_mnemonic(token):
    base = base_mnemonic(token)
    if base in HARDWARE_MNEMONICS:
        return True
    # vmulax / vmadday / vmaddaz / vaddx ... : a family member with a lane
    # suffix fused into the mnemonic.
    return any(base.startswith(fam) and base != fam for fam in
               ('vmula', 'vmadda', 'vmsuba', 'vmul', 'vmadd', 'vmsub', 'vadd', 'vsub',
                'vmax', 'vmini', 'vopm'))


QUAD_OPS = ('lqc2', 'sqc2', 'lq', 'sq')
_MEM_OPERAND = re.compile(r'(?P<disp>-?(?:0x[0-9a-fA-F]+|\d+))?\s*\(\s*(?P<base>%\d+|\$[a-z0-9]+)\s*\)')


def quad_accesses(block):
    """[(mnemonic, operand_index_or_None, register_or_None)] for quadword ops."""
    found = []
    for line in block.lines:
        parts = line.split(None, 1)
        if not parts or base_mnemonic(parts[0]) not in QUAD_OPS:
            continue
        m = _MEM_OPERAND.search(line)
        if not m:
            found.append((parts[0], None, None))
            continue
        base = m.group('base')
        if base.startswith('%'):
            found.append((parts[0], int(base[1:]), None))
        else:
            found.append((parts[0], None, base))
    return found


# ---------------------------------------------------------------------------
# C locals
# ---------------------------------------------------------------------------

_DECL = re.compile(r'^\s*(?P<type>(?:const\s+|volatile\s+|static\s+|register\s+|unsigned\s+|signed\s+|'
                   r'struct\s+|union\s+|long\s+|short\s+)*[A-Za-z_][A-Za-z0-9_]*)\s+'
                   r'(?P<decl>[^;=]*[A-Za-z_][A-Za-z0-9_]*\s*(?:\[[^\]]*\])?)\s*(?:=[^;]*)?;')
_ALIGNED16 = re.compile(r'__attribute__\s*\(\s*\(\s*[^)]*aligned\s*\(\s*(?:16|32|64|128)\s*\)')


def c_locals(body_text):
    """{name: {'type', 'pointer', 'aligned16'}} for the function's own locals.

    Only declarations in the outermost statement list are read: that is where a
    quadword-addressed local lives, and a nested one would need the same
    treatment anyway.  Parameters are excluded (they are not stack objects the
    contract has to justify; a pointer parameter's alignment is the caller's).
    """
    masked = blank_comments(body_text)
    result = {}
    depth = 0
    for line in masked.split('\n'):
        opens, closes = line.count('{'), line.count('}')
        if depth <= 1:
            m = _DECL.match(line)
            if m and not re.match(r'^\s*(return|if|for|while|switch|do|else|case|goto)\b', line):
                decl = m.group('decl').strip()
                for one in decl.split(','):
                    one = one.strip()
                    name = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$', one)
                    if not name:
                        continue
                    result[name.group(1)] = dict(
                        type=m.group('type').strip(), pointer=one.lstrip().startswith('*'),
                        aligned16=bool(_ALIGNED16.search(line)))
        depth += opens - closes
    return result


# ---------------------------------------------------------------------------
# Contract prose
# ---------------------------------------------------------------------------

# A colon is NOT a clause break: "... is not a hardware operation: it is the
# delay-slot filler ..." is one statement, and splitting it dropped the negation
# that scopes the second half.
_CLAUSE_SPLIT = re.compile(r'(?<=[.;])\s+|\s+--\s+|\n+')
_NEGATION = re.compile(
    r"\b(?:no|not|never|neither|nor|none|nothing|without|cannot|can't|isn't|aren't|doesn't|don't)\b"
    r"|\bis not\b|\bare not\b|\bdoes not\b|\bdo not\b|\bn't\b", re.I)
# Phrases that place a construct outside the inline block: a contract saying the
# compiler emits a return, or that calls FOLLOW the block, is not claiming the
# block contains one.  `original` is deliberately absent: MoveHokan's false
# claim was "Preserves the original branch-likely forms ...".
_OUT_OF_BLOCK = re.compile(
    r'\b(?:follow(?:s|ed|ing)?|outside|surround(?:s|ing)?|epilogue|prologue|cc1|compiler|'
    r'assembler|after (?:this|the) block|before (?:this|the) block|around (?:it|the block)|'
    r'does not apply|ordinary c|plain c|between|c body|the c\b|jal\b|tail jump|after the call|'
    r'preced(?:e|es|ing)|sits inside)', re.I)
# Case-sensitive: a standalone capital `C` is the language.  A clause that says
# the construct is in the C -- "the same C `if (...)` branch", "the C body calls
# ..." -- is not claiming the inline block contains one.
_OUT_OF_BLOCK_C = re.compile(r'(?<![A-Za-z0-9_])C(?![A-Za-z0-9_])')

HAZARD_TERMS = {
    'branch': re.compile(r'\bbranch(?:es|ing)?\b|\bbranch-likely\b', re.I),
    'call': re.compile(r'\bcalls?\b|\bcalled\b', re.I),
    'delay slot': re.compile(r'\bdelay[- ]slots?\b', re.I),
    'label': re.compile(r'\blabels?\b', re.I),
    'loop': re.compile(r'\bloops?\b', re.I),
}

_QUOTED_CONSTRAINT = re.compile(r'"([=+&%#*?!]{0,3}[a-zA-Z0-9])"')
_NUMBER_WORDS = dict(one=1, two=2, three=3, four=4, five=5, six=6, seven=7, eight=8,
                     nine=9, ten=10, eleven=11, twelve=12)
_COUNT_CLAIM = re.compile(
    r'(?<![A-Za-z0-9_#\u00a7])(?P<n>one|two|three|four|five|six|seven|eight|nine|ten|eleven|'
    # ... and an ordinal is not a count: without this lookahead `eighth` reads as
    # `eight`, which was invisible only because the old `mid` group happened to
    # stop at the digit of a following `lqc2`.
    r'twelve|\d{1,2})(?![A-Za-z])'
    # A digit INSIDE a token is part of a name, not a second count: `VU0`, `COP2`,
    # `$f8`, `0x20`.  The `mid` group used to exclude every digit, so the sentence
    # "keeps two constrained VU0 macro-mode blocks" did not match at all -- the
    # `0` of `VU0` broke it -- and the claim was read from a later sentence
    # instead, producing a contradiction that was not in the document.  A
    # contradiction-driven linter that invents contradictions is worse than one
    # that misses them, so only a STANDALONE number is excluded here.
    r'(?P<mid>(?:[^.;"#\d]|(?<=[A-Za-z0-9])\d|0(?=[xX])){0,60}?)'
    r'(?P<noun>`?__asm__`?[^.;]{0,30}?statements?|asm statements?|inline[- ]asm statements?|'
    r'inline assembly statements?|asm blocks?|statements? per function|blocks?)',
    re.I)

# Register spellings.  Numeric names are authoritative; the ABI names are
# accepted so a contract that talks in `v0`/`a1`/`s0` terms is still compared.
_ABI_GPR = dict(zero=0, at=1, v0=2, v1=3, a0=4, a1=5, a2=6, a3=7, t0=8, t1=9, t2=10,
                t3=11, t4=12, t5=13, t6=14, t7=15, s0=16, s1=17, s2=18, s3=19, s4=20,
                s5=21, s6=22, s7=23, t8=24, t9=25, k0=26, k1=27, gp=28, sp=29, fp=30,
                s8=30, ra=31)
# Only these ABI names are recognised without a `$`: the rest (`at`, `sp`, `gp`,
# `fp`, `zero`, `t0`..`t9`, `k0`, `k1`) are ordinary English or too short to be
# read as a register out of prose without one.
_BARE_ABI = {'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 'ra',
             's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7'}
_REG_TOKEN = re.compile(r'(?<![A-Za-z0-9_])(?P<dollar>\$)?(?P<name>f?\d{1,2}|[a-z]{1,2}\d?)(?![A-Za-z0-9_])')


def normalise_register(token):
    """A canonical `$N` / `$fN` name for a register spelling, else None."""
    t = token.strip().lstrip('$').lower()
    if re.fullmatch(r'f\d{1,2}', t) and int(t[1:]) < 32:
        return '$' + t
    if re.fullmatch(r'\d{1,2}', t) and int(t) < 32:
        return '$' + t
    if t in _ABI_GPR:
        return '$%d' % _ABI_GPR[t]
    return None


def registers_in(text):
    """Canonical register names a piece of prose spells out."""
    found = set()
    for m in re.finditer(r'(?<![A-Za-z0-9_.])\$(?P<n>f?\d{1,2})(?![A-Za-z0-9_])', text):
        r = normalise_register(m.group('n'))
        if r:
            found.add(r)
    for m in re.finditer(r'(?<![A-Za-z0-9_.$])(?P<n>[a-z]{1,2}\d)(?![A-Za-z0-9_])', text):
        name = m.group('n')
        if name in _BARE_ABI:
            found.add(normalise_register(name))
    # bare `ra`
    if re.search(r'(?<![A-Za-z0-9_.$])ra(?![A-Za-z0-9_])', text):
        found.add('$31')
    # bare COP1 names `f0`..`f31`, but never the `vf` prefix of a VU register.
    for m in re.finditer(r'(?<![A-Za-z0-9_.$v])f(?P<n>\d{1,2})(?![A-Za-z0-9_])', text):
        if int(m.group('n')) < 32:
            found.add('$f' + m.group('n'))
    return found


def clauses(text):
    return [c.strip() for c in _CLAUSE_SPLIT.split(text or '') if c.strip()]


def asserted_clauses(text, allow_out_of_block=False):
    """Clauses that assert something, i.e. are neither negated nor out-of-block."""
    out = []
    for c in clauses(text):
        if _NEGATION.search(c):
            continue
        if not allow_out_of_block and (_OUT_OF_BLOCK.search(c) or _OUT_OF_BLOCK_C.search(c)):
            continue
        out.append(c)
    return out


# Words that look like identifiers but are project/tool vocabulary, not symbols.
_IDENT_STOPLIST = {
    'cc1', 'cpp', 'gcc', 'gnu', 'iso', 'abi', 'sdk', 'ee', 'iop', 'vu0', 'vu1', 'cop0',
    'cop1', 'cop2', 'r5900', 'gpr', 'fpr', 'vf', 'vi', 'acc', 'sha256', 'agents',
    'tu_audit', 'tu_edit', 'match', 'asm_contract', 'include_asm', 'accepted_asm',
    'xyzw', 'xyz', 'xz', 'xy', 'yz', 'ps2', 'dma', 'gif', 'vif', 'nan', 'gp', 'hi', 'lo',
    'o2', 'o1', 'g0', 'g8', 'x86', 'utf', 'json', 'md', 'py', 'mac',
}
_IDENT = re.compile(r'(?<![A-Za-z0-9_.$/])[A-Za-z_][A-Za-z0-9_]{2,}(?![A-Za-z0-9_])')
_PATHISH = re.compile(r'[A-Za-z0-9_./-]*\.(?:md|json|py|c|h|s|txt|ld|yaml)\b')


def symbol_shaped(word):
    """A token that reads as a code symbol rather than an English word."""
    if word.lower() in _IDENT_STOPLIST:
        return False
    if is_hardware_mnemonic(word) or base_mnemonic(word) in HARDWARE_MNEMONICS:
        return False
    if re.fullmatch(r'v?f?\d+', word) or re.fullmatch(r'vf\d+', word):
        return False
    if '_' in word and len(word) >= 4:
        return True
    if re.search(r'[a-z][A-Z]', word):                   # camelCase / PascalCase
        return True
    return False


def contract_identifiers(contract):
    """Symbol-shaped identifiers the prose fields mention."""
    found = set()
    for field in PROSE_FIELDS:
        text = _PATHISH.sub(' ', str(contract.get(field) or ''))
        for m in _IDENT.finditer(text):
            if symbol_shaped(m.group(0)):
                found.add(m.group(0))
    return found


# ---------------------------------------------------------------------------
# Source facts
# ---------------------------------------------------------------------------

class Facts:
    """Everything the linter derives from the delivered source."""

    def __init__(self, kind, functions, source_path, whole_text):
        self.kind = kind                  # 'c' or 'asm'
        self.functions = functions        # {name: dict(...)}
        self.source_path = source_path
        self.whole_text = whole_text
        self.file_scope_asm = []

    @property
    def blocks(self):
        return [b for f in self.functions.values() for b in f['blocks']]

    def all_mnemonics(self):
        return {base_mnemonic(t) for b in self.blocks for t in b.mnemonics}

    def all_clobbers(self):
        out = set()
        for b in self.blocks:
            out |= set(b.clobbers)
        return out

    def declared_registers(self):
        out = set()
        for name in self.all_clobbers():
            reg = normalise_register(name)
            if reg:
                out.add(reg)
        for f in self.functions.values():
            for reg, _ in f['register_pins']:
                out.add(normalise_register(reg) or reg)
        return out

    def template_registers(self):
        out = set()
        for b in self.blocks:
            for line in b.lines:
                for m in re.finditer(r'\$(?:f?\d{1,2})(?![A-Za-z0-9_])', line):
                    r = normalise_register(m.group(0))
                    if r:
                        out.add(r)
        return out

    def all_constraints(self):
        return {c for b in self.blocks for c in b.constraints}


def _c_function_bodies(text, names):
    """{name: body_text} using tools/tu_edit.py when the file is a TU file."""
    bodies = {}
    try:
        from tools import tu_edit
        tu = tu_edit.parse(text)
        for b in tu.blocks:
            if b.state == 'c' and (b.name in names or b.key in names):
                bodies[b.name if b.name in names else b.key] = tu.block_text(b)
    except Exception:                                     # not a TU file: scan braces
        pass
    missing = [n for n in names if n not in bodies]
    if missing:
        masked = blank_comments(text)
        for name in missing:
            for m in re.finditer(r'(?<![A-Za-z0-9_])' + re.escape(name) + r'\s*\(', masked):
                head = masked.rfind('\n\n', 0, m.start())
                open_brace = masked.find('{', m.end())
                if open_brace < 0:
                    continue
                semi = masked.find(';', m.end())
                if 0 <= semi < open_brace:
                    continue                              # a prototype or a call
                depth, i, n = 0, open_brace, len(masked)
                while i < n:
                    if masked[i] == '{':
                        depth += 1
                    elif masked[i] == '}':
                        depth -= 1
                        if not depth:
                            break
                    i += 1
                bodies[name] = text[max(head, 0):i + 1]
                break
    return bodies


def facts_from_bodies(bodies, source_label='<audited TU>', file_scope_text=None, whole_text=None):
    """Facts for {function name: its C text}, without re-reading a file.

    This is the entry point `tools/tu_audit.py` uses: the audit has already
    parsed the TU and holds each claimed block's exact text, so the lint runs on
    what the audit compiled and never re-resolves a path.
    """
    facts = Facts('c', {}, source_label, whole_text if whole_text is not None else '')
    for name, body in bodies.items():
        blocks = asm_statements(body)
        last_end = max((b.end for b in blocks), default=0)
        facts.functions[name] = dict(
            body=body, blocks=blocks, register_pins=register_pins(body),
            locals=c_locals(body), after_last_block=blank_comments(body)[last_end:],
            calls={m.group(1) for m in re.finditer(
                r'(?<![A-Za-z0-9_])([A-Za-z_][A-Za-z0-9_]*)\s*\(', blank_comments(body))})
    # None means "not known", and the top_level_asm check then does not run.
    facts.file_scope_asm = (None if file_scope_text is None else
                            [ln for b in asm_statements(file_scope_text) for ln in b.lines])
    return facts


def derive_from_c(path, names):
    text = Path(path).read_text(errors='replace')
    bodies = _c_function_bodies(text, names)
    unknown = [n for n in names if n not in bodies]
    if unknown:
        raise LintError('not a C definition in %s: %s' % (path, ', '.join(sorted(unknown))))
    # File-scope asm: statements outside every known function body.
    outside = blank_comments(text)
    for body in bodies.values():
        at = text.find(body)
        if at >= 0:
            outside = outside[:at] + ' ' * len(body) + outside[at + len(body):]
    return facts_from_bodies(bodies, str(path), file_scope_text=outside, whole_text=text)


def check_bodies(contract, bodies, unit=None, root=ROOT, source_label='<audited TU>',
                 file_scope_text=None):
    """Lint an already-parsed contract against already-extracted function texts."""
    facts = facts_from_bodies(bodies, source_label, file_scope_text)
    report = Report('<parsed asm_contract>', source_label, list(bodies), 'c')
    symbols = None
    try:
        symbols = load_symbols(unit, root) if unit else None
    except OSError:
        symbols = None
    return lint(contract, facts, report, symbols)


_ENT = re.compile(r'^\s*\.ent\s+(?P<name>[A-Za-z_$.][A-Za-z0-9_$.]*)', re.M)


def derive_from_asm(path, names):
    """Facts from a standalone `.s` (an `exact_asm`-era contract's subject)."""
    text = Path(path).read_text(errors='replace')
    facts = Facts('asm', {}, str(path), text)
    ents = [(m.group('name'), m.start()) for m in _ENT.finditer(text)]
    for idx, (name, start) in enumerate(ents):
        end = ents[idx + 1][1] if idx + 1 < len(ents) else len(text)
        if names and name not in names:
            continue
        lines = []
        for raw in text[start:end].split('\n'):
            line = raw.split('#', 1)[0].strip()
            if line and not line.startswith('.') and not line.endswith(':'):
                lines.append(line)
        block = Block(start, end, lines, [], [], [], True, False)
        facts.functions[name] = dict(body=text[start:end], blocks=[block], register_pins=[],
                                     locals={}, after_last_block='', calls=set())
    missing = [n for n in names if n not in facts.functions]
    if missing:
        raise LintError('no .ent for %s in %s' % (', '.join(sorted(missing)), path))
    return facts


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

CONTRADICTION, UNDECIDABLE, NOTE = 'contradiction', 'undecidable', 'note'


class Report:
    def __init__(self, contract_path, source_path, functions, kind):
        self.contract = contract_path
        self.source = source_path
        self.functions = sorted(functions)
        self.kind = kind
        self.findings = []
        self.derived = {}

    def add(self, check, severity, detail, **extra):
        self.findings.append(dict(check=check, severity=severity, detail=detail, **extra))

    @property
    def contradictions(self):
        return [f for f in self.findings if f['severity'] == CONTRADICTION]

    @property
    def ok(self):
        return not self.contradictions

    def to_json(self):
        return dict(schema='asm-contract-lint/1', contract=self.contract, source=self.source,
                    source_kind=self.kind, functions=self.functions, ok=self.ok,
                    contradictions=len(self.contradictions), findings=self.findings,
                    derived=self.derived,
                    not_checked=NOT_CHECKED,
                    meaning='ok=true means no contract claim is contradicted by the source; '
                            'it is NOT a statement that the contract is true. The fields and '
                            'questions in `not_checked` remain a human reviewer\'s job.')


NOT_CHECKED = [
    '`purpose` and `privilege`: intent and hardware eligibility are prose a pattern match '
    'cannot decide (is this function really entitled to run `ei`? is the stated operation the '
    'one the original performs?). A reviewer must read them.',
    'whether an alignment precondition is SATISFIED: the linter reports that one exists and '
    'whether the contract states it; the correctness question is the project task '
    'tu-align-vector4-lqc2-audit.',
    'the semantics of `memory`/`state` beyond the mechanical claims checked here: lane masks, '
    'ACC/Q producer-consumer order, aliasing arguments, cross-call persistent VU state.',
    'instruction counts and byte extents quoted in prose ("30 COP2 instructions", "136 bytes").',
    'whether the recovered C is the C the original programmers wrote (docs/style.md).',
]


def lint(contract, facts, report, symbols=None):
    governed = set(report.functions)
    whole = '\n'.join(str(contract.get(f) or '') for f in PROSE_FIELDS)
    document = json.dumps(contract)
    derived_mnemonics = facts.all_mnemonics()
    report.derived = dict(
        blocks_per_function={n: len(f['blocks']) for n, f in sorted(facts.functions.items())},
        mnemonics=sorted(derived_mnemonics),
        constraints=sorted(facts.all_constraints()),
        clobbers=sorted(facts.all_clobbers()),
        declared_registers=sorted(facts.declared_registers()),
        register_pins=sorted({r for f in facts.functions.values() for r, _ in f['register_pins']}),
        has_branch=any(BRANCH.fullmatch(base_mnemonic(t)) for b in facts.blocks for t in b.mnemonics),
        has_call=any(CALL.fullmatch(base_mnemonic(t)) for b in facts.blocks for t in b.mnemonics),
        has_label=any(re.match(r'^[A-Za-z_$.][A-Za-z0-9_$.]*:', ln) for b in facts.blocks
                      for ln in b.lines),
        file_scope_asm=facts.file_scope_asm,
    )

    # -- 1. identity ---------------------------------------------------------
    unnamed_functions = sorted(n for n in governed if n not in document)
    if unnamed_functions:
        report.add('identity.not_named', CONTRADICTION,
                   'the contract never names %s, the function(s) it is attached to; a hash-pinned '
                   'document must quote the code it describes'
                   % ', '.join(unnamed_functions), functions=unnamed_functions)

    purpose = str(contract.get('purpose') or '')
    source_text = '\n'.join(f['body'] for f in facts.functions.values())
    # A `purpose` that describes a function the submission does not deliver, and
    # names none of the ones it does, is a contract written for other code.  Data
    # symbols are excluded: naming the constant a block materialises is normal.
    purpose_names = {w for w in contract_identifiers(dict(purpose=purpose))
                     if (symbols.get(w) if symbols is not None else True)
                     and w not in source_text}
    if purpose_names and not (contract_identifiers(dict(purpose=purpose)) & governed):
        foreign = sorted(n for n in purpose_names if n not in governed)
        if foreign:
            report.add('identity.purpose_describes_another_function', CONTRADICTION,
                       f'`purpose` describes {", ".join(foreign)} and names none of the governed '
                       f'functions ({", ".join(sorted(governed))}): this is a contract for other code',
                       named=foreign)

    prov = contract.get('_provenance') or {}
    listed = [n for n in (prov.get('functions') or []) if n not in governed]
    if listed:
        report.add('identity.provenance_lists_undelivered_functions', CONTRADICTION,
                   f'_provenance.functions lists {", ".join(listed)}, which this submission does '
                   f'not deliver: the contract was not narrowed with the allocation',
                   named=listed)

    for word in sorted(contract_identifiers(contract)):
        if word in governed or word in source_text:
            continue
        if symbols is not None and word in symbols:
            report.add('identity.foreign_symbol',
                       CONTRADICTION if facts.kind == 'c' else UNDECIDABLE,
                       f'the contract names {word}, an original symbol of this unit that the '
                       f'delivered source neither defines nor references (a neighbour left as '
                       f'INCLUDE_ASM/ACCEPTED_ASM scaffolding is not delivered)'
                       + ('' if facts.kind == 'c' else '; on a standalone .s a symbol can still be '
                          'reached through a relocation the text does not spell, so this is '
                          'reported and not decided'),
                       symbol=word)
        elif symbols is None:
            report.add('identity.identifier_not_in_source', UNDECIDABLE,
                       f'the contract names {word}, which the delivered source neither defines '
                       f'nor references; '
                       f'no symbol table was supplied, so whether it is a real symbol is undecided',
                       symbol=word)

    # -- 2. block count ------------------------------------------------------
    counts = set()
    for field in ('control_flow', 'purpose', 'outputs', 'memory', 'state'):
        for clause in clauses(str(contract.get(field) or '')):
            # Parenthesised asides carry their own numbers and their own nouns
            # ("one (xglVectorLength/...) or two (...) statements per function"),
            # so they are dropped before the claim is read.
            flat = re.sub(r'\([^()]*\)', ' ', clause)
            found = [m for m in _COUNT_CLAIM.finditer(flat)]
            if not found:
                continue
            values = set()
            for m in found:
                n = m.group('n').lower()
                values.add(_NUMBER_WORDS.get(n, None) if not n.isdigit() else int(n))
            values.discard(None)
            if values:
                counts.update(values)
    if counts:
        per_function = report.derived['blocks_per_function']
        bad = {n: c for n, c in per_function.items() if c not in counts}
        if bad and facts.kind == 'c':
            report.add('blocks.count_claim_contradicted', CONTRADICTION,
                       'the contract states %s inline asm statement(s); the delivered source has %s'
                       % ('/'.join(str(c) for c in sorted(counts)),
                          ', '.join(f'{n}: {c}' for n, c in sorted(bad.items()))),
                       claimed=sorted(counts), derived=per_function)

    # -- 3. control flow -----------------------------------------------------
    if facts.kind == 'c':
        for term, pattern in HAZARD_TERMS.items():
            present = dict(branch=report.derived['has_branch'], call=report.derived['has_call'],
                           label=report.derived['has_label'],
                           **{'delay slot': report.derived['has_branch'], 'loop': False})[term]
            if present:
                continue
            for clause in asserted_clauses(str(contract.get('control_flow') or '')):
                if pattern.search(clause):
                    report.add('control_flow.claims_construct_no_block_has', CONTRADICTION,
                               f'`control_flow` asserts a {term} that no inline asm statement of '
                               f'the delivered source contains: "{clause[:180]}"',
                               construct=term, clause=clause[:400])
                    break

    # -- 4. operands ---------------------------------------------------------
    if facts.kind == 'c':
        have = facts.all_constraints()
        claimed = set()
        for field in ('inputs', 'outputs', 'clobbers', 'memory', 'state'):
            for clause in asserted_clauses(str(contract.get(field) or ''), allow_out_of_block=True):
                for m in _QUOTED_CONSTRAINT.finditer(clause):
                    token = m.group(1)
                    if token in ('memory', 'cc') or not _CONSTRAINT.fullmatch(token):
                        continue
                    claimed.add(token)
        absent = sorted(c for c in claimed if c not in have)
        if absent:
            report.add('operands.constraint_claimed_absent', CONTRADICTION,
                       'the contract claims operand constraint(s) %s; the delivered statements '
                       'declare %s' % (', '.join(f'"{c}"' for c in absent),
                                       ', '.join(f'"{c}"' for c in sorted(have)) or 'none'),
                       claimed=absent, derived=sorted(have))

    # -- 5. clobbers ---------------------------------------------------------
    if facts.kind == 'c':
        declared = facts.declared_registers()
        in_templates = facts.template_registers()
        claimed = set()
        for clause in asserted_clauses(str(contract.get('clobbers') or '')):
            claimed |= registers_in(clause)
        unfounded = sorted(claimed - declared - in_templates,
                           key=lambda r: (r.startswith('$f'), int(r.lstrip('$f'))))
        if unfounded:
            report.add('clobbers.claims_registers_the_source_never_declares', CONTRADICTION,
                       '`clobbers` names %s; no inline asm statement of the delivered source '
                       'declares or writes %s (declared: %s)'
                       % (', '.join(unfounded), 'them' if len(unfounded) > 1 else 'it',
                          ', '.join(sorted(facts.all_clobbers())) or 'nothing'),
                       claimed=unfounded, declared=sorted(facts.all_clobbers()))
        stated = registers_in(str(contract.get('clobbers') or ''))
        omitted = sorted(r for r in declared if r not in stated)
        if omitted:
            report.add('clobbers.declared_clobber_not_stated', CONTRADICTION,
                       '`clobbers` never mentions %s, which the delivered source declares'
                       % ', '.join(omitted), omitted=omitted)
        if any('memory' in b.clobbers for b in facts.blocks) and 'memory' not in whole:
            report.add('clobbers.memory_clobber_not_stated', NOTE,
                       'the source declares a "memory" clobber the contract never mentions')

    # -- 6. mnemonics --------------------------------------------------------
    claimed_mnemonics = {}
    for field in PROSE_FIELDS:
        for clause in asserted_clauses(str(contract.get(field) or ''), allow_out_of_block=True):
            for m in re.finditer(r'(?<![A-Za-z0-9_.$%])([a-z][a-z0-9]{1,9}(?:\.[a-z]{1,4})?)'
                                 r'(?![A-Za-z0-9_])', clause):
                token = m.group(1)
                if is_hardware_mnemonic(token):
                    claimed_mnemonics.setdefault(base_mnemonic(token), field)
    absent = sorted(t for t in claimed_mnemonics
                    if not any(d == t or d.startswith(t) for d in derived_mnemonics))
    if absent:
        report.add('mnemonics.claimed_absent', CONTRADICTION,
                   'the contract enumerates %s; no delivered statement contains %s (present: %s)'
                   % (', '.join(f'`{t}` ({claimed_mnemonics[t]})' for t in absent),
                      'them' if len(absent) > 1 else 'it',
                      ', '.join(sorted(derived_mnemonics)) or 'nothing'),
                   claimed=absent, derived=sorted(derived_mnemonics))
    unnamed = sorted(d for d in derived_mnemonics
                     if is_hardware_mnemonic(d)
                     and not any(d == t or d.startswith(t) for t in claimed_mnemonics))
    if unnamed:
        report.add('mnemonics.present_but_unnamed', NOTE,
                   'the delivered statements contain %s, which the contract never names; a '
                   'contract is expected to enumerate the hardware it admits'
                   % ', '.join(f'`{t}`' for t in unnamed), present=unnamed)

    # -- 7. alignment preconditions -----------------------------------------
    if facts.kind == 'c':
        align_clauses = [c for c in clauses(whole) if re.search(r'\balign', c, re.I)]
        align_text = ' '.join(align_clauses)
        stated_16 = bool(re.search(r'\b(?:16|32|64|128)[- ]byte[- ]align|align(?:ed|ment)?\b[^.]{0,40}\b'
                                   r'(?:16|32|64|128)\b', align_text, re.I))
        for fname, f in sorted(facts.functions.items()):
            for block in f['blocks']:
                for mnemonic, operand, _reg in quad_accesses(block):
                    if operand is None:
                        continue
                    ordered = block.outputs + block.inputs
                    if operand >= len(ordered):
                        continue
                    expr = ordered[operand][1]
                    names = {m.group(0) for m in re.finditer(r'[A-Za-z_][A-Za-z0-9_]*', expr)}
                    for local in sorted(names & set(f['locals'])):
                        info = f['locals'][local]
                        if info['pointer'] or info['aligned16']:
                            continue
                        if not stated_16:
                            report.add('alignment.precondition_unstated', CONTRADICTION,
                                       f'{fname}: `{mnemonic}` addresses the stack local `{local}` '
                                       f'({info["type"]}), whose C type guarantees 4-byte alignment '
                                       f'while the instruction requires 16; the contract states no '
                                       f'alignment precondition anywhere',
                                       function=fname, local=local, mnemonic=mnemonic)
                        elif not re.search(r'(?<![A-Za-z0-9_])' + re.escape(local) +
                                           r'(?![A-Za-z0-9_])', align_text):
                            report.add('alignment.precondition_unstated_for_local', CONTRADICTION,
                                       f'{fname}: `{mnemonic}` addresses the stack local `{local}` '
                                       f'({info["type"]}), which the contract\'s alignment statement '
                                       f'never names; 16-byte alignment of this object is a '
                                       f'precondition its C type does not make',
                                       function=fname, local=local, mnemonic=mnemonic)
                        report.add('alignment.correctness_not_decided', UNDECIDABLE,
                                   f'{fname}: whether `{local}` is actually placed 16-byte aligned '
                                   f'depends on the compiler\'s frame layout under the TU contract; '
                                   f'this linter does not decide it (project task '
                                   f'tu-align-vector4-lqc2-audit)',
                                   function=fname, local=local)

    # -- 8. a local claimed dead that the C uses ----------------------------
    if facts.kind == 'c':
        dead = re.compile(r'\bnever (?:read|used)\b|\bnot read back\b|\bdead after\b|'
                          r'\bno further use\b|\bis never read back\b', re.I)
        # "the w lane ... never read afterward" is a claim about one lane, not
        # about the object; per-lane liveness is not tracked here.
        partial = re.compile(r'\blanes?\b|\bwords?\b|\bcomponents?\b|\bfields?\b|\bmembers?\b|'
                             r'\.[xyzw]\b|\[[0-9]\]', re.I)
        for field in ('memory', 'outputs', 'state'):
            for clause in clauses(str(contract.get(field) or '')):
                if not dead.search(clause) or partial.search(clause):
                    continue
                for fname, f in sorted(facts.functions.items()):
                    for local in sorted(f['locals']):
                        if not re.search(r'(?<![A-Za-z0-9_])' + re.escape(local) +
                                         r'(?![A-Za-z0-9_])', clause):
                            continue
                        if re.search(r'(?<![A-Za-z0-9_])' + re.escape(local) +
                                     r'(?![A-Za-z0-9_])', f['after_last_block']):
                            report.add('memory.local_claimed_dead_is_used', CONTRADICTION,
                                       f'`{field}` says `{local}` is never read back, but the C '
                                       f'after the last inline statement of {fname} uses it: '
                                       f'"{clause[:180]}"',
                                       function=fname, local=local, clause=clause[:400])

    # -- 9. register pins ----------------------------------------------------
    for fname, f in sorted(facts.functions.items()):
        for reg, _off in f['register_pins']:
            canonical = normalise_register(reg) or reg
            if canonical not in registers_in(whole):
                report.add('state.register_pin_unstated', CONTRADICTION,
                           f'{fname} pins a local to {reg} with a `register ... asm()` declaration; '
                           f'the contract never mentions that register',
                           function=fname, register=reg)

    # -- 10. top_level_asm ---------------------------------------------------
    listed_top = contract.get('top_level_asm') or []
    if listed_top and facts.kind == 'c' and facts.file_scope_asm is None:
        report.add('top_level_asm.not_checked', UNDECIDABLE,
                   'the caller did not supply the TU\'s file-scope text, so the `top_level_asm` '
                   'entries were not compared with the file-scope asm() statements')
    elif listed_top and facts.kind == 'c':
        stray = [ln for ln in listed_top if ln not in facts.file_scope_asm]
        if stray:
            report.add('top_level_asm.not_file_scope', CONTRADICTION,
                       '`top_level_asm` is the list of file-scope asm() lines the TU emits; %d of '
                       'its entries are not file-scope asm in the delivered source (%s)'
                       % (len(stray), '; '.join(stray[:3])), stray=stray[:10])

    # -- 11. what is not mechanically checkable ------------------------------
    for field in UNCHECKABLE_FIELDS:
        if str(contract.get(field) or '').strip():
            report.add(f'{field}.not_mechanically_checked', UNDECIDABLE,
                       f'`{field}` is prose about intent and eligibility; nothing here decides '
                       f'whether it is true. A reviewer must read it against the original.')
    if facts.kind == 'asm':
        report.add('operands.not_applicable', UNDECIDABLE,
                   'the subject is standalone assembly: there is no GNU operand/clobber contract '
                   'to compare, so the operand, clobber and alignment checks did not run')
    return report


# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------

def load_symbols(unit, root=ROOT):
    """{name: is_function} from config/symbols/<unit>.txt, or None when absent."""
    path = Path(root) / 'config/symbols' / f'{unit}.txt'
    if not path.is_file():
        return None
    out = {}
    for m in re.finditer(r'^([A-Za-z_][A-Za-z0-9_$.]*)\s*=[^\n]*', path.read_text(errors='replace'),
                         re.M):
        out[m.group(1)] = 'type:func' in m.group(0)
    return out


def check(contract_path, source_path, functions, unit=None, asm_source=False, root=ROOT):
    """Lint one contract against one source. Returns a Report."""
    contract = json.loads(Path(contract_path).read_text())
    missing = [k for k in PROSE_FIELDS
               if not (isinstance(contract.get(k), str) and contract[k].strip())]
    if missing:
        raise LintError('asm contract lacks %s' % sorted(missing))
    facts = (derive_from_asm(source_path, functions) if asm_source
             else derive_from_c(source_path, functions))
    report = Report(str(contract_path), str(source_path), functions or list(facts.functions),
                    facts.kind)
    symbols = load_symbols(unit, root) if unit else None
    return lint(contract, facts, report, symbols)


def render(report, stream=sys.stdout):
    order = {CONTRADICTION: 0, UNDECIDABLE: 1, NOTE: 2}
    print('contract: %s' % report.contract, file=stream)
    print('source:   %s (%s)' % (report.source, report.kind), file=stream)
    print('function: %s' % ', '.join(report.functions), file=stream)
    for f in sorted(report.findings, key=lambda x: order[x['severity']]):
        mark = {CONTRADICTION: 'FAIL', UNDECIDABLE: 'UNDECIDED', NOTE: 'note'}[f['severity']]
        print('  [%s] %s: %s' % (mark, f['check'], f['detail']), file=stream)
    print('  => %d contradiction(s). %s' % (
        len(report.contradictions),
        'No claim is contradicted by the source; this is NOT a statement that the contract is true.'
        if report.ok else 'The contract is false about the code it is attached to.'), file=stream)


def record_source_path(data, root):
    """The file an accepted record's contract should actually be read against.

    `source.path` is not always the live file. A record re-cut by
    `tools/tu_edit.py split-asm` deliberately keeps `source.path` at the
    pre-migration path it was accepted at -- that is a fact about the accepted
    work, and the domain oracle of `tools/tu_build_manifest.py` reads it -- and
    names the narrowed file in `source.staged_path` (config/units/
    main-mmath-matrix-ops.json: `path` is the retired thirteen-function
    mmath_matrix_ops.s, `staged_path` the one-function MMathSpecialVectorSub.s
    that is tracked today). Reading `path` there reports the live record as
    "record source missing" and never lints its contract at all.

    `staged_path` is only preferred when it resolves to a file in this tree:
    a record accepted directly from a worker attempt can carry an attempt-local
    `staged_path` (config/units/math-main-nml-packet-trans-microcode.json has
    the bare `candidate.c`), which is not a repository path and must not
    displace `source.path`.
    """
    source = data.get('source') or {}
    staged = source.get('staged_path')
    if staged and (Path(root) / staged).is_file():
        return staged
    return source.get('path')


def sweep(root=ROOT):
    """Lint every published contract against its published source."""
    reports, errors = [], []
    classes_path = Path(root) / 'config/tu/source-classes.json'
    build_path = Path(root) / 'config/tu-build.json'
    tu_paths = {}
    if build_path.is_file():
        for rec in json.loads(build_path.read_text()).get('tus') or []:
            if isinstance(rec, dict) and rec.get('id') and rec.get('path'):
                tu_paths[rec['id']] = rec['path']
    if classes_path.is_file():
        data = json.loads(classes_path.read_text())
        for tu_id, entry in sorted((data.get('tus') or {}).items()):
            unit = tu_id.split('/')[0]
            by_contract = {}
            for name, spec in sorted((entry.get('functions') or {}).items()):
                contract = (spec.get('asm_contract') or {}).get('path')
                if contract:
                    by_contract.setdefault(contract, []).append(name)
            tu_contract = (entry.get('asm_contract') or {}).get('path')
            if tu_contract:
                by_contract.setdefault(tu_contract, [])
            for contract, names in sorted(by_contract.items()):
                src = tu_paths.get(tu_id)
                if not src:
                    errors.append(dict(contract=contract, tu=tu_id,
                                       error='config/tu-build.json has no path for this TU'))
                    continue
                try:
                    reports.append(check(Path(root) / contract, Path(root) / src, names,
                                         unit=unit, root=root))
                except (LintError, OSError, ValueError) as e:
                    errors.append(dict(contract=contract, tu=tu_id, source=src, error=str(e)))
    for record in sorted((Path(root) / 'config/units').glob('*.json')):
        try:
            data = json.loads(record.read_text())
        except ValueError:
            continue
        contract = data.get('asm_contract')
        if not isinstance(contract, dict):
            continue
        src = record_source_path(data, root)
        unit = data.get('unit')
        names = [f.get('name') for f in (data.get('functions') or []) if isinstance(f, dict)]
        names = [n for n in names if n]
        if not src or not (Path(root) / src).is_file():
            errors.append(dict(contract=str(record), error='record source missing: %s' % src))
            continue
        is_asm = str(src).endswith('.s')
        try:
            tmp = Path(root) / '.work/attempts/asm-contract-lint/.record-contract.json'
            tmp.parent.mkdir(parents=True, exist_ok=True)
            tmp.write_text(json.dumps(contract))
            reports.append(check(tmp, Path(root) / src, names, unit=unit, asm_source=is_asm,
                                 root=root))
            reports[-1].contract = '%s#asm_contract' % record.relative_to(root)
        except (LintError, OSError, ValueError) as e:
            errors.append(dict(contract='%s#asm_contract' % record.relative_to(root),
                               source=src, error=str(e)))
    return reports, errors


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--contract', help='the asm_contract JSON document')
    p.add_argument('--source', help='the delivered C source (a TU file or any C file)')
    p.add_argument('--asm-source', help='a standalone .s, for an exact_asm-era contract')
    p.add_argument('--function', action='append', default=[],
                   help='a function the contract governs (repeatable)')
    p.add_argument('--unit', help='main/ov01/... : enables the original-symbol checks')
    p.add_argument('--root', default=str(ROOT), help='repository root for config lookups')
    p.add_argument('--sweep', action='store_true',
                   help='lint every published contract against its published source')
    p.add_argument('--json-out', help='write the machine-readable report here')
    args = p.parse_args(argv)

    try:
        if args.sweep:
            reports, errors = sweep(Path(args.root))
            for r in reports:
                render(r)
                print('', file=sys.stdout)
            for e in errors:
                print('  [ERROR] %s: %s' % (e.get('contract'), e['error']))
            payload = dict(schema='asm-contract-lint-sweep/1',
                           reports=[r.to_json() for r in reports], errors=errors,
                           contradictions=sum(len(r.contradictions) for r in reports))
            failed = payload['contradictions']
        else:
            if not (args.contract and (args.source or args.asm_source)):
                p.error('--contract and --source (or --asm-source) are required without --sweep')
            report = check(args.contract, args.asm_source or args.source, args.function,
                           unit=args.unit, asm_source=bool(args.asm_source), root=Path(args.root))
            render(report)
            payload = report.to_json()
            failed = len(report.contradictions)
    except LintError as e:
        print('asm_contract_lint: %s' % e, file=sys.stderr)
        return 2
    if args.json_out:
        Path(args.json_out).write_text(json.dumps(payload, indent=1, sort_keys=True) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())

"""Confined GNU EE native TI typedef admission; original text is compiled intact."""
import re
from tools.match import ordinary_c, require, _hardware_line_continuations_ok

PROFILE = 'ee-gcc2_96-native-ti-copy-v1'
CONTRACT = 'gnu-ee-unsigned-ti-storage-copy-v1'
# The unchanged 2.96 engine's matrix. G0 was the calibration scope of v1; G8 was
# enabled 2026-09-11 (infra-core-cc-20260911): the TI admission guard is
# flag-independent and the engine still requires the original _gp witness.
FLAGS = [['-O2', '-G0'], ['-O2', '-G8']]
ACCELERATION_ERRORS = {
    'm2c': 'native TI m2c scaffold is not independently validated; scalar EE support is not equivalent',
    'permuter': 'native TI decomp-permuter is not independently validated; scalar debug scoring is not equivalent',
}
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)
DECL = re.compile(r'typedef\s+unsigned\s+int\s+([A-Za-z][A-Za-z_0-9]*)'
                  r'\s+__attribute__\s*\(\s*\(\s*mode\s*\(\s*TI\s*\)\s*\)\s*\)\s*;')


def source(text, staged=None, headers=(), *, hardware_builtins=False):
    """Only whole-line, file-scope unsigned integer typedefs gain admission.

    This is a source-admission check, not a C semantic proof. Per-function review
    must justify aligned typed objects and storage/copy use; general wide integer
    arithmetic, alias-punning, opcode arrays and fake padding gain no eligibility.
    """
    require(text.isascii() and '\x00' not in text and '??' not in text and '%:' not in text
            and _hardware_line_continuations_ok(text), 'native TI encoding/escape rejected')
    # Admission follows C's line-splicing phase. The compiler still receives
    # the original source intact; standard literal/comment escapes are not
    # executable tokens, and continued defines must retain the usual guards.
    text = re.sub(r'\\\r?\n', '', text)
    # Reject alternative punctuators before hiding comments/literals. cpp0 can
    # preserve digraphs, so counting only literal braces would miss C block scope.
    require(not any(p in text for p in ('<%', '%>', '<:', ':>')),
            'native TI alternative C punctuators rejected')
    lexical = TOKENS.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text)
    require('\\' not in lexical, 'native TI executable escape rejected')
    require(not re.search(r'\b(?:__extension__|__int128|typeof|__typeof__)\b', lexical),
            'uncalibrated native TI builtin or alternate type syntax')
    if hardware_builtins:
        # Only the composed inline-ASM routes opt in. Reuse their closed,
        # calibrated direct-call check; no tokens are hidden from admission.
        from tools.match import _hardware_c_builtin_calls
        _hardware_c_builtin_calls(lexical, 'native TI composed hardware C')
    else:
        require(not re.search(r'\b__builtin_\w+\b', lexical),
                'uncalibrated native TI builtin or alternate type syntax')
    approved, braces, parens, start = [], 0, 0, 0
    for line in lexical.splitlines(keepends=True):
        original = text[start:start + len(line)]
        if line.lstrip().startswith('#'):
            require(not re.search(r'[{}]', line),
                    'native TI preprocessing cannot supply structural braces')
            require(not re.search(r'\b(?:typedef|mode|TI|__attribute__)\b', line)
                    and not re.search(r'^\s*#\s*(?:define|undef)\s+(?:int|signed|unsigned)\b', line),
                    'native TI type syntax cannot be supplied/redefined by preprocessing')
        else:
            declaration = DECL.fullmatch(line.strip())
            if declaration:
                require(braces == parens == 0, 'native TI typedef must be at file scope')
                require(not re.search(r'["\']', original), 'literal inside native TI typedef')
                approved.append((start, start + len(line), declaration[1]))
            braces += line.count('{') - line.count('}')
            parens += line.count('(') - line.count(')')
        start += len(line)
    remainder = text
    for begin, end, name in reversed(approved):
        remainder = remainder[:begin] + 'typedef unsigned int ' + name + ';\n' + remainder[end:]
    ordinary_c(remainder, staged, headers)
    return [name for _, _, name in approved]


def hardware_source(text, staged=None, headers=()):
    """Native TI remainder of a calibrated inline-ASM route only.

    The ordinary native-TI route, TU free text and headers keep refusing every
    builtin. This composition permits exactly the already-calibrated direct
    __builtin_fabsf/__builtin_sqrtf calls and preserves all native-type guards.
    """
    return source(text, staged, headers, hardware_builtins=True)

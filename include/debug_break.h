#ifndef DEBUG_BREAK_H
#define DEBUG_BREAK_H

/*
 * The original assert macros end in a bare debugger trap (`break 0`) that
 * falls through when the debugger resumes. cc1 2.96 has no trap builtin, so
 * the trap is this one inline asm statement (`ee-assert-break`,
 * docs/ee-hand-asm.md); a function that uses it is declared with
 * `--function-class NAME=ee-assert-break` and reported exact_c_with_asm.
 */
#define DEBUG_BREAK() __asm__ __volatile__("break")

#endif

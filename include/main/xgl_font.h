#ifndef INCLUDE_MAIN_XGL_FONT_H
#define INCLUDE_MAIN_XGL_FONT_H

extern void xglFontPrint(int x, int y, int color, const char *text);

/* The accepted io-domain prototype (include/xeno/io/functions.h, the same
 * spelling as src/io/main-00256190/HddTest.c). The io headers cannot be
 * included next to the core ones (conflicting XglClock/PadPrefix views), so
 * it is repeated here. It must be the void prototype: an implicit int
 * declaration makes the call set $v0, and local-alloc's false-dependency
 * avoidance then keeps the owner pointer out of $v0 (funcObserver:
 * attempt-4d6f277fff4a form 17, implicit, vs attempt-5684d45333e0 form 01). */
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

unsigned int xglFontAscii2Euc(signed char character, signed char **text);

#endif /* INCLUDE_MAIN_XGL_FONT_H */

#include "common.h"

typedef char *va_list;
#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)
#define va_end(ap) ((void)0)

extern int printf(const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list args);
extern const unsigned char D_004DBB78[];

void tracePrint(const char *format, ...) {
    va_list args;
    char buffer[0x100];

    va_start(args, format);
    vsprintf(buffer, format, args);
    printf(D_004DBB78, buffer);
    va_end(args);
}

/*
 * These exported debug entry points contain no instructions beyond their
 * return sequence.  Their callers are not present in the bounded resident
 * call search, so the historical argument lists remain unresolved; the
 * unspecified C parameter lists preserve that uncertainty without inventing
 * a prototype.  The original symbol spellings are retained.
 */
void sefPrintVector()
{
}

void sefPrintMatrix()
{
}

extern const unsigned char D_004DBB78[8];

extern const char D_004DBB88[8];

extern const char D_004DBB90[8];

extern const char D_004DBB98[8];

extern const char D_004DBBA0[8];

extern const char D_004DBBA8[8];

extern const char D_004DBBB0[8];

extern const char D_004DBBC0[8];

extern const char D_004DBBC8[8];

extern const char D_004DBBD0[8];

extern const char D_004DBBD8[8];

extern const char D_004DBBE0[8];

extern const char D_004DBBE8[8];

extern const char D_004DBBF0[8];

extern const char D_004DBBF8[8];

extern const char D_004DBC00[8];

extern const char D_004DBC08[8];

extern const char D_004DBC10[8];

extern const char D_004DBC18[8];

extern const char D_004DBC20[8];

extern const char D_004DBC28[8];

extern const char D_004DBC30[8];

extern const char D_004DBC38[8];

extern const char D_004DBC40[8];

extern const char D_004DBC48[8];

extern const char D_004DBC50[8];

extern const char D_004DBC58[8];

extern const char D_004DBC60[8];

extern const char D_004DBC68[8];

extern const char D_004DBC70[8];

extern const char D_004DBC78[8];

extern const char D_004DBC80[8];

extern const char D_004DBC88[8];

extern const char D_004DBC90[8];

extern const char D_004DBC98[8];

extern const char D_004DBCA0[8];

extern const char D_004DBCA8[8];

extern const char D_004DBCB0[8];

extern const char D_004DBCB8[8];

extern const char D_004DBCC0[8];

extern const char D_004DBCC8[8];

extern const char D_004DBCD0[8];

extern const char D_004DBCD8[8];

extern const char D_004DBCE0[8];

extern const char D_004DBCE8[8];

extern const char D_004DBCF0[8];

extern const char D_004DBCF8[8];

extern const char D_004DBD00[8];

extern const char D_004DBD08[8];

extern const char D_004DBD10[8];

extern const char D_004DBD18[8];

extern const char D_004DBD20[8];

extern const char D_004DBD28[8];

extern const char D_004DBD30[8];

extern const char D_004DBD38[8];

extern const char D_004DBD40[8];

extern const char D_004DBD48[8];

extern const char D_004DBD50[8];

extern const char D_004DBD58[8];

extern const char D_004DBD60[8];

extern const char D_004DBD68[8];

extern const char D_004DBD70[8];

extern const char D_004DBD78[8];

extern const char D_004DBD80[8];

extern const char D_004DBD88[8];

extern const char D_004DBD90[8];

extern const char D_004DBD98[8];

extern const char D_004DBDA0[8];

extern const char D_004DBDA8[8];

extern const char D_004DBDB0[8];

extern const char D_004DBDB8[8];

extern const char D_004DBDC0[8];

extern const char D_004DBDC8[8];

extern const char D_004DBDD0[8];

extern const char D_004DBDD8[8];

extern const char D_004DBDE0[8];

extern const char D_004DBDE8[8];

extern const char D_004DBDF0[8];

extern const char D_004DBDF8[8];

extern const char D_004DBE00[8];

extern const char D_004DBE08[8];

extern const char D_004DBE10[8];

extern const char D_004DBE18[8];

extern const char D_004DBE20[8];

extern const char D_004DBE28[8];

extern const char D_004DBE30[8];

extern const char D_004DBE38[8];

extern const char D_004DBE40[8];

extern const char D_004DBE48[8];

extern const char D_004DBE50[8];

extern const char D_004DBE58[8];

extern const char D_004DBE60[8];

extern const char D_004DBE68[8];

extern const char D_004DBE70[8];

extern const char D_004DBE78[8];

extern const char D_004DBE80[8];

extern const char D_004DBE88[8];

extern const char D_004DBE90[8];

extern const char D_004DBE98[8];

extern const char D_004DBEA0[8];

extern const char D_004DBEA8[8];

extern const char D_004DBEB0[8];

extern const char D_004DBEB8[8];

extern const char D_004DBEC0[8];

extern const char D_004DBEC8[8];

extern const char D_004DBED0[8];

extern const char D_004DBED8[8];

extern const char D_004DBEE0[8];

extern const char D_004DBEE8[8];

extern const char D_004DBEF0[8];

extern const char D_004DBEF8[8];

extern const char D_004DBF00[8];

extern const char D_004DBF08[8];

extern const char D_004DBF10[8];

extern const char D_004DBF18[8];

extern const char D_004DBF20[8];

extern const char D_004DBF28[8];

extern const char D_004DBF30[8];

extern const char D_004DBF38[8];

extern const char D_004DBF40[8];

extern const char D_004DBF48[8];

extern const char D_004DBF50[8];

extern const char D_004DBF58[8];

extern const char D_004DBF60[8];

extern const char D_004DBF68[8];

extern const char D_004DBF70[8];

extern const char D_004DBF78[8];

extern const char D_004DBF80[8];

extern const char D_004DBF88[8];

extern const char D_004DBF90[8];

extern const char D_004DBF98[8];

extern const char D_004DBFA0[8];

extern const char D_004DBFA8[8];

extern const char D_004DBFB0[8];

extern const char D_004DBFB8[8];

extern const char D_004DBFC0[8];

extern const char D_004DBFC8[8];



extern int vmDebugMask;



extern const char D_004DBBB8[4];


const unsigned char D_004DBB78[8] = "%s\n";

int vmDebugMask = 0;

const char D_004DBB88[8] = "jsr_w";

const char D_004DBB90[8] = "goto_w";

const char D_004DBB98[8] = "ifnull";

const char D_004DBBA0[8] = "wide";

const char D_004DBBA8[8] = "athrow";

const char D_004DBBB0[8] = "new";

const char D_004DBBB8[4] = "\?\?\?";

const char D_004DBBC0[8] = "return";

const char D_004DBBC8[8] = "areturn";

const char D_004DBBD0[8] = "dreturn";

const char D_004DBBD8[8] = "freturn";

const char D_004DBBE0[8] = "lreturn";

const char D_004DBBE8[8] = "ireturn";

const char D_004DBBF0[8] = "ret";

const char D_004DBBF8[8] = "jsr";

const char D_004DBC00[8] = "goto";

const char D_004DBC08[8] = "ifle";

const char D_004DBC10[8] = "ifgt";

const char D_004DBC18[8] = "ifge";

const char D_004DBC20[8] = "iflt";

const char D_004DBC28[8] = "ifne";

const char D_004DBC30[8] = "ifeq";

const char D_004DBC38[8] = "dcmpg";

const char D_004DBC40[8] = "dcmpl";

const char D_004DBC48[8] = "fcmpg";

const char D_004DBC50[8] = "fcmpl";

const char D_004DBC58[8] = "lcmp";

const char D_004DBC60[8] = "d2f";

const char D_004DBC68[8] = "d2l";

const char D_004DBC70[8] = "d2i";

const char D_004DBC78[8] = "f2d";

const char D_004DBC80[8] = "f2l";

const char D_004DBC88[8] = "f2i";

const char D_004DBC90[8] = "l2d";

const char D_004DBC98[8] = "l2f";

const char D_004DBCA0[8] = "l2i";

const char D_004DBCA8[8] = "i2d";

const char D_004DBCB0[8] = "i2f";

const char D_004DBCB8[8] = "i2l";

const char D_004DBCC0[8] = "iinc";

const char D_004DBCC8[8] = "lxor";

const char D_004DBCD0[8] = "ixor";

const char D_004DBCD8[8] = "lor";

const char D_004DBCE0[8] = "ior";

const char D_004DBCE8[8] = "land";

const char D_004DBCF0[8] = "iand";

const char D_004DBCF8[8] = "lushr";

const char D_004DBD00[8] = "iushr";

const char D_004DBD08[8] = "lshr";

const char D_004DBD10[8] = "ishr";

const char D_004DBD18[8] = "lshl";

const char D_004DBD20[8] = "ishl";

const char D_004DBD28[8] = "dneg";

const char D_004DBD30[8] = "fneg";

const char D_004DBD38[8] = "lneg";

const char D_004DBD40[8] = "ineg";

const char D_004DBD48[8] = "drem";

const char D_004DBD50[8] = "frem";

const char D_004DBD58[8] = "lrem";

const char D_004DBD60[8] = "irem";

const char D_004DBD68[8] = "ddiv";

const char D_004DBD70[8] = "fdiv";

const char D_004DBD78[8] = "ldiv";

const char D_004DBD80[8] = "idiv";

const char D_004DBD88[8] = "dmul";

const char D_004DBD90[8] = "fmul";

const char D_004DBD98[8] = "lmul";

const char D_004DBDA0[8] = "imul";

const char D_004DBDA8[8] = "dsub";

const char D_004DBDB0[8] = "fsub";

const char D_004DBDB8[8] = "lsub";

const char D_004DBDC0[8] = "isub";

const char D_004DBDC8[8] = "dadd";

const char D_004DBDD0[8] = "fadd";

const char D_004DBDD8[8] = "ladd";

const char D_004DBDE0[8] = "iadd";

const char D_004DBDE8[8] = "swap";

const char D_004DBDF0[8] = "dup2_x2";

const char D_004DBDF8[8] = "dup2_x1";

const char D_004DBE00[8] = "dup2";

const char D_004DBE08[8] = "dup_x2";

const char D_004DBE10[8] = "dup_x1";

const char D_004DBE18[8] = "dup";

const char D_004DBE20[8] = "pop2";

const char D_004DBE28[8] = "pop";

const char D_004DBE30[8] = "sastore";

const char D_004DBE38[8] = "castore";

const char D_004DBE40[8] = "bastore";

const char D_004DBE48[8] = "aastore";

const char D_004DBE50[8] = "dastore";

const char D_004DBE58[8] = "fastore";

const char D_004DBE60[8] = "lastore";

const char D_004DBE68[8] = "iastore";

const char D_004DBE70[8] = "astore";

const char D_004DBE78[8] = "dstore";

const char D_004DBE80[8] = "fstore";

const char D_004DBE88[8] = "lstore";

const char D_004DBE90[8] = "istore";

const char D_004DBE98[8] = "saload";

const char D_004DBEA0[8] = "caload";

const char D_004DBEA8[8] = "baload";

const char D_004DBEB0[8] = "aaload";

const char D_004DBEB8[8] = "daload";

const char D_004DBEC0[8] = "faload";

const char D_004DBEC8[8] = "laload";

const char D_004DBED0[8] = "iaload";

const char D_004DBED8[8] = "aload_3";

const char D_004DBEE0[8] = "aload_2";

const char D_004DBEE8[8] = "aload_1";

const char D_004DBEF0[8] = "aload_0";

const char D_004DBEF8[8] = "dload_3";

const char D_004DBF00[8] = "dload_2";

const char D_004DBF08[8] = "dload_1";

const char D_004DBF10[8] = "dload_0";

const char D_004DBF18[8] = "fload_3";

const char D_004DBF20[8] = "fload_2";

const char D_004DBF28[8] = "fload_1";

const char D_004DBF30[8] = "fload_0";

const char D_004DBF38[8] = "lload_3";

const char D_004DBF40[8] = "lload_2";

const char D_004DBF48[8] = "lload_1";

const char D_004DBF50[8] = "lload_0";

const char D_004DBF58[8] = "iload_3";

const char D_004DBF60[8] = "iload_2";

const char D_004DBF68[8] = "iload_1";

const char D_004DBF70[8] = "iload_0";

const char D_004DBF78[8] = "aload";

const char D_004DBF80[8] = "dload";

const char D_004DBF88[8] = "fload";

const char D_004DBF90[8] = "lload";

const char D_004DBF98[8] = "iload";

const char D_004DBFA0[8] = "ldc2w";

const char D_004DBFA8[8] = "ldc2";

const char D_004DBFB0[8] = "ldc1";

const char D_004DBFB8[8] = "sipush";

const char D_004DBFC0[8] = "bipush";

const char D_004DBFC8[8] = "nop";

#ifndef INCLUDE_OV01_UNIT_CMD_H
#define INCLUDE_OV01_UNIT_CMD_H

#include "ov01/calc.h"

struct CmdEntry {
    int selector;      /* +0x00 */
    int argument;      /* +0x04 */
    int extra;         /* +0x08 */
    int itmId;         /* +0x0C: unitCmdTgtSet copies it into the TgtCmd
                         * (src/ov01/calc.c) itmId it builds for
                         * tgtFindFirst (lw $3, 0xC($7); sw $3, 0x8($29),
                         * unit_cmd/unitCmdTgtSet.s 0x00a03434/0x00a0343c). */
    int mode;          /* +0x10: unitCmdTgtSet's own TgtCmd.mode, the same
                         * offset tgtFindChk branches on for mode 3
                         * (lw $2, 0x10($7); sw $2, 0xC($29),
                         * unit_cmd/unitCmdTgtSet.s 0x00a03444/0x00a0344c). */
};

#endif /* INCLUDE_OV01_UNIT_CMD_H */

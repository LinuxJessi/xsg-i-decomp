#ifndef INCLUDE_OV01_UNIT_CMD_H
#define INCLUDE_OV01_UNIT_CMD_H

#include "ov01/calc.h"

struct CmdEntry {
    int selector;      /* +0x00 */
    int argument;      /* +0x04 */
    int extra;         /* +0x08 */
    unsigned char unmodeled_0c[8]; /* +0x0C: unwritten by any claimed call */
};

#endif /* INCLUDE_OV01_UNIT_CMD_H */

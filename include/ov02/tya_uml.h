#ifndef INCLUDE_OV02_TYA_UML_H
#define INCLUDE_OV02_TYA_UML_H

#include "shared.h"

/*
 * One 0x10-byte entry of the eight-slot texture table tyaUmlDispInit2
 * clears and texture_trans transfers through sceVif1PkRef: only the two
 * dimension fields, the environment pointer and the transfer count are
 * evidenced, so the rest of the record stays unmodeled.
 */
typedef struct TyaUmlImage {
    u8 unmodeled_00[4];
    u16 width;          /* 0x04: masked to an even value before use */
    s16 height;         /* 0x06 */
    void *environment;  /* 0x08: cleared by tyaUmlDispInit2 */
    int count;           /* 0x0C: sceVif1PkRef's transfer count */
} TyaUmlImage;

extern TyaUmlImage image[8];

#endif /* INCLUDE_OV02_TYA_UML_H */

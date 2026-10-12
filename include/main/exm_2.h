#ifndef INCLUDE_MAIN_EXM_2_H
#define INCLUDE_MAIN_EXM_2_H

#include "shared.h"

typedef struct MapUnitDrawView {
    unsigned int flags; /* +0x00 */
    void (*update)(void *unit); /* +0x04 */
    unsigned char unmodeled_08[4];
    void (*type_update)(void *unit); /* +0x0c */
    unsigned char unmodeled_10[0x94];
    short serial; /* +0xa4 */
    unsigned char unmodeled_a6[0x2e];
    void *resource_model; /* +0xd4 */
    unsigned char unmodeled_d8[8];
    void *model; /* +0xe0 */
    int resource_status; /* +0xe4 */
    unsigned char unmodeled_e8[0x158];
    void *model_state; /* +0x240 */
    unsigned char unmodeled_244[0xbc];
} MapUnitDrawView;

typedef struct MapUnitTransformView {
    unsigned int flags; /* +0x00 */
    unsigned char unmodeled_04[0x0c];
    Vector4 position; /* +0x10 */
    Vector4 rotation; /* +0x20 */
    Vector4 scale; /* +0x30 */
    Matrix4 matrix; /* +0x40 */
    unsigned char unmodeled_80[0x20];
    unsigned char serial; /* +0xa0 */
    unsigned char signal; /* +0xa1 */
    unsigned char unmodeled_a2[0x1a2 - 0xa2];
    unsigned short drill_reset_value; /* +0x1a2 */
    unsigned char unmodeled_1a4[0x15c];
} MapUnitTransformView;

typedef struct MapUnitDoorView {
    unsigned char unmodeled_00[0xc0];
    Vector4 position; /* +0xc0 */
    unsigned char unmodeled_d0[0x230];
} MapUnitDoorView;

#endif /* INCLUDE_MAIN_EXM_2_H */

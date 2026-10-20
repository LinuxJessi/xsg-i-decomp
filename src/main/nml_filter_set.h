#ifndef SRC_MAIN_NML_FILTER_SET_H
#define SRC_MAIN_NML_FILTER_SET_H

#include "common.h"
#include "shared.h"

/* The GS slot representation used by nml_packet_add's packet writers.
 * A register slot is stored as a doubleword and read as a low word. */

/* Owner-header proposal for main/tu106's existing LayoutStore.
 * nmlModelSetTransparency witnesses the float member at +0x2c0.
 * The genuine mixed float/int slots and 800-byte extent are retained. */
#include "main/nml_packet_add.h"

typedef union {
    float f;
    int i;
} LayoutSlot;

/* nmlFilterStealthMake allocates 31 sixteen-byte records. Its draw consumer
 * witnesses fourteen XYZ records at +0x30 and fourteen UV records at +0x110.
 * Unused vector components retain an explicitly partial representation. */
struct NmlStealthXYZ {
    int x;
    int y;
    int z;
    unsigned char unmodeled_0c[4];
};

struct NmlStealthUV {
    int u;
    int v;
    int clipFlags;
    unsigned char unmodeled_0c[4];
};

struct NmlStealthGeometry {
    Vector4 endpoints[2];
    float sortClass;
    unsigned char unmodeled_24[0x0c];
    struct NmlStealthXYZ xyz[14];
    struct NmlStealthUV uv[14];
};

typedef union {
    LayoutSlot slots[200];
    struct {
        unsigned char unmodeled_000[0x1e0];
        float filterAlpha;
        float filterAmplitude;
        unsigned char unmodeled_1e8[0x234 - 0x1e8];
        u32 filterFlags;
        int pixelAlpha;
        unsigned char unmodeled_23c[4];
        const char *texture;
        int texfunc;
        u64 alpha;
        u32 render_status;
        unsigned char unmodeled_254[0x26c - 0x254];
        float textureFillWeight;
        int textureMapAlpha;
        unsigned char unmodeled_274[0x280 - 0x274];
        struct NmlStealthGeometry *stealthParts[10];
        unsigned char unmodeled_2a8[0x2c0 - 0x2a8];
        float transparency;
        unsigned char unmodeled_2c4[4];
        int stealthPartCount;
        unsigned char unmodeled_2cc[0x2e0 - 0x2cc];
        unsigned short mapShadowParts[8];
    } fields;
} LayoutStore;

/* Additive partial owner proposals for the previously untreated producer.
 * Field names describe observed use, not recovered original declarations. */


/* The allocator's 31 sixteen-byte records, including actual endpoint vectors. */

/* Relative offsets address two arrays of 64-byte joint matrices. */

/* Only fields this producer accesses are completed; the tail stays unknown. */

#endif

#include "common.h"

#include "shared.h"

#include "nml_filter_set.h"

static void nmlModelSetFilterGunosys(void *model, int flags, int count);

static void nmlModelSetFilterStealth(int *model, int flags);

struct NmlFilterRenderView {
    u8 unmodeled_00[2];
    short pixelStorageMode;
    short frameWidth;
    short frameHeight;
    u8 unmodeled_08[0x0a];
    u16 opaqueTexturePage;
    u16 framebufferPage;
    u8 unmodeled_16[0x0a];
    u16 displayBufferBase;
};

extern struct NmlFilterRenderView sRender;

struct NmlFilterGsRegisterEntry {
    u64 value;
    u64 registerId;
};

/* One scratchpad write contains two of the owner's actual GS slots. */

extern void nmlPacketGsInit(void);

/* Defining-owner interface; normal header transport remains an integration need. */
extern void packet_gs_entry64(u32 registerId, const u64 *value);

extern void nmlPacketAddGsFlush(void);

extern void nmlPacketAddGsFrame(int frame, int offset);

extern void nmlPacketAddGsFBA(int value);

extern void nmlPacketAddGsZbuf(int zBuffer);

extern void nmlPacketAddGsFlushWide(void);


/* Six original cube faces, each containing four corner indices. */


/* The projection helper's VFT0I/SQC2 output is integer GS coordinates. */

static void _CurSetMatrix(float matrix[4][4])
{
    __asm__ __volatile__(
        "lqc2 $vf27,0(%0)\n\t"
        "lqc2 $vf28,16(%0)\n\t"
        "lqc2 $vf29,32(%0)\n\t"
        "lqc2 $vf30,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

static void _CurSetViewScaleTrans(const float viewScale[4], const float viewTrans[4])
{
    __asm__ __volatile__(
        "lqc2 $vf25,0(%0)\n\t"
        "lqc2 $vf26,0(%1)\n\t"
        "nop"
        :
        : "r"(viewScale), "r"(viewTrans)
        : "memory"
    );
}

static int _CurRotTransPersClip(Vector4 *destination, const Vector4 *vector)
{
    register int clipFlags asm("$2");
    /* The SQC2 destination is an EE32 address in GPR4. After that
     * hardware use ends, the same word carries the masked clip flags. */
    register unsigned int destinationOrFlags asm("$4") = (unsigned int)destination;

    __asm__ __volatile__(
        "ctc2 $0,$vi18\n\t"
        "lqc2 $vf31,0(%2)\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf31x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf31y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf31z\n\t"
        "vmaddw.xyzw vf31xyzw,vf30xyzw,vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vclipw.xyz vf31xyz,vf31w\n\t"
        "vdiv Q,vf0w,vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw vf31xyzw,vf31xyzw,Q\n\t"
        "vmulaw.xyzw ACC,vf26xyzw,vf0w\n\t"
        "vmadd.xyzw vf31xyzw,vf31xyzw,vf25xyzw\n\t"
        "vftoi4.xyw vf23xyw,vf31xyw\n\t"
        "vftoi0.z vf23z,vf31z\n\t"
        "vsub.w vf23w,vf23w,vf23w\n\t"
        "sqc2 $vf23,0(%1)\n\t"
        "cfc2 %0,$vi18\n\t"
        "nop"
        : "=r"(clipFlags)
        : "r"(destinationOrFlags), "r"(vector)
        : "memory"
    );
    clipFlags &= 0x3f;
    destinationOrFlags = clipFlags;
    __asm__ __volatile__("" : : "r"(destinationOrFlags), "r"(clipFlags));
    return destinationOrFlags;
}



#define NML_FILTER_EMIT(id, payload) do { \
    packet[entryCount * 2].value = (payload); \
    packet[entryCount * 2 + 1].value = (id); \
    ++entryCount; \
} while (0)

#define NML_FILTER_VERTEX(point) do { \
    NML_FILTER_EMIT(3, (u64)(point)->displaced.u | \
        ((u64)(point)->displaced.v << 16)); \
    NML_FILTER_EMIT(4, (u64)(point)->displaced.x | \
        ((u64)(point)->displaced.y << 16) | 0x7fffffff00000000ULL); \
} while (0)

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlModelSetFilterGunosys);

#undef NML_FILTER_EMIT
#undef NML_FILTER_VERTEX


/* Original read-only data remains scaffolding, with no recovery credit. */

#define NML_STEALTH_VERTEX(uvEntry, xyzEntry, vertexIndex, clipMask) do { \
    (uvEntry)[1].value = 3; \
    (uvEntry)[0].value = (u64)geometry->uv[vertexIndex].u | \
        ((u64)geometry->uv[vertexIndex].v << 16); \
    (xyzEntry)[0].value = (u64)geometry->xyz[vertexIndex].x | \
        ((u64)geometry->xyz[vertexIndex].y << 16) | \
        ((u64)geometry->xyz[vertexIndex].z << 32); \
    (xyzEntry)[1].value = (clipMask); \
} while (0)

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlModelSetFilterStealth);

#undef NML_STEALTH_VERTEX

static void _WeightToGlobalPlaceInit(float matrix[4][4])
{
    __asm__ __volatile__(
        "lqc2 $vf10,0(%0)\n\t"
        "lqc2 $vf11,16(%0)\n\t"
        "lqc2 $vf12,32(%0)\n\t"
        "lqc2 $vf13,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

static void _WeightToGlobalPlaceVec(Vector4 *destination,
    const Matrix4 *jointMatrix, const Matrix4 *jointAuxMatrix)
{
    __asm__ __volatile__(
        "lq $2,0(%2)\n\t"
        "lq $3,16(%2)\n\t"
        "lq $4,32(%2)\n\t"
        "lqc2 $vf3,48(%2)\n\t"
        "qmfc2.ni $5,$vf0\n\t"
        "pextlw $6,$3,$2\n\t"
        "pextuw $7,$3,$2\n\t"
        "pextlw $8,$5,$4\n\t"
        "pextuw $9,$5,$4\n\t"
        "pcpyld $2,$8,$6\n\t"
        "pcpyud $3,$6,$8\n\t"
        "pcpyld $4,$9,$7\n\t"
        "qmtc2.ni $2,$vf5\n\t"
        "qmtc2.ni $3,$vf6\n\t"
        "qmtc2.ni $4,$vf7\n\t"
        "vmulax.xyz ACC,$vf5,$vf3x\n\t"
        "vmadday.xyz ACC,$vf6,$vf3y\n\t"
        "vmaddz.xyz $vf2,$vf7,$vf3z\n\t"
        "vmove.w $vf2,$vf0\n\t"
        "lqc2 $vf27,0(%1)\n\t"
        "lqc2 $vf28,16(%1)\n\t"
        "lqc2 $vf29,32(%1)\n\t"
        "lqc2 $vf30,48(%1)\n\t"
        "vsub.xyz $vf2,$vf0,$vf2\n\t"
        "vmulax.xyzw ACC,$vf27,$vf2x\n\t"
        "vmadday.xyzw ACC,$vf28,$vf2y\n\t"
        "vmaddaz.xyzw ACC,$vf29,$vf2z\n\t"
        "vmaddw.xyzw $vf31,$vf30,$vf2w\n\t"
        "vmulax.xyzw ACC,$vf10,$vf31x\n\t"
        "vmadday.xyzw ACC,$vf11,$vf31y\n\t"
        "vmaddaz.xyzw ACC,$vf12,$vf31z\n\t"
        "vmaddw.xyzw $vf31,$vf13,$vf0w\n\t"
        "sqc2 $vf31,0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(jointMatrix), "r"(jointAuxMatrix)
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
          "memory"
    );
}

static float _VectorLengthSQ(const Vector4 *first, const Vector4 *second)
{
    register float lengthSq asm("$f0");

    __asm__ __volatile__(
        "lqc2 $vf10,0(%1)\n\t"
        "lqc2 $vf11,0(%2)\n\t"
        "vsub.xyz vf10xyz,vf11xyz,vf10xyz\n\t"
        "vmul.xyz vf10xyz,vf10xyz,vf10xyz\n\t"
        "vaddz.x vf10x,vf10x,vf10z\n\t"
        "vaddy.x vf10x,vf10x,vf10y\n\t"
        "qmfc2 $2,$vf10\n\t"
        "mtc1 $2,$f2\n\t"
        "mtc1 $2,%0"
        : "=f"(lengthSq)
        : "r"(first), "r"(second)
        : "$2", "memory"
    );
    return lengthSq;
}

/* Private owner proposals; original data stays unchanged scaffolding. */
static void _WeightToGlobalPlaceVec(Vector4 *destination,
    const Matrix4 *jointMatrix, const Matrix4 *jointAuxMatrix);

/* Seven cross-sections form the fourteen-vertex strip. The XYZ and UV bands
 * use different weights and retain conversion before longitudinal extension. */
#define NML_STEALTH_XYZ_BAND(vertex, center, outsideXContribution, outsideY, centerWeight, outsideWeight, centerYContribution) do { \
    geometry->xyz[vertex][2] = geometry->xyz[center][2]; \
    geometry->xyz[vertex][1] = (int)((centerYContribution) + (outsideY) * (outsideWeight)); \
    geometry->xyz[vertex][0] = (int)((float)geometry->xyz[center][0] * (centerWeight) + (outsideXContribution)); \
} while (0)

#define NML_STEALTH_UV_BAND(vertex, center, outsideXContribution, outsideY, uvCenterWeight, uvOutsideWeight, centerYValue) do { \
    geometry->uv[vertex].v = (int)((centerYValue) * (uvCenterWeight) + (outsideY) * (uvOutsideWeight)); \
    geometry->uv[vertex].u = (int)((float)geometry->xyz[center][0] * (uvCenterWeight) + (outsideXContribution)); \
} while (0)

#define NML_STEALTH_EXTEND_XYZ(vertex, dx, dy, amount) do { \
    geometry->xyz[vertex][0] += (int)((dx) * (amount)); \
    geometry->xyz[vertex][1] += (int)((dy) * (amount)); \
} while (0)

#define NML_STEALTH_EXTEND_UV(vertex, dx, dy, amount) do { \
    geometry->uv[vertex].u += (int)((dx) * (amount)); \
    geometry->uv[vertex].v += (int)((dy) * (amount)); \
} while (0)

/* Interpolate the endpoint outside a screen edge in both texture and screen
 * coordinates. The original lower-edge formula uses the UV coordinate itself,
 * whereas the upper-edge formula subtracts the upper bound. */
#define NML_STEALTH_CLIP_ENDPOINT(numerator, denominator) do { \
    struct NmlStealthUV *outsideUV = &uv[outside]; \
    struct NmlStealthUV *insideUV = &uv[1 - outside]; \
    int *outsideXYZ = xyz[outside]; \
    int *insideXYZ = xyz[1 - outside]; \
    float fraction = __builtin_fabsf((float)(numerator) / (float)(denominator)); \
    float complement = 1.0f - fraction; \
    outsideUV->u = (int)(fraction * (float)insideUV->u + complement * (float)outsideUV->u); \
    outsideUV->v = (int)(fraction * (float)insideUV->v + complement * (float)outsideUV->v); \
    outsideXYZ[0] = (int)(fraction * (float)insideXYZ[0] + complement * (float)outsideXYZ[0]); \
    outsideXYZ[1] = (int)(fraction * (float)insideXYZ[1] + complement * (float)outsideXYZ[1]); \
} while (0)

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterStealthMake);

#undef NML_STEALTH_XYZ_BAND
#undef NML_STEALTH_UV_BAND
#undef NML_STEALTH_EXTEND_XYZ
#undef NML_STEALTH_EXTEND_UV
#undef NML_STEALTH_CLIP_ENDPOINT

void nmlFilterSetPacket(int *model, int count)
{
    int flags;
    int i;

    flags = model[0x234 / 4];
    for (i = 0; i < 31; i++) {
        switch (flags & (1 << i)) {
        case 1:
            flags &= ~1;
            nmlModelSetFilterGunosys(model, flags, count);
            break;
        case 2:
            flags &= ~2;
            nmlModelSetFilterStealth(model, flags);
            break;
        }
    }
}

void nmlFilterSetFrameToBuffer(int displayBufferBase)
{
    struct NmlFilterGsRegisterEntry *gsEntry3f;
    struct NmlFilterGsRegisterEntry *gsEntry50;
    struct NmlFilterGsRegisterEntry *gsEntry51;
    struct NmlFilterGsRegisterEntry *gsEntry52;
    struct NmlFilterGsRegisterEntry *gsEntry53;
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;
    u64 bitbltBuffer;
    u64 sourceBuffer;
    long long pixelStorageMode;

    remaining = 4;
    pixelStorageMode = sRender.pixelStorageMode;
    sourceBuffer = ((u64)sRender.framebufferPage << 37) | ((u64)0x8000 << 36);
    bitbltBuffer = 0x80000 | ((displayBufferBase & 0xffff) << 5) |
        (pixelStorageMode << 56) | sourceBuffer | (pixelStorageMode << 24);
    gsEntry50 = (struct NmlFilterGsRegisterEntry *)0x70000010;
    gsEntry51 = (struct NmlFilterGsRegisterEntry *)0x70000020;
    gsEntry52 = (struct NmlFilterGsRegisterEntry *)0x70000030;
    gsEntry53 = (struct NmlFilterGsRegisterEntry *)0x70000040;
    gsEntry3f = (struct NmlFilterGsRegisterEntry *)0x70000000;
    gsEntry3f->value = 0;
    gsEntry3f->registerId = 0x3f;
    gsEntry50->value = bitbltBuffer;
    gsEntry50->registerId = 0x50;
    gsEntry51->value = 0;
    gsEntry51->registerId = 0x51;
    gsEntry52->value = ((u64)0x1c0 << 32) | 0x200;
    gsEntry52->registerId = 0x52;
    gsEntry53->value = 2;
    gsEntry53->registerId = 0x53;
    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;
    nmlPacketGsInit();
    for (; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((int)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

void nmlFilterSetBufferToFrame(int frameBufferBase)
{
    struct NmlFilterGsRegisterEntry *gsEntry3f;
    struct NmlFilterGsRegisterEntry *gsEntry50;
    struct NmlFilterGsRegisterEntry *gsEntry51;
    struct NmlFilterGsRegisterEntry *gsEntry52;
    struct NmlFilterGsRegisterEntry *gsEntry53;
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;
    u64 bitbltBuffer;
    u64 sourceBuffer;
    long long pixelStorageMode;

    remaining = 4;
    pixelStorageMode = sRender.pixelStorageMode;
    sourceBuffer = ((u64)(frameBufferBase & 0xffff) << 37) | ((u64)0x8000 << 36);
    bitbltBuffer = 0x80000 | (sRender.framebufferPage << 5) |
        (pixelStorageMode << 56) | sourceBuffer | (pixelStorageMode << 24);
    gsEntry50 = (struct NmlFilterGsRegisterEntry *)0x70000010;
    gsEntry51 = (struct NmlFilterGsRegisterEntry *)0x70000020;
    gsEntry52 = (struct NmlFilterGsRegisterEntry *)0x70000030;
    gsEntry53 = (struct NmlFilterGsRegisterEntry *)0x70000040;
    gsEntry3f = (struct NmlFilterGsRegisterEntry *)0x70000000;
    gsEntry3f->value = 0;
    gsEntry3f->registerId = 0x3f;
    gsEntry50->value = bitbltBuffer;
    gsEntry50->registerId = 0x50;
    gsEntry51->value = 0;
    gsEntry51->registerId = 0x51;
    gsEntry52->value = ((u64)0x1c0 << 32) | 0x200;
    gsEntry52->registerId = 0x52;
    gsEntry53->value = 2;
    gsEntry53->registerId = 0x53;
    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;
    nmlPacketGsInit();
    for (; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((int)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

void nmlFilterSetBufferRender(float opacity)
{
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;

    ((struct NmlFilterGsRegisterEntry *)0x70000000)->registerId = 0x3f;
    ((struct NmlFilterGsRegisterEntry *)0x70000000)->value = 0;
    ((struct NmlFilterGsRegisterEntry *)0x70000010)->value =
        0x24020000 | ((u64)(sRender.framebufferPage << 5)) |
        ((u64)0x20000006 << 32) | ((u64)0x4000 << 16);
    ((struct NmlFilterGsRegisterEntry *)0x70000010)->registerId = 6;
    ((struct NmlFilterGsRegisterEntry *)0x70000020)->value = 0;
    ((struct NmlFilterGsRegisterEntry *)0x70000020)->registerId = 0x14;
    ((struct NmlFilterGsRegisterEntry *)0x70000030)->value = 0;
    ((struct NmlFilterGsRegisterEntry *)0x70000030)->registerId = 8;
    ((struct NmlFilterGsRegisterEntry *)0x70000040)->value = 0x31001;
    ((struct NmlFilterGsRegisterEntry *)0x70000040)->registerId = 0x47;
    ((struct NmlFilterGsRegisterEntry *)0x70000050)->value =
        ((u64)(int)(opacity * 128.0f) << 32) | 0x64;
    ((struct NmlFilterGsRegisterEntry *)0x70000050)->registerId = 0x42;
    ((struct NmlFilterGsRegisterEntry *)0x70000060)->value =
        0x3f80000080808080ULL;
    ((struct NmlFilterGsRegisterEntry *)0x70000060)->registerId = 1;
    ((struct NmlFilterGsRegisterEntry *)0x70000070)->value = 0x116;
    if (opacity < 1.0f) {
        ((struct NmlFilterGsRegisterEntry *)0x70000070)->value = 0x156;
    }
    ((struct NmlFilterGsRegisterEntry *)0x70000070)->registerId = 0;
    ((struct NmlFilterGsRegisterEntry *)0x70000080)->value = 0;
    ((struct NmlFilterGsRegisterEntry *)0x70000080)->registerId = 3;
    ((struct NmlFilterGsRegisterEntry *)0x70000090)->value = 0x72007000;
    ((struct NmlFilterGsRegisterEntry *)0x70000090)->registerId = 4;
    ((struct NmlFilterGsRegisterEntry *)0x700000A0)->value = 0x1C102000;
    ((struct NmlFilterGsRegisterEntry *)0x700000A0)->registerId = 3;
    ((struct NmlFilterGsRegisterEntry *)0x700000B0)->value = 0x8E009000;
    ((struct NmlFilterGsRegisterEntry *)0x700000B0)->registerId = 4;

    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;
    nmlPacketGsInit();
    for (remaining = 11; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((u32)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

void nmlFilterSetFrameAlphaClear(void)
{
    struct NmlFilterGsRegisterEntry *gsEntry42;
    struct NmlFilterGsRegisterEntry *gsEntry47;
    struct NmlFilterGsRegisterEntry *gsEntry1;
    struct NmlFilterGsRegisterEntry *gsEntry46;
    struct NmlFilterGsRegisterEntry *gsEntry4First;
    struct NmlFilterGsRegisterEntry *gsEntry4Last;
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;

    gsEntry42 = (struct NmlFilterGsRegisterEntry *)0x70000000;
    gsEntry47 = (struct NmlFilterGsRegisterEntry *)0x70000010;
    gsEntry1 = (struct NmlFilterGsRegisterEntry *)0x70000020;
    gsEntry46 = (struct NmlFilterGsRegisterEntry *)0x70000030;
    gsEntry4First = (struct NmlFilterGsRegisterEntry *)0x70000040;
    gsEntry4Last = (struct NmlFilterGsRegisterEntry *)0x70000050;
    gsEntry42->value = 0x68;
    gsEntry42->registerId = 0x42;
    gsEntry47->value = 0x31001;
    gsEntry47->registerId = 0x47;
    gsEntry1->value = 0x3f80000001000000;
    gsEntry1->registerId = 1;
    gsEntry46->value = 0x46;
    gsEntry46->registerId = 0;
    gsEntry4First->value = 0x72007000;
    gsEntry4First->registerId = 4;
    gsEntry4Last->value = 0x8e009000;
    gsEntry4Last->registerId = 4;
    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;

    nmlPacketGsInit();
    nmlPacketAddGsFrame(sRender.displayBufferBase, 0);
    nmlPacketAddGsFBA(0);
    for (remaining = 5; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((int)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

void nmlFilterSetTexClear(void)
{
    struct NmlFilterGsRegisterEntry *gsEntry42;
    struct NmlFilterGsRegisterEntry *gsEntry47;
    struct NmlFilterGsRegisterEntry *gsEntry1;
    struct NmlFilterGsRegisterEntry *gsEntry46;
    struct NmlFilterGsRegisterEntry *gsEntry4First;
    struct NmlFilterGsRegisterEntry *gsEntry4Last;
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;

    gsEntry42 = (struct NmlFilterGsRegisterEntry *)0x70000000;
    gsEntry47 = (struct NmlFilterGsRegisterEntry *)0x70000010;
    gsEntry1 = (struct NmlFilterGsRegisterEntry *)0x70000020;
    gsEntry46 = (struct NmlFilterGsRegisterEntry *)0x70000030;
    gsEntry4First = (struct NmlFilterGsRegisterEntry *)0x70000040;
    gsEntry4Last = (struct NmlFilterGsRegisterEntry *)0x70000050;
    gsEntry42->value = 0x8a;
    gsEntry42->registerId = 0x42;
    gsEntry47->value = 0x31001;
    gsEntry47->registerId = 0x47;
    gsEntry1->value = 0x3f80000080000000;
    gsEntry1->registerId = 1;
    gsEntry46->value = 0x46;
    gsEntry46->registerId = 0;
    gsEntry4First->value = 0x72007000;
    gsEntry4First->registerId = 4;
    gsEntry4Last->value = 0x7ff09000;
    gsEntry4Last->registerId = 4;
    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;

    nmlPacketGsInit();
    for (remaining = 5; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((int)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

void nmlFilterBackClear(void)
{
    struct NmlFilterGsRegisterEntry *gsEntry47;
    struct NmlFilterGsRegisterEntry *gsEntry1;
    struct NmlFilterGsRegisterEntry *gsEntry0;
    struct NmlFilterGsRegisterEntry *gsEntry4First;
    struct NmlFilterGsRegisterEntry *gsEntry4Last;
    struct NmlFilterGsRegisterEntry *entry;
    int remaining;

    gsEntry47 = (struct NmlFilterGsRegisterEntry *)0x70000000;
    gsEntry1 = (struct NmlFilterGsRegisterEntry *)0x70000010;
    gsEntry0 = (struct NmlFilterGsRegisterEntry *)0x70000020;
    gsEntry4First = (struct NmlFilterGsRegisterEntry *)0x70000030;
    gsEntry4Last = (struct NmlFilterGsRegisterEntry *)0x70000040;
    gsEntry47->value = 0x30000;
    gsEntry47->registerId = 0x47;
    gsEntry1->value = (u64)0xfe00 << 46;
    gsEntry1->registerId = 1;
    gsEntry0->value = 6;
    gsEntry0->registerId = 0;
    gsEntry4First->value = 0x72007000;
    gsEntry4First->registerId = 4;
    gsEntry4Last->value = 0x8e009000;
    gsEntry4Last->registerId = 4;
    entry = (struct NmlFilterGsRegisterEntry *)0x70000000;

    nmlPacketGsInit();
    nmlPacketAddGsZbuf(1);
    nmlPacketAddGsFrame(sRender.framebufferPage, 0);
    for (remaining = 4; remaining >= 0; remaining--, entry++) {
        packet_gs_entry64((int)entry->registerId, &entry->value);
    }
    nmlPacketAddGsFlush();
}

extern void nmlPacketAddGsFrame1(int frame, int offset);

void nmlFilterSetFlatRender(LayoutStore *model)
{
    struct NmlFilterRenderView *render;
    GsTag *gsEntry3f;
    GsTag *gsEntry6;
    GsTag *gsEntry14;
    GsTag *gsEntry8;
    GsTag *gsEntry42;
    GsTag *gsEntry47First;
    GsTag *gsEntry1;
    GsTag *gsEntry0;
    GsTag *gsEntry3First;
    GsTag *gsEntry4First;
    GsTag *gsEntry3Last;
    GsTag *gsEntry4Last;
    GsTag *gsEntry47Last;
    GsTag *entry;
    int remaining;
    u64 texture;
    GsTag alphaPayload;
    int clampRegisterId;

    if (model == 0) {
        return;
    } else {
        clampRegisterId = 8;
        render = &sRender;
        texture = (u64)(0x24020000 | (render->framebufferPage << 5)) |
            0x2000000640000000ULL;
        gsEntry6 = (GsTag *)0x70000010;
        gsEntry14 = (GsTag *)0x70000020;
        gsEntry8 = (GsTag *)0x70000030;
        gsEntry3f = (GsTag *)0x70000000;
        gsEntry3f[0].value = 0;
        gsEntry3f[1].value = 0x3f;
        gsEntry6[0].value = texture;
        gsEntry6[1].value = 6;
        gsEntry14[0].value = 0x60;
        gsEntry14[1].value = 0x14;
        gsEntry8[0].value = 0;
        gsEntry8[1].value = clampRegisterId;
        alphaPayload.value = ((u64)(int)(model->fields.transparency * 128.0f) << 32) | 0x68;
        gsEntry42 = (GsTag *)0x70000040;
        gsEntry47First = (GsTag *)0x70000050;
        gsEntry1 = (GsTag *)0x70000060;
        gsEntry0 = (GsTag *)0x70000070;
        gsEntry3First = (GsTag *)0x70000080;
        gsEntry4First = (GsTag *)0x70000090;
        gsEntry3Last = (GsTag *)0x700000a0;
        gsEntry4Last = (GsTag *)0x700000b0;
        gsEntry47Last = (GsTag *)0x700000c0;
        gsEntry42[0] = alphaPayload;
        gsEntry42[1].value = 0x42;
        gsEntry47First[0].value = 0x30000;
        gsEntry47First[1].value = 0x47;
        gsEntry1[0].value = 0x3f80000080808080ULL;
        gsEntry1[1].value = 1;
        gsEntry0[0].value = 0x156;
        gsEntry0[1].value = 0;
        gsEntry3First[0].value = 0;
        gsEntry3First[1].value = 3;
        gsEntry4First[0].value = 0x72007000;
        gsEntry4First[1].value = 4;
        gsEntry3Last[0].value = 0x1c002000;
        gsEntry3Last[1].value = 3;
        gsEntry4Last[0].value = 0x8e009000ULL;
        gsEntry4Last[1].value = 4;
        gsEntry47Last[0].value = 0x50000;
        gsEntry47Last[1].value = 0x47;
        entry = (GsTag *)0x70000000;
        remaining = 12;
        nmlPacketGsInit();
        nmlPacketAddGsZbuf(1);
        nmlPacketAddGsFrame(render->displayBufferBase, 0);
        nmlPacketAddGsFrame1(render->displayBufferBase, 0);
        for (; remaining >= 0; remaining--, entry += 2) {
            packet_gs_entry64(entry[1].words[0], &entry[0].value);
        }
        nmlPacketAddGsFlushWide();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetTexFillDraw);

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetVolumeCubeRender);

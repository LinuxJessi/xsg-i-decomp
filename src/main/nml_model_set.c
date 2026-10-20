#include "common.h"
#include "main/xgl_render.h"
#include "main/xgl_2.h"

#include "shared.h"

#include "nml_model_set.h"
#include "main/xgl_studio.h"

extern unsigned char s_aDispVisible[];

int g_aSubWindow[4] = { 0 };

LayoutStore s_inLayout = { 0 };

static void *s_pMapLast = 0;

static int s_nParentBuf = 0;

static int s_nToumeiNum = 0;

static unsigned int s_nShadowVec = 0;

static int s_nMapShadowParts = 0;

static float s_fSortOffsetEntry = 0.0f;

static int s_nParent = 0;

static int s_nClip = 0;

static int s_nShapeNum = 0;

static int s_nMapClip = 0;

static int s_nPacketSignal = 0;

static int s_nUseZwrite = 0;

static int s_nUseBackBuffer = 0;

static int s_nEffectWrite = 1;

static int s_nMapLast = 0;

static int s_nFadeDoit = 0;

static int s_nPause = 0;

static int s_nMenu = 0;

static int s_nFrameLockOff = 0;

static int s_nRenderCancelOld = 0;

int g_nGsEntry = 0;

static int s_nAlphaGroup;

static int s_nNonAlphaGroup;

static int s_aToumeiId[16];

static float s_aToumei[16];

static int s_aShapeId[32];

static float s_aShapeWeight[32];

static int s_aMapLast[12];

static Vector4 s_inShadowVec;

static Vector4 s_inGblPos;

/*
 * Circle rendering copies this 0x400-byte table after the 0x20-byte prefix
 * at 0x00956920: nmlPacketSetAttributeData16N consumes 66 16-byte qwords.
 * Keep its definition between s_inGblPos and s_inProReal, matching the
 * original local-storage order. set_circle_shadow_ratio writes entries 0-7
 * and 16-23.
 */

static int D_00956940[256];

/*
 * The renderer passes this 0x1e0-byte block to nmlPacketSetAttributeData16N
 * with count 0x1e, so the copied extent is established. nmlModelClear clears
 * projection_entry at +0x1c0. No C body names the earlier float slots; their
 * meanings stay unknown, so that region is explicit unmodeled storage.
 */

typedef struct NmlProRealFields {
    Matrix4 matrix_000;
    Matrix4 inverseTransform;
    /* The original Circle/Map bodies use the +0x80/+0xC0 ranges only as
     * whole-qword copy destinations. Their base is 0x0095B940 and every
     * access is at a 16-byte slot in the corresponding function.s; model
     * each owned qword directly, without an overlay. */
    Vector4 unmodeled_080[4];
    Vector4 unmodeled_0c0[4];
    int unmodeled_100;
    int unmodeled_104;
    int unmodeled_108;
    int unmodeled_10c;
    short unmodeled_110;
    short unmodeled_112;
    short unmodeled_114;
    short unmodeled_116;
    int unmodeled_118;
    int unmodeled_11c;
    float unmodeled_120;
    float unmodeled_124;
    unsigned char unmodeled_128[8];
    u64 textureWord;
    u64 clampWord;
    float textureUOffset;
    float textureUScale;
    float textureVOffset;
    float textureVScale;
    Vector4 transformOrigin;
    Vector4 unmodeled_160;
    Vector4 unmodeled_170[2];
    Vector4 unmodeled_190[3];
} NmlProRealFields;

typedef struct NmlProRealState {
    NmlProRealFields fields;
    int projection_entry;
    unsigned char unmodeled_1c4[0x1e0 - 0x1c4];
} NmlProRealState;




static NmlProRealState s_inProReal[1];

static int s_inBackBuffer[10];

static int s_inMapHandle[4];

static FadeControl s_inFadeOut;

static FadeControl s_inFadeIn;

static FadeControl s_inActiveFadeOut;

static FadeControl s_inActiveFadeIn;

#define NML_RENDER_MAP_ENTRY 0x10000u


static void CONSTRUCT_MODELSYSTEM(void);

/* INIT_BACK_BUFFER (above) is still INCLUDE_ASM; declare it so its caller
 * does not see an implicit declaration. */

static void INIT_BACK_BUFFER(void);

/*
 * TU-local partial view of the shared render-state global sRender (main
 * 0x004A90E0, size 0x5C; see src/main/game_over.c's DrawImageRenderState,
 * src/main/map_1.c's MapRenderState, src/main/window_tex_load.c's
 * UmnRenderSize and src/main/game_defocus.c's DefocusRenderState for other
 * TUs' views of the same object). Only the halfword at +0x22 is evidenced
 * here: nmlModelSendSignalMovieFinish below forwards it unchanged as the
 * "ownerIndex" argument of nmlModelSetBackBuffer and
 * nmlModelSetBackBufferToBattle.
 */







/*
 * nmlModelSetFadeInCancel/nmlModelSetFadeOutCancel/nmlModelSetBackBuffer
 * (below, this TU) and nmlModelSetBackBufferToBattle (still INCLUDE_ASM)
 * are used before their definitions; declare them so these calls do not
 * see an implicit declaration.
 */

void nmlModelSetFadeInCancel(int frames);

void nmlModelSetFadeOutCancel(int frames);

void nmlModelSetBackBuffer(int modelId, int duration, int ownerIndex, int value);

void nmlModelSetBackBufferToBattle(int ownerIndex);

static void fade_render(NmlPacket packet, FadeControl *control);

/*
 * D_0095DB90 is main:0x0095DB90, the same byte the FadeControl comment above
 * ties to s_inFadeOut's "locked" field at +0x30 (0x0095DB60 + 0x30): cited
 * there only as evidence, not as claimed source. Addressing it through
 * &s_inFadeOut+0x30 rather than its own symbol changes the emitted %lo
 * immediate (sw v0,48(v1) for the original's sw v0,0(v1)), so this
 * function keeps the flat scaffold symbol, as an unsized array so -G8
 * small-data gp-relative addressing does not apply to it either (matching
 * D_0095BB3C/D_0095BB44 above).
 */

extern int D_0095DB90[];

/* D_0095DC48 is s_inActiveFadeIn's "cancelFrames" byte (+0x28); see the
 * D_0095DB90 comment above for why it stays a flat scaffold symbol. */

extern int D_0095DC48[];

/* D_0095DBC8 is s_inFadeIn's "cancelFrames" byte (+0x28). */

extern int D_0095DBC8[];

/* D_0095DC08 is s_inActiveFadeOut's "cancelFrames" byte (+0x28). */

extern int D_0095DC08[];

/* D_0095DB88 is s_inFadeOut's "cancelFrames" byte (+0x28). */

extern int D_0095DB88[];

/* D_0095DB84 is s_inFadeOut's "dispose" byte (+0x24). */

extern int D_0095DB84[];

/* D_0095DBC4 is s_inFadeIn's "dispose" byte (+0x24). */

extern int D_0095DBC4[];

/*
 * TU-local partial view of GameLoopState (main 0x00338680, size 0x2a030;
 * TU-local by canon, config/header-canon.json). Only the two fields this
 * setter reads are modeled: the fade level at +0x58 and the three fade
 * color components at +0x90, both forwarded to fade_set unchanged.
 */

typedef struct {
    float color[3];
    float duration;
} NmlFadeValues;

typedef struct {
    unsigned char unmodeled_000[0xc - 0x0];
    unsigned short stealthMode; /* +0xc */
    unsigned char unmodeled_00e[0x10 - 0xe];
    unsigned int flags; /* +0x10 */
    unsigned char unmodeled_014[0x58 - 0x14];
    int level;                 /* +0x58 */
    int previousFadeMode; /* +0x5c */
    unsigned char unmodeled_060[0x30];
    union {
        float color[3]; /* +0x90 */
        NmlFadeValues fade;
    };
    NmlFadeValues previousFade; /* +0xa0 */
} NmlGameLoopState;




extern NmlGameLoopState GameLoopState;

/* fade_set (near the top of this TU) is still INCLUDE_ASM; declare it so
 * this caller does not see an implicit declaration. */

static void fade_set(FadeControl *, float *, int, int, int, int, int);

/* ACT_modelDrawSub supplies alpha as a signed word from its actor at +0x9a8. */

#define NML_RENDER_STENCIL 0x4u


#define NML_RENDER_ZWRITE 0x8u


#define NML_RENDER_TOUMEI 0x20u


/*
 * Bounds and alignment a model texture pointer must satisfy: the pointer
 * lands inside main RAM at or above the executable's own load base
 * (config/tu/main/tu000.json "start": "0x00200000") and at or below the top
 * of the 32 MiB EE RAM, and it is qword (16-byte) aligned as GIF/DMA
 * texture transfers require.
 */

#define NML_TEXTURE_RAM_BASE 0x00200000u


#define NML_TEXTURE_RAM_MAX_OFFSET 0x01dfffffu


#define NML_TEXTURE_ALIGN_MASK 0xfu


/*
 * Stores the current model matrix pointer in the pointer-width s_inLayout
 * word at byte offset 0x23c (slot 143 of the shared LayoutStore view, which
 * gives that slot no pointer member).
 */

/* nmlModelFogPara (above) is still INCLUDE_ASM; declare it so its caller
 * does not see an implicit declaration. */

static void nmlModelFogPara(Vector4 *fog, float fogNear, float fogFar, float fogMin, float fogMax);

#define NML_RENDER_FOG 0x2u


/* The four fog distances of RgFog.dist (defaults 50, 250, 0, 1). */

/* The map callers pass halfword part IDs; the layout has eight shadow slots. */

extern unsigned int strlen(const char *string);

extern int memcmp(const void *left, const void *right, unsigned int count);

extern int strcmp(const char *left, const char *right);

extern char *strstr(const char *string, const char *pattern);

/* nmlModelLexDataCheck remains assembly-owned below. */

int nmlModelLexDataCheck(NmlModel *model);

#define NML_NAME_MATCH_PREFIX 1


#define NML_NAME_MATCH_EXACT 2


#define NML_NAME_MATCH_SUBSTRING 3


/* nmlModelDirectXtxSub (above) is still INCLUDE_ASM; declare it so this
 * caller does not see an implicit declaration. */

static void nmlModelDirectXtxSub(void *data, int modelId, int nextModelId);

/*
 * Callers (GameRadarDraw main 0x00252fc8, unit6003_draw main 0x003211a0;
 * both still INCLUDE_ASM) never inspect the result -- the tail call below
 * matches the compiled form, so this is void.
 */

#define LAYOUT_W(off) (s_inLayout.slots[(off) / 4].f)

#define LAYOUT_S32(off) (s_inLayout.slots[(off) / 4].i)

/*
 * CONSTRUCT_CIRCLR_SHADOW (near the top of this TU) is still INCLUDE_ASM
 * and nmlModelClear is defined below (this TU); declare them, and the
 * s_inMapHandle array nmlModelClear also declares nearer its own use, so
 * this caller does not see an implicit declaration.
 */

static void CONSTRUCT_CIRCLR_SHADOW(void);

void nmlModelClear(void);

/* CLEAR_LAYOUT_MODEL and CLEAR_MODEL_ENTRY (near the top of this TU) are
 * still INCLUDE_ASM; declare them so this caller does not see an implicit
 * declaration. */

static void CLEAR_LAYOUT_MODEL(LayoutStore *arg0);

static void CLEAR_MODEL_ENTRY(void);

/* FLUSH_MODELSYSTEM (above) is still INCLUDE_ASM; declare it so this
 * caller does not see an implicit declaration. */

static void FLUSH_MODELSYSTEM(void);

/*
 * Circle rendering copies this 0x400-byte table after the 0x20-byte prefix
 * at 0x00956920: nmlPacketSetAttributeData16N consumes 66 16-byte qwords.
 * Keep its definition between s_inGblPos and s_inProReal, matching the
 * original local-storage order. set_circle_shadow_ratio writes entries 0-7
 * and 16-23.
 */

/*
 * The renderer passes this 0x1e0-byte block to nmlPacketSetAttributeData16N
 * with count 0x1e, so the copied extent is established.  ProjectMap and
 * ProjectCircle access the listed float slots; nmlModelClear clears the word
 * at +0x1c0.  Other bytes remain unnamed because their roles are not shown
 * by this TU's C callers.
 */

extern int s_nDispVisible;

extern char s_aTexName[32];

typedef struct NmlMaterialName {
    char text[32];
} NmlMaterialName;
extern NmlMaterialName s_aMatName;

extern Matrix4 s_inGblPlace;

extern void xglStudioChange(int);

extern void xglLightCalcMatrix(StudioLight *);

extern Matrix4 s_inGblPlace;

extern void xglStudioChange(int);

extern void xglLightCalcMatrix(StudioLight *);

struct NmlShadowTexture {
    unsigned int uploadHeader[8];
    unsigned char pixels[64][64];
};




extern struct NmlShadowTexture s_aCircleShadow;

extern unsigned int s_aCircleShadowClut[8];

extern int s_nCircleShadow;

extern float xglPointLength(Vector4 *first, Vector4 *second);

extern float D_004D88D8;

extern float D_004D88DC;





typedef struct NmlDirectRecord {
    void *data;
    s16 count;
    s16 attribute;
} NmlDirectRecord;




extern NmlDirectRecord s_aDirect[];

extern u8 *nmlPacketSetAttributeData16N(const void *data, int count);

typedef struct {
    unsigned char unmodeled_00[8];
    int entryCount;
    int entriesOffset;
} NmlDirectXtxBlock;




typedef struct {
    unsigned int packed;
    unsigned int word4;
    unsigned int word8;
    int size;
    int dataOffset;
} NmlDirectXtxEntry;




extern u64 TestEnv_0_004A9170[12];

extern void nmlFilterStealthMake(int, f32, void *, LayoutSlot *);

extern f32 D_004D88E4;

extern f32 tanf(f32);

extern f32 D_004D88E8;

extern f32 D_004D88EC;

extern void *s_apBlock[];

extern unsigned short s_aBlockLayout[];

extern void *s_apBlockLast[];

extern unsigned short s_aBlockLayoutLast[];

extern LayoutStore *s_apModelLayout[];

extern int s_nCount;

extern int s_aShapeMathPara[10];

extern Vector4 s_inMulColor;

extern f32 D_004D88E0;

typedef struct NmlModelSampleView {
    unsigned char unmodeled_000[0x48];
    int sampleCount;
    unsigned char unmodeled_04c[0x58 - 0x4c];
    int weightOffset;
    int transformOffset;
    unsigned char unmodeled_060[0x90 - 0x60];
    Vector4 samplePosition;
} NmlModelSampleView;




extern int nmlModelCalcClipStudio(Vector4 *, int);

extern f32 D_004D88F4;

extern f32 D_004D88F8;

extern f32 D_004D88FC;

extern f32 D_004D8900;

extern f32 D_004D8904;

extern f32 D_004D8908;

extern f32 D_004D890C;

extern f32 D_004D8910;

extern f32 D_004D8914;

extern f32 D_004D8918;

extern char D_004DC470[];

static float deapth_for_studio(int studio, Vector4 *position, float scale);

extern float D_004D88F0;

typedef struct {
    unsigned char unmodeled_00[0x94];
    float fov;
} NmlStudioFovView;




typedef int NmlProjectedPoint[4];




typedef struct NmlMaterialRenderState NmlMaterialRenderState;




typedef struct NmlModelRenderState NmlModelRenderState;




struct NmlModelRenderRange {
    short first;
    short count;
    unsigned char unmodeled_04[8];
    LayoutStore *layout;
};




extern void nmlPacketGsInit(void);

extern void nmlPacketAddGsZbuf(int);

extern void nmlPacketAddGsFBA(int);

extern void nmlPacketAddGsFlush(void);

extern void nmlPacketAddGsPixeltest(int);

extern void nmlPacketAddGsAlpha(u64 alpha);

extern void nmlPacketAddGifTagStandard(int, int);

extern void nmlPacketAddScreen(NmlModelRenderState *);

extern void nmlPacketAddTransMicrocode(const NmlMaterialRenderState *,
                                       const NmlModelRenderState *);

extern void nmlPacketAddExecProg(NmlMaterialRenderState *,
                                 NmlModelRenderState *, int, int);

extern StudioCamera *nmlPacketAddParts(NmlMaterialRenderState *,
                                       NmlModelRenderState *, StudioCamera *);

extern void nmlPacketAddBlockMaterial(NmlMaterialRenderState *,
                                      NmlModelRenderState *, int, int);

extern void nmlFilterSetFrameAlphaClear(void);

extern int s_nStencilVal;

extern void nmlPacketAddGsFrame(int, int);

extern void nmlPacketAddGsFrame1(int, int);

extern void nmlPacketAddGsFBA1(unsigned int);

extern void nmlFilterSetFrameToBuffer(int);

extern int s_nRenderCancel;

extern int g_nPacketOver;

extern void AlphaGroupSort(void);

extern void nmlPacketAddTransMicrocodeInit(void);

extern void nmlPacketAddGsZbuf1(unsigned int);

extern void nmlPacketAddGsPixeltest1(unsigned int);

extern void nmlPacketAddGsAlpha1(unsigned long long);

extern void nmlPacketAddGsTexture(void);

extern void nmlPacketAddGsClamp(unsigned int);

extern void nmlFilterSetBufferRender(float);

extern void sefDrawEffect(void);

void xglMatrixScale(Matrix4 destination, const Matrix4 source, const float scale[4]);

extern void xglVectorMulMat(Vector4 *destination, const Matrix4 matrix,
                            const Vector4 *vector);

extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);

typedef struct NmlProRealOrigin {
    u64 word0;
    u64 word8;
} NmlProRealOrigin;


extern NmlProRealOrigin D_004D2820;






typedef struct {
    short x;                            /* +0x00 */
    short y;                            /* +0x02 */
    unsigned char unmodeled_004[8];
    int ownerAddress;                   /* +0x0c, 32-bit word */
} NmlCircleOrigin;




typedef struct {
    unsigned char unmodeled_000[0x250];
    unsigned int flags;                 /* +0x250 */
    unsigned char unmodeled_254[0x270 - 0x254];
    int ratio;                          /* +0x270 */
    unsigned char unmodeled_274[0x2c0 - 0x274];
    float scale;                        /* +0x2c0 */
} NmlCircleModelView;




typedef struct {
    unsigned char unmodeled_000[0x26c];
    float viewLimit;                    /* +0x26c */
} NmlCircleOwnerView;




int nmlModelRenderProreal(LayoutStore *model, int frame);

void nmlPacketSendCircleTexture(void *shadow, void *clut);

extern float D_004D894C;

static void set_circle_shadow_ratio(int scale);

static void _WeightToGlobalVec(Vector4 *destination, const Matrix4 matrix, const Matrix4 weights);

void nmlFadePacketWrite(NmlPacket packet);

void nmlModelDirectSend(int mode, const void *data, int count);



typedef struct NmlCircleLayout {
    unsigned char unmodeled_000[0x250];
    u32 renderFlags;
    unsigned char unmodeled_254[0x26c - 0x254];
    float clipThreshold;
    int ratioValue;
    unsigned char unmodeled_274[0x2c0 - 0x274];
    float ratioScale;
} NmlCircleLayout;



void nmlModelFlushClear(void);

static void AlphaGroupLastEntry(void);

extern int nmlModelRenderTexture(void *, int, int);

static int nmlModelRenderCircle(const NmlCircleOrigin *origin, int pitch, int frame);

extern int nmlModelRenderDropCircle(void *, int, StudioCamera *);

static StudioCamera *nmlModelRenderDrop(struct NmlModelRenderRange *range,
                                        int step, StudioCamera *screen, int mode);

typedef struct NmlFlushMaterialView {
    char name[0x20];
    unsigned int render_flags; /* +0x20 */
    unsigned char unmodeled_024[0x2c - 0x24];
    int partIndex; /* +0x2c */
    unsigned char unmodeled_030[0xc0 - 0x30];
    unsigned int material_flags; /* +0xc0 */
} NmlFlushMaterialView;



typedef struct NmlDirectEntry {
    const void *data;
    unsigned short size;
    unsigned short type;
} NmlDirectEntry;



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

static int _CurRotTransPersClip(NmlProjectedPoint destination, const Vector4 *vector)
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

static void _CurRotTransPersFog(Vector4 *destination, const Vector4 *vector, const Vector4 *fog) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "lqc2 vf4, 0(%2)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "vdiv Q, vf0w, vf31w\n\t"
        "vmulz.w vf1, vf0, vf4z\n\t"
        "vwaitq\n\t"
        "vmula.w ACC, vf1, vf0\n\t"
        "vmaddq.w vf31, vf4, Q\n\t"
        "vmaxx.w vf31, vf31, vf4x\n\t"
        "vminiy.w vf31, vf31, vf4y\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(vector), "r"(fog)
        : "memory"
    );
}

static void _CurApplyMatrix(Vector4 *destination, const Vector4 *vector) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "sqc2 vf31, 0(%1)\n\t"
        "nop"
        :
        : "r"(vector), "r"(destination)
        : "memory"
    );
}

static void _WeightToGlobalVec(Vector4 *destination, const Matrix4 matrix, const Matrix4 weights)
{
    __asm__ __volatile__(
        "lq $2, 0(%2)\n\t"
        "lq $3, 16(%2)\n\t"
        "lq $4, 32(%2)\n\t"
        "lqc2 vf3, 48(%2)\n\t"
        "qmfc2 $5, vf0\n\t"
        "pextlw $6, $3, $2\n\t"
        "pextuw $7, $3, $2\n\t"
        "pextlw $8, $5, $4\n\t"
        "pextuw $9, $5, $4\n\t"
        "pcpyld $2, $8, $6\n\t"
        "pcpyud $3, $6, $8\n\t"
        "pcpyld $4, $9, $7\n\t"
        "qmtc2 $2, vf5\n\t"
        "qmtc2 $3, vf6\n\t"
        "qmtc2 $4, vf7\n\t"
        "vmulax.xyz ACC, vf5, vf3x\n\t"
        "vmadday.xyz ACC, vf6, vf3y\n\t"
        "vmaddz.xyz vf2, vf7, vf3z\n\t"
        "vmove.w vf2, vf0\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vsub.xyz vf2, vf0, vf2\n\t"
        "vmulax.xyzw ACC, vf27, vf5x\n\t"
        "vmadday.xyzw ACC, vf28, vf5y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf5z\n\t"
        "vmaddw.xyzw vf5, vf30, vf5w\n\t"
        "vmulax.xyzw ACC, vf27, vf6x\n\t"
        "vmadday.xyzw ACC, vf28, vf6y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf6z\n\t"
        "vmaddw.xyzw vf3, vf30, vf6w\n\t"
        "vmulax.xyzw ACC, vf27, vf7x\n\t"
        "vmadday.xyzw ACC, vf28, vf7y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf7z\n\t"
        "vmaddw.xyzw vf4, vf30, vf7w\n\t"
        "vmulax.xyzw ACC, vf27, vf2x\n\t"
        "vmadday.xyzw ACC, vf28, vf2y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf2z\n\t"
        "vmaddw.xyzw vf30, vf30, vf2w\n\t"
        "sqc2 vf30, 0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(matrix), "r"(weights)
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "memory");
}

static void _MinMaxSort(Vector4 *min, Vector4 *max) {
    __asm__ __volatile__(
        "lqc2 vf20, 0(%0)\n\t"
        "lqc2 vf21, 0(%1)\n\t"
        "vmini.xyz vf22, vf20, vf21\n\t"
        "vmax.xyz vf23, vf20, vf21\n\t"
        "sqc2 vf22, 0(%0)\n\t"
        "sqc2 vf23, 0(%1)\n\t"
        "nop"
        :
        : "r"(min), "r"(max)
        : "memory"
    );
}

void CLEAR_LAYOUT_MODEL(LayoutStore *arg0)
{
    StudioLight *light;

    xglStudioChange(0);
    xglStudioGetLight(&light);
    xglLightCalcMatrix(light);
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\t"
        "sq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\t"
        "sq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\t"
        "sq $2, 48(%0)"
        :
        : "r"(arg0->fields.lightDirection), "r"(&light->unmodeled_70[0x40])
        : "$2", "memory"
    );
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\t"
        "sq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\t"
        "sq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\t"
        "sq $2, 48(%0)"
        :
        : "r"(arg0->fields.lightColor), "r"(light->unmodeled_70)
        : "$2", "memory"
    );
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\t"
        "sq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\t"
        "sq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\t"
        "sq $2, 48(%0)"
        :
        : "r"(s_inGblPlace), "r"(arg0->fields.place)
        : "$2", "memory"
    );
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(&s_inGblPos), "r"(&arg0->slots[0x1F0 / 4])
        : "$2", "memory"
    );
    arg0->slots[0x88].f = 1.0f;
    arg0->slots[0x89].f = 2.0f;
    arg0->slots[0xB0].f = 1.0f;
    arg0->slots[0x90].f = 0.0f;
    xglMatrixUnit(arg0->fields.place);
    xglMatrixUnit(arg0->fields.inversePlace);
    arg0->slots[0x8F].f = 0.0f;
    arg0->slots[0x94].i &= 0xFF000000;
    arg0->fields.alpha = 0x44;
    arg0->slots[0x8A].f = 1.0f;
    arg0->slots[0xC0].f = 1.0f;
    arg0->slots[0xC1].f = 1.0f;
    arg0->slots[0xC2].f = 1.0f;
    arg0->slots[0xC3].f = 1.0f;
    arg0->slots[0xC4].f = 0.0f;
    arg0->slots[0xC5].f = 0.0f;
    arg0->slots[0xC6].f = 0.0f;
    arg0->slots[0xC7].f = 0.0f;
    s_inScale.z = 1.0f;
    s_inScale.y = 1.0f;
    s_inScale.x = 1.0f;
    arg0->slots[0x8E].i = 117;
    arg0->slots[0x8B].f = 1.0f;
    arg0->slots[0x83].f = 1.0f;
    arg0->slots[0x82].f = 1.0f;
    arg0->slots[0x81].f = 1.0f;
    arg0->slots[0x80].f = 1.0f;
    arg0->slots[0x8D].f = 0.0f;
    arg0->slots[0xA0].f = 0.0f;
    arg0->slots[0xB2].f = 0.0f;
    arg0->slots[0x8C].f = 0.0f;
    arg0->slots[0x9D].f = 1.0f;
    arg0->slots[0x9F].f = 0.0f;
    arg0->slots[0x9A].f = 0.0f;
    arg0->slots[0xAA].f = 0.0f;
    arg0->slots[0xAC].f = 0.0f;
    arg0->slots[0xAD].f = 0.0f;
    arg0->slots[0xAE].f = 0.0f;
    arg0->slots[0xAF].f = 1.0f;
    arg0->slots[0xBC].f = 1.0f;
    arg0->slots[0xBD].f = 1.0f;
    arg0->slots[0xBE].f = 1.0f;
    arg0->slots[0xBF].f = 1.0f;
    arg0->slots[0xB1].f = 0.0f;
    __asm__ __volatile__(
        "sq $0, 0(%0)\n\t"
        "sq $0, 16(%0)\n\t"
        "sq $0, 32(%0)\n\t"
        "sq $0, 48(%0)"
        :
        : "r"(&arg0->fields.pointColor[0])
        : "memory"
    );
    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(&arg0->slots[0x1F0 / 4]) : "memory");
    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(&arg0->slots[0x210 / 4]) : "memory");
}

static void CONSTRUCT_CIRCLR_SHADOW(void)
{
    int row;
    int column;
    struct NmlShadowTexture *shadow;
    float pixelScale = 14.0f;

    if (s_nCircleShadow != 0) {
        return;
    }
    row = 0;
    shadow = &s_aCircleShadow;

    for (; row < 64; row++) {
        float y = (float)row - 32.0f;
        for (column = 0; column < 64; column++) {
            float x = (float)column - 32.0f;
            float distance = __builtin_sqrtf(x * x + y * y);
            if (distance <= 1.0f) {
                distance = 0.0f;
            } else if (distance > 32.0f) {
                distance = 1.0f;
            } else {
                distance = (distance - 1.0f) / 31.0f;
            }
            shadow->pixels[row][column] =
                (unsigned char)((1.0f - distance) * pixelScale);
        }
    }

    {
        unsigned int *uploadHeader = s_aCircleShadowClut;

        uploadHeader[3] = 0x50000041;
        uploadHeader[4] = 0x8040;
        uploadHeader[0] = 0;
        uploadHeader[1] = 0;
        uploadHeader[2] = 0;
        uploadHeader[5] = 0x08000000;
        uploadHeader[6] = 0;
        uploadHeader[7] = 0;
        uploadHeader = s_aCircleShadow.uploadHeader;
        uploadHeader[4] = 0x8100;
        uploadHeader[3] = 0x50000101;
        uploadHeader[5] = 0x08000000;
        uploadHeader[0] = 0;
        uploadHeader[1] = 0;
        uploadHeader[2] = 0;
        uploadHeader[6] = 0;
        uploadHeader[7] = 0;
    }
    set_circle_shadow_ratio(0x1e);
    s_nCircleShadow = 1;
}

static void CONSTRUCT_ALPHA_GROUP(void)
{
    s_nAlphaGroup = 0;
    s_nNonAlphaGroup = 0;
}

static void INIT_ALPHA_GROUP(void)
{
    CONSTRUCT_ALPHA_GROUP();
}

static void FLUSH_ALPHA_GROUP(void)
{
    s_nAlphaGroup = 0;
    s_nNonAlphaGroup = 0;
}

static void CLEAR_PROREAL(int *proReal)
{
    proReal[0x1c0 / 4] = 0;
}

static void CONSTRUCT_BACK_BUFFER(void)
{
    s_inBackBuffer[0x10 / 4] = -1;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    s_inBackBuffer[0x08 / 4] = 0;
    s_inBackBuffer[0x0c / 4] = 0;
    s_inBackBuffer[0x18 / 4] = 0;
    s_inBackBuffer[0x1c / 4] = 0;
    s_inBackBuffer[0x20 / 4] = 0;
    s_inBackBuffer[0x24 / 4] = 0;
}

static void INIT_BACK_BUFFER(void)
{
    s_inBackBuffer[0x10 / 4] = -1;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    s_inBackBuffer[0x08 / 4] = 0;
    s_inBackBuffer[0x0c / 4] = 0;
    s_inBackBuffer[0x18 / 4] = 0;
    s_inBackBuffer[0x1c / 4] = 0;
    s_inBackBuffer[0x20 / 4] = 0;
}

static void FLUSH_BACK_BUFFER(void)
{
    s_inBackBuffer[0x00 / 4] = 0;
}

static void CONSTRUCT_PARENT_BUF(void)
{
    s_nParentBuf = 0;
}

static void INIT_PARENT_BUF(void)
{
    CONSTRUCT_PARENT_BUF();
}

static void FLUSH_PARENT_BUF(void)
{
    s_nParentBuf = 0;
}

static void CONSTRUCT_MAP_HANDLE(int *mapHandle)
{
    /*
     * The original writes word 0 and puts the word-1 store in the return's
     * delay slot; the two stores are independent, and in declaration order
     * cc1 schedules them the other way round (form 01, build/form-01,
     * first difference at 0x6e8: sw zero,4(a0) for sw zero,0(a0)).
     */
    mapHandle[1] = 0;
    mapHandle[0] = 0;
}

static void INIT_MAP_HANDLE(int *mapHandle)
{
    CONSTRUCT_MAP_HANDLE(mapHandle);
}

static void CLEAR_MAP_HANDLE(int *mapHandle)
{
    mapHandle[0] = 0;
}

static void FLUSH_MAP_HANDLE(int *mapHandle)
{
    mapHandle[1] = 0;
}

static void CONSTRUCT_FADE_CONTROL(FadeControl *control)
{
    control->frame = -1;
    control->duration = 1;
    control->skipRender = 0;
    control->dispose = 0;
    control->cancelFrames = 0;
    control->startDelay = 0;
    control->locked = 0;
}

static void INIT_FADE_CONTROL(FadeControl *control)
{
    CONSTRUCT_FADE_CONTROL(control);
}

static void CLEAR_MODEL_ENTRY(void)
{
    s_nDispVisible = -1;
    s_nToumeiNum = s_nShadowVec = s_nMapShadowParts = s_aTexName[0] = s_aMatName.text[0] = 0;
    s_fSortOffsetEntry = 0.0f;
    s_nParent = 0;
    s_nClip = 0;
    s_nShapeNum = 0;
    s_nMapClip = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", CONSTRUCT_MODELSYSTEM);

static void INIT_MODELSYSTEM(void)
{
    CONSTRUCT_MODELSYSTEM();
}

static void FLUSH_MODELSYSTEM(void)
{
    StudioCamera *camera;

    s_nPacketSignal = s_nPacketSignal + 1;
    s_nBlocks = 1;
    s_nModel = 0;
    s_nDirect = 0;
    s_nBlocksAlpha = 0;
    s_nBlocksAlphaLast = 0;
    s_nAnotherStudio = 0;
    s_nUseStealth = 0;
    s_nUseGnosys = 0;
    s_nUseZwrite = 0;
    s_nUseBackBuffer = 0;
    s_nMapAlphaEntry = 0;
    s_nFrameLockOff = 0;
    camera = xglStudioSelectGetActiveCamera(0);
    if (camera != 0) {
        __asm__ __volatile__(
            "lq $2, 0(%1)\n\t"
            "sq $2, 0(%0)"
            :
            : "r"(&s_inMainCameraPos), "r"(&camera->position)
            : "$2", "memory"
        );
        __asm__ __volatile__(
            "lq $2, 0(%1)\n\t"
            "sq $2, 0(%0)"
            :
            : "r"(&s_inMainCameraAng), "r"(&camera->rotation)
            : "$2", "memory"
        );
        s_nMainCameraWarp = 0;
    }
}

void nmlModelSetBackBufferClear(void)
{
    INIT_BACK_BUFFER();
}

void nmlModelSetFaceModel(int enabled)
{
    if (enabled != 0) {
        s_inLayout.slots[0x250 / 4].i |= 0x100000;
    }
}

void nmlModelSetHumanModel(int enabled)
{
    if (enabled != 0) {
        s_inLayout.slots[0x250 / 4].i |= 0x40;
    }
}

void nmlModelSetMapClip(int enabled)
{
    if (enabled != 0) {
        s_nMapClip = 1;
    }
}

void nmlModelSendRenderCancel(void)
{
}

int nmlModelGetRenderCancel(void)
{
    return s_nRenderCancelOld;
}

void nmlModelSendBackBufferSignal(void)
{
    s_nUseBackBuffer = 1;
}

int nmlModelIsBackBufferRequest(void)
{
    return D_0095BB3C[0];
}

void nmlModelSendPauseSignal(int pause)
{
    s_nPause = pause;
}

void nmlModelSendMenuStart(void)
{
    s_nMenu = 1;
}

void nmlModelSendMenuEnd(void)
{
    s_nMenu = 0;
}

int nmlModelGetMenuStatus(void)
{
    return s_nMenu;
}

void nmlModelSendSignalStartBattle(void)
{
    s_nFrameLockOff = 1;
}

void nmlModelSendSignalEventFinish(void)
{
    s_nFrameLockOff = 1;
}

void nmlModelSendPacketChangeSignal(void)
{
    s_nPacketSignal = 0;
    D_0095BB3C[0] = 0;
}

void nmlModelSendSignalMovieStart(void)
{
    INIT_BACK_BUFFER();
    INIT_FADE_CONTROL(&s_inFadeIn);
    INIT_FADE_CONTROL(&s_inFadeOut);
    INIT_FADE_CONTROL(&s_inActiveFadeIn);
    INIT_FADE_CONTROL(&s_inActiveFadeOut);
}

void nmlModelSetMpeg2CrossFadeTime(int time)
{
    D_0095BB44[0] = time;
}

static inline void NmlMovieRestoreSavedBuffer(void)
{
    nmlModelSetBackBuffer(D_0095BB44[0], 1, sRender.draw_buffer_base, 1);
    nmlModelSetFadeInCancel(30);
}

void nmlModelSendSignalMovieFinish(int mode)
{
    switch (mode - 1) {
    case 0:
        nmlModelSetBackBufferToBattle(sRender.draw_buffer_base);
        nmlModelSetFadeInCancel(30);
        nmlModelSetFadeOutCancel(30);
        break;
    case 1:
        nmlModelSetBackBuffer(0x26, 1, sRender.draw_buffer_base, 0);
        nmlModelSetFadeInCancel(30);
        break;
    case 4:
        nmlModelSetBackBuffer(0x26, 1, sRender.draw_buffer_base, 0);
        nmlModelSetFadeOutCancel(30);
        nmlModelSetFadeInCancel(30);
        break;
    case 2:
        nmlModelSetBackBuffer(3, 1, sRender.draw_buffer_base, 0);
        nmlModelSetFadeInCancel(30);
        break;
    case 3:
        NmlMovieRestoreSavedBuffer();
        break;
    }
}

int nmlModelSetRenderLevel(int level)
{
    int renderLevel = s_inLayout.slots[0x2b0 / 4].i | level;
    s_inLayout.slots[0x2b0 / 4].i = renderLevel;
    return renderLevel;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", fade_render);

void nmlFadePacketWrite(NmlPacket packet)
{
    fade_render(packet, &s_inActiveFadeIn);
    fade_render(packet, &s_inActiveFadeOut);
    fade_render(packet, &s_inFadeIn);
    fade_render(packet, &s_inFadeOut);
    s_nFadeDoit = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", fade_set);

void nmlModelSetFadeOutLock(void) {
    D_0095DB90[0] = 1;
}

void nmlModelSetFadeOutLockOff(void) {
    D_0095DB90[0] = 0;
}

void nmlModelSetActiveFadeInCancel(int frames) {
    D_0095DC48[0] = (frames < 0) ? 0 : frames;
}

void nmlModelSetFadeInCancel(int frames) {
    D_0095DBC8[0] = (frames < 0) ? 0 : frames;
}

void nmlModelSetActiveFadeOutCancel(int frames) {
    D_0095DC08[0] = (frames < 0) ? 0 : frames;
}

void nmlModelSetFadeOutCancel(int frames) {
    D_0095DB88[0] = (frames < 0) ? 0 : frames;
}

void nmlModelFadeDoit(void)
{
    if (s_inFadeIn.cancelFrames != 0) {
        s_inFadeIn.cancelFrames--;
    }
    if (s_inFadeOut.cancelFrames != 0) {
        s_inFadeOut.cancelFrames--;
    }
    if (s_inActiveFadeIn.cancelFrames != 0) {
        s_inActiveFadeIn.cancelFrames--;
    }
    if (s_inActiveFadeOut.cancelFrames != 0) {
        s_inActiveFadeOut.cancelFrames--;
    }
}

void nmlModelSetFadeDoit(void)
{
    s_nFadeDoit = 1;
}

void nmlModelSetFadeOutDispose(void) {
    D_0095DB84[0] = 1;
}

void nmlModelSetFadeInDispose(void) {
    D_0095DBC4[0] = 1;
}

void nmlModelSetFadeOut(int duration, int mode) {
    fade_set(&s_inFadeOut, GameLoopState.color, GameLoopState.level, duration + 2, 0, mode, 0);
}

void nmlModelSetFadeIn(int duration, int mode)
{
    float zeroValue;

    fade_set(&s_inFadeIn, GameLoopState.previousFade.color,
             GameLoopState.previousFadeMode, duration, 1, mode, 0);
    zeroValue = 0.0f;
    GameLoopState.level = 0;
    GameLoopState.previousFade.color[2] = zeroValue;
    GameLoopState.previousFade.color[1] = zeroValue;
    GameLoopState.previousFade.color[0] = zeroValue;
    GameLoopState.previousFade.duration = 30.0f;
    GameLoopState.previousFadeMode = 0;
    GameLoopState.fade.color[2] = zeroValue;
    GameLoopState.fade.color[1] = zeroValue;
    GameLoopState.fade.duration = 30.0f;
    GameLoopState.fade.color[0] = zeroValue;
}

void nmlModelSetFadeInInterrupt(int frames, float red, float green, float blue)
{
    s_inFadeIn.level = 0;
    s_inFadeIn.duration = frames;
    s_inFadeIn.frame = frames;
    s_inFadeIn.color[0] = (int) (red * 255.0f);
    s_inFadeIn.color[1] = (int) (green * 255.0f);
    s_inFadeIn.color[2] = (int) (blue * 255.0f);
}

void nmlModelSetActiveFadeOut(int duration, int packedColor, int startDelay)
{
    float color[3];
    u32 rgb = packedColor;

    if (s_inActiveFadeOut.frame < 0) {
        color[0] = (float)(rgb & 0xff) * 0.0078125f;
        color[1] = (float)((rgb >> 8) & 0xff) * 0.0078125f;
        color[2] = (float)((rgb >> 16) & 0xff) * 0.0078125f;
        fade_set(&s_inActiveFadeOut, color, 0, duration + 2, 0, 1, startDelay);
    }
}

void nmlModelSetActiveFadeIn(int duration, int packedColor, int startDelay)
{
    float color[3];
    u32 rgb = packedColor;

    color[0] = (float)(rgb & 0xff) * 0.0078125f;
    color[1] = (float)((rgb >> 8) & 0xff) * 0.0078125f;
    color[2] = (float)((rgb >> 16) & 0xff) * 0.0078125f;
    fade_set(&s_inActiveFadeIn, color, 0, duration, 1, 1, startDelay);
}

int nmlModelGetFadeLevel(void)
{
    int active = 1;

    if (s_inFadeIn.frame < 0) {
        active = 0;
    }
    if (s_inFadeOut.frame >= 0) {
        active = 1;
    }
    if (s_inActiveFadeIn.frame >= 0) {
        active = 1;
    }
    if (s_inActiveFadeOut.frame >= 0) {
        active = 1;
    }
    return active;
}

int nmlModelAskNonLinearCamera(void)
{
    StudioCamera *camera;
    Vector4 angleDelta;
    float length;
    float angleY;

    if (s_nMainCameraWarp == 0) {
        camera = xglStudioSelectGetActiveCamera(0);
        if (camera != 0) {
            s_nMainCameraWarp = 1;
            length = xglPointLength(&s_inMainCameraPos, &camera->position);
            if (length >= 1.0f) {
                s_nMainCameraWarp = 2;
            } else {
                __asm__ __volatile__(
                    "lqc2 vf3, 0(%1)\n\t"
                    "lqc2 vf2, 0(%2)\n\t"
                    "vsub.xyz vf2, vf2, vf3\n\t"
                    "sqc2 vf2, 0(%0)"
                    :
                    : "r"(&angleDelta), "r"(&camera->rotation), "r"(&s_inMainCameraAng)
                    : "memory");
                angleY = __builtin_fabsf(angleDelta.y);
                if (D_004D88D8 < angleY && angleY < D_004D88DC) {
                    s_nMainCameraWarp = 2;
                }
            }
        }
    }
    return s_nMainCameraWarp == 2;
}

void nmlModelGetGblPosition(Vector4 *position)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(position), "r"(&s_inGblPos)
        : "$2", "memory"
    );
    position->w = 1.0f;
}

/* Battle and event-skip configure the same special back-buffer request:
 * a 0x28-frame time, the control state and the owner, written as one group.
 * The do/while (0) group is what keeps the original store order: without it
 * cc1 schedules the +0x20 clear ahead of the owner and +0x04 stores. */
void nmlModelSetBackBufferToBattle(int ownerIndex)
{
    do {
        s_inBackBuffer[6] = 0x28;
        s_inBackBuffer[5] = 1;
        s_inBackBuffer[0] = 1;
        s_inBackBuffer[4] = ownerIndex;
        s_inBackBuffer[1] = 0;
    } while (0);
    s_inBackBuffer[8] = 0;
}

void nmlModelSetBackBufferToEventSkip(int ownerIndex)
{
    do {
        s_inBackBuffer[6] = 0x28;
        s_inBackBuffer[5] = 1;
        s_inBackBuffer[0] = 1;
        s_inBackBuffer[4] = ownerIndex;
        s_inBackBuffer[1] = 0;
    } while (0);
    s_inBackBuffer[8] = 0;
}

void nmlModelSetBackBuffer(int modelId, int duration, int ownerIndex, int value)
{
    s_inBackBuffer[8 / 4] = modelId;
    s_inBackBuffer[0x10 / 4] = ownerIndex;
    s_inBackBuffer[0x20 / 4] = value;
    s_inBackBuffer[0xC / 4] = duration;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    if (duration <= 0) {
        s_inBackBuffer[0xC / 4] = 1;
    }
}

void nmlModelSetEffectWrite(int enabled)
{
    s_nEffectWrite = enabled;
}

void nmlModelUseSubWindow(unsigned int index, int enabled) {
    if (index < 4U) {
        g_aSubWindow[index] = enabled;
    }
}

void nmlModelSetWindow(u32 window) {
    if (window < 4U) {
        s_inLayout.slots[0x27c / 4].i = window;
    }
}

void nmlModelSetSortOffset(float offset)
{
    s_fSortOffsetEntry = offset;
}

void nmlModelSetGlobalPointLight(int index, const Vector4 *position, const Vector4 *color)
{
    if (index < 3U) {
        NmlPointQuadword *point = &s_inGblPointP[index];

        __asm__ __volatile__(
            "lq $2, 0(%1)\n\t"
            "sq $2, 0(%0)"
            :
            : "r"(point), "r"(position)
            : "$2", "memory"
        );
        xglVectorScaleXYZ(128.0f, &s_inGblPointC[index], color);
        s_inLayout.fields.render_status |= 0x02000000;
    }
}

void nmlModelSetGlobalPointLightCol(int index, const Vector4 *color)
{
    if (index < 3U) {
        xglVectorScaleXYZ(128.0f, &s_inGblPointC[index], color);
        s_inLayout.fields.render_status |= 0x02000000;
    }
}

void nmlModelSetGlobalPointLightPos(unsigned int index, const NmlPointQuadword *position)
{
    if (index < 3) {
        NmlPointQuadword *destination = &s_inGblPointP[index];
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            : : "r" (destination), "r" (position) : "$2", "memory");
        s_inLayout.fields.render_status |= 0x02000000;
    }
}

void nmlModelSetGlobalPointLightReset(void)
{
    s_inLayout.fields.render_status &= 0xfdffffffu;
    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(s_inGblPointC) : "memory");
    __asm__ __volatile__("sq $0, 16(%0)" : : "r"(s_inGblPointC) : "memory");
    __asm__ __volatile__("sq $0, 32(%0)" : : "r"(s_inGblPointC) : "memory");
    __asm__ __volatile__("sq $0, 48(%0)" : : "r"(s_inGblPointC) : "memory");
}

void nmlModelSetPointLight(unsigned int index, const Vector4 *position, const Vector4 *color)
{
    if (index < 3) {
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            : : "r" (&s_inLayout.fields.pointPosition[index]), "r" (position)
            : "$2", "memory");
        xglVectorScaleXYZ(128.0f, &s_inLayout.fields.pointColor[index], color);
        s_inLayout.fields.render_status |= 0x1000;
    }
}

void nmlModelSetSpecularOff(int disableSpecular) {
    if (disableSpecular != 0) {
        s_inLayout.slots[0x2b0 / 4].i |= 2;
    }
}

void nmlModelSetShadowMapId(void)
{
}

void nmlModelSetShadowHeight(float height)
{
    s_inLayout.slots[0x2ac / 4].f = height;
    s_inLayout.slots[0x2a8 / 4].i = 1;
}

void nmlModelSetShadowVec(const Vector4 *shadowVector)
{
    __asm__ __volatile__(
        "lq $2,0(%0)\n\t"
        "sq $2,0(%1)"
        :
        : "r"(shadowVector), "r"(&s_inShadowVec)
        : "$2", "memory"
    );
    s_nShadowVec = 1;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelSetTexMap);

void nmlModelSetProreal(NmlProRealState *input)
{
    u64 transformOrigin[2];
    Vector4 scale;
    Matrix4 scaledTransform;
    int textureStartX;
    int textureEndX;
    int textureStartY;
    int textureEndY;
    u64 textureXDirection;
    u64 textureYDirection;
    int screenWidth;
    int screenHeight;
    int textureMode;

    screenWidth = 0x400;
    screenHeight = 0x200;
    scale.x = (float)input->fields.unmodeled_108 * 0.015625f * input->fields.unmodeled_120;
    scale.y = (float)input->fields.unmodeled_10c * 0.015625f * input->fields.unmodeled_124;
    transformOrigin[0] = D_004D2820.word0;
    transformOrigin[1] = D_004D2820.word8;
    scale.w = 1.0f;
    scale.z = 1.0f;
    xglMatrixScale(scaledTransform, (const float (*)[4])input->fields.matrix_000, &scale.x);
    xglVectorMulMat(&input->fields.transformOrigin, (const float (*)[4])scaledTransform,
                    (const Vector4 *)transformOrigin);

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n"
        "lqc2 vf2, 0(%1)\n"
        "vsub.xyz vf2, vf2, vf3\n"
        "sqc2 vf2, 0(%2)\n"
        :
        : "r"(&scaledTransform[3]), "r"(&input->fields.transformOrigin),
          "r"(&input->fields.unmodeled_160)
        : "memory");

    xglMatrixInverse(input->fields.inverseTransform, (const float (*)[4])scaledTransform);
    xglVectorNormal(&input->fields.unmodeled_160, &input->fields.unmodeled_160);

    textureMode = input->fields.unmodeled_114;
    switch (textureMode) {
    case 20: {
        int textureBase = (int)input->fields.unmodeled_116 << 5;
        input->fields.textureWord =
            (u64)(textureBase | 0x29440000) |
            ((u64)(textureBase + input->fields.unmodeled_118) << 37) |
            0x2000000640000000ULL;
        break;
    }
    case 19: {
        int textureBase = (int)input->fields.unmodeled_116 << 5;
        input->fields.textureWord =
            (u64)(textureBase | 0x29340000) |
            ((u64)(textureBase + input->fields.unmodeled_118) << 37) |
            0x2000000640000000ULL;
        break;
    }
    case 0:
        screenWidth = 0x200;
        input->fields.textureWord = (u64)((int)input->fields.unmodeled_116 << 5) |
                               0x24020000ULL | 0x2000000640000000ULL;
        break;
    }

    textureXDirection = input->fields.unmodeled_110 != 0 ? 3 : 2;
    textureYDirection = input->fields.unmodeled_112 != 0 ? 3 : 2;
    textureStartX = input->fields.unmodeled_110 != 0 ? input->fields.unmodeled_108 - 1 : input->fields.unmodeled_100;
    textureEndX = input->fields.unmodeled_110 != 0 ? input->fields.unmodeled_100 :
                  input->fields.unmodeled_100 + input->fields.unmodeled_108 - 1;
    textureStartY = input->fields.unmodeled_112 != 0 ? input->fields.unmodeled_10c - 1 : input->fields.unmodeled_104;
    textureEndY = input->fields.unmodeled_112 != 0 ? input->fields.unmodeled_104 :
                  input->fields.unmodeled_104 + input->fields.unmodeled_10c - 1;

    input->fields.clampWord = (u64)textureXDirection |
        ((u64)textureYDirection << 2) |
        ((u64)textureStartX << 4) |
        ((u64)textureEndX << 14) |
        ((u64)textureStartY << 24) |
        ((u64)textureEndY << 34);
    input->fields.textureUScale = (float)input->fields.unmodeled_108 / (float)screenWidth;
    input->fields.textureUOffset = (float)input->fields.unmodeled_100 / (float)screenWidth +
                            input->fields.textureUScale * 0.5f;
    input->fields.textureVScale = -(float)input->fields.unmodeled_10c / (float)screenHeight;
    input->fields.textureVOffset = (float)input->fields.unmodeled_104 / (float)screenHeight -
                            input->fields.textureVScale * 0.5f;

    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&input->fields.unmodeled_0c0[0]),
                             "r"(&s_inProReal[0].fields.unmodeled_170[0])
                         : "$2", "memory");
    __asm__ __volatile__("sq $0, 0(%0)"
                         : : "r"(&input->fields.unmodeled_0c0[1]) : "memory");
    __asm__ __volatile__("sq $0, 0(%0)"
                         : : "r"(&input->fields.unmodeled_0c0[2]) : "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&input->fields.unmodeled_0c0[3]),
                             "r"(&s_inProReal[0].fields.unmodeled_170[1])
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&input->fields.unmodeled_080[0]),
                             "r"(&s_inProReal[0].fields.unmodeled_160)
                         : "$2", "memory");
    input->fields.unmodeled_11c = 0;
    s_inLayout.fields.render_status |= 0x800;
    s_inProReal[0] = *input;
}

int nmlModelSetTexProreal(void) {
    int renderStatus = s_inLayout.fields.render_status | 0x800;
    s_inLayout.fields.render_status = renderStatus;
    return renderStatus;
}

int nmlModelSetAxisSymmetry(float symmetry) {
    int flags;

    s_inLayout.slots[0x230 / 4].f = symmetry * 0.0f;
    flags = s_inLayout.slots[0x230 / 4].i | 1;
    s_inLayout.slots[0x230 / 4].i = flags;
    return flags;
}

void nmlModelSetParent(int parent)
{
    s_nParent = parent;
}

void nmlModelSetRenderStatus(int status)
{
    if (status == 0x100) {
        s_inLayout.fields.render_status |= 0x100;
    } else if (status == 0x200) {
        s_inLayout.fields.render_status |= 0x200;
    }
}

void nmlModelSetShapeId(int shapeId, float weight)
{
    extern int s_aShapeId[];
    extern float s_aShapeWeight[];

    if (s_nShapeNum < 32) {
        s_aShapeId[s_nShapeNum] = shapeId;
        s_aShapeWeight[s_nShapeNum++] = weight;
    }
}

void nmlModelSetToumeiParts(int partId, float alpha)
{
    extern int s_aToumeiId[];
    extern float s_aToumei[];

    if (s_nToumeiNum < 16) {
        s_aToumeiId[s_nToumeiNum] = partId;
        s_aToumei[s_nToumeiNum++] = alpha;
    }
}

void nmlModelSetFilter(int filter, float start, float end)
{
    s_inLayout.slots[0x234 / 4].i = filter;
    if (start < 0.0f) {
        start = 0.0f;
    }
    if (start > 128.0f) {
        start = 128.0f;
    }
    s_inLayout.slots[0x1e0 / 4].f = start;
    s_inLayout.slots[0x1e4 / 4].f = end;
    if ((filter & 2) != 0) {
        s_nUseStealth = 1;
        if (GameLoopState.stealthMode == 2 && SCRIPT_getCfTime() < 2) {
            s_nUseStealth = 0;
        }
    }
}

void nmlModelSetPixelAlpha(int alpha)
{
    if (alpha <= 0) {
        alpha = 1;
    }
    if (alpha >= 128) {
        alpha = 127;
    }
    s_inLayout.fields.pixelAlpha = alpha;
}

void nmlModelSetPartsPixelAlpha(int partId, int alpha)
{
    if (alpha <= 0) {
        alpha = 1;
    }
    if (alpha >= 128) {
        alpha = 127;
    }
    if (s_inLayout.fields.partPixelAlphaCount < 4) {
        s_inLayout.fields.partPixelAlpha[s_inLayout.fields.partPixelAlphaCount].alpha = alpha;
        s_inLayout.fields.partPixelAlpha[s_inLayout.fields.partPixelAlphaCount].partId = partId;
        ++s_inLayout.fields.partPixelAlphaCount;
    }
}

void nmlModelSetScale(const Vector4 *scale)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(&s_inScale), "r"(scale)
        : "$2", "memory"
    );
    if (s_inScale.x < s_inScale.y) {
        s_inScale.x = s_inScale.y;
    }
    if (s_inScale.x < s_inScale.z) {
        s_inScale.x = s_inScale.z;
    }
    if (s_inScale.x < 1.0f) {
        s_inScale.x = 1.0f;
    }
    s_inLayout.slots[0x22c / 4].f = s_inScale.x;
}

void nmlModelSetClip(int clip)
{
    s_nClip = clip;
}

void nmlModelSetAlpha(u64 alpha)
{
    s_inLayout.fields.alpha = alpha;
}

void nmlModelSetStencil(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_STENCIL;
    }
}

void nmlModelSetZwrite(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_ZWRITE;
    }
}

void nmlModelSetTransparency(float transparency)
{
    if (transparency < 0.0f) {
        transparency = 0.0f;
    }
    if (1.0f < transparency) {
        transparency = 1.0f;
    }
    s_inLayout.slots[0x220 / 4].f = transparency;
    s_inLayout.slots[0x2c0 / 4].f = transparency;
}

void nmlModelSetReflTransparency(float transparency)
{
    if (transparency < 0.0f) {
        transparency = 0.0f;
    }
    if (1.0f < transparency) {
        transparency = 1.0f;
    }
    s_inLayout.slots[0x224 / 4].f = transparency;
}

void nmlModelSetToumei(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_TOUMEI;
    }
}

void nmlModelSetLight(const Matrix4 *direction, const Matrix4 *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n" "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n" "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n" "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n" "sq $2, 48(%0)\n"
        : : "r" (&s_inLayout.fields.lightDirection), "r" (direction)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n" "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n" "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n" "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n" "sq $2, 48(%0)\n"
        : : "r" (&s_inLayout.fields.lightColor), "r" (color)
        : "$2", "memory");
    s_inLayout.fields.render_status |= 0x10;
}

void nmlModelSetTexture(const char *texture)
{
    /* Accept only a non-null, in-range, qword-aligned pointer whose header
     * carries the "XTX" texture-resource signature: texture[0..2] ==
     * 'X','T','X' (texture[2] is compared against texture[0], not a literal,
     * per the original disassembly). */
    if (texture == 0 ||
        (u32)texture - NML_TEXTURE_RAM_BASE > NML_TEXTURE_RAM_MAX_OFFSET ||
        ((u32)texture & NML_TEXTURE_ALIGN_MASK) != 0 ||
        texture[0] != 'X' || texture[1] != 'T' || texture[2] != texture[0]) {
        return;
    }
    s_inLayout.fields.texture = texture;
}

void nmlModelSetTexfunc(int texfunc)
{
    s_inLayout.fields.texfunc = texfunc;
}

/*
 * Stores the current model matrix pointer in the pointer-width s_inLayout
 * word at byte offset 0x23c (slot 143 of the shared LayoutStore view, which
 * gives that slot no pointer member).
 */
void nmlModelSetMatrix(void *matrix)
{
    *(void **)&s_inLayout.slots[0x23c / 4] = matrix;
}

void nmlModelSetPlace(const Matrix4 *place)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n" "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n" "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n" "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n" "sq $2, 48(%0)\n"
        : : "r" (&s_inLayout.fields.place), "r" (place)
        : "$2", "memory");
    xglMatrixInverse(s_inLayout.fields.inversePlace, *place);
    s_inLayout.fields.render_status |= 1;
}

void nmlModelSetTexOffset(const char *name, float offsetU, float offsetV)
{
    __builtin_memcpy(s_aTexName, name, sizeof(s_aTexName));
    s_aTexName[31] = 0;
    s_aTexOffset[0] = offsetU;
    s_aTexOffset[1] = offsetV;
}

void nmlModelSetMatOffset(const NmlMaterialName *name, const Vector4 *color)
{
    s_aMatName = *name;
    s_aMatName.text[31] = 0;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n" "sq $2, 0(%0)\n"
        : : "r" (&s_inMulColor), "r" (color) : "$2", "memory");
}

void nmlModelSetMulColor(const Vector4 *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        : : "r" (&s_inLayout.fields.multiplyColor), "r" (color)
        : "$2", "memory");
}

void nmlModelSetFogCol(const Vector4 *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n" "sq $2, 0(%0)\n"
        : : "r" (&s_inLayout.fields.fogColor), "r" (color)
        : "$2", "memory");
    s_inLayout.fields.render_status |= NML_RENDER_FOG;
}

static void nmlModelFogPara(Vector4 *fog, float fogNear, float fogFar, float fogMin, float fogMax)
{
    float shadeMax;
    float shadeMin;

    shadeMax = (1.0f - fogMax) * 255.0f;
    shadeMin = (1.0f - fogMin) * 255.0f;
    if (shadeMax < 0.0f) {
        shadeMax = 0.0f;
    }
    if (shadeMax > 255.0f) {
        shadeMax = 255.0f;
    }
    if (shadeMin < 0.0f) {
        shadeMin = 0.0f;
    }
    if (shadeMin > 255.0f) {
        shadeMin = 255.0f;
    }
    fog->x = shadeMax;
    fog->y = shadeMin;
    fog->w = (fogFar * fogNear * (shadeMin - shadeMax)) / (fogFar - fogNear);
    fog->z = ((((shadeMax - shadeMin) * (fogFar + fogNear)) / (fogFar - fogNear)) + (shadeMax + shadeMin)) * 0.5f;
}

void nmlModelSetFogDist(float fogNear, float fogFar, float fogMin, float fogMax)
{
    nmlModelFogPara(&s_inLayout.fields.fogPara, fogNear, fogFar, fogMin, fogMax);
    s_inLayout.fields.render_status |= NML_RENDER_FOG;
}

void nmlModelSetGlobalFogCol(const NmlPointQuadword *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        : : "r" (s_inGblFogCol), "r" (color) : "$2", "memory");
    s_inLayout.fields.render_status |= 0x01000000;
}

void nmlModelSetGlobalFogDist(float fogNear, float fogFar, float fogMin, float fogMax)
{
    nmlModelFogPara(&s_inGblFogPara, fogNear, fogFar, fogMin, fogMax);
    s_inLayout.fields.render_status |= 0x01000000;
}

unsigned int nmlModelGetFogPara(const Vector4 *position)
{
    const Vector4 *fog;
    StudioCamera *camera;
    Vector4 projected;
    unsigned int result;

    result = 0xff;
    fog = &s_inGblFogPara;
    if (s_inLayout.fields.render_status & NML_RENDER_FOG) {
        fog = &s_inLayout.fields.fogPara;
    }
    camera = xglStudioSelectGetActiveCamera(LAYOUT_S32(0x27c));
    if (camera != 0) {
        _CurSetMatrix(camera->viewMatrix);
        _CurRotTransPersFog(&projected, position, fog);
        result = (unsigned int) projected.w;
    }
    return result;
}

unsigned int nmlModelGetFogParaStudio(const Vector4 *position, int studio)
{
    const Vector4 *fog;
    StudioCamera *camera;
    Vector4 projected;
    unsigned int result;

    result = 0xff;
    fog = &s_inGblFogPara;
    if ((s_inLayout.fields.render_status & 2) != 0) {
        fog = &s_inLayout.fields.fogPara;
    }
    camera = xglStudioSelectGetActiveCamera(studio);
    if (camera != 0) {
        _CurSetMatrix(camera->viewMatrix);
        _CurRotTransPersFog(&projected, position, fog);
        result = (unsigned int) projected.w;
    }
    return result;
}

int nmlModelSetGlobalFogReset(void) {
    int renderStatus = s_inLayout.fields.render_status & 0xfeffffff;
    s_inLayout.fields.render_status = renderStatus;
    return renderStatus;
}

void nmlModelSetFogCancel(void) {
    int renderStatus = s_inLayout.fields.render_status | 0x2000;
    s_inLayout.fields.render_status = renderStatus;
}

void nmlModelInitPartsVisible(NmlModel *model, int visible)
{
    int *blockOffsets;
    int blockIndex;
    NmlModelBlock *block;

    if (nmlModelLexDataCheck(model) != 0) {
        return;
    }
    blockOffsets = model->blockOffsets;
    block = (NmlModelBlock *)((char *)model + blockOffsets[0]);
    for (blockIndex = 0; blockIndex < model->blockCount; blockIndex++) {
        block = (NmlModelBlock *)((char *)model + blockOffsets[blockIndex]);
        if (visible != 0) {
            block->flags = block->flags & ~NML_BLOCK_HIDDEN;
        } else {
            block->flags |= NML_BLOCK_HIDDEN;
        }
    }
}

void nmlModelSetMapEntry(void)
{
    int *mapHandle = s_inMapHandle;

    s_inLayout.fields.render_status |= NML_RENDER_MAP_ENTRY;
    mapHandle[0] = 1;
}

void nmlModelSetShadowMapEntry(void) {
    s_inLayout.fields.render_status |= 0x10000;
}

void nmlModelSetMapShadowParts(int partId)
{
    if (s_nMapShadowParts < 8) {
        s_inLayout.fields.mapShadowParts[s_nMapShadowParts++] = partId;
    }
}

void nmlModelSetMapLastEntry(void *map, int blockId)
{
    extern void *s_pMapLast;
    extern int s_aMapLast[];

    if (s_nMapLast < 12) {
        s_pMapLast = map;
        s_aMapLast[s_nMapLast++] = blockId;
    }
}

void nmlModelSetMapLastInit(void)
{
    s_nMapLast = 0;
}

void nmlModelSetPartsVisible(NmlModel *model, int partId, int visible)
{
    int *blockOffsets;
    int blockIndex;
    NmlModelBlock *block;
    u32 hideBits;
    u32 keepBits;

    if (nmlModelLexDataCheck(model) != 0 || partId >= model->partCount) {
        return;
    }
    blockOffsets = model->blockOffsets;
    if ((((NmlModelBlock *)((char *)model + blockOffsets[0]))->materialFlags & NML_BLOCK_MATERIAL_NAMED_PARTS) != 0) {
        hideBits = NML_BLOCK_HIDDEN;
        keepBits = 0xffffffffu;
        if (visible != 0) {
            hideBits = 0;
            keepBits = ~NML_BLOCK_HIDDEN;
        }
        for (blockIndex = 0; blockIndex < model->blockCount; blockIndex++) {
            block = (NmlModelBlock *)((char *)model + blockOffsets[blockIndex]);
            if (block->partId == partId) {
                block->flags = (block->flags | hideBits) & keepBits;
            }
        }
    } else if (visible == 0 && s_nDispVisible < 1023) {
        s_aDispVisible[++s_nDispVisible] = (unsigned char) partId;
    }
}

void nmlModelSetNameVisible(NmlModel *model, const char *name, int visible, int match)
{
    int *block_offsets;
    NmlModelBlock *first_block;
    NmlModelBlock *block;
    u32 set_flags;
    u32 keep_flags;
    int block_index;

    if (nmlModelLexDataCheck(model) != 0) {
        return;
    }

    block_offsets = model->blockOffsets;
    first_block = (NmlModelBlock *)((char *)model + block_offsets[0]);
    if (!(first_block->materialFlags & NML_BLOCK_MATERIAL_NAMED_PARTS)) {
        return;
    }

    set_flags = NML_BLOCK_HIDDEN;
    keep_flags = ~0u;
    if (visible) {
        set_flags = 0;
        keep_flags = ~NML_BLOCK_HIDDEN;
    }

    switch (match) {
    case NML_NAME_MATCH_PREFIX:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (memcmp(block->name, name, strlen(name)) == 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    case NML_NAME_MATCH_EXACT:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (strcmp(block->name, name) == 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    case NML_NAME_MATCH_SUBSTRING:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (strstr(block->name, name) != 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    }
}

void nmlModelDirectSend(int mode, const void *data, int count)
{
    NmlDirectRecord *entry;

    if (s_nPacketSignal != 0) {
        return;
    }

    if (s_nDirect >= 5500) {
        return;
    }

    entry = &s_aDirect[s_nDirect];
    entry->count = (s16)count;
    entry->attribute = 1;
    switch (mode) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 9:
        case 11:
            entry->data = nmlPacketSetAttributeData16N(data, count);
            break;
        case 2:
        case 4:
        case 6:
        case 8:
        case 10:
        case 12:
            entry->data = (void *)data;
            break;
    }
    switch (mode) {
        case 1:
        case 2:
            entry->attribute = 1;
            break;
        case 3:
        case 4:
            entry->attribute = 2;
            break;
        case 5:
        case 6:
            entry->attribute = 3;
            break;
        case 7:
        case 8:
            entry->attribute = 4;
            break;
        case 9:
        case 10:
            entry->attribute = 5;
            break;
        case 11:
        case 12:
            entry->attribute = 6;
            break;
    }
    s_nDirect++;
}

static void nmlModelDirectXtxSub(void *data, int modelId, int nextModelId)
{
    NmlDirectXtxBlock *block;
    NmlDirectXtxEntry *entry;
    unsigned int low;
    unsigned int high;
    int index;
    u64 env4;
    u64 env8;
    u64 *env;

    block = (NmlDirectXtxBlock *) data;
    entry = (NmlDirectXtxEntry *) ((char *) data + block->entriesOffset);
    for (index = 0; index < block->entryCount; index++) {
        env = TestEnv_0_004A9170;
        low = entry->packed & 0xffffu;
        high = entry->packed >> 16;
        if (entry->packed == low) {
            high = (unsigned int) (((int) (low + 0x3f)) >> 6);
        }
        env4 = ((u64) ((entry->word8 >> 6) + 0x3800) << 32) | ((u64) high << 48);
        __builtin_memcpy(&env[4], &env4, sizeof(env4));
        env8 = low | ((u64) entry->word4 << 32);
        __builtin_memcpy(&env[8], &env8, sizeof(env8));
        nmlModelDirectSend(modelId, env, 6);
        nmlModelDirectSend(nextModelId, (char *)data + entry->dataOffset, entry->size);
        entry++;
    }
}

void nmlModelDirectSendXtx(int modelId, void *data)
{
    const signed char *bytes = data;

    if (s_nPacketSignal == 0 &&
        (u32)((u32)data - NML_TEXTURE_RAM_BASE) <= NML_TEXTURE_RAM_MAX_OFFSET &&
        ((u32)data & 0xF) == 0 &&
        bytes[0] == 'X' && bytes[1] == 'T' && bytes[2] == bytes[0]) {
        nmlModelDirectXtxSub(data, modelId, modelId + 1);
    }
}

static void set_group_status(LayoutStore *layout, u32 *status)
{
    *status = 0;
    if (layout->fields.render_status & NML_RENDER_FOG) {
        *status = 0x4;
    }
    if (layout->fields.render_status & NML_RENDER_STENCIL) {
        *status |= 0x2;
    }
    if (layout->fields.render_status & 0x4000) {
        *status |= 0x1;
    }
    if (layout->fields.render_status & 0x800) {
        *status |= 0x8;
    }
    if (layout->fields.render_status & 0x10000) {
        *status |= 0x10;
    }
    if (layout->fields.render_status & NML_RENDER_TOUMEI) {
        *status |= 0x20;
    }
    if (layout->fields.render_status & 0x20000) {
        *status |= 0x40;
    }
    if (layout->fields.render_status & 0x40000) {
        *status |= 0x80;
    }
    if (layout->fields.render_status & 0x200000) {
        *status |= 0x100;
    }
    if (layout->fields.render_status & 0x400000) {
        *status |= 0x200;
    }
    if (layout->fields.render_status & 0x800000) {
        *status |= 0x800;
    }
    if (layout->fields.render_status & NML_RENDER_ZWRITE) {
        *status |= 0x400;
        s_nUseZwrite = 1;
    }
}

static void AlphaGroupSortEntry(const Vector4 *position)
{
    StudioCamera *camera = xglStudioSelectGetActiveCamera(s_inLayout.fields.renderLevel);
    float distance = 0.0f;
    if (camera != 0) {
        distance = _VectorLengthSQ(&camera->position, position);
    }
    s_aAlphaGroup[s_nAlphaGroup].sortDistance = distance + s_fSortOffset;
}

static void AlphaGroupLastEntry(void)
{
    int index = 0;

    if (s_nBlocksAlphaLast > 0) {
        for (index = 0; index < s_nBlocksAlphaLast; ++index) {
            int capacityCount = s_nBlocksAlpha;
            if (capacityCount + s_nBlocks < 0xFFF) {
                int groupIndex = s_nAlphaGroup;
                if (groupIndex < 0x1FF) {
                    int *alphaBlockCount = &s_nBlocksAlpha;
                    LayoutStore *modelLayout;
                    int appendedBlockCount;

                    __builtin_memcpy(&s_apBlock[0xFFF - capacityCount],
                                     &s_apBlockLast[index], sizeof(s_apBlockLast[index]));
                    s_aBlockLayout[0xFFF - *alphaBlockCount] = s_aBlockLayoutLast[index];
                    __builtin_memcpy(&modelLayout,
                                     &s_apModelLayout[s_aBlockLayoutLast[index]],
                                     sizeof(modelLayout));
                    s_aAlphaGroup[groupIndex].firstBlock = (short)(0xFFF - *alphaBlockCount);
                    s_aAlphaGroup[groupIndex].blockCount = 1;
                    appendedBlockCount = *alphaBlockCount + 1;
                    __builtin_memcpy(alphaBlockCount, &appendedBlockCount,
                                     sizeof(appendedBlockCount));
                    __builtin_memset(&s_aAlphaGroup[groupIndex].sortDistance, 0,
                                     sizeof(s_aAlphaGroup[groupIndex].sortDistance));
                    set_group_status(modelLayout, &s_aAlphaGroup[groupIndex].status);
                    ++s_nAlphaGroup;
                }
            }
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", AlphaGroupSort);

static int is_shadow_map_parts(const LayoutStore *layout, int partId)
{
    int found = 0;
    int count = layout->fields.mapShadowPartCount;
    if (count != 0) {
        int index;
        for (index = 0; index < count; ++index) {
            if (layout->fields.mapShadowParts[index] == partId) {
                found = 1;
                break;
            }
        }
    }
    return found;
}

static int is_parts_transparency(const NmlModelBlock *block)
{
    int index;
    int found = 0;
    for (index = 0; index < s_nToumeiNum; ++index) {
        if (s_aToumeiId[index] == block->partId) {
            found = 1;
            break;
        }
    }
    return found;
}

static int is_block_last_entry(const NmlModelBlock *block)
{
    int index;
    int found = 0;
    for (index = 0; index < s_nMapLast; ++index) {
        if (block->partId == s_aMapLast[index]) {
            found = 1;
            break;
        }
    }
    return found;
}

NmlParentBuffer *parent_buf_entry(void)
{
    NmlParentBuffer *entry = 0;
    if (s_nParentBuf < 512) {
        LayoutStore **layout;
        int index;
        entry = &s_aParentBuf[s_nParentBuf];
        layout = &entry->layout;
        *layout = 0;
        index = s_nParentBuf;
        s_aParentBuf[index].nonAlphaCount = 0;
        s_nParentBuf = index + 1;
        s_aParentBuf[index].alphaCount = 0;
    }
    return entry;
}

static NmlParentBuf *parent_buf_search(int parent)
{
    NmlParentBuf *found;
    int i;

    found = 0;
    for (i = s_nParentBuf - 1; i >= 0; i--) {
        NmlParentBuf *record = &s_aParentBuf[i];

        if (record->layout != 0 && record->layout->fields.parent == parent && parent != 0) {
            found = record;
            break;
        }
    }
    return found;
}

static void set_circle_shadow_ratio(int scale)
{
    float ratio = (float)scale * 0.0625f;
    int i;

    for (i = 0; i < 16; i++) {
        int index = (i < 8) ? i : i + 8;

        D_00956940[index] = (int)(128.0f - (float)i * ratio) << 24;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelLexDataCheck);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelLexBlockDoit);

static void nmlModelStealthEntry(void *arg0, void *arg1)
{
    NmlModelSampleView *model = arg0;
    StudioCamera *camera = arg1;
    const NmlStudioFovView *fovView = arg1;
    const Vector4 *samplePosition;
    NmlProjectedPoint projected;
    f32 depth;
    int index;
    int swapped;

    _CurSetMatrix(camera->viewMatrix);
    _CurSetViewScaleTrans((const float *)&camera->screenScale,
                         (const float *)&camera->screenOffset);
    samplePosition = &s_inLayout.fields.unmodeled_1f0;
    _CurRotTransPersClip(projected, samplePosition);
    depth = ((float)projected[2] * D_004D88E4) / tanf(fovView->fov * 0.5f);

    if (model->sampleCount != 0) {
        LayoutStore *const layout = &s_inLayout;
        for (index = 0; index < 10; ++index) {
            nmlFilterStealthMake(index, depth, camera, layout->slots);
            ++layout->fields.stealthEntryCount;
        }
        {
            float sortThreshold = D_004D88E8;
            LayoutStore *const sortLayout = layout;
            float distanceTolerance = D_004D88EC;
            swapped = 1;
            while (swapped != 0) {
                swapped = 0;
                for (index = 1; index <= 9; ++index) {
                    NmlStealthPacket *left = sortLayout->fields.stealthPackets[index - 1];
                    NmlStealthPacket *right;
                    if (sortThreshold < left->unclassified_20) {
                        right = sortLayout->fields.stealthPackets[index];
                        if (left->distanceSquared + distanceTolerance < right->distanceSquared) {
                            sortLayout->fields.stealthPackets[index - 1] = right;
                            swapped = 1;
                            sortLayout->fields.stealthPackets[index] = left;
                        }
                    }
                }
            }
        }
    } else {
        nmlFilterStealthMake(0, depth, camera, s_inLayout.slots);
        ++s_inLayout.fields.stealthEntryCount;
    }
}

static void nmlModelCalcSample(void *arg0, void **arg1)
{
    unsigned char *modelBytes = (unsigned char *)arg0;
    NmlModelSampleView *model = (NmlModelSampleView *)arg0;
    NmlPointQuadword *weightedPosition;
    NmlPointQuadword *currentPosition;

    if (model->sampleCount != 0) {
        weightedPosition = (NmlPointQuadword *)&s_inLayout.slots[0x7C];
        _WeightToGlobalVec((Vector4 *)weightedPosition,
                           (const float (*)[4])(const void *)(modelBytes + model->weightOffset),
                           (const float (*)[4])(const void *)(modelBytes + model->transformOffset));
        if ((s_inLayout.slots[0x230 / 4].i & 1) != 0) {
            s_inLayout.slots[0x1F4 / 4].f =
                s_inLayout.slots[0x230 / 4].f - s_inLayout.slots[0x1F4 / 4].f;
        }
        currentPosition = (NmlPointQuadword *)&s_inLayout.slots[0x84];
        __asm__ __volatile__(
            "lq $2, 0(%1)\n\t"
            "sq $2, 0(%0)"
            :
            : "r"(currentPosition), "r"(weightedPosition)
            : "$2", "memory"
        );
        if (arg1 != 0) {
            NmlPointQuadword *parentPosition =
                (NmlPointQuadword *)((char *)*arg1 + 0x210);
            __asm__ __volatile__(
                "lq $2, 0(%1)\n\t"
                "sq $2, 0(%0)"
                :
                : "r"(currentPosition), "r"(parentPosition)
                : "$2", "memory"
            );
        }
    } else {
        __asm__ __volatile__(
            "lqc2 vf3, 0(%1)\n\t"
            "lqc2 vf2, 0(%0)\n\t"
            "vadd.xyz vf2, vf2, vf3\n\t"
            "sqc2 vf2, 0(%0)"
            :
            : "r"((char *)&s_inLayout + 0x1F0),
              "r"(&model->samplePosition)
            : "memory"
        );
    }

    _CurSetMatrix((f32 (*)[4])&s_inLayout.fields.place);
    _CurApplyMatrix((Vector4 *)((char *)&s_inLayout + 0x1F0),
                    (Vector4 *)((char *)&s_inLayout + 0x1F0));
    s_inLayout.slots[0x1FC / 4].f = model->samplePosition.w;
    s_inLayout.slots[0x2C4 / 4].f = model->samplePosition.w;
    if (arg1 != 0) {
        NmlModelSampleView *parentModel =
            *(NmlModelSampleView **)((char *)*arg1 + 0x254);
        s_inLayout.slots[0x2C4 / 4].f = parentModel->samplePosition.w;
    }

    if (strstr((char *)modelBytes + 0x11, "boss_030") != 0 && model->sampleCount >= 30) {
        Vector4 sample;
        int sampleOffset = 0x7C0;

        _WeightToGlobalVec(&sample,
                           (const float (*)[4])(const void *)(modelBytes + model->weightOffset + sampleOffset),
                           (const float (*)[4])(const void *)(modelBytes + model->transformOffset + sampleOffset));
        _CurSetMatrix((f32 (*)[4])&s_inLayout.fields.place);
        _CurApplyMatrix(&sample, &sample);
        if (sample.y < 0.0f) {
            s_inLayout.slots[0x2C4 / 4].f = model->samplePosition.w * 0.25f;
        }
    }

    if (s_inLayout.slots[0x7F].f < D_004D88F0) {
        s_inLayout.slots[0x7F].f = 1.0f;
    }
    {
        float length = __builtin_sqrtf(
            s_inLayout.fields.place[0][0] * s_inLayout.fields.place[0][0] +
            s_inLayout.fields.place[0][1] * s_inLayout.fields.place[0][1] +
            s_inLayout.fields.place[0][2] * s_inLayout.fields.place[0][2]);

        if (length < 1.0f) {
            length = 1.0f;
        }
        s_inLayout.slots[0x7F].f =
            s_inLayout.slots[0x7F].f * (s_inLayout.slots[0x8B].f * length);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryClip);

static float deapth_for_studio(int studio, Vector4 *position, float scale)
{
    StudioCamera *camera;
    NmlProjectedPoint projected;
    float depth;
    float factor;

    depth = 0.0f;
    factor = scale;
    camera = xglStudioSelectGetActiveCamera(studio);
    if (camera == 0) {
        return depth;
    }
    _CurSetMatrix(camera->viewMatrix);
    _CurSetViewScaleTrans((const float *)&camera->screenScale,
                          (const float *)&camera->screenOffset);
    _CurRotTransPersClip(projected, position);
    depth = ((float)projected[2] * factor) /
            tanf(((NmlStudioFovView *)camera)->fov * 0.5f);
    return depth;
}

static void nmlModelCalcSpecularClip(void *model, int useParentDepth)
{
    f32 distanceScale = s_inLayout.slots[0x7F].f;
    f32 depth;
    f32 subwindowEnd;
    f32 subwindowRange;
    int flags;
    int index;

    if (strstr((char *)model + 0x10, D_004DC470) == 0 && s_nParent != 0) {
        NmlModelSampleView *parent = (NmlModelSampleView *)(unsigned int)s_nParent;
        distanceScale = parent->samplePosition.w;
    }
    if (useParentDepth != 0) {
        NmlModelSampleView *parent = (NmlModelSampleView *)(unsigned int)s_nParent;
        distanceScale = parent->samplePosition.w;
    }

    depth = deapth_for_studio(s_inLayout.fields.renderLevel, &s_inLayout.fields.unmodeled_1f0, distanceScale);
    flags = s_inLayout.slots[0xAC].i;
    if ((flags & 2) == 0) {
        if ((flags & 0x20) == 0) {
            if (__builtin_fabsf(depth) < 62582.0f) {
                s_inLayout.slots[0xAC].i = flags | 2;
            } else if (__builtin_fabsf(depth) <= 147523.0f) {
                s_inLayout.slots[0x8A].f =
                    (s_inLayout.slots[0x8A].f *
                     (__builtin_fabsf(depth) - 62582.0f)) /
                    84941.0f;
            }
            if (*(f32 *)((char *)model + 0x9C) > 10.0f) {
                s_inLayout.slots[0x8A].f = 1.0f;
                s_inLayout.slots[0xAC].i &= ~2;
            }
        }
        if (__builtin_fabsf(depth) < 10000.0f) {
            s_inLayout.slots[0x9B].f = 0.0f;
        } else if (__builtin_fabsf(depth) <= 20000.0f) {
            s_inLayout.slots[0x9B].f =
                (s_inLayout.slots[0x9B].f *
                 (__builtin_fabsf(depth) - 10000.0f)) /
                10000.0f;
        }
        if (s_inLayout.slots[0x89].f <= 0.0f) {
            s_inLayout.slots[0xAC].i |= 0x10;
        }
        flags = s_inLayout.slots[0xAC].i;
        if ((flags & 0x10) != 0) {
            s_inLayout.slots[0x89].f = 0.0f;
        }
        if ((flags & 0x102) != 0) {
            s_inLayout.slots[0x8A].f = 0.0f;
            s_inLayout.slots[0xAC].i = flags | 2;
        }

        s_inLayout.slots[s_inLayout.slots[0x9F].i + 0xC0].f =
            s_inLayout.slots[0x8A].f;
        subwindowEnd = 147523.0f;
        subwindowRange = 84941.0f;
        for (index = 0; index < 4; ++index) {
            if (s_inLayout.slots[0x9F].i != index &&
                g_aSubWindow[index] != 0) {
                f32 subwindowAbsoluteDepth;
                depth = deapth_for_studio(index, &s_inLayout.fields.unmodeled_1f0, distanceScale);
                subwindowAbsoluteDepth = __builtin_fabsf(depth);

                if (subwindowAbsoluteDepth < 62582.0f) {
                    s_inLayout.slots[index + 0xC0].i = 0;
                } else if (subwindowAbsoluteDepth <= subwindowEnd) {
                    s_inLayout.slots[index + 0xC0].f =
                        (s_inLayout.slots[index + 0xC0].f *
                         (subwindowAbsoluteDepth - 62582.0f)) /
                        subwindowRange;
                }
            }
        }
    } else {
        s_inLayout.slots[0xC0].f = 0.0f;
        s_inLayout.slots[0xC1].f = 0.0f;
        s_inLayout.slots[0xC2].f = 0.0f;
        s_inLayout.slots[0xC3].f = 0.0f;
        s_inLayout.slots[0x8A].f = 0.0f;
    }
    if ((s_inLayout.slots[0x94].i & 0x100040) != 0) {
        float clipDistance = __builtin_fabsf(depth);
        float clipLimit = 32582.0f;
        if (clipDistance < clipLimit) {
            s_inLayout.slots[0xAC].i |= 0x800;
        }
    }
}

static void nmlModelCalcDropShadow(void)
{
    xglMatrixUnit((float (*)[4])&s_inLayout.slots[0x180 / 4].f);

    {
        Vector4 shadow;

        shadow.x = LAYOUT_W(0x40);
        shadow.y = 1.5f;
        shadow.z = LAYOUT_W(0x60);

        if (s_nShadowVec != 0) {
            shadow.x = s_inShadowVec.x;
            shadow.z = s_inShadowVec.z;
        }

        if (shadow.y != 0.0f) {
            LAYOUT_W(0x190) = -shadow.x / shadow.y;
            LAYOUT_W(0x198) = -shadow.z / shadow.y;
        } else {
            LAYOUT_W(0x190) = 0.0f;
            LAYOUT_W(0x198) = 0.0f;
        }

        LAYOUT_W(0x194) = 0.0f;
        LAYOUT_W(0x1b4) = LAYOUT_W(0xb4) + 0.005f;
        LAYOUT_W(0x1b0) = -LAYOUT_W(0xb4) * LAYOUT_W(0x190);
        LAYOUT_W(0x1b8) = -LAYOUT_W(0xb4) * LAYOUT_W(0x198);

        if (LAYOUT_S32(0x2a8) != 0) {
            LAYOUT_W(0x1b4) = LAYOUT_W(0x2ac) + 0.005f;
            LAYOUT_W(0x1b0) = -LAYOUT_W(0x2ac) * LAYOUT_W(0x190);
            LAYOUT_W(0x1b8) = -LAYOUT_W(0x2ac) * LAYOUT_W(0x198);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryProjectCircle);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryProjectMap);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryPartsClip);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelEntry);

ACCEPTED_ASM("src/main/nml_model_set", nmlModelEntryEffect);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelEntryCard);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelRenderProreal);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelRenderTexture);

static int nmlModelRenderCircle(const NmlCircleOrigin *origin, int pitch, int frame)
{
    extern unsigned short s_aBlockLayout[];
    extern float D_004D894C;
    NmlCircleOwnerView *owner;
    LayoutStore *model;
    int result;
    int index;
    int end;
    int ratio;

    model = 0;
    result = 0;
    owner = (NmlCircleOwnerView *) origin->ownerAddress;
    if (owner->viewLimit <= D_004D894C) {
        result = 0;
    } else {
        index = origin->x - pitch;
        for (;;) {
            end = origin->x + origin->y * pitch - pitch;
            if (index == end) {
                break;
            }
            index += pitch;
            model = s_apModelLayout[s_aBlockLayout[index]];
            if ((model->fields.render_status & 0x100000u) != 0) {
                model = 0;
            } else {
                break;
            }
        }
        if (model != 0) {
            ratio = (int) ((float) model->fields.texMapSize * model->fields.circleShadowScale);
            set_circle_shadow_ratio(ratio);
            nmlPacketSendCircleTexture(&s_aCircleShadow, s_aCircleShadowClut);
            result = nmlModelRenderProreal(model, frame);
        }
    }
    return result;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelRenderDropCircle);

static StudioCamera *nmlModelRenderDrop(struct NmlModelRenderRange *range,
                                        int step, StudioCamera *screen, int mode)
{
    StudioCamera *partsResult = 0;
    unsigned int index;
    void *block;
    LayoutStore *layout;

    nmlFilterSetFrameAlphaClear();
    nmlPacketGsInit();
    nmlPacketAddGsFBA(1);
    nmlPacketAddGsZbuf(1);
    nmlPacketAddGsAlpha(0x89);
    nmlPacketAddGsPixeltest(0x54000);
    nmlPacketAddGifTagStandard(0, 1);
    nmlPacketAddGsFlush();
    if (s_nAnotherStudio) {
        nmlPacketAddScreen((NmlModelRenderState *)screen);
    }
    do {
        index = range->first - step;
        while (index != range->first + range->count * step - step) {
            index += step;
            block = s_apBlock[index];
            layout = s_apModelLayout[s_aBlockLayout[index]];
            if ((layout->slots[0x94].i & 0x100000) == 0) {
                layout->slots[0x94].i |= 0x80000;
                nmlPacketAddTransMicrocode(block, (NmlModelRenderState *)layout);
                nmlPacketAddBlockMaterial(block, (NmlModelRenderState *)layout, 0, mode);
                partsResult = nmlPacketAddParts(block, (NmlModelRenderState *)layout, screen);
                if (partsResult != 0) {
                    break;
                }
                nmlPacketAddExecProg(block, (NmlModelRenderState *)layout, 0, 0);
                layout->slots[0x94].i &= ~0x80000u;
            }
        }
    } while (0);
    return partsResult;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelFlushSubNonAlpha);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelFlushSubAlpha);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelFlushSub);

void nmlModelConstruct(void)
{
    CONSTRUCT_MODELSYSTEM();
    CONSTRUCT_CIRCLR_SHADOW();
    CONSTRUCT_BACK_BUFFER();
    CONSTRUCT_ALPHA_GROUP();
    CONSTRUCT_PARENT_BUF();
    CONSTRUCT_MAP_HANDLE(s_inMapHandle);
    CONSTRUCT_FADE_CONTROL(&s_inFadeIn);
    CONSTRUCT_FADE_CONTROL(&s_inFadeOut);
    CONSTRUCT_FADE_CONTROL(&s_inActiveFadeIn);
    CONSTRUCT_FADE_CONTROL(&s_inActiveFadeOut);
    nmlModelClear();
    s_inLayout.fields.render_status = 0;
}

void nmlModelInit(void)
{
    INIT_MODELSYSTEM();
    INIT_BACK_BUFFER();
    INIT_ALPHA_GROUP();
    INIT_PARENT_BUF();
    INIT_MAP_HANDLE(s_inMapHandle);
    nmlModelClear();
    s_inLayout.fields.render_status = 0;
    D_0095BB44[0] = 30;
}

void nmlModelClear(void) {
    CLEAR_LAYOUT_MODEL(&s_inLayout);
    CLEAR_PROREAL((int *) s_inProReal);
    CLEAR_MAP_HANDLE(s_inMapHandle);
    CLEAR_MODEL_ENTRY();
}

void nmlModelFlush(void)
{
    int result = 0;
    int group_index;
    int subwindow_index;
    unsigned int game_flags;

    if (s_nRenderCancel == 0) {
        int back_buffer_mode = s_inBackBuffer[5];
        switch (back_buffer_mode) {
        case 0:
            if (s_inBackBuffer[1] > 0 && s_inBackBuffer[8] == 0) {
                s_nRenderCancel = 1;
            }
            break;
        case 1:
            s_nRenderCancel = back_buffer_mode;
            break;
        }
    }

    AlphaGroupSort();
    AlphaGroupLastEntry();
    nmlPacketAddTransMicrocodeInit();
    nmlPacketGsInit();
    nmlPacketAddGsZbuf(0);
    nmlPacketAddGsZbuf1(1);
    nmlPacketAddGsFBA(0);
    nmlPacketAddGsFBA1(0);
    nmlPacketAddGsPixeltest(0x50000);
    nmlPacketAddGsPixeltest1(0x50000);
    nmlPacketAddGsFrame(sRender.display_buffer_base, 0);
    nmlPacketAddGsFrame1(sRender.display_buffer_base, 0);
    nmlPacketAddGsAlpha1(0x0000008000000068ULL);
    nmlPacketAddGsFlush();

    if (s_nPause == 0) {
        ++s_nCount;
    }
    game_flags = GameLoopState.flags;
    if (game_flags & 0x01000000) {
        s_nStencilVal = 0;
    } else {
        s_nStencilVal = (int)0x80000000u;
    }
    for (subwindow_index = 1; subwindow_index < 4; ++subwindow_index) {
        if (g_aSubWindow[subwindow_index] != 0) {
            s_nAnotherStudio = 1;
        }
    }

    if (s_nRenderCancel == 0) {
        result = nmlModelFlushSub(0);
    }
    if (result == 0 && s_nEffectWrite == 1 && s_nRenderCancel == 0) {
        nmlPacketGsInit();
        nmlPacketAddGsZbuf(0);
        nmlPacketAddGsPixeltest(0x50000);
        nmlPacketAddGsFrame(sRender.display_buffer_base, 0);
        nmlPacketAddGsFlush();
        sefDrawEffect();
    }
    if (result == 0 && s_nUseGnosys != 0) {
        nmlFilterSetFrameToBuffer(sRender.display_buffer_base);
    }
    if (result == 0 && s_nRenderCancel == 0) {
        for (group_index = 1; group_index < 4; ++group_index) {
            if (g_aSubWindow[group_index] != 0) {
                result = nmlModelFlushSub(group_index);
                if (result != 0) {
                    break;
                }
            }
        }
    }

    if (s_inBackBuffer[3] != 0 && --s_inBackBuffer[3] == 0) {
        s_inBackBuffer[0] = 1;
        s_inBackBuffer[1] = s_inBackBuffer[2];
    }
    if (s_inBackBuffer[0] != 0) {
        int display_buffer = (short)sRender.display_buffer_base;
        s_inBackBuffer[0] = 0;
        if (s_inBackBuffer[4] >= 0) {
            display_buffer = (short)s_inBackBuffer[4];
        }
        nmlFilterSetFrameToBuffer((unsigned short)display_buffer);
    }

    {
        int back_buffer_mode = s_inBackBuffer[5];
        switch (back_buffer_mode) {
        case 0: {
            float blend = 1.0f;
            if (s_inBackBuffer[8] != 0) {
                blend = (float)s_inBackBuffer[1] / (float)s_inBackBuffer[2];
            }
            if (s_inBackBuffer[1] > 0) {
                --s_inBackBuffer[1];
                nmlFilterSetBufferRender(blend);
                s_inBackBuffer[7] = 1;
            }
            break;
        }
        case 1:
            nmlFilterSetBufferRender(1.0f);
            s_inBackBuffer[7] = back_buffer_mode;
            if (s_inBackBuffer[6] > 0 && --s_inBackBuffer[6] == 0) {
                INIT_BACK_BUFFER();
            }
            if (s_nFrameLockOff != 0) {
                INIT_BACK_BUFFER();
            }
            break;
        }
    }

    nmlModelFadeDoit();
    nmlPacketGsInit();
    nmlPacketAddGsTexture();
    nmlPacketAddGsClamp(0);
    nmlPacketAddGsZbuf(0);
    nmlPacketAddGsPixeltest(0x50000);
    nmlPacketAddGsAlpha(0x0000008000000044ULL);
    nmlPacketAddGsFrame(sRender.display_buffer_base, 0);
    nmlPacketAddGsFlush();
    if (result != 0 || g_nPacketOver != 0) {
        sRender.render_status = 1;
    }
    g_nPacketOver = 0;
    s_nRenderCancelOld = s_nRenderCancel;
    if (s_nRenderCancel > 0) {
        --s_nRenderCancel;
    } else {
        s_nRenderCancel = 0;
    }
    nmlModelFlushClear();
}

void nmlModelFlushClear(void) {
    FLUSH_MODELSYSTEM();
    FLUSH_ALPHA_GROUP();
    FLUSH_MAP_HANDLE(s_inMapHandle);
    FLUSH_PARENT_BUF();
    FLUSH_BACK_BUFFER();
    nmlModelClear();
}

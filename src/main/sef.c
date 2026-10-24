#include "common.h"
#include "main/xgl_2.h"

#include "shared.h"

#include "sef.h"

#include "main/srs.h"

/* The effect control subrecord is the interior pointer the scheduler
 * queries walk. Its owner contains the separate liveness word. */
typedef struct SefBossDeathEffect {
    short character_id;
    unsigned short effect_no;
} SefBossDeathEffect;



extern const SefBossDeathEffect _sefBossDeathPrm_4[];

typedef struct SefEffectControl {
    unsigned char unmodeled_00[4];
    short character_id;
    unsigned char unmodeled_06[2];
    short effect_no;
    unsigned char unmodeled_0a[0x40 - 0x0a];
} SefEffectControl;



typedef struct SefEffectScheduler {
    unsigned char unmodeled_000[0x6b0];
    int inUse;
    unsigned char unmodeled_6b4[0xa70 - 0x6b4];
    SefEffectControl effect;
} SefEffectScheduler;



/* Original-backed literal aliases; keep the accepted function bodies intact. */

#define circle_angle_scale_literal (0.01745329238474369f)

#define effect_scale_literal_0_1 (0.10000000149011612f)

#define D_004D8318 (0.01745329238474369f)

#define lit4_004d8354 (0.01745329238474369f)

typedef struct SefActor {
    unsigned int flags;
    unsigned char unmodeled_004[0x10 - 4];
    Vector4 position;                /* +0x10 */
    unsigned char unmodeled_20[0x50 - 0x20];
    Vector4 rotation;                /* +0x50 */
    Vector4 scale;
    unsigned char unmodeled_70[0x510 - 0x70];
    Vector4 light_parameters[15];
    unsigned char unmodeled_600[0x824 - 0x600];
    Matrix4 *accessoryMatrix;
} SefActor;



#define SEF_ACTOR_LIGHT_FLAG 0x00008000u


#define SEF_BATTLE_ACTOR_MAX 24


typedef struct SefBattleActor {
    unsigned char unmodeled_000[0x20];
    Vector4 light_direction;         /* +0x20 */
    unsigned char unmodeled_030[0x20];
    Vector4 light_accumulator;       /* +0x50 */
    Vector4 light_position;          /* +0x60 */
    Vector4 actor_position;          /* +0x70 */
    struct SefActor *actor;
    short excluded;
    short lightCount;
    short hitSignal;
    unsigned char unmodeled_08a[2];
    short seSignal;
    unsigned char unmodeled_08e[2];
} SefBattleActor;



typedef struct SefBattleActorTbl {
    SefBattleActor actors[SEF_BATTLE_ACTOR_MAX];
    Vector4 center;
    int count;
    unsigned char unmodeled_d94[0x0c];
} SefBattleActorTbl;



static SefBattleActorTbl _battleActor[1];

#define SEF_LINE_DATA_RECORD_SIZE 0x820


#define SEF_LINE_DATA_COUNT 0x80


typedef struct SefLineDataEntry {
    unsigned char unmodeled_000[2];
    unsigned short inUse;
    unsigned char unmodeled_004[SEF_LINE_DATA_RECORD_SIZE - 4];
} SefLineDataEntry;



typedef struct SefLineDataTable {
    unsigned char unmodeled_000[0x810];
    SefLineDataEntry entries[SEF_LINE_DATA_COUNT];
} SefLineDataTable;



extern unsigned char _lineData[];

typedef struct GameLoopFlagsView {
    unsigned char unmodeled_00[0x10];
    int flags;
} GameLoopFlagsView;



extern GameLoopFlagsView GameLoopState;

#define SEF_DRAW_FLAG_PENDING 0x04000000


/* The freed-scheduler helper is declared after an assembly placeholder below. */

extern void sefFreeScheduler(unsigned int scheduler_index);

typedef struct SefKey3 {
    short frame;
    short value;
    short jump;
} SefKey3;



typedef struct SefProgressState {
    int key_index;
    int frame;
    int value;
    unsigned char unmodeled_0c[4];
    Vector4 vector;
} SefProgressState;



/* The float-valued view of SefProgressState used by the line-data scale keys. */
typedef struct SefFloatProgressState {
    int key_index;
    int frame;
    float value;
} SefFloatProgressState;



extern int ACT_jointGetAccessories(SefActor *actor, int index);

/* The 0x24-byte effect request block sefCreateEffect hands to the script creator. */
typedef struct SefEffectRequest {
    int source;                      /* +0x00 */
    int target;                      /* +0x04 */
    unsigned char unmodeled_08[4];
    short size;                      /* +0x0c */
    short size_copy;                 /* +0x0e */
    short character_id;              /* +0x10 */
    short character_id_copy;         /* +0x12 */
    short effect_no;                 /* +0x14 */
    short special;
    unsigned char reaction_mask;
    unsigned char reaction_mask_high;
    unsigned short flags;            /* +0x1a */
    unsigned char unmodeled_1c[4];
    int param;                       /* +0x20 */
} SefEffectRequest;



extern const char D_004CBF78[];

extern void *scAnalyzeScriptCf(int effect_no, int *count);

extern void scCreateScript(SefEffectRequest *request);

extern void sresFreeReloaderMemoryNo(int no);

extern void MMathDivVectorS(Vector4 *dest, Vector4 *source, float divisor);

float MMathCalcLength(float *vec);

/*
 * These constants are backed by the original literal-pool bytes. Repeated
 * values stay repeated at separate use sites where the compiler emits them.
 */

/* Original TU-local scheduler parent index (small-data word, initialized -1). */

static int _parentLine = -1;

/* Original local floating-point tuning values used by the scheduler assembly. */

static float _gravity = 0.0f;

static float _colision = 0.0f;

static float _height = 0.0f;

/* Original local direction flag. */

static short _revDirZ = 0;

/* The original TU-local zero flag is present as a two-byte small-data object. */

static short _initialize = 0;

/* Original global signal flags shared with the scheduler assembly in this TU. */

short _hitFlag = 0;

short _hitSignal = 0;

short _seSignal = 0;

/* Original global battle-mode flag; the current C callers set and clear it. */

short _sefBattleMode = 0;

/* The original local pointer stores the KSEG0 alias of the named table
 * sefAtanTbl at 0x0040FAC0 (main symbol map: 0x0040FAC0, size 0x1004). */

static float *atanTbl_0 = (float *)0x2040FAC0u;

/* Original TU-local scheduler-local index and effect-load queue. */

static int _nowParentLocal;

static int _sefLoadEftQue;

/*
 * sefIsBossID (main:0x002e11e8): true when character_id falls in the boss ID
 * range [0x97, 0xba] (sefCnvDeathEffectNo, sefSetupEnemy, this TU).
 */

/*
 * GNU EE native TI storage/copy type (docs/native-ti.md), used only for the
 * 16-byte aligned zero-fill blocks sefMemZero clears with por+sq; no wide
 * arithmetic is done on it.
 */

typedef unsigned int Quadword __attribute__((mode(TI)));



typedef union SefQuadBlock {
    Quadword quad;
    unsigned int words[4];
} SefQuadBlock;



extern Matrix4 *MMathRotateMatrixXYZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);

extern Matrix4 *MMathScaleMatrix(Matrix4 *out, Matrix4 *matrix, Vector4 *scale);

extern StudioCamera *xglStudioGetActiveCamera(void);

extern Matrix4 *MMathRotateMatrixYXZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);

int sefSearchMapperIndex2(int effect_id, int number, int category);

extern char *srsAnalyzeEftNo(int effect_id, int *number, int *category);

static unsigned char _battleData[0x230];

static unsigned char _ptAlloc[0xA0810];

/*
 * sevInitPtAllocator (main:0x002e1580): clears the whole _ptAlloc particle
 * allocator table and _battleData, then rebuilds _ptAlloc's own trailing
 * free-index queue (1024 entries, main VA 0x00794110's 0xa07fe-byte offset)
 * with the descending indices 0x3ff..0. sefInitEffect is its only caller.
 */

/*
 * A 10-byte keyed-animation record (config/units, sefProgressKey5): the
 * end_frame this function tests against 1024 and the following record's own
 * jump index are the only fields any accepted function of this TU touches;
 * the six bytes between them are not yet evidenced.
 */

typedef struct SefKey5 {
    short end_frame;
    short vector[3];
    short jump;
} SefKey5;



extern void sefGetPoint(Vector4 *position, const unsigned char *source, int mode);

static const unsigned char _zeroPos_004CBF00[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x3f
};

/*
 * sefGetCubePosBtm (main:0x002e23c8) is still INCLUDE_ASM in this TU;
 * declared here so sefGetCubePosTop (main:0x002e24c0) can call it. Both are
 * LOCAL in the original, so both stay static. `extent` is the cube's
 * integer extent vector (four ints, loaded with lqc2 and converted with
 * vitof0); the Top variant forwards it untouched.
 */

static void sefGetCubePosBtm(Vector4 *pos, const int *extent);

extern void sefGetOfsRange(void *position, unsigned char *offset_table, short index);

/*
 * SefParticleOffset (sefGetStartPos/sefGetTargetPos, main:0x002e2960 and
 * main:0x002e29b0): the only fields sefCreateParticle's effect record these
 * two functions evidence -- a start and a target position vector, and a
 * start and a target offset-table/index pair consumed opaquely by
 * sefGetOfsRange (still assembly). The bytes between and around them are
 * not evidenced by any accepted function in this TU.
 */

typedef struct SefParticleOffset {
    unsigned char unmodeled_00[0x30];
    Vector4 start_position;                  /* +0x030 */
    unsigned char unmodeled_40[0x10];
    Vector4 target_position;                 /* +0x050 */
    unsigned char unmodeled_60[0x110];
    unsigned char start_offset_table[0x20];  /* +0x170 */
    unsigned char target_offset_table[0x20]; /* +0x190 */
    unsigned char unmodeled_1b0[0x24];
    short *start_index;                      /* +0x1D4 */
    unsigned char unmodeled_1d8[4];
    short *target_index;                     /* +0x1DC */
} SefParticleOffset;



/*
 * sefCalcLocalMatrix (main:0x002e2d48), re-treated from the accepted assembly
 * group src/main/sef/sefCalcLocalMatrix.s toward readable C with constrained
 * ee-vu-cop2 inline assembly (route ee-vu-cop2, docs/ps2-capabilities.md).
 * The second function of that group, sefCalcRotChange, converted the same
 * way further down this file; the group now holds no tracked assembly.
 *
 * Names recovered from config/symbols/main.txt: _battleData (0x00794110),
 * _parentLine, _nowParentLocal, _ptAlloc. The battle state block and the
 * scheduler local record are read through narrowly evidenced scalar accesses
 * rather than through structs with invented members: only the fields below
 * are evidenced, the bytes between them are not, and docs/naming.md forbids
 * inventing padding members to close the gaps.
 *   _battleData + 0x00c  short  battle phase compared against 2
 *   _battleData + 0x22c  int    "battle is running" flag
 *   _battleData + 0x200         the matrix source both callees take
 *   record     + 0x000         the local matrix this call produces
 *   record     + 0x080         the base matrix it is multiplied by
 *   record     + 0x0c0         the rotation vector
 *   record     + 0x0f0/0x0f8   the offset the multiply is gated on
 *
 * record is kept as unsigned char* (its full layout is not recovered, only
 * these offsets are evidenced), and sefGetDirMatrix/sefGetVecMatrix are
 * called through Matrix4 and Vector4 pointer casts of record's own address to match
 * their published prototypes (src/main/sef.h): both take the destination
 * matrix at offset 0, so the cast is exactly what an already-Matrix4-typed
 * `record` would give the callee, with no different bytes read or written.
 */

#define SEF_BATTLE_PHASE         0x00c


#define SEF_BATTLE_MATRIX_SOURCE 0x200


#define SEF_BATTLE_RUNNING       0x22c


#define SEF_LOCAL_MATRIX         0x000


#define SEF_LOCAL_BASE_MATRIX    0x080


#define SEF_LOCAL_ROTATION       0x0c0


#define SEF_LOCAL_OFFSET         0x0f0


#define SEF_LOCAL_OFFSET_Z       0x0f8


#define SEF_PARENT_DIR_MATRIX    0x120


/* _battleData + 0x200, still owned by the scaffold, so it keeps the original
 * object's own name for the address (docs/naming.md). The two callees below
 * materialise the same address differently, which is why both spellings are
 * here: the flag path reaches it from the block base it already holds. */

extern unsigned char D_00794310[];

extern void MMathMulMatrix(void *destination, void *left, void *right);

/*
 * sefCalcRotChange (main:0x002e2f00), re-treated from the accepted assembly
 * group src/main/sef/sefCalcRotChange.s (itself the re-cut remainder of
 * src/main/sef/sefCalcLocalMatrix.s once its sibling sefCalcLocalMatrix
 * converted, config/units/math-correction13-5.json) toward readable C with
 * constrained ee-vu-cop2 inline assembly (route ee-vu-cop2,
 * docs/ps2-capabilities.md). The group now holds no assembly of its own.
 *
 * record keeps the offsets sefCalcLocalMatrix's own comment already
 * evidences, plus what this function itself reads and writes:
 *   record + 0x040   the source matrix MMathRotateMatrixXYZ multiplies by
 *                    when the masked mode selects it
 *   record + 0x080   SEF_LOCAL_BASE_MATRIX, the matrix every path here
 *                    rotates in place and sefCalcLocalMatrix later reads
 *   record + 0x0e0   the accumulated offset before this frame's change
 *   record + 0x0f0/0x0f4/0x0f8
 *                    SEF_LOCAL_OFFSET, the combined offset this call
 *                    produces and sefCalcLocalMatrix gates its multiply on.
 *                    The three components are the x/y/z of one Vector4, so
 *                    the single-axis rotations below read them as fields of
 *                    `(Vector4 *)(record + SEF_LOCAL_OFFSET)` rather than as
 *                    three separate offsets; SEF_LOCAL_OFFSET_Z stays for
 *                    sefCalcLocalMatrix, which reads +0x0f8 on its own
 *   record + 0x1c0   the per-frame rotation delta: degrees on entry,
 *                    converted to radians in place by the first block
 *
 * flags selects the rotation order: masking off bits 0x6000 and comparing to
 * 1 picks the XYZ path (Y/Z/X one axis at a time when bit 0x4000 is also set,
 * or one MMathRotateMatrixXYZ call over the source matrix otherwise); with
 * that not selected, bit 0x6000 clear takes MMathRotateMatrixYXZ, and
 * otherwise bit 0x4000 takes the same Y/Z/X order and bit 0x2000 takes
 * MMathRotateMatrixXYZ again, this time from the identity default and the
 * combined offset. update disables all of it: only the vector scale and
 * offset accumulation happen. MMathRotateMatrixX/Y/Z/XYZ/YXZ are main/tu219
 * (src/main/m_math.c); a null matrix argument here reaches their own
 * null-default idiom (docs/tu-worker.md), not a call site concern.
 *
 * The degrees-to-radians factor is D_004D8318, a second occurrence of the
 * same .lit4 constant sefDeg2RadVector reads as lit4_004d8354 above: EE GCC
 * 2.96 does not merge identical .lit4 words across call sites, so each user
 * gets its own address and its own extern (docs/naming.md, "Scaffold-owned
 * data keeps its splat name").
 *
 * MMathRotateMatrixX/Y/Z/XYZ/YXZ are declared with a non-const Matrix4 *
 * `matrix` parameter, matching MMathMulMatrix's own cross-TU declaration
 * above rather than m_math.c's `const Matrix4 *`: EE GCC 2.96 warns passing
 * a plain Matrix4 pointer (or a literal 0) into a `const Matrix4 *` array
 * parameter, and dropping the const here does not change what is read.
 */

#define SEF_ROTCHANGE_SRC_MATRIX  0x040


#define SEF_ROTCHANGE_OFFSET_BASE 0x0e0


#define SEF_ROTCHANGE_DELTA       0x1c0


extern Matrix4 *MMathRotateMatrixX(Matrix4 *out, Matrix4 *matrix, float angle);

extern Matrix4 *MMathRotateMatrixY(Matrix4 *out, Matrix4 *matrix, float angle);

extern Matrix4 *MMathRotateMatrixZ(Matrix4 *out, Matrix4 *matrix, float angle);

static void sevFreePtAllocator(unsigned int handle);

#define SEF_LOCAL_DATA_PT_TABLE 0x600


/*
 * PARTIAL ACCESSED VIEW of an effect-table record (sefInitEffectData): the
 * first 0x20 bytes are filled with 0xff by memset and are not otherwise
 * touched by this function; owner, effect_no and category are the fields it
 * sets from its own parameters, flags and frame are reset to 0. The 2-byte
 * gap after flags is not yet evidenced by any accepted function of this TU.
 */

typedef struct EffectData {
    signed char lineHandles[0x20];
    int owner;
    short category;
    short effect_no;
    short flags;
    short active;
    short frame;
} EffectData;



static void sefFreeLineData(int line_index);

/* One line-data record (SEF_LINE_DATA_RECORD_SIZE bytes): the particle-allocator
 * handle table sefFreeLocalData walks and the cursor sefAllocLocalData scans from. */
typedef struct SefLocalData {
    unsigned char *position_keys;                    /* +0x000 */
    unsigned char *spread_rotation_keys;             /* +0x004 */
    unsigned char *size_keys;                        /* +0x008 */
    unsigned char *signal_keys;                      /* +0x00c */
    unsigned char unmodeled_010[0x14 - 0x10];
    unsigned char *rotation_keys;                    /* +0x014 */
    unsigned char unmodeled_018[0x20 - 0x18];
    unsigned short initial_frame;                    /* +0x020 */
    short frame_count;                               /* +0x022 */
    unsigned char unmodeled_024[0x1e0 - 0x24];
    unsigned char *color_keys;                       /* +0x1e0 */
    unsigned char *uniform_scale_keys;               /* +0x1e4 */
    unsigned char unmodeled_1e8[SEF_LOCAL_DATA_PT_TABLE - 0x1e8];
    short pointHandles[0x100];                         /* +0x600 */
    unsigned char unmodeled_800[0x80e - 0x800];
    short cursor;                                      /* +0x80e */
} SefLocalData;

/* Only fields read or written by sefCreateParticle are named here. */
typedef struct SefParticle {
    unsigned char unmodeled_000[0x040];
    Matrix4 transform;                               /* +0x040 */
    unsigned char unmodeled_080[0x0c0 - 0x080];
    Vector4 start_position;                           /* +0x0c0 */
    Vector4 target_position;                           /* +0x0d0 */
    Vector4 random_rotation;                           /* +0x0e0 */
    Vector4 rotated_position;                          /* +0x0f0 */
    unsigned char unmodeled_100[0x10c - 0x100];
    float one_initialized_scalar;                     /* +0x10c */
    unsigned char unmodeled_110[0x130 - 0x110];
    Vector4 random_translation;                        /* +0x130 */
    unsigned char unmodeled_140[0x150 - 0x140];
    float random_scale;                               /* +0x150 */
    float battle_scale;                               /* +0x154 */
    int kind;                                         /* +0x158 */
    short frame;                                      /* +0x15c */
    short state;                                      /* +0x15e */
    unsigned char unmodeled_160[0x162 - 0x160];
    unsigned char signal;                             /* +0x162 */
    unsigned char unmodeled_163[0x188 - 0x163];
    short rotation_state;                             /* +0x188 */
    unsigned char unmodeled_18a[0x1a0 - 0x18a];
    int color[4];                                     /* +0x1a0 */
    unsigned char unmodeled_1b0[0x1c0 - 0x1b0];
    Vector4 rotation;                                 /* +0x1c0 */
    unsigned char unmodeled_1d0[0x1e0 - 0x1d0];
    Vector4 color_scale;                              /* +0x1e0 */
    unsigned char unmodeled_1f0[0x200 - 0x1f0];
    Vector4 color_offset;                             /* +0x200 */
    unsigned char unmodeled_210[0x240 - 0x210];
    Vector4 size;                                     /* +0x240 */
} SefParticle;



static void sefFreeEffectData(unsigned char *effect, unsigned int index);

extern void xglSoundEffectStopID(int soundId, int channel);

static void sefDestroyEffectData(unsigned char *effect);

extern int sefCreateScheduler2(int effect_no, void *parameters, void *position, int table, int flags, int index);

static const int offset_2[5] = {0, 0, 1, 2, 2};

static int revEft_3[10] = {0x0b03, 0x0b04, 0x0afd, 0x0b07, 0x0b05, 0x0b32, 0x0b31, 0x0afb, 0x0afe, 0x0b06};

extern void smInitilize(void *pool, unsigned int poolSize);

extern void sresInitMemoryRes(void);

extern void srsInitCdRead(void);

extern void svInitImageMapper(void);

void sefInitScheduler(void);

extern void scInitScript(void);

extern void sdvInitSpecialWork(void);

extern void sdvInitAmbient(void);

extern void sresLoadCommonMemory(void);

static unsigned char _eftBuffer[0xD4800];

static unsigned char _battlePrm[0x34];

typedef struct {
    unsigned char unmodeled_00[0x2c];
    void (*battleDrawCallback)(void);              /* +0x2c */
    unsigned char unmodeled_30[0x38 - 0x30];
    void (*cfDrawCallback)(void);                    /* +0x38 */
} SefRenderState;



extern SefRenderState sRender;

extern SrsMemRes _srsMemRes;

extern void sdvInitAlters(void);

void sefDestroyEffect(void);

void sefDestroyEffectCf(void);

extern void sresLoadCfMemory(void);

void sefDrawEffect2D(void);

extern void *smAlloc(unsigned int size);

extern void sresLoadBattleData(unsigned char *battle_prm);

extern int D_00794370[];

extern void scDestroyScriptAll(void);

extern void sdvDestroyAlters(void);

extern void sresFreeReloaderMemory(int reload_bgm);

void sefKillEffect(int effect_no);

static int sefCnvEtEffectNo(int effect_no, int character_id);

extern void svFileLoadScript(int mode, int effect_no);

extern int srsLeaveCdRead(void);

extern void scExecEffect(void);

extern void sdvExecAlters(void);

Matrix4 _invView = {0};

extern void svDrawScheduler(void);

extern void svDrawScheduler3D(int flags);

extern void svDrawScheduler2D(int flags);

void sefFreeSchedulerCf(SchedulerState *scheduler);

extern int srsFileLoadCf(int cf_id, int effect_no);

extern int srsEffectNameToID(char *effect_name);

extern int srsMemoryLoadCf(void *data, int effect_no, int size);


static SchedulerState *sefCreateEffectCf2(int effectNo,
                                           const Vector4 *position,
                                           const Vector4 *orientation,
                                           int schedulerIndex);

#define SEF_SCHEDULER_REWIND_FLAG 1


#define SEF_RECORD_ANCHOR 0xa70


#define SEF_RECORD_LIVE (0x6b0 - SEF_RECORD_ANCHOR)

#define SEF_RECORD_CHARACTER_ID (0xa74 - SEF_RECORD_ANCHOR)

#define SEF_RECORD_EFFECT_NO (0xa78 - SEF_RECORD_ANCHOR)

#define SEF_SCHEDULER_RECORD_SIZE 0xab0


#define SEF_SCHEDULER_COUNT 0x80


#define SEF_TARGET_DAMAGE_NULL_A 527


#define SEF_TARGET_DAMAGE_NULL_B 528


/* _ptAlloc's per-record byte size, the same 640 sefCalcLocalMatrix already
 * multiplies by at sef.c:359. */

#define SEF_PT_ALLOC_RECORD_SIZE 640


/* These values remain mutable objects so the recovered callers use storage. */

const char D_004CBFC0[16] = "boss_033";

const char D_004CBFD0[16] = "boss_032";

const char D_004CBFE0[16] = "boss_031";

const char D_004CBFF0[16] = "boss_030";

const char D_004CC000[16] = "boss_029";

const char D_004CC010[16] = "boss_028";

const char D_004CC020[16] = "boss_027";

const char D_004CC030[16] = "boss_026";

const char D_004CC040[16] = "boss_025";

const char D_004CC050[16] = "boss_024";

const char D_004CC060[16] = "boss_023";

const char D_004CC070[16] = "boss_022";

const char D_004CC080[16] = "boss_021";

const char D_004CC090[16] = "boss_020";

const char D_004CC0A0[16] = "boss_019";

const char D_004CC0B0[16] = "boss_018";

const char D_004CC0C0[16] = "boss_017";

const char D_004CC0D0[16] = "boss_016";

const char D_004CC0E0[16] = "boss_015";

const char D_004CC0F0[16] = "boss_014";

const char D_004CC100[16] = "boss_013";

const char D_004CC110[16] = "boss_012";

const char D_004CC120[16] = "boss_011";

const char D_004CC130[16] = "boss_010";

const char D_004CC140[16] = "boss_009";

const char D_004CC150[16] = "boss_008";

const char D_004CC160[16] = "boss_007";

const char D_004CC170[16] = "boss_006";

const char D_004CC180[16] = "boss_005";

const char D_004CC190[16] = "boss_004";

const char D_004CC1A0[16] = "boss_003";

const char D_004CC1B0[16] = "boss_002";

const char D_004CC1C0[16] = "boss_001";

const char D_004CC1D0[16] = "utso_007";

const char D_004CC1E0[16] = "utso_006";

const char D_004CC1F0[16] = "utso_005";

const char D_004CC200[16] = "utso_004";

const char D_004CC210[16] = "utso_003";

const char D_004CC220[16] = "utso_002";

const char D_004CC230[16] = "utso_001";

const char D_004CC240[16] = "utro_007";

const char D_004CC250[16] = "utro_006";

const char D_004CC260[16] = "utro_005";

const char D_004CC270[16] = "utro_004";

const char D_004CC280[16] = "utro_003";

const char D_004CC290[16] = "utro_002";

const char D_004CC2A0[16] = "utro_001";

const char D_004CC2B0[16] = "utre_006";

const char D_004CC2C0[16] = "utre_005";

const char D_004CC2D0[16] = "utre_004";

const char D_004CC2E0[16] = "utre_003";

const char D_004CC2F0[16] = "utre_002";

const char D_004CC300[16] = "utre_001";

const char D_004CC310[16] = "utma_014";

const char D_004CC320[16] = "utma_013";

const char D_004CC330[16] = "utma_012";

const char D_004CC340[16] = "utma_011";

const char D_004CC350[16] = "utma_010";

const char D_004CC360[16] = "utma_009";

const char D_004CC370[16] = "utma_008";

const char D_004CC380[16] = "utma_007";

const char D_004CC390[16] = "utma_006";

const char D_004CC3A0[16] = "utma_005";

const char D_004CC3B0[16] = "utma_004";

const char D_004CC3C0[16] = "utma_003";

const char D_004CC3D0[16] = "utma_002";

const char D_004CC3E0[16] = "utma_001";

const char D_004CC3F0[16] = "feso_004";

const char D_004CC400[16] = "feso_003";

const char D_004CC410[16] = "feso_002";

const char D_004CC420[16] = "feso_001";

const char D_004CC430[16] = "fero_001";

const char D_004CC440[16] = "fere_003";

const char D_004CC450[16] = "fere_002";

const char D_004CC460[16] = "fere_001";

const char D_004CC470[16] = "fema_006";

const char D_004CC480[16] = "fema_005";

const char D_004CC490[16] = "fema_004";

const char D_004CC4A0[16] = "fema_003";

const char D_004CC4B0[16] = "fema_002";

const char D_004CC4C0[16] = "fema_001";

const char D_004CC4D0[16] = "guno_114";

const char D_004CC4E0[16] = "guno_025";

const char D_004CC4F0[16] = "guno_024";

const char D_004CC500[16] = "guno_023";

const char D_004CC510[16] = "guno_022";

const char D_004CC520[16] = "guno_021";

const char D_004CC530[16] = "guno_020";

const char D_004CC540[16] = "guno_019";

const char D_004CC550[16] = "guno_018";

const char D_004CC560[16] = "guno_017";

const char D_004CC570[16] = "guno_016";

const char D_004CC580[16] = "guno_015";

const char D_004CC590[16] = "guno_014";

const char D_004CC5A0[16] = "guno_013";

const char D_004CC5B0[16] = "guno_012";

const char D_004CC5C0[16] = "guno_011";

const char D_004CC5D0[16] = "guno_010";

const char D_004CC5E0[16] = "guno_009";

const char D_004CC5F0[16] = "guno_008";

const char D_004CC600[16] = "guno_007";

const char D_004CC610[16] = "guno_006";

const char D_004CC620[16] = "guno_005";

const char D_004CC630[16] = "guno_004";

const char D_004CC640[16] = "guno_003";

const char D_004CC650[16] = "guno_002";

const char D_004CC660[16] = "guno_001";

const char D_004CC670[16] = "catherin";

/* Original-backed literal aliases; keep the accepted function bodies intact. */

extern const float sef_rand_unit_scale_literal;

static unsigned int sefRandSeed = 0x12345678;

typedef struct SefPtAllocatorState {
    unsigned char records[0xA0000];
    short freeSlots[0x400];
    unsigned int head;
    unsigned int tail;
    unsigned char unmodeled_a0808[8];
} SefPtAllocatorState;



#define SEF_PT_ALLOCATOR ((SefPtAllocatorState *)(void *)_ptAlloc)

/* Original TU-local scheduler storage, kept named for its recovered users. */

static unsigned char _scheduler[0x55800];

void sefLerpIVectorA(const void *keys, void *vector);

static const char D_004CBF60[24] = {
    (char)0xA5, (char)0xAD, (char)0xA5, (char)0xE3,
    (char)0xA5, (char)0xE9, (char)0xA4, (char)0xAC,
    (char)0xC4, (char)0xC9, (char)0xB2, (char)0xC3,
    (char)0xA4, (char)0xC7, (char)0xA4, (char)0xAD,
    (char)0xA4, (char)0xDE, (char)0xA4, (char)0xBB,
    (char)0xA4, (char)0xF3, 0, 0
};

typedef struct SefBattleParameters {
    short player_character_ids[3];
    short enemy_character_ids[3];
    short enemy_effect_numbers[3];
    short player_setup_parameters[3][3];
    short player_active[3];
    short enemy_active[3];
    int has_boss;
} SefBattleParameters;



void sefSetupEffect(void);

extern void tracePrint(const char *format, ...);

extern int func_A33248(void);

extern short D_00794120[];

typedef union SefIntFloatVector {
    int ints[4];
    Vector4 floats;
} SefIntFloatVector;



extern Vector4 *MMathApplyMatrix(Vector4 *destination, const void *matrix, const Vector4 *point);

extern Vector4 *MMathSpecialVectorSub(Vector4 *destination, const Vector4 *first, const Vector4 *second, float parameter);

extern Vector4 *MMathApplyMatrix(Vector4 *destination, const void *matrix, const Vector4 *point);

typedef struct SefAnalyzeRecord {
    short time;
    short image_id;
    short alt_image_id;
    unsigned char unmodeled_06;
    signed char flag;
} SefAnalyzeRecord;



typedef struct SefImageItem {
    void *data;
    unsigned char unmodeled_04[0x08];
    unsigned short u12;
    unsigned char unmodeled_0e[4];
    unsigned short u18;
    unsigned short u20;
    unsigned short u22;
    unsigned short u24;
    unsigned short u26;
    unsigned short u28;
    unsigned short u30;
} SefImageItem;



extern void *svGetImageListItem(int type, int image_id);

extern const float D_004D8334;

typedef struct SefAnimRecord {
    short frame;
    unsigned char unmodeled_02[4];
    signed char jump;
    unsigned char unmodeled_07;
} SefAnimRecord;



typedef struct SefAnimBlock {
    int item_a_word;
    int item_b_word;
    unsigned long long packed_params;
    float scale_u;
    float scale_v;
    short item_u12;
    short total_time;
    int first_flag;
} SefAnimBlock;



typedef struct SefAnimCursor {
    int key_index;
    SefAnimBlock block;
} SefAnimCursor;



extern unsigned int D_00794334[];

extern float D_004D8338;

extern const char D_004CBF98[];

typedef struct SefSavePrm {
    Vector4 position;
    Vector4 orientation;
    Vector4 saved_vector;
    int active;
    int saved_words[5];
    short effect_no;
    unsigned char saved_flags;
    unsigned char unmodeled_4b[5];
} SefSavePrm;



typedef struct SefSavedEffect {
    Vector4 saved_vector;
    unsigned char unmodeled_010[0x608];
    int active;
    int copied_words[4];
    unsigned char unmodeled_62c[0x3C0];
    int tail_word;
    unsigned char unmodeled_9f0[0x0A];
    unsigned char saved_flags;
    unsigned char unmodeled_9fb[0xB5];
} SefSavedEffect;



#define SEF_SAVE_PRM_COUNT 0x80
extern SefSavePrm _savePrm[SEF_SAVE_PRM_COUNT];

extern SefSavedEffect D_0065A9A0[];

typedef enum SefSaveActive {
    SEF_SAVE_INACTIVE,
    SEF_SAVE_ACTIVE
} SefSaveActive;



typedef enum SefSaveTail {
    SEF_SAVE_TAIL_NONE
} SefSaveTail;



typedef struct SefIntegerProgressState {
    int key_index;
    int frame;
    int value;
} SefIntegerProgressState;

struct SefKey5Table {
    unsigned int unmodeled_000;
    SefKey5 keys[1];
};

typedef struct SefLineData {
    unsigned char unmodeled_000[0xB0];
    Vector4 position;
    unsigned char unmodeled_0C0[0x20];
    Vector4 scale_vector;
    unsigned char unmodeled_0F0[0x10];
    Vector4 rotation_vector;
    unsigned char unmodeled_110[0x10];
    Vector4 position_vector;
    unsigned char position_source[0x30];
    SefProgressState first_vector_progress;
    SefProgressState second_vector_progress;
    unsigned char unmodeled_1A0[0x0C];
    SefIntegerProgressState first_integer_progress;
    SefIntegerProgressState second_integer_progress;
    unsigned char unmodeled_1C4[0x0C];
    unsigned char *scale_keys;
    struct SefKey5Table *first_vector_keys;
    unsigned char *rotation_keys;
    struct SefKey5Table *second_vector_keys;
    unsigned char unmodeled_1E0[8];
    struct SefKey3 *first_integer_keys;
    struct SefKey3 *second_integer_keys;
    unsigned char unmodeled_1F0[4];
    short *position_keys;
    unsigned char unmodeled_1F8[0x400];
    float gravity;
    float height;
    unsigned char unmodeled_600[0x202];
    short global_result;
    short timer;
    unsigned short frame;
    unsigned char unmodeled_808[2];
    unsigned short loop_mode;
    unsigned short global_data_count;
    unsigned char unmodeled_80E[6];
    unsigned short local_result;
    short collision_progress;
    unsigned char unmodeled_818[8];
} SefLineData;



extern float D_004D834C;

int sefExecLineGlobalData(SefLineData *line_data, unsigned short frame);

int sefExecLineLocalData(SefLineData *line_data, unsigned short frame);





static void sefCnvWaitEffectNo(SefEffectRequest *request);

#define D_004D833C (0.009999999776482582f)

#define D_004D8340 (0.009999999776482582f)

#define D_004D8344 (3.0518509447574615e-05f)

#define D_004D8348 (3.0518509447574615e-05f)



void sefDeleteEffect2(SchedulerState *scheduler);

extern short srsEftNoWeaponEffectID(int effectNo);

static void sefGetSpherePos(Vector4 *position, float radius);

static int sefAnimate(unsigned char *object, unsigned char *state, int advance);

float srsAtan2(float x, float y)
{
    int y_is_positive = y >= 0.0f;
    int x_is_positive = x >= 0.0f;
    float result = 0.0f;
    float angle;

    if (y == 0.0f) {
        if (x != 0.0f) {
            if (x_is_positive) {
                result = 1.5707964f;
            } else {
                result = -1.5707964f;
            }
        }
    } else if (x == 0.0f) {
        if (!y_is_positive) {
            result = 3.1415927f;
        }
    } else {
        if (!y_is_positive) {
            y = -y;
        }
        if (!x_is_positive) {
            x = -x;
        }

        if (x <= y) {
            angle = atanTbl_0[(int)(x * 1024.0f / y)];
            if (y_is_positive) {
                if (!x_is_positive) {
                    angle = -angle;
                }
            } else if (x_is_positive) {
                angle = 3.1415927f - angle;
            } else {
                angle += 3.1415927f;
            }
        } else {
            angle = atanTbl_0[(int)(y * 1024.0f / x)];
            if (y_is_positive) {
                if (x_is_positive) {
                    angle = 1.5707964f - angle;
                } else {
                    angle = -(1.5707964f - angle);
                }
            } else if (x_is_positive) {
                angle += 1.5707964f;
            } else {
                angle = 4.712389f - angle;
            }
        }
        result = angle;
    }
    return result;
}

static float sefRandf(void)
{
    unsigned int seed = sefRandSeed;
    unsigned int next_seed;
    int sample;

    next_seed = seed * 0x41C64E6Du;
    next_seed += 0x3039u;
    sefRandSeed = next_seed;
    sample = (int)((next_seed >> 16) & 0x7FFFu);
    return (float)sample * sef_rand_unit_scale_literal;
}

static int sefIsBossID(int character_id)
{
    return (unsigned int)(character_id - 0x97) < 0x24;
}

void sefMemZero(void *dest, int size)
{
    SefQuadBlock *blocks = (SefQuadBlock *)dest;
    int block_count = size >> 4;
    int word_count = (size & 0xf) >> 2;
    SefQuadBlock *tail = blocks + block_count;
    int i;

    for (i = 0; i < block_count; i++) {
        blocks[i].quad = 0;
    }
    for (i = 0; i < word_count; i++) {
        tail->words[i] = 0;
    }
}

void sefCalcRotTransSMatrix(Vector4 *angles, Vector4 *translation, Vector4 *scale, Matrix4 *matrix)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(translation) : "memory");
    __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf2" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf4, vf3" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf2, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf3, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(matrix) : "memory");
    MMathRotateMatrixXYZ(matrix, matrix, angles);
    MMathScaleMatrix(matrix, matrix, scale);
}

static void sefCalcInvView(Matrix4 *matrix) {
    StudioCamera *camera = xglStudioGetActiveCamera();

    __asm__ __volatile__("vmr32.xyzw vf1, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf0, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf1, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf2, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf3, 0(%0)" : : "r"(matrix) : "memory");
    MMathRotateMatrixYXZ(matrix, matrix, &camera->rotation);
}

int sefSearchMapperIndex2(int effect_id, int number, int category) {
    int index;
    int column;
    int weapon_id;

    if (category == 14) {
        return 14;
    }
    if (category == 15) {
        return 15;
    }
    if ((unsigned int)(category - 16) < 24U) {
        for (index = 0; index < 24; index++) {
            if (_srsMemRes.effectData[index] != 0 &&
                _srsMemRes.no[16 + index] == effect_id) {
                return index + 16;
            }
        }
        return 0;
    }

    if ((unsigned int)(effect_id - 0xAF0) < 0xC8U) {
        weapon_id = srsEftNoWeaponEffectID(effect_id);
        if (weapon_id <= 0) {
            return 0;
        }
        for (index = 0; index < 3; index++) {
            for (column = 0; column < 3; column++) {
                if (_srsMemRes.weaponData[index][column] != 0 &&
                    _srsMemRes.no[index * 3 + column + 2] == weapon_id) {
                    return index * 3 + column + 2;
                }
            }
        }
        return 0;
    }

    if (((unsigned int)effect_id < 0x64U) ||
        ((unsigned int)(effect_id - 0xF0) < 0x9DU) ||
        ((unsigned int)(effect_id - 0x9C4) < 0x64U) ||
        ((unsigned int)(effect_id - 0xA78) < 0x6AU)) {
        {
            for (index = 0; index < 3; index++) {
                if (D_00794120[0] != 0 && ((SefBattleParameters *)_battlePrm)->player_character_ids[index] == D_00794120[0]) {
                    return index * 3 + 2;
                }
            }
        }
        {
            for (index = 0; index < 3; index++) {
                if (D_00794120[0] != 0 && ((SefBattleParameters *)_battlePrm)->enemy_character_ids[index] == D_00794120[0]) {
                    return index + 11;
                }
            }
        }
    }
    return 0;
}

int sefSearchMapperIndex(int effect_id) {
    int number;
    int category;

    srsAnalyzeEftNo(effect_id, &number, &category);
    return sefSearchMapperIndex2(effect_id, number, category);
}

static void sevInitPtAllocator(void)
{
    short *slot;
    int index;

    memset(_ptAlloc, 0, 0xA0810);
    slot = (short *)(_ptAlloc + 0xA07FE);
    memset(_battleData, 0, 0x230);
    index = 0x3FF;
    do {
        *slot = (short)index;
        index--;
        slot--;
    } while (index >= 0);
}

static int sevAllocPtAllocator(void)
{
    int head = (int)SEF_PT_ALLOCATOR->head;
    int next_head = (head + 1) & 0x3ff;

    if (next_head == SEF_PT_ALLOCATOR->tail)
        return -1;

    SEF_PT_ALLOCATOR->head = (unsigned int)next_head;
    return SEF_PT_ALLOCATOR->freeSlots[head];
}

static void sevFreePtAllocator(unsigned int handle)
{
    unsigned int tail = SEF_PT_ALLOCATOR->tail;
    unsigned int next_tail;

    if (handle < 0x400u && SEF_PT_ALLOCATOR->head != tail) {
        SEF_PT_ALLOCATOR->freeSlots[tail] = (short)handle;
        next_tail = (tail + 1u) & 0x3ffu;
        SEF_PT_ALLOCATOR->tail = next_tail;
    }
}

static void sefProgressKey3(SefKey3 *keys, SefProgressState *state)
{
    int frame = state->frame + 1;

    state->frame = frame;
    if (frame < 1024) {
        SefKey3 *key = keys + state->key_index;

        if (key->frame != 1024 && frame >= key[1].frame) {
            if (key[1].jump >= 0) {
                state->key_index = key[1].jump;
                key = &keys[state->key_index];
                state->frame = key->frame;
            } else {
                state->key_index++;
            }
        }
    }
}

static SefKey5 *sefProgressKey5(SefKey5 *keys, SefProgressState *state)
{
    int key_index = state->key_index;
    int frame = state->frame + 1;
    SefKey5 *key = keys + key_index;
    short jump;

    state->frame = frame;
    if (frame < 1024) {
        if (key->end_frame != 1024 && frame >= key[1].end_frame) {
            key++;
            jump = key->jump;
            if (jump >= 0) {
                state->key_index = jump;
                key = keys + jump;
                state->frame = key->end_frame;
            } else {
                state->key_index = key_index + 1;
            }
        }
    }
    return key;
}

static void sefLerpVectorA(void *keys, Vector4 *dest)
{
    __asm__ __volatile__(
        "ldl $9, 7(%0)\n\t"
        "ldr $9, 0(%0)\n\t"
        "dsrl $9, $9, 0x10\n\t"
        "vmove.w $vf2w, $vf0w\n\t"
        "pcgth $10, $0, $9\n\t"
        "pextlh $10, $10, $9\n\t"
        "qmtc2 $10, $vf1\n\t"
        "vitof0.xyz $vf2xyz, $vf1xyz\n\t"
        "sqc2 $vf2, 0(%1)\n\t"
        "nop"
        :
        : "r"(keys), "r"(dest)
        : "$9", "$10", "memory"
    );
}

int sefLerpVector(SefKey5 *keys, void *state_data)
{
    SefProgressState *state = state_data;
    SefKey5 *key;
    Vector4 *vector = &state->vector;
    short start_frame;
    short end_frame;

    key = keys + state->key_index;
    if (key[1].end_frame == 1024) {
        return 0;
    }

    key = sefProgressKey5(keys, state);
    start_frame = key->end_frame;
    end_frame = key[1].end_frame;
    if (start_frame != end_frame) {
        float factor = (float)(state->frame - start_frame) /
                       (float)(end_frame - start_frame);
        SefKey5 *next_key = key + 1;

        __asm__ __volatile__(
            "mfc1 $9, %3\n\t"
            "qmtc2 $9, $vf3\n\t"
            "ldl $8, 7(%0)\n\t"
            "ldr $8, 0(%0)\n\t"
            "ldl $9, 7(%1)\n\t"
            "ldr $9, 0(%1)\n\t"
            "dsrl $8, $8, 16\n\t"
            "dsrl $9, $9, 16\n\t"
            "pcgth $10, $0, $8\n\t"
            "pcgth $11, $0, $9\n\t"
            "pextlh $10, $10, $8\n\t"
            "pextlh $11, $11, $9\n\t"
            "qmtc2 $10, $vf1\n\t"
            "qmtc2 $11, $vf2\n\t"
            "vitof0.xyzw $vf1, $vf1\n\t"
            "vitof0.xyzw $vf2, $vf2\n\t"
            "vsub.xyz $vf2, $vf2, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
            "vadd.xyz $vf2, $vf2, $vf1\n\t"
            "sqc2 $vf2, 0(%2)"
            :
            : "r"(key), "r"(next_key), "r"(vector), "f"(factor)
            : "$8", "$9", "$10", "$11", "memory"
        );
        vector->w = 1.0f;
    } else {
        sefLerpVectorA(key, vector);
    }
    return 1;
}

static int sefLerpVectorSC(void *keys, Vector4 *vectors)
{
    int status = sefLerpVector(keys, vectors);
    float scale = effect_scale_literal_0_1;

    __asm__ __volatile__("" : : "f"(scale));

    if (status != 0) {
        vectors++;

        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2 $8, $vf2\n\t"
            "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(vectors), "f"(scale)
            : "$8", "memory"
        );
    }

    return status;
}

void sefLerpIVectorA(const void *keys, void *vector)
{
    /* Decode four signed halfwords from eight source bytes into sixteen
     * destination bytes. These are integer representations, not floats. */
    __asm__ __volatile__(
        "ldl $9, 7(%0)\n\t"
        "ldr $9, 0(%0)\n\t"
        "pcgth $10, $0, $9\n\t"
        "pextlh $10, $10, $9\n\t"
        "sq $10, 0(%1)"
        :
        : "r"(keys), "r"(vector)
        : "$9", "$10", "memory");
}

void sefLerpIVector(SefKey5 *keys, SefProgressState *state)
{
    SefKey5 *key = keys + state->key_index;
    Vector4 *vector = &state->vector;
    short start_frame;
    short end_frame;
    float factor;
    SefKey5 *next_key;

    if (key[1].end_frame == 1024) {
        return;
    }

    key = sefProgressKey5(keys, state);
    start_frame = key->end_frame;
    end_frame = key[1].end_frame;
    if (start_frame != end_frame) {
        factor = (float)(state->frame - start_frame) /
                 (float)(end_frame - start_frame);
        next_key = key + 1;

        __asm__ __volatile__(
            "ldl $8, 7(%0)\n\t"
            "ldr $8, 0(%0)\n\t"
            "ldl $9, 7(%1)\n\t"
            "ldr $9, 0(%1)\n\t"
            "mfc1 $10, %2\n\t"
            "qmtc2 $10, $vf3\n\t"
            "pcgth $10, $0, $8\n\t"
            "pcgth $11, $0, $9\n\t"
            "pextlh $10, $10, $8\n\t"
            "pextlh $11, $11, $9\n\t"
            "qmtc2 $10, $vf1\n\t"
            "qmtc2 $11, $vf2\n\t"
            "vitof0.xyzw $vf1, $vf1\n\t"
            "vitof0.xyzw $vf2, $vf2\n\t"
            "vsub.xyz $vf2, $vf2, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
            "vadd.xyz $vf2, $vf2, $vf1\n\t"
            "vftoi0.xyzw $vf2, $vf2\n\t"
            "sqc2 $vf2, 0(%3)"
            :
            : "r"(key->vector), "r"(next_key->vector), "f"(factor),
              "r"(vector)
            : "$8", "$9", "$10", "$11", "memory"
        );
        return;
    }
    sefLerpIVectorA(key->vector, vector);
}

void sefLerpIVector2(SefKey5 *keys, SefProgressState *state)
{
    SefKey5 *key = keys + state->key_index;
    SefIntFloatVector *vector = (SefIntFloatVector *)&state->vector;
    int start_frame;
    int end_frame;
    int factor;

    if (key[1].end_frame != 1024) {
        key = sefProgressKey5(keys, state);
        start_frame = key->end_frame;
        end_frame = key[1].end_frame;
        if (start_frame != end_frame) {
            factor = ((state->frame - start_frame) << 16) / (end_frame - start_frame);
            vector->ints[0] = (((key[1].vector[0] - key->vector[0]) * factor) >> 16) + key->vector[0];
            vector->ints[1] = (((key[1].vector[1] - key->vector[1]) * factor) >> 16) + key->vector[1];
            vector->ints[2] = (((key[1].vector[2] - key->vector[2]) * factor) >> 16) + key->vector[2];
            vector->ints[3] = -((((key[1].jump - key->jump) * factor) >> 16) + key->jump);
            return;
        }
    }
    vector->ints[0] = key->vector[0];
    vector->ints[1] = key->vector[1];
    vector->ints[2] = key->vector[2];
    vector->ints[3] = -key->jump;
}

void sefLerpFloat(SefKey3 *keys, SefFloatProgressState *state)
{
    SefKey3 *key;
    float fraction;

    key = &keys[state->key_index];
    if (key[1].frame != 1024) {
        sefProgressKey3(keys, (SefProgressState *)state);
        key = &keys[state->key_index];
        if (key->frame != key[1].frame) {
            fraction = (float)(state->frame - key->frame) / (float)(key[1].frame - key->frame);
            state->value = ((float)(key[1].value - key->value) * fraction + (float)key->value) * 0.01f;
            return;
        }
    }
    state->value = (float)key->value * 0.01f;
}

void sefLerpInt(SefKey3 *keys, SefProgressState *state)
{
    SefKey3 *key;
    int frame_delta;
    int frame_offset;
    int key_index;
    float fraction;

    key = &keys[state->key_index];
    if (key->frame == 1024) {
        state->value = key->value;
    } else if (key[1].frame == 1024) {
        state->value = key->value;
    } else {
        sefProgressKey3(keys, state);
        key_index = state->key_index;
        key = &keys[key_index];
        if (key->frame != key[1].frame) {
            frame_delta = key[1].frame - key->frame;
            frame_offset = state->frame - key->frame;
            fraction = (float)frame_offset / (float)frame_delta;
            state->value = (short)((float)(key[1].value - key->value) * fraction + (float)key->value);
        } else {
            state->value = key->value;
        }
    }
}

static void sefProgressInt(SefKey3 *keys, SefProgressState *state)
{
    sefProgressKey3(keys, state);
    state->value = keys[state->key_index].value;
}

static inline int sefIsNonZeroVector(Vector4 *vector)
{
    return (((int *)vector)[2] | *(unsigned long *)vector) != 0;
}

void sefGetMotionNullMatrix(Matrix4 *matrix, SefActor *actor, int index)
{
    int joint;

    if (actor != 0) {
        joint = ACT_jointGetAccessories(actor, index + 8);
        if (joint >= 0 && actor->accessoryMatrix != 0) {
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                :
                : "r"(matrix), "r"((joint << 6) + (int)actor->accessoryMatrix)
                : "$8", "$9", "$10", "$11", "memory"
            );
            if (sefIsNonZeroVector((Vector4 *)(*matrix)[3])) {
                return;
            }
        }
        sefCalcRotTransSMatrix(&actor->rotation, &actor->position, &actor->scale, matrix);
        return;
    }
    __asm__ __volatile__("vmr32.xyzw vf1, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf0, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf1, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf2, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf3, 0(%0)" : : "r"(matrix) : "memory");
}

void sefGetNullPosition(Vector4 *position, SefActor *actor, unsigned int index)
{
    int joint;
    Matrix4 *matrices;

    if (actor != 0 && index < 16) {
        joint = ACT_jointGetAccessories(actor, index + 8);
        matrices = actor->accessoryMatrix;
        if (joint < 0 || matrices == 0) {
            position->y = 20.0f;
        } else {
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(position), "r"((joint << 6) + (int)matrices + 48) : "$8", "memory");
        }
    }
}

void sefGetWeaponPosition(Vector4 *position, SefActor *actor, unsigned int index)
{
    int joint;
    Matrix4 *matrices;

    if (actor != 0 && index < 8) {
        joint = ACT_jointGetAccessories(actor, index);
        matrices = actor->accessoryMatrix;
        if (joint < 0 || matrices == 0) {
            position->y = 20.0f;
        } else {
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(position), "r"((joint << 6) + (int)matrices + 48) : "$8", "memory");
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefGetPoint);

void sefGetPosition(Vector4 *position, unsigned char *source, int mode)
{
    const unsigned char *point = source;

    if (point == 0) {
        point = _zeroPos_004CBF00;
    }
    sefGetPoint(position, point, mode);
    position->w = 1.0f;
}

static void sefGetCirclePos(Vector4 *pos, float radius)
{
    float angle = sefRandf() * 360.0f * circle_angle_scale_literal;
    float x_unit;
    float z_unit;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2 $8, $vf4\n\t"
        "vcallms 0xe8\n\t"
        "qmfc2.i $8, $vf1\n\t"
        "mtc1 $8, %0\n\t"
        : "=f"(x_unit)
        : "f"(angle)
        : "$8", "memory"
    );
    pos->x = x_unit * radius;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2 $8, $vf4\n\t"
        "vcallms 0x20\n\t"
        "qmfc2.i $8, $vf1\n\t"
        "mtc1 $8, %0\n\t"
        : "=f"(z_unit)
        : "f"(angle)
        : "$8", "memory"
    );

    pos->y = 0.0f;
    pos->w = 1.0f;
    pos->z = z_unit * radius;
}

static void sefGetSpherePos(Vector4 *position, float radius)
{
    int random_vector[4];
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    int random_x;
    int random_y;
    int random_z;
    float component_center;

    do {
        component_center = 16384.0f;
        seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
        seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
        seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
        sefRandSeed = seed_3;
        random_x = (int)((seed_1 >> 16) & 0x7FFF);
        random_y = (int)((seed_2 >> 16) & 0x7FFF);
        random_vector[0] = random_x;
        random_vector[1] = random_y;
        random_z = (int)((seed_3 >> 16) & 0x7FFF);
        random_vector[2] = random_z;
    } while (0);

    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "mfc1 $2, %1\n\t"
        "vitof0.xyzw vf1, vf1\n\t"
        "qmtc2 $2, vf2"
        :
        : "r"(random_vector), "f"(component_center)
        : "$2", "memory");
    __asm__ __volatile__("vsubx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("vmul.xyz vf2, vf1, vf1" : : : "memory");
    __asm__ __volatile__("vaddy.x vf2, vf2, vf2y" : : : "memory");
    __asm__ __volatile__("vaddz.x vf2, vf2, vf2z" : : : "memory");
    __asm__ __volatile__("vsqrt Q, vf2x" : : : "memory");
    __asm__ __volatile__("vwaitq" : : : "memory");
    __asm__ __volatile__("vaddq.x vf2, vf0, Q" : : : "memory");
    __asm__ __volatile__("vdiv Q, vf0w, vf2x" : : : "memory");
    __asm__ __volatile__("vmove.w vf2, vf0" : : : "memory");
    __asm__ __volatile__("vwaitq" : : : "memory");
    __asm__ __volatile__("vmulq.xyz vf2, vf1, Q" : : : "memory");
    __asm__ __volatile__("qmtc2.ni %0, vf1" : : "r"(radius) : "memory");
    __asm__ __volatile__("vmulx.xyz vf2, vf2, vf1x" : : : "memory");
    __asm__ __volatile__("sqc2 vf2, 0(%0)" : : "r"(position) : "memory");
}

void sefGetCubePosBtm(Vector4 *pos, const int *extent)
{
    Vector4 extent_vector;
    Vector4 random_vector;
    Vector4 *random_position;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    int random_x;
    int random_y;
    int random_z;
    float random_scale;
    float final_scale;
    float half_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(extent) : "memory");
    __asm__ __volatile__("vitof0.xyzw vf1, vf1" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    random_vector.w = 0.0f;
    random_position = &random_vector;

    random_scale = 3.051851e-05f;
    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
    sefRandSeed = seed_3;
    random_x = (int)((seed_1 >> 16) & 0x7FFF);
    random_y = (int)((seed_2 >> 16) & 0x7FFF);
    random_z = (int)((seed_3 >> 16) & 0x7FFF);
    random_vector.x = (float)random_x * random_scale;
    random_vector.y = (float)random_y * random_scale;
    random_vector.z = (float)random_z * random_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vmul.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    extent_vector.y = 0.0f;
    __asm__ __volatile__("" : : "m"(extent_vector.y));

    half_scale = 0.5f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(half_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vsub.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");

    final_scale = 0.1f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(final_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(pos) : "memory");
}

static void sefGetCubePosTop(Vector4 *pos, const int *extent)
{
    sefGetCubePosBtm(pos, extent);
    pos->y = -pos->y;
}

static void sefGetCubePos(Vector4 *pos, const int *extent)
{
    Vector4 extent_vector;
    Vector4 random_vector;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    int random_x;
    int random_y;
    int random_z;
    float random_scale;
    float final_scale;
    float half_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(extent) : "memory");
    __asm__ __volatile__("vitof0.xyzw vf1, vf1" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    random_vector.w = 0.0f;

    random_scale = 3.051851e-05f;
    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
    sefRandSeed = seed_3;
    random_x = (int)((seed_1 >> 16) & 0x7FFF);
    random_y = (int)((seed_2 >> 16) & 0x7FFF);
    random_z = (int)((seed_3 >> 16) & 0x7FFF);
    random_vector.x = (float)random_x * random_scale;
    random_vector.y = (float)random_y * random_scale;
    random_vector.z = (float)random_z * random_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vmul.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");

    half_scale = 0.5f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(half_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vsub.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");

    final_scale = 0.1f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(final_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(pos) : "memory");
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefGetOfsRange);

static void sefGetDirMatrix(Matrix4 *dest, Vector4 *dir)
{
    Vector4 flipped;
    Vector4 angle;
    float xz_length;
    int length_bits;

    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "vsub.xyz vf2, vf0, vf1\n\t"
        "vmul.xz vf4, vf2, vf2\n\t"
        "sqc2 vf0, 0(%1)\n\t"
        "vaddz.x vf3, vf4, vf4z\n\t"
        "vmove.y vf2, vf1\n\t"
        "vsqrt Q, vf3x\n\t"
        "sqc2 vf2, 0(%2)"
        : : "r"(dir), "r"(&angle), "r"(&flipped) : "memory");
    angle.y = srsAtan2(flipped.x, flipped.z);
    __asm__ __volatile__(
        "vwaitq\n\t"
        "vaddq.x vf1, vf0, Q\n\t"
        "qmfc2 %0, vf1\n\t"
        "mtc1 %0, %1"
        : "=r"(length_bits), "=f"(xz_length) : : "memory");
    angle.x = srsAtan2(flipped.y, xz_length);
    __asm__ __volatile__(
        "lqc2 vf7, 0(%0)\n\t"
        "vaddx.x vf4, vf0, vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x vf5, vf0, vf1x\n\t"
        "vaddy.x vf4, vf0, vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y vf5, vf0, vf1x\n\t"
        "vaddx.x vf4, vf0, vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x vf6, vf0, vf1x\n\t"
        "vaddy.x vf4, vf0, vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y vf6, vf0, vf1x\n\t"
        "vmove.xyzw vf4, vf0\n\t"
        "vmr32.xyzw vf3, vf4\n\t"
        "vmr32.xyzw vf2, vf3\n\t"
        "vmr32.xyzw vf1, vf2\n\t"
        "vmulay.xyzw ACC, vf1, vf5y\n\t"
        "vmsuby.xyzw vf7, vf3, vf6y\n\t"
        "vmulay.xyzw ACC, vf3, vf5y\n\t"
        "vmaddy.xyzw vf3, vf1, vf6y\n\t"
        "vmulax.xyzw ACC, vf2, vf5x\n\t"
        "vmaddx.xyzw vf8, vf3, vf6x\n\t"
        "vmulax.xyzw ACC, vf3, vf5x\n\t"
        "vmsubx.xyzw vf3, vf2, vf6x\n\t"
        "sqc2 vf7, 0(%1)\n\t"
        "sqc2 vf8, 16(%1)\n\t"
        "sqc2 vf3, 32(%1)\n\t"
        "sqc2 vf4, 48(%1)"
        : : "r"(&angle), "r"(dest) : "memory");
}

void sefGetVecMatrix(Matrix4 *dest, Vector4 *source_a, Vector4 *source_b)
{
    Vector4 dir;

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "lqc2 $vf2, 0(%1)\n\t"
        "vsub.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%2)\n\t"
        :
        : "r"(source_a), "r"(source_b), "r"(&dir)
        : "memory"
    );
    sefGetDirMatrix(dest, &dir);
}

static void sefGetDirVector(void *dest, void *source) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tvsub.z vf1, vf0, vf1\n\tvmulax.xyzw ACC, vf7, vf1x\n\tvmadday.xyzw ACC, vf8, vf1y\n\tvmaddaz.xyzw ACC, vf3, vf1z\n\tvmaddw.xyzw vf1, vf4, vf1w\n\tsqc2 vf1, 0(%1)\n\tnop" :  : "r"(source), "r"(dest) : "memory");
}

static void sefGetStartPos(Vector4 *position, SefParticleOffset *record) {
    sefGetOfsRange(position, record->start_offset_table, *record->start_index);
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&record->start_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
    __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(position) : "memory");
}

static void sefGetTargetPos(Vector4 *position, SefParticleOffset *record) {
    sefGetOfsRange(position, record->target_offset_table, *record->target_index);
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&record->target_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
    __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(position) : "memory");
}

static void sefGetSpeed(Vector4 *position, void *particle, int mode, int option)
{
    unsigned char *record = (unsigned char *)particle;
    Vector4 vector;
    Vector4 *source;
    Vector4 *dest;
    unsigned char *parent;
    float speed;
    Vector4 *out;

    source = (Vector4 *)(record + 0x200);
    speed = *(float *)(record + 0x270) + *(float *)(record + 0x150);
    out = position;
    if (0.0f == speed) {
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(out) : "memory");
        return;
    }

    if (option != 0 && mode != 0) {
        MMathSpecialVectorSub(&vector, source, source, (float)option);
    } else {
        __asm__ __volatile__(
            "lq $8, 0(%0)\n\t"
            "sq $8, 0(%1)"
            : : "r"(source), "r"(&vector) : "$8", "memory");
    }

    if (mode >= 5) {
        sefGetVecMatrix((Matrix4 *)particle, (Vector4 *)(record + 0xd0), (Vector4 *)(record + 0xc0));
        sefGetDirVector(&vector, &vector);
    } else {
        if (mode == 4) {
            if (_parentLine >= 0 && _nowParentLocal >= 0) {
                parent = &_ptAlloc[_nowParentLocal * 640];
            } else {
                parent = 0;
            }
            if (parent != 0) {
                sefGetDirMatrix((Matrix4 *)particle, (Vector4 *)(parent + 0x120));
                sefGetDirVector(&vector, &vector);
            }
        } else if (mode == 3) {
            speed = 10.0f * speed;
            __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%0)" : : "r"(&vector) : "$8", "memory");
        } else if (mode == 1) {
            if (*(int *)_battleData != 0) {
                MMathApplyMatrix(&vector, _battleData + 0x80, &vector);
            }
        }
    }

    dest = (Vector4 *)(record + 0x120);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "mfc1 $8, %2\n\t"
        "qmtc2 $8, vf2\n\t"
        "sqc2 vf1, 0(%1)\n\t"
        "vmulx.xyz vf1xyz, vf1xyz, vf2x\n\t"
        "sqc2 vf1, 0(%3)"
        : : "r"(&vector), "r"(dest), "f"(speed), "r"(out) : "$8", "memory");
}

typedef struct {
    unsigned char unmodeled_00[0x0c];
    short phase;
    unsigned char unmodeled_0e[0x22c - 0x0e];
    int running;
} SefBattleStatus;

void sefMoveParticle(void *arg0, void *arg1, int arg2) {
    unsigned char *particle = (unsigned char *)arg0;
    Vector4 speed;
    int target;

    target = *(short *)*(void **)((unsigned char *)arg1 + 16);
    if (target == 529) {
        target = (((SefBattleStatus *)_battleData)->phase >= 2) ? 527 : 528;
    }
    if (((SefBattleStatus *)_battleData)->running != 0) {
        if ((unsigned int)(target - 257) < 16U) {
            target += 1792;
        }
        if ((_nowScheduler->flags & 0x1400) == 0x1000) {
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                : : "r"(particle + 0x40), "r"((unsigned char *)_battleData + 0x180)
                : "$8", "$9", "$10", "$11", "memory");
            *(float *)(particle + 0x154) = *(float *)((unsigned char *)_battleData + 0x220);
        }
    }

    if (*(signed char *)(particle + 0x163) == 0) {
        sefGetSpeed(&speed, particle, target, arg2);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 0(%1)\n\t"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
            "sqc2 $vf1, 0(%0)"
            : : "r"(particle + 0xc0), "r"(&speed) : "memory");
    }
    *(float *)(particle + 0x114) = *(float *)(particle + 0x114) - _gravity;
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "lqc2 $vf2, 0(%1)\n\t"
        "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%0)"
        : : "r"(particle + 0xc0), "r"(particle + 0x110) : "memory");
    if (_colision != 0.0f) {
        if (*(float *)(particle + 0xc4) <= _height) {
            float colision;
            if (*(signed char *)(particle + 0x163) == 0) {
                __asm__ __volatile__(
                    "lqc2 $vf1, 0(%0)\n\t"
                    "lqc2 $vf2, 0(%1)\n\t"
                    "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
                    "sqc2 $vf1, 0(%1)"
                    : : "r"(&speed), "r"(particle + 0x110) : "memory");
            }
            *(signed char *)(particle + 0x163) = 1;
            colision = _colision;
            *(float *)(particle + 0xc4) = _height;
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%0)\n\t"
                "mfc1 $8, %1\n\t"
                "qmtc2 $8, $vf2\n\t"
                "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
                "sqc2 $vf1, 0(%0)"
                : : "r"(particle + 0x110), "f"(colision) : "$8", "memory");
            if (*(float *)(particle + 0x114) < 0.0f) {
                *(float *)(particle + 0x114) = -*(float *)(particle + 0x114);
            }
        }
    }
}




typedef struct {
    unsigned char unmodeled_00[0xf0];
    unsigned long long offsetXYBits;
    float offsetZ;
    float offsetW;
} SefLocalOffset;



typedef struct {
    unsigned char unmodeled_00[0x6b0];
    int inUse;
    unsigned char unmodeled_6b4[0xa74 - 0x6b4];
    short characterId;
    unsigned char unmodeled_a76[2];
    short effectNo;
    unsigned char unmodeled_a7a[0xab0 - 0xa7a];
} SefSchedulerRecord;



static void sefCalcLocalMatrix(unsigned char *record, int target, int previous_target)
{
    unsigned char *battle = _battleData;
    SefBattleStatus *battleStatus = (SefBattleStatus *)battle;
    SefLocalOffset *local = (SefLocalOffset *)record;
    int in_battle = battleStatus->running;

    target &= ~0xe000;
    if (in_battle != 0 && (unsigned int)(target - 257) < 16) {
        target += 1792;
    }
    if (target == 529) {
        target = (battleStatus->phase >= 2) ? 527 : 528;
    }

    if (target == 4) {
        unsigned char *parent;

        if (_parentLine >= 0 && _nowParentLocal >= 0) {
            parent = &_ptAlloc[_nowParentLocal * 640];
        } else {
            parent = 0;
        }
        if (parent != 0 && target != previous_target) {
            sefGetDirMatrix((Matrix4 *)record, (Vector4 *)(parent + SEF_PARENT_DIR_MATRIX));
        }
    } else if (target == 1) {
        /* The base matrix becomes the local one: the four quadword moves the
         * original performs inline through the EE's own scratch GPRs. */
        __asm__ __volatile__(
            "lq $8, 0(%0)\n\t"
            "lq $9, 16(%0)\n\t"
            "lq $10, 32(%0)\n\t"
            "lq $11, 48(%0)\n\t"
            "sq $8, 0(%1)\n\t"
            "sq $9, 16(%1)\n\t"
            "sq $10, 32(%1)\n\t"
            "sq $11, 48(%1)"
            :
            : "r"(record + SEF_LOCAL_BASE_MATRIX), "r"(record)
            : "$8", "$9", "$10", "$11", "memory");
        return;
    } else if (in_battle != 0 && (target & 0x200) != 0) {
        Vector4 direction;

        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 0(%1)\n\t"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
            "sqc2 $vf1, 0(%2)"
            :
            : "r"(record + SEF_LOCAL_ROTATION), "r"((unsigned char *)_nowScheduler + 0x80),
              "r"(&direction)
            : "memory");
        sefGetVecMatrix((Matrix4 *)record, (Vector4 *)(battle + SEF_BATTLE_MATRIX_SOURCE),
                        &direction);
    } else if (target >= 5) {
        sefGetVecMatrix((Matrix4 *)record, (Vector4 *)D_00794310,
                        (Vector4 *)(record + SEF_LOCAL_ROTATION));
    } else {
        /* The identity matrix, built by rotating $vf0 = (0,0,0,1) one lane at
         * a time and storing the rows back to front. */
        __asm__ __volatile__(
            "vmr32.xyzw $vf1xyzw, $vf0xyzw\n\t"
            "vmr32.xyzw $vf2xyzw, $vf1xyzw\n\t"
            "vmr32.xyzw $vf3xyzw, $vf2xyzw\n\t"
            "sqc2 $vf0, 48(%0)\n\t"
            "sqc2 $vf1, 32(%0)\n\t"
            "sqc2 $vf2, 16(%0)\n\t"
            "sqc2 $vf3, 0(%0)"
            :
            : "r"(record)
            : "memory");
    }

    if (local->offsetXYBits != 0 ||
            local->offsetZ != 0.0f) {
        MMathMulMatrix(record, record, record + SEF_LOCAL_BASE_MATRIX);
    }
}

static void sefCalcRotChange(unsigned char *record, int flags, int update)
{
    __asm__ __volatile__("lqc2 $vf2, 0(%0)" : : "r"(record + SEF_ROTCHANGE_DELTA) : "memory");

    {
        register float degToRad asm("$f8") = D_004D8318;

        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf1x\n\t"
            "sqc2 $vf2, 0(%0)"
            :
            : "r"(record + SEF_ROTCHANGE_DELTA), "f"(degToRad)
            : "$8", "memory");
    }

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n\t"
        "lqc2 $vf2, 0(%2)\n\t"
        "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%0)"
        :
        : "r"(record + SEF_LOCAL_OFFSET), "r"(record + SEF_ROTCHANGE_OFFSET_BASE),
          "r"(record + SEF_ROTCHANGE_DELTA)
        : "memory");

    if (update == 0) {
        return;
    }

    if ((flags & 0xffff1fff) == 1) {
        if (flags & 0x4000) {
            MMathRotateMatrixY((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->y);
            MMathRotateMatrixZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->z);
            MMathRotateMatrixX((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->x);
        } else {
            MMathRotateMatrixXYZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                                 (Matrix4 *)(record + SEF_ROTCHANGE_SRC_MATRIX),
                                 (Vector4 *)(record + SEF_ROTCHANGE_DELTA));
        }
        return;
    }

    if ((flags & 0x6000) == 0) {
        MMathRotateMatrixYXZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                             (Vector4 *)(record + SEF_LOCAL_OFFSET));
        return;
    }

    if (flags & 0x4000) {
        MMathRotateMatrixY((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->y);
        MMathRotateMatrixZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->z);
        MMathRotateMatrixX((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->x);
        return;
    }

    if (flags & 0x2000) {
        MMathRotateMatrixXYZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                             (Vector4 *)(record + SEF_LOCAL_OFFSET));
        return;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefExecLineLocalData);

static int sefFreeLocalData(unsigned char *line_data, unsigned int index) {
    int valid = index < 0x100;
    int offset = (int)(index * 2) + SEF_LOCAL_DATA_PT_TABLE;

    if (valid) {
        short *slot = (short *)(line_data + offset);

        sevFreePtAllocator(*slot);
        offset = -1;
        *slot = -1;
    }
    return offset;
}

static int sefAllocLocalData(SefLocalData *data)
{
    int index;
    int end;
    int handle;

    index = data->cursor;
    end = (index - 1) & 0xff;
    for (; index != end; index = (index + 1) & 0xff) {
        if (data->pointHandles[index] < 0) {
            break;
        }
    }
    if (index == end) {
        return -1;
    }
    handle = sevAllocPtAllocator();
    if (handle < 0) {
        return -1;
    }
    data->pointHandles[index] = handle;
    data->cursor = (index + 1) & 0xff;
    return index;
}

static void sefDestroyLocalData(SefLocalData *data)
{
    int index;

    for (index = 0; index < 0x100; index++) {
        if (data->pointHandles[index] >= 0) {
            sefFreeLocalData((unsigned char *)data, index);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefInitLineData);

static int sefAllocLineData(void)
{
    SefLineDataTable *lineData = (SefLineDataTable *)_lineData;
    SefLineDataEntry *entry = lineData->entries;
    int line_index;

    for (line_index = 0; line_index < SEF_LINE_DATA_COUNT; line_index++) {
        if (entry->inUse == 0) {
            entry->inUse = 1;
            return line_index;
        }
        entry++;
    }
    return -1;
}

static void sefFreeLineData(int line_index)
{
    SefLocalData *data;

    if ((unsigned int)line_index < SEF_LINE_DATA_COUNT) {
        if (line_index < 0) {
            data = 0;
        } else {
            data = (SefLocalData *)&_lineData[line_index * SEF_LINE_DATA_RECORD_SIZE];
        }
        sefDestroyLocalData(data);
        ((SefLineDataTable *)_lineData)->entries[line_index].inUse = 0;
    }
}

static void sefAnalyzeAnim(unsigned char *object)
{
    SefAnalyzeRecord *records = *(SefAnalyzeRecord **)(object + 496);
    int accumulated = 0;
    int index = 0;
    float scale;

    if (records->image_id <= 0) {
        return;
    }
    scale = D_004D8334;
    do {
        SefImageItem *item_a;
        SefImageItem *item_b;
        int type = *(short *)((unsigned char *)_nowScheduler + 2724);

        item_a = (SefImageItem *)svGetImageListItem(type, records[index].image_id);
        item_b = (SefImageItem *)svGetImageListItem(type, records[index].alt_image_id);
        if (item_a != 0 && item_b != 0) {
            int flag = records->flag;

            if (index < 32) {
                unsigned long long tex0;
                unsigned char *dest = (unsigned char *)((index << 5) + (int)object) + 512;
                int psm;

                *(float *)(dest + 8) = (float)item_a->u24 * scale;
                *(float *)(dest + 12) = (float)item_a->u26 * scale;
                *(short *)(dest + 16) = (short)item_a->u12;
                psm = (item_a->u20 == 8) ? 19 : 20;
                ((void **)dest)[-2] = item_a->data;
                ((void **)dest)[-1] = item_b->data;
                tex0 = (unsigned long long)item_a->u18 | ((unsigned long long)((unsigned int)item_a->u22 >> 6) << 14) | ((unsigned long long)psm << 20) | ((unsigned long long)item_a->u28 << 26) | ((unsigned long long)item_a->u30 << 30) | (1ULL << 34) | ((unsigned long long)item_b->u18 << 37) | (1ULL << 61);
                accumulated = (records[index].time != 1024) ? accumulated + records[index].time : 1024;
                *(unsigned long long *)(object + (index << 5) + 512) = tex0;
                *(int *)(dest + 20) = flag > 0;
                *(short *)(dest + 18) = (short)accumulated;
            }
        } else {
            memset(object + 504 + index * 32, 0, 32);
        }
        index++;
    } while (records[index].image_id > 0);
}

static int sefAnimate(unsigned char *object, unsigned char *state, int advance)
{
    SefAnimCursor *cursor = (SefAnimCursor *)(state + 360);
    SefAnimRecord *records = *(SefAnimRecord **)(object + 496);
    int key = cursor->key_index;
    SefAnimRecord *current = records + key;
    int advanced = 0;
    int found = 0;
    int result = 0;
    unsigned int scan = D_00794334[0];
    short value;
    unsigned short frame_bits;
    short first_frame;

    if (advance < 0 || (scan & 1) != 0) {
        cursor->block = *(SefAnimBlock *)(object + 504);
        cursor->key_index = 0;
        *(short *)(state + 352) = 0;
        return 0;
    }
    first_frame = current->frame;
    frame_bits = current->frame;
    if (first_frame == 1024) {
        return 0;
    }
    if ((scan & 8) != 0) {
        for (scan = key; ; scan++, current = records + scan, frame_bits = current->frame) {
            int scan_jump = current->jump;

            if ((short)frame_bits == 1024) {
                return 0;
            }
            if (scan_jump >= 0) {
                if (current[1].frame == 1024) {
                    continue;
                }
                return 0;
            }
            cursor->key_index = scan;
            found = 1;
            break;
        }
    }
    if (found) {
        cursor->key_index++;
        result = 1;
        advanced = 1;
    } else {
        value = *(short *)((unsigned char *)cursor + 34);
        if (advance >= value) {
            int jump = current->jump;

            if (jump >= 0) {
                cursor->key_index = jump;
                result = 4096;
                if (jump == 0) {
                    *(short *)(state + 352) = 0;
                } else {
                    *(short *)(state + 352) = *(short *)(object + 32 * jump + 498);
                }
            } else if (jump == -1) {
                if (current[1].frame != 1024) {
                    cursor->key_index++;
                    result = 1;
                }
            } else if (jump == -2) {
                result = 0;
            } else {
                if (current[1].frame == 1024) {
                    return 0;
                }
            }
            advanced = 1;
        }
    }
    if (advanced) {
        cursor->block = *(SefAnimBlock *)(object + 504 + 32 * cursor->key_index);
    }
    *(short *)(state + 352) = *(short *)(state + 352) + 1;
    return result;
}

int sefCreateParticle(SefLocalData *data, int kind)
{
    unsigned char *battle;
    unsigned char *params;
    SefParticle *particle;
    unsigned char *rotation_source_keys;
    int key_flag;
    Vector4 spread;
    Vector4 jitter;
    int count;
    int index;
    int result;
    int update;
    short angle;
    short handle;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    float random_x;
    float random_y;
    float random_z;
    float uniform_jitter;

    index = sefAllocLocalData(data);
    params = (unsigned char *)data;
    result = -1;
    if (index >= 0) {
        handle = data->pointHandles[index];
        particle = (SefParticle *)(_ptAlloc + handle * 640);
        if (particle != 0) {
            sefMemZero(particle, 640);
            sefGetStartPos(&particle->start_position, (SefParticleOffset *)data);
            sefGetTargetPos(&particle->target_position, (SefParticleOffset *)data);
            particle->state = 0;
            particle->frame = data->initial_frame;
            count = data->frame_count;
            if (count > 0) {
                sefRandSeed = sefRandSeed * 0x41C64E6Du + 0x3039u;
                particle->frame += (short)((int)((sefRandSeed >> 16) & 0x7FFF) % count);
            }
            sefAnimate(params, (unsigned char *)particle, -1);

            if (data->spread_rotation_keys != 0) {
                spread.x = (float)*(short *)(data->spread_rotation_keys + 2);
                spread.y = (float)*(short *)(data->spread_rotation_keys + 4);
                spread.z = (float)*(short *)(data->spread_rotation_keys + 6);
                jitter.x = sefRandf();
                jitter.y = sefRandf();
                jitter.z = sefRandf();
                jitter.w = 0.0f;
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "lqc2 vf2, 0(%1)\n\t"
                    "vmul.xyz vf1, vf1, vf2\n\t"
                    "sqc2 vf1, 0(%0)"
                    : : "r"(&jitter), "r"(&spread) : "memory");
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "mfc1 $8, %1\n\t"
                    "qmtc2 $8, vf2\n\t"
                    "vmulx.xyz vf1, vf1, vf2x\n\t"
                    "sqc2 vf1, 0(%0)"
                    : : "r"(&spread), "f"(0.5f) : "$8", "memory");
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "lqc2 vf2, 0(%1)\n\t"
                    "vsub.xyz vf1, vf1, vf2\n\t"
                    "sqc2 vf1, 0(%2)"
                    : : "r"(&jitter), "r"(&spread), "r"(&particle->random_rotation) : "memory");
                __asm__ __volatile__(
                    "lqc2 vf2, 0(%0)\n\t"
                    "lwc1 $f8, %1\n\t"
                    "mfc1 $8, $f8\n\t"
                    "qmtc2 $8, vf1\n\t"
                    "vmulx.xyz vf2, vf2, vf1x\n\t"
                    "sqc2 vf2, 0(%0)"
                    : : "r"(&particle->random_rotation), "m"(D_004D8338) : "$8", "$f8", "memory");
                battle = _battleData;
                if (*(int *)(battle + 0x22C) != 0) {
                    __asm__ __volatile__(
                        "lq $8, 0(%1)\n\t"
                        "lq $9, 16(%1)\n\t"
                        "lq $10, 32(%1)\n\t"
                        "lq $11, 48(%1)\n\t"
                        "sq $8, 0(%0)\n\t"
                        "sq $9, 16(%0)\n\t"
                        "sq $10, 32(%0)\n\t"
                        "sq $11, 48(%0)"
                        : : "r"(&particle->transform), "r"(battle + 0x180) : "$8", "$9", "$10", "$11", "memory");
                    particle->battle_scale = *(float *)(battle + 0x220);
                } else if (*(short *)data->spread_rotation_keys == 1) {
                    MMathRotateMatrixYXZ(&particle->transform, 0, &particle->random_rotation);
                }
            }

            particle->random_scale = sefRandf() * (float)*(short *)data->uniform_scale_keys * D_004D833C;
            particle->kind = kind;

            if (data->position_keys == 0) {
                particle->color[2] = 64;
                particle->color[1] = 64;
                particle->color[0] = 64;
            } else if (*(short *)(data->position_keys + 18) == 1024) {
                sefLerpIVectorA(data->position_keys + 10, (Vector4 *)particle->color);
            }

            if (data->size_keys != 0 && *(short *)(data->size_keys + 10) == 1024) {
                sefLerpIVectorA(data->size_keys + 2, &particle->color_scale);
            }

            if (data->color_keys != 0 && *(short *)(data->color_keys + 18) == 1024) {
                sefLerpVectorA(data->color_keys + 8, &particle->color_offset);
                if (_revDirZ != 0) {
                    particle->color_offset.z = -particle->color_offset.z;
                }
            }

            if (data->rotation_keys != 0 && *(short *)(data->rotation_keys + 10) == 1024) {
                sefLerpVectorA(data->rotation_keys, &particle->size);
                if (*(short *)data->color_keys != 3) {
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%3)\n\t"
                        "mfc1 $8, %2\n\t"
                        "qmtc2 $8, vf2\n\t"
                        "vmulx.xyz vf1, vf1, vf2x\n\t"
                        "sqc2 vf1, 0(%3)"
                        : "=m"(particle->size)
                        : "m"(particle->size), "f"(D_004D8340), "r"(&particle->size)
                        : "$8", "memory");
                }
            }

            if (data->spread_rotation_keys != 0 && *(short *)(data->spread_rotation_keys + 18) == 1024) {
                sefLerpVectorA(data->spread_rotation_keys + 8, &particle->rotation);
                rotation_source_keys = data->spread_rotation_keys;
                update = 0;
                key_flag = *(short *)(rotation_source_keys - 2);
                angle = *(short *)rotation_source_keys;
                if (particle->rotation_state != 4 || key_flag != 0) {
                    update = 1;
                }
                sefCalcRotChange((unsigned char *)particle, angle, update);
            }
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n\t"
                "lqc2 vf2, 0(%1)\n\t"
                "vadd.xyz vf1, vf1, vf2\n\t"
                "sqc2 vf1, 0(%2)"
                : : "r"(&particle->random_rotation), "r"(&particle->rotation), "r"(&particle->rotated_position) : "memory");

            if (data->signal_keys != 0) {
                particle->signal = *data->signal_keys;
            }

            particle->one_initialized_scalar = 1.0f;
            particle->random_translation.w = 1.0f;
            if (data->position_keys != 0) {
                if (*(short *)(data->position_keys + 6) == 0) {
                    short range_x;
                    short range_y;
                    short range_z;
                    float scale_k;
                    range_x = *(short *)(data->position_keys + 0);
                    range_y = *(short *)(data->position_keys + 2);
                    range_z = *(short *)(data->position_keys + 4);
                    scale_k = D_004D8344;
                    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
                    random_x = (float)(int)((seed_1 >> 16) & 0x7FFFu);
                    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
                    random_y = (float)(int)((seed_2 >> 16) & 0x7FFFu);
                    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
                    random_z = (float)(int)((seed_3 >> 16) & 0x7FFFu);
                    sefRandSeed = seed_3;
                    particle->random_translation.x = random_x * (float)range_x * scale_k * 0.015625f;
                    particle->random_translation.y = random_y * (float)range_y * scale_k * 0.015625f;
                    particle->random_translation.z = random_z * (float)range_z * scale_k * 0.015625f;
                } else {
                    short range;
                    float rand_value;
                    unsigned int seed;

                    range = *(short *)(data->position_keys + 0);
                    seed = sefRandSeed * 0x41C64E6Du + 0x3039u;
                    sefRandSeed = seed;
                    rand_value = (float)(int)((seed >> 16) & 0x7FFFu);
                    uniform_jitter = rand_value * (float)range * D_004D8348 * 0.015625f;
                    particle->random_translation.x = uniform_jitter;
                    particle->random_translation.y = uniform_jitter;
                    particle->random_translation.z = uniform_jitter;
                }
            }
        }
        result = index;
    }
    return result;
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefExecLineGlobalData);

static int sefExecLineData(SefLineData *line_data)
{
    unsigned short frame = line_data->frame;

    line_data->global_result = -1;
    if (line_data->loop_mode != 0) {
        line_data->timer--;
        if (line_data->timer < 0 &&
            (line_data->global_data_count == 0 || line_data->local_result == 0)) {
            line_data->timer = 0;
            return 0;
        }
    }
    if (line_data->timer < 0) {
        line_data->timer = -1;
    }

    _gravity = line_data->gravity;
    _height = line_data->height;
    _colision = (float)line_data->collision_progress * D_004D834C;
    sefLerpVectorSC(line_data->scale_keys + 8, &line_data->scale_vector);
    sefLerpVectorSC(line_data->rotation_keys + 8, &line_data->rotation_vector);
    if (line_data->position_keys != 0) {
        ((void (*)(void *, Vector4 *))sefLerpVectorSC)(line_data->position_keys + 4, &line_data->position_vector);
        sefGetPosition(&line_data->position, line_data->position_source, *line_data->position_keys);
    }
    sefLerpIVector(line_data->first_vector_keys->keys,
                   &line_data->first_vector_progress);
    sefLerpIVector(line_data->second_vector_keys->keys,
                   &line_data->second_vector_progress);
    sefProgressInt(line_data->first_integer_keys,
                   (SefProgressState *)&line_data->first_integer_progress);
    sefProgressInt(line_data->second_integer_keys,
                   (SefProgressState *)&line_data->second_integer_progress);

    if (line_data->timer < 0 || sefExecLineGlobalData(line_data, frame) != 0) {
        line_data->local_result = sefExecLineLocalData(line_data, frame);
        line_data->frame++;
        if (line_data->frame == 0x3FF) {
            if (line_data->loop_mode == 0) {
                line_data->frame -= (unsigned short)line_data->second_integer_progress.value;
                if (line_data->frame == 0) {
                    line_data->frame = 1;
                }
            } else {
                line_data->frame = 0x3FE;
            }
        }
    }
    return 1;
}

static void sefInitEffectData(EffectData *effect, int owner, int effect_no, int category)
{
    memset(effect, -1, 0x20);
    effect->owner = owner;
    effect->effect_no = (short)effect_no;
    effect->category = (short)category;
    effect->frame = 0;
    effect->flags = 0;
}

static void sefDestroyEffectData(unsigned char *effect)
{
    EffectData *effectData = (EffectData *)effect;
    int index;

    if (effectData->owner != 0) {
        for (index = 0; index < 0x20; index++) {
            sefFreeEffectData(effect, index);
        }
    }
    effectData->owner = 0;
}

static unsigned char *sefAllocEffectData(EffectData *effect)
{
    int index = effect->flags;
    signed char *slot;
    int line_index;

    if ((unsigned int)index >= 0x20) {
        return 0;
    }
    slot = effect->lineHandles + index;
    if (*slot >= 0) {
        return 0;
    }
    line_index = sefAllocLineData();
    if (line_index < 0) {
        return 0;
    }
    *slot = line_index;
    effect->active = 1;
    effect->flags++;
    return &_lineData[line_index * SEF_LINE_DATA_RECORD_SIZE];
}

static void sefFreeEffectData(unsigned char *effect, unsigned int index)
{
    if (index < 0x20) {
        signed char *handle = (signed char *)effect + index;

        if (*handle >= 0) {
            sefFreeLineData(*handle);
            *handle = -1;
        }
    }
}

static void sefCheckFinish(int effect_no, int condition) {
    if (condition & 0x4000) {
        _nowScheduler->flags |= 0x40;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefExecEffectData);

void sefInitScheduler(void)
{
    SchedulerState *scheduler = (SchedulerState *)_scheduler;
    ScriptBinding *binding;
    unsigned char *effects = scheduler->effects[0];
    EffectData *effect;
    short invalid = -1;
    int index;
    int count;

    sefMemZero(scheduler, 0x55800);
    binding = &scheduler->scriptBinding;
    for (index = 0; index < SEF_SCHEDULER_COUNT; index++) {
        binding->script_id = invalid;
        binding->task_id = invalid;
        binding->link_id = invalid;
        effect = (EffectData *)(index * SEF_SCHEDULER_RECORD_SIZE + (int)effects);
        for (count = 0; count < 0x20; count++) {
            sefInitEffectData(effect, 0, -1, 0);
            effect++;
        }
        binding = (ScriptBinding *)((unsigned char *)binding + SEF_SCHEDULER_RECORD_SIZE);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefAllocScheduler);

void sefFreeSchedulerCf(SchedulerState *scheduler)
{
  int soundId;
  int i;

  if (scheduler != 0 && scheduler->inUse != 0)
  {
    soundId = scheduler->soundId;
    if (soundId > 0)
    {
      xglSoundEffectStopID(soundId, 0);
      scheduler->soundId = 0;
    }
    for (i = 0; i < 32; i++)
    {
      sefDestroyEffectData(scheduler->effects[i]);
    }
    scheduler->scriptBinding.state = 0;
    scheduler->inUse = 0;
    scheduler->scriptBinding.script_id = -1;
    scheduler->scriptBinding.task_id = -1;
  }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefFreeScheduler);

void sefDestroyScriptScheduler(int script_index)
{
    SchedulerState *scheduler;
    ScriptBinding *record;
    int index;

    index = 0;
    scheduler = (SchedulerState *)_scheduler;
    record = &scheduler->scriptBinding;
    do {
        if (record->script_id == script_index) {
            sefFreeScheduler(index);
        }
        index++;
        record = (ScriptBinding *)((unsigned char *)record + 0xab0);
    } while (index < 0x80);
}

void sefDestroyScriptScheduler2(int script_index, int task_index)
{
    SchedulerState *scheduler;
    ScriptBinding *record;
    int index;

    index = 0;
    scheduler = (SchedulerState *)_scheduler;
    record = &scheduler->scriptBinding;
    do {
        if (record->script_id == script_index && record->task_id == task_index) {
            sefFreeScheduler(index);
        }
        index++;
        record = (ScriptBinding *)((unsigned char *)record + 0xab0);
    } while (index < 0x80);
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefInitEffectTbl);

INCLUDE_ASM("asm/main/nonmatchings/sef", sefCreateScheduler2);

int sefCreateScheduler(int effect_no, void *parameters, void *position, int table, int index) {
    return sefCreateScheduler2(effect_no, parameters, position, table, 0, index);
}

static int sefGetSizeOffset(int index) {
    return offset_2[index];
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefCreateReactionEffect);

float sefGetMatrixScale(float *matrix)
{
    return MMathCalcLength(matrix);
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefGetParentMatrix);

INCLUDE_ASM("asm/main/nonmatchings/sef", sefExecScheduler);

int sefIsDeadSchduler(unsigned int scheduler_index)
{

    if (scheduler_index >= 0x80) {
        return 1;
    }
    return ((SefSchedulerRecord *)_scheduler)[scheduler_index].inUse == 0;
}

static int sefSetReverseDir(int effect_id, int category)
{
    int normalized_id;
    unsigned int index;

    if ((unsigned int)(category - 0x18) < 8) {
        normalized_id = effect_id;
        if ((unsigned int)(effect_id - 0xB54) < 0x63) {
            normalized_id = effect_id - 0x64;
        }

        for (index = 0; index < 10; index++) {
            if (normalized_id == revEft_3[index]) {
                _revDirZ = 1;
                return 1;
            }
        }
    }

    _revDirZ = 0;
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefCreateBattleActorTbl);

static void sefInitBattlePrm(void)
{
    SefBattleActor *record;
    SefActor *actor;
    Vector4 *position;
    int count = 0;
    int i;

    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&_battleActor->center) : "memory");
    for (i = 0; i < _battleActor->count; i++) {
        record = &_battleActor->actors[i];
        actor = record->actor;
        record->lightCount = 0;
        record->hitSignal = 0;
        record->seSignal = 0;
        if (actor != 0 && record->excluded == 0) {
            position = &actor->position;
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&_battleActor->center) : "memory");
            __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
            __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&_battleActor->center) : "memory");
            count++;
        }
    }
    if (count != 0) {
        MMathDivVectorS(&_battleActor->center, &_battleActor->center, (float)count);
    }
}

ACCEPTED_ASM("src/main/sef", sefCaclAllTarget);

static void sefSetLightFlag(void)
{
    SefBattleActor *record;
    SefActor *actor;
    int i;

    for (i = 0; i < _battleActor->count; i++) {
        record = &_battleActor->actors[i];
        actor = record->actor;
        if (actor != 0 && record->lightCount > 0) {
            actor->flags |= SEF_ACTOR_LIGHT_FLAG;
        } else {
            actor->flags &= ~SEF_ACTOR_LIGHT_FLAG;
        }
    }
}

static void sefSetHitSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                record->hitSignal++;
                break;
            }
        }
    }
}

int sefIsHitActor(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                return record->hitSignal != 0;
            }
        }
    }
    return 0;
}

static void sefSetSeSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                record->seSignal++;
                break;
            }
        }
    }
}

int sefIsSeSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                return record->seSignal != 0;
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefAddLightActor);

void sefInitEffect(void)
{
    smInitilize(_eftBuffer, 0xD4800);
    sresInitMemoryRes();
    srsInitCdRead();
    sevInitPtAllocator();
    svInitImageMapper();
    sefInitScheduler();
    scInitScript();
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    sdvInitSpecialWork();
    sdvInitAmbient();
    sresLoadCommonMemory();
    _sefLoadEftQue = 0;
    _initialize = 1;
}

void sefInitEffectBattle(void)
{
    _sefBattleMode = 1;
    sdvInitAlters();
    sefDestroyEffect();

    sRender.battleDrawCallback = sefDrawEffect2D;
    sRender.cfDrawCallback = 0;
    if (_srsMemRes.battleImage == 0) {
        _srsMemRes.no[SRS_RES_BATTLE_IMAGE] = -1;
        _srsMemRes.battleImage = smAlloc(0x40000);
    }
}

void sefInitEffectCf(void)
{
    _sefBattleMode = 0;
    sefDestroyEffectCf();
    sresLoadCfMemory();

    sRender.battleDrawCallback = 0;
    sRender.cfDrawCallback = sefDrawEffect2D;
}

/* _battlePrm is the 0x34-byte battle parameter block read as SefBattleParameters. */
static inline SefBattleParameters *sefBattleParameters(void)
{
    return (SefBattleParameters *)_battlePrm;
}

void sefSetupPlayer(int character_id, int setup_value_1,
                    int setup_value_2, int setup_value_3)
{
    int index;

    if (character_id == 0xbd) {
        return;
    }
    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->player_character_ids[index] == character_id) {
            return;
        }
    }
    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->player_character_ids[index] == 0) {
            SefBattleParameters *parameters = sefBattleParameters();

            parameters->player_character_ids[index] = character_id;
            parameters->player_setup_parameters[index][0] = setup_value_1;
            parameters->player_setup_parameters[index][1] = setup_value_2;
            parameters->player_setup_parameters[index][2] = setup_value_3;
            parameters->player_active[index] = 1;
            sefSetupEffect();
            return;
        }
    }
    tracePrint(D_004CBF60, character_id);
}

void sefSetupEnemy(int character_id)
{
    int index;

    if (character_id <= 0) {
        return;
    }
    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->enemy_effect_numbers[index] == character_id ||
            sefBattleParameters()->enemy_character_ids[index] == character_id) {
            sefBattleParameters()->enemy_character_ids[index] = character_id;
            return;
        }
    }
    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->enemy_character_ids[index] == 0) {
            if (sefBattleParameters()->enemy_effect_numbers[index] > 0) {
                sresFreeReloaderMemoryNo(index + 3);
            }
            if (sefIsBossID(character_id)) {
                sefBattleParameters()->has_boss = 1;
            }
            sefBattleParameters()->enemy_character_ids[index] = character_id;
            sefBattleParameters()->enemy_effect_numbers[index] = character_id;
            sefBattleParameters()->enemy_active[index] = 1;
            sefSetupEffect();
            return;
        }
    }
    tracePrint(D_004CBF78, character_id);
}

void sefReleaseID(int effect_id)
{
    int index;

    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->player_character_ids[index] == effect_id) {
            sresFreeReloaderMemoryNo(index);
            sefBattleParameters()->player_character_ids[index] = 0;
            sefBattleParameters()->player_setup_parameters[index][0] = 0;
            sefBattleParameters()->player_setup_parameters[index][1] = 0;
            sefBattleParameters()->player_setup_parameters[index][2] = 0;
            sefBattleParameters()->player_active[index] = 0;
            return;
        }
    }

    for (index = 0; index < 3; index++) {
        if (sefBattleParameters()->enemy_character_ids[index] == effect_id) {
            sefBattleParameters()->enemy_effect_numbers[index] = effect_id;
            sefBattleParameters()->enemy_character_ids[index] = 0;
            sefBattleParameters()->enemy_active[index] = 0;
            return;
        }
    }

    tracePrint(D_004CBF98, effect_id);
}

void sefSetupEffect(void) {
    sresLoadBattleData(_battlePrm);
}

int sefIsEntryBoss(void) {
    return D_00794370[0];
}

void sefDestroyEffect(void)
{
    _sefLoadEftQue = 0;
    sefKillEffect(-1);
    sdvInitAmbient();
    scDestroyScriptAll();
    sresFreeReloaderMemory(1);
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    memset(_battleActor, 0, 0xDA0);
    sdvDestroyAlters();
}

void sefDestroyEffectCf(void)
{
    _sefLoadEftQue = 0;
    sefKillEffect(-1);
    sdvInitAmbient();
    scDestroyScriptAll();
    sresFreeReloaderMemory(0);
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    memset(_battleActor, 0, 0xDA0);
    sdvDestroyAlters();
}

void sefLoadEffect(int effect_no, int character_id)
{
    if (effect_no > 0) {
        svFileLoadScript(0, sefCnvEtEffectNo(effect_no, character_id));
    }
}

int sefCheckLoad(void)
{
    if (_sefLoadEftQue != 0) {
        return 1;
    }
    return srsLeaveCdRead() > 0;
}

void sefExecEffect(void)
{
    scExecEffect();
    sdvExecAlters();
    _hitFlag = 0;
    _hitSignal = 0;
    _seSignal = 0;
}

void sefProgressEffect(int frame_count)
{
    int remaining;

    if (frame_count > 0) {
        remaining = frame_count;
        do {
            sefExecEffect();
            remaining--;
        } while (remaining != 0);
    }
}

void sefDrawEffect(void)
{
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            sefCalcInvView(&_invView);
            svDrawScheduler();
        }
    }
}

void sefDrawEffect3D(void) {
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            svDrawScheduler3D(SEF_DRAW_FLAG_PENDING);
        }
    }
}

void sefDrawEffect2D(void) {
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            svDrawScheduler2D(SEF_DRAW_FLAG_PENDING);
        }
    }
}

static void sefCnvDeathEffectNo(SefEffectRequest *request)
{
    int character_id;
    int kind;
    unsigned int i;

    if (request->effect_no == 0x4000) {
        character_id = request->character_id;
        kind = 0;
        if (sefIsBossID(character_id)) {
            for (i = 0; i < 36; i++) {
                if (request->character_id == _sefBossDeathPrm_4[i].character_id) {
                    request->effect_no = _sefBossDeathPrm_4[i].effect_no;
                    return;
                }
            }
        } else {
            if (request->flags & 0x2000) {
                kind = 1;
            } else if (request->flags & 0x1000) {
                kind = 2;
            } else {
                kind = 0;
            }
        }
        request->effect_no = kind * 3 + sefGetSizeOffset(request->size) + 2603;
        request->target = request->source;
        request->character_id_copy = request->character_id;
        request->size_copy = request->size;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefCnvWaitEffectNo);

void sefDeleteEffectWait(int character_id)
{
    SefEffectRequest request;
    SchedulerState *schedulers;
    SchedulerState *scheduler;
    unsigned int table_address;
    short *effect_no_cursor;
    int effect_no_offset;
    int remaining;
    int byte_offset;

    memset(&request, 0, sizeof(request));
    request.character_id = character_id;
    request.effect_no = 0x4001;
    sefCnvWaitEffectNo(&request);
    if (request.effect_no > 0) {
        schedulers = (SchedulerState *)_scheduler;
        table_address = (unsigned int)schedulers;
        effect_no_cursor = &schedulers[0].effectNo;
        effect_no_offset = offsetof(SchedulerState, effectNo);
        byte_offset = 0;
        remaining = 127;
        for (;;) {
            scheduler = (SchedulerState *)((unsigned char *)effect_no_cursor - effect_no_offset);
            if (scheduler->inUse != 0
                && *effect_no_cursor == request.effect_no) {
                sefDeleteEffect2((SchedulerState *)(byte_offset + table_address));
            }
            if (--remaining < 0) {
                break;
            }
            effect_no_cursor = (short *)((unsigned char *)effect_no_cursor
                + sizeof(SchedulerState));
            byte_offset += sizeof(SchedulerState);
        }
    }
}

static int sefCnvEtEffectNo(int effect_no, int character_id)
{
    if ((unsigned int)effect_no - 0x7ebu < 3u) {
        if (character_id != 2) {
            effect_no -= 3;
        }
    } else if ((unsigned int)effect_no - 0x7e8u < 3u) {
        if (character_id == 2) {
            effect_no += 3;
        }
    } else if ((unsigned int)effect_no - 0x7f8u < 2u) {
        if (character_id != 1) {
            effect_no -= 2;
        }
    } else if ((unsigned int)effect_no - 0x7f6u < 2u && character_id == 1) {
        effect_no += 2;
    }
    return effect_no;
}

void sefCreateEffect(SefEffectRequest *request)
{
    SefEffectRequest *script_request;

    if (request->effect_no > 0) {
        if (request->effect_no == 55) {
            request->special = 1;
        }
        script_request = (SefEffectRequest *)_battleData;
        *script_request = *request;
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(_battleData + 0x30) : "memory");
        sefCnvDeathEffectNo(script_request);
        scCreateScript(script_request);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefCreateEffect2);

void sefDeleteEffect2(SchedulerState *scheduler)
{
    sefFreeSchedulerCf(scheduler);
}

int sefLoadEffectCf(int cf_id, int effect_no)
{
    srsFileLoadCf(cf_id, effect_no);
    return 0;
}

int sefLoadEffectCfName(int cf_id, char *effect_name)
{
    int effect_no = srsEffectNameToID(effect_name);

    if (effect_no > 0) {
        srsFileLoadCf(cf_id, effect_no);
    }
    return 0;
}

int sefLoadMemoryEffectCf(void *data, int effect_no, int size)
{
    srsMemoryLoadCf(data, effect_no, size);
    return 0;
}

int sefLoadMemoryEffectCfName(void *data, char *effect_name, int size)
{
    int effect_no = srsEffectNameToID(effect_name);

    if (effect_no > 0) {
        srsMemoryLoadCf(data, effect_no, size);
    }
    return 0;
}

static SchedulerState *sefCreateEffectCf2(int effectNo,
                                           const Vector4 *position,
                                           const Vector4 *orientation,
                                           int schedulerIndex)
{
    SchedulerState *scheduler = 0;
    short *script;
    int count;
    SefEffectRequest request;
    int index;

    if (effectNo <= 0) {
        return 0;
    }
    script = scAnalyzeScriptCf(effectNo, &count);
    if (script == 0) {
        return 0;
    }
    memset(&request, 0, sizeof(request));
    request.effect_no = effectNo;
    index = sefCreateScheduler((int)(script + count), &request, (void *)position,
                               (int)script, schedulerIndex);
    if (index >= 0) {
        scheduler = &((SchedulerState *)_scheduler)[index];
        scheduler->flags |= 0x412;
        if (orientation != 0) {
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&scheduler->vectors[1]), "r"(orientation) : "$8", "memory");
        }
    }
    return scheduler;
}

SchedulerState *sefCreateEffectCf(int effectNo, const Vector4 *position,
                                const Vector4 *orientation)
{
    return sefCreateEffectCf2(effectNo, position, orientation, -1);
}

void sefDeleteEffectCf(SchedulerState *scheduler)
{
    sefFreeSchedulerCf(scheduler);
}

void sefRewindEffectCf(SchedulerState *scheduler)
{
    if (scheduler != 0) {
        scheduler->flags |= SEF_SCHEDULER_REWIND_FLAG;
    }
}

void sefClearEffectCf(SchedulerState *scheduler)
{
    EffectData *effect;
    unsigned char *lines;
    SefLocalData *line;
    int handle;
    int i;
    int j;

    if (scheduler != 0) {
        effect = (EffectData *)scheduler->effects;
        for (i = 0; i < 0x20; i++) {
            if (effect->owner != 0) {
                for (j = 0, lines = _lineData; j < 0x20; j++) {
                    handle = effect->lineHandles[j];
                    if (handle >= 0) {
                        line = (SefLocalData *)(handle * SEF_LINE_DATA_RECORD_SIZE + (int)lines);
                        if (line != 0) {
                            sefDestroyLocalData(line);
                        }
                    }
                }
            }
            effect++;
        }
    }
}

int sefIsFinishEffect(int effect_no)
{
    unsigned int scheduler_address;
    int *scheduler_cursor;
    int active_count = 0;
    int scheduler_index;

    if (func_A33248() != 0) {
        return 0;
    }

    scheduler_index = 0x7f;
    scheduler_address = (unsigned int)_scheduler;
    scheduler_cursor = (int *)(scheduler_address + 0xa70);
    do {
        if (scheduler_cursor[-0xf0] != 0) {
            if ((scheduler_cursor[7] & 0x40) == 0) {
                if (effect_no == 0 || ((short *)scheduler_cursor)[4] == effect_no) {
                    active_count++;
                }
            }
        }
        scheduler_index--;
        scheduler_cursor += 0x2ac;
    } while (scheduler_index >= 0);

    return active_count == 0;
}

int sefIsFinishEffect2(int effect_no)
{
    int record_offset;
    SefEffectScheduler *schedulers = (void *)_scheduler;
    SefEffectScheduler *record;
    SefEffectControl *control;
    unsigned int control_address;
    int index;
    int running;

    running = 0;
    if (effect_no <= 0) {
        return 1;
    }
    record_offset = (unsigned char *)&schedulers->effect - (unsigned char *)schedulers;
    control_address = (unsigned int)_scheduler + record_offset;
    for (index = 0; index < SEF_SCHEDULER_COUNT; index++) {
        control = (SefEffectControl *)control_address;
        record = (SefEffectScheduler *)(control_address - record_offset);
        if (record->inUse != 0
            && sefCnvEtEffectNo(control->effect_no, control->character_id)
                   == effect_no) {
            running++;
        }
        control_address += sizeof(*schedulers);
    }
    return running == 0;
}

void sefSetLoadQue(int effect_no)
{
    _sefLoadEftQue = effect_no;
}

int *sefGetLoadQue(void)
{
    return &_sefLoadEftQue;
}

void sefExecLoadQue(void)
{
    if (_sefLoadEftQue > 0) {
        svFileLoadScript(0, _sefLoadEftQue);
    }
}

void sefKillEffect(int effect_no)
{
    int record_offset;
    SefEffectScheduler *schedulers = (void *)_scheduler;
    SefEffectScheduler *record;
    SefEffectControl *control;
    int index;

    record_offset = (unsigned char *)&schedulers->effect - (unsigned char *)schedulers;
    control = (void *)(_scheduler + record_offset);
    index = 0;
    do {
        record = (void *)((unsigned char *)control - record_offset);
        if (record->inUse != 0
            && (effect_no < 0 || control->effect_no == effect_no)) {
            sefFreeScheduler(index);
        }
        index++;
        if (index < SEF_SCHEDULER_COUNT) {
            control = &schedulers[index].effect;
        }
    } while (index < 0x80);
}

void sefHitEffect(void) {
    _hitFlag = 1;
}

INCLUDE_ASM("asm/main/nonmatchings/sef", sefPushEffect);

void sefPopEffect(void)
{
    unsigned char *save_params;
    unsigned char *save_params_10;
    unsigned char *save_params_20;
    unsigned char *effect_data;
    unsigned char *effect_cursor;
    unsigned char *save_cursor;
    int record_offset;
    int effect_offset;
    int index;

    sefKillEffect(-1);
    save_params = (unsigned char *)_savePrm;
    save_params_10 = save_params + 0x10;
    save_params_20 = save_params + 0x20;
    effect_data = (unsigned char *)D_0065A9A0;
    effect_cursor = effect_data + 0x620;
    save_cursor = save_params + 0x40;
    record_offset = 0;
    effect_offset = 0;
    for (index = 0; index < SEF_SAVE_PRM_COUNT; index++) {
        if (((SefSavePrm *)(save_cursor - 0x40))->active != 0) {
            sefCreateEffectCf2(*(short *)(save_cursor + 0x08),
                               (Vector4 *)(record_offset + (int)save_params),
                               (Vector4 *)(record_offset + (int)save_params_10), index);
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "sq $8, 0(%0)"
                : : "r"(effect_offset + (int)effect_data), "r"(record_offset + (int)save_params_20) : "$8", "memory");
            *(SefSaveActive *)(effect_cursor - 0x08) = *(SefSaveActive *)(save_cursor - 0x10);
            *(int *)(effect_cursor - 0x04) = *(int *)(save_cursor - 0x0C);
            *(int *)(effect_cursor + 0x00) = *(int *)(save_cursor - 0x08);
            *(int *)(effect_cursor + 0x04) = *(int *)(save_cursor - 0x04);
            *(int *)(effect_cursor + 0x08) = *(int *)(save_cursor + 0x00);
            *(SefSaveTail *)(effect_cursor + 0x3CC) = *(SefSaveTail *)(save_cursor + 0x04);
            *(unsigned char *)(effect_cursor + 0x3DA) = *(save_cursor + 0x0A);
        }
        effect_cursor += 0xAB0;
        effect_offset += 0xAB0;
        save_cursor += 0x50;
        record_offset += 0x50;
    }
    sefMemZero(_savePrm, 0x2800);
    sefProgressEffect(10);
}

unsigned char *sefGetBattleData(void)
{
    return _battleData;
}

int sefGetDmgNull(void)
{
    return (D_0079411C[0] >= 2) ? SEF_TARGET_DAMAGE_NULL_A : SEF_TARGET_DAMAGE_NULL_B;
}

unsigned char *sevGetPtAllocator(int allocator_index)
{
    return &_ptAlloc[allocator_index * SEF_PT_ALLOC_RECORD_SIZE];
}

unsigned char *sevGetPtAllocator2(int allocator_index)
{
    return &_ptAlloc[allocator_index * SEF_PT_ALLOC_RECORD_SIZE];
}

unsigned char *sefGetLineAdr(int line_index)
{
    if (line_index < 0) {
        return 0;
    }
    return &_lineData[line_index * SEF_LINE_DATA_RECORD_SIZE];
}

unsigned char *sefGetScheduler(void)
{
    return _scheduler;
}

SchedulerState *sefGetNowScheduler(void)
{
    return _nowScheduler;
}

unsigned char *sefGetParentLine(int line, int parent_local)
{
    if (line >= 0 && parent_local >= 0) {
        return &_ptAlloc[parent_local * SEF_PT_ALLOC_RECORD_SIZE];
    }
    return 0;
}

void sefScaleIVectorAdd(void *dest, void *scale_source, void *addend, float scale) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tlqc2 vf3, 0(%1)\n\tvitof0.xyzw vf1, vf1\n\tqmtc2.ni %2, vf2\n\tvmulx.xyz vf1, vf1, vf2x\n\tvadd.xyz vf3, vf1, vf3\n\tsqc2 vf3, 0(%3)\n\tnop" :  : "r"(scale_source), "r"(addend), "r"(scale), "r"(dest) : "memory");
}

void sefDeg2RadVector(Vector4 *dst, Vector4 *src)
{
    __asm__ __volatile__("lqc2 $vf2, 0(%0)" : : "r"(src) : "memory");

    {
        register float degToRad asm("$f8") = lit4_004d8354;

        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf1x\n\t"
            "sqc2 $vf2, 0(%0)\n\t"
            "nop"
            :
            : "r"(dst), "f"(degToRad)
            : "$8", "memory"
        );
    }
}

void sefMergeMatrixPos(Matrix4 *dest, Matrix4 *matrix, Vector4 *position)
{
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 0(%2)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        :
        : "r"(dest), "r"(matrix), "r"(position)
        : "$8", "$9", "$10", "$11", "memory"
    );
}

void sefLerpVectorB(void *first, Vector4 *dest, float factor)
{
    void *second = (char *)first + 10;

    __asm__ __volatile__(
        "mfc1 $9, %3\n\t"
        "qmtc2.ni $9, $vf3\n\t"
        "ldl $8, 7(%0)\n\t"
        "ldr $8, 0(%0)\n\t"
        "ldl $9, 7(%1)\n\t"
        "ldr $9, 0(%1)\n\t"
        "dsrl $8, $8, 16\n\t"
        "dsrl $9, $9, 16\n\t"
        "pcgth $10, $0, $8\n\t"
        "pcgth $11, $0, $9\n\t"
        "pextlh $10, $10, $8\n\t"
        "pextlh $11, $11, $9\n\t"
        "qmtc2.ni $10, $vf1\n\t"
        "qmtc2.ni $11, $vf2\n\t"
        "vitof0.xyzw $vf1, $vf1\n\t"
        "vitof0.xyzw $vf2, $vf2\n\t"
        "vsub.xyz $vf2, $vf2, $vf1\n\t"
        "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
        "vadd.xyz $vf2, $vf2, $vf1\n\t"
        "sqc2 $vf2, 0(%2)\n\t"
        "nop"
        :
        : "r"(first), "r"(second), "r"(dest), "f"(factor)
        : "$8", "$9", "$10", "$11", "memory"
    );
}

void sefLerpIVectorB(void *first, Vector4 *dest, float factor)
{
    void *second = (char *)first + 10;

    __asm__ __volatile__(
        "ldl $8, 7(%0)\n\t"
        "ldr $8, 0(%0)\n\t"
        "ldl $9, 7(%1)\n\t"
        "ldr $9, 0(%1)\n\t"
        "mfc1 $10, %3\n\t"
        "qmtc2.ni $10, $vf3\n\t"
        "pcgth $10, $0, $8\n\t"
        "pcgth $11, $0, $9\n\t"
        "pextlh $10, $10, $8\n\t"
        "pextlh $11, $11, $9\n\t"
        "qmtc2.ni $10, $vf1\n\t"
        "qmtc2.ni $11, $vf2\n\t"
        "vitof0.xyzw $vf1, $vf1\n\t"
        "vitof0.xyzw $vf2, $vf2\n\t"
        "vsub.xyz $vf2, $vf2, $vf1\n\t"
        "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
        "vadd.xyz $vf2, $vf2, $vf1\n\t"
        "vftoi0.xyzw $vf2, $vf2\n\t"
        "sqc2 $vf2, 0(%2)\n\t"
        "nop"
        :
        : "r"(first), "r"(second), "r"(dest), "f"(factor)
        : "$8", "$9", "$10", "$11", "memory"
    );
}

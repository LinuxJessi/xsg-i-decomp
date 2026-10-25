#include "common.h"
#include "main/xgl_dma.h"
#include "main/xgl_2.h"
#include "main/sef.h"
#include "main/enemy_1.h"
#include "main/map_2.h"

#include "shared.h"

/*
 * UwamonoMapUnit is this TU's local view of the same MapUnit[] record that
 * src/main/init_drill.c also touches; only the fields SendBrokenSignal,
 * GetUwamonoSignal and CreateUwamonoCommon read or write are named, and the
 * rest is unmodeled padding recovered as its exact byte span so the offsets
 * below stay exact.
 */

typedef union UwamonoAlignedVector {
    Vector4 value;
    long long words[2];
} UwamonoAlignedVector;



typedef struct UwamonoNyuruAlignedActor {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x10];
    UwamonoAlignedVector displacement;
} UwamonoNyuruAlignedActor;



typedef struct UwamonoScreenPoint {
    int x;
    int y;
    int z;
    int w;
} UwamonoScreenPoint;



typedef union UwamonoAlignedScreenPoint {
    UwamonoScreenPoint value;
    int coordinates[4];
    long long words[2];
} UwamonoAlignedScreenPoint;



typedef struct UwamonoSpritePacket {
    unsigned char unmodeled_00[0x30];
    UwamonoScreenPoint color0;
    UwamonoScreenPoint position0;
    UwamonoScreenPoint color1;
    UwamonoScreenPoint position1;
} UwamonoSpritePacket;



typedef struct UwamonoLinePacket {
    unsigned char unmodeled_00[0x20];
    UwamonoScreenPoint color0;
    UwamonoScreenPoint position0;
    UwamonoScreenPoint color1;
    UwamonoScreenPoint position1;
} UwamonoLinePacket;



extern UwamonoSpritePacket sPoint_0;

extern UwamonoLinePacket sSortLine_1;

extern signed char lineflg;

typedef struct UwamonoCrossBasis {
    float xAxisX;
    unsigned char unmodeled_04[4];
    float xAxisZ;
    unsigned char unmodeled_0c[0x14];
    float zAxisX;
    unsigned char unmodeled_24[4];
    float zAxisZ;
    unsigned char unmodeled_2c[0x14];
} UwamonoCrossBasis;

typedef struct UwamonoMapUnit {
    u32 flags;                        /* +0x00 */
    unsigned char unmodeled_04[0x0c];
    Vector4 position;                 /* +0x10 */
    unsigned char unmodeled_20[4];
    /*
     * MAP_updateUnitSymbol (VA 0x002bfbe0) adds the scaffold-owned .lit4
     * constant D_004D7EBC (value 0.05) to this field unconditionally
     * (`lwc1`/`add.s`/`swc1`, no branch in the original); no other role is
     * evidenced here.
     */
    float symbolPhase;                /* +0x24 */
    unsigned char unmodeled_28[0x58];
    struct UwamonoCrossBasis *basis;         /* +0x80 */
    unsigned char unmodeled_84[0x1d];
    signed char actionNo;             /* +0xA1 */
    signed char actionSub;            /* +0xA2 */
    unsigned char unmodeled_a3;
    short serial;                     /* +0xA4 */
    unsigned char unmodeled_a6[2];
    short sequenceNo;                 /* +0xA8 */
    short sequenceSub;                /* +0xAA */
    unsigned char unmodeled_ac[4];
    float radius;                     /* +0xB0 */
    float height;                     /* +0xB4 */
    float boxDepth;                   /* +0xB8 */
    unsigned char unmodeled_bc[0xe8];
    signed char signal;               /* +0x1A4 */
    unsigned char unmodeled_1a5[0x28];
    signed char crossType;            /* +0x1CD */
} UwamonoMapUnit;



/* Runtime-loaded object message with a hexadecimal unit identifier. */

/* Loading resident field objects, followed by their hexadecimal identifier. */
const char D_004CA028[24] = "\xBE\xEF\xC3\xF3\xBE\xE5\xCA\xAA\xA5\xED\xA1\xBC\xA5\xC9 %x\n";

int uwares_tbl[14] = { 0 };

int xtxres_tbl[14] = { 0 };

extern int printf(const char *format, ...);

/*
 * MAP_LoadUwamonoResource's own additive view of the same MapUnit[] record
 * UwamonoMapUnit above already partially names; only the fields
 * MAP_LoadUwamonoResource (VA 0x002be5f0) writes are named here: it stores
 * uwares_tbl[index] and xtxres_tbl[index], where index = resourceId -
 * 0x7001.
 */

typedef struct UwamonoResourceUnit {
    unsigned char unmodeled_00[0xe0];
    int uwaresId;                     /* +0xE0 */
    int xtxresId;                     /* +0xE4 */
} UwamonoResourceUnit;



/*
 * Valid resourceId values run 0x7001..0x700E (14 entries, the size of
 * uwares_tbl and xtxres_tbl); 0x7012 is excluded explicitly ahead of the
 * broader >= 0x1000 range check.
 */

/*
 * GetPartsPos's own additive view of the global game-loop record (TU-local
 * by canon, config/header-canon.json): only the map-parts resource root
 * pointer at +0x54 is read here.
 */

typedef struct MapPartsResource {
    unsigned char unmodeled_00[0x40];
    int partCount;                /* +0x40 */
    unsigned char unmodeled_44[0x0c];
    int arrayOffset;              /* +0x50, added to this pointer to form the entry array base */
} MapPartsResource;



typedef struct MapPartsRoot {
    unsigned char unmodeled_00[4];
    struct MapPartsResource *resource;   /* +0x04 */
} MapPartsRoot;



typedef struct UwamonoSoundActor {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x60];
    unsigned char actorIndex;
    unsigned char unmodeled_81[0x4d0 - 0x81];
    unsigned short flags;
} UwamonoSoundActor;



typedef UwamonoSoundActor AimHeightReference;



typedef struct UwamonoGameLoopState {
    unsigned char unmodeled_00[4];
    struct UwamonoSoundActor *actor; /* +0x04 */
    unsigned char unmodeled_08[8];
    unsigned int systemFlags;             /* +0x10 */
    unsigned char unmodeled_14[8];
    int enemyUpdateState;             /* +0x1C */
    unsigned int aimHeightFlags;      /* +0x20 */
    unsigned char unmodeled_24[0x30];
    struct MapPartsRoot *partsRoot;          /* +0x54 */
    unsigned char unmodeled_58[0x8c];
    short shootUnit[64];              /* +0xE4 */
    unsigned char unmodeled_164[0x29ddd];
    signed char aimMapUnitIndex;
    unsigned char unmodeled_29f42[2];
    float selectionDistance;      /* +0x29F41 */
} UwamonoGameLoopState;



extern UwamonoGameLoopState GameLoopState;

/*
 * GetPartsPos's own additive view of the same MapUnit[] record other
 * functions in this TU already partially name; only the fields it writes
 * are named here: position (+0x10, the full Vector4 copied from the
 * resource entry, w set to 1.0f) and rotation's three touched components
 * (+0x20/+0x24/+0x28, x and z always cleared, y set from atan2f); entry
 * (+0x80, the resource entry pointer stored for later use); serial (+0xA4,
 * this unit's index into the resource entry array).
 */

typedef struct GetPartsPosEntry {
    unsigned char unmodeled_00[0x20];
    float basisX;             /* +0x20 */
    unsigned char unmodeled_24[4];
    float basisZ;              /* +0x28 */
    unsigned char unmodeled_2c[4];
    float x;                    /* +0x30 */
    float y;                     /* +0x34 */
    float z;                      /* +0x38 */
    unsigned char unmodeled_3c[4];
} GetPartsPosEntry;



typedef struct GetPartsPosUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;      /* +0x10, w set to 1.0f */
    float rotationX;         /* +0x20, always cleared */
    float rotationY;          /* +0x24, atan2f(entry->basisX, entry->basisZ) */
    float rotationZ;           /* +0x28, always cleared */
    unsigned char unmodeled_2c[0x54];
    struct GetPartsPosEntry *entry;      /* +0x80 */
    unsigned char unmodeled_84[0x20];
    short serial;                  /* +0xA4 */
} GetPartsPosUnit;



extern float atan2f(float y, float x);

extern void sefDeleteEffectCf(int effectId);

extern void xglSoundEffectStopID(int soundId, int channel);

/*
 * ClearUwamonoEffect's own additive view of the same MapUnit[] record
 * UwamonoMapUnit above already partially names; only the fields
 * ClearUwamonoEffect (VA 0x002be8c0) reads or writes are named here.
 */

typedef struct UwamonoEffectUnit {
    unsigned char unmodeled_00[0xa0];
    /* Read once (`lbu`), plus one, as the channel argument to
     * xglSoundEffectStopID; no other role is evidenced here. */
    unsigned char seChannel;           /* +0xA0 */
    unsigned char unmodeled_a1[0x47];
    /* Walked in order, deleting and zeroing each non-zero effect handle
     * with sefDeleteEffectCf. */
    int effectCf[3];                   /* +0xE8 */
    unsigned char unmodeled_f4[0xac];
    /*
     * The original computes this sub-pointer once, ahead of the effect
     * loop, and keeps it live (register evidence) across the whole
     * function; UwamonoTimers below names the same +0x1A0 block for
     * UwamonoCommonFunc's view.
     */
    struct UwamonoEffectTimers {
        unsigned char unmodeled_00[0x48];
        int bgmTimer;                   /* +0x48 (record +0x1E8) */
    } timers;                           /* +0x1A0 */
} UwamonoEffectUnit;



typedef struct InitMapPartsUnit {
    unsigned int flags;
    void (*update)(struct InitMapPartsUnit *);
    unsigned char unmodeled_08[0x98];
    unsigned char hideMode;          /* +0xa0 */
    unsigned char unmodeled_a1[3];
    short serial;                    /* +0xa4 */
    unsigned char unmodeled_a6[0x3a];
    int uwaresId;
    unsigned char unmodeled_e4[0xbc];
    short mapType;
} InitMapPartsUnit;



extern void InitMapDoor(InitMapPartsUnit *unit);

void InitMapKoware(InitMapPartsUnit *mapUnit);

extern void InitDrill(InitMapPartsUnit *unit);

const char D_004CA0D0[24] = "Illegal Parts Type %d\n";

#define UWAMONO_SYMBOL_PHASE_STEP 0.05f

/*
 * HitCheckMapUnitPos (main:0x002c02f0): a thin tail call into
 * HitCheckMapUnitPosSize with its distance threshold fixed at 0.0f (a
 * true `j`, no `jal`, so this function's own return value is whatever
 * HitCheckMapUnitPosSize returns).
 */

int HitCheckMapUnitPosSize(const Vector4 *position, float radius);

/*
 * *(UwamonoLinkedUnit.position below) is a separate record UwamonoCommonFunc
 * keeps synchronized with this unit's position; only the field it writes is
 * named.
 */

typedef struct UwamonoLinkedUnit {
    float direction[3];               /* +0x00 */
    unsigned char unmodeled_0c[0x24];
    Vector4 position; /* +0x30, set from the owning unit's position */
} UwamonoLinkedUnit;



typedef struct UwamonoTimers {
    unsigned char unmodeled_00[0xa];
    signed char projectSound;         /* +0x0A (record +0x1AA) */
    unsigned char unmodeled_0b[0x23];
    signed char heightCheckFlag; /* +0x2E (record +0x1CE) */
    signed char heightBounce;    /* +0x2F (record +0x1CF) */
    unsigned char unmodeled_30[0x18];
    int bgmTimer;                /* +0x48 (record +0x1E8) */
} UwamonoTimers;




typedef struct UwamonoCommonUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    UwamonoAlignedVector position;                 /* +0x10 */
    float rotationX;
    float rotationY;
    float rotationZ;
    unsigned char unmodeled_2c[0x54];
    struct UwamonoLinkedUnit *linkedUnit;     /* +0x80 */
    unsigned char unmodeled_84[0x1c];
    unsigned char seChannel;
    unsigned char unmodeled_a1[3];
    short serial;                     /* +0xA4 */
    unsigned char unmodeled_a6[0xfa];
    UwamonoTimers timers;              /* +0x1A0 */
    unsigned char unmodeled_1ec[0x38];
    float heightVelocity; /* +0x224 */
} UwamonoCommonUnit;



/* Defined in src/main/map_create_unit_peer.c (main/tu270), still in asm. */

extern void MAP_updateUnitPartsSequence(void *unit);

extern void MAP_updateUnitSequence(void *unit);

/* Defined later in this TU (a local sibling still in asm). */

void CheckUwamonoHeight(UwamonoCommonUnit *unit);

/* Defined later in this TU (a local sibling still in asm). */

void UwamonoBgmFunc(UwamonoCommonUnit *unit);

/*
 * UwamonoBgmFadeOut's own additive view of the same MapUnit[] record the
 * structs above already partially name; only the fields it reads are named
 * here. flags (+0x00) is the same field UwamonoMapUnit and DrillMapUnit
 * name; bit 0x10000 is the created/active flag CreateUwamonoCommon sets.
 * seChannel (+0xA0) is the same field ClearUwamonoEffect's UwamonoEffectUnit
 * names, used the same way (+1 as a channel-shaped argument). bgmTimer
 * (+0x1E8) is the same field UwamonoTimers/UwamonoEffectTimers name; here
 * its high halfword is a SoundWork effect-bank index and its low halfword
 * a sub-id, combined below into the effect_id SsdFadeoutEffect takes.
 */

typedef struct UwamonoBgmFadeOutUnit {
    u32 flags;                        /* +0x00 */
    unsigned char unmodeled_04[0x9c];
    unsigned char seChannel;          /* +0xA0 */
    unsigned char unmodeled_a1[0x147];
    int bgmTimer;                     /* +0x1E8 */
    unsigned char unmodeled_1ec[0x114];
} UwamonoBgmFadeOutUnit;



typedef struct UwamonoMapSlot UwamonoMapSlot;




typedef struct UwamonoBoxBasis {
    float x;
    unsigned char unmodeled_04[4];
    float z;
} UwamonoBoxBasis;

struct UwamonoMapSlot {
    unsigned int flags;
    void (*update)(void *);
    void (*draw)(void *);
    unsigned char unmodeled_0c[4];
    UwamonoAlignedVector position;
    float rotationX, rotationY, rotationZ;
    unsigned char unmodeled_2c[0x14];
    Matrix4 localMatrix;
    union {
        struct UwamonoBoxBasis *basis;
        Matrix4 *matrixView;
    };
    unsigned char unmodeled_84[0x1e];
    unsigned char actionSub;
    unsigned char linkedUnitIndex;
    short serial;
    unsigned char unmodeled_a6[0x0a];
    UwamonoAlignedVector dimensions;
    unsigned char unmodeled_c0[0x28];
    int effectCf[3];
    unsigned char unmodeled_f4[0xac];
    struct UwamonoMapCollisionState {
        short kindIndex;
        unsigned char unmodeled_02[2];
        signed char signal;
        unsigned char unmodeled_05[0x28];
        signed char crossType;
        signed char hideOverride;
        unsigned char unmodeled_2f[0x131];
    } state;
};



extern UwamonoMapSlot MapUnit[];

typedef struct UwamonoSoundWorkView {
    unsigned char unmodeled_00[0x20];
    struct {
        unsigned short handle;        /* +0x20 + bank * 4 */
        unsigned char unmodeled_02[2];
    } effectBanks[16];
} UwamonoSoundWorkView;



extern UwamonoSoundWorkView SoundWork;

/* Defined in src/main/ssd_1.c (main/tu ssd_1), already C. */

extern void SsdFadeoutEffect(int effect_id, int source_id, int fade_time);


typedef struct UwamonoInitSymbolState {
    unsigned char unmodeled_00[0x2d];
    signed char state;                 /* +0x2d */
    unsigned char unmodeled_2e[0x16];
    int timer;                         /* +0x44 */
} UwamonoInitSymbolState;



typedef struct UwamonoInitSymbolUnit {
    int flags;                         /* +0x00 */
    void (*updateFunction)(struct UwamonoMapUnit *unit); /* +0x04 */
    unsigned char unmodeled_08[8];
    Vector4 position;                  /* +0x10 */
    float rotationX;                   /* +0x20 */
    float rotationY;                   /* +0x24 */
    float rotationZ;                   /* +0x28 */
    unsigned char unmodeled_2c[0x14];
    float matrix[4][4];                /* +0x40 */
    float (*savedMatrix)[4];           /* +0x80 */
    unsigned char unmodeled_84[0x5c];
    int uwaresId;                      /* +0xe0 */
    int xtxresId;                      /* +0xe4 */
    unsigned char unmodeled_e8[0xb8];
    UwamonoInitSymbolState timerState; /* +0x1a0 */
} UwamonoInitSymbolUnit;



const char D_004CA170[32] = "Create Save Symbol %f %f %f\n";

const char D_004CA190[32] = "Create Shop Symbol %f %f %f\n";

void MAP_updateUnitSaveSymbol(UwamonoMapUnit *unit);

void MAP_updateUnitShopSymbol(UwamonoMapUnit *unit);

void MAP_updateUnitSymbol(UwamonoMapUnit *unit);



typedef struct UwamonoCreatedState {
    short mapType;                    /* +0x00 */
    short initialUnitId;              /* +0x02 */
    unsigned char unmodeled_04;
    signed char signal;
    unsigned char unmodeled_06[0x1e];
    float height;
    unsigned char unmodeled_28[4];
    signed char state;
    signed char initialState;
    signed char heightCheckFlag;
    signed char unmodeled_2f;
    short initialPartId;              /* +0x30 */
    signed char target;
    unsigned char unmodeled_33;
    int initialValue34;
    int initialValue38;
    unsigned char unmodeled_3c[4];
    int initialValue40;
    int timer;
    int initialValue48;
    unsigned char unmodeled_4c[0x34];
} UwamonoCreatedState;



typedef struct UwamonoCreatedUnit {
    unsigned int flags;
    void (*initialize)(struct UwamonoCreatedUnit *);
    unsigned char unmodeled_08[8];
    Vector4 position;
    float rotationX;
    float rotationY;
    float rotationZ;
    unsigned char unmodeled_2c[0x14];
    Matrix4 matrix; /* +0x40 */
    float (*savedMatrix)[4]; /* +0x80 */
    unsigned char unmodeled_84[0x1c];
    unsigned char sequenceIndex;
    unsigned char actionNo;
    unsigned char actionSub;
    unsigned char unmodeled_a3;
    short serial;
    unsigned char unmodeled_a6[2];
    short sequenceNo;
    short sequenceSub;
    unsigned char unmodeled_ac[0xf4];
    UwamonoCreatedState state;
} UwamonoCreatedUnit;



extern const char D_004CA1B0[32];



extern int MAP_loadUnitResource(UwamonoCreatedUnit *unit, int resourceId);

extern void InitMapTrap(UwamonoCreatedUnit *unit);

extern void *memset(void *destination, int value, unsigned int bytes);

typedef struct UwamonoMapUnitState {
    unsigned char unmodeled_00[2];
    short kindIndex;
    signed char signal;                 /* +0x04 */
    signed char hideObjectIndex;
    unsigned char unmodeled_06[6];
    float baseSizeX;
    float baseSizeY;
    float baseSizeZ;
    float sizeOffsetX;
    float sizeOffsetY;
    float sizeOffsetZ;
    unsigned char unmodeled_24[8];
    signed char enemyIndex;
    signed char state;                 /* +0x2d */
    unsigned char unmodeled_2e[2];
    short initialPartId;               /* +0x30 */
    signed char target;                /* +0x32 */
    unsigned char unmodeled_33[9];
    int frameLimit;                    /* +0x3c */
    union { int effectResourceId; int kindParameter; };              /* +0x40 */
    int timer;                         /* +0x44 */
} UwamonoMapUnitState;



typedef union UwamonoKowareDimensions {
    unsigned long long lanes[2];
    unsigned long long words[2];
    Vector4 vector;
} UwamonoKowareDimensions;



typedef struct UwamonoKowareKind {
    signed char shape;
    unsigned char unmodeled_01[0xf];
    UwamonoKowareDimensions dimensions; /* +0x10 */
    signed char target;
    unsigned char unmodeled_21[3];
    int kindParameter;                 /* +0x24 */
    unsigned long long unmodeled_28;
} UwamonoKowareKind;



typedef struct UwamonoKowareUnit {
    unsigned int flags;
    void (*updateFunction)(void *unit);
    unsigned char unmodeled_08[8];
    Vector4 position;                  /* +0x10 */
    Vector4 rotation;                  /* +0x20 */
    unsigned char unmodeled_30[0x70];
    unsigned char sequenceIndex;       /* +0xA0 */
    unsigned char actionNo;            /* +0xA1 */
    unsigned char actionSub;           /* +0xA2 */
    unsigned char unmodeled_a3;
    short serial;                      /* +0xA4 */
    unsigned char unmodeled_a6[2];
    short breakCount;                  /* +0xA8 */
    unsigned char unmodeled_aa[6];
    UwamonoKowareDimensions dimensions;
    unsigned char unmodeled_c0[0xe0];
    UwamonoMapUnitState state;         /* +0x1a0 */
} UwamonoKowareUnit;



extern UwamonoKowareKind KowareKind_t[];

extern const char D_004CA1D0[];

void MAP_updateUnitKoware(void *argument);

void CreateUwamonoCommon(UwamonoMapUnit *unit);

typedef union UwamonoCollisionPosition {
    Vector4 value;
    XglQuadword quadword;
} UwamonoCollisionPosition;

typedef struct UwamonoCollisionUnit {
    unsigned char unmodeled_00[0x10];
    UwamonoCollisionPosition position;
    unsigned char unmodeled_20[0x60];
    struct UwamonoCrossBasis *basis;
    unsigned char unmodeled_84[0x2c];
    UwamonoAlignedVector dimensions;
    unsigned char unmodeled_c0[0xe0];
    UwamonoMapUnitState state;
    unsigned char unmodeled_1e8[0x118];
} UwamonoCollisionUnit;



typedef struct UwamonoAimQuery {
    unsigned char unmodeled_00[8];
    unsigned short attributeMask;
    unsigned char unmodeled_0a[0x0e];
    void *header;
    unsigned char unmodeled_1c[0x24];
} UwamonoAimQuery;
typedef struct UwamonoCollisionActor {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x10];
    Vector4 displacement;              /* +0x30 */
    unsigned char unmodeled_40[0x488];
    UwamonoAimQuery undulation; /* +0x4c8 */
    unsigned char unmodeled_508[0x4e0];
    float radius;
    unsigned char unmodeled_9ec[0x84];
} UwamonoCollisionActor;



extern float CheckDist2D(const Vector4 *first, const Vector4 *second);

int HitCheckBoxCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

unsigned int HitCheckMapUnitPosAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

typedef struct UwamonoScanFields {
    unsigned char channel;            /* +0x00 */
    unsigned char unmodeled_01[3];
    short resourceId;                 /* +0x04 */
    unsigned char unmodeled_06[0xfa];
    UwamonoMapUnitState state;         /* +0x100; state byte at +0x12D */
} UwamonoScanFields;



typedef struct UwamonoScanUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    Vector4 position;
    unsigned char unmodeled_20[0x80];
    UwamonoScanFields scan;            /* +0xA0 */
    unsigned char unmodeled_1e8[0x118];
} UwamonoScanUnit;



typedef struct UwamonoBoxCollisionState {
    unsigned char unmodeled_00[0x2d];
    signed char crossType;
} UwamonoBoxCollisionState;

typedef struct UwamonoBoxUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;                 /* +0x10 */
    unsigned char unmodeled_20[0x60];
    struct UwamonoBoxBasis *basis;           /* +0x80 */
    unsigned char unmodeled_84[0x2c];
    Vector4 dimensions;               /* +0xB0 */
    unsigned char unmodeled_c0[0xe0];
    UwamonoBoxCollisionState state;   /* +0x1A0 */
} UwamonoBoxUnit;

struct UwamonoBoxUnit;

int HitCheckBox(UwamonoCollisionActor *actor, struct UwamonoBoxUnit *boxUnit);

extern const float D_004D7ED0;

extern const float D_004D7ED4;

extern const float D_004D7ED8;

extern const float D_004D7EDC;







int HitCheckCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

typedef struct UwamonoCrossState {
    unsigned char unmodeled_00[0x2d];
    signed char crossType;
} UwamonoCrossState;



typedef struct UwamonoCrossUnit {
    unsigned char unmodeled_00[0x10];
    union {
        Vector4 position;
        UwamonoAlignedVector alignedPosition;
    };
    unsigned char unmodeled_20[0x60];
    struct UwamonoCrossBasis *basis;
    unsigned char unmodeled_84[0x2c];
    UwamonoAlignedVector dimensions;
    unsigned char unmodeled_c0[0xe0];
    UwamonoCrossState state;
} UwamonoCrossUnit;



typedef Vector4 UwamonoNyuruVector;



typedef Vector4 UwamonoBoxVector;



typedef struct UwamonoNyuruActor {
    unsigned char unmodeled_00[0x10];
    UwamonoNyuruVector position;
    unsigned char unmodeled_20[0x10];
    UwamonoNyuruVector displacement;
    unsigned char unmodeled_40[0x488];
    UwamonoAimQuery undulation;
    unsigned char unmodeled_508[0x4e0];
    float radius; /* +0x9e8 */
} UwamonoNyuruActor;



static int NyuruCircle(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit);

static int NyuruMatrix(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit, int mode);

int HitCheckMapUnit(void *actor);

/*
 * Still assembler: it only reads `from` and `to`, and reads the unit's
 * position and radius (+0x10/+0x18, +0xB0), so it takes the collision view.
 */
int CrossCheckMapUnitCircle(const Vector4 *from, const Vector4 *to,
                            UwamonoCollisionUnit *unit);

extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);


int AreaCheckBox(const Vector4 *position, UwamonoMapSlot *unit, float radius);

int CrossCheckMapUnitAt(Vector4 *first, Vector4 *second, UwamonoCrossUnit *unit);

struct UwamonoEnemyUnit;

typedef struct UwamonoCrossActor {
    unsigned int flags;
    void (*update)(struct UwamonoEnemyUnit *);
    unsigned char unmodeled_08[8];
    union {
        Vector4 position;
        UwamonoAlignedVector alignedPosition;
    };
    unsigned char unmodeled_20[0x30];
    float unmodeled_50[3];
    unsigned char unmodeled_5c[0x24];
    unsigned char actorIndex;
    unsigned char unmodeled_81[5];
    short resourceId;
    unsigned char unmodeled_88[0x848];
    void *collisionObject;
    unsigned char unmodeled_8d4[0x114];
    float radius;
    unsigned char unmodeled_9ec[0x84];
} UwamonoCrossActor;



extern UwamonoCrossActor actor[64];

extern const float D_004D7F18;



extern int CheckCrossCircle(float radius, const Vector4 *from, const Vector4 *to, const Vector4 *center);

static float CheckCornerDist(const Vector4 *position, UwamonoCollisionUnit *unit);

typedef struct BrokenMapUnitState {
    unsigned char unmodeled_00[4];
    signed char signal;
    unsigned char unmodeled_05[0x3f];
    int brokenId;
} BrokenMapUnitState;



typedef struct BrokenMapUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0xa0];
    short serial;
    unsigned char unmodeled_a6[0xfa];
    BrokenMapUnitState brokenState;
} BrokenMapUnit;



int CheckBrokenMapUnit(BrokenMapUnit *unit);

extern void xglSoundEffectPosID(int soundId, const float *position, int enabled, int channel);

/* Defined in src/main/ssd_1.c (main/tu ssd_1), already C. */

int HitCheckUwamonoAt(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit);




typedef struct UwamonoVectorWords {
    long long lower;
    long long upper;
} UwamonoVectorWords;



typedef struct UwamonoEnemyLink {
    unsigned char unmodeled_00[0x40];
    union {
        unsigned char linkState[12];
        struct {
            unsigned char unmodeled_link_40[8];
            unsigned char unmodeled_48[3];
            unsigned char unitId; /* +0x4b */
        };
    };
    unsigned char unmodeled_4c[0x3864];
} UwamonoEnemyLink;



typedef struct UwamonoEnemyUnit UwamonoEnemyUnit;



struct UwamonoEnemyUnit {
    unsigned int flags;
    void (*update)(UwamonoEnemyUnit *);
    unsigned char unmodeled_08[8];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x34];
    float heading;
    unsigned char unmodeled_58[0x28];
    unsigned char enemyIndex;
    unsigned char unmodeled_81[0x963];
    float actorHeading;
};



extern UwamonoEnemyLink enepc[16];

extern void ACT_updateEnemy(UwamonoEnemyUnit *unit);

typedef struct UwamonoWind {
    unsigned char mode;
    unsigned char unmodeled_01[7];
    long long unmodeled_08;
    Vector4 position;
    float decay;
    float strength;
    int timer;
    unsigned char unmodeled_2c[4];
} UwamonoWind;



typedef struct UwamonoWindUnit {
    long long unmodeled_00[2];
    Vector4 position;
    unsigned char unmodeled_20[0x94];
    float height;
} UwamonoWindUnit;



UwamonoWind uwaWind = { 0 };

extern void EXM_SetWindStruct(UwamonoWind *wind);

#define D_004D7EC0 0.3f

#define D_004D7EC4 0.1f

typedef struct UwamonoBrokenSoundState {
    unsigned char unmodeled_00[0x32];
    signed char material;
    unsigned char unmodeled_33;
    int soundId;
} UwamonoBrokenSoundState;



typedef struct UwamonoBrokenSoundUnit {
    unsigned char unmodeled_00[0xa0];
    unsigned char seChannel;
    unsigned char unmodeled_a1[0xff];
    UwamonoBrokenSoundState state;
} UwamonoBrokenSoundUnit;



extern void xglSoundEffectNormalID(int soundId, int channel);

#define D_004D7F44 0.1f

int HitCheckBoxUwamono(UwamonoBoxUnit *actor, UwamonoMapSlot *unit);

#define D_004D7F1C 0.6f

extern void nmlModelSetPartsVisible(void *resource, int partsId, int visible);

extern float Get_Angle(const Vector4 *first, const Vector4 *second);

extern float CheckDist3D(const Vector4 *first, const Vector4 *second);

#define D_004D7EB4 0.05f

#define D_004D7EB8 0.05f

void UnlockMapUnit(void);

/*
 * The hit and cross checks read a MapUnit[] slot through the collision view
 * (position, radius/height, shape byte); callers holding the slot as a
 * UwamonoMapSlot or UwamonoMapUnit pass it in that view, the same way
 * CheckNearMapUnit hands its scan slot to CheckCornerDist.
 */
int HitCheckMapUnitAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

void CalcNearCrossPointCircle(const Vector4 *from, const Vector4 *to,
                              UwamonoCollisionUnit *unit, Vector4 *out);

void CalcNearCrossPointBox(const Vector4 *first, const Vector4 *second,
                           UwamonoMapUnit *unit, Vector4 *result);

int CrossCheckMapUnitBox(Vector4 *first, Vector4 *second,
                         UwamonoCrossUnit *unit);

extern const char *D_004C9F20[14];

extern const char D_004C9F58[];

extern const char D_004C9F88[];

extern const char D_004C9FB0[];

extern const char D_004C9FD0[];

extern const char D_004DB6D8[];

extern const char D_004DB6E0[];

extern unsigned char uwamono_dat[];

extern unsigned char ItemBoxTbl[0x12C8];

extern unsigned char EvtItemTbl[0x7F80];

extern char *strcpy(char *destination, const char *source);

void InitUwamono(UwamonoCreatedUnit *unit);

typedef struct UwamonoSequenceRecord {
    int initialValues00[2];
    unsigned char unmodeled_08[4];
    int initialValues0c[10];
    unsigned char unmodeled_34[0x20c];
    int initialValues240[3];
    float scaleX;
    float scaleY;
    int initialValues254[2];
    float scaleZ;
} UwamonoSequenceRecord;



extern signed char printflg;

extern const char D_004C9FF0[];

extern const char D_004CA008[];

extern UwamonoSequenceRecord unitSequence[];

typedef struct UwamonoKowareSizeUnit {
    unsigned int flags;
    void (*updateFunction)(void *unit);
    unsigned char unmodeled_08[8];
    Vector4 position;
    float rotationX;
    float rotationY;
    float rotationZ;
    unsigned char unmodeled_2c[0x74];
    unsigned char hideMode;
    unsigned char unmodeled_a1[3];
    short serial;
    unsigned char unmodeled_a6[0x0a];
    UwamonoKowareDimensions dimensions;
    unsigned char unmodeled_c0[0xe0];
    UwamonoMapUnitState state;
} UwamonoKowareSizeUnit;



extern const char D_004CA040[];

extern const char D_004CA050[];

extern double fptodp(float value);

extern const char D_004CA060[];

extern const char D_004CA088[];

typedef struct UwamonoProbeParameters {
    unsigned char unmodeled_00[8];
    unsigned short flags;              /* +0x08 */
    unsigned char unmodeled_0a[0x0e];
    void *dataHeader;                   /* +0x18 */
    unsigned char unmodeled_1c[0x24];
} UwamonoProbeParameters;



extern void *UnduDataGetHeader(int dataId, int flags);

extern void UnduParamInit(void *parameters);

extern float UnduCheck(Vector4 *position, Vector4 *displacement, void *parameters);

extern void InitItemBox(UwamonoCreatedUnit *unit);

extern void InitItemSymbol(UwamonoCreatedUnit *unit);

void InitSaveSymbol(UwamonoInitSymbolUnit *unit);

void InitShopSymbol(UwamonoInitSymbolUnit *unit);

void InitEvsSymbol(UwamonoInitSymbolUnit *unit);

void InitRetSymbol(UwamonoInitSymbolUnit *unit);

extern void InitSpecialSymbol(UwamonoCreatedUnit *unit);

extern const char D_004CA220[];

extern const char D_004CA230[];

extern const char D_004CA240[];

extern const char D_004CA250[];

extern const float D_004D7EE0;

extern const float D_004D7EE4;

extern const char D_004DB6F0[];

extern const char D_004DB6F8[];

extern const char D_004DB700[];

extern const char D_004DB708[];

extern const char D_004DB710[];

extern const char D_004DB718[];

extern const char D_004DB720[];

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

typedef struct KowarePartsSizeState {
    unsigned char unmodeled_00[2];
    short kindIndex;
    unsigned char unmodeled_04[8];
    float baseSizeX;
    float baseSizeY;
    float baseSizeZ;
    float sizeOffsetX;
    float sizeOffsetY;
    float sizeOffsetZ;
    unsigned char unmodeled_24[8];
    unsigned char unmodeled_2c[1];
    signed char state;
    unsigned char unmodeled_2e[4];
    signed char target;
    unsigned char unmodeled_33[0x0d];
    int kindParameter;
    int timer;
} KowarePartsSizeState;



typedef union KowarePartsSizeUnitDimensions {
    UwamonoKowareDimensions raw;
    Vector4 vector;
} KowarePartsSizeUnitDimensions;



typedef struct KowarePartsSizeUnit {
    unsigned char unmodeled_00[0xb0];
    KowarePartsSizeUnitDimensions dimensions;
    unsigned char unmodeled_c0[0xe0];
    KowarePartsSizeState state;
} KowarePartsSizeUnit;



typedef struct KowareInitState {
    unsigned char unmodeled_00[2];
    short kindIndex;
    unsigned char unmodeled_04[0x28];
    unsigned char unmodeled_2c;
    signed char state;
    unsigned char unmodeled_2e[4];
    signed char target;
    unsigned char unmodeled_33[0x11];
    int timer;
} KowareInitState;



typedef struct KowareInitUnit {
    unsigned int flags;
    void (*updateFunction)(void *unit);
    unsigned char unmodeled_08[8];
    Vector4 position;
    unsigned char unmodeled_20[0x80];
    unsigned char hideMode;
    unsigned char unmodeled_a1[3];
    short serial;
    unsigned char unmodeled_a6[0x0a];
    UwamonoKowareDimensions dimensions;
    unsigned char unmodeled_c0[0xe0];
    KowareInitState state;
} KowareInitUnit;



typedef struct KowareInitResourceCount {
    unsigned char unmodeled_00[0x40];
    int partCount;
} KowareInitResourceCount;



extern const char D_004CA0E8[];

extern const char D_004CA108[];

extern const char D_004CA138[];

void SetHideObject(void *argument);

typedef struct UwamonoHideEffect {
    unsigned char unmodeled_00[0xa9a];
    signed char enabled;
} UwamonoHideEffect;



typedef struct UwamonoCreatedPosition {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;
} UwamonoCreatedPosition;



void MAP_updateUnitHide(void *argument);

extern const char D_004DB6E8[];

extern const char D_004CA1F8[];

void DrawActiveCursol(UwamonoMapUnit *argument);

extern void Vibration_Set_Strong(int strength, int duration, int interval);

extern void EXM_ClearWindStruct(void);

extern void MAP_updateUnitTrap(void *unit);

extern void MAP_updateUnitItemBox(void *unit);

extern void MAP_drawUnitItemBox(void *unit);

extern void MAP_updateUnitItemSymbol(void *unit);

typedef struct DrawActiveCursorUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0x9c];
    unsigned char aimIndex;
    unsigned char unmodeled_a1[0x0f];
    float cursorScale[3];
    unsigned char unmodeled_bc[0xe8];
    signed char signal;
} DrawActiveCursorUnit;



typedef struct DrawActiveCursorEffect {
    unsigned char unmodeled_00[0xa0];
    Vector4 scale;
    unsigned char unmodeled_b0[0x614];
    void *casterUnit;
    unsigned char unmodeled_6c8[0x3c4];
    unsigned int flags;
} DrawActiveCursorEffect;



extern float D_004D7EC8;

extern float D_004D7ECC;



static inline float UwamonoProjectBoxBasisXZ(float x, float z,
                                           const struct UwamonoBoxBasis *basis)
{
    return x * basis->x + z * basis->z;
}

typedef struct UwamonoCornerCollisionUnit {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x60];
    const float (*basis)[4];
    unsigned char unmodeled_84[0x2c];
    UwamonoAlignedVector halfSize;
} UwamonoCornerCollisionUnit;



extern float D_004D7EE8;

extern float xglSin(float angle);

extern float xglCos(float angle);



typedef struct UwamonoBoxDimensions {
    float radius;
    float height;
    float boxDepth;
    unsigned char unmodeled_0c[4];
} UwamonoBoxDimensions;



extern float D_004D7EEC;

extern float D_004D7EF0;

typedef union UwamonoAlignedBoxDims {
    UwamonoBoxDimensions value;
    long long words[2];
} UwamonoAlignedBoxDims;



typedef struct UwamonoAimActor {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x60];
    unsigned char actorIndex;
    unsigned char unmodeled_81[0x4D0 - 0x81];
    unsigned short flags;
    unsigned char unmodeled_4d2[0x16];
    unsigned long long shootModeFlags;    /* +0x4e8 */
    unsigned char unmodeled_4f0[0x200];
    int shootFlags;                       /* +0x6f0 */
    float shootProgress;                  /* +0x6f4 */
    float motionSpeed;                    /* +0x6f8 */
    unsigned char unmodeled_6fc[8];
    unsigned short motion;                /* +0x704 */
    unsigned char unmodeled_706[0x9e4 - 0x706];
    float targetAngle;                    /* +0x9e4 */
} UwamonoAimActor;



typedef struct UwamonoShotEffect {
    unsigned char unmodeled_000[0x6bc];
    struct UwamonoAimActor *owner;
    unsigned char unmodeled_6c0[0x3cc];
    unsigned int flags;
} UwamonoShotEffect;



extern signed char dispflg;

extern signed char collflg;



extern const char D_004CA260[];

extern float D_004D7EF8;

extern float D_004D7EFC;

int AimMapUnit(void);

extern void DispDebugUwamono(void);

extern void DispCollUwamono(void);

extern void Vibration_Set_Weak(int strength);

extern void ACT_setMotion(UwamonoAimActor *actor, unsigned int motion);

extern void Enemy_FindByEar(UwamonoAimActor *actor, Vector4 *position);

int UwamonoAreaCheck(const Vector4 *position, UwamonoMapSlot *unit, float radius);

extern int CheckPointLine(const Vector4 *first, const Vector4 *second,
                          const Vector4 *point);

extern int CheckCrossLine(const Vector4 *first, const Vector4 *second,
                          const Vector4 *edgeStart, const Vector4 *edgeEnd);

extern void CalcCrossPoint(const Vector4 *first, const Vector4 *second,
                           const Vector4 *edgeStart, const Vector4 *edgeEnd, Vector4 *out);

extern const float D_004D7F10;

extern const float D_004D7F14;

extern float D_004D7F20;




extern float D_004D7F2C;

extern float D_004D7F30;

extern float D_004D7F34;

extern float D_004D7F38;

extern float D_004D7F3C;

extern float D_004D7F40;

typedef union UwamonoCollisionCopy {
    UwamonoCollisionUnit value;
    unsigned long long words[0x300 / 8];
} UwamonoCollisionCopy;



extern float D_004D7F28;

extern float D_004D7EF4;

typedef struct UwamonoAimCandidate {
    int unitIndex;
    unsigned char unmodeled_04[0x0c];
    UwamonoAlignedScreenPoint screenPoint;
    float distance;
    unsigned char unmodeled_24[0x0c];
} UwamonoAimCandidate;



float xglRotTransPers(Vector4 *out, const Matrix4 matrix,
                      Vector4 *point, int camera_id);

void InitMapParts(InitMapPartsUnit *unit);

int HitCheckNyuru(UwamonoNyuruAlignedActor *actor, UwamonoCollisionUnit *unit);

void MAP_updateUnitEnemy(UwamonoEnemyUnit *unit);

void UwamonoBrokenSe(UwamonoBrokenSoundUnit *unit);

typedef struct NativeUnitPeer NativeUnitPeer;


void DrawSprite(const UwamonoAlignedScreenPoint *position,
                const UwamonoScreenPoint *color, int width, int height);

static void NyuruCorner(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit);

void CrossPointUwamonoAt(const Vector4 *first, const Vector4 *second,
                         UwamonoMapUnit *unit, void *result);

int CrossPointUwamono(const Vector4 *position, const Vector4 *previous,
                      float radius, Vector4 *result);

void CheckNearPoint(const Vector4 *origin, const Vector4 *candA,
                    const Vector4 *candB, Vector4 *out);

int AimMapUnitLookCheck(UwamonoScanUnit *target);

void SortLine(const UwamonoAlignedScreenPoint *from,
              const UwamonoAlignedScreenPoint *to, int mode);

int HitCheckUwamono(UwamonoScanUnit *actor);

void InitUwamonoSys(void)
{
    char path[128];
    const char *paths[14];
    unsigned char *dataEnd = uwamono_dat;
    int index;
    int size;

    __builtin_memcpy(paths, D_004C9F20, sizeof(paths));
    for (index = 0; index < 14; index++) {
        memset(path, 0, sizeof(path));
        strcpy(path, paths[index]);
        strcat(path, D_004DB6D8);
        uwares_tbl[index] = (int)dataEnd;
        dataEnd += xglCdReadFile(path, dataEnd, 0, 1);
        dataEnd = (unsigned char *) (((unsigned int) dataEnd + 15) & ~15);

        memset(path, 0, sizeof(path));
        strcpy(path, paths[index]);
        strcat(path, D_004DB6E0);
        xtxres_tbl[index] = (int)dataEnd;
        dataEnd += xglCdReadFile(path, dataEnd, 0, 1);
        dataEnd = (unsigned char *) (((unsigned int) dataEnd + 15) & ~15);
    }

    size = (int) (dataEnd - uwamono_dat);
    if (size > 0x50000) {
        printf(D_004C9F58, size / 1024);
    } else {
        printf(D_004C9F88, size / 1024, index);
    }

    memset(ItemBoxTbl, 0, sizeof(ItemBoxTbl));
    memset(EvtItemTbl, 0, sizeof(EvtItemTbl));
    {
        unsigned char *workEnd = WorkEnd;

        xglCdReadFile(D_004C9FB0, workEnd, 0, 1);
        __builtin_memcpy(ItemBoxTbl, workEnd, 0x12C8);
        xglCdReadFile(D_004C9FD0, workEnd, 0, 1);
        __builtin_memcpy(EvtItemTbl, workEnd, 0x7F80);
    }

    GameLoopState.aimHeightFlags |= 3;
    GameLoopState.selectionDistance = 3.0f;
}

UwamonoCreatedUnit *Unit_CreateUwamono(int index, int resourceId)
{
    int slot;
    float identityScale;
    UwamonoCreatedUnit *unit = 0;
    UwamonoCreatedState *state;

    if (resourceId < 0x1000) {
        for (slot = 0; slot < 64; slot++) {
            if (resourceId != MapUnit[slot].serial) {
                continue;
            }
            if ((MapUnit[slot].flags & 0x10000) == 0) {
                continue;
            }
            if (printflg != 0) {
                printf(D_004C9FF0, resourceId);
            }
            unit = (UwamonoCreatedUnit *)&MapUnit[slot];
            break;
        }
    }

    if (unit == 0) {
        unit = (UwamonoCreatedUnit *)MAP_createUnit(index, resourceId);
        if (unit == 0) {
            printf(D_004CA008);
            return 0;
        }
    }

    if (resourceId >= 0x1001 &&
        ((unsigned int)(resourceId - 0x7000) >= 0x14U) &&
        ((unsigned int)(resourceId - 0x7005) >= 3U) &&
        (resourceId != 0x7013)) {
        MAP_loadUnitResource(unit, resourceId);
    }

    identityScale = 1.0f;
    unit->flags = 0x10004;
    unit->savedMatrix = unit->matrix;
    unit->actionNo = 0;
    unit->sequenceNo = 0;
    unit->sequenceSub = 0;
    unit->actionSub = 0;
    unit->position.x = 0.0f;
    unit->position.y = 0.0f;
    unit->position.z = 0.0f;
    unit->position.w = identityScale;

    state = &unit->state;
    memset(state, 0, sizeof(*state));
    state->signal = -1;
    state->initialUnitId = -1;
    state->state = -1;
    state->initialState = -1;
    state->heightCheckFlag = -1;
    state->target = -1;
    state->initialValue48 = -1;
    state->timer = -1;
    state->initialValue34 = -1;
    state->initialValue38 = -1;
    state->initialValue40 = -1;
    state->initialPartId = -1;
    unit->initialize = InitUwamono;
    {
        UwamonoSequenceRecord *sequence = &unitSequence[unit->sequenceIndex];

        sequence->initialValues00[0] = 0;
        sequence->initialValues00[1] = 0;
        sequence->initialValues240[0] = 0;
        sequence->initialValues240[1] = 0;
        sequence->initialValues240[2] = 0;
        sequence->scaleX = identityScale;
        sequence->scaleY = identityScale;
        sequence->initialValues254[0] = 0;
        sequence->initialValues254[1] = 0;
        sequence->initialValues0c[0] = 0;
        sequence->initialValues0c[2] = 0;
        sequence->initialValues0c[3] = 0;
        sequence->initialValues0c[4] = 0;
        sequence->initialValues0c[5] = 0;
        sequence->initialValues0c[6] = 0;
        sequence->initialValues0c[7] = 0;
        sequence->initialValues0c[8] = 0;
        sequence->scaleZ = identityScale;
        sequence->initialValues0c[9] = 0;
    }

    return unit;
}

void MAP_LoadUwamonoResource(UwamonoResourceUnit *unit, int resourceId)
{
    int index;
    int uwaresId;
    int xtxresId;

    printf((const char *) D_004CA028);
    if (resourceId != 0x7012 && resourceId >= 0x1000) {
        index = resourceId - 0x7001;
        uwaresId = uwares_tbl[index];
        xtxresId = xtxres_tbl[index];
        unit->xtxresId = xtxresId;
        unit->uwaresId = uwaresId;
    }
}

void GetPartsPos(GetPartsPosUnit *unit)
{
    MapPartsRoot *root;
    MapPartsResource *resource;
    GetPartsPosEntry *table;
    GetPartsPosEntry *entry;

    root = GameLoopState.partsRoot;
    resource = root->resource;
    table = (GetPartsPosEntry *) ((char *) resource + resource->arrayOffset);
    entry = &table[unit->serial];

    unit->entry = entry;
    unit->position.x = entry->x;
    unit->position.y = entry->y;
    unit->position.z = entry->z;
    unit->position.w = 1.0f;
    unit->rotationX = 0.0f;
    unit->rotationY = atan2f(entry->basisX, entry->basisZ);
    unit->rotationZ = 0.0f;
}

void SendBrokenSignal(UwamonoMapUnit *unit)
{
    unit->signal = 2;
}

void GetPartsSize(UwamonoKowareSizeUnit *unit)
{
    UwamonoMapUnitState *state;
    short kindIndex;

    state = &unit->state;
    kindIndex = state->kindIndex;
    if (state->state == -1) {
        state->state = KowareKind_t[kindIndex].shape;
    }
    if (state->baseSizeX > 0.0f) {
        unit->dimensions.vector.x = state->baseSizeX * 0.5f;
        unit->dimensions.vector.y = state->baseSizeY;
        unit->dimensions.vector.z = state->baseSizeZ * 0.5f;
        if (printflg != 0) {
            const char *format = D_004CA050;
            printf(D_004CA040);
            printf(format, fptodp(state->baseSizeX),
                   fptodp(state->baseSizeY), fptodp(state->baseSizeZ));
        }
    } else {
        unit->dimensions = KowareKind_t[kindIndex].dimensions;
    }
    unit->dimensions.vector.x += state->sizeOffsetX * 0.5f;
    unit->dimensions.vector.y += state->sizeOffsetY;
    unit->dimensions.vector.z += state->sizeOffsetZ * 0.5f;
    if (state->target == -1) {
        state->target = KowareKind_t[kindIndex].target;
    }
    if (state->kindParameter == -1) {
        state->kindParameter = KowareKind_t[kindIndex].kindParameter;
    }
}

void ClearUwamonoEffect(UwamonoEffectUnit *unit)
{
    int i;
    int effectId;
    struct UwamonoEffectTimers *timers = &unit->timers;

    for (i = 0; i < 3; i++) {
        effectId = unit->effectCf[i];
        if (effectId != 0) {
            sefDeleteEffectCf(effectId);
            unit->effectCf[i] = 0;
        }
    }
    if (timers->bgmTimer > 0) {
        xglSoundEffectStopID(timers->bgmTimer, unit->seChannel + 1);
        timers->bgmTimer = -1;
    }
}

signed char GetUwamonoSignal(UwamonoMapUnit *unit)
{
    return unit->signal;
}

void InitUwamono(UwamonoCreatedUnit *unit)
{
    UwamonoCreatedState *map = &unit->state;
    UwamonoProbeParameters parameters;

    unit->flags &= ~4;
    if (map->mapType == 0) {
        printf(D_004CA060, unit->serial, unit->sequenceIndex);
        unit->serial = -1;
        unit->initialize = 0;
        return;
    }

    if (map->heightCheckFlag == -1) {
        map->heightCheckFlag = 0;
    }
    UnduParamInit(&parameters);
    parameters.dataHeader = UnduDataGetHeader(0, 0x8000);
    if (map->unmodeled_2f != 0) {
        parameters.flags |= 0x20;
    }
    if (UnduCheck(&unit->position, 0, &parameters) == -1000.0f && map->unmodeled_2f == 0) {
        map->heightCheckFlag = 0;
    }

    if (unit->serial < 0x1000) {
        InitMapParts((InitMapPartsUnit *)unit);
        return;
    }

    switch (map->mapType) {
    case 4:
        InitItemBox(unit);
        return;
    case 5:
        InitItemSymbol(unit);
        return;
    case 3:
        InitMapTrap(unit);
        return;
    case 6:
        InitSaveSymbol((UwamonoInitSymbolUnit *)(void *)unit);
        return;
    case 7:
        InitShopSymbol((UwamonoInitSymbolUnit *)(void *)unit);
        return;
    case 8:
        InitEvsSymbol((UwamonoInitSymbolUnit *)(void *)unit);
        return;
    case 9:
        InitRetSymbol((UwamonoInitSymbolUnit *)(void *)unit);
        return;
    case 10:
        InitSpecialSymbol(unit);
        return;
    default:
        printf(D_004CA088, map->mapType);
        unit->initialize = 0;
        return;
    }
}

void InitMapParts(InitMapPartsUnit *unit)
{
    unit->uwaresId = 0;
    switch (unit->mapType) {
    case 2:
        InitMapKoware(unit);
        break;
    case 1:
        InitMapDoor(unit);
        break;
    case 20:
        InitDrill(unit);
        break;
    default:
        printf(D_004CA0D0, unit->mapType);
        unit->update = 0;
        break;
    }
}

void InitMapKoware(InitMapPartsUnit *mapUnit)
{
    KowareInitUnit *unit = (KowareInitUnit *)mapUnit;
    MapPartsResource *resource = GameLoopState.partsRoot->resource;
    KowareInitResourceCount *resourceCount = (KowareInitResourceCount *)resource;
    KowareInitState *state = &unit->state;
    short kindIndex = state->kindIndex;
    if (kindIndex < 0) {
        printf(D_004CA0E8, kindIndex);
    }
    GetPartsPos((GetPartsPosUnit *)unit);
    GetPartsSize((UwamonoKowareSizeUnit *)(void *)unit);
    if (state->timer == -1) {
        state->timer = 1;
    }
    unit->flags = 0;
    unit->updateFunction = MAP_updateUnitKoware;
    CreateUwamonoCommon((UwamonoMapUnit *)unit);
    SetHideObject(unit);
    if (unit->serial >= 0 && unit->serial < resourceCount->partCount) {
        nmlModelSetPartsVisible(resource, unit->serial, 1);
    } else {
        printf(D_004CA108, unit->serial);
        unit->serial = -1;
        unit->updateFunction = 0;
    }
    if (printflg != 0) {
        printf(D_004CA138, unit->serial, kindIndex,
               fptodp(unit->position.x), fptodp(unit->position.y),
               fptodp(unit->position.z));
    }
}

void InitSaveSymbol(UwamonoInitSymbolUnit *unit)
{
    UwamonoInitSymbolState *timerState = &unit->timerState;
    float (*matrix)[4] = unit->matrix;
    Vector4 *position = &unit->position;
    int uwaresId = uwares_tbl[5];

    unit->xtxresId = xtxres_tbl[5];
    unit->uwaresId = uwaresId;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&position->x);
    xglMatrixStackRotX(unit->rotationX);
    xglMatrixStackRotY(unit->rotationY);
    xglMatrixStackRotZ(unit->rotationZ);
    xglMatrixStackSave(matrix);
    unit->savedMatrix = matrix;

    if (timerState->state == -1) {
        timerState->state = 0;
    }
    *(int *)((char *)timerState + 0x44) = 0;
    unit->updateFunction = MAP_updateUnitSaveSymbol;
    unit->flags |= 0x10000;
    unit->flags |= 0x20000000;

    if (printflg != 0) {
        printf(D_004CA170,
                      fptodp(position->x),
                      fptodp(unit->position.y),
                      fptodp(unit->position.z));
    }
}

void InitShopSymbol(UwamonoInitSymbolUnit *unit)
{
    UwamonoInitSymbolState *timerState = &unit->timerState;
    float (*matrix)[4] = unit->matrix;
    Vector4 *position = &unit->position;
    int uwaresId = uwares_tbl[6];

    unit->xtxresId = xtxres_tbl[6];
    unit->uwaresId = uwaresId;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&position->x);
    xglMatrixStackRotX(unit->rotationX);
    xglMatrixStackRotY(unit->rotationY);
    xglMatrixStackRotZ(unit->rotationZ);
    xglMatrixStackSave(matrix);
    unit->savedMatrix = matrix;

    if (timerState->state == -1) {
        timerState->state = 0;
    }
    *(int *)((char *)timerState + 0x44) = 0;
    unit->updateFunction = MAP_updateUnitShopSymbol;
    unit->flags |= 0x10000;
    unit->flags |= 0x40000000;

    if (printflg != 0) {
        printf(D_004CA190,
                      fptodp(position->x),
                      fptodp(unit->position.y),
                      fptodp(unit->position.z));
    }
}

extern const signed char printflgReadOnly __asm__("printflg");

void InitEvsSymbol(UwamonoInitSymbolUnit *unit)
{
    UwamonoInitSymbolState *timerState = &unit->timerState;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotationX);
    xglMatrixStackRotY(unit->rotationY);
    xglMatrixStackRotZ(unit->rotationZ);
    xglMatrixStackSave(&unit->matrix[0]);
    unit->savedMatrix = &unit->matrix[0];

    if (timerState->state == -1) {
        timerState->state = 0;
    }
    timerState->timer = 0;
    unit->updateFunction = MAP_updateUnitSymbol;
    unit->flags = unit->flags | 0x10000;
    if (printflgReadOnly == 0) return;
    printf(D_004CA190,
           fptodp(unit->position.x),
           fptodp(unit->position.y),
           fptodp(unit->position.z));
}

void InitRetSymbol(UwamonoInitSymbolUnit *unit)
{
    UwamonoInitSymbolState *timerState = &unit->timerState;
    float (*matrix)[4] = unit->matrix;
    Vector4 *position = &unit->position;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&position->x);
    xglMatrixStackRotX(unit->rotationX);
    xglMatrixStackRotY(unit->rotationY);
    xglMatrixStackRotZ(unit->rotationZ);
    xglMatrixStackSave(matrix);
    unit->savedMatrix = matrix;

    if (timerState->state == -1) {
        timerState->state = 0;
    }
    timerState->timer = 0;
    unit->updateFunction = MAP_updateUnitSymbol;
    unit->flags |= 0x10000;

    if (printflgReadOnly != 0) {
        printf(D_004CA190,
               fptodp(position->x),
               fptodp(unit->position.y),
               fptodp(unit->position.z));
    }
}

void SetHideObject(void *argument)
{
    UwamonoCreatedUnit *created = argument;
    UwamonoCreatedPosition *createdPosition = argument;
    UwamonoCreatedState *state = &created->state;
    signed char actorIndex = state->state;
    signed char mapUnitIndex;

    if (actorIndex != -1) {
        unsigned char sequenceIndex = created->sequenceIndex;
        UwamonoCrossActor *enemy = &actor[actorIndex];
        unsigned char *linkState = enepc[enemy->actorIndex].linkState;

        enemy->update = MAP_updateUnitEnemy;
        linkState[8] = 0;
        linkState[11] = sequenceIndex;
        enemy->alignedPosition = createdPosition->position;
        enemy->unmodeled_50[0] = 0.0f;
        enemy->unmodeled_50[1] = 0.0f;
        enemy->unmodeled_50[2] = 0.0f;
        ACT_updateEnemy((UwamonoEnemyUnit *)enemy);
        enemy->flags |= 8;
    }

    mapUnitIndex = state->signal;
    if (mapUnitIndex != -1) {
        UwamonoMapSlot *unit = &MapUnit[mapUnitIndex];
        struct UwamonoMapCollisionState *mapState = &unit->state;
        UwamonoHideEffect *effect = (UwamonoHideEffect *)unit->effectCf[0];
        MapPartsResource *resource = GameLoopState.partsRoot->resource;

        unit->linkedUnitIndex = created->sequenceIndex;
        unit->position = createdPosition->position;
        unit->update = MAP_updateUnitHide;
        unit->flags |= 0x100004;
        unit->draw = 0;
        if (effect != 0) {
            effect->enabled = 0;
        }
        if (mapState->kindIndex == 2) {
            if (unit->serial >= 0 && unit->serial < resource->partCount) {
                nmlModelSetPartsVisible(resource, unit->serial, 0);
                return;
            }
            printf(D_004CA108, unit->serial, effect, resource);
            unit->serial = -1;
            unit->update = 0;
        }
    }
}

void GameStateRestoreKoware(void)
{
    UwamonoGameLoopState *state;
    int index;
    index = 0;
    while (index < 64 && GameLoopState.shootUnit[index] != -1) {
        state = &GameLoopState;
        nmlModelSetPartsVisible(state->partsRoot->resource, state->shootUnit[index], 0);
        ++index;
    }
}

void SetMapUwamono(void)
{
    UwamonoGameLoopState *state = &GameLoopState;
    UwamonoMapSlot *unit = (UwamonoMapSlot *) MapUnit;
    short *shootUnit = state->shootUnit;
    int remaining = 63;

    do {
        *shootUnit++ = -1;
        if (unit->flags & 0x10000) {
            if (unit->update)
                unit->update(unit);
        }
        --remaining;
        ++unit;
    } while (remaining >= 0);
    UnlockMapUnit();
}

void ResetShootSys(void)
{
    UwamonoGameLoopState *state = &GameLoopState;
    short emptyShootUnitId = -1;
    int index = 0x3f;
    short *shootUnit = &state->shootUnit[0x3f];

    do {
        index--;
        *shootUnit = emptyShootUnitId;
        shootUnit--;
    } while (index >= 0);

    UnlockMapUnit();
}

UwamonoCreatedUnit *CreateUwamono(float x, float y, float z, float rotationY, int resourceId)
{
    UwamonoCreatedUnit *unit =
        (UwamonoCreatedUnit *)(void *)MAP_createUnit(-1, resourceId);
    UwamonoCreatedState *state;
    int inactiveValue;

    if (unit == 0) {
        printf(D_004CA1B0);
        return 0;
    }
    MAP_loadUnitResource(unit, resourceId);
    state = &unit->state;
    memset(state, 0, sizeof(*state));
    inactiveValue = -1;
    state->height = 3.0f;
    state->heightCheckFlag = 1;
    state->signal = inactiveValue;
    state->state = inactiveValue;
    state->target = inactiveValue;
    state->timer = 1;
    unit->position.x = x;
    unit->position.y = y;
    unit->position.z = z;
    unit->position.w = 1.0f;
    unit->rotationY = rotationY;
    unit->rotationX = 0.0f;
    unit->rotationZ = 0.0f;
    InitMapTrap(unit);
    return unit;
}

UwamonoKowareUnit *CreateKowaremono(short resourceId, short kindIndex)
{
    UwamonoKowareUnit *unit = (UwamonoKowareUnit *)MAP_createUnit(-1, resourceId);
    UwamonoMapUnitState *state;
    if (unit == 0) {
        printf(D_004CA1D0);
        return 0;
    }
    GetPartsPos((GetPartsPosUnit *)unit);
    state = &unit->state;
    if (state->state == -1) {
        state->state = KowareKind_t[kindIndex].shape;
    }
    unit->dimensions = KowareKind_t[kindIndex].dimensions;
    if (state->target == -1) {
        state->target = KowareKind_t[kindIndex].target;
    }
    unit->flags = 0;
    CreateUwamonoCommon((UwamonoMapUnit *)unit);
    unit->updateFunction = MAP_updateUnitKoware;
    return unit;
}

void CreateUwamonoCommon(UwamonoMapUnit *unit)
{
    unit->actionNo = 0;
    unit->flags |= 0x10000;
    unit->sequenceNo = 0;
    unit->sequenceSub = 0;
    unit->actionSub = 0;
}

void SetUwaWind(UwamonoWindUnit *unit);
void UwamonoCommonFunc(UwamonoCommonUnit *unit);

void MAP_updateUnitKoware(void *argument)
{
    UwamonoKowareUnit *unit = argument;
    UwamonoMapUnitState *state = &unit->state;
    int slot;

    if (state->timer != 0) {
        DrawActiveCursol((UwamonoMapUnit *)(void *)unit);
    }
    UwamonoCommonFunc((UwamonoCommonUnit *)(void *)unit);

    switch (unit->actionSub) {
    case 0:
        if (state->signal == 0) {
            return;
        }
        unit->actionSub = 1;
        for (slot = 0; slot < 64; slot++) {
            if (GameLoopState.shootUnit[slot] == -1) {
                GameLoopState.shootUnit[slot] = unit->serial;
                break;
            }
        }

        Vibration_Set_Strong(255, 32, 30);
        UwamonoBrokenSe((UwamonoBrokenSoundUnit *)unit);
        sefCreateEffectCf(state->effectResourceId, &unit->position, &unit->rotation);
        ClearUwamonoEffect((UwamonoEffectUnit *)unit);
        SetUwaWind((UwamonoWindUnit *)unit);
        if (state->initialPartId != -1) {
            GameLoopState.enemyUpdateState = 0;
            CallMethod_I(D_004DB6E8, state->initialPartId);
        }
        return;
    case 1:
        break;
    default:
        return;
    }
    unit->breakCount++;
    if (unit->breakCount >= 3) {
        nmlModelSetPartsVisible(GameLoopState.partsRoot->resource, unit->serial, 0);
        unit->flags |= 0x20000;
    }
    if (state->frameLimit < unit->breakCount) {
        if (printflg != 0) {
            printf(D_004CA1F8, unit->serial, unit->sequenceIndex);
        }
        unit->updateFunction = 0;
        unit->flags = 0;
        unit->serial = -1;
        EXM_ClearWindStruct();
    }
}

void MAP_updateUnitEnemy(UwamonoEnemyUnit *unit)
{
    unsigned char linkedUnitId = enepc[unit->enemyIndex].unitId;
    UwamonoGameLoopState *state;

    if (!MapUnit[linkedUnitId].actionSub) {
        unit->flags |= 8;
    } else {
        unit->position = MapUnit[linkedUnitId].position;
        unit->flags &= ~8;
        unit->update = ACT_updateEnemy;
        state = &GameLoopState;
        state->enemyUpdateState = 0;
        unit->actorHeading = (unit->heading = Get_Angle(&unit->position.value, &state->actor->position));
    }
}

void MAP_updateUnitHide(void *argument)
{
    UwamonoMapSlot *unit = argument;
    UwamonoHideEffect *effect;
    struct UwamonoMapCollisionState *state = &unit->state;
    MapPartsResource *resource;
    Matrix4 *matrix;

    if (MapUnit[unit->linkedUnitIndex].actionSub == 0) {
        unit->flags |= 0x100000;
        state->hideOverride = 0;
        effect = (UwamonoHideEffect *)unit->effectCf[0];
        if (effect != 0) {
            effect->enabled = 0;
        }
        return;
    }

    unit->position = MapUnit[unit->linkedUnitIndex].position;
    unit->flags &= ~0x100000;
    state->hideOverride = 1;
    effect = (UwamonoHideEffect *)unit->effectCf[0];
    if (effect != 0) {
        effect->enabled = 1;
    }
    matrix = unit->matrixView;
    (*matrix)[3][0] = unit->position.value.x;
    (*matrix)[3][1] = unit->position.value.y;
    (*matrix)[3][2] = unit->position.value.z;

    switch (state->kindIndex) {
    case 4:
        unit->update = MAP_updateUnitItemBox;
        unit->draw = MAP_drawUnitItemBox;
        break;
    case 5:
        unit->flags &= ~4;
        unit->update = MAP_updateUnitItemSymbol;
        break;
    case 2:
        unit->flags &= ~4;
        unit->update = MAP_updateUnitKoware;
        resource = GameLoopState.partsRoot->resource;
        if (unit->serial < 0 || unit->serial >= resource->partCount) {
            printf(D_004CA108, unit->serial);
            unit->serial = -1;
            unit->update = 0;
        } else {
            nmlModelSetPartsVisible(resource, unit->serial, 1);
        }
        break;
    case 3:
        unit->flags &= ~4;
        unit->update = MAP_updateUnitTrap;
        break;
    default:
        break;
    }
}

void MAP_updateUnitSaveSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase += D_004D7EB4;
    if (CheckDist3D(&GameLoopState.actor->position, &unit->position) < 0.0f)
        return;
}

void MAP_updateUnitShopSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase += D_004D7EB8;
    if (CheckDist3D(&GameLoopState.actor->position, &unit->position) < 0.0f)
        return;
}

void MAP_updateUnitSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase = unit->symbolPhase + UWAMONO_SYMBOL_PHASE_STEP;
}

void SetUwaWind(UwamonoWindUnit *unit)
{
    UwamonoWind *wind = &uwaWind;
    float heightDelta;

    wind->mode = 2;
    __builtin_memcpy((UwamonoVectorWords *) &wind->position,
                     (const UwamonoVectorWords *) &unit->position, sizeof(Vector4));
    heightDelta = unit->height * 0.5f;
    wind->position.w = D_004D7EC0;
    wind->decay = D_004D7EC4;
    wind->strength = 1.0f;
    wind->timer = 0;
    wind->position.y += heightDelta;
    EXM_SetWindStruct(wind);
}

void UwamonoBrokenSe(UwamonoBrokenSoundUnit *unit)
{
    UwamonoBrokenSoundState *state = &unit->state;
    int soundId = state->soundId;
    int channel = unit->seChannel + 1;

    if (soundId != -1) {
        xglSoundEffectNormalID(soundId, channel);
        return;
    }
    switch (state->material) {
    case 0:
        xglSoundEffectNormalID(0x10003, channel);
        break;
    case 1:
        xglSoundEffectNormalID(0x10006, channel);
        break;
    case 2:
        xglSoundEffectNormalID(0x10007, channel);
        break;
    default:
        xglSoundEffectNormalID(0x10003, channel);
        break;
    }
}

void DrawActiveCursol(UwamonoMapUnit *argument)
{
    DrawActiveCursorUnit *unit = (DrawActiveCursorUnit *)argument;
    UwamonoEffectUnit *effects = (UwamonoEffectUnit *)argument;
    DrawActiveCursorEffect *effect;
    float scale;
    if (unit->signal == 1 || !(unit->flags & 0x40000)) {
        if (effects->effectCf[1] != 0) {
            sefDeleteEffectCf(effects->effectCf[1]);
            effects->effectCf[1] = 0;
        }
        if (effects->effectCf[2] != 0) {
            sefDeleteEffectCf(effects->effectCf[2]);
            effects->effectCf[2] = 0;
        }
        return;
    } else if (GameLoopState.aimMapUnitIndex == unit->aimIndex) {
        if (effects->effectCf[1] != 0) {
            sefDeleteEffectCf(effects->effectCf[1]);
            effects->effectCf[1] = 0;
        }
        if (effects->effectCf[2] == 0) {
            effect = (DrawActiveCursorEffect *)sefCreateEffectCf(0x2e3, 0, 0);
            if (effect != 0) {
                scale = (unit->cursorScale[0] + unit->cursorScale[1] +
                                unit->cursorScale[2]) / 3.0f;
                scale = scale * D_004D7EC8;

                effect->casterUnit = argument;
                effect->flags = (effect->flags & ~0x400u) | 4;
                if (scale < D_004D7EC8)
                    scale = D_004D7EC8;
                if (scale > 2.0f)
                    scale = 2.0f;
                effects->effectCf[2] = (int)effect;
            } else {
                return;
            }
        } else {
            return;
        }
    } else {
        if (effects->effectCf[2] != 0) {
            sefDeleteEffectCf(effects->effectCf[2]);
            effects->effectCf[2] = 0;
        }
        if (effects->effectCf[1] == 0) {
            effect = (DrawActiveCursorEffect *)sefCreateEffectCf(0x2e2, 0, 0);
            if (effect != 0) {
                scale = (unit->cursorScale[0] + unit->cursorScale[1] +
                                unit->cursorScale[2]) / 3.0f;
                scale = scale * D_004D7ECC;

                effect->casterUnit = argument;
                effect->flags = (effect->flags & ~0x400u) | 4;
                if (scale < D_004D7ECC)
                    scale = D_004D7ECC;
                if (scale > 2.0f)
                    scale = 2.0f;
                effects->effectCf[1] = (int)effect;
            } else {
                return;
            }
        } else {
            return;
        }
    }
    effect->scale.z = scale;
    effect->scale.y = scale;
    effect->scale.x = scale;
}

int HitCheckMapUnit(void *actor)
{
    int index = 0;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1) {
            unsigned int flags = unit->flags;
            if (!(flags & 0x100000) && state->crossType &&
                (flags & 0x10000) &&
                HitCheckMapUnitAt(actor, (UwamonoCollisionUnit *)unit)) {
                return index;
            }
        }
        ++index;
    } while (index < 64);
    return -1;
}

int HitCheckMapUnitWithNyuru(UwamonoNyuruAlignedActor *actor)
{
    int candidates[64];
    int count;
    int index;
    int candidate;
    UwamonoAlignedVector position;
    UwamonoAlignedVector displacement;

    for (index = 63; index >= 0; index--) {
        candidates[index] = -1;
    }

    count = 0;
    index = 0;
    for (; index < 64; index++) {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1 && !(unit->flags & 0x100000) &&
            state->crossType != 0 && (unit->flags & 0x10000) &&
            HitCheckMapUnitAt((UwamonoCollisionActor *)actor,
                              (UwamonoCollisionUnit *)unit)) {
            candidates[count] = index;
            count++;
        }
    }

    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        return HitCheckNyuru(actor, (UwamonoCollisionUnit *)&MapUnit[candidates[0]]);
    }

    if (candidates[0] != -1) {
        candidate = 0;
        while (candidates[candidate] != -1) {
            UwamonoMapSlot *unit = &MapUnit[candidates[candidate]];

            position = actor->position;
            displacement = actor->displacement;
            switch (unit->state.crossType) {
            case 1:
                NyuruCircle((UwamonoNyuruActor *)actor,
                            (UwamonoCollisionUnit *)unit);
                break;
            case 2:
                NyuruMatrix((UwamonoNyuruActor *)actor,
                            (UwamonoCollisionUnit *)unit, 1);
                break;
            }

            if (HitCheckMapUnit(actor) == -1) {
                return 1;
            }

            actor->position = position;
            actor->displacement = displacement;
            candidate++;
        }
    }

    actor->position.value.x -= actor->displacement.value.x;
    actor->position.value.z -= actor->displacement.value.z;
    actor->displacement.value.x = 0.0f;
    actor->displacement.value.z = 0.0f;
    return 0;
}

int HitCheckMapUnitReverse(void *actor)
{
    int index = 64;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1) {
            unsigned int flags = unit->flags;
            if (!(flags & 0x100000) && state->crossType &&
                (flags & 0x10000) &&
                HitCheckMapUnitAt(actor, (UwamonoCollisionUnit *)unit))
                return index;
        }
        --index;
    } while (index > 0);
    return -1;
}

int HitCheckMapUnitPos(const Vector4 *position) {
    return HitCheckMapUnitPosSize(position, 0.0f);
}

int HitCheckMapUnitPosSize(const Vector4 *position, float radius)
{
    UwamonoCollisionActor probe;
    int index = 0;

    probe.position = *position;
    probe.radius = radius;
    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1 && state->crossType &&
            (unit->flags & 0x10000) &&
            HitCheckMapUnitPosAt(&probe, (UwamonoCollisionUnit *)unit)) {
            return index;
        }
        ++index;
    } while (index < 64);
    return -1;
}

int HitCheckMapUnitPosReverse(Vector4 *position)
{
    UwamonoCollisionActor probe;
    int index = 64;

    probe.position = *position;
    probe.radius = 0.0f;
    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1 && state->crossType &&
            (unit->flags & 0x10000) &&
            HitCheckMapUnitAt(&probe, (UwamonoCollisionUnit *)unit))
            return index;
        --index;
    } while (index > 0);
    return -1;
}

int HitCheckMapUnitAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoMapUnitState *state;

    if (unit->position.value.y + unit->dimensions.value.y * D_004D7ED0 < actor->position.y) {
        return 0;
    }

    state = &unit->state;
    if (actor->position.y < unit->position.value.y - D_004D7ED4) {
        return 0;
    }

    switch (state->state) {
    case 1: {
        float distance = CheckDist2D(&actor->position, &unit->position.value);
        float collisionRadius = actor->radius + unit->dimensions.value.x;
        if (!(distance < collisionRadius)) {
            return 0;
        }
        return 1;
    }
    case 2:
        return HitCheckBox(actor, (struct UwamonoBoxUnit *)unit);
    }
    return 0;
}

unsigned int HitCheckMapUnitPosAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoMapUnitState *state = &unit->state;
    float unitY = unit->position.value.y;
    float actorY = actor->position.y;
    signed char shape;

    if (unitY + unit->dimensions.value.y * D_004D7ED8 < actorY) {
        return 0;
    }
    if (actorY < unitY - D_004D7EDC) {
        return 0;
    }
    shape = state->state;
    switch (shape) {
    case 1:
        if (CheckDist2D(&actor->position, &unit->position.value) <
            actor->radius + unit->dimensions.value.x) {
            return 1;
        }
        return 0;
    case 2:
        return HitCheckBoxCorner(actor, unit);
    default:
        return 0;
    }
}

int HitCheckBox(UwamonoCollisionActor *actor, struct UwamonoBoxUnit *boxUnit)
{
    const float (*basis)[4];
    UwamonoCornerCollisionUnit *unit = (UwamonoCornerCollisionUnit *)boxUnit;
    UwamonoAlignedVector dimensions = unit->halfSize;
    UwamonoAlignedVector position;
    float unitX;
    float actorX;
    float actorZ;
    float basisX;
    float basisZ;
    float halfWidth;
    float margin;
    float testDistance;
    float depth;
    float depthSquared;
    float differenceX;
    float differenceZ;
    float axisX;
    float deltaZ;

    position.words[1] = unit->position.words[1];
    position.words[0] = unit->position.words[0];
    /* Begin with unit-relative X before reconstructing projected world X. */
    unitX = position.value.x;
    actorX = actor->position.x;
    actorZ = actor->position.z;
    deltaZ = actorZ - position.value.z;
    basis = unit->basis;
    axisX = actorX - unitX;
    /* The saved transform's first row supplies the horizontal box axis. */
    testDistance = axisX * basis[0][0] + deltaZ * basis[0][2];
    halfWidth = dimensions.value.x;
    margin = actor->radius * 1.5f;
    if (halfWidth + margin < __builtin_fabsf(testDistance)) {
        return 0;
    }
    depth = dimensions.value.z + margin;
    depthSquared = depth * depth;
    basisZ = basis[0][2];
    basisX = basis[0][0];
    axisX = testDistance * basisX + unitX;
    differenceX = actorX - axisX;
    deltaZ = testDistance * basisZ + position.value.z;
    differenceZ = actorZ - deltaZ;
    /* The final test measures squared residual distance from the box axis. */
    testDistance = differenceX * differenceX + differenceZ * differenceZ;
    return testDistance < depthSquared;
}

int HitCheckBoxCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoMapSlot *box = (UwamonoMapSlot *)unit;
    int outsideWidth = 0;
    UwamonoAlignedVector dimensions = box->dimensions;
    UwamonoAlignedVector position;
    struct UwamonoBoxBasis *basis;
    struct UwamonoBoxBasis **basisSlot;
    float basisX;
    float basisZ;
    float actorX;
    float actorZ;
    float testDistance;
    float offsetX;
    float offsetZ;
    float differenceX;
    float differenceZ;
    float depth;

    position.words[1] = box->position.words[1];
    position.words[0] = box->position.words[0];
    actorX = actor->position.x;
    actorZ = actor->position.z;
    basisSlot = &box->basis;
    basis = *basisSlot;
    basisX = basis->x;
    basisZ = basis->z;
    offsetX = actorX - position.value.x;
    offsetZ = actorZ - position.value.z;
    testDistance = offsetX * basisX + offsetZ * basisZ;
    if (dimensions.value.x + actor->radius * 1.5f < __builtin_fabsf(testDistance)) {
        return 0;
    }
    if (dimensions.value.x < __builtin_fabsf(testDistance)) {
        outsideWidth = 1;
    }
    offsetX = testDistance * basisX + position.value.x;
    differenceX = actorX - offsetX;
    offsetZ = testDistance * basisZ + position.value.z;
    differenceZ = actorZ - offsetZ;
    depth = dimensions.value.z + actor->radius * 1.5f;
    testDistance = differenceX * differenceX + differenceZ * differenceZ;
    if (testDistance < depth * depth) {
        if (dimensions.value.z * dimensions.value.z < testDistance &&
            outsideWidth == 1) {
            return HitCheckCorner(actor, unit) != 0;
        }
        return 1;
    }
    return 0;
}

int HitCheckCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoCornerCollisionUnit *box = (UwamonoCornerCollisionUnit *)unit;
    const float (*basis)[4] = box->basis;
    Vector4 corners[4];
    UwamonoAlignedVector center = box->position;
    UwamonoAlignedVector halfSize = box->halfSize;

    corners[0].x = center.value.x + (halfSize.value.x * basis[0][0] + halfSize.value.z * basis[2][0]);
    corners[0].y = center.value.y;
    corners[0].z = center.value.z + (halfSize.value.x * basis[0][2] + halfSize.value.z * basis[2][2]);
    if (CheckDist2D(&actor->position, &corners[0]) < actor->radius * 1.5f) {
        return 1;
    }

    corners[1].x = center.value.x - (halfSize.value.z * basis[0][2] + halfSize.value.x * basis[2][2]);
    corners[1].y = center.value.y;
    corners[1].z = center.value.z + (halfSize.value.z * basis[0][0] + halfSize.value.x * basis[2][0]);
    if (CheckDist2D(&actor->position, &corners[1]) < actor->radius * 1.5f) {
        return 1;
    }

    corners[2].x = center.value.x - (halfSize.value.x * basis[0][0] + halfSize.value.z * basis[2][0]);
    corners[2].y = center.value.y;
    corners[2].z = center.value.z - (halfSize.value.x * basis[0][2] + halfSize.value.z * basis[2][2]);
    if (CheckDist2D(&actor->position, &corners[2]) < actor->radius * 1.5f) {
        return 1;
    }

    corners[3].x = center.value.x + (halfSize.value.z * basis[0][2] + halfSize.value.x * basis[2][2]);
    corners[3].y = center.value.y;
    corners[3].z = center.value.z - (halfSize.value.z * basis[0][0] + halfSize.value.x * basis[2][0]);
    return CheckDist2D(&actor->position, &corners[3]) < actor->radius * 1.5f;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBoxTest);

int HitCheckNyuru(UwamonoNyuruAlignedActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoAlignedVector position = actor->position;
    UwamonoAlignedVector displacement = actor->displacement;
    int mapIndex = -1;

    switch (unit->state.state) {
    case 1:
        if (!NyuruCircle((UwamonoNyuruActor *)actor, unit)) {
            return 0;
        }
        mapIndex = HitCheckMapUnit(actor);
        break;
    case 2:
        if (!NyuruMatrix((UwamonoNyuruActor *)actor, unit, 0)) {
            return 0;
        }
        mapIndex = HitCheckMapUnit(actor);
        break;
    }
    if (mapIndex != -1) {
        actor->position = position;
        actor->displacement = displacement;
        actor->position.value.x -= actor->displacement.value.x;
        actor->position.value.z -= actor->displacement.value.z;
        actor->displacement.value.x = 0.0f;
        actor->displacement.value.z = 0.0f;
        return 0;
    }
    return 1;
}

static int NyuruCircle(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit)
{
    float gapZ;
    float gapX;
    float displacementLength;
    float displacementAngle;
    float movedAngle;
    float scale;
    float angleCorrection;
    float combinedRadius;
    float distance;
    Vector4 target;
    float height;
    Vector4 direction;

    displacementLength = __builtin_sqrtf(actor->displacement.x * actor->displacement.x + actor->displacement.z * actor->displacement.z);
    scale = displacementLength;
    direction.x = actor->displacement.x / scale;
    direction.z = actor->displacement.z / scale;

    displacementAngle = Get_Angle(&actor->position, &unit->position.value);
    (void)atan2f(actor->displacement.x, actor->displacement.z);
    combinedRadius = actor->radius + unit->dimensions.value.x;
    actor->position.x -= actor->displacement.x;
    actor->position.z -= actor->displacement.z;
    movedAngle = Get_Angle(&actor->position, &unit->position.value);

    target.x = unit->position.value.x - combinedRadius * xglSin(displacementAngle);
    target.z = unit->position.value.z - combinedRadius * xglCos(displacementAngle);
    gapX = actor->position.x - target.x;
    gapZ = actor->position.z - target.z;
    distance = __builtin_sqrtf(gapX * gapX + gapZ * gapZ);
    scale = (displacementLength - distance) * 0.25f;
    angleCorrection = scale / (unit->dimensions.value.x * 2.0f);
    if (0.0f < movedAngle - displacementAngle) {
        displacementAngle -= angleCorrection;
    } else {
        displacementAngle += angleCorrection;
    }

    combinedRadius = actor->radius + unit->dimensions.value.x + D_004D7EE8;
    target.x = unit->position.value.x - combinedRadius * xglSin(displacementAngle);
    target.z = unit->position.value.z - combinedRadius * xglCos(displacementAngle);
    actor->displacement.x = target.x - actor->position.x;
    actor->displacement.z = target.z - actor->position.z;
    height = UnduCheck(&actor->position, &actor->displacement,
                       &actor->undulation);
    if (height == -1000.0f) {
        actor->position.x -= actor->displacement.x;
        actor->position.z -= actor->displacement.z;
        return 0;
    }
    actor->position.y = height;
    return 1;
}

static int NyuruMatrix(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit, int mode)
{
    UwamonoCrossBasis *basis;
    UwamonoAlignedVector dims;
    float dx = actor->displacement.x;
    float dz = actor->displacement.z;
    float remainingX = actor->position.x - dx;
    float remainingZ = actor->position.z - dz;
    float distanceSquared = dx * dx + dz * dz;
    float distance;
    float relativeX;
    float relativeZ;
    float alongX;
    Vector4 direction;
    float projection;
    UwamonoCrossBasis basisCopy;

    dims = unit->dimensions;
    basis = unit->basis;
    basisCopy = *basis;
    actor->position.x = remainingX;
    actor->position.z = remainingZ;
    relativeX = remainingX - unit->position.value.x;
    relativeZ = remainingZ - unit->position.value.z;
    alongX = relativeX * basis->xAxisX + relativeZ * basis->xAxisZ;
    distance = __builtin_sqrtf(distanceSquared);
    if (distance == 0.0f) {
        return 0;
    }
    direction.x = actor->displacement.x / distance;
    direction.z = actor->displacement.z / distance;
    direction.y = 0.0f;

        if (__builtin_fabsf(alongX) < dims.value.x + actor->radius * 1.5f) {
        projection = direction.x * basis->xAxisX + direction.z * basis->xAxisZ;
        actor->displacement.x = projection * basis->xAxisX * distance;
        actor->displacement.z = projection * basis->xAxisZ * distance;
        if (__builtin_fabsf(actor->displacement.x) < D_004D7EEC &&
            __builtin_fabsf(actor->displacement.z) < D_004D7EEC && mode == 0) {
            if (alongX > dims.value.x) {
                actor->displacement.x = basis->xAxisX * distance * 0.5f;
                actor->displacement.z = basis->xAxisZ * distance * 0.5f;
            } else if (alongX < -dims.value.x) {
                actor->displacement.x = -basis->xAxisX * distance * 0.5f;
                actor->displacement.z = -basis->xAxisZ * distance * 0.5f;
            }
        }
    } else {
        projection = direction.x * basis->zAxisX + direction.z * basis->zAxisZ;
        actor->displacement.x = projection * basis->zAxisX * distance;
        actor->displacement.z = projection * basis->zAxisZ * distance;
        if (__builtin_fabsf(actor->displacement.x) < D_004D7EF0 &&
            __builtin_fabsf(actor->displacement.z) < D_004D7EF0 && mode == 0) {
            alongX = relativeX * basis->zAxisX + relativeZ * basis->zAxisZ;
            if (alongX > dims.value.z) {
                actor->displacement.x = basis->zAxisX * distance * 0.5f;
                actor->displacement.z = basis->zAxisZ * distance * 0.5f;
            } else if (alongX < -dims.value.z) {
                actor->displacement.x = -basis->zAxisX * distance * 0.5f;
                actor->displacement.z = -basis->zAxisZ * distance * 0.5f;
            }
        }
    }

    actor->undulation.attributeMask |= 0x4000;
    {
        float height = UnduCheck(&actor->position, &actor->displacement,
                                 &actor->undulation);
        if (height == -1000.0f) {
            actor->position.x -= actor->displacement.x;
            actor->position.z -= actor->displacement.z;
            return 0;
        }
        actor->position.y = height;
    }
    return 1;
}

int NyuruBox(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit, int mode)
{
    UwamonoCrossBasis *basis;
    UwamonoAlignedVector dims;
    Vector4 normal;
    UwamonoCrossBasis basisCopy;
    float dx;
    float dz;
    float distance;
    float relativeX;
    float relativeZ;
    float alongX;
    float projection;

    basis = unit->basis;
    basisCopy = *basis;
    dims = unit->dimensions;

    if (mode == 0 && HitCheckCorner(actor, unit)) {
        NyuruCorner((UwamonoNyuruActor *)actor, unit);
        return 1;
    }
    dx = actor->displacement.x;
    dz = actor->displacement.z;
    actor->position.x -= dx;
    actor->position.z -= dz;
    relativeX = actor->position.x - unit->position.value.x;
    relativeZ = actor->position.z - unit->position.value.z;
    alongX = relativeX * basis->xAxisX + relativeZ * basis->xAxisZ;
    distance = __builtin_sqrtf(actor->displacement.x * actor->displacement.x + actor->displacement.z * actor->displacement.z);
    if (distance == 0.0f) {
        return 0;
    }

    normal.x = actor->displacement.x / distance;
    normal.y = 0.0f;
    normal.z = actor->displacement.z / distance;
    if (__builtin_fabsf(alongX) < dims.value.x + actor->radius * 1.5f) {
        projection = normal.x * basis->xAxisX + normal.z * basis->xAxisZ;
        actor->displacement.x = projection * basis->xAxisX * distance;
        actor->displacement.z = projection * basis->xAxisZ * distance;
    } else {
        projection = normal.x * basis->zAxisX + normal.z * basis->zAxisZ;
        actor->displacement.x = projection * basis->zAxisX * distance;
        actor->displacement.z = projection * basis->zAxisZ * distance;
    }

    ((UwamonoNyuruActor *)actor)->undulation.attributeMask |= 0x4000;
    {
        float height = UnduCheck(&actor->position, &actor->displacement,
                                 &((UwamonoNyuruActor *)actor)->undulation);
        if (height == -1000.0f) {
            actor->position.x -= actor->displacement.x;
            actor->position.z -= actor->displacement.z;
            return 0;
        }
        actor->position.y = height;
    }
    return 1;
}

static void NyuruCorner(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoCrossBasis *basis;
    UwamonoCollisionPosition nearest;
    UwamonoCollisionPosition candidate;
    UwamonoCollisionPosition center;
    UwamonoAlignedVector dimensions;
    UwamonoCollisionUnit adjustedUnit;
    float nearestDistance;
    float distance;
    float actorRadius;
    float adjustedRadius;

    basis = unit->basis;
    center = unit->position;
    dimensions = unit->dimensions;
    nearest.value.x = center.value.x +
                      (dimensions.value.x * basis->xAxisX +
                       dimensions.value.z * basis->zAxisX);
    nearest.value.y = center.value.y;
    nearest.value.z = center.value.z +
                      (dimensions.value.x * basis->xAxisZ +
                       dimensions.value.z * basis->zAxisZ);
    candidate.value.x = center.value.x -
                        (dimensions.value.z * basis->xAxisZ +
                         dimensions.value.x * basis->zAxisZ);
    candidate.value.y = center.value.y;
    candidate.value.z = center.value.z +
                        (dimensions.value.z * basis->xAxisX +
                         dimensions.value.x * basis->zAxisX);
    nearestDistance = CheckDist2D(&actor->position, &nearest.value);
    distance = CheckDist2D(&actor->position, &candidate.value);
    if (nearestDistance > distance) {
        nearest = candidate;
    }

    candidate.value.x = center.value.x -
                        (dimensions.value.x * basis->xAxisX +
                         dimensions.value.z * basis->zAxisX);
    candidate.value.y = center.value.y;
    candidate.value.z = center.value.z -
                        (dimensions.value.x * basis->xAxisZ +
                         dimensions.value.z * basis->zAxisZ);
    nearestDistance = CheckDist2D(&actor->position, &nearest.value);
    distance = CheckDist2D(&actor->position, &candidate.value);
    if (nearestDistance > distance) {
        nearest = candidate;
    }

    candidate.value.x = center.value.x +
                        (dimensions.value.z * basis->xAxisZ +
                         dimensions.value.x * basis->zAxisZ);
    candidate.value.y = center.value.y;
    candidate.value.z = center.value.z -
                        (dimensions.value.z * basis->xAxisX +
                         dimensions.value.x * basis->zAxisX);
    nearestDistance = CheckDist2D(&actor->position, &nearest.value);
    distance = CheckDist2D(&actor->position, &candidate.value);
    if (nearestDistance > distance) {
        nearest = candidate;
    }

    adjustedUnit = *unit;
    adjustedUnit.position = nearest;
    actorRadius = actor->radius;
    adjustedRadius = actorRadius * 1.5f;
    adjustedUnit.dimensions.value.z = adjustedRadius;
    adjustedUnit.dimensions.value.x = adjustedRadius;
    actor->radius = D_004D7EF4;
    NyuruCircle(actor, &adjustedUnit);
    actor->radius = actorRadius;
}

void ShootMapUnit(void)
{
    UwamonoGameLoopState *state = &GameLoopState;
    UwamonoAimActor *actor = (UwamonoAimActor *)state->actor;
    int unitIndex;
    struct UwamonoMapCollisionState *unitState;
    float threshold;

    if (actor->shootModeFlags & 0x0F000000) {
        UnlockMapUnit();
        return;
    }
    if (dispflg == 1) {
        DispDebugUwamono();
    }
    if (collflg == 1) {
        DispCollUwamono();
    }

    if (actor->motion != 14) {
        if (!(state->systemFlags & 0x8000)) {
            AimMapUnit();
            if ((PadData.half_2a & 0x80) &&
                state->aimMapUnitIndex != -1) {
                const Vector4 *targetPosition;

                unitIndex = state->aimMapUnitIndex;
                targetPosition = &MapUnit[unitIndex].position.value;
                actor->targetAngle = Get_Angle(&actor->position.value,
                                               targetPosition);
                {
                    UwamonoShotEffect *effect =
                        (UwamonoShotEffect *)(void *)sefCreateEffectCf(0x2de, 0, 0);
                    if (effect != 0) {
                        effect->owner = actor;
                        effect->flags &= ~0x400u;
                    }
                }
                actor->motionSpeed = D_004D7EF8;
                state->systemFlags |= 0xC000;
                ACT_setMotion(actor, 14);
                actor->shootFlags = actor->shootFlags & ~8;
                xglSoundEffectNormalID(0x10002, 0);
            }
        }
        if (actor->motion != 14) {
            return;
        }
    }

    unitIndex = GameLoopState.aimMapUnitIndex;
    unitState = &MapUnit[unitIndex].state;
    if (actor->shootProgress > D_004D7EFC &&
        actor->shootProgress < 0.36f) {
            Vibration_Set_Weak(4);
    }
    threshold = 0.36f;
    if (threshold < actor->shootProgress) {
        if (unitState->signal == 0) {
            if (printflg != 0) {
                printf(D_004CA260, GameLoopState.aimMapUnitIndex);
            }
            Enemy_FindByEar(actor, &MapUnit[unitIndex].position.value);
            unitState->signal = 1;
        }
    }
    if (actor->shootProgress > 0.5f) {
        GameLoopState.aimMapUnitIndex = -1;
        GameLoopState.systemFlags &= ~0x4000u;
        GameLoopState.systemFlags &= ~0x8000u;
        actor->shootFlags |= 8;
    }
}

int CrossPointUwamono(const Vector4 *position, const Vector4 *previous,
                      float radius, Vector4 *result)
{
    int candidates[64];
    Vector4 nearStart;
    Vector4 nearEnd;
    Vector4 farStart;
    Vector4 farEnd;
    Vector4 normal;
    Vector4 candidate;
    int count;
    float scaledRadius;
    float reach;
    int empty;
    int i;

    normal.w = 1.0f;
    normal.x = position->x - previous->x;
    normal.z = position->z - previous->z;
    normal.y = 0.0f;
    scaledRadius = radius * 1.5f;
    reach = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    reach = __builtin_sqrtf(reach) + scaledRadius;
    empty = -1;
    for (i = 63; i >= 0; i--) {
        candidates[i] = empty;
    }
    count = 0;
    for (i = 0; i < 64; i++) {
        struct UwamonoMapCollisionState *state = &MapUnit[i].state;
        UwamonoMapSlot *unit = &MapUnit[i];
        if (unit->serial != -1 && !(unit->flags & 0x100000) &&
            !(0.3f < __builtin_fabsf(position->y - unit->position.value.y)) &&
            state->crossType != 0 && (unit->flags & 0x10000) &&
            UwamonoAreaCheck(position, unit, reach)) {
            candidates[count] = i;
            count++;
        }
    }

    result->x = 65535.0f;
    result->z = 65535.0f;
    result->y = -1000.0f;
    if (count == 0) {
        *result = *previous;
        return 0;
    }

    xglVectorNormal(&normal, &normal);
    nearStart = *position;
    nearEnd = *previous;
    farStart = *position;
    __builtin_memcpy(&farEnd, previous, sizeof(farEnd));
    farEnd.w = farStart.w = nearEnd.w = nearStart.w = 1.0f;
    nearStart.x += scaledRadius * normal.z;
    nearStart.z -= scaledRadius * normal.x;
    nearEnd.x += scaledRadius * normal.z;
    nearEnd.z -= scaledRadius * normal.x;
    farStart.x -= scaledRadius * normal.z;
    farStart.z += scaledRadius * normal.x;
    farEnd.x -= scaledRadius * normal.z;
    farEnd.z += scaledRadius * normal.x;

    for (i = 0; i < 64; i++) {
        UwamonoMapSlot *unit;
        if (candidates[i] == -1) {
            break;
        }
        unit = &MapUnit[candidates[i]];
        CrossPointUwamonoAt(&nearStart, &nearEnd,
                            (UwamonoMapUnit *)unit, &candidate);
        if (candidate.y != -1000.0f) {
            CheckNearPoint(position, &candidate, result, result);
        }
        CrossPointUwamonoAt(&farStart, &farEnd,
                            (UwamonoMapUnit *)unit, &candidate);
        if (candidate.y != -1000.0f) {
            CheckNearPoint(position, &candidate, result, result);
        }
        candidate.x = position->x - unit->position.value.x;
        candidate.z = position->z - unit->position.value.z;
        if ((normal.x * candidate.x + normal.z * candidate.z) > 0.0f &&
            CheckPointLine(&nearStart, &nearEnd, &unit->position.value) !=
                CheckPointLine(&farStart, &farEnd, &unit->position.value)) {
            CrossPointUwamonoAt(position, &unit->position.value,
                                (UwamonoMapUnit *)unit, &candidate);
            CheckNearPoint(position, &candidate, result, result);
        }
    }

    if (result->y == -1000.0f) {
        *result = *previous;
        return 0;
    }
    nearEnd.x = position->x - result->x;
    nearEnd.y = position->y;
    nearEnd.z = position->z - result->z;
    reach = normal.x * nearEnd.x + normal.z * nearEnd.z;
    reach -= scaledRadius;
    if (reach < 0.0f) {
        *result = *position;
        return 1;
    }
    result->x = position->x - reach * normal.x;
    result->y = position->y;
    result->z = position->z - reach * normal.z;
    return 1;
}

void CrossPointUwamonoAt(const Vector4 *first, const Vector4 *second, UwamonoMapUnit *unit, void *result)
{
    switch (unit->crossType) {
    case 1:
        CalcNearCrossPointCircle(first, second, (UwamonoCollisionUnit *)unit, result);
        return;
    case 2:
        CalcNearCrossPointBox(first, second, unit, result);
        break;
    }
}

void CalcNearCrossPointCircle(const Vector4 *from, const Vector4 *to,
                              UwamonoCollisionUnit *unit, Vector4 *out)
{
    Vector4 delta;
    Vector4 direction;
    float distance;
    if (CrossCheckMapUnitCircle(from, to, unit)) {
        delta = *from;
        delta.y = unit->position.value.y;
        __asm__ __volatile__(
            "lqc2 vf3,0(%0)\n\t"
            "lqc2 vf2,0(%1)\n\t"
            "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
            "sqc2 vf2,0(%2)"
            :
            : "r"(from), "r"(&unit->position.value), "r"(&delta)
            : "memory");
        xglVectorLength(&distance, &delta);
        xglVectorNormal(&direction, &delta);
        distance -= unit->dimensions.value.x;
        if (distance < 0.0f) {
            *out = *from;
        } else {
            out->x = from->x + distance * direction.x;
            out->y = from->y;
            out->z = from->z + distance * direction.z;
        }
    } else {
        out->y = -1000.0f;
    }
}

void CalcNearCrossPointBox(const Vector4 *first, const Vector4 *second,
                           UwamonoMapUnit *unit, Vector4 *result)
{
    Vector4 corners[4];
    UwamonoAlignedVector centerCopy;
    UwamonoAlignedBoxDims dims;
    Vector4 candidate;
    UwamonoCrossBasis *basis = unit->basis;

    centerCopy = *(UwamonoAlignedVector *)&unit->position;
    dims = *(UwamonoAlignedBoxDims *)&unit->radius;
    result->y = -1000.0f;
    candidate.y = first->y;

    corners[0].x = centerCopy.value.x + (dims.value.radius * basis->xAxisX + dims.value.boxDepth * basis->zAxisX);
    corners[0].y = centerCopy.value.y;
    corners[0].z = centerCopy.value.z + (dims.value.radius * basis->xAxisZ + dims.value.boxDepth * basis->zAxisZ);
    corners[1].x = centerCopy.value.x - (dims.value.boxDepth * basis->xAxisZ + dims.value.radius * basis->zAxisZ);
    corners[1].y = centerCopy.value.y;
    corners[1].z = centerCopy.value.z + (dims.value.boxDepth * basis->xAxisX + dims.value.radius * basis->zAxisX);
    corners[2].x = centerCopy.value.x - (dims.value.radius * basis->xAxisX + dims.value.boxDepth * basis->zAxisX);
    corners[2].y = centerCopy.value.y;
    corners[2].z = centerCopy.value.z - (dims.value.radius * basis->xAxisZ + dims.value.boxDepth * basis->zAxisZ);
    corners[3].x = centerCopy.value.x + (dims.value.boxDepth * basis->xAxisZ + dims.value.radius * basis->zAxisZ);
    corners[3].y = centerCopy.value.y;
    corners[3].z = centerCopy.value.z - (dims.value.boxDepth * basis->xAxisX + dims.value.radius * basis->zAxisX);
    if (CheckCrossLine(first, second, &corners[0], &corners[1])) {
        CalcCrossPoint(first, second, &corners[0], &corners[1], &candidate);
        *(UwamonoAlignedVector *)result = *(UwamonoAlignedVector *)&candidate;
    }
    if (CheckCrossLine(first, second, &corners[1], &corners[2])) {
        CalcCrossPoint(first, second, &corners[1], &corners[2], &candidate);
        if (result->y != -1000.0f) {
            CheckNearPoint(first, &candidate, result, result);
        } else {
            *(UwamonoAlignedVector *)result = *(UwamonoAlignedVector *)&candidate;
        }
    }
    if (CheckCrossLine(first, second, &corners[2], &corners[3])) {
        CalcCrossPoint(first, second, &corners[2], &corners[3], &candidate);
        if (result->y != -1000.0f) {
            CheckNearPoint(first, &candidate, result, result);
        } else {
            *(UwamonoAlignedVector *)result = *(UwamonoAlignedVector *)&candidate;
        }
    }
    if (CheckCrossLine(first, second, &corners[3], &corners[0])) {
        CalcCrossPoint(first, second, &corners[3], &corners[0], &candidate);
        if (result->y != -1000.0f) {
            CheckNearPoint(first, &candidate, result, result);
        } else {
            *(UwamonoAlignedVector *)result = *(UwamonoAlignedVector *)&candidate;
        }
    }
}

void CheckNearPoint(const Vector4 *origin, const Vector4 *candA, const Vector4 *candB, Vector4 *out)
{
    Vector4 delta;
    float lenA;
    float lenB;

    __asm__ __volatile__(
        "lqc2 vf3,0(%1)\n\t"
        "lqc2 vf2,0(%0)\n\t"
        "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
        "sqc2 vf2,0(%2)"
        :
        : "r"(origin), "r"(candA), "r"(&delta)
        : "memory"
    );
    xglVectorLength(&lenA, &delta);

    __asm__ __volatile__(
        "lqc2 vf3,0(%1)\n\t"
        "lqc2 vf2,0(%0)\n\t"
        "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
        "sqc2 vf2,0(%2)"
        :
        : "r"(origin), "r"(candB), "r"(&delta)
        : "memory"
    );
    xglVectorLength(&lenB, &delta);

    if (lenA < lenB)
        *out = *candA;
    else
        *out = *candB;
}

int UwamonoAreaCheck(const Vector4 *position, UwamonoMapSlot *unit, float radius)
{
    const struct UwamonoMapCollisionState *state = &unit->state;
    float positionY = position->y;
    float unitY = unit->position.value.y;
    float lowerHeight;

    if (positionY > unitY + unit->dimensions.value.y * D_004D7F10)
        return 0;
    lowerHeight = unitY - D_004D7F14;
    if (positionY < lowerHeight)
        return 0;
    switch (state->crossType) {
    case 1: {
        float distance = CheckDist2D(position, &unit->position.value);
        float limit = radius + unit->dimensions.value.x;
        if (distance < limit)
            return 1;
        break;
    }
    case 2: {
        int boxResult = AreaCheckBox(position, unit, radius);
        if (boxResult != 0)
            return 1;
        break;
    }
    default:
        break;
    }
    return 0;
}

static inline float UwamonoProjectBoxAxisXZ(float x, float z,
                                           const Matrix4 *matrix)
{
    return x * (*matrix)[0][0] + z * (*matrix)[0][2];
}

int AreaCheckBox(const Vector4 *position, UwamonoMapSlot *unit, float radius)
{
    UwamonoAlignedVector dimensionsSnapshot;
    float pointX = position->x;
    float pointZ = position->z;
    float unitZ;
    float unitX;
    float offsetX;
    float offsetZ;
    float differenceZ;
    Matrix4 *matrix;
    Matrix4 **matrixReference;
    float basisZ;
    float basisX;
    float projection;
    float depth;

    unitX = unit->position.value.x;
    unitZ = unit->position.value.z;
    /* The routine snapshots dimensions here but reads live bounds below. */
    dimensionsSnapshot = unit->dimensions;
    offsetX = pointX - unitX;
    offsetZ = pointZ - unitZ;
    /* Keep an addressed reference to the real matrixView member. */
    matrixReference = &unit->matrixView;
    matrix = *matrixReference;
    projection = UwamonoProjectBoxAxisXZ(offsetX, offsetZ, (const Matrix4 *)matrix);
    if (unit->dimensions.value.x + radius < __builtin_fabsf(projection)) {
        return 0;
    }
    depth = unit->dimensions.value.z + radius;
    basisZ = (*matrix)[0][2];
    basisX = (*matrix)[0][0];
    offsetX = pointX - (projection * basisX + unitX);
    differenceZ = pointZ - (projection * basisZ + unitZ);
    offsetZ = differenceZ * differenceZ;
    if (offsetX * offsetX + offsetZ < depth * depth) {
        return 1;
    }
    return 0;
}

int CrossCheckMapUnitAim(Vector4 *from, UwamonoScanUnit *aim)
{
    int index = 0;
    int unitOffset = 0;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoScanUnit *scanUnit = &((UwamonoScanUnit *)MapUnit)[index];
        UwamonoScanFields *scan = &scanUnit->scan;

        if (scan->resourceId != -1 && scan->resourceId != 0x7000) {
            UwamonoMapSlot *unit =
                (UwamonoMapSlot *)&((unsigned char *)&MapUnit)[unitOffset];
            unsigned int flags = scanUnit->flags;
            if (!(flags & 0x100000) && scan->channel != aim->scan.channel &&
                state->crossType && (flags & 0x10000) &&
                CrossCheckMapUnitAt(from, &aim->position, (UwamonoCrossUnit *)unit)) {
                return index;
            }
        }
        ++index;
        unitOffset += sizeof(MapUnit[0]);
    } while (index < 64);
    return -1;
}

int CrossCheckMapUnit(Vector4 *from, Vector4 *to)
{
    int index = 0;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1) {
            unsigned int flags = unit->flags;
            if (!(flags & 0x100000) && state->crossType &&
                (flags & 0x10000) &&
                CrossCheckMapUnitAt(from, to, (UwamonoCrossUnit *)unit)) {
                return index;
            }
        }
        ++index;
    } while (index < 64);
    return -1;
}

int CrossCheckActor(UwamonoCrossActor *source, const Vector4 *to)
{
    int index;
    for (index = 0; index < 64; ++index) {
        if (!(D_004D7F18 < __builtin_fabsf(source->position.y - actor[index].position.y)) &&
            actor[index].resourceId >= 0 && !(actor[index].flags & 8) &&
            actor[index].collisionObject != 0 && index != source->actorIndex &&
            CheckCrossCircle(actor[index].radius, &source->position, to, &actor[index].position)) {
            return index;
        }
    }
    return -1;
}

int CrossCheckMapUnitAt(Vector4 *first, Vector4 *second, UwamonoCrossUnit *unit)
{
    UwamonoCrossState *state = &unit->state;

    if (D_004D7F1C < __builtin_fabsf(first->y - unit->position.y))
        return 0;
    switch (state->crossType) {
    case 1:
        return CrossCheckMapUnitCircle(first, second, (UwamonoCollisionUnit *)unit);
    case 2:
        return CrossCheckMapUnitBox(first, second, unit);
    default:
        return 0;
    }
}

int CrossCheckMapUnitCircle(const Vector4 *from, const Vector4 *to,
                            UwamonoCollisionUnit *unit)
{
    Vector4 middle;
    Vector4 direction;
    float radius;
    float halfLength;
    float projection;
    float projectedX;
    float offsetX;
    float offsetZ;
    int hit;

    direction.x = to->x - from->x;
    direction.y = 0.0f;
    direction.z = to->z - from->z;
    radius = unit->dimensions.value.x;
    halfLength = __builtin_sqrtf(direction.x * direction.x +
                                 direction.z * direction.z) * 0.5f;
    /* The normalizer's vector ABI uses w as storage, not an xyz input. */
    xglVectorNormal(&direction, &direction);
    halfLength += radius;
    hit = 0;
    middle.x = from->x + (to->x - from->x) * 0.5f;
    middle.z = from->z + (to->z - from->z) * 0.5f;
    offsetX = unit->position.value.x - middle.x;
    offsetZ = unit->position.value.z - middle.z;
    projection = offsetX * direction.x + offsetZ * direction.z;
    if (!(halfLength < __builtin_fabsf(projection))) {
        projectedX = projection * direction.x;
        offsetX = unit->position.value.x - (projectedX + middle.x);
        offsetZ = unit->position.value.z - (projection * direction.z + middle.z);
        hit = 1;
        if (!(offsetX * offsetX + offsetZ * offsetZ < radius * radius)) {
            hit = 0;
        }
    }
    return hit;
}

int CrossCheckMapUnitBox(Vector4 *first, Vector4 *second,
                         UwamonoCrossUnit *unit)
{
    Vector4 corners[4];
    UwamonoAlignedVector center;
    UwamonoAlignedVector extents;
    UwamonoCrossBasis *basis;

    extents.words[1] = unit->dimensions.words[1];
    extents.words[0] = unit->dimensions.words[0];
    center = unit->alignedPosition;
    basis = unit->basis;
    extents.value.x += D_004D7F20;
    extents.value.z += D_004D7F20;
    corners[0].x = center.value.x + (extents.value.x * basis->xAxisX + extents.value.z * basis->zAxisX);
    corners[0].y = center.value.y;
    corners[0].z = center.value.z + (extents.value.x * basis->xAxisZ + extents.value.z * basis->zAxisZ);

    corners[1].x = center.value.x - (extents.value.z * basis->xAxisZ + extents.value.x * basis->zAxisZ);
    corners[1].y = center.value.y;
    corners[1].z = center.value.z + (extents.value.z * basis->xAxisX + extents.value.x * basis->zAxisX);
    if (CheckCrossLine(first, second, &corners[0], &corners[1]) != 0) {
        return 1;
    }

    corners[2].x = center.value.x - (extents.value.x * basis->xAxisX + extents.value.z * basis->zAxisX);
    corners[2].y = center.value.y;
    corners[2].z = center.value.z - (extents.value.x * basis->xAxisZ + extents.value.z * basis->zAxisZ);
    if (CheckCrossLine(first, second, &corners[1], &corners[2]) != 0) {
        return 1;
    }

    corners[3].x = center.value.x + (extents.value.z * basis->xAxisZ + extents.value.x * basis->zAxisZ);
    corners[3].y = center.value.y;
    corners[3].z = center.value.z - (extents.value.z * basis->xAxisX + extents.value.x * basis->zAxisX);
    if (CheckCrossLine(first, second, &corners[2], &corners[3]) != 0) {
        return 1;
    }
    return CheckCrossLine(first, second, &corners[3], &corners[0]) != 0;
}

void UnlockMapUnit(void)
{
    int unitIndex = 63;
    int *effectCf = &MapUnit[0].effectCf[0];

    do {
        MapUnit[63 - unitIndex].flags &= ~0x40000u;
        if (effectCf[1] != 0) {
            sefDeleteEffectCf(effectCf[1]);
            effectCf[1] = 0;
        }
        if (effectCf[2] != 0) {
            sefDeleteEffectCf(effectCf[2]);
            effectCf[2] = 0;
        }
        unitIndex--;
        if (unitIndex >= 0)
            effectCf = &MapUnit[63 - unitIndex].effectCf[0];
    } while (unitIndex >= 0);

    GameLoopState.aimMapUnitIndex = -1;
}

int CheckNearMapUnit(void)
{
    UwamonoScanUnit *unit = (UwamonoScanUnit *)MapUnit;
    struct UwamonoSoundActor *actor = GameLoopState.actor;
    float nearest = GameLoopState.selectionDistance + 1000.0f;
    int selected = -1;
    int index = 0;

    do {
        if (CheckBrokenMapUnit((BrokenMapUnit *)unit) & 1) {
            if (unit->flags & 0x40000) {
                float distance = CheckCornerDist(&actor->position, (UwamonoCollisionUnit *)unit);
                if (distance < nearest) {
                    nearest = distance;
                    selected = index;
                }
            }
        }
        ++index;
        ++unit;
    } while (index < 64);
    if (nearest < GameLoopState.selectionDistance) {
        return selected;
    }
    return -1;
}

int CheckBrokenMapUnit(BrokenMapUnit *unit)
{
    BrokenMapUnitState *state = &unit->brokenState;
    int flags;
    int brokenId;

    if (unit->serial == -1)
        return 0;
    flags = unit->flags;
    if (!(flags & 0x10000))
        return 0;
    if (flags & 4)
        return 0;
    if (state->signal == 1)
        return 0;
    brokenId = state->brokenId;
    return brokenId == -1 ? 0 : brokenId;
}

int AimHeightCheck(UwamonoMapUnit *unit)
{
    UwamonoGameLoopState *state = &GameLoopState;
    AimHeightReference *heightReference = state->actor;
    if (!(state->aimHeightFlags & 2))
        return 1;
    return __builtin_fabsf(heightReference->position.y - unit->position.y) < 0.3f;
}

int AimMapUnit(void)
{
    UwamonoAlignedScreenPoint actorScreen;
    UwamonoAimCandidate candidates[64];
    UwamonoAimCandidate savedCandidate;
    UwamonoAlignedVector actorPosition;
    UwamonoAlignedVector unitPosition;
    int candidateCount;
    int selectedFound;
    UwamonoAimCandidate *candidate;
    UwamonoAimCandidate *list;
    UwamonoSoundActor *actor = GameLoopState.actor;
    int index;

    actorPosition = *(UwamonoAlignedVector *)&actor->position;
    actorPosition.value.y += 1.0f;
    xglRotTransPers((Vector4 *)&actorScreen, 0, &actorPosition.value, 0);
    candidateCount = 0;
    selectedFound = 0;
    candidate = candidates;
    list = candidates;

    for (index = 0; index < 64; ++index) {
        unsigned int oldFlags = MapUnit[index].flags;
        float distance;

        MapUnit[index].flags &= ~0x40000u;
        candidate->unitIndex = -1;
        if (!(CheckBrokenMapUnit((BrokenMapUnit *)&MapUnit[index]) & 1)) {
            continue;
        }
        if (!AimHeightCheck((UwamonoMapUnit *)&MapUnit[index])) {
            continue;
        }

        distance = CheckCornerDist(&actor->position,
                                   (UwamonoCollisionUnit *)&MapUnit[index]);
        if (GameLoopState.selectionDistance < distance) {
            continue;
        }
        if (!AimMapUnitLookCheck((UwamonoScanUnit *)&MapUnit[index])) {
            continue;
        }

        if ((oldFlags & 0x40000u) == 0) {
            xglSoundEffectNormalID(0x10001, 0);
        }
        candidate->distance = distance;
        unitPosition = MapUnit[index].position;
        ((Vector4 *)&unitPosition)->y += 0.5f * ((const Vector4 *)&MapUnit[index].dimensions)->y;
        MapUnit[index].flags = MapUnit[index].flags | 0x40000u;
        if (GameLoopState.aimMapUnitIndex == index) {
            selectedFound = 1;
        }
        candidate->unitIndex = index;
        xglRotTransPers((Vector4 *)&candidate->screenPoint, 0,
                        &unitPosition.value, 0);
        ++candidateCount;
        ++candidate;
    }

    if (candidateCount < 2) {
        GameLoopState.aimMapUnitIndex = candidates[0].unitIndex;
        if (candidates[0].unitIndex != -1) {
            SortLine(&actorScreen, &candidates[0].screenPoint, 1);
        }
        return candidates[0].unitIndex;
    }

    index = 1;
    if (candidates[1].unitIndex != -1) {
        for (;;) {
            if (candidates[index - 1].screenPoint.value.x > candidates[index].screenPoint.value.x) {
            savedCandidate = candidates[index];
            candidates[index] = candidates[index - 1];
            candidates[index - 1] = savedCandidate;
            index = 0;
            }
            ++index;
            if (!(index < 64 && candidates[index].unitIndex != -1)) {
                break;
            }
        }
    }

    for (index = 0; index < 63 && candidates[index].unitIndex != -1; ++index) {
        if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
            int offset = index * 48 + 16;

            SortLine(&actorScreen, (UwamonoAlignedScreenPoint *)((char *)candidates + offset), 1);
        } else {
            int offset = index * 48 + 16;

            SortLine(&actorScreen, (UwamonoAlignedScreenPoint *)((char *)candidates + offset), 0);
        }
    }

    if (!selectedFound) {
        GameLoopState.aimMapUnitIndex = CheckNearMapUnit();
    }

    if (PadData.half_2a & 8) {
        xglSoundEffectNormalID(3, 0);
        for (index = 0; index <= candidateCount; ++index) {
            if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
                if (candidates[index + 1].unitIndex == -1) {
                    GameLoopState.aimMapUnitIndex = candidates[0].unitIndex;
                } else {
                    GameLoopState.aimMapUnitIndex = candidates[index + 1].unitIndex;
                }
                return GameLoopState.aimMapUnitIndex;
            }
        }
    }

    if (PadData.half_2a & 4) {
        xglSoundEffectNormalID(3, 0);
        for (index = candidateCount - 1; index >= 0; --index) {
            if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
                if (index == 0) {
                    GameLoopState.aimMapUnitIndex =
                        candidates[candidateCount - 1].unitIndex;
                } else {
                    GameLoopState.aimMapUnitIndex = candidates[index - 1].unitIndex;
                }
                return GameLoopState.aimMapUnitIndex;
            }
        }
    }

    return candidates[0].unitIndex;
}

int AimMapUnitLookCheck(UwamonoScanUnit *target)
{
    UwamonoSoundActor *listener = GameLoopState.actor;
    UwamonoAimQuery query;
    UwamonoAlignedVector position;
    Vector4 direction;
    float result;

    if ((GameLoopState.aimHeightFlags & 8) != 0 &&
        CrossCheckMapUnitAim(&listener->position, target) != -1) {
        return 0;
    }
    if ((GameLoopState.aimHeightFlags & 4) == 0) {
        return 1;
    }

    UnduParamInit(&query);
    query.header = UnduDataGetHeader(0x305, 0x8000);
    if (query.header == 0) {
        query.header = UnduDataGetHeader(0, 0x8000);
        if ((listener->flags & 0x20) != 0) {
            query.attributeMask |= 0x20;
        }
    }
    position = *(UwamonoAlignedVector *)&listener->position;
    direction.x = target->position.x - listener->position.x;
    direction.y = target->position.y - listener->position.y;
    direction.z = target->position.z - listener->position.z;
    direction.w = 1.0f;
    query.attributeMask = 0x1810;
    result = UnduCheck(&position.value, &direction, &query);
    if (result == -1000.0f) {
        return 0;
    }
    if (!(__builtin_fabsf(position.value.x - target->position.x) < D_004D7F28)) {
        return 0;
    }
    return __builtin_fabsf(position.value.z - target->position.z) < D_004D7F28;
}

static float CheckCornerDist(const Vector4 *position, UwamonoCollisionUnit *unit)
{
    UwamonoAlignedVector corner;
    float distance;

    switch (unit->state.state) {
    case 1:
        distance = CheckDist2D(position, &unit->position.value) - unit->dimensions.value.x;
        break;
    case 2:
        corner = ((UwamonoMapSlot *)unit)->dimensions;
        {
            float offsetX = __builtin_fabsf(position->x - unit->position.value.x);
            float offsetZ;

            if (offsetX < corner.value.x) {
                corner.value.x = offsetX;
            }
            offsetZ = __builtin_fabsf(position->z - unit->position.value.z);
            if (offsetZ < corner.value.z) {
                corner.value.z = offsetZ;
            }
        }
        distance = CheckDist2D(position, &unit->position.value);
        distance -= __builtin_sqrtf(corner.value.x * corner.value.x +
                                    corner.value.z * corner.value.z);
        break;
    default:
        distance = CheckDist2D(position, &unit->position.value);
        break;
    }
    return distance;
}

void UwamonoCommonFunc(UwamonoCommonUnit *unit)
{
    UwamonoTimers *timers = &unit->timers;

    if (unit->serial < 0x1000) {
        MAP_updateUnitPartsSequence(unit);
    } else {
        MAP_updateUnitSequence(unit);
    }

    if (timers->heightCheckFlag != 0) {
        CheckUwamonoHeight(unit);
    }

    unit->linkedUnit->position.x = unit->position.value.x;
    unit->linkedUnit->position.y = unit->position.value.y;
    unit->linkedUnit->position.z = unit->position.value.z;

    if (timers->bgmTimer > 0) {
        UwamonoBgmFunc(unit);
    }
}

void UwamonoBgmFunc(UwamonoCommonUnit *unit)
{
    UwamonoTimers *timers = &unit->timers;
    UwamonoAlignedVector soundPosition;

    if (timers->projectSound == 0) {
        soundPosition = unit->position;
    } else {
        float x = unit->position.value.x;
        float y = unit->position.value.y;
        float z = unit->position.value.z;
        UwamonoLinkedUnit *linkedUnit = unit->linkedUnit;
        float projection =
            (GameLoopState.actor->position.x - x) * linkedUnit->direction[0] +
            (GameLoopState.actor->position.y - y) * linkedUnit->direction[1] +
            (GameLoopState.actor->position.z - z) * linkedUnit->direction[2];

        soundPosition.value.x = x + projection * linkedUnit->direction[0];
        soundPosition.value.y = y + projection * linkedUnit->direction[1];
        soundPosition.value.z = z + projection * linkedUnit->direction[2];
        soundPosition.value.w = 1.0f;
    }
    xglSoundEffectPosID(timers->bgmTimer, &soundPosition.value.x, 1, unit->seChannel + 1);
}

void UwamonoBgmFadeOut(void)
{
    UwamonoBgmFadeOutUnit *unit;
    int i;
    int bgmTimer;
    int bank;
    unsigned short handle;

    unit = (UwamonoBgmFadeOutUnit *) MapUnit;
    for (i = 0; i < 0x40; i++, unit++) {
        if (!(unit->flags & 0x10000)) {
            continue;
        }
        bgmTimer = unit->bgmTimer;
        if (bgmTimer < 0) {
            continue;
        }
        if (bgmTimer > 0xFFFFF) {
            continue;
        }
        bank = bgmTimer >> 16;
        handle = SoundWork.effectBanks[bank].handle;
        if (handle == 0xFFFF) {
            continue;
        }
        SsdFadeoutEffect((handle << 16) + (bgmTimer & 0xFFFF), 0x12C, unit->seChannel + 1);
    }
}

void CheckUwamonoHeight(UwamonoCommonUnit *unit)
{
    UwamonoTimers *timers = &unit->timers;
    float floorHeight;
    UwamonoAimQuery query;

    UnduParamInit(&query);
    query.header = UnduDataGetHeader(0, 0x8000);
    if (timers->heightBounce != 0) {
        query.attributeMask |= 0x20;
    } else {
        query.attributeMask = 0x1010;
    }

    floorHeight = UnduCheck(&unit->position.value, 0, &query);
    if (unit->heightVelocity > D_004D7F2C) {
        unit->heightVelocity -= D_004D7F30;
    }

    unit->position.value.y += unit->heightVelocity;
    if (floorHeight > unit->position.value.y) {
        unit->heightVelocity = -unit->heightVelocity * 0.25f;
        unit->position.value.y = floorHeight + D_004D7F34;
        if (unit->heightVelocity < D_004D7F38) {
            unit->flags |= 0x80000;
            unit->heightVelocity = 0.0f;
        }
    } else {
        int hitIndex = HitCheckUwamono((UwamonoScanUnit *)unit);
        float hitHeight;
        float reboundVelocity;
        if (hitIndex == -1) {
            return;
        }
        hitHeight = MapUnit[hitIndex].position.value.y +
                    MapUnit[hitIndex].dimensions.value.y + D_004D7F40;
        reboundVelocity = -unit->heightVelocity;
        unit->heightVelocity = 0.25f * reboundVelocity;
        unit->position.value.y = hitHeight;
        if (unit->heightVelocity < D_004D7F3C) {
            unit->heightVelocity = 0.0f;
        }
    }
}

int HitCheckUwamono(UwamonoScanUnit *actor)
{
    int index = 0;
    unsigned int callByteOffset = 0;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoScanUnit *scanUnit = &((UwamonoScanUnit *)MapUnit)[index];
        UwamonoScanFields *scan = &scanUnit->scan;

        if (scan->resourceId != -1) {
            UwamonoBoxUnit *target = (UwamonoBoxUnit *)((unsigned char *)MapUnit + callByteOffset);
            if (scan->channel != actor->scan.channel) {
                unsigned int flags = MapUnit[index].flags;
                if (!(flags & 0x20000)) {
                    if (!(flags & 0x100000) && state->crossType &&
                        (flags & 0x10000) &&
                        HitCheckUwamonoAt((UwamonoBoxUnit *)actor, target)) {
                        return index;
                    }
                }
            }
        }
        ++index;
        callByteOffset += sizeof(MapUnit[0]);
    } while (index < 64);
    return -1;
}

int HitCheckUwamonoAt(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit)
{
    UwamonoBoxCollisionState *state = &unit->state;
    float actorHeight = actor->position.y;
    float baseHeight = unit->position.y;

    if (!(actorHeight < baseHeight) && !(baseHeight + unit->dimensions.y < actorHeight)) {
        switch (state->crossType) {
        case 1:
            if (CheckDist2D(&actor->position, &unit->position) < unit->dimensions.x + D_004D7F44)
                return 1;
            return 0;
        case 2:
            return HitCheckBoxUwamono(actor, (UwamonoMapSlot *)unit);
        default:
            return 0;
        }
    }
    return 0;
}

int HitCheckBoxUwamono(UwamonoBoxUnit *actor, UwamonoMapSlot *unit)
{
    UwamonoAlignedVector dimensionsSnapshot;
    UwamonoAlignedVector position;
    Matrix4 *matrix;
    Matrix4 **matrixReference;
    float margin;
    float actorX;
    float actorZ;
    float offsetZ;
    float offsetX;
    float testDistance;
    float depth;
    float differenceX;
    float differenceZ;
    float depthSquared;

    actorX = actor->position.x;
    actorZ = actor->position.z;
    position = unit->position;
    /* The routine snapshots dimensions here but reads live bounds below. */
    dimensionsSnapshot = unit->dimensions;
    offsetZ = actorZ - position.value.z;
    /* Keep an addressed reference to the real matrixView member. */
    matrixReference = &unit->matrixView;
    matrix = *matrixReference;
    offsetX = actorX - position.value.x;
    testDistance = UwamonoProjectBoxAxisXZ(offsetX, offsetZ, (const Matrix4 *)matrix);
    margin = 0.15f;
    if (__builtin_fabsf(testDistance) > unit->dimensions.value.x + margin) {
        return 0;
    }
    offsetX = testDistance * (*matrix)[0][0] + position.value.x;
    differenceX = actorX - offsetX;
    offsetZ = testDistance * (*matrix)[0][2] + position.value.z;
    differenceZ = actorZ - offsetZ;
    testDistance = differenceX * differenceX + differenceZ * differenceZ;
    depth = unit->dimensions.value.z + margin;
    depthSquared = depth * depth;
    return testDistance < depthSquared;
}

void DrawRadar(void)
{
    UwamonoScreenPoint color;
    UwamonoAlignedScreenPoint position;
    const UwamonoSoundActor *listener = GameLoopState.actor;
    const UwamonoCrossActor *candidate;
    int index;
    float diffX;
    float diffZ;
    float listenerZ;
    const Vector4 *candidatePosition;
    int z;
    int x;
    int absX;
    int absZ;

    color.x = 0xFF;
    color.y = 0;
    color.z = 0;
    color.w = 0x80;
    position.value.x = 0x8B00;
    position.value.y = 0x8900;
    position.value.z = 0xFFFFFF;
    position.value.w = 0;
    DrawSprite(&position, &color, 0x20, 0x20);

    color.x = 0xFF;
    color.y = 0xFF;
    color.z = 0;
    color.w = 0x80;
    for (index = 0; index < 64; index++) {
        candidate = actor + index;
        if (candidate->resourceId < 0 || (candidate->flags & 8) != 0 ||
            !candidate->collisionObject ||
            index == listener->actorIndex) {
            continue;
        }

        {
            candidatePosition = &candidate->position;
            listenerZ = listener->position.z;
            diffZ = candidatePosition->z - listenerZ;
            diffX = candidatePosition->x - listener->position.x;
            (&position.value)->x = 0x8B00;
            (&position.value)->y = 0x8900;
            z = (int)(diffZ * 64.0f);
            x = (int)(diffX * 64.0f);
            absX = x < 0 ? -x : x;
            absZ = z < 0 ? -z : z;
            if (absX < 0x3E1 && absZ < 0x3E1) {
                (&position.value)->x = x + 0x8B00;
                (&position.value)->y = z + 0x8900;
                DrawSprite(&position, &color, 0x20, 0x20);
            }
        }
    }
}

void DrawSprite(const UwamonoAlignedScreenPoint *position,
                const UwamonoScreenPoint *color, int width, int height)
{
    UwamonoAlignedScreenPoint point = *position;
    UwamonoSpritePacket *packet = &sPoint_0;

    packet->color0.x = color->x;
    packet->color0.y = color->y;
    packet->color0.z = color->z;
    packet->color0.w = color->w;
    packet->color1.x = color->x;
    packet->color1.y = color->y;
    packet->color1.z = color->z;
    packet->color1.w = color->w;
    packet->position0.x = point.value.x - width;
    packet->position0.y = point.value.y - height;
    packet->position1.x = point.value.x + width;
    packet->position1.y = point.value.y + height;
    packet->position0.z = point.value.z;
    packet->position0.w = point.value.w;
    packet->position1.z = point.value.z;
    packet->position1.w = point.value.w;
    xglPrimAddGifTagDirect(0, packet, 7);
}

void SortLine(const UwamonoAlignedScreenPoint *from,
              const UwamonoAlignedScreenPoint *to, int mode)
{
    UwamonoAlignedScreenPoint first;
    UwamonoAlignedScreenPoint second;

    first.words[0] = from->words[0];
    first.words[1] = from->words[1];
    from = &first;
    second.words[0] = to->words[0];
    second.words[1] = to->words[1];
    to = &second;

    if (from->coordinates[2] >= 0 && to->coordinates[2] >= 0 && lineflg) {
        if (mode == 1) {
            sSortLine_1.color0.x = 0;
            sSortLine_1.color0.y = 0;
            sSortLine_1.color0.z = 255;
            sSortLine_1.color0.w = 0;
            sSortLine_1.color1.x = 0;
            sSortLine_1.color1.y = 0;
            sSortLine_1.color1.z = 255;
            sSortLine_1.color1.w = 0;
        } else {
            sSortLine_1.color0.x = 255;
            sSortLine_1.color0.y = 0;
            sSortLine_1.color0.z = 0;
            sSortLine_1.color0.w = 0;
            sSortLine_1.color1.x = 255;
            sSortLine_1.color1.y = 0;
            sSortLine_1.color1.z = 0;
            sSortLine_1.color1.w = 0;
        }
        sSortLine_1.position0.x = from->coordinates[0];
        sSortLine_1.position0.y = from->coordinates[1];
        sSortLine_1.position0.z = from->coordinates[2];
        sSortLine_1.position0.w = 128;
        sSortLine_1.position1.x = to->coordinates[0];
        sSortLine_1.position1.y = to->coordinates[1];
        sSortLine_1.position1.z = to->coordinates[2];
        sSortLine_1.position1.w = 128;
        nmlModelDirectSend(5, (unsigned char *)&sSortLine_1, 6);
    }
}

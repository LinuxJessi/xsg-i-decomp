/*
 * TU-local declarations of main/tu215 (src/main/sdv.c).
 */

#ifndef SRC_MAIN_SDV_H
#define SRC_MAIN_SDV_H

extern const Vector4 McMathUnitVector;

typedef struct SdvKeyCursor {
    int position;
    int elapsed;
} SdvKeyCursor;

typedef struct SdvCameraChoice {
    int selected;
    int script_base;
    unsigned char unmodeled_08[8];
    int animation;
    unsigned char unmodeled_14[8];
    int camera_commands[4][3];
} SdvCameraChoice;

/* These typed caller declarations agree with the defining sc_get and sef
 * functions; their generated owner headers do not yet export these names. */
extern int scGetImmAdrImmIdx2(int base, unsigned short *table, int index);
extern short scGetImmNumIdx(short *table, int index);
extern int sefIsEntryBoss(void);
extern int rand(void);

typedef short int16_t;

/*
 * Fresh Spark hypothesis F1: renamed partial camera-offset view.
 * Changed from the predecessor's SdvCameraOffset/offset_mode/offset_source
 * spelling: identical 48-byte layout and else-if branch shape, but distinct
 * type/field/local identifiers (SdvCamOffset/cam_kind/cam_vec, pos/ang) to
 * test the ABI saved-register hypothesis that local-alias lifetime spelling
 * keeps mode in s1 and source in s2. Unnamed 32-bit units preserve the
 * observed +0x10/+0x20 vector offsets and assert no historical semantics.
 */
typedef struct {
    int kind;
    unsigned int : 32;
    unsigned int : 32;
    unsigned int : 32;
    float pos[3];
    unsigned int : 32;
    float ang[3];
    unsigned int : 32;
} SdvCamOffset;

extern int _sdvAmbFrame;
extern int _sdvAmbState;

static void sdvSetAmbStateSub(int state, int effect_no, int force);
extern void *xglStudioGetLight2(void);
extern void xglLightIntensityAmbient(void *light, void *ambient);
extern void func_A2C3D8(void *map_rgb);
extern void xglSoundEffectNormalID(int sound_id, int variant);

extern int srsAnalyzeEftNo(int effectId, unsigned char *charId,
                            int *effectCategory);
extern unsigned char charID_0;
extern int eftCate_1;

/*
 * One 12-byte channel of a camera schedule task: sdvScheduleCamera reads
 * only the last word (nonzero enables the channel for this frame) and hands
 * the whole 12 bytes, or a channel-specific prefix of it, to sdvProgressPrm.
 * The first 8 bytes are otherwise unread by sdvScheduleCamera.
 */
typedef struct SdvCameraPhase {
    SdvKeyCursor cursor;           /* +0x00 */
    const short *active;           /* +0x08 */
} SdvCameraPhase;

/*
 * A battle camera schedule task (sefExecScheduler's caller data): only the
 * fields sdvScheduleCamera reads are named.
 */
typedef struct SdvCameraTask {
    int active;                    /* +0x00 */
    unsigned char unmodeled_04[0x10];
    SdvCameraPhase pos;             /* +0x14 */
    SdvCameraPhase angle;           /* +0x20 */
    SdvCameraPhase scale;           /* +0x2C */
    SdvCameraPhase offset;          /* +0x38 */
} SdvCameraTask;

/* A screen-space point: x and y in 1/16 pixels before the offset, z the depth. */
typedef struct SdvScreenPoint {
    int x;
    int y;
    int z;
    int w;
} SdvScreenPoint;

/* The now-running scheduler: the matrix a special effect starts from at +0,
 * its camera matrix at +0x40 and the flag word at +0xa8c, bit 1 of which asks
 * for the actor offset to go through it. */
typedef struct SdvScheduler {
    Matrix4 base_matrix;
    Matrix4 matrix;
    unsigned char unmodeled_80[0xa8c - 0x80];
    int flags;
} SdvScheduler;

/* One 20-byte key of a special effect table. */
typedef struct SdvSpecialKey {
    unsigned short time;            /* +0x00, 1024 ends the table */
    unsigned short depth_step;      /* +0x02 */
    unsigned char color[4];         /* +0x04 */
    short x;                        /* +0x08 */
    short y;                        /* +0x0a */
    unsigned short width;           /* +0x0c */
    unsigned short height;          /* +0x0e */
    unsigned short intensity;       /* +0x10 */
    short z;                        /* +0x12 */
} SdvSpecialKey;

/* A special effect table: the kind of effect, then its keys. */
typedef struct SdvSpecialTable {
    short kind;                     /* +0x00 */
    short part;                     /* +0x02, the actor part an offset follows */
    SdvSpecialKey keys[1];          /* +0x04 */
} SdvSpecialTable;

/* One of the seven running special effects: the matrix it was started with,
 * the key it currently shows, its key table, the frame in it and the key
 * that frame belongs to. */
typedef struct SdvSpecialSlot {
    Matrix4 matrix;                 /* +0x00 */
    SdvSpecialKey key;              /* +0x40 */
    SdvSpecialTable *table;         /* +0x54 */
    int frame;                      /* +0x58 */
    int index;                      /* +0x5c */
} SdvSpecialSlot;

extern short _sefBattleMode;
extern int sdvProgressPrm(int kind, void *data, int size);
extern void func_A31920(int kind, float *params);

void sdvScheduleCamera(SdvCameraTask *task);

extern void GameDefocusSet(int index, int, const int *);
extern short _sdvSpecialBuf[8];

void sdvClearSpecialWork(void);

/*
 * The 36-byte request that starts an alter: the actors the selectors 256 and
 * 512 name, the weapon the selectors from 1024 name, and the effect number
 * whose texture images the alter draws.
 */
typedef struct SdvActorPosition {
    unsigned char unmodeled_00[16];
    Vector4 position;               /* +0x10 */
} SdvActorPosition;

typedef struct SdvAlterRequest {
    SdvActorPosition *source_actor; /* +0x00 */
    SdvActorPosition *target_actor; /* +0x04 */
    void *weapon;                   /* +0x08 */
    unsigned char unmodeled_0c[8];
    short effect_no;                /* +0x14 */
    unsigned char unmodeled_16[14];
} SdvAlterRequest;

/*
 * One 0x1280-byte alter record. Only the +0x1278 status halfword that
 * sdvInitAlters, sdvDestroyAlters and sdvExecAlters read is named.
 */
/* RGBA channels share the original aligned 128-bit packet color. */
typedef unsigned int SdvPackedColor __attribute__((mode(TI)));

typedef union SdvQuad {
    SdvPackedColor quad;
    unsigned int word[4];
    unsigned long long dword[2];
} SdvQuad;

typedef struct SdvAlterParameters {
    SdvAlterRequest request;
    unsigned char unmodeled_24[12];
    SdvQuad rgba;                   /* +0x30 */
    unsigned long long tex0[2];     /* +0x40 */
    unsigned char unmodeled_50[4];
    float length;                   /* +0x54 */
    short selector[2];              /* +0x58 */
    short life;                     /* +0x5c */
    short max_samples;              /* +0x5e */
    unsigned short shrink;          /* +0x60 */
    short blend;                    /* +0x62 */
    short image_count;              /* +0x64 */
    short mode;                     /* +0x66 */
    unsigned short image_index;     /* +0x68 */
    short image_frames;             /* +0x6a */
    unsigned short image_timer;     /* +0x6c */
    short category;                 /* +0x6e */
} SdvAlterParameters;

typedef struct SdvAlter {
    SdvAlterParameters parameters;
    Vector4 trail[2][128];          /* +0x70 */
    float level[128];               /* +0x1070 */
    float phase;
    unsigned short cursor;
    unsigned short samples;
    unsigned short active;          /* +0x1278 */
    unsigned short wrapped;         /* +0x127a */
    short serial;
    unsigned char unmodeled_127e[2];
} SdvAlter;

extern int sdvCreateAlter(SdvAlterParameters *parameters);
extern void sdvInitAlter(SdvAlter *alter);
extern void sdvDestroyAlter(SdvAlter *alter);
extern void sdvExecAlter(SdvAlter *alter);
/* The GIF packet accumulator of the SGs* family: a growing array of 16-byte
 * records at `base`, `count` records long; `tag_index` is the record the open
 * GIF tag sits in and `loop_count` is what closing it ORs into its NLOOP. */
typedef struct SdvGifPacket {
    int count;                      /* +0x00 */
    int tag_index;                  /* +0x04 */
    int loop_count;                 /* +0x08 */
    void *base;                     /* +0x0c */
} SdvGifPacket;

extern void sdvDrawAlter(SdvAlter *alter, SdvGifPacket *packet);
extern SdvGifPacket _packet;
extern void SGsInitEnv(void);

void sdvInitAlters(void);
void sdvDestroyAlters(void);
void sdvExecAlters(void);

#endif /* SRC_MAIN_SDV_H */

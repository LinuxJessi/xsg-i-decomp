/*
 * TU-local declarations of main/tu126 (src/main/game_debug_menu.c).
 */

#ifndef SRC_MAIN_GAME_DEBUG_MENU_H
#define SRC_MAIN_GAME_DEBUG_MENU_H

#include "shared.h"

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

/*
 * PadData's pressed-button halfword (include/shared.h PadPrefix.half_2a).
 * PauseMenuPage0 and GameDebugMenu both test bit 0x40 of it; other TUs name
 * the same bit PAD_CANCEL (src/main/game_over.c) or PAD_BATU
 * (src/ov12/xrg_pad.c) for their own contexts. This TU's uses cycle/dismiss
 * the debug pages, so it keeps the physical button name.
 */
#define PAD_CROSS 0x0040
#define PAD_L1 0x0004
#define PAD_R1 0x0008
#define PAD_UP 0x1000
#define PAD_RIGHT 0x2000
#define PAD_DOWN 0x4000
#define PAD_LEFT 0x8000
#define PAD_CIRCLE 0x0020

/* PadData through +0x2e: the PadPrefix halfwords plus the two this TU's
 * pages also read. */
typedef struct {
    u8 pad_00[0x28];
    u16 half_28;
    u16 half_2a;
    u16 half_2c;
    u16 half_2e;
} PadDebugView;

extern PadDebugView PadData;

/*
 * The engine's actor record (`actor`, 64-entry array at main 0x0043c1e0,
 * 0xa70-byte stride; fuller evidence in src/main/near_dir.h,
 * src/main/enemy_2.h, src/main/set_motion.h and src/main/db_light_write.h).
 * PauseMenuPage0 reads only `inUseId` (+0x86, the in-use id ACT_create
 * writes and ACT_update skips a slot on when it is zero); the offsets
 * between the head and it are not evidenced by this TU and stay unmodeled.
 */
#define ACTOR_COUNT 64
#define ACTOR_IN_USE_ID_OFFSET 0x86

typedef struct {
    u32 flags;
    u8 unmodeled_04[ACTOR_IN_USE_ID_OFFSET - 0x04];
    short inUseId;
    u8 unmodeled_88[0xA70 - (ACTOR_IN_USE_ID_OFFSET + 2)];
} ActorHead;

extern ActorHead actor[ACTOR_COUNT];

void GameResourceDump(int dump);

/* GameDebugMenu selects six pages and resets debugPageInitialized when
 * cycling pages. Pages 2 and 3 keep their private state in the same bytes
 * of GameLoopState (+0x29f72 and +0x29f80), each with its own access
 * widths and signedness; the other GameLoopState bytes are not accessed by
 * this TU beyond the named members. The page-3 rows are listed in
 * tbl_1_00367A70. */
#define DEBUG_ROW_COUNT 10
#define MAP_COUNT 2000
#define SCRIPT_ID_EVENT_BASE 0x10000000

/* The unit GameLoopState points at (+0x04); page 3 resets and replaces its
 * position from the map's layout header. */
typedef struct {
    unsigned char unmodeled_00[0x10];
    float x;
    float y;
    float z;
    unsigned char unmodeled_1c[0x54 - 0x1c];
    float angle;
    unsigned char unmodeled_58[0x9e4 - 0x58];
    float layoutAngle;
} DebugPlayerUnit;

typedef struct {
    unsigned char unmodeled_00[0x04];
    DebugPlayerUnit *player;
    unsigned char unmodeled_08[0x0c - 0x08];
    short mode;
    unsigned char unmodeled_0e[0x10 - 0x0e];
    int runtimeFlags;
    unsigned char unmodeled_14[0x18 - 0x14];
    int scriptId;
    unsigned char unmodeled_1c[0x50 - 0x1c];
    unsigned short currentMapId;
    unsigned char unmodeled_52[0xc1 - 0x52];
    unsigned char cfCameraFlags;
    unsigned char unmodeled_c2[0xd0 - 0xc2];
    signed char snapQuality;
    unsigned char unmodeled_d1[0x29f50 - 0xd1];
    unsigned char debugPage;
    unsigned char debugPageInitialized;
    unsigned char unmodeled_29f52[0x29f71 - 0x29f52];
    signed char debugCursor;
    union {
        unsigned short modelNumberCheck;
        short mapChangeId;
    } target;
    unsigned short leaderBattleId;
    unsigned char unmodeled_29f76[0x29f80 - 0x29f76];
    union {
        unsigned int actorFlag8Clear[2];
        struct {
            unsigned short cfId;
            unsigned short eventId;
        } scriptJump;
    } scratch;
    unsigned char unmodeled_29f88[0x29f90 - 0x29f88];
    unsigned int mapUnitFlag4Clear[2];
} DebugMenuPageState;

extern DebugMenuPageState GameLoopState;

extern char *tbl_1_00367A70[];

extern char D_004C04B0[];
extern char D_004D9F18[];
extern char D_004D9F20[];
extern char D_004C0528[];
extern char D_004C0538[];
extern char D_004D9F48[];
extern char D_004C0548[];
extern char D_004D9F28[];
extern char D_004D9F30[];
extern char D_004D9F38[];

/* SaveData (include/umbrella/public.h) up to the leader's battle id
 * (+0x100a4); the TU reads only that halfword. */
typedef struct {
    unsigned char unmodeled_00[0x100a4];
    unsigned short leaderBattleId;
} DebugSavedParty;


/* GameResource: 128 resource blocks of 16 bytes (address, size, handle,
 * state); the page-3 usage line walks address and size. */
#define GAME_RESOURCE_COUNT 128
typedef struct {
    u32 address;
    int size;
    unsigned char unmodeled_08[0x10 - 0x08];
} DebugResourceSlot;

extern DebugResourceSlot GameResource[GAME_RESOURCE_COUNT];
extern int DebugMenuResourceSizeAdd;

typedef struct {
    float components[4];
} DebugLayoutHeader;

extern unsigned char ModelPath;
extern unsigned char ScenePath;
extern u8 UseTestPath;
extern int ScanLineInterpolate;

extern char *MapGetName(int mapId);
extern void MapChange(int mapId);
extern DebugLayoutHeader *UnduDataGetHeader(int mapIndex, int unitIndex);
extern void GameCfPlayerLoadResource(int resource);
extern int GameSnapShotNumber(int number);
extern void GameStateSave(void);
extern void GameStateRestore(void);
extern void func_A22668(void);
extern void Java_xeno_util_Runtime_jump_sub(int target, int argument);
extern char *MAP_getPath(void);
extern char *RES_getPath(void);
static void PauseMenuPage2(void);
static void PauseMenuPage3(void);
extern void PauseMenuPageUwamono(void);
extern void PauseMenuPageEnemy(void);
extern void PauseMenuPagePartyDebug(void);

#endif /* SRC_MAIN_GAME_DEBUG_MENU_H */

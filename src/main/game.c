#include "common.h"

#include "shared.h"

#include "game.h"
void GameStateSave(void);


/*
 * GNU EE native TI storage/copy type. Used only for the
 * 16-byte aligned typed storage that the original clears with por+sq; no
 * wide arithmetic.
 */

typedef struct GameOrgRecord {
    u8 unmodeled_00[0x180];
} GameOrgRecord;

typedef struct GamePlayerRecord {
    u8 unmodeled_00[0xa8];
} GamePlayerRecord;

typedef struct GameUserSave {
    u8 unmodeled_0000[0x101b0];
    int event_id;
    float params[8];
    u8 unmodeled_101d4[0x101e0 - 0x101d4];
    Vector4 actor_position;
    Vector4 actor_rotation;
    u8 unmodeled_10200[0x103b4 - 0x10200];
    GameOrgRecord org[33];
    GamePlayerRecord players[16];
    u8 unmodeled_13fb4[0x14634 - 0x13fb4];
    u16 item_box[256];
    u16 weapon_box[256];
    u16 bullet_box[256];
    u16 accessory_box[256];
    u16 event_box[256];
    u8 unmodeled_15034[4];
    u64 money;
    u8 unmodeled_15040[0x16340 - 0x15040];
    u32 user_word;
    u8 unmodeled_16344[0x16350 - 0x16344];
    u64 user_longs[2];
} GameUserSave;

extern GameOrgRecord orgData[33];

extern GamePlayerRecord plChaData[16];

extern u16 itmBox[255];

extern u16 wpnBox[255];

extern u16 bltBox[255];

extern u16 accBox[255];

extern u16 evtBox[255];

extern long long moneyBox;

typedef union GameVector8 {
    Vector4 vector;
    u64 pair[2];
} GameVector8;

typedef struct GameMapUnit {
    u32 flags;
    u8 unmodeled_04[0x10 - 0x04];
    GameVector8 position;
    u8 unmodeled_20[0x60];
    Matrix4 *orientation;
    u8 unmodeled_84[0x1e];
    u8 action_sub;
    u8 unmodeled_a3;
    s16 serial;
    u8 unmodeled_a6[0x0a];
    GameVector8 extent;
    u8 unmodeled_c0[0x1a0 - 0xc0];
    s16 symbol_kind;
    s16 shop_number;
    u8 item_box_hit;
    u8 unmodeled_1a5[0x300 - 0x1a5];
} GameMapUnit;

typedef struct GameClearEnvironment {
    u8 unmodeled_00[0x20];
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
} GameClearEnvironment;

typedef struct GameTestEnvironment {
    u8 unmodeled_00[0x30];
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u8 unmodeled_40[0x60 - 0x40];
} GameTestEnvironment;

extern GameClearEnvironment ClearEnv;

extern GameTestEnvironment TestEnv_2_00338620;

extern int StudioEntrySkip;

typedef struct GameSequenceSlot {
    u32 flags;
    u32 state_flags;
    u8 unmodeled_08[4];
    u32 cleared_on_init;
    u8 unmodeled_10[4];
    u32 cleared_on_init_run[4];
    u32 handler[4];
    u8 unmodeled_34[0x260 - 0x34];
} GameSequenceSlot;

extern GameSequenceSlot actSequence[64];

/* GameDebugMenu accesses the first two bytes at GameLoopState+0x29f50.
 * Mode exits reset this complete 16-byte region with one native quadword
 * store. The remaining bytes have no field interpretation in these TUs. */
typedef union GameDebugMenuStorage {
    Quadword storage;
    struct {
        u8 page;
        u8 page_initialized;
        u8 unmodeled_02[14];
    } fields;
} GameDebugMenuStorage;

/*
 * PARTIAL ACCESSED VIEW of GameLoopState (0x2a030-byte object at 0x338680).
 * Union of the offsets the merged handlers access:
 *  +0x0c kind u16, +0x10 flags u32, +0x28 camera_callback,
 *  +0x29f40 status u8 (read by GameModeCfEvent),
 *  +0x29f50 debug_menu 16-byte reset region (cleared with por+sq),
 *  +0x29f60 saved_kind u16.
 * Spans named _unmodeled_* are opaque sizing bytes never accessed by these
 * bodies; the remainder of the object beyond 0x29f62 is unmodeled. The name
 * is TU-local by canon
 */

/*
 * GameStateSave/GameStateRestore back up and restore two more GameLoopState
 * halfwords (main VA 0x00243278/0x002432f0): +0x0e kindDetail, the halfword
 * GameStateSave copies alongside +0x0c kind, and the +0xe0/+0xe2 backup slot
 * GameStateSave writes them into and GameStateRestore reads back to restore
 * kind and kindDetail.
 */

/* Private defining-owner proposals. NmlModel's canonical serialized
 * resource prefix/table comes from src/main/nml_model_set.h175..194.
 * The original Game_Data_Push reads each block's signed index at +0x2c.
 * The variable resource tail remains an explicit owner transport concern. */

typedef struct NmlModel {
    unsigned char unmodeled_000[0x44];
    int blockCount;
    unsigned char unmodeled_048[0x68];
    int blockOffsets[1];
} NmlModel;

/* Only the loaded data pointer at +4 is used by the original here.
 * No complete resource-handle extent or unknown prefix field is claimed. */
typedef struct GameResourceModelPrefix {
    unsigned char unmodeled_00[4];
    NmlModel *data;
} GameResourceModelPrefix;

typedef struct ActorHead {
    u32 flags;
    void (*update)(void *actor); /* +0x4 */
    unsigned char unmodeled_008[0x10 - 0x8];
    Vector4 position;
    unsigned char unmodeled_020[0x50 - 0x20];
    Vector4 rotation; /* +0x50 */
    unsigned char unmodeled_060[0x80 - 0x60];
    u8 number;
    u8 unmodeled_81;
    u8 tag_mark;
    unsigned char unmodeled_083[0x84 - 0x83];
    short state84; /* +0x84 */
    short inUseId;
    u8 unmodeled_88[0x4e8 - 0x88];
    u64 attribute_flags;
    u8 unmodeled_4f0[0x6f0 - 0x4f0];
    u32 motion_flags;
    unsigned char unmodeled_6f4[0x6f8 - 0x6f4];
    float motion_speed; /* +0x6f8 */
    unsigned char unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x70a - 0x706];
    u16 matrix_buffer_toggle;
    u8 unmodeled_70c[0x824 - 0x70c];
    void *jointMatrices;
    void *previousJointMatrices;
    void *staticValueRecords;
    void *valueRecords;
    u8 unmodeled_834[0x8fc - 0x834];
    struct Actor *parent;
    unsigned char unmodeled_900[0x9f4 - 0x900];
    int talk_message; /* +0x9f4 */
    int touch_message;
    u8 unmodeled_9fc[0xA70 - 0x9fc];
} ActorHead;


/* Accurate defining-owner proposals; normal owner transport is pending.
 * MapUnitSlot fields/extent: src/main/map_2.h. ActorUndulation and
 * LookAtPlayerActor spelling: src/main/look_at_player.c21..75. */
typedef struct MapUnitSlot {
    u32 flags;
    void (*update)(struct MapUnitSlot *unit);
    u8 unmodeled_08[8];
    /* Vector consumer at CheckGameSymbol and position producers agree on
     * four floats at+0x10; canonical map_2.h uses float[4], so this nominal
     * proposal requires defining-owner transport. Extent remains0x300. */
    Vector4 position;
    float rotation[4];
    float scale[4];
    u8 unmodeled_40[0xa4 - 0x40];
    s16 serial;
    u8 unmodeled_a6[0x1a0 - 0xa6];
    s16 symbol_kind;
    s16 shop_id;
    u8 unmodeled_1a4[0x300 - 0x1a4];
} MapUnitSlot;
extern MapUnitSlot MapUnit[64];

typedef struct ActorUndulation {
    u8 unmodeled_00[8];
    s16 attr_mask;
} ActorUndulation;
typedef struct PlayerEffect PlayerEffect;
struct LookAtPlayerActor {
    u32 flags;
    void (*update)(struct LookAtPlayerActor *actor);
    void (*draw)(struct LookAtPlayerActor *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 external_velocity;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float move_speed;
    Vector4 scale;
    u8 unmodeled_70[0x10];
    u8 number;
    u8 unmodeled_81[0x4c8 - 0x81];
    ActorUndulation undulation;
    u8 unmodeled_4d2[0x4e8 - 0x4d2];
    u64 terrain_flags;
    u8 unmodeled_4f0[0x694 - 0x4f0];
    signed char stick_x;
    signed char stick_y;
    u8 unmodeled_696[0x6f8 - 0x696];
    float motion_speed;
    u8 unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x9a0 - 0x706];
    u32 render_flags;
    u8 unmodeled_9a4[0x9c0 - 0x9a4];
    float filter_param;
    u8 unmodeled_9c4[0x9d8 - 0x9c4];
    u64 flash_frames;
    u8 unmodeled_9e0[0x9e4 - 0x9e0];
    float aim_angle;
    float body_radius;
    s16 look_at_timer;
    s16 look_at_target;
    s16 flash_timer;
    u8 unmodeled_9f2[0xa10 - 0x9f2];
    PlayerEffect *effects[8];
    s16 effect_timer;
    s16 effect_slot;
    u8 unmodeled_a34[0xa70 - 0xa34];
};
/* Partial defining-owner prefix, from act_2.h. Only these two genuine
 * parent fields are accessed; no complete Actor extent is claimed here. */
struct Actor {
    u8 unmodeled_00[0x86];
    short resourceKind;
    u8 unmodeled_88[0x824 - 0x88];
    void *jointMatrices;
};
extern void ACT_setMotion2(struct Actor *actor, int motion, unsigned int flags);
extern void ACT_updateMotion(struct Actor *actor);
extern int xglCdSync(void);
#include "main/xgl_thread.h"
extern void GameResourceLoad(int scene_id);
extern void GameStateRestore(void);
extern void sefPopEffect(void);
extern void UnlockMapUnit(void);
extern void ClearUwamonoEffect(MapUnitSlot *unit);
extern void nmlModelSetPartsVisible(NmlModel *model, int part, int visible);

typedef struct GameLightParallel {
    Quadword intensity;
    Quadword direction;
} GameLightParallel;

typedef struct GameLightSet {
    Quadword ambient_intensity;
    GameLightParallel parallel[3];
    Quadword matrices[8];
} GameLightSet;

typedef struct GameLoopStateLayout {
    u8 _unmodeled_00[4];
    struct LookAtPlayerActor *controlled_actor;
    u8 _unmodeled_08[4];
    u16 kind;
    u16 kindDetail;
    union {
        struct {
            u32 flags;
            u32 base_flags;
        };
        u64 flags_wide; /* +0x10, original GameDrawSync/GameExecute 64-bit flags */
    };
    u32 current_event_id;
    u32 cleared_1c;
    u32 shoot_control;
    u8 _unmodeled_24[4];
    GameModeCameraCallback camera_callback;
    void (*clear_callback)(void); /* +0x2c */
    void (*map_draw_callback)(void); /* +0x30 */
    void (*actor_draw_callback)(void); /* +0x34 */
    void (*shadow_draw_callback)(void); /* +0x38 */
    void (*unit_draw_callback)(void); /* +0x3c */
    void (*frame_hook)(int stage); /* +0x40 */
    unsigned char unmodeled_044[0x50 - 0x44];
    u16 map_number; /* +0x50 */
    unsigned char unmodeled_052[0x54 - 0x52];
    GameResourceModelPrefix *model;
    unsigned char unmodeled_058[0x60 - 0x58];
    float params[8]; /* +0x60 */
    unsigned char unmodeled_080[0xb8 - 0x80];
    int frame_countdown; /* +0xb8 */
    unsigned char unmodeled_0bc[0xc1 - 0xbc];
    u8 move_mode;
    u8 _unmodeled_c2[0xe0 - 0xc2];
    u16 restoreKind;
    u16 restoreKindDetail;
    u8 _unmodeled_e4[0x164 - 0xe4];
    u32 hidden_parts[32];
    unsigned char unmodeled_1e4[0x1f0 - 0x1e4];
    u32 saved_actor_flags; /* +0x1f0 */
    short saved_actor_state84; /* +0x1f4 */
    short saved_actor_in_use_id; /* +0x1f6 */
    void (*saved_actor_update)(void *actor); /* +0x1f8 */
    unsigned char unmodeled_1fc[0x200 - 0x1fc];
    Vector4 saved_actor_position; /* +0x200 */
    Vector4 saved_actor_rotation; /* +0x210 */
    u16 saved_map_number; /* +0x220 */
    unsigned char unmodeled_222[0x250 - 0x222];
    ActorHead saved_actors[64];
    GameLightSet saved_light; /* +0x29e50, 0xf0-byte native-TI camera light snapshot */
    u8 status;
    u8 _unmodeled_29f41[3];
    float shoot_range;
    u8 _unmodeled_29f48[8];
    GameDebugMenuStorage debug_menu;
    u16 saved_kind;
    unsigned char unmodeled_29f62[0x2a018 - 0x29f62];
    u32 user_word; /* +0x2a018 */
    unsigned char unmodeled_2a01c[0x2a020 - 0x2a01c];
    u64 user_longs[2]; /* +0x2a020 */
} GameLoopStateLayout;

GameLoopStateLayout GameLoopState = {0};

unsigned char SnapDrawCreditFlag = 0;

unsigned char UseTestPath = 0;

/*
 * PadData (0xd0-byte object at 0x490d90) through the shared
 * PadDataDebugLayout view (xeno/core/types.h): the merged handlers read the
 * halfwords at +0x28 (held), +0x2a (pressed) and +0x2e (debug_buttons,
 * GameModeDebugMenu). The canonical `extern PadPrefix PadData;` of
 * xeno/core/functions.h stops at +0x2c, so that header is not included and
 * the prototypes this TU needs are repeated below in their canonical
 * spelling.
 */

extern PadDataDebugLayout PadData;

extern void xglSoundEffectNormalDirect(int effect_id);

extern void TWSYS_update(void);

extern void EvtTools(void);

extern void PauseMenu(void);

extern void PartyTimePauseEnd(void);

extern void ACT_pauseUpdate(void);

extern StudioCamera *xglStudioGetCamera2(int camera_id);

extern void JTHREAD_cntl(void);

extern void PLAY_ctrl(void);

extern void TCAMERA_update(void);

extern void ACT_update(void);

extern void MAP_updateUnit(void);

extern void GameDebugMenu(void);

int GameModeDebugMenu(void);

#define game_mode_flags (GameLoopState.flags)

#define game_mode_kind (GameLoopState.kind)

#define game_mode_status (GameLoopState.status)

#define game_mode_saved_kind (GameLoopState.saved_kind)

#define game_mode_debug_storage (GameLoopState.debug_menu.storage)

#define game_mode_camera_callback (GameLoopState.camera_callback)

extern void xglSoundSendEffect(void *swd, void *sed, int bank);

extern void After_Talk(SceneObject object);

typedef struct EnemyWorkPostTalk {
    u8 unmodeled_00[0x37b4];
    u32 post_talk_flags;
    u8 unmodeled_37b8[0x38b0 - 0x37b8];
} EnemyWorkPostTalk;

extern EnemyWorkPostTalk enepc[16];

#define ACTOR_NUMBER_OFFSET 0x80

extern void GameStateRestoreCameraLight(void);

extern void GameStateRestoreKoware(void);

static u64 attrPrev;

typedef struct GameActorAttributeView {
    u8 unmodeled_00[0x4e8];
    u64 attribute_flags;
} GameActorAttributeView;

#define ACTOR_SCRIPT_FLAGS_OFFSET 0x124

/*
 * The engine's actor record (`actor`, 64-entry array at main 0x0043c1e0,
 * 0xa70-byte stride; fuller evidence in src/main/near_dir.h,
 * src/main/enemy_2.h, src/main/set_motion.h, src/main/db_light_write.h and
 * src/main/tya.c, which name the same two fields for the same reason).
 * GameDrawShadow only reads +0x00 flags (bits 0x8 and 0x20) and +0x86, the
 * in-use id ACT_create writes and ACT_update skips a slot on when it is
 * zero; the offsets between them are not evidenced by this TU and stay
 * unmodeled.
 */

#define ACTOR_COUNT 64

#define ACTOR_IN_USE_ID_OFFSET 0x86


extern ActorHead actor[ACTOR_COUNT];

/* Defined in src/main/act_3.c (main/tu265). */

void ACT_DrawShadowBegin(void);

void ACT_DrawShadow(ActorHead *unit);

void ACT_DrawShadowEnd(void);

#include "main/control_entry.h"

#include "main/xgl_sound.h"

static void GameCFSoundPurgeSub(void)
{
    int index = 0;

    xglSoundSendEffect(0, 0, 2);
    xglSoundSendEffect(0, 0, 3);
    for (; index < 8; index++) {
        xglSoundSendEffect(0, 0, index + 4);
    }
}

static void GameCFSoundPurge(void)
{
    int bank;

    for (bank = 0; bank < 8; bank++) {
        xglSoundSendSwd(0, -1 - bank);
        xglSoundSendSmd2(0, bank);
    }
    xglSoundSendEffect(0, 0, 1);
    GameCFSoundPurgeSub();
    SsdResetSegmentAllocMode(0x70000);
    SsdSetSegmentAllocMode(0xA8000, 0x10000);
}

static void GameCFSoundReload(void)
{
    int bank;
    u8 *segment;

    segment = (u8 *) ((u32) (WorkEnd + 0x3F) & ~0x3F);
    for (bank = 0; bank < 8; bank++) {
        xglSoundSendSwd(0, -1 - bank);
        xglSoundSendSmd2(0, bank);
    }
    SsdResetSegmentAllocMode(0xA8000);
    SsdSetSegmentAllocMode(0x70000, 0x10000);
    xglSoundLoadEffect(D_004BE2B0, segment, 1);
    GameCFSoundPurgeSub();
}

void GameCFSoundMenuPurge(int mode)
{
    int bank;
    int result[4];
    unsigned short sequence;

    if (mode == 1) {
        GameCFSoundMenuPurgeFlag = 0;
        xglSoundSendEffect(0, 0, 3);
        for (bank = 0; bank < 8; bank++) {
            sequence = SoundWork.channels[bank].sequence;
            if (sequence == 0xFFFF) {
                continue;
            }
            SsdGetSeqPlayStatus(sequence);
            do {
            } while (SsdGetResultValue(result) < 0);
            if ((unsigned short)result[0] == 1) {
                GameCFSoundMenuPurgeFlag |= 1 << bank;
                xglSoundSequenceStop2(bank);
            }
        }
    }
    EnemySound_StopAll(1);
    for (bank = 4; bank < 8; bank++) {
        xglSoundSendEffect(0, 0, bank + 4);
    }
}

void GameCFSoundMenuReload(void)
{
    char environment_name[64];
    const char *enemy_name;
    void *free_address;
    int channel;
    int enemy_index;

    free_address = GameResourceGetFreeAddr();
    xglSoundSendSwd(0, -5);
    xglSoundSendSmd2(0, 4);
    RES_GetMapEnvSeName(environment_name);
    xglSoundLoadEffect(environment_name, free_address, 3);

    for (channel = 0; channel < 8; channel++) {
        if (((GameCFSoundMenuPurgeFlag >> channel) & 1) != 0 &&
            UmnSimulationNo == 0 && MenuDrillCall == 0) {
            xglSoundSequenceNormal2(channel, 0x7F);
        }
    }

    for (enemy_index = 4; ; enemy_index++) {
        enemy_name = RES_GetEnemySeName(enemy_index);
        if (enemy_name == 0 || enemy_name[0] == 0) {
            break;
        }
        xglSoundLoadEffect(enemy_name, free_address, enemy_index + 4);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/game", Game_Data_Push);

void Game_Data_Pop(void)
{
    unsigned short serials[64];
    short index;
    ActorHead *backup;
    NmlModel *model;
    int bit;

    for (index = 0; index < 64; index++) {
        serials[index] = MapUnit[index].serial;
        MapUnit[index].serial = -1;
    }
    while (xglCdSync() != 0)
        xglSleep();
    GameCFSoundReload();
    sefInitEffectCf();
    GameResourceLoad(GameLoopState.current_event_id);
    for (index = 0; index < 64; index++)
        MapUnit[index].serial = serials[index];
    GameStateRestore();
    GameLoopState.cleared_1c = 0;
    backup = GameLoopState.saved_actors;

    for (index = 0; index < ACTOR_COUNT; index++, backup++) {
        actor[index] = *backup;
        if (actor[index].inUseId == 0)
            continue;
        {
            ActorHead *unit = &actor[index];
            {
                void *matrices;

                ACT_setMotion2((struct Actor *)unit, actor[index].motion_id, actor[index].motion_flags & ~3);
                ACT_updateMotion((struct Actor *)unit);
                matrices = unit->jointMatrices;
                unit->jointMatrices = unit->previousJointMatrices;
                unit->previousJointMatrices = matrices;
                matrices = unit->staticValueRecords;
                unit->staticValueRecords = unit->valueRecords;
                unit->valueRecords = matrices;
                unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
            }
            {
                void *matrices;

                ACT_setMotion2((struct Actor *)unit, actor[index].motion_id, actor[index].motion_flags & ~3);
                ACT_updateMotion((struct Actor *)unit);
                matrices = unit->jointMatrices;
                unit->jointMatrices = unit->previousJointMatrices;
                unit->previousJointMatrices = matrices;
                matrices = unit->staticValueRecords;
                unit->staticValueRecords = unit->valueRecords;
                unit->valueRecords = matrices;
                unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
            }
        }
    }
    sefPopEffect();
    GameLoopState.flags &= ~0x400000;
    GameLoopState.flags &= ~0x800000;
    GameLoopState.flags &= ~0x20000000;
    GameLoopState.flags &= ~0x40000000;
    UnlockMapUnit();
    for (index = 0; index < 64; index++) {
        if (MapUnit[index].serial != -1 && (MapUnit[index].flags & 0x20000)) {
            MapUnitSlot *map_unit = &MapUnit[index];
            MapUnit[index].serial = -1;
            MapUnit[index].update = 0;
            ClearUwamonoEffect(map_unit);
        }
    }
    model = GameLoopState.model->data;
    for (index = 0; index < 32; index++) {
        for (bit = 0; bit < 32; bit++) {
            /* Original uses signed SRAV before selecting the one bit.
             * This scalar interpretation does not erase an interface. */
            if (((int)GameLoopState.hidden_parts[index] >> bit) & 1)
                nmlModelSetPartsVisible(model, index * 32 + bit, 0);
        }
    }
    {
        u32 flags = GameLoopState.flags;

        GameLoopState.controlled_actor->undulation.attr_mask |= 0x1000;
        flags &= ~0x4000;
        flags &= ~0x8000;
        flags &= ~0x40;
        GameLoopState.flags = flags;
        GameLoopState.move_mode |= 0x20;
    }
}

void MenuGameDataPush(void)
{
    short i;

    for (i = 0; i < ACTOR_COUNT; i++) {
        GameLoopState.saved_actors[i] = actor[i];
    }
    GameStateSave();
}

INCLUDE_ASM("asm/main/nonmatchings/game", MenuGameDataPop);

void GameStateSaveCameraLight(void)
{
    GameLightSet *saved_light = &GameLoopState.saved_light;

    GameCameraStateSave();
    *saved_light = *(GameLightSet *)xglStudioGetLight2();
}

void GameStateRestoreCameraLight(void)
{
    GameLightSet *light;
    GameLightSet *saved_light;

    GameCameraStateRestore();
    light = (GameLightSet *)xglStudioGetLight2();
    saved_light = &GameLoopState.saved_light;
    *light = *saved_light;
}

void GameStateSave(void)
{
    ActorHead *controlled_actor;

    GameLoopState.restoreKind = GameLoopState.kind;
    GameLoopState.restoreKindDetail = GameLoopState.kindDetail;
    controlled_actor = (ActorHead *)GameLoopState.controlled_actor;
    GameLoopState.saved_actor_flags = controlled_actor->flags;
    GameLoopState.saved_actor_state84 = controlled_actor->state84;
    GameLoopState.saved_actor_in_use_id = controlled_actor->inUseId;
    GameLoopState.saved_actor_update = controlled_actor->update;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&GameLoopState.saved_actor_position), "r" (&controlled_actor->position)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&GameLoopState.saved_actor_rotation), "r" (&controlled_actor->rotation)
        : "$2", "memory");
    GameLoopState.saved_map_number = GameLoopState.map_number;
    GameStateSaveCameraLight();
}

void GameStateRestore(void)
{
    GameLoopState.kind = GameLoopState.restoreKind;
    GameLoopState.kindDetail = GameLoopState.restoreKindDetail;
    GameStateRestoreCameraLight();
    GameStateRestoreKoware();
}

void GamePushSaveDataUser(void)
{
    GameUserSave *save;
    GameOrgRecord *org;
    GamePlayerRecord *players;
    GameLoopStateLayout *state = &GameLoopState;
    int i;

    save = (GameUserSave *)(SaveData + 0x50);
    save->event_id = state->current_event_id;
    for (i = 0; i < 8; i++) {
        save->params[i] = state->params[i];
    }
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&save->actor_position), "r" (&state->saved_actor_position)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&save->actor_rotation), "r" (&state->saved_actor_rotation)
        : "$2", "memory");
    org = save->org;
    players = save->players;
    for (i = 0; i < 33; i++) {
        org[i] = orgData[i];
    }
    for (i = 0; i < 16; i++) {
        players[i] = plChaData[i];
    }
    for (i = 0; i < 255; i++) {
        save->item_box[i] = itmBox[i];
        save->weapon_box[i] = wpnBox[i];
        save->bullet_box[i] = bltBox[i];
        save->accessory_box[i] = accBox[i];
        save->event_box[i] = evtBox[i];
    }
    save->money = moneyBox;
    save->user_word = state->user_word;
    save->user_longs[0] = state->user_longs[0];
    save->user_longs[1] = state->user_longs[1];
}

void GamePopSaveDataUser(void)
{
    GameUserSave *save;
    GameOrgRecord *org;
    GamePlayerRecord *players;
    GameLoopStateLayout *state = &GameLoopState;
    int i;

    save = (GameUserSave *)(SaveData + 0x50);
    state->current_event_id = save->event_id;
    for (i = 0; i < 8; i++) {
        state->params[i] = save->params[i];
    }
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&state->saved_actor_position), "r" (&save->actor_position)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&state->saved_actor_rotation), "r" (&save->actor_rotation)
        : "$2", "memory");
    org = save->org;
    players = save->players;
    for (i = 0; i < 33; i++) {
        orgData[i] = org[i];
    }
    for (i = 0; i < 16; i++) {
        plChaData[i] = players[i];
    }
    for (i = 0; i < 255; i++) {
        itmBox[i] = save->item_box[i];
        wpnBox[i] = save->weapon_box[i];
        bltBox[i] = save->bullet_box[i];
        accBox[i] = save->accessory_box[i];
        evtBox[i] = save->event_box[i];
    }
    moneyBox = save->money;
    state->user_word = save->user_word;
    state->user_longs[0] = save->user_longs[0];
    state->user_longs[1] = save->user_longs[1];
    SsdSetOutputMode(SaveData[0x28]);
    PadData.vibrationReset = SaveData[0x3c];
    PadData.vibrationStatus = SaveData[0x3c];
    tyaDisplaySetting(0);
    xglHddActivate(SaveData[0x40] ^ 1);
}

INCLUDE_ASM("asm/main/nonmatchings/game", InitCfSystem);

static int checkAttr(GameActorAttributeView *actor)
{
    u64 attributes;
    u64 selected_attributes;

    if (actor == 0) {
        return 0;
    }
    attributes = actor->attribute_flags;
    selected_attributes = attributes & 0x1f00;
    if (attributes & 0xe000) {
        return 0;
    }
    selected_attributes >>= 8;
    attrPrev = selected_attributes;
    return selected_attributes;
}

int getScriptFlag(SceneObject object)
{
    int *script_flags = (int *)((SceneByte *)object + ACTOR_SCRIPT_FLAGS_OFFSET);
    return *script_flags;
}

static ActorHead *checkTalk(ActorHead *player)
{
    int index;
    float best_distance;
    ActorHead *best_actor;
    ActorHead *target;
    Vector4 origin;
    float delta_x;
    float delta_z;
    float distance;
    float limit;
    float reach_margin;

    best_actor = 0;
    origin.x = player->position.x;
    origin.y = player->position.y;
    origin.z = player->position.z;
    best_distance = 65535.0f;
    origin.w = 0.25f;
    if (GameLoopState.flags & 0x400) {
        return 0;
    }
    if (GameLoopState.flags & 0x2000) {
        return 0;
    }
    if (GameLoopState.flags & 0x1000) {
        return 0;
    }
    if (UseVMFlag != 0) {
        return 0;
    }
    if (XTK_getWindowOwner() != 0) {
        return 0;
    }
    for (index = 0; index < ACTOR_COUNT; index++) {
        target = &actor[index];
        reach_margin = 1.0f;
        if (!(__builtin_fabsf(target->position.y - player->position.y) > 2.0f)
            && target->talk_message != 0 && (getScriptFlag((SceneObject)target) & 0x10000)
            && target->inUseId != 0 && target->number != player->number) {
            delta_x = target->position.x - origin.x;
            delta_z = target->position.z - origin.z;
            distance = delta_x * delta_x + delta_z * delta_z;
            limit = __builtin_fabsf(origin.w + reach_margin);
            distance = __builtin_fabsf(distance);
            if (distance < limit
                && __builtin_fabsf(nearDir(atan2f(delta_x, delta_z), player->rotation.y)) < 1.0471976f
                && distance < best_distance) {
                best_distance = distance;
                best_actor = target;
            }
        }
    }
    return best_actor;
}

INCLUDE_ASM("asm/main/nonmatchings/game", checkTouch);

int checkItemBoxDir(ActorHead *actor, GameMapUnit *unit)
{
    Vector4 start;
    Vector4 end;
    GameVector8 extent;
    GameVector8 position;
    Matrix4 *orientation;

    position = unit->position;
    extent = unit->extent;
    orientation = unit->orientation;
    start.x = position.vector.x - (extent.vector.x * (*orientation)[0][0] + extent.vector.z * (*orientation)[2][0]);
    start.y = position.vector.y;
    start.z = position.vector.z - (extent.vector.x * (*orientation)[0][2] + extent.vector.z * (*orientation)[2][2]);
    end.x = position.vector.x + (extent.vector.z * (*orientation)[0][2] + extent.vector.x * (*orientation)[2][2]);
    end.y = position.vector.y;
    end.z = position.vector.z - (extent.vector.z * (*orientation)[0][0] + extent.vector.x * (*orientation)[2][0]);
    return CheckPointLine(&start, &end, &actor->position) == 2;
}

static GameMapUnit *checkItemBox(ActorHead *actor)
{
    int index;
    float best_distance;
    GameMapUnit *best_unit;
    GameMapUnit *unit;
    Vector4 origin;
    float delta_x;
    float delta_z;
    float extent_x;
    float extent_z;
    float distance;
    float reach_margin;

    best_distance = 65535.0f;
    best_unit = 0;
    origin.x = actor->position.x;
    origin.y = actor->position.y;
    origin.z = actor->position.z;
    for (index = 0; index < 64; index++) {
        unit = (GameMapUnit *)&MapUnit[index];
        if (!(__builtin_fabsf(actor->position.y - unit->position.vector.y) > 1.8f) && unit->serial != -1
            && (unit->flags & 0x08000000) && unit->action_sub == 0) {
            reach_margin = 1.0f;
            delta_x = unit->position.vector.x - origin.x;
            delta_z = unit->position.vector.z - origin.z;
            extent_x = unit->extent.vector.x;
            extent_z = unit->extent.vector.z;
            distance = delta_x * delta_x + delta_z * delta_z;
            if (extent_z < extent_x) {
                origin.w = extent_x * extent_x;
            } else {
                origin.w = extent_z * extent_z;
            }
            distance = __builtin_fabsf(distance);
            if (distance < __builtin_fabsf(origin.w + reach_margin)
                && __builtin_fabsf(nearDir(atan2f(delta_x, delta_z), actor->rotation.y)) < 1.0471976f
                && distance < best_distance && checkItemBoxDir(actor, unit) != 0) {
                best_distance = distance;
                best_unit = unit;
            }
        }
    }
    return best_unit;
}





INCLUDE_ASM("asm/main/nonmatchings/game", CheckGameSymbol);

/* The RTPS producer writes integer screen xyz and float reciprocal w.
 * Its frozen Vector4 output prototype remains an explicit nominal
 * transport boundary; this private diagnostic does not erase that type. */
typedef struct GameProjectedPoint {
    int x;
    int y;
    int z;
    float w;
} GameProjectedPoint;

static void GameDispTag(void)
{
    Matrix4 matrix;
    GameProjectedPoint projected;
    Vector4 origin;
    int screen_x;
    int screen_y;
    int depth;
    int camera_id;
    int index;
    unsigned int color;
    char *format;
    char *map_format;

    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    origin.w = 1.0f;
    for (camera_id = 0; camera_id < 8; camera_id++) {
        if (xglStudioGetCamera2(camera_id)->active == 1)
            break;
    }
    format = str_0;
    for (index = 0; index < ACTOR_COUNT; index++) {
        ActorHead *unit = &actor[index];
        struct Actor *parent;

        if (unit->inUseId == 0)
            continue;
        parent = unit->parent;
        if ((unit->inUseId & 0xf000) == 0 && parent == 0) {
            Matrix4 *model_matrices = unit->jointMatrices;
            xglMatrixMul((Matrix *)matrix, (Matrix *)model_matrices[0], (Matrix *)model_matrices[45]);
            matrix[3][3] = 1.0f;
            xglRotTransPers((Vector4 *)&projected, 0, (Vector4 *)matrix[3], camera_id);
        } else {
            if (parent != 0 && (parent->resourceKind & 0xf000) == 0) {
                Matrix4 *parent_matrices = parent->jointMatrices;
                xglMatrixMul((Matrix *)matrix, (Matrix *)parent_matrices[0], (Matrix *)parent_matrices[45]);
            } else {
                const Matrix4 *root_matrix = unit->jointMatrices;
                /* The original copies these four complete matrix rows
                 * with the SDK quadword idiom. The scratch is confined
                 * to this block; both bases are genuine C objects. */
                __asm__ __volatile__(
                    "lq $2,0(%0)\n\t"
                    "sq $2,0(%1)\n\t"
                    "lq $2,16(%0)\n\t"
                    "sq $2,16(%1)\n\t"
                    "lq $2,32(%0)\n\t"
                    "sq $2,32(%1)\n\t"
                    "lq $2,48(%0)\n\t"
                    "sq $2,48(%1)"
                    :
                    : "r"(root_matrix), "r"(matrix)
                    : "$2", "memory");
            }
            matrix[3][3] = 1.0f;
            xglRotTransPers((Vector4 *)&projected, 0, (Vector4 *)matrix[3], camera_id);
        }
        screen_x = projected.x;
        /* Unsigned subtraction preserves the original SLTIU window
         * without signed overflow for arbitrary projected coordinates. */
        if ((unsigned int)screen_x - 0x7001u >= 0x1fffu ||
            projected.y < 0x7201 || projected.y > 0x8dff ||
            projected.z <= 0)
            continue;
        screen_y = projected.y;
        depth = projected.z;
        color = 0xffffff;
        if (unit->tag_mark == 1) {
            color = 0xff9933;
        } else {
            switch (unit->inUseId & 0xf000) {
            case 0: color = 0x3399ff; break;
            case 0x4000: color = 0x33ff33; break;
            case 0x2000: color = 0xff3333; break;
            }
        }
        format[1] = color >> 16;
        format[2] = color >> 8;
        format[3] = color;
        xglFontDebugPrintf(((screen_x >> 4) - 0x700) >> 1,
                           ((screen_y >> 4) - 0x720) >> 1,
                           format, index + 0x200, depth);
    }
    map_format = str_1;
    for (index = 0; index < 64; index++) {
        MapUnitSlot *unit = &MapUnit[index];
        if (unit->serial < 0)
            continue;
        xglMatrixStackUnit();
        xglMatrixStackScale(unit->scale);
        xglMatrixStackTrans(&unit->position.x);
        xglMatrixStackRotX(unit->rotation[0]);
        xglMatrixStackRotY(unit->rotation[1]);
        xglMatrixStackRotZ(unit->rotation[2]);
        xglMatrixStackSave(matrix);
        xglRotTransPers((Vector4 *)&projected, (const float (*)[4])matrix, &origin, camera_id);
        screen_x = projected.x;
        if ((unsigned int)screen_x - 0x7001u >= 0x1fffu ||
            projected.y < 0x7201 || projected.y > 0x8dff ||
            projected.z <= 0)
            continue;
        screen_y = projected.y;
        depth = projected.z;
        color = 0xffffff;
        switch (unit->serial & 0xf000) {
        case 0x5000: color = 0x7f7f7f; break;
        case 0x6000: color = 0xcc66ff; break;
        case 0x3000: color = 0xffff00; break;
        }
        map_format[1] = color >> 16;
        map_format[2] = color >> 8;
        map_format[3] = color;
        xglFontDebugPrintf(((screen_x >> 4) - 0x700) >> 1,
                           ((screen_y >> 4) - 0x720) >> 1,
                           D_004D8A60, index + 0x300, depth);
    }
}

void GameDrawShadow(void)
{
    ActorHead *unit;
    int i;

    unit = actor;
    ACT_DrawShadowBegin();
    for (i = ACTOR_COUNT - 1; i >= 0; i--, unit++) {
        if (unit->inUseId != 0) {
            if (!(unit->flags & 8)) {
                if (unit->flags & 0x20) {
                    ACT_DrawShadow(unit);
                }
            }
        }
    }
    ACT_DrawShadowEnd();
}

void GameDrawSync(void)
{
    VibrationEngine();
    xglStudioMainCameraMove();
    xglStudioMove();
    if (GameLoopState.frame_hook != 0) {
        GameLoopState.frame_hook(3);
    }
    if (GameLoopState.flags & 0x2) {
        if (GameLoopState.clear_callback != 0) {
            GameLoopState.clear_callback();
        } else {
            TestEnv_2_00338620.red = ClearEnv.red;
            TestEnv_2_00338620.green = ClearEnv.green;
            TestEnv_2_00338620.blue = ClearEnv.blue;
            TestEnv_2_00338620.alpha = 0x80;
            sceVif1PkRef(xglPacketGetCurrent(), &TestEnv_2_00338620, 6, 0, 0, 0);
        }
        TWSYS_draw();
        if (GameLoopState.map_draw_callback != 0) {
            GameLoopState.map_draw_callback();
        } else {
            MapDraw();
        }
        if (GameLoopState.actor_draw_callback != 0) {
            GameLoopState.actor_draw_callback();
        } else {
            ACT_draw();
        }
        if (GameLoopState.shadow_draw_callback != 0) {
            GameLoopState.shadow_draw_callback();
        } else {
            GameDrawShadow();
        }
        if (GameLoopState.unit_draw_callback != 0) {
            GameLoopState.unit_draw_callback();
        } else {
            MAP_drawUnit();
        }
        if (!(GameLoopState.flags_wide & 0x4000001)) {
            sefExecEffect();
        }
        GameMovieMain();
        xglSoundStreamMain();
        if (GameLoopState.flags & 0x4) {
            GameDispTag();
        }
    }
    if (GameLoopState.frame_hook != 0) {
        GameLoopState.frame_hook(4);
    }
    GameSnapShotCheck();
    StudioEntrySkip = 1;
    xglSleep();
    nmlModelSetGlobalFogReset();
    StudioEntrySkip = 0;
    if (GameLoopState.frame_countdown > 0) {
        GameLoopState.frame_countdown--;
    }
    if (GameLoopState.frame_hook != 0) {
        GameLoopState.frame_hook(5);
    }
}

static int GameModeEvtTools(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_debug_storage = 0;
                game_mode_kind = game_mode_saved_kind;
            }
        }
    }

    TWSYS_update();
    EvtTools();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

static int GameModePause(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_debug_storage = 0;
                game_mode_kind = game_mode_saved_kind;
                PartyTimePauseEnd();
            }
        }
    }

    TWSYS_update();
    PauseMenu();
    if (game_mode_saved_kind == 2 || game_mode_saved_kind == 3) {
        ACT_pauseUpdate();
    }
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

static int GameModeEvtDebug(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_debug_storage = 0;
                game_mode_kind = game_mode_saved_kind;
            }
        }
    }

    TWSYS_update();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/game", GameModeCfMain);

void checkTalkPoint(void)
{
    ActorHead *talker;
    GameMapUnit *unit;

    if (PadData.pressed & 0x20) {
        if (GameLoopState.status == 0 && xglCdSync() == 0) {
            if (GameLoopState.flags_wide & 0x20400000) {
                GameStateSave();
                MenuFileMain(0);
                GameLoopState.status = 0x20;
                return;
            }
            if (GameLoopState.flags & 0x800000) {
                CallMethod("evsExit");
                GameLoopState.status = 0x20;
                return;
            }
            if (GameLoopState.flags & 0x40000000) {
                if (shopNo == -1) {
                    shopNo = 1;
                }
                GameStateSaveCameraLight();
                MenuShopMain(shopNo);
                GameStateRestoreCameraLight();
                GameLoopState.status = 0x20;
                return;
            }
        }
        talker = checkTalk((ActorHead *)GameLoopState.controlled_actor);
        if (talker != 0) {
            EnemyWorkPostTalk *enemy = &enepc[talker->number];
            enemy->post_talk_flags = 0;
            SCRIPT_execTalkto(talker);
        } else {
            unit = checkItemBox((ActorHead *)GameLoopState.controlled_actor);
            if (unit != 0) {
                unit->item_box_hit = 1;
            }
        }
    }
}

void actSequenceClear(ActorHead *actor)
{
    GameSequenceSlot *sequence = &actSequence[actor->number];

    actor->flags &= ~2;
    actor->motion_speed = 0.033333335f;
    sequence->flags = 0;
    sequence->state_flags = 0;
    sequence->cleared_on_init = 0;
    sequence->handler[3] = 0;
    actor->motion_flags = 8;
    sequence->cleared_on_init_run[1] = 0;
    sequence->cleared_on_init_run[2] = 0;
    sequence->cleared_on_init_run[3] = 0;
    sequence->handler[0] = 0;
    sequence->handler[1] = 0;
    sequence->handler[2] = 0;
    sequence->cleared_on_init_run[0] = 0;
}

void actTalkAfter(SceneObject object)
{
    EnemyWorkPostTalk *enemy = &enepc[object[ACTOR_NUMBER_OFFSET]];

    After_Talk(object);
    enemy->post_talk_flags = GameLoopState.flags & 0x400;
}

static int GameModeCfEvent(void)
{
    int cleared = GameLoopState.flags & -2;
    u8 event_active = GameLoopState.status;

    GameLoopState.flags = cleared;
    if (event_active == 0 && (PadData.pressed & 0x800) != 0 &&
        (PadData.held & 0x10c) == 268) {
        GameLoopState.flags = cleared | 0x80000000;
        return 1;
    }

    TWSYS_update();
    JTHREAD_cntl();
    PLAY_ctrl();
    TCAMERA_update();
    ACT_update();
    MAP_updateUnit();
    return 0;
}

int GameModeDebugMenu(void)
{
    if ((PadData.debug_buttons & 0x100) != 0) {
        GameLoopState.debug_menu.storage = 0;
        GameLoopState.flags &= 0xfffffffeu;
        GameLoopState.kind = GameLoopState.saved_kind;
    }

    TWSYS_update();
    ACT_pauseUpdate();
    GameDebugMenu();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && GameLoopState.camera_callback != 0) {
            GameLoopState.camera_callback();
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/game", Game);

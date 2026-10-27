#ifndef INCLUDE_MAIN_LOOK_AT_PLAYER_H
#define INCLUDE_MAIN_LOOK_AT_PLAYER_H

#include "shared.h"

typedef struct ActorUndulation {
    u8 unmodeled_00[8];
    s16 attr_mask;
} ActorUndulation;

typedef struct PlayerEffect PlayerEffect;

typedef struct LookAtPlayerActor {
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
    u8 signal;
    u8 unmodeled_82[0x88 - 0x82];
    s16 frame_counter;
    u8 unmodeled_8a[0x124 - 0x8a];
    enum { LOCATOR_FLAGS_PLAYER_INIT = 0x70000 } locator_flags;
    u8 unmodeled_128[0x4c0 - 0x128];
    void *java_object;
    u8 unmodeled_4c4[0x4c8 - 0x4c4];
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
} LookAtPlayerActor;

void GameCfPlayerLoadResource(int use_cache);

#endif /* INCLUDE_MAIN_LOOK_AT_PLAYER_H */

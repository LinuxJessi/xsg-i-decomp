#ifndef INCLUDE_MAIN_ENEMY_1_H
#define INCLUDE_MAIN_ENEMY_1_H

#include "shared.h"

typedef union EnemyVector4 {
    Vector4 vector;
    unsigned long long words[2];
} EnemyVector4;

typedef union EnemyWorkState40 {
    short alert_level;
    short spline_parameter;
} EnemyWorkState40;

typedef struct EnemyEscapeTarget {
    union {
        Vector4 position;
        EnemyVector4 position_pair;
    };
    float scale;
    float scale_from;
    float scale_to;
    short scale_frame;
    short scale_frame_count;
} EnemyEscapeTarget;

typedef struct EnemyWorkEntry {
    float locator_distance;
    short initial_motion_id;
    short locator_start_frame;
    short locator_end_frame;
    short wall_attribute;
    float value_0c;
    float goback_x;
    unsigned char unmodeled_014[4];
    float goback_z;
    unsigned char unmodeled_01c[4];
    Vector4 step_limit;
    float drift_x;
    float fall_speed;
    float drift_z;
    unsigned char unmodeled_03c[4];
    EnemyWorkState40 state_40;
    short fast_point_count;
    unsigned short hit_count;
    short hit_limit;
    signed char action;
    signed char move_result;
    unsigned char pause_kind;
    unsigned char unmodeled_04b[1];
    unsigned short value_4c;
    unsigned short stiff_timer;
    short effect_timer[2];
    signed char route_marks[2][8];
    short route_cursor[2];
    unsigned char unmodeled_068[2];
    unsigned char stiff_flag;
    unsigned char unmodeled_06b;
    short chase_frame_limit;
    short chase_frame;
    int map_index;
    short mark;
    short mark_timer;
    unsigned char unmodeled_078[8];
    Vector4 spline_points[256];
    short spline_point_count;
    unsigned char unmodeled_1082[0xe];
    Vector4 route_points[256];
    short action_motion_table[256][4];
    short motion_id_table[256][4];
    short unique_pattern;
    unsigned short unique_step;
    short route_point_count;
    unsigned char unmodeled_3096[0x70a];
    short homing_count;
    signed char freeze_state;
    signed char turn_lock;
    short target_number;
    unsigned char unmodeled_37a6[0x12];
    int event_type;
    unsigned char unmodeled_37bc[4];
    EnemyEscapeTarget escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0xc];
    EnemyVector4 last_player_position;
    unsigned char unmodeled_3800[0x30];
    short encount_lock;
    unsigned char unmodeled_3832[0x22];
    float stiff_saved_speed;
    float value_3858;
    unsigned char unmodeled_385c[0x44];
    void *stiff_effect;
    unsigned char unmodeled_38a4[4];
    short applied_light_level;
    unsigned char unmodeled_38aa[2];
    short sense_block_timer;
    unsigned char unmodeled_38ae[2];
} EnemyWorkEntry;

#endif /* INCLUDE_MAIN_ENEMY_1_H */

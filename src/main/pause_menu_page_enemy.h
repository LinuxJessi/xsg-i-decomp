#ifndef PAUSE_MENU_PAGE_ENEMY_H
#define PAUSE_MENU_PAGE_ENEMY_H

extern const char D_004CB160[], D_004CB180[], D_004CB1A0[];
extern const char D_004CB338[], D_004CB378[], D_004CB390[];
extern const char D_004CB3B0[], D_004CB3E0[], D_004CB4C8[];
extern const char D_004CB890[], D_004CB8E8[], D_004CB908[];
extern const char D_004CB990[], D_004CB9F0[];

#include "main/enemy_1.h"

#include "main/xgl_2.h"
#include "main/xgl_studio.h"

extern short Times;
extern float D_004D8160;

void xglLightCalcMatrix(StudioLight *light);
void nmlModelSetLight(Matrix4 colorMatrix, Matrix4 normalMatrix);
void nmlModelSetPlace(Matrix4 matrix);
void nmlModelEntry(int entry);

typedef struct EnemyParameters {
    float route_step_length;
    float chase_distance;
    float move_speed;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    short stiff_duration;
    short pause_interval;
    short threat_duration;
    short threat_interval;
    short turn_duration;
    short gravity_mode;
    unsigned char unmodeled_24[0x50 - 0x24];
    short sound_effects[8];
    unsigned char unmodeled_60[4];
    int locator_flags;
    float hearing_distance;
    short unmodeled_6c;
    short light_level;
} EnemyParameters;

typedef struct Actor {
    unsigned int flags;
    void (*update)(struct Actor *actor);
    void (*draw)(struct Actor *actor);
    unsigned char unmodeled_0c[4];
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    Vector4 global_position;
    unsigned char number;
    unsigned char unmodeled_81;
    unsigned char kind;
    unsigned char unmodeled_83[3];
    short in_use_id;
    unsigned char unmodeled_88[0xc0 - 0x88];
    EnemyParameters parameters;
    unsigned char unmodeled_130[0x704 - 0x130];
    unsigned short motion_number;
    unsigned char unmodeled_706[0x9e4 - 0x706];
    float target_angle;
    float body_radius;
    short look_at_state;
    short look_at_target;
    unsigned char unmodeled_9f0[0xa70 - 0x9f0];
} Actor;

typedef struct EnemyScale {
    float current;
    float start;
    float target;
    short frame;
    short duration;
} EnemyScale;

typedef struct EnemyWork {
    float tentacle_range;
    short tentacle_duration;
    short locator_start_frame;
    short locator_end_frame;
    short territory_index;
    float height;
    Vector4 goback_position;
    Vector4 step_limit;
    Vector4 stop_cause;
    short spline_parameter;
    short fast_point_count;
    short hit_count;
    short hit_limit;
    signed char action;
    unsigned char unmodeled_49;
    unsigned char pause_kind;
    unsigned char hidden_unit;
    short turn_count;
    short explosion_count;
    short electric_count;
    short fall_count;
    signed char page_status[2][8];
    short page_count[2];
    unsigned char unmodeled_68[4];
    short chase_frame_limit;
    short chase_frame;
    unsigned int map_index;
    short mark;
    short mark_timer;
    unsigned char unmodeled_78[8];
    float spline_coordinates[256 * 4];
    short spline_point_count;
    unsigned char unmodeled_1082[0x2090 - 0x1082];
    short motion_tables[2][256][4];
    short route_index;
    short action_index;
    unsigned char unmodeled_3094[0x30a0 - 0x3094];
    float route_coordinates[64 * 4];
    short route_values[64][4];
    short route_metadata[2 * 64];
    short route_point_count;
    signed char freeze_state;
    signed char turn_lock;
    short target_number;
    unsigned char unmodeled_37a6[2];
    unsigned int previous_motion;
    unsigned char unmodeled_37ac[0x37b8 - 0x37ac];
    unsigned int entry_id;
    unsigned char unmodeled_37bc[4];
    Vector4 target_position;
    EnemyScale scale;
    int state_flags;
    unsigned char unmodeled_37e4[0x37f0 - 0x37e4];
    /* Enemy_Chase copies target_position or a collision-result position here
     * and passes the complete position to TM_Enemy_Move_Step. */
    EnemyVector4 final_target;
    unsigned char unmodeled_3800[0x3858 - 0x3800];
    float freeze_saved_speed;
    unsigned char unmodeled_385c[0x3890 - 0x385c];
    float sight_position[3];
    unsigned char unmodeled_389c[0x38a8 - 0x389c];
    short applied_light_level;
    short light;
    unsigned char unmodeled_38ac[4];
} EnemyWork;

typedef struct GameLoopStateRecord {
    unsigned char unmodeled_00000[0x29fa0];
    signed char debug_directions[16];
    unsigned char debug_enabled[16];
    float debug_values[16];
    unsigned char unmodeled_2a000[0x30];
} GameLoopStateRecord;

typedef struct LayoutHeader {
    float components[4];
} LayoutHeader;

typedef struct UnduParam {
    int queryFlags;
    unsigned char unmodeled_04[4];
    short attrMask;
    unsigned char unmodeled_0a[0x18 - 0x0a];
    struct LayoutHeader *header;
    unsigned char unmodeled_1c[4];
    long long attribute;
} UnduParam;

extern Actor actor[64];

extern EnemyWork enepc[16];

extern short DebugActor;

extern short Page_0;

extern short SelNum;

extern signed char FLAG_FRAME_60;

extern PadPrefix PadData;

extern UnduParam UnduTemp;

extern unsigned long long EneTerritoryID[];

int Get_Attr(const Vector4 *position, int map_index, int mask);

LayoutHeader *UnduDataGetHeader(int map_index, int unit_index);

void UnduParamInit(UnduParam *param);

float UnduCheck(const Vector4 *position, void *exclude, UnduParam *param);

char Get_LocaterType(short index);

void Vibration_Set_Strong(int start, int end, int duration);

void xglFontDebugPrintf(int x, int y, const char *format, ...);

extern const char D_004CAEF0[];

extern const char D_004CAF08[];

extern const char D_004CAF18[];

extern const char D_004CAF28[];

extern const char D_004CAF38[];

extern const char D_004CAF50[];

extern const char D_004CAF60[];

extern const char D_004CAF70[];

extern const char D_004CAF80[];

extern const char D_004CAF90[];

extern const char D_004CAFA0[];

extern const char D_004CAFF8[];

extern const char D_004CB008[];

extern const char D_004CB018[];

extern const char D_004CB028[];

extern const char D_004CB038[];

extern const char D_004CB0C8[];

extern const char D_004CB0E0[];

extern const char D_004CB0F0[];

extern const char D_004CB120[];

extern const char D_004CB130[];

extern const char D_004DB868[];

extern const char D_004DB870[];

extern const char D_004DB878[];

extern const char D_004DB880[];

extern const char D_004DB888[];

extern const char D_004DB890[];

extern const char D_004DB898[];

extern const char D_004DB8A0[];

extern const char D_004DB8A8[];

extern const char D_004DB8B0[];

extern const char D_004DB8B8[];

extern const char D_004DB8C0[];

extern const char D_004DB8C8[];

extern const char D_004DB8D0[];

extern const char D_004DB8D8[];

extern const char D_004DB8E0[];

extern const char D_004DB8E8[];

extern const char D_004DB8F0[];

extern const char D_004DB8F8[];

extern const char D_004DB900[];

extern const char D_004DB908[];

extern const char D_004DB910[];

extern const char D_004DB918[];

extern const char D_004DB920[];

extern const char D_004DB928[];

extern const char D_004DB930[];

extern const char D_004DB938[];

extern const char D_004DB940[];

extern const char D_004DB948[];

extern const char D_004DB958[];

extern const char D_004CB1C0[];

extern const char D_004CB1D0[];

extern const char D_004CB1E0[];

extern const char D_004CB1F8[];

extern const char D_004CB210[];

extern const char D_004CB228[];

extern const char D_004CB240[];

extern const char D_004CB258[];

extern const char D_004CB278[];

extern const char D_004CB298[];

extern const char D_004CB2B8[];

extern const char D_004CB2D8[];

extern const char D_004CB2F8[];

extern const char D_004CB318[];

extern const char D_004CB358[];

extern const char D_004DB968[];

extern const char D_004CB368[];

extern const char D_004CB3C8[];

extern const char D_004CB400[];

extern const char D_004CB420[];

extern const char D_004CB440[];

extern const char D_004CB460[];

extern const char D_004CB480[];

extern const char D_004CB498[];

extern const char D_004CB4B0[];

extern const char D_004CB4F0[];

extern const char D_004CB508[];

extern const char D_004CB518[];

extern const char D_004CB538[];

extern const char D_004CB558[];

extern const char D_004CB578[];

extern const char D_004CB598[];

extern const char D_004CB5B8[];

extern const char D_004CB5D8[];

extern const char D_004CB5F8[];

extern const char D_004CB618[];

extern const char D_004CB628[];

extern const char D_004CB640[];

extern const char D_004CB658[];

extern const char D_004CB670[];

extern const char D_004CB690[];

extern const char D_004CB6A8[];

extern const char D_004CB6C8[];

extern const char D_004CB6E8[];

extern const char D_004CB708[];

extern const char D_004CB718[];

extern const char D_004CB730[];

extern const char D_004CB748[];

extern const char D_004CB760[];

extern const char D_004CB778[];

extern const char D_004CB790[];

extern const char D_004CB7A8[];

extern const char D_004CB7C0[];

extern const char D_004CB7E0[];

extern const char D_004CB7F8[];

extern const char D_004CB810[];

extern const char D_004CB838[];

extern const char D_004CB858[];

extern const char D_004CB868[];

extern const char D_004CB880[];

extern const char D_004CB8A8[];

extern const char D_004CB8C0[];

extern const char D_004CB8D8[];

extern const char D_004CB8F8[];

extern const char D_004CB920[];

extern const char D_004CB938[];

extern const char D_004CB950[];

extern const char D_004CB978[];

extern const char D_004CB9A8[];

extern const char D_004CB9C8[];

extern const char D_004CBA18[];

extern const char D_004CBA30[];

extern const char D_004CBA48[];

extern const char D_004CBA78[];

extern const char D_004DB978[];

extern const char D_004DB980[];

extern const char D_004DB988[];

extern const char D_004DB998[];

extern const char D_004DB9A0[];

extern GameLoopStateRecord GameLoopState;


#endif

#ifndef PAUSE_MENU_PAGE_ENEMY_H
#define PAUSE_MENU_PAGE_ENEMY_H

#define D_004CB160 ("Welcome to the 60-Frame world!")
#define D_004CB180 ("     30-Frame Debug Mode      ")
#define D_004CB1A0 ("\241\341\241\241Page %3d[%2d:%3d]\241\241\241\341")
#define D_004CB338 ("\241\241\260\334\306\260\243\311\243\304 [%3d] = &H%8x")
#define D_004CB378 ("\245\277\245\244\245\327\241\241\241\241%2d:%s")
#define D_004CB390 ("\245\342\241\274\245\311\241\241\241\241%2d:%s (%3d/%3d)")
#define D_004CB3B0 ("\245\325\245\352\241\274\245\272\241\241%2d:%s")
#define D_004CB3E0 ("\261\243\244\354\244\306\244\244\244\353\245\346\245\313\245\303\245\310\310\326\271\346\241\241[%5d]")
#define D_004CB4C8 ("\277\310\304\271\241\241\241\241[%8f]\241\241\273\353\300\376\241\277\301\352\274\352 [%2d,%2d]")
#define D_004CB890 ("%2d:(%8f,%8f)%8f%s\n")
#define D_004CB8E8 ("\244\275\244\316 %1d = %3d\n")
#define D_004CB908 ("\274\357\316\340\241\241\241\241\241\241%2d:%s")
#define D_004CB990 ("%3d:(%8f,%8f,%8f)")
#define D_004CB9F0 ("%3d[%1d]:Motion = %3d / Frame = %3d")

#include "main/enemy_1.h"

#include "main/xgl_2.h"
#include "main/xgl_studio.h"

extern short Times;
#define D_004D8160 (6.2831854820251465f)

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

#define D_004CAFF8 ("\245\316\241\274\245\336\245\353")

#define D_004CB008 ("\245\353\241\274\245\310\277\250\274\352")

#define D_004CB018 ("\261\375\311\374\277\250\274\352")

#define D_004CB028 ("\277\250\274\352\314\347\310\326")

#define D_004CB038 ("\245\353\241\274\245\310\304\311\300\327")

#define D_004CB0C8 ("\245\242\245\353\245\264\245\352\245\272\245\340\304\344\273\337")

#define D_004CB0E0 ("\264\260\301\264\304\344\273\337")

#define D_004CB0F0 ("\264\260\301\264\304\344\273\337\241\312\243\303\243\310\243\322\267\277\273\310\315\321\262\304\307\275\241\313")

#define D_004CB120 ("\244\323\244\303\244\257\244\352\245\336\241\274\245\257")

#define D_004CB130 ("\244\317\244\306\244\312\245\336\241\274\245\257")

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

#define D_004DB940 ("\244\244\244\244\244\250")

#define D_004DB948 ("\244\317\244\244")

#define D_004DB958 ("\304\276\300\376")

#define D_004CB1C0 ("\264\351\244\310\273\327\244\357\244\354\244\353")

#define D_004CB1D0 ("\241\371\260\334\306\260\241\371")

#define D_004CB1E0 ("\270\275\272\337\303\317(%8f,%8f,%8f)")

#define D_004CB1F8 ("\315\275\304\352\316\314(%8f,%8f,%8f)")

#define D_004CB210 ("\314\334\311\270\305\300(%8f,%8f,%8f)")

#define D_004CB228 ("\264\267\300\255\241\241(%8f,%8f,%8f)")

#define D_004CB240 ("\262\363\305\276 [%8f] -> [%8f]")

#define D_004CB258 ("\260\334\306\260\302\256\305\331[%8f] \260\334\306\260\265\367\316\245[%8f]")

#define D_004CB278 ("\272\367\305\250\265\367\316\245[%8f] \272\367\305\250\263\321\305\331[%8f]")

#define D_004CB298 ("\304\311\300\327\302\256\305\331[%8f] \241\241\262\273\265\367\316\245[%8f]")

#define D_004CB2B8 ("\304\374\244\341\265\367\316\245[%8f] \304\374\244\341\273\376\264\326[%5d]")

#define D_004CB2D8 ("\272\307\275\252\314\334\311\270[%8f] \245\265\245\244\245\272\241\241[%8f]")

#define D_004CB2F8 ("\245\250\245\352\245\242\243\311\243\304 [%3d] = &H%8x")

#define D_004CB318 ("\241\241\241\241\245\242\245\310\245\352 [%3d] = &H%8x")

#define D_004CB358 ("\245\250\245\315\245\337\241\274")

#define D_004DB968 ("\243\316\243\320\243\303")

#define D_004CB368 ("\241\371\276\365\302\326\241\371")

#define D_004CB3C8 ("\304\344\273\337\270\266\260\370\241\241(%8f,%8f,%8f)")

#define D_004CB400 ("      \242\247\245\325\245\354\241\274\245\340\277\364\241\241\242\247\311\321\305\331")

#define D_004CB420 ("\271\305\304\276\241\241\241\241[%4d]\241\241\241\241\241\241[%4d]")

#define D_004CB440 ("\260\322\263\305\241\241\241\241[%4d]\241\241\241\241\241\241[%4d]")

#define D_004CB460 ("\262\363\305\276\241\241\241\241     \241\241\241\241\241\241[%4d]")

#define D_004CB480 ("\245\253\245\246\245\363\245\277\241\247\307\372\310\257\241\241%5d")

#define D_004CB498 ("\241\241\241\241\241\241\241\241\241\241\305\305\267\342\241\241%5d")

#define D_004CB4B0 ("\241\241\241\241\241\241\241\241\241\241\315\356\262\274\241\241%5d")

#define D_004CB4F0 ("\273\353\300\376\241\241(%8f,%8f,%8f)")

#define D_004CB508 ("\241\371\245\325\245\351\245\260\241\371")

#define D_004CB518 ("\245\342\241\274\245\267\245\347\245\363\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB538 ("\262\363\305\276\241\241\241\241\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB558 ("\274\363\277\266\244\352\241\241\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB578 ("\245\255\245\343\245\363\245\273\245\353\262\304\241\241\241\241\241\241%2d:%s")

#define D_004CB598 ("\245\325\245\352\241\274\245\272\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB5B8 ("\243\312\243\301\243\326\243\301\245\310\241\274\245\257\241\241\241\241%2d:%s")

#define D_004CB5D8 ("\241\241\241\241\241\241\241\241\245\277\245\303\245\301\241\241\241\241%2d:%s")

#define D_004CB5F8 ("\241\241\241\241\241\241\241\241\245\355\245\261\241\274\245\277\241\241%2d:%s")

#define D_004CB618 ("\241\371\277\250\274\352\241\371")

#define D_004CB628 ("\277\250\274\352\274\315\304\370\241\241\241\241\241\241[%8f]")

#define D_004CB640 ("\277\250\274\352\264\374\264\326\241\241\241\241\241\241[%8d]")

#define D_004CB658 ("\310\275\304\352\263\253\273\317\241\301\275\252\316\273[%8d-%8d]")

#define D_004CB670 ("\241\371\245\271\245\261\241\274\245\353\241\277\310\276\306\251\314\300\241\277\244\275\244\316\302\276\241\371")

#define D_004CB690 ("\245\312\245\246\244\312\245\271\245\261\241\274\245\353\241\241%8f")

#define D_004CB6A8 ("\245\253\245\246\245\363\245\277\241\241\241\241\241\241\241\241%3d/ %3d")

#define D_004CB6C8 ("\245\271\245\261\241\274\245\353\277\344\260\334\241\241\241\241%8f->%8f")

#define D_004CB6E8 ("\306\251\314\300\305\331\241\241\241\241\241\241\241\241\241\241%8d->%8d")

#define D_004CB708 ("\241\371\306\303\304\247\241\371")

#define D_004CB718 ("\245\250\245\363\245\253\245\246\245\363\245\310\241\241%2d:%s")

#define D_004CB730 ("\245\263\245\363\245\331\245\242\241\241\241\241\241\241%2d:%s")

#define D_004CB748 ("\263\260\275\320\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB760 ("\260\334\306\260\245\277\245\244\245\327\241\241\241\241%2d:%s")

#define D_004CB778 ("\310\364\271\324\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB790 ("\307\313\262\365\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB7A8 ("\312\321\267\301\241\241\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB7C0 ("\245\257\245\352\245\303\245\324\245\363\245\260\241\241%2d:%s(%8f)")

#define D_004CB7E0 ("\243\311\243\304\314\265\270\372\241\241\241\241\241\241%2d:%s")

#define D_004CB7F8 ("\262\363\305\276\266\330\273\337\241\241\241\241\241\241%2d:%s")

#define D_004CB810 ("\245\342\241\274\245\267\245\347\245\363\241\241\241\241%2d -> \260\354\300\316\301\260\241\247%2d")

#define D_004CB838 ("\304\311\300\327\302\320\276\335[%2d(%6f,%6f,%6f)]")

#define D_004CB858 ("\245\325\245\351\245\260\241\241\241\241\241\241\241\241:")

#define D_004CB868 ("\265\242\301\343\303\346\241\241\241\241\241\241\241\241%2d:%s")

#define D_004CB880 ("\241\371\245\355\245\261\241\274\245\277\241\371")

#define D_004CB8A8 ("\241\371\245\317\245\267\245\264\244\316\310\353\314\251\241\371")

#define D_004CB8C0 ("%2d:%s[%8f m]%s\n")

#define D_004CB8D8 ("\241\371\244\260\244\353\241\371")

#define D_004CB8F8 ("\241\371\245\265\245\306\245\351\245\244\245\310\241\371")

#define D_004CB920 ("\245\253\245\246\245\363\245\277\241\241[%5d/%5d]")

#define D_004CB938 ("\241\371\245\271\245\327\245\351\245\244\245\363\243\261\241\371")

#define D_004CB950 ("\245\253\245\246\245\363\245\277\241\241[%5d/%5d]\241\241\303\346\267\321\305\300\277\364 [%5d]")

#define D_004CB978 ("\315\275\310\367\306\260\272\356\264\374\264\326\241\241\241\241%8d")

#define D_004CB9A8 ("\241\371\245\271\245\327\245\351\245\244\245\363\243\262\241\277\272\243\270\345\244\316\315\275\304\352\241\371")

#define D_004CB9C8 ("\245\353\241\274\245\310\241\241\241\241[%5d]    \241\241\271\324\306\260\275\347\275\370 [%5d]")

#define D_004CBA18 ("\241\371\245\244\245\331\245\363\245\310\241\312\245\335\241\274\245\272\241\313\241\371")

#define D_004CBA30 ("\241\371\245\244\245\331\245\363\245\310\241\312\260\322\263\305\241\313\241\371")

#define D_004CBA48 ("\241\371\265\242\244\352\306\273\300\337\304\352\245\304\241\274\245\353\241\326\244\252\265\242\244\352\244\317\244\263\244\301\244\351\241\327Ver0.1\241\371")

#define D_004CBA78 ("%2d:(%4f,%4f)")

#define D_004DB978 ("%3d[%s]")

#define D_004DB980 ("%2d:")

#define D_004DB988 ("--")

extern const char D_004DB998[];

extern const char D_004DB9A0[];

extern GameLoopStateRecord GameLoopState;


#endif

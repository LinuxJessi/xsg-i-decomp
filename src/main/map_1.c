#include "common.h"

#include "shared.h"

/* Partial view of the resource record RES_loadFile hands back for a map:
 * MapReset (0x00246598, still INCLUDE_ASM) dereferences it at +0x04
 * (xglCullingMapSet argument), +0x0c (UnduInit argument) and +0x10 (second
 * argument to the still-unrecovered func_A1CFB8). Only those three offsets
 * are evidenced; the rest stays unmodeled. */

typedef struct {
    const Vector4 *place;
    int culling_map;
    const char *texture;
    unsigned char *undu_state;
    const void *think_data;
} MapResource;

typedef struct MapPlayerActorState MapPlayerActorState;

typedef struct {
    u8 unmodeled_00[4];
    MapPlayerActorState *player_actor;
    u8 unmodeled_08[4];
    u16 map_draw_mode;
    u8 unmodeled_0e[2];
    u32 render_flags;
    u8 unmodeled_14[0x3c];
    short map_no;
    short entrance_no;
    MapResource *map_resource;
    u8 unmodeled_58[0x28];
    float render_color[4];
    u8 unmodeled_90[0x20];
    int render_level;
    int clip_mode;
    int fade_countdown;
    u8 unmodeled_bc[5];
    u8 map_flags;
} MapGameState;

typedef struct {
    u8 unmodeled_00[0x10];
    u8 state;
    u8 unmodeled_11[3];
} MapTaskState;

typedef void (*MapTaskCallback)(void *task);

typedef struct {
    u8 unmodeled_00[0x14];
    u16 packet_offset_units;
    u8 unmodeled_16[0x1a];
    MapTaskCallback callback;
} MapRenderState;

extern MapGameState GameLoopState;

extern u8 UseTestPath;

extern void MapChangeFadeSet(void);

extern void MapChange2(int map_id);

extern MapResource *RES_loadFile(int file_id, int resource_type, int map_id, int flags);

extern void MapReset(void);

extern XglPacket *xglPacketGetCurrent(void);

extern void xglRenderDrawFlipPk(XglPacket *packet);

extern void nmlModelSetFadeInCancel(int model_id);

extern void nmlModelSetFadeOutCancel(int model_id);

const char D_004BE308[16] = "data\\map\\";

const char D_004BE318[16] = "data\\map\\test\\";

const char D_004BE328[16] = "data\\map\\check\\";

static void taskMapChange(void *task);

extern MapTaskState tsk;

extern MapRenderState sRender;

struct MapPlayerActorState {
    u8 unmodeled_00[0x10];
    float position_x;
    float position_y;
    float position_z;
    u8 unmodeled_1c[0x38];
    float entrance_rotation;
    u8 unmodeled_58[0x98c];
    float entrance_rotation_copy;
};

typedef struct {
    MapResource *resource;
    int size;
    int handle;
    int state;
} GameResourceEntry;

typedef struct {
    u8 unmodeled_00[0x20];
    u64 task_state_word;
    u8 unmodeled_28[0x28];
    u64 vif_command_word;
    u8 unmodeled_58[0x58];
} MapTestPrimData;

typedef struct {
    float components[4];
} MapEntranceHeader;

typedef struct {
    char **names;
    int count;
} MapNameGroup;

typedef struct {
    Vector4 color;
    Vector4 direction;
} MapDrawLightEntry;

typedef struct {
    Vector4 ambient_color;
    MapDrawLightEntry lights[3];
    float normal_matrix[4][4];
    float color_modifier_matrix[4][4];
} MapDrawLight;

extern GameResourceEntry GameResource[];

extern MapNameGroup maptbl[];

extern MapEntranceHeader *UnduDataGetHeader(int map_index, int unit_index);

extern void UnduInit(unsigned char *data);

extern int func_A1CFB8(int think_no, const void *data);

extern void nmlModelSetMapLastInit(void);

extern void xglCullingMapSet(int culling_map);

extern MapDrawLight *xglStudioGetLight2(void);

extern void xglLightCalcMatrix(MapDrawLight *light);

extern void nmlModelSetLight(void *color_modifier, void *normal);

extern void nmlModelSetTexture(const char *texture);

extern void nmlModelSetMapEntry(void);

extern void nmlModelSetMulColor(const float color[4]);

extern void nmlModelSetRenderLevel(int level);

extern void nmlModelSetPlace(const Vector4 *place);

extern void nmlModelSetClip(int clip);

extern void nmlModelEntry(int entry);

extern void sceVif1PkCnt(NmlPacket packet, int count);

extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);

extern void nmlModelSendBackBufferSignal(void);



const char *MAP_getPath(void)
{
    int test_path = UseTestPath;

    if (test_path != 1) {
        if (test_path < 2) {
            return D_004BE308;
        }
        if (test_path != 2) {
            return D_004BE308;
        }
        return D_004BE328;
    }
    return D_004BE318;
}

static void taskMapChange(void *task)
{
    static MapTestPrimData TestPrim = { .unmodeled_00 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 81, 1, 128, 0, 0, 0, 64, 171, 144, 238, 238, 49, 53, 5, 0, 0, 0 }, .task_state_word = 274877907044ULL, .unmodeled_28 = { 66, 0, 0, 0, 0, 0, 0, 0, 5, 192, 127, 0, 252, 6, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 1, 48, 7, 0, 0, 0, 0, 0, 71, 0, 0, 0, 0, 0, 0, 0 }, .vif_command_word = 0ULL, .unmodeled_58 = { 6, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 111, 0, 0, 247, 113, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 32, 0, 0, 0, 28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 143, 0, 0, 247, 141, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0 } };

    int command_low = 0x24120000 | ((u32)sRender.packet_offset_units << 5);

    TestPrim.task_state_word = ((u64)tsk.state << 32) | 100;
    TestPrim.vif_command_word = ((u64)0xc800 << 19) | command_low;
    sceVif1PkCnt(task, 0);
    sceVif1PkAddDataN(task, &TestPrim, 0x2c);
    nmlModelSendBackBufferSignal();
    tsk.state -= 4;
    if (tsk.state == 0) {
        sRender.callback = 0;
    }
}

void MapChangeFadeSet(void)
{
    if (GameLoopState.fade_countdown <= 0) {
        xglRenderDrawFlipPk(xglPacketGetCurrent());
        tsk.state = 0x80;
        sRender.callback = taskMapChange;
        nmlModelSetFadeInCancel(5);
        nmlModelSetFadeOutCancel(5);
    }
}

void MapReset(void)
{
    UnduInit(GameLoopState.map_resource->undu_state);
    GameLoopState.map_flags |= 0x80;
    GameLoopState.render_flags |= 0x30;
    func_A1CFB8(GameLoopState.map_no, GameLoopState.map_resource->think_data);
    nmlModelSetMapLastInit();
    xglCullingMapSet(GameLoopState.map_resource->culling_map);
    GameLoopState.render_level = 0;
    GameLoopState.clip_mode = 0;
    GameLoopState.render_color[3] = 1.0f;
    GameLoopState.render_color[2] = 1.0f;
    GameLoopState.render_color[1] = 1.0f;
    GameLoopState.render_color[0] = 1.0f;
}

void MapChangeResource(int map_id, int entrance_id)
{
    if (GameResource[map_id].state == 1) {
        GameLoopState.map_no = GameResource[map_id].handle;
        GameLoopState.map_resource = GameResource[map_id].resource;
        if (map_id < 1000) {
            MapChangeFadeSet();
        }
        MapReset();
        GameLoopState.entrance_no = (short)entrance_id;
        {
            MapEntranceHeader *header = UnduDataGetHeader(0x201, entrance_id + 0x8000);

            if (header != 0) {
                float last_component;

                GameLoopState.player_actor->position_x = header->components[0];
                GameLoopState.player_actor->position_y = header->components[1];
                GameLoopState.player_actor->position_z = header->components[2];
                last_component = header->components[3];
                GameLoopState.player_actor->entrance_rotation = last_component;
                GameLoopState.player_actor->entrance_rotation_copy = last_component;
            }
        }
    }
}

void MapChange(int map_id)
{
    MapChangeFadeSet();
    MapChange2(map_id);
}

void MapChange2(int map_id)
{
    if ((unsigned int)map_id < 0x71e) {
        GameLoopState.map_resource = RES_loadFile(-1, 1, map_id, 0);
        GameLoopState.map_no = map_id;
        MapReset();
    }
}

char *MapGetName(int map_no)
{
    int table_index;
    int entry_count;
    char *map_name;

    if (map_no < 0) {
        map_no = GameLoopState.map_no;
    }
    map_name = 0;
    if (map_no < 0x71F) {
        table_index = map_no / 100;
        entry_count = maptbl[table_index].count;
        if ((map_no % 100) < entry_count) {
            map_name = maptbl[table_index].names[map_no % 100];
        }
    }
    return map_name;
}

void MapDraw(void)
{
    MapResource *resource = GameLoopState.map_resource;

    if (GameLoopState.map_draw_mode == 2 && (GameLoopState.render_flags & 0x20) != 0) {
        if ((GameLoopState.render_flags & 0x10) != 0) {
            GameLoopState.render_flags &= ~0x10;
        } else {
            GameLoopState.render_flags &= ~0x20;
        }
    }

    if (resource != 0 && resource->culling_map != 0) {
        MapDrawLight *light = xglStudioGetLight2();

        xglLightCalcMatrix(light);
        nmlModelSetLight(light->color_modifier_matrix, light->normal_matrix);
        if (resource->texture != 0) {
            nmlModelSetTexture(resource->texture);
        }
        nmlModelSetMapEntry();
        nmlModelSetMulColor(GameLoopState.render_color);
        nmlModelSetRenderLevel(GameLoopState.render_level);
        nmlModelSetPlace(resource->place);
        if (GameLoopState.clip_mode != 0) {
            nmlModelSetClip(1);
        } else {
            nmlModelSetClip(2);
        }
        nmlModelEntry(resource->culling_map);
    }
}

int MapGetNo(void)
{
    return GameLoopState.map_no;
}

void MapInit(void)
{
    UseTestPath = 0;
}



const char D_004BE338[16] = "MC_VOK12B";

const char D_004BE348[16] = "MC_VOK14B";

const char D_004BE358[16] = "MC_VOK13B";

const char D_004BE368[16] = "MC_VOK10B";

const char D_004BE378[16] = "MC_VOK09B";

const char D_004BE388[16] = "MC_VOK08B";

const char D_004BE398[16] = "MC_VOK07B";

const char D_004BE3A8[16] = "MC_VOK06B";

const char D_004BE3B8[16] = "MC_VOK05B";

const char D_004BE3C8[16] = "MC_VOK04B";

const char D_004BE3D8[16] = "MC_VOK03B";

const char D_004BE3E8[16] = "MC_ELS01";

const char D_004BE3F8[16] = "MC_VOK26";

const char D_004BE408[16] = "MC_VOK25";

const char D_004BE418[16] = "MC_VOK24";

const char D_004BE428[16] = "MC_VOK23";

const char D_004BE438[16] = "MC_VOK22";

const char D_004BE448[16] = "MC_VOK21";

const char D_004BE458[16] = "MC_VOK20";

const char D_004BE468[16] = "MC_VOK19";

const char D_004BE478[16] = "MC_VOK18";

const char D_004BE488[16] = "MC_VOK16";

const char D_004BE498[16] = "MC_VOK15";

const char D_004BE4A8[16] = "MC_VOK14";

const char D_004BE4B8[16] = "MC_VOK13";

const char D_004BE4C8[16] = "MC_VOK12";

const char D_004BE4D8[16] = "MC_VOK11";

const char D_004BE4E8[16] = "MC_VOK10";

const char D_004BE4F8[16] = "MC_VOK09";

const char D_004BE508[16] = "MC_VOK08";

const char D_004BE518[16] = "MC_VOK07";

const char D_004BE528[16] = "MC_VOK06";

const char D_004BE538[16] = "MC_VOK05";

const char D_004BE548[16] = "MC_VOK04";

const char D_004BE558[16] = "MC_VOK03";

const char D_004BE568[16] = "MC_VOK02";

const char D_004BE578[16] = "MC_VOK01";

const char D_004BE588[16] = "MC_BTL01";

const char D_004BE598[16] = "MC_TST13";

const char D_004BE5A8[16] = "MC_TST12";

const char D_004BE5B8[16] = "MC_TST11";

const char D_004BE5C8[16] = "MC_TST10";

const char D_004BE5D8[16] = "MC_TST09";

const char D_004BE5E8[16] = "MC_TST08";

const char D_004BE5F8[16] = "MC_TST07";

const char D_004BE608[16] = "MC_TST06";

const char D_004BE618[16] = "MC_TST05";

const char D_004BE628[16] = "MC_TST04";

const char D_004BE638[16] = "MC_TST03";

const char D_004BE648[16] = "MC_TST02";

const char D_004BE658[16] = "MC_TST01";

const char D_004BE668[16] = "araikabe";

const char D_004BE678[16] = "MC_ELS06H";

const char D_004BE688[16] = "MC_ELS01H";

const char D_004BE698[16] = "MC_ELS13B";

const char D_004BE6A8[16] = "MC_ELS12B";

const char D_004BE6B8[16] = "MC_ELS08B";

const char D_004BE6C8[16] = "MC_ELS07B";

const char D_004BE6D8[16] = "MC_ELS06B";

const char D_004BE6E8[16] = "MC_ELS05B";

const char D_004BE6F8[16] = "MC_ELS04B";

const char D_004BE708[16] = "MC_ELS02B";

const char D_004BE718[16] = "MC_ELS16";

const char D_004BE728[16] = "MC_ELS15";

const char D_004BE738[16] = "MC_ELS13";

const char D_004BE748[16] = "MC_ELS12";

const char D_004BE758[16] = "MC_ELS11";

const char D_004BE768[16] = "MC_ELS10";

const char D_004BE778[16] = "MC_ELS09";

const char D_004BE788[16] = "MC_ELS08";

const char D_004BE798[16] = "MC_ELS07";

const char D_004BE7A8[16] = "MC_ELS06";

const char D_004BE7B8[16] = "MC_ELS05";

const char D_004BE7C8[16] = "MC_ELS04";

const char D_004BE7D8[16] = "MC_ELS03";

const char D_004BE7E8[16] = "MC_ELS02";

const char D_004BE7F8[16] = "MC_SYU01";

const char D_004BE808[16] = "MC_UMN02";

const char D_004BE818[16] = "MC_UMN01";

const char D_004BE828[16] = "MC_UZU08";

const char D_004BE838[16] = "MC_UZU07";

const char D_004BE848[16] = "MC_UZU06";

const char D_004BE858[16] = "MC_UZU05";

const char D_004BE868[16] = "MC_UZU04";

const char D_004BE878[16] = "MC_UZU03";

const char D_004BE888[16] = "MC_UZU02";

const char D_004BE898[16] = "MC_UZU01";

const char D_004BE8A8[16] = "MC_MIL02";

const char D_004BE8B8[16] = "MC_MIL01";

const char D_004BE8C8[16] = "MC_EVE49";

const char D_004BE8D8[16] = "MC_EVE48";

const char D_004BE8E8[16] = "MC_EVE47";

const char D_004BE8F8[16] = "MC_EVE46";

const char D_004BE908[16] = "MC_EVE45";

const char D_004BE918[16] = "MC_EVE44";

const char D_004BE928[16] = "MC_EVE43";

const char D_004BE938[16] = "MC_EVE42";

const char D_004BE948[16] = "MC_EVE41";

const char D_004BE958[16] = "MC_EVE40";

const char D_004BE968[16] = "MC_EVE39";

const char D_004BE978[16] = "MC_EVE38";

const char D_004BE988[16] = "MC_EVE37";

const char D_004BE998[16] = "MC_EVE36";

const char D_004BE9A8[16] = "MC_EVE35";

const char D_004BE9B8[16] = "MC_EVE33";

const char D_004BE9C8[16] = "MC_EVE32";

const char D_004BE9D8[16] = "MC_EVE31";

const char D_004BE9E8[16] = "MC_EVE30";

const char D_004BE9F8[16] = "MC_EVE29";

const char D_004BEA08[16] = "MC_EVE28";

const char D_004BEA18[16] = "MC_EVE27";

const char D_004BEA28[16] = "MC_EVE26";

const char D_004BEA38[16] = "MC_EVE25";

const char D_004BEA48[16] = "MC_EVE24";

const char D_004BEA58[16] = "MC_EVE23";

const char D_004BEA68[16] = "MC_EVE22";

const char D_004BEA78[16] = "MC_EVE21";

const char D_004BEA88[16] = "MC_EVE20";

const char D_004BEA98[16] = "MC_EVE19";

const char D_004BEAA8[16] = "MC_EVE18";

const char D_004BEAB8[16] = "MC_EVE17";

const char D_004BEAC8[16] = "MC_EVE16";

const char D_004BEAD8[16] = "MC_EVE15";

const char D_004BEAE8[16] = "MC_EVE14";

const char D_004BEAF8[16] = "MC_EVE13";

const char D_004BEB08[16] = "MC_EVE12";

const char D_004BEB18[16] = "MC_EVE11";

const char D_004BEB28[16] = "MC_EVE10";

const char D_004BEB38[16] = "MC_EVE09";

const char D_004BEB48[16] = "MC_EVE08";

const char D_004BEB58[16] = "MC_EVE07";

const char D_004BEB68[16] = "MC_EVE06";

const char D_004BEB78[16] = "MC_EVE05";

const char D_004BEB88[16] = "MC_EVE04";

const char D_004BEB98[16] = "MC_EVE03";

const char D_004BEBA8[16] = "MC_EVE02";

const char D_004BEBB8[16] = "MC_EVE01";

const char D_004BEBC8[16] = "MC_DYU04B";

const char D_004BEBD8[16] = "MC_DYU18";

const char D_004BEBE8[16] = "MC_DYU17";

const char D_004BEBF8[16] = "MC_DYU16";

const char D_004BEC08[16] = "MC_DYU15";

const char D_004BEC18[16] = "MC_DYU14";

const char D_004BEC28[16] = "MC_DYU13";

const char D_004BEC38[16] = "MC_DYU12";

const char D_004BEC48[16] = "MC_DYU10";

const char D_004BEC58[16] = "MC_DYU08";

const char D_004BEC68[16] = "MC_DYU07";

const char D_004BEC78[16] = "MC_DYU06";

const char D_004BEC88[16] = "MC_DYU05";

const char D_004BEC98[16] = "MC_DYU04";

const char D_004BECA8[16] = "MC_DYU03";

const char D_004BECB8[16] = "MC_DYU02";

const char D_004BECC8[16] = "MC_DYU01";

const char D_004BECD8[16] = "MC_UTA19";

const char D_004BECE8[16] = "MC_UTA18";

const char D_004BECF8[16] = "MC_UTA17";

const char D_004BED08[16] = "MC_UTA16";

const char D_004BED18[16] = "MC_UTA15";

const char D_004BED28[16] = "MC_UTA14";

const char D_004BED38[16] = "MC_UTA13";

const char D_004BED48[16] = "MC_UTA12";

const char D_004BED58[16] = "MC_UTA11";

const char D_004BED68[16] = "MC_UTA10";

const char D_004BED78[16] = "MC_UTA09";

const char D_004BED88[16] = "MC_UTA08";

const char D_004BED98[16] = "MC_UTA07";

const char D_004BEDA8[16] = "MC_UTA06";

const char D_004BEDB8[16] = "MC_UTA05";

const char D_004BEDC8[16] = "MC_UTA04";

const char D_004BEDD8[16] = "MC_UTA03";

const char D_004BEDE8[16] = "MC_UTA02";

const char D_004BEDF8[16] = "MC_UTA01";

const char D_004BEE08[16] = "MC_SYO01";

const char D_004BEE18[16] = "MC_PRO09";

const char D_004BEE28[16] = "MC_PRO08";

const char D_004BEE38[16] = "MC_PRO07";

const char D_004BEE48[16] = "MC_PRO06";

const char D_004BEE58[16] = "MC_PRO05";

const char D_004BEE68[16] = "MC_PRO04";

const char D_004BEE78[16] = "MC_PRO03";

const char D_004BEE88[16] = "MC_PRO02";

const char D_004BEE98[16] = "MC_PRO01";

const char D_004BEEA8[16] = "MC_KAS41";

const char D_004BEEB8[16] = "MC_KAS40";

const char D_004BEEC8[16] = "MC_KAS39";

const char D_004BEED8[16] = "MC_KAS38";

const char D_004BEEE8[16] = "MC_KAS37";

const char D_004BEEF8[16] = "MC_KAS36";

const char D_004BEF08[16] = "MC_KAS35";

const char D_004BEF18[16] = "MC_KAS34";

const char D_004BEF28[16] = "MC_KAS33";

const char D_004BEF38[16] = "MC_KAS32";

const char D_004BEF48[16] = "MC_KAS31";

const char D_004BEF58[16] = "MC_KAS30";

const char D_004BEF68[16] = "MC_KAS29";

const char D_004BEF78[16] = "MC_KAS28";

const char D_004BEF88[16] = "MC_KAS27";

const char D_004BEF98[16] = "MC_KAS26";

const char D_004BEFA8[16] = "MC_KAS25";

const char D_004BEFB8[16] = "MC_KAS24";

const char D_004BEFC8[16] = "MC_KAS23";

const char D_004BEFD8[16] = "MC_KAS22";

const char D_004BEFE8[16] = "MC_KAS21";

const char D_004BEFF8[16] = "MC_KAS20";

const char D_004BF008[16] = "MC_KAS19";

const char D_004BF018[16] = "MC_KAS18";

const char D_004BF028[16] = "MC_KAS17";

const char D_004BF038[16] = "MC_KAS16";

const char D_004BF048[16] = "MC_KAS15";

const char D_004BF058[16] = "MC_KAS14";

const char D_004BF068[16] = "MC_KAS13";

const char D_004BF078[16] = "MC_KAS12";

const char D_004BF088[16] = "MC_KAS11";

const char D_004BF098[16] = "MC_KAS10";

const char D_004BF0A8[16] = "MC_KAS09";

const char D_004BF0B8[16] = "MC_KAS08";

const char D_004BF0C8[16] = "MC_KAS07";

const char D_004BF0D8[16] = "MC_KAS06";

const char D_004BF0E8[16] = "MC_KAS05";

const char D_004BF0F8[16] = "MC_KAS04";

const char D_004BF108[16] = "MC_KAS03";

const char D_004BF118[16] = "MC_KAS02";

const char D_004BF128[16] = "MC_KAS01";

const char D_004BF138[16] = "MC_KUK16";

const char D_004BF148[16] = "MC_KUK10B";

const char D_004BF158[16] = "MC_KUK09B";

const char D_004BF168[16] = "MC_KUK08B";

const char D_004BF178[16] = "MC_KUK07B";

const char D_004BF188[16] = "MC_KUK06B";

const char D_004BF198[16] = "MC_KUK05B";

const char D_004BF1A8[16] = "MC_KUK04B";

const char D_004BF1B8[16] = "MC_KUK03B";

const char D_004BF1C8[16] = "MC_KUK15";

const char D_004BF1D8[16] = "MC_KUK14";

const char D_004BF1E8[16] = "MC_KUK13";

const char D_004BF1F8[16] = "MC_KUK12";

const char D_004BF208[16] = "MC_KUK11";

const char D_004BF218[16] = "MC_KUK10";

const char D_004BF228[16] = "MC_KUK09";

const char D_004BF238[16] = "MC_KUK08";

const char D_004BF248[16] = "MC_KUK07";

const char D_004BF258[16] = "MC_KUK06";

const char D_004BF268[16] = "MC_KUK05";

const char D_004BF278[16] = "MC_KUK04";

const char D_004BF288[16] = "MC_KUK03";

const char D_004BF298[16] = "MC_KUK02";

const char D_004BF2A8[16] = "MC_KUK01";

const char D_004BF2B8[16] = "MC_GNK25";

const char D_004BF2C8[16] = "MC_GNK24";

const char D_004BF2D8[16] = "MC_GNK23";

const char D_004BF2E8[16] = "MC_GNK22";

const char D_004BF2F8[16] = "MC_GNK21";

const char D_004BF308[16] = "MC_GNK20";

const char D_004BF318[16] = "MC_GNK19";

const char D_004BF328[16] = "MC_GNK18";

const char D_004BF338[16] = "MC_GNK17";

const char D_004BF348[16] = "MC_GNK16";

const char D_004BF358[16] = "MC_GNK15";

const char D_004BF368[16] = "MC_GNK14";

const char D_004BF378[16] = "MC_GNK13";

const char D_004BF388[16] = "MC_GNK12";

const char D_004BF398[16] = "MC_GNK11";

const char D_004BF3A8[16] = "MC_GNK10";

const char D_004BF3B8[16] = "MC_GNK09";

const char D_004BF3C8[16] = "MC_GNK08";

const char D_004BF3D8[16] = "MC_GNK07";

const char D_004BF3E8[16] = "MC_GNK06";

const char D_004BF3F8[16] = "MC_GNK05";

const char D_004BF408[16] = "MC_GNK04";

const char D_004BF418[16] = "MC_GNK03";

const char D_004BF428[16] = "MC_GNK02";

const char D_004BF438[16] = "MC_GNK01";

const char D_004BF448[16] = "MC_UTK03B";

const char D_004BF458[16] = "MC_UTK11";

const char D_004BF468[16] = "MC_UTK10";

const char D_004BF478[16] = "MC_UTK09";

const char D_004BF488[16] = "MC_UTK08";

const char D_004BF498[16] = "MC_UTK07";

const char D_004BF4A8[16] = "MC_UTK06";

const char D_004BF4B8[16] = "MC_UTK05";

const char D_004BF4C8[16] = "MC_UTK04";

const char D_004BF4D8[16] = "MC_UTK03";

const char D_004BF4E8[16] = "MC_UTK02";

const char D_004BF4F8[16] = "MC_UTK01";

const char D_004BF508[16] = "MC_KOU07";

const char D_004BF518[16] = "MC_KOU05";

const char D_004BF528[16] = "MC_KOU04";

const char D_004BF538[16] = "MC_KOU02";

const char D_004BF548[16] = "MC_KOU01";

const char D_004BF558[16] = "MC_GNU14";

const char D_004BF568[16] = "MC_GNU13";

const char D_004BF578[16] = "MC_GNU12";

const char D_004BF588[16] = "MC_GNU11";

const char D_004BF598[16] = "MC_GNU10";

const char D_004BF5A8[16] = "MC_GNU09";

const char D_004BF5B8[16] = "MC_GNU08";

const char D_004BF5C8[16] = "MC_GNU07";

const char D_004BF5D8[16] = "MC_GNU06";

const char D_004BF5E8[16] = "MC_GNU05";

const char D_004BF5F8[16] = "MC_GNU04";

const char D_004BF608[16] = "MC_GNU03";

const char D_004BF618[16] = "MC_GNU02";

const char D_004BF628[16] = "MC_GNU01";

const char D_004BF638[16] = "MC_UDO22";

const char D_004BF648[16] = "MC_UDO21";

const char D_004BF658[16] = "MC_UDO15";

const char D_004BF668[16] = "MC_UDO14";

const char D_004BF678[16] = "MC_UDO13";

const char D_004BF688[16] = "MC_UDO12";

const char D_004BF698[16] = "MC_UDO11";

const char D_004BF6A8[16] = "MC_UDO10";

const char D_004BF6B8[16] = "MC_UDO09";

const char D_004BF6C8[16] = "MC_UDO08";

const char D_004BF6D8[16] = "MC_UDO07";

const char D_004BF6E8[16] = "MC_UDO06";

const char D_004BF6F8[16] = "MC_UDO05";

const char D_004BF708[16] = "MC_UDO04";

const char D_004BF718[16] = "MC_UDO03";

const char D_004BF728[16] = "MC_UDO02";

const char D_004BF738[24] = "MC_UDO01";

MapTaskState tsk = { { 0 }, 0, { 0, 0, 0 } };

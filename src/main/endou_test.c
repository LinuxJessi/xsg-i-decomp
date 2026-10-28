#include "common.h"
#include "main/act_1.h"
#include "main/open_close_main.h"
#include "main/e_number_main.h"

#include "shared.h"

#include "endou_test.h"

extern int Select_82;

extern int TopMode_86;

typedef struct PartyAttackSlot {
    unsigned short character;
    signed char position;
    unsigned char unmodeled_03;
} PartyAttackSlot;



typedef struct PartyDataView {
    unsigned char unmodeled_00[0x30];
    PartyAttackSlot attack_slots[3];
} PartyDataView;



extern const char D_004C2F10[];

extern const char D_004C2F30[];

extern const char *text00_83[10];

extern const char *text01_84[3];

extern const char *text02_85[];

extern const char D_004C2F68[];

extern const char D_004C2F90[];

typedef struct EndouBattleWindowParams {
    int x;
    int y;
    int style;
    int information_x;
    int information_y;
    void *commands;
    const char *title;
    int repeat;
    int selection;
} EndouBattleWindowParams;



typedef struct EndouBattleMessageParams {
    int x;
    int y;
    int style;
    const char *title;
    const char *text;
} EndouBattleMessageParams;



typedef struct EndouBattleAnalysisParams {
    int x;
    int y;
    int style;
    const char *name;
    short hp;
    short max_hp;
    unsigned short conditions;
    u8 unmodeled_16[2];
    int technique_points;
    int ether_points;
    int skill_points;
    int experience;
    long money;
    unsigned short common_drop;
    unsigned short rare_drop;
} EndouBattleAnalysisParams;



typedef struct EndouBattleRewardParams {
    int x;
    int y;
    int style;
    int width;
    const char *text;
    const char *title;
} EndouBattleRewardParams;



extern unsigned char data_15[];

extern const char *tag_16[];

extern const char text_17[];

extern const char *msg_19[];

extern const char tag_20[];

extern int count_18;

extern const char D_004C2A70[];

extern const char D_004C2A88[];

extern const char D_004DA720[];

extern const char D_004DA730[];

extern void xglSleep(void);

/* ld at +0x28 and lhu/sh at +0x28/+0x2a access the same button storage. */
typedef struct PadDataButtonView {
    u8 unmodeled_00[0x28];
    union {
        u64 button_state;
        struct {
            u16 held;
            u16 pressed;
            u16 repeat;
            u16 released;
        } input;
    } buttons;
} PadDataButtonView;



extern PadDataButtonView PadData;

extern void xglCdLoadOverlay(int overlayId);

extern void SeisanInit(void);

extern int SeisanMain(void);

extern void SeisanDisp(void);

extern void xglRenderClearFrame(void);

typedef struct EModelTestState {
    unsigned char unmodeled_00[0x11];
    unsigned char isReady;
    unsigned char unmodeled_12[1];
    unsigned char extFuncFlag;
} EModelTestState;



extern void MenuModelUnitOpen(void *unit, int drawType);

extern void MenuModelControl(void *unit);

extern void ACT_init(void);

extern void MenuLoadInit(void);

extern void *MenuModelInit(void *work_start);

extern void MenuModelMemoryInit(void *base, int size);

extern void MenuModelMemorySet(int count);

extern void MenuModelCreate(int *model, int character_id);

extern void MenuModelExtFuncSet(int model,
                                void (*callback)(EModelTestState *),
                                int argument);

extern void MenuModelMain(void);

/* EUC-JP bytes for the model-test diagnostic with its leading control byte. */

/* JP: モデルテスト
 * EN: Model test */
const unsigned char D_004C2AE8[16] =
    "\x0B\xA5\xE2\xA5\xC7\xA5\xEB\xA5\xC6\xA5\xB9\xA5\xC8";

static const char not_found_format[16] = "%s/NotFound";

static const char read_error_format[24] = "%s/ReadError";

typedef struct GifPackedTag {
    unsigned int loop_count_and_eop;
    unsigned int primitive_flags_and_register_count;
    u64 register_descriptors;
} GifPackedTag;



typedef struct GifAdWrite {
    u64 value;
    u64 register_id;
} GifAdWrite;



typedef struct GifPackedRgbaq {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
} GifPackedRgbaq;



typedef struct GifUv {
    unsigned int u;
    unsigned int v;
    unsigned int q;
    unsigned int reserved;
} GifUv;



typedef struct GifXyz2 {
    unsigned int x;
    unsigned int y;
    unsigned int z;
    unsigned int fog;
} GifXyz2;



typedef struct BgTestRenderState {
    unsigned char unmodeled_00[0x14];
    unsigned short packetControl;
} BgTestRenderState;



typedef struct BgTestEnvironment {
    unsigned int vif_nop[3];
    unsigned int vif_directhl_nine_quadwords;
    GifPackedTag tag;
    GifAdWrite texture_setup;
    GifAdWrite packet_write;
    GifAdWrite primitive_setup;
    GifPackedRgbaq color;
    GifUv first_uv;
    GifXyz2 first_vertex;
    GifUv second_uv;
    GifXyz2 second_vertex;
} BgTestEnvironment;



enum {
    BG_TEST_COMMAND_HIGH = 0xc800,
    BG_TEST_COMMAND_BASE = 0x2412,
    BG_TEST_COMMAND_BASE_SHIFT = 16,
    BG_TEST_CONTROL_SHIFT = 5,
    BG_TEST_COMMAND_HIGH_SHIFT = 19
};



extern BgTestRenderState sRender;

static BgTestEnvironment TestEnv_47 = {
    { 0, 0, 0 }, 0x50000009,
    { 0x00008001, 0x808B4000, 0x0000000053531EEEULL },
    { 0x20, 0x14 },
    { 0, 0x6 },
    { 0x00050000, 0x47 },
    { 0x80, 0x80, 0x80, 0x80 },
    { 0, 0, 0, 0 },
    { 0x6FF8, 0x71F8, 0, 0 },
    { 0x2000, 0x1C00, 0, 0 },
    { 0x8FF8, 0x8DF8, 0, 0 },
};

extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

void *test_data = 0;

static char fname_67[64];

static float size_68;

static unsigned int TestEnv_66[32] = {
    0x00008001, 0x208B4000, 0x000000EE, 0x00000000,
    0x00000000, 0x00000000, 0x0000003F, 0x00000000,
    0xDD343840, 0x20070005, 0x00000006, 0x00000000,
    0x00000080, 0x00000080, 0x00000080, 0x00000080,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00007C00, 0x00007BF8, 0x10000000, 0x00000000,
    0x00000800, 0x00000800, 0x00000000, 0x00000000,
    0x00008400, 0x000083F8, 0x10000000, 0x00000000
};

typedef struct WinTexEnvironment {
    unsigned int vif_nop_before_flush;
    unsigned int vif_nop_before_direct;
    unsigned int vif_flushe;
    unsigned int vif_directhl_six_quadwords;
    GifPackedTag gif_tag;
    GifAdWrite texture_flush;
    GifAdWrite bitblt_buffer;
    GifAdWrite transfer_position;
    GifAdWrite transfer_region;
    GifAdWrite transfer_direction;
} WinTexEnvironment;



/* VIF DIRECTHL carries a GIF PACKED tag and five GS A+D register writes. */

static WinTexEnvironment WinTexEnv_65 = {
    0, 0, 0x11000000, 0x51000006,
    { 0x00008005, 0x10000000, 0x000000000000000EULL },
    { 0x0000000000000000ULL, 0x000000000000003FULL },
    { 0x0008380000000000ULL, 0x0000000000000050ULL },
    { 0x0000000000000000ULL, 0x0000000000000051ULL },
    { 0x0000008000000200ULL, 0x0000000000000052ULL },
    { 0x0000000000000000ULL, 0x0000000000000053ULL },
};

/*
 * The context e_test's argument points to: only the VIF packet pointer at
 * +0x00 is evidenced here (main VA 0x002728f8, lw $4,0x0($16) before each
 * sceVif1Pk* call); the rest stays an unmodeled span (docs/naming.md).
 */

typedef struct ETestContext {
    XglPacket *packet;
} ETestContext;



extern void xglFontReloadTexture(ETestContext *context, int mode);

/*
 * The original PadData object is 0xD0 bytes at 0x00490D90. EndouTest uses the
 * same eight bytes at +0x28 as two halfwords for button state and as one
 * 64-bit exit-chord word; this local prefix models only that evidenced view.
 * SeisanTest keeps its separate full-width declaration above unchanged.
 */

typedef struct EndouPadDataPrefix {
    u8 unmodeled_00[0x28];
    union {
        u64 button_state;
        struct {
            u16 half_28;
            u16 half_2a;
            u8 unmodeled_2c[4];
        } halves;
    } buttons;
} EndouPadDataPrefix;



typedef struct EndouClearEnvironment {
    u8 unmodeled_00[0x20];
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
} EndouClearEnvironment;



extern EndouClearEnvironment ClearEnv;

extern const char D_004C2440[];

extern const char D_004C2458[];

extern const char D_004C2468[];

extern const char D_004C2480[];

extern const char D_004C2498[];

extern const char D_004C24B0[];

extern const char D_004C24C8[];

extern const char D_004C24E0[];

extern const char D_004C24F8[];

extern const char D_004C2510[];

extern const char D_004C2528[];

extern const char D_004C2540[];

extern const char D_004DA660[];

extern void endPrintInit(void);

void SeisanTest(void);

void eMessageTest(void);

void eNumberTest(void);

void eModelTest(void);

void TexturTest(void);

void JpegTest(void);

void BattleWindowTest(void);

void ModelTest(void);

void TextTest(void);

void ShopTest(void);

extern int MenuFileMain(int mode);

typedef struct ShopDataRecord {
    u16 values[0xb8];
} ShopDataRecord;



extern void PartyFriendOn(int party_id);

extern void PartyTakeAgwsOn(int characterId);

/* Clear the CPU button state before the render thread samples it again. */

#define ENDOU_PAD_BUTTONS ((EndouPadDataPrefix *)(void *)&PadData)

/* Volatile input view: the separate xglRenderEntry thread calls xglPadRead,
 * which writes the decoded controller words. xglSleep yields between reads. */

#define ENDOU_PAD_INPUT ((volatile EndouPadDataPrefix *)(void *)&PadData)

#define ENDOU_CLEAR_ENV ((EndouClearEnvironment *)(void *)&ClearEnv)

/* EUC-JP bytes for the model-test diagnostic with its leading control byte. */

typedef struct EMessageTestWindow {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    u16 message_x;
    u16 message_y;
    u32 message_color;
    u8 unmodeled_0c[2];
    s16 selection_value;
    u8 unmodeled_10[2];
    u8 debug_value;
    u8 debug_cursor;
    u8 unmodeled_14[4];
    const char *message_text;
    u8 page_state;
    u8 unmodeled_1d[0x33];
} EMessageTestWindow;



extern int WindowTexLoad(unsigned char *buffer, unsigned int request);

extern void eMessageSet(void *message, const char *text);

extern void eMessageTextChange(void *message, const char *text);

extern int eMessageNextPage(void *message, int mode);

extern void eMessageDraw(void *message);

extern void eMessageMain(void *message);

extern void endPrintExtFunc(int mode, int amount, void *data);

extern void xglFontDebugHex(int x, int y, int value, int digits);

extern const char *msg_45[5];

extern const char text00_46[];

extern const char D_004C2C70[];

#include "main/control_entry.h"

#include "main/xgl_cd.h"

#include "main/xgl_font.h"

extern const char D_004C2CF0[];

extern const char D_004C2D08[];

extern const char D_004C2D18[];

extern void xglFontPrintExtFunc(unsigned int mask,
                                void (*callback)(ETestContext *),
                                void *context);

typedef struct UmnEventTextRecord {
    u32 unmodeled_00;
    const char *message_text;
} UmnEventTextRecord;



typedef struct TextTestMessage {
    u8 unmodeled_00[4];
    u16 message_x;
    u16 message_y;
    int message_parameter;
    u8 unmodeled_0c[0x10];
    u8 page_state;
    u8 unmodeled_1d[0x33];
} TextTestMessage;



extern u8 *UmnEventTextInit(u8 *work_end);

extern int UmnEventTextMake(int mode);

extern UmnEventTextRecord *UmnEventTextNextGet(int index);

extern void eMessageModeChange(void *message, u8 mode);

extern const char D_004C2558[];

typedef struct UmnMailDisplayParams {
    int mode;
    u8 unmodeled_04[4];
    u16 texture_x;
    u16 texture_y;
    u16 texture_width;
    u16 texture_height;
    u8 unmodeled_10[8];
    s16 scroll_x;
    s16 scroll_y;
    u16 view_width;
    u16 view_height;
    u8 unmodeled_20[0x20];
} UmnMailDisplayParams;



extern void tyaUmlDispInit(void);

extern void tyaUmlDispParamReset(UmnMailDisplayParams *params, int mode);

extern void tyaUmlDispMain(UmnMailDisplayParams *params);

extern const char D_004C2CD0[];

typedef struct JpegTestDecodeRequest {
    void *source;
    void *destination;
    u8 unmodeled_08[8];
    u16 source_width;
    u16 source_height;
    u16 max_width;
    u16 max_height;
    u8 unmodeled_18[8];
} JpegTestDecodeRequest;



typedef struct JpegTestDisplay {
    u16 x;
    u16 y;
    u32 color;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    void *image_data;
} JpegTestDisplay;



extern int GameSnapShotSaveThumbnail(int quality, void *jpeg_buffer);

extern const char D_004C2DA8[];

extern int Select_74;

extern int Select_76;

extern int Select_78;

extern int Select_80;

extern const char D_004C2DF0[];

extern const char D_004C2E08[];

extern const char D_004C2E38[];

extern const char D_004C2E58[];

extern const char D_004C2E88[];

extern const char D_004C2EA8[];

extern const char D_004C2EC0[];

extern const char D_004DA768[];

extern const char *text00_75[];

extern const char *text00_77[];

extern const char *text00_79[];

extern const char *text00_81[];

extern char *MenuCharNameGet(int characterId);

extern int PartyTakeAgwsCheck(int characterId);

extern void PartyTakeAgwsOff(int characterId);

extern int PartyLockPartyCheck(int characterId);

extern void PartyLockPartyOn(int characterId);

extern void PartyLockPartyOff(int characterId);

extern void func_A22628(void);

extern int dataMoneyBoxInc(int amount);

extern void MenuShopMain(int menu_selection);

extern const char D_004C2568[], D_004C2588[], D_004C2598[], D_004C25B0[], D_004C25C8[];

extern const char D_004DA668[], D_004DA670[], D_004DA678[], D_004DA680[], D_004DA688[];

extern const char D_004DA690[], D_004DA698[], D_004DA6A0[], D_004DA6A8[], D_004DA6B0[];

extern const char D_004DA6B8[];

static inline void endouShopHeading(int y, const char *format, int shop_number)
{
    xglFontDebugPrintf(0, y, format, shop_number);
}

typedef struct FileLoadState {
    int state;
    void *buffer;
    unsigned char unmodeled_08[0x40];
} FileLoadState;



extern FileLoadState files;

extern u64 eNumberTexEnv_69[12];

extern u64 TestEnv_70[20];

extern int x_71;

extern const char D_004C2D80[];

extern const char D_004C2D98[];

extern void (*xglCdSetCallback(void (*callback)(int, int)))(int, int);

typedef int s32;



typedef s32 M2C_UNK;



typedef struct StudioLight StudioLight;



typedef Vector4 EndouVector __attribute__((aligned(16)));
typedef struct EndouModelActor {
    unsigned char unmodeled_00[0x10];
    EndouVector position;
    unsigned char unmodeled_20[0x30];
    EndouVector rotation;
    EndouVector scale;
    unsigned char unmodeled_70[0x16];
    short resource_kind;
    unsigned char unmodeled_88[0x8d0 - 0x88];
    void *model_resource;
    void *matrix_resource;
    void *move;
    void *anim_pack_tables[8];
    unsigned char unmodeled_8fc[0x9d8 - 0x8fc];
    unsigned long long draw_alpha;
} EndouModelActor;



M2C_UNK ACT_initMotion();

void ACT_modelDraw(EndouModelActor *actor);

EndouModelActor *ACT_setArms(EndouModelActor *actor, EndouModelActor *parent,
                             unsigned int accessory_id, int flags);

void ACT_setModelWrapper(EndouModelActor *actor, int unused);

void ACT_setMotion(EndouModelActor *actor, int motion_id);

void ACT_updateMotion(EndouModelActor *actor);

void GameResourceInit(unsigned int address, int size);

void RES_GetMdlFileName(char *filename, int resource_kind);

void nmlModelFlush(void);

char *strcpy(char *destination, const char *source);

void xglRotTransPersN(void *destination, const Matrix4 matrix,
                      const void *points, int count, int cameraId);

void xglStudioChange(int studio_index);

StudioCamera *xglStudioGetActiveCamera(void);

void xglLightDirection(void *light, unsigned int index,
                       const Vector4 *direction);
void xglLightIntensityAmbient(void *light, const void *intensity);
void xglLightIntensityParallel(void *light, unsigned int index,
                               const void *intensity);
void xglStudioGetLight(StudioLight **light_out);

typedef struct EndouTestLighting {
    unsigned long long ambient[2];
    unsigned long long parallel_intensity[3][2];
    Vector4 direction[3];
} EndouTestLighting;

static inline void endModelCameraPosition(StudioCamera *camera,
                                         float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
}

static __inline__ void EndouApplyModelLighting(StudioLight **light_slot,
                                              EndouTestLighting profile)
{
    xglStudioGetLight(light_slot);
    xglLightIntensityAmbient(*light_slot, profile.ambient);
    xglLightIntensityParallel(*light_slot, 0, profile.parallel_intensity[0]);
    xglLightDirection(*light_slot, 0, &profile.direction[0]);
    xglLightIntensityParallel(*light_slot, 1, profile.parallel_intensity[1]);
    xglLightDirection(*light_slot, 1, &profile.direction[1]);
    xglLightIntensityParallel(*light_slot, 2, profile.parallel_intensity[2]);
    xglLightDirection(*light_slot, 2, &profile.direction[2]);
}



extern const EndouTestLighting D_004C25E0;

extern const signed char D_004C2650[];

extern const char D_004C2658[];

extern const char D_004C26F0[];


extern const unsigned long long D_004C2720[8];

extern const char D_004DA6C0[5];

extern const char D_004DA6C8[5];

extern const char D_004DA6D0[];

extern float ang_0[4];

extern const char *msg00_14[];

extern struct EMessageParam msg_11;

extern s32 no_13;

extern f32 pipo_2;

extern float pos_1[4];

extern s32 time_10;

typedef struct EndouModelLineVertex {
    int x;
    int y;
    int z;
    int terminator;
} EndouModelLineVertex;



typedef struct EndouModelLinePoint {
    EndouModelLineVertex vertex;
    unsigned char color[4];
    unsigned char unmodeled_14[12];
} EndouModelLinePoint;



extern EndouModelLinePoint line00_3[7];

extern EndouModelLinePoint line01_4[7];

extern EndouModelLinePoint line02_5[7];

extern EndouModelLinePoint line03_6[7];

extern EndouModelLinePoint line10_7[7];

extern EndouModelLinePoint line11_8[7];

extern EndouModelLinePoint line12_9[7];

typedef struct EndouOrientationStorage {
    EndouVector rotation;
    EndouVector scale;
} EndouOrientationStorage;



extern const unsigned char D_004DA6E0[];

extern const unsigned char D_004DA6E8[];

typedef struct EndouModelBackQuad {
    int vertices[4][4];
    float uv[4];
} EndouModelBackQuad;

extern EndouModelBackQuad bsp_12;

typedef struct EndouPrintLinePoint {
    int x;
    int y;
    int z;
    u32 unmodeled_0c;
    u8 color[4];
    u8 unmodeled_14[12];
} EndouPrintLinePoint;



typedef struct EndouELinePoint {
    int x;
    int y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} EndouELinePoint;

typedef struct EndouELineSetData {
    u8 state;
    u8 unmodeled_001[2];
    u8 duration;
    u8 progress[5];
    u8 unmodeled_009[3];
    EndouELinePoint points[5];
} EndouELineSetData;



void eLineSet(EndouELineSetData *line, int unused);

typedef struct EndouELineEntry {
    u8 state;
    u8 unmodeled_001[2];
    u8 duration;
    u8 progress[5];
    u8 unmodeled_009[3];
    EndouELinePoint points[5];
    u32 count;
    EndouELinePoint render[5];
} EndouELineEntry;

void eLineMain(EndouELineEntry *line);

void endPrintDirectLine(EndouPrintLinePoint *line);

void endSpriteSet(void *sprite, int mode);

void xglStudioGetCamera(StudioCamera **camera_out, int camera_index);

extern u8 time_37;

extern const char D_004C2AD8[];

extern ECursolMainData cur_25;

typedef struct EndouSpriteQuadVertex {
    int x;
    int y;
    int depth;
    unsigned char unmodeled_00c[4];
} EndouSpriteQuadVertex;

typedef struct EndouSpriteQuad {
    unsigned char flags;
    signed char coordinate_mode;
    unsigned short uv;
    unsigned char unmodeled_004[12];
    EndouSpriteQuadVertex vertices[4];
    unsigned char unmodeled_050[16];
} EndouSpriteQuad;

extern EndouSpriteQuad d_28;

extern ENumberSpriteDescriptor d_33;

extern ENumberSpriteDescriptor d_35[2];

extern ENumberSpriteDescriptor d_36;

extern ENumberSpriteDescriptor d_38;

extern ENumberRenderDescriptor d_39;

extern ENumberRenderDescriptor d_40;

extern ENumberSpriteDescriptor d_41;

typedef struct EndouPrintPoint {
    short x;
    unsigned short y;
    int depth;
    unsigned char color[4];
} EndouPrintPoint;

extern EndouPrintPoint d_42[7];

extern EndouPrintPoint d_43;

typedef struct EndouQuadColor {
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} EndouQuadColor;

typedef struct EndouQuadPosition {
    short x;
    short y;
    int depth;
} EndouQuadPosition;

typedef struct EndouQuadDescriptor {
    EndouQuadPosition points[4];
    EndouQuadColor colors[4];
    unsigned int format;
} EndouQuadDescriptor;

extern EndouQuadDescriptor d_44;

extern EndouELineEntry line_23[4];

extern EndouPrintPoint line_34[6];

extern unsigned char moji2_27[];

extern unsigned char moji_26[];

extern ENumberMainData num_29[1];

extern ENumberMainData num_30[1];

extern ENumberMainData num_31[1];

extern ENumberMainData num_32[1];

typedef struct EndouRibbonRecord {
    short x;
    short y;
    int work;
    short width;
    short height;
    signed char mode;
    unsigned char progress;
    unsigned char visible;
    unsigned char style;
    ERibbonPoint points[6];
    unsigned char unmodeled_058[0x18];
} EndouRibbonRecord;

extern EndouRibbonRecord rbn_21[3];

extern ESprite spr_22[4];

typedef union EndouTagFontViews {
    ETagFont control;
    ETagFontMainData draw;
} EndouTagFontViews;

extern EndouTagFontViews tgf_24[2];

extern unsigned char *buffer_72;

extern char mm_73[];

extern const char D_004C2DB8[], D_004C2DD0[], D_004C2DE0[];























#define NULL ((void *)0)

typedef struct EndouModelPadState {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
    unsigned char unmodeled_2c[4];
    unsigned char unmodeled_30[8];
    signed char stick_axis[4];
} EndouModelPadState;
#define ENDOU_MODEL_PAD ((EndouModelPadState *)(void *)&PadData)

typedef struct EndouModelMessagePrefix {
    unsigned char flags;
    unsigned char mode;
    unsigned char visible;
    unsigned char wrap_width_half;
    short x;
    short y;
    int draw_parameter;
} EndouModelMessagePrefix;
#define ENDOU_MODEL_MESSAGE ((EndouModelMessagePrefix *)(void *)&msg_11)





enum FileLoadDrawState {
    FILE_DRAW_UNINITIALIZED = -1,
    FILE_DRAW_ENABLED = 1
};

struct FileLoadInfo {
    enum FileLoadDrawState status;
    unsigned char *buffer;
    char filename[64];
};


struct FileLoadDrawEnvironment {
    unsigned char unmodeled_00[0x30];
    unsigned long long texture_parameters;
};


void EndouTest(void)
{
    /* Initialize the CPU-owned packet before the renderer flushes it. */
    ENDOU_CLEAR_ENV->color_r = 0;
    ENDOU_CLEAR_ENV->color_g = 0x40;
    ENDOU_CLEAR_ENV->color_b = 0;
    ENDOU_CLEAR_ENV->color_a = 0;

    ENDOU_PAD_BUTTONS->buttons.halves.half_2a = 0;
    endPrintInit();

    for (;;) {
        xglRenderClearFrame();
        if ((ENDOU_PAD_INPUT->buttons.button_state & 0x08000100ULL) == 0x08000100ULL) {
            break;
        }

        xglFontDebugPrintf(0, 0, D_004DA660);
        xglFontDebugPrintf(0, 0x10, D_004C2440);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x10) != 0) {
            SeisanTest();
        }

        xglFontDebugPrintf(0, 0x20, D_004C2458);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_28 & 0x80) != 0) {
            TextTest();
        }

        xglFontDebugPrintf(0, 0x30, D_004C2468);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x40) != 0) {
            eMessageTest();
        }

        xglFontDebugPrintf(0, 0x40, D_004C2480);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x20) != 0) {
            eNumberTest();
        }

        xglFontDebugPrintf(0, 0x50, D_004C2498);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4) != 0) {
            eModelTest();
        }

        xglFontDebugPrintf(0, 0x60, D_004C24B0);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1) != 0) {
            MenuFileMain(1);
        }

        xglFontDebugPrintf(0, 0x70, D_004C24C8);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x8) != 0) {
            TexturTest();
        }

        xglFontDebugPrintf(0, 0x80, D_004C24E0);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2) != 0) {
            JpegTest();
        }

        xglFontDebugPrintf(0, 0x90, D_004C24F8);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1000) != 0) {
            BattleWindowTest();
        }

        xglFontDebugPrintf(0, 0xa0, D_004C2510);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4000) != 0) {
            MenuFileMain(0);
        }

        xglFontDebugPrintf(0, 0xb0, D_004C2528);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2000) != 0) {
            ModelTest();
        }

        xglFontDebugPrintf(0, 0xc0, D_004C2540);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x8000) != 0) {
            ShopTest();
        }

        xglSleep();
    }
}

void TextTest(void)
{
    TextTestMessage message;
    UmnEventTextRecord *record;

    xglCdLoadOverlay(2);
    xglRenderClearFrame();
    WorkEnd = UmnEventTextInit(WorkEnd);
    UmnEventTextMake(1);
    endPrintInit();

    record = UmnEventTextNextGet(0);
    if (record == 0) {
        return;
    }

    eMessageSet(&message, record->message_text);
    message.message_x = 0x40;
    message.message_y = 0x40;
    message.message_parameter = 0x00fffff0;
    message.page_state = 2;
    eMessageModeChange(&message, 0x22);

    while (xglFontDebugPrintf(0, 0x10, D_004C2558),
           (PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        eMessageMain(&message);
        if ((PadData.buttons.input.pressed & 8U) != 0 &&
            eMessageNextPage(&message, 0) == 0) {
            record = UmnEventTextNextGet(1);
            if (record == 0) {
                return;
            }
            eMessageTextChange(&message, record->message_text);
            eMessageModeChange(&message, 0x22);
        }

        endPrintExtFunc(0, 100, 0);
        xglSleep();
    }
}

void ShopTest(void)
{
    unsigned int ab_column_offset;
    typedef struct ShopFile {
        u16 records[20][0xb8];
    } ShopFile;
    int row_index;
    int menu_open;
    unsigned int menu_selection = 1;
    ShopFile *shop_file;
    unsigned short pressed;

    xglCdLoadOverlay(1);
    func_A22628();
    ACT_init();

    shop_file = (ShopFile *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = (u8 *)shop_file + xglCdReadFile(D_004C2568, shop_file, 0, 1);
    PartyFriendOn(3);
    PartyFriendOn(2);
    menu_open = 0;
    PartyFriendOn(7);
    PartyTakeAgwsOn(0x11);
    PartyTakeAgwsOn(0x18);
    if ((PadData.buttons.input.held & 0x100U) != 0)
        dataMoneyBoxInc(0x64);
    else
        dataMoneyBoxInc(0xf4240);
    xglRenderClearFrame();

    for (;;) {
        xglFontDebugPrintf(0, 0x10, D_004C2588);
        if ((PadData.buttons.button_state & 0x08000100U) == 0x08000100U)
            break;
        pressed = PadData.buttons.input.pressed;
        if ((pressed & 0x40) != 0)
            break;

        switch (menu_open) {
        case 0: {
            u16 repeated;
            u16 (*selected_page)[0xb8];
            const u16 *entry_values;
            const u16 *neighbor_values;
            const u16 *extra_values;

            repeated = PadData.buttons.input.repeat;
            if ((repeated & 0x1000U) != 0)
                menu_selection = menu_selection == 1 ? 0x14 : menu_selection - 1;
            if ((repeated & 0x4000U) != 0)
                menu_selection = menu_selection == 0x14 ? 1 : menu_selection + 1;
            if ((pressed & 0x20) != 0)
                menu_open = 1;
            xglFontDebugPrintf(0, 0x20, D_004C2598);
            xglFontDebugPrintf(0x80, 0x20, D_004C25B0);
            endouShopHeading(0x30, D_004C25C8, menu_selection);

            {
            u16 (*const page)[0xb8] =
                &shop_file->records[menu_selection];

            endouShopHeading(0x40, D_004DA668, menu_selection);
            endouShopHeading(0x50, D_004DA670, menu_selection);
            selected_page = page - 1;
            endouShopHeading(0x60, D_004DA678, menu_selection);
            endouShopHeading(0x70, D_004DA680, menu_selection);
            endouShopHeading(0x80, D_004DA688, menu_selection);
            endouShopHeading(0x90, D_004DA690, menu_selection);
            endouShopHeading(0xa0, D_004DA698, menu_selection);
            endouShopHeading(0xb0, D_004DA6A0, menu_selection);
            endouShopHeading(0xc0, D_004DA6A8, menu_selection);
            endouShopHeading(0xd0, D_004DA6B0, menu_selection);

            row_index = 0;
            extra_values = page[-1] + 4;
            entry_values = selected_page[0];
            neighbor_values = selected_page[0] + 0x8c;

            do {
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x40, D_004DA6B8,
                                   *entry_values);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x50, D_004DA6B8,
                                   neighbor_values[-0x78]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x60, D_004DA6B8,
                                   entry_values[0x28]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x70, D_004DA6B8,
                                   neighbor_values[-0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x80, D_004DA6B8,
                                   entry_values[0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x90, D_004DA6B8,
                                   neighbor_values[-0x28]);
                ab_column_offset = 6 * 20;
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xa0, D_004DA6B8,
                                   entry_values[ab_column_offset]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xb0, D_004DA6B8,
                                   *neighbor_values++);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xc0, D_004DA6B8,
                                   (entry_values++)[0xa0]);
            } while (++row_index < 0x14);
            for (row_index = 0; row_index < 4; row_index++) {
                int x = (row_index % 10) * 0x18 + 0x10;
                int y = (row_index / 10) * 8;

                xglFontDebugPrintf(x, y + 0xd0, D_004DA6B8,
                                   extra_values[0xb0 + row_index]);
            }
            }
            break;
        }
        case 1:
            menu_open = 0;
            MenuShopMain(menu_selection);
            xglRenderClearFrame();
            break;
        }
        xglSleep();
    }
}

void SeisanTest(void)
{
    const int exit_chord = 0x08000100;

    xglCdLoadOverlay(1);
    SeisanInit();
    xglRenderClearFrame();

    while ((PadData.buttons.button_state & exit_chord) != exit_chord) {
        SeisanMain();
        SeisanDisp();
        xglSleep();
    }
}

void ModelTest(void) {
    StudioLight *light;
    u8 **work_end = &WorkEnd;
    s32 *message_time = &time_10;
    int motion;

    motion = 1;
    xglStudioChange((M2C_UNK) 0);
    EndouApplyModelLighting(&light, D_004C25E0);
    {
    char sp70[128];
    char spF0[128];
    EndouTestLighting unused_lighting;
    char sp1E0[128];
    char sp260[128];
    char sp2E0[128];
    char sp360[128];
    int point_index;
    StudioCamera *temp_17_4;
    StudioCamera *temp_2;
    StudioCamera *temp_2_2;
    f32 temp_f0_21;
    f32 temp_f0_22;
    f32 temp_f0_23;
    f32 temp_f0_24;
    f32 temp_f0_25;
    f32 temp_f0_26;
    f32 temp_f0_27;
    f32 temp_f1;
    f32 temp_f20;
    f32 temp_f2;
    s32 temp_2_11;
    s32 temp_2_14;
    s32 temp_2_16;
    s32 temp_2_18;
    s32 temp_2_20;
    s32 temp_2_22;
    s32 temp_2_24;
    s32 temp_2_26;
    s32 temp_2_5;
    s32 temp_2_7;
    s32 temp_2_9;
    u16 temp_16;
    u16 temp_4;
    u16 var_3_2;
    u8 *temp_3;
    u8 *temp_3_10;
    u8 *temp_3_11;
    u8 *temp_3_2;
    u8 *temp_3_3;
    u8 *temp_3_4;
    u8 *temp_3_5;
    u8 *temp_3_6;
    u8 *temp_3_7;
    u8 *temp_3_8;
    u8 *temp_3_9;
    EndouModelActor *temp_23_9;
    EndouModelActor *temp_2_12;
    EndouModelActor *temp_2_3;
    u8 *resource_buffer;
    u8 *accessory_resource_buffer;

    temp_2 = xglStudioGetCamera2(0);
    temp_2->rotation.x = 0.0f;
    temp_2->rotation.y = 0.0f;
    temp_2->rotation.z = 0.0f;
    endModelCameraPosition(temp_2, 0.0f, 1.0f, 4.0f);
    temp_2->active = 1;
    temp_2->state = 1;
    temp_2_2 = xglStudioGetCamera2(1);
    temp_2_2->state = 1;
    endModelCameraPosition(temp_2_2, -1.0f, 1.0f, 4.0f);
    temp_2_2->rotation.y = 0.0f;
    temp_2_2->rotation.z = 0.0f;
    temp_2_2->active = 0;
    temp_2_2->rotation.x = 0.0f;
    GameResourceInit(0x07000000U, 0x06000000);
    ACT_init();
    temp_2_3 = (EndouModelActor *)(void *)ACT_create(-1, 0x2101);
    ACT_initMotion(temp_2_3);
    __builtin_memcpy(sp70, D_004C2650, 2);
    memset(&sp70[2], 0, 0x7EU);
    __builtin_memcpy(spF0, D_004C2650, 2);
    memset(&spF0[2], 0, 0x7EU);
    RES_GetMdlFileName(sp70, temp_2_3->resource_kind);
    resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, D_004DA6C0);
    temp_2_5 = xglCdReadFile(&spF0, resource_buffer, 0, 0);
    temp_3 = (*work_end) + temp_2_5;
    temp_2_3->model_resource = resource_buffer;
    (*work_end) = temp_3;
    resource_buffer = (u8 *)((u32)(temp_3 + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, D_004DA6C8);
    temp_2_7 = xglCdReadFile(&spF0, resource_buffer, 0, 0);
    temp_3_2 = (*work_end) + temp_2_7;
    temp_2_3->matrix_resource = resource_buffer;
    (*work_end) = temp_3_2;
    resource_buffer = (u8 *)((u32)(temp_3_2 + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, D_004DA6D0);
    temp_2_9 = xglCdReadFile(&spF0, resource_buffer, 0, 0);
    temp_3_3 = ((*work_end) + temp_2_9);
    temp_2_3->move = resource_buffer;
    resource_buffer = (u8 *)((u32)(temp_3_3 + 0x7f) & ~0x7fU);
    (*work_end) = temp_3_3;
    temp_2_11 = xglCdReadFile(&D_004C2658, resource_buffer, 0, 0);
    temp_3_4 = (*work_end) + temp_2_11;
    temp_2_3->anim_pack_tables[0] = resource_buffer;
    (*work_end) = temp_3_4;
    ACT_allocMatrix((ActRecord *)(void *)temp_2_3, -1);
    ACT_setModelWrapper(temp_2_3, 0);
    ACT_setMotion(temp_2_3, (s32) 1);
    temp_2_12 = (EndouModelActor *)(void *)ACT_create(-1, 1);
    ACT_initMotion(temp_2_12);
    __builtin_memcpy(sp1E0, D_004C2650, 2);
    memset(&sp1E0[2], 0, 0x7EU);
    __builtin_memcpy(sp260, D_004C2650, 2);
    memset(&sp260[2], 0, 0x7EU);
    RES_GetMdlFileName(sp1E0, temp_2_12->resource_kind);
    resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, D_004DA6C0);
    temp_2_14 = xglCdReadFile(&sp260, resource_buffer, 0, 0);
    temp_3_5 = (*work_end) + temp_2_14;
    temp_2_12->model_resource = resource_buffer;
    (*work_end) = temp_3_5;
    resource_buffer = (u8 *)((u32)(temp_3_5 + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, D_004DA6C8);
    temp_2_16 = xglCdReadFile(&sp260, resource_buffer, 0, 0);
    temp_3_6 = (*work_end) + temp_2_16;
    temp_2_12->matrix_resource = resource_buffer;
    (*work_end) = temp_3_6;
    resource_buffer = (u8 *)((u32)(temp_3_6 + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, D_004DA6D0);
    temp_2_18 = xglCdReadFile(&sp260, resource_buffer, 0, 0);
    temp_3_7 = (*work_end) + temp_2_18;
    temp_2_12->move = resource_buffer;
    resource_buffer = (u8 *)((u32)(temp_3_7 + 0x7f) & ~0x7fU);
    (*work_end) = temp_3_7;
    temp_2_20 = xglCdReadFile(&D_004C26F0, resource_buffer, 0, 0);
    temp_3_8 = (*work_end) + temp_2_20;
    temp_2_12->anim_pack_tables[0] = resource_buffer;
    (*work_end) = temp_3_8;
    ACT_allocMatrix((ActRecord *)(void *)temp_2_12, -1);
    ACT_setModelWrapper(temp_2_12, 0);
    ACT_setMotion(temp_2_12, (s32) 1);
    temp_23_9 = (EndouModelActor *)(void *)ACT_create(-1, 0x3003);
    __builtin_memcpy(sp2E0, D_004C2650, 2);
    memset(&sp2E0[2], 0, 0x7EU);
    __builtin_memcpy(sp360, D_004C2650, 2);
    memset(&sp360[2], 0, 0x7EU);
    RES_GetMdlFileName(sp2E0, temp_23_9->resource_kind);
    accessory_resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, D_004DA6C0);
    temp_2_22 = xglCdReadFile(&sp360, accessory_resource_buffer, 0, 0);
    temp_3_9 = (*work_end) + temp_2_22;
    temp_23_9->model_resource = accessory_resource_buffer;
    (*work_end) = temp_3_9;
    accessory_resource_buffer = (u8 *)((u32)(temp_3_9 + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, D_004DA6C8);
    temp_2_24 = xglCdReadFile(&sp360, accessory_resource_buffer, 0, 0);
    temp_3_10 = (*work_end) + temp_2_24;
    temp_23_9->matrix_resource = accessory_resource_buffer;
    (*work_end) = temp_3_10;
    accessory_resource_buffer = (u8 *)((u32)(temp_3_10 + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, D_004DA6D0);
    if (xglCdGetFileSize(&sp360) < 0) {
        temp_23_9->move = NULL;
    } else {
        temp_2_26 = xglCdReadFile(&sp360, accessory_resource_buffer, 0, 0);
        temp_3_11 = (*work_end) + temp_2_26;
        temp_23_9->move = accessory_resource_buffer;
        (*work_end) = temp_3_11;
    }
    ACT_allocMatrix((ActRecord *)(void *)temp_23_9, -1);
    ACT_setModelWrapper(temp_23_9, 0);
    ACT_setArms(temp_23_9, temp_2_3, 0x108U, 0);
    {
        while ((PadData.buttons.button_state & 0x08000100) != 0x08000100) {
            var_3_2 = ENDOU_MODEL_PAD->pressed;
            if (var_3_2 & 8) {
                ACT_setMotion(temp_2_3, ++motion);
                var_3_2 = ENDOU_MODEL_PAD->pressed;
            }
            if (var_3_2 & 2) {
                ACT_setMotion(temp_2_3, --motion);
            }
            temp_4 = ENDOU_MODEL_PAD->held;
            if (temp_4 & 0x100) {
                EndouVector reset_position = {0.0f, 0.0f, 0.0f, 1.0f};
                EndouVector reset_rotation = {0.0f, 0.0f, 0.0f, 1.0f};
                EndouVector reset_scale = {1.0f, 1.0f, 1.0f, 1.0f};
                temp_4 = ENDOU_MODEL_PAD->held;
                temp_2_3->position = reset_position;
                temp_2_3->rotation = reset_rotation;
                temp_2_3->scale = reset_scale;
            }
            {
            Matrix4 camera_matrix;
            float world_points[5][6][4];
            EndouModelLineVertex screen_points[5][6];
            Matrix4 projection_matrix;
            unsigned char column_alphas[5];
            unsigned char row_alphas[6];
            unsigned long long quad_words[8];
            if (temp_4 & 0x10) {
                if (temp_4 & 0x2000) {
                    temp_2_3->scale.x += 0.1f;
                }
                if (temp_4 & 0x8000) {
                    temp_2_3->scale.x -= 0.1f;
                }
                if (temp_4 & 0x1000) {
                    temp_2_3->scale.y += 0.1f;
                }
                if (temp_4 & 0x4000) {
                    temp_2_3->scale.y -= 0.1f;
                }
                if (temp_4 & 4) {
                    temp_2_3->scale.z += 0.1f;
                }
                if ((unsigned short)(temp_4 & 1)) {
                    temp_2_3->scale.z -= 0.1f;
                }
            } else {
                if (temp_4 & 0x2000) {
                    temp_2_3->position.x += 0.1f;
                }
                if (temp_4 & 0x8000) {
                    temp_2_3->position.x -= 0.1f;
                }
                if (temp_4 & 0x1000) {
                    temp_2_3->position.y += 0.1f;
                }
                if (temp_4 & 0x4000) {
                    temp_2_3->position.y -= 0.1f;
                }
                if (temp_4 & 4) {
                    temp_2_3->position.z += 0.1f;
                }
                if ((unsigned short)(temp_4 & 1)) {
                    temp_2_3->position.z -= 0.1f;
                }
            }
            if (ENDOU_MODEL_PAD->stick_axis[0] < -0x40) {
                temp_2_3->rotation.y += 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[0] >= 0x41) {
                temp_2_3->rotation.y -= 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[1] < -0x40) {
                temp_2_3->rotation.x += 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[1] >= 0x41) {
                temp_2_3->rotation.x -= 0.04f;
            }
            temp_17_4 = xglStudioGetCamera2(0);
            if (ENDOU_MODEL_PAD->stick_axis[2] < -0x40) {
                ang_0[1] += 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[2] >= 0x41) {
                ang_0[1] -= 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[3] < -0x40) {
                ang_0[0] += 0.04f;
            }
            if (ENDOU_MODEL_PAD->stick_axis[3] >= 0x41) {
                ang_0[0] -= 0.04f;
            }
            xglMatrixStackUnit();
            xglMatrixStackRotZ(ang_0[2]);
            xglMatrixStackRotY(ang_0[1]);
            xglMatrixStackRotX(ang_0[0]);
            xglMatrixStackTrans(pos_1);
            xglMatrixStackSave(camera_matrix);
            temp_f0_21 = ang_0[0];
            temp_16 = ENDOU_MODEL_PAD->held;
            temp_17_4->rotation.x = temp_f0_21;
            temp_f0_22 = ang_0[1];
            temp_17_4->rotation.y = temp_f0_22;
            temp_f1 = ang_0[2];
            temp_17_4->rotation.z = temp_f1;
            temp_f2 = camera_matrix[3][0];
            temp_17_4->position.x = temp_f2;
            temp_f0_23 = camera_matrix[3][1] + 1.0f;
            temp_17_4->position.y = temp_f0_23;
            temp_f20 = camera_matrix[3][2];
            temp_17_4->position.z = temp_f20;
            if (temp_16 & 8) {
                temp_f0_24 = pipo_2 + 0.2;
                pipo_2 = temp_f0_24;
            }
            if (temp_16 & 2) {
                temp_f0_26 = pipo_2 - 0.2;
                pipo_2 = temp_f0_26;
            }
            temp_f0_25 = pipo_2;
            temp_f0_27 = temp_f20 + temp_f0_25;
            temp_17_4->position.z = temp_f0_27;
            temp_2_12->position = temp_2_3->position;
            temp_2_12->position.x += 1.0f;
            temp_2_12->rotation = temp_2_3->rotation;
            temp_2_12->scale = temp_2_3->scale;
            ACT_updateMotion(temp_2_3);
            ACT_updateMotion(temp_2_12);
            xglStudioGetActiveCamera()->active = 0;
            xglStudioGetCamera2(0)->active = 1;
            ACT_modelDraw(temp_2_3);
            xglStudioGetActiveCamera()->active = 0;
            xglStudioGetCamera2(0)->active = 1;
            ACT_modelDraw(temp_2_12);
            nmlModelFlush();
            {
                int row;

                for (point_index = 0; point_index < 6; point_index++) {
                    float x = point_index * 0.5f + -1.25f;

                    for (row = 0; row < 5; row++) {
                        world_points[row][point_index][0] = x;
                        world_points[row][point_index][1] = 0.0f;
                        world_points[row][point_index][3] = 1.0f;
                        world_points[row][point_index][2] = row * 0.5f + -1.0f;
                    }
                }
                {
                    for (point_index = 0; point_index < 6; point_index++) {
                        world_points[0][point_index][2] -= 0.5f;
                        world_points[4][point_index][2] += 0.5f;
                    }
                    for (point_index = 0; point_index < 5; point_index++) {
                        world_points[point_index][0][0] -= 0.5f;
                        world_points[point_index][5][0] += 0.5f;
                    }
                }
            }
            __builtin_memcpy(column_alphas, D_004DA6E0,
                             sizeof(column_alphas));
            __builtin_memcpy(row_alphas, D_004DA6E8, sizeof(row_alphas));
            xglMatrixStackUnit();
            xglMatrixStackSave(projection_matrix);
            xglRotTransPersN(screen_points, projection_matrix, world_points, 30, 0);
            {
                for (point_index = 0; point_index < 5; point_index++) {
                    unsigned char *color;
                    line00_3[point_index].vertex = screen_points[point_index][1];
                    color = line00_3[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line00_3[point_index].vertex.terminator = 1;
                }
                line00_3[5].vertex.terminator = 0;

                {
                for (point_index = 0; point_index < 5; point_index++) {
                    unsigned char *color;
                    line01_4[point_index].vertex = screen_points[point_index][2];
                    color = line01_4[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line01_4[point_index].vertex.terminator = 1;
                }
                (line01_4 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < 5; point_index++) {
                    unsigned char *color;
                    line02_5[point_index].vertex = screen_points[point_index][3];
                    color = line02_5[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line02_5[point_index].vertex.terminator = 1;
                }
                (line02_5 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < 5; point_index++) {
                    unsigned char *color;
                    line03_6[point_index].vertex = screen_points[point_index][4];
                    color = line03_6[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line03_6[point_index].vertex.terminator = 1;
                }
                (line03_6 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < 6; point_index++) {
                    unsigned char *color;
                    line10_7[point_index].vertex = screen_points[1][point_index];
                    color = line10_7[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line10_7[point_index].vertex.terminator = 1;
                }
                (line10_7 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < 6; point_index++) {
                    unsigned char *color;
                    line11_8[point_index].vertex = screen_points[2][point_index];
                    color = line11_8[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line11_8[point_index].vertex.terminator = 1;
                }
                (line11_8 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < 6; point_index++) {
                    unsigned char *color;
                    line12_9[point_index].vertex = screen_points[3][point_index];
                    color = line12_9[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line12_9[point_index].vertex.terminator = 1;
                }
                (line12_9 + point_index)->vertex.terminator = 0;
                }
            }
            endPrintExtFunc(0xFFFFF0, 0xB, line00_3);
            endPrintExtFunc(0xFFFFF1, 0xB, line01_4);
            endPrintExtFunc(0xFFFFF2, 0xB, line02_5);
            endPrintExtFunc(0xFFFFF3, 0xB, line03_6);
            endPrintExtFunc(0xFFFFF4, 0xB, line10_7);
            endPrintExtFunc(0xFFFFF5, 0xB, line11_8);
            endPrintExtFunc(0xFFFFF6, 0xB, line12_9);
            __builtin_memcpy(quad_words, D_004C2720, sizeof(quad_words));
            endPrintExtFunc(0, 0x6A, NULL);
            endPrintExtFunc(0, 0x6C, NULL);
            if (*message_time == 0) {
                eMessageSet(&msg_11, msg00_14[no_13]);
                ENDOU_MODEL_MESSAGE->mode = 0x24;
                *message_time = 0x40;
                no_13 = (no_13 + 1) & 3;
            }
            (*message_time)--;
            ENDOU_MODEL_MESSAGE->x = 0;
            ENDOU_MODEL_MESSAGE->draw_parameter = 0xFFFFF0;
            ENDOU_MODEL_MESSAGE->y = 0;
            eMessageMain(&msg_11);
            endPrintExtFunc(0, 0x6B, NULL);
            bsp_12.uv[0] = 0.0f;
            bsp_12.uv[2] = 0.234375f;
            bsp_12.uv[3] = 0.046875f;
            bsp_12.uv[1] = 0.0f;
            xglRotTransPersN(bsp_12.vertices, NULL, (Vector4 *)quad_words, 4, 0);
            endPrintExtFunc(0, 0x6D, &bsp_12);
            xglSleep();
            }
        }
    }
    }
}

extern void eBattleWinOpen(EndouBattleWindowParams *params);
extern int eBattleWinMain(EndouBattleWindowParams *params);
extern void eBattleWinOpen2(EndouBattleRewardParams *params);
extern int eBattleWinMain2(void);
extern void eBattleWinOpen3(EndouBattleAnalysisParams *params);
extern int eBattleWinMain3(void);
extern void eBattleWinOpen4(EndouBattleMessageParams *params);
extern int eBattleWinMain4(void);

void BattleWindowTest(void)
{
    EndouBattleWindowParams window;
    EndouBattleMessageParams message;
    EndouBattleAnalysisParams analysis;
    EndouBattleRewardParams reward;

    window.x = 16;
    window.style = 0x00fffffd;
    window.title = tag_16[0];
    window.commands = data_15;
    window.y = 144;
    window.information_y = 160;
    window.information_x = 0;
    xglRenderClearFrame();
    xglSleep();
    xglCdLoadOverlay(1);
    eBattleWinInit();

    if ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
    do {
        xglFontDebugPrintf(0, 0, D_004C2A70);
        if ((PadData.buttons.input.pressed & 0x10U) != 0) {
            message.x = 16;
            message.y = 32;
            message.title = D_004DA720;
            message.style = 0x00fffffd;
            message.text = text_17;
            eBattleWinOpen4(&message);
        }
        if ((PadData.buttons.input.pressed & 0x80U) != 0 &&
            eBattleWinPageCheck4() != 0)
            eBattleWinClose4();
        eBattleWinMain4();
        if ((PadData.buttons.input.pressed & 0x10U) != 0) {
            analysis.x = 64;
            analysis.experience = 0x2107a;
            analysis.y = 256;
            analysis.conditions = 0x208;
            analysis.hp = 100;
            analysis.max_hp = 200;
            analysis.technique_points = 495;
            analysis.ether_points = 689;
            analysis.skill_points = 800;
            analysis.money = 0xdac0;
            analysis.common_drop = 5;
            analysis.rare_drop = 10;
            analysis.style = 0x00fffff0;
            analysis.name = D_004C2A88;
            eBattleWinOpen3(&analysis);
        }
        if ((PadData.buttons.input.pressed & 0x80U) != 0)
            eBattleWinClose3();
        eBattleWinMain3();
        if ((PadData.buttons.input.pressed & 4U) != 0) {
            reward.width = 0x800000b4;
            reward.x = 16;
            reward.y = 32;
            reward.text = msg_19[count_18];
            reward.style = 0x00fffff0;
            reward.title = tag_20;
            eBattleWinOpen2(&reward);
            count_18 ^= 1;
        }
        if ((PadData.buttons.input.pressed & 1U) != 0)
            eBattleWinClose2();
        eBattleWinMain2();
        if ((PadData.buttons.input.pressed & 8U) != 0)
            eBattleWinOpen(&window);
        if ((PadData.buttons.input.pressed & 2U) != 0)
            eBattleWinClose(&window);
        window.repeat = PadData.buttons.input.repeat;
        xglFontDebugPrintf(0, 32, D_004DA730, PadData.buttons.input.repeat);
        eBattleWinMain(&window);
        xglSleep();
    } while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", eNumberTest);

void eModelTestMain(EModelTestState *unit)
{
    if (unit->isReady == 0) {
        return;
    }

    if (unit->extFuncFlag == 0) {
        MenuModelUnitOpen(unit, 2);
        unit->extFuncFlag = 20;
    }

    MenuModelControl(unit);
}

void eModelTest(void)
{
    int model_id;

    xglCdLoadOverlay(1);
    ACT_init();
    xglRenderClearFrame();
    xglSleep();
    MenuLoadInit();
    MenuModelInit((void *)0x01000000);
    MenuModelMemoryInit((void *)0x01100000, 0x00500000);
    MenuModelMemorySet(0);
    MenuModelCreate(&model_id, 29);
    MenuModelExtFuncSet(model_id, eModelTestMain, 0);

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2AE8);
        MenuModelMain();
        xglSleep();
    }
}

void ePrintTest(void)
{
}

void eMessageTest(void)
{
    EMessageTestWindow windows[2];
    EMessageTestWindow current;
    int message_index = 0;

    WindowTexLoad(0, 0);
    endPrintInit();
    eMessageSet(&windows[0], msg_45[0]);
    eMessageSet(&windows[1], msg_45[0]);
    windows[1].mode = 0x20;
    eMessageSet(&current, text00_46);
    current.page_state = 1;
    eMessageTextChange(&current, text00_46);

    PadData.buttons.input.pressed = 0;
    current.message_x = 0x40;
    current.message_y = 300;
    current.message_color = 0x00fffff0;
    current.mode = 0x24;
    ClearEnv.color_g = 0x40;
    ClearEnv.color_r = 0;
    ClearEnv.color_b = 0;
    ClearEnv.color_a = 0;

    if ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        do {
            xglFontDebugPrintf(0, 0, D_004C2C70);
            if ((PadData.buttons.input.pressed & 0x20U) != 0) {
                eMessageNextPage(&current, 0);
            }
            if ((PadData.buttons.input.pressed & 0x100U) != 0) {
                message_index += 1;
                if (message_index >= 5) {
                    message_index = 0;
                }
                windows[0].message_text = msg_45[message_index];
            }
            if ((PadData.buttons.input.pressed & 8U) != 0) {
                windows[0].mode = 0x22;
                windows[1].mode = 0x22;
                eMessageNextPage(&current, 1);
                current.mode = 0x22;
            }
            if ((PadData.buttons.input.pressed & 2U) != 0) {
                windows[0].mode = 0x70;
                windows[1].mode = 0x70;
                current.mode = 0x70;
            }
            if ((PadData.buttons.input.held & 4U) != 0) {
                windows[0].selection_value -= 1;
            }
            if ((PadData.buttons.input.held & 1U) != 0) {
                windows[0].selection_value += 1;
            }
            if ((PadData.buttons.input.held & 0x1000U) != 0) {
                windows[0].debug_cursor -= 1;
            }
            if ((PadData.buttons.input.held & 0x4000U) != 0) {
                windows[0].debug_cursor += 1;
            }

            xglFontDebugHex(0, 0x10, windows[0].debug_value, 2);
            windows[0].message_color = 0x01ffffff;
            windows[1].message_color = 0x01ffff00;
            windows[0].message_x = 100;
            windows[0].message_y = 100;
            windows[1].message_x = 0xa4;
            windows[1].message_y = 0xa4;
            eMessageDraw(&windows[1]);
            eMessageMain(&current);
            endPrintExtFunc(0x00ffffff, 100, 0);
            xglSleep();
        } while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U);
    }
}

void BgTest(void)
{
    XglPacket *packet;
    int packet_command = BG_TEST_COMMAND_BASE << BG_TEST_COMMAND_BASE_SHIFT;

    packet_command |= sRender.packetControl << BG_TEST_CONTROL_SHIFT;
    TestEnv_47.packet_write.value = packet_command |
        ((u64)BG_TEST_COMMAND_HIGH << BG_TEST_COMMAND_HIGH_SHIFT);
    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, &TestEnv_47, 10, 0, 0, 0);
}

typedef struct TexturTestHeader {
    u8 unmodeled_00[8];
    int entry_count;
    u32 entries_offset;
} TexturTestHeader;



typedef struct TexturTestEntry {
    u32 width_and_pitch;
    u32 height;
    u32 vram_offset;
    int data_quadwords;
    u32 data_offset;
} TexturTestEntry;



typedef struct TexturTestUploadEnvironment {
    u8 unmodeled_00[0x20];
    u64 destination_parameters;
    u8 unmodeled_28[0x18];
    u64 transfer_dimensions;
} TexturTestUploadEnvironment;



typedef struct TexturTestDrawEnvironment {
    u8 unmodeled_00[0x50];
    u64 texture_parameters;
} TexturTestDrawEnvironment;



extern TexturTestUploadEnvironment eNumberTexEnv_48;
extern TexturTestDrawEnvironment TestEnv_52;
extern int x_53;
extern const char D_004C2C88[];
extern const char D_004C2CA0[];
extern const char D_004C2CB8[];
extern XglPacket *xglPacketGetCurrent(void);
extern void xglRenderClearDepth(void);
extern void FlushCache(int mode);

void TexturTest(void)
{
    u8 *work_start = WorkEnd;
    TexturTestHeader *texture_file =
        (TexturTestHeader *)(((u32)work_start + 15U) & 0xfffffff0U);
    int size;
    u8 *texture_end;
    void *second_file;

    size = xglCdReadFile(D_004C2C88, texture_file, 0, 0);
    if (size < 0)
        return;
    texture_end = (u8 *)texture_file + size;
    second_file = (void *)(((u32)texture_end + 15U) & 0xfffffff0U);
    WorkEnd = texture_end;
    size = xglCdReadFile(D_004C2CA0, second_file, 0, 0);
    if (size < 0)
        return;
    WorkEnd = (u8 *)second_file + size;
    xglRenderClearDepth();

    for (;;) {
        int index;
        XglPacket *packet;
        TexturTestEntry *entry;
        u16 repeated;

        BgTest();
        xglFontDebugPrintf(0, 0, D_004C2CB8);
        if ((PadData.buttons.button_state & 0x08000100U) == 0x08000100U)
            break;
        index = 0;
        packet = xglPacketGetCurrent();
        entry = (TexturTestEntry *)
            ((u8 *)texture_file + texture_file->entries_offset);
        if (texture_file->entry_count > 0) {
            TexturTestUploadEnvironment *upload_environment = &eNumberTexEnv_48;

            do {
                int width = entry->width_and_pitch & 0xffffU;
                u32 pitch = entry->width_and_pitch >> 16;

                if (entry->width_and_pitch == width)
                    pitch = (width + 63) >> 6;
                upload_environment->transfer_dimensions =
                    (u32)width | ((u64)entry->height << 32);
                upload_environment->destination_parameters =
                    ((u64)((entry->vram_offset + 0xe0000U) >> 6) << 32) |
                    ((u64)pitch << 48);
                FlushCache(0);
                sceVif1PkRef(packet, upload_environment, 6, 0, 0, 0);
                {
                    u8 *transfer_data = (u8 *)texture_file + entry->data_offset;
                    int transfer_quadwords = entry->data_quadwords;

                    entry++;
                    sceVif1PkRef(packet, transfer_data,
                                 transfer_quadwords, 0, 0, 0);
                }
                index++;
            } while (index < texture_file->entry_count);
        }
        xglFontDebugHex(0, 0x20, x_53, 5);
        repeated = PadData.buttons.input.repeat;
        if ((repeated & 0x2000U) != 0)
            x_53 += 0x100;
        if ((repeated & 0x8000U) != 0)
            x_53 -= 0x100;
        TestEnv_52.texture_parameters =
            ((u64)(((u32)x_53 + 0xe0000U) >> 6) << 37) |
            0x2000000629343800ULL;
        sceVif1PkRef(packet, &TestEnv_52, 12, 0, 0, 0);
        xglSleep();
    }
    xglRenderClearFrame();
    WorkEnd = work_start;
}

void UmnPrintTest(void)
{
}

void UmnMailDispTest(void)
{
    UmnMailDisplayParams windows[2];

    tyaUmlDispInit();
    tyaUmlDispParamReset(&windows[0], 0);
    tyaUmlDispInit();
    tyaUmlDispParamReset(&windows[1], 0);

    windows[0].mode = 2;
    windows[0].texture_x = 0x710;
    windows[0].texture_y = 0x730;
    windows[0].texture_width = 0x1e0;
    windows[0].texture_height = 0xc8;
    windows[0].view_width = 0x1e0;
    windows[0].view_height = 0x180;

    windows[1].mode = 3;
    windows[1].texture_x = 0x710;
    windows[1].texture_y = 0x808;
    windows[1].texture_width = 0x1e0;
    windows[1].texture_height = 0xc8;
    windows[1].view_width = 0x1e0;
    windows[1].view_height = 0x180;

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2CD0);
        if ((PadData.buttons.input.repeat & 0x1000U) != 0) {
            windows[0].scroll_y -= 8;
        }
        if ((PadData.buttons.input.repeat & 0x4000U) != 0) {
            windows[0].scroll_y += 8;
        }
        if ((PadData.buttons.input.repeat & 0x8000U) != 0) {
            windows[0].scroll_x -= 8;
        }
        if ((PadData.buttons.input.repeat & 0x2000U) != 0) {
            windows[0].scroll_x += 8;
        }
        if ((PadData.buttons.input.pressed & 8U) != 0) {
            windows[0].mode = 0;
        }
        if ((PadData.buttons.input.pressed & 2U) != 0) {
            windows[0].mode = 1;
        }
        if ((PadData.buttons.input.pressed & 0x10U) != 0) {
            windows[0].mode = 2;
        }
        if ((PadData.buttons.input.pressed & 0x20U) != 0) {
            windows[0].mode = 3;
        }
        if ((PadData.buttons.input.pressed & 0x80U) != 0) {
            windows[0].mode = 4;
        }
        if ((PadData.buttons.input.pressed & 0x40U) != 0) {
            windows[0].mode = 5;
        }
        tyaUmlDispMain(&windows[0]);
        tyaUmlDispMain(&windows[1]);
        xglSleep();
    }
}

static void e_test(ETestContext *context)
{
    sceVif1PkCloseDirectHLCode(context->packet);
    sceVif1PkRef(context->packet, (unsigned char *)&WinTexEnv_65,
                 7, 0, 0, 0);
    sceVif1PkRef(context->packet, (unsigned char *) test_data + 0x30, 0x4002, 0, 0, 0);
    sceVif1PkCnt(context->packet, 0);
    sceVif1PkOpenDirectHLCode(context->packet, 0);
    sceVif1PkAddDirectDataN(context->packet, TestEnv_66, 8);
    xglFontReloadTexture(context, 0);
}

void ReLoadTest(void)
{
    xglRenderClearFrame();
    xglSleep();
    test_data = (void *)(((u32)WorkEnd + 0x0fU) & 0xfffffff0U);
    WorkEnd = (u8 *)test_data +
        xglCdReadFile(D_004C2CF0, test_data, 0, 0);

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2D08);
        xglFontPrint(0x40, 0x28, 0x00ffffff, D_004C2D18);
        xglFontPrintExtFunc(0x01ffffffU, e_test, 0);
        xglFontPrint(0x40, 0x48, 0x02ffffff, D_004C2D18);
        xglSleep();
    }
}

static void endCallback(int result, int amount)
{
    switch (result) {
    case 0: {
        const char *source = (const char *)amount;
        char *destination = fname_67;
        do {
            *destination = *source++;
        } while (((unsigned int)*destination++ << 24) != 0);
        break;
    }
    case 1:
        size_68 = I2F(amount) * progress_percent;
        break;
    case 2:
    case 3:
        if (result != 3)
            xglSleep();
        break;
    case -1: {
        for (;;) {
            xglFontDebugPrintf(8, 216, not_found_format, fname_67);
            xglSleep();
        }
    }
    case -2: {
        for (;;) {
            xglFontDebugPrintf(8, 216, read_error_format, fname_67);
            xglSleep();
        }
    }
    case 4:
        break;
    default:
        break;
    }
}




extern XglPacket *xglPacketGetCurrent(void);

void FileLoadTest(void)
{
    void (*previous_callback)(int, int);
    unsigned int pad_mask;
    u64 pad_state;
    u64 *pad_words;

    xglRenderClearFrame();
    xglSleep();
    files.buffer = (void *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = (u8 *)files.buffer + 0x100000;
    previous_callback = xglCdSetCallback(endCallback);
    xglCdReadFile(D_004C2D80, files.buffer, 0, 0);
    files.state = 1U;
    pad_words = (u64 *)&PadData;
    pad_mask = 0x08000100U;
    pad_state = pad_words[5];

    if ((pad_state & pad_mask) != pad_mask)
    do {
        xglFontDebugPrintf(0, 0, D_004C2D98);
        if (files.state == 1) {
            XglPacket *packet = xglPacketGetCurrent();

            sceVif1PkRef(packet, eNumberTexEnv_69, 6, 0, 0, 0);
            sceVif1PkRef(packet, (u8 *)files.buffer + 0x30, 0x4002, 0, 0, 0);
            xglFontDebugHex(0, 0x20, x_71, 5);
            if ((PadData.buttons.input.repeat & 0x2000U) != 0) {
                x_71 += 0x100;
            }
            if ((PadData.buttons.input.repeat & 0x8000U) != 0) {
                x_71 -= 0x100;
            }
            TestEnv_70[6] = ((u64)((x_71 + 0xf0000U) >> 6) << 37) | 0x2000000669343c00ULL;
            sceVif1PkRef(packet, TestEnv_70, 10, 0, 0, 0);
        }
        xglSleep();
    } while ((PadData.buttons.button_state & pad_mask) != pad_mask);
    xglCdSetCallback(previous_callback);
}

void JpegTest(void)
{
    JpegTestDisplay display;
    JpegTestDecodeRequest request;
    u8 *source_buffer;
    u8 *destination_buffer;

    source_buffer = (u8 *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = source_buffer + 0x80000;
    memset(source_buffer, 0, 0x80000);

    destination_buffer = (u8 *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = destination_buffer + 0x80000;
    memset(destination_buffer, 0, 0x80000);

    GameSnapShotSaveThumbnail(100, source_buffer);

    memset(&request, 0, sizeof(request));
    request.source = source_buffer;
    request.destination = destination_buffer;
    request.source_width = 0;
    request.source_height = 0;
    request.max_width = 0;
    request.max_height = 0;
    xglJpegDecode(&request);

    display.x = 100;
    display.y = 100;
    display.color = 0x00ffffffU;
    display.alpha = -0x80;
    display.blue = -0x80;
    display.green = -0x80;
    display.red = -0x80;
    display.image_data = destination_buffer;

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2DA8);
        endPrintExtFunc(0, 0x0d, &display);
        xglSleep();
    }
}

void UmlTest(void)
{
    unsigned int source_byte;
    unsigned long character;
    unsigned char *file_buffer;
    const char *text;
    unsigned char *output;
    unsigned char *read_buffer;
    const char *file_name;

    xglRenderClearFrame();
    xglSleep();
    file_buffer = (unsigned char *)(((unsigned int)WorkEnd + 127) ^ 127);
    WorkEnd = file_buffer + 0x20000;
    buffer_72 = file_buffer;
    memset(file_buffer, 0, 0x10000);
    file_name = D_004C2DB8;
    read_buffer = file_buffer;
    xglCdReadFile(file_name, read_buffer, 0, 0);
    output = (unsigned char *)mm_73;
    text = (const char *)buffer_72;
    source_byte = (unsigned char)*text;
    goto test_character;
    do {
        character &= 0xffUL;
        if (character == '\\') {
            unsigned char digits[16];
            unsigned char value;
            int digit_count;
            int place;
            int index;
            int trailing_index;
            unsigned char trailing_count;
            unsigned char trailing_fields;
            unsigned char trailing_value;

            ++text;
            source_byte = (unsigned char)*text;
            if (source_byte != 't')
                goto test_character;
            text += 2;
            digit_count = 0;
            value = 0;
            while ((unsigned char)((unsigned char)*text - '0') < 10) {
                digits[digit_count++] = (unsigned char)*text++ - '0';
            }
            place = 1;
            for (index = 0; index < digit_count - 1; ++index)
                place *= 10;
            for (index = 0; index < digit_count; ++index) {
                value += digits[index] * place;
                place /= 10;
            }
            *output++ = value;
            ++text;
            *output++ = '\t';
            trailing_fields = 2;
            trailing_count = 0;
            for (; trailing_fields != 0; --trailing_fields) {
                while ((unsigned char)((unsigned char)*text - '0') < 10) {
                    digits[trailing_count] = (unsigned char)*text - '0';
                    ++trailing_count;
                    ++text;
                }
                place = 1;
                for (trailing_index = 0; trailing_index < trailing_count - 1; ++trailing_index)
                    place *= 10;
                trailing_value = 0;
                for (trailing_index = 0; trailing_index < trailing_count; ++trailing_index) {
                    trailing_value += digits[trailing_index] * place;
                    place /= 10;
                }
                ++text;
            }
        } else {
            *output = source_byte;
            ++output;
            ++text;
        }
        source_byte = (unsigned char)*text;
test_character:
        character = source_byte;
    } while (character != 0);
    *output = 0;
    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2DD0);
        xglFontPrint(0, 64, 0x00fffff0, D_004C2DE0);
        xglFontPrint(0, 64, 0x00ffffff, mm_73);
        xglSleep();
    }
}

void PauseMenuPagePartyDebugTakeAgws(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_74 -= 1;
        break;
    case 0x2000:
        Select_74 += 1;
        break;
    case 0x1000:
        Select_74 -= 2;
        break;
    case 0x4000:
        Select_74 += 2;
        break;
    default:
        break;
    }

    if (Select_74 < 0) {
        Select_74 += 0xc;
    }
    Select_74 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyTakeAgwsCheck(Select_74 + 0x11) != 0) {
            PartyTakeAgwsOff(Select_74 + 0x11);
        } else {
            PartyTakeAgwsOn(Select_74 + 0x11);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2DF0);
    xglFontDebugPrintf((Select_74 % 2) * 0x80 + 8, (Select_74 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 0x11));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x68, (i / 2) * 0x10 + 0x30, text00_75[PartyTakeAgwsCheck(i + 0x11)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E08);
}

void PauseMenuPagePartyDebugLockParty(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_76 -= 1;
        break;
    case 0x2000:
        Select_76 += 1;
        break;
    case 0x1000:
        Select_76 -= 2;
        break;
    case 0x4000:
        Select_76 += 2;
        break;
    default:
        break;
    }

    if (Select_76 < 0) {
        Select_76 += 0xc;
    }
    Select_76 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyLockPartyCheck(Select_76 + 1) != 0) {
            PartyLockPartyOff(Select_76 + 1);
        } else {
            PartyLockPartyOn(Select_76 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2E38);
    xglFontDebugPrintf((Select_76 % 2) * 0x80 + 8, (Select_76 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_77[PartyLockPartyCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E58);
}

void PauseMenuPagePartyDebugOutFriend(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_78 -= 1;
        break;
    case 0x2000:
        Select_78 += 1;
        break;
    case 0x1000:
        Select_78 -= 2;
        break;
    case 0x4000:
        Select_78 += 2;
        break;
    default:
        break;
    }

    if (Select_78 < 0) {
        Select_78 += 0xc;
    }
    Select_78 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyOutFriendCheck(Select_78 + 1) != 0) {
            PartyOutFriendOff(Select_78 + 1);
        } else {
            PartyOutFriendOn(Select_78 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2E88);
    xglFontDebugPrintf((Select_78 % 2) * 0x80 + 8, (Select_78 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_79[PartyOutFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E58);
}

void PauseMenuPagePartyDebugFriend(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_80 -= 1;
        break;
    case 0x2000:
        Select_80 += 1;
        break;
    case 0x1000:
        Select_80 -= 2;
        break;
    case 0x4000:
        Select_80 += 2;
        break;
    default:
        break;
    }

    if (Select_80 < 0) {
        Select_80 += 7;
    }
    Select_80 %= 7;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyFriendCheck(Select_80 + 1) != 0) {
            PartyFriendOff(Select_80 + 1);
        } else {
            PartyFriendOn(Select_80 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2EA8);
    xglFontDebugPrintf((Select_80 % 2) * 0x80 + 8, (Select_80 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 7; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_81[PartyFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2EC0);
}

void PauseMenuPagePartyDebugAttacker(void)
{
    PartyDataView *party;
    int character;
    int position;
    int i;

    if (PadData.buttons.input.repeat & 0x1000) {
        if (Select_82 == 0) {
            Select_82 = 5;
        } else {
            Select_82 -= 1;
        }
    }
    if (PadData.buttons.input.repeat & 0x4000) {
        if (Select_82 == 5) {
            Select_82 = 0;
        } else {
            Select_82 += 1;
        }
    }

    if (Select_82 < 3) {
        party = (PartyDataView *)PartyDataGet();
        character = party->attack_slots[Select_82].character;
        if (PadData.buttons.input.repeat & 0x8000) {
            character = (character != 0) ? character - 1 : 0x1d;
        }
        if (PadData.buttons.input.repeat & 0x2000) {
            character = (character != 0x1d) ? character + 1 : 0;
        }
        party = (PartyDataView *)PartyDataGet();
        party->attack_slots[Select_82].character = character;
    } else {
        party = (PartyDataView *)PartyDataGet();
        position = party->attack_slots[Select_82 - 3].position;
        if (PadData.buttons.input.repeat & 0x8000) {
            position = (position != 0) ? position - 1 : 9;
        }
        if (PadData.buttons.input.repeat & 0x2000) {
            position = (position != 9) ? position + 1 : 0;
        }
        party = (PartyDataView *)PartyDataGet();
        party->attack_slots[Select_82 - 3].position = position;
    }

    party = (PartyDataView *)PartyDataGet();
    xglFontDebugPrintf(8, 0x20, D_004C2F10);
    xglFontDebugPrintf(8, Select_82 * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 3; i++) {
        xglFontDebugPrintf(0x10, i * 0x10 + 0x30, text01_84[i]);
        xglFontDebugPrintf(0x30, i * 0x10 + 0x30, text02_85[0]);
        xglFontDebugPrintf(0x70, i * 0x10 + 0x30, MenuCharNameGet(party->attack_slots[i].character));
    }
    for (i = 0; i < 3; i++) {
        xglFontDebugPrintf(0x10, i * 0x10 + 0x60, text01_84[i]);
        xglFontDebugPrintf(0x30, i * 0x10 + 0x60, text02_85[1]);
        xglFontDebugPrintf(0x70, i * 0x10 + 0x60, text00_83[party->attack_slots[i].position]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2F30);
}

void PauseMenuPagePartyDebug(void)
{
    xglFontDebugPrintf(8, 0x10, D_004C2F68);
    xglFontDebugPrintf(8, 0xb0, D_004C2F90);

    if (PadData.buttons.input.repeat & 0x80) {
        if (TopMode_86 == 4) {
            TopMode_86 = 0;
        } else {
            TopMode_86 += 1;
        }
    }

    switch (TopMode_86) {
    case 0:
        PauseMenuPagePartyDebugFriend();
        return;
    case 1:
        PauseMenuPagePartyDebugAttacker();
        return;
    case 2:
        PauseMenuPagePartyDebugOutFriend();
        return;
    case 3:
        PauseMenuPagePartyDebugLockParty();
        return;
    case 4:
        PauseMenuPagePartyDebugTakeAgws();
        break;
    }
}

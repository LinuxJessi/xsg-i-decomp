#include "common.h"
#include "main/menu_1.h"
#include "main/menu_top_command_check.h"
#include "main/xgl_render.h"
#include "main/xgl_packet.h"
#include "main/xgl_thread.h"
#include "main/party.h"
#include "main/game_snap_shot.h"

#include "shared.h"

#include "main/xgl_task.h"

#include "menu_shop.h"

static MenuShopWorkData *MenuShopWork;

/* Referenced original data recovered from this translation unit. */

const char D_004C7858[40] = "List                   Price";

extern const char D_004DB230[];

typedef struct ShopDataCategory {
    unsigned short count;
    unsigned char unmodeled_02[30];
    unsigned short slots[4];
} ShopDataCategory;



typedef union ShopDataTail {
    struct {
        unsigned short count;
        unsigned char unmodeled_02[6];
    };
    unsigned short agwsSlots[4];
} ShopDataTail;



typedef struct MenuShopListRow {
    const char *name;
    int amount;
    unsigned char flag;
} MenuShopListRow;



typedef struct MenuShopWindowDX {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char unmodeled_11[0x183];
} MenuShopWindowDX;



typedef struct MenuShopMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x0c];
    const char *text;
} MenuShopMessage;



typedef struct MenuShopEx2Work {
    unsigned char unmodeled_00[4];
    int color;
    MenuShopWindowDX window;
    MenuShopMessage message;
} MenuShopEx2Work;



typedef struct MenuShopWeapon {
    unsigned short shopParameter1;
    unsigned char unmodeled_02[4];
    unsigned short flags;
    unsigned char unmodeled_08[0x0A];
    unsigned short wagl;
    unsigned char unmodeled_14[6];
    short bullet;
    unsigned short shopParameter2;
} MenuShopWeapon;



/* Nine 0x28-byte categories and the final eight-byte slot/count view share
 * the original 0x170-byte storage extent. */
typedef struct {
    ShopDataCategory records[9];
    ShopDataTail tail;
} MenuShopData;

static MenuShopData ShopData;

extern int MenuScenarioNo;

extern void *MenuSortAddrGet(int list);

extern int MenuSortGet(int list, int index);

extern MenuShopListRow *MenuListGet(int list);

extern int MenuSortCheck(int list);

extern int MenuBoxMoneyGet(int item, int mode);

extern int dataMoneyBoxChk(void);

extern int MenuBoxChk(int item);

int subMenuShopEquipCheck(int item, int category);

int MenuShopNoSaleCheck(int item);

extern void MenuSortSet(int list, int type, int order);

extern int dataEvtBoxChk(int event_id);

extern void MoveSlide(short *current, short *target, float rate);

extern void eMessageSet(void *message, const char *text);

extern void eMessageMain(void *message);

extern MenuShopWeapon *func_A1A3D8(int item);

extern const char **func_A2C738(int bullet);

typedef struct MenuShopWindowSP {
    unsigned char unmodeled_00;
    unsigned char row_count;
    unsigned char unmodeled_02[0x0A];
    short width;
    short height;
    const char *title;
    unsigned char columns;
    unsigned char rows;
    unsigned char unmodeled_16[0x06];
    struct MenuShopListRow *items;
    unsigned char unmodeled_20[0x06];
    unsigned char state;
} MenuShopWindowSP;



static MenuShopWindowSP *MenuShopWinSP;

extern unsigned char MenuKeepSelect[0x64];

extern MenuShopListRow *MenuListMake(int list, int mode);

void MenuShopSortSet(int list, int type, int order, int option);

/* Referenced original data recovered from this translation unit. */

#include "main/xgl_studio.h"

/* Referenced original data recovered from this translation unit. */

struct MenuShopPasMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x0c];
    const char *text;
    unsigned char unmodeled_1c[0x28];
};



struct MenuShopModelTransform {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    Vector4 rotation;
    unsigned char unmodeled_30[0x30];
    float fade;
};



struct MenuShopModelActor {
    unsigned char unmodeled_00[0xC0];
    struct MenuShopModelTransform transform;
};



struct MenuShopModelUnit {
    unsigned char unmodeled_00[0x10];
    signed char taskPhase;
    unsigned char active;
    unsigned char unmodeled_12;
    unsigned char state;
    unsigned char unmodeled_14[0x0C];
    struct MenuShopModelActor *actor;
};



struct MenuShopModelCallbackData {
    unsigned char unmodeled_00[0x10];
    unsigned char state;
    unsigned char unmodeled_11[0x0B];
    int modelId;
};



/* The map anchors nine 0x28-byte count records and a separate 8-byte tail.
 * The remaining bytes stay explicitly unmodeled. */

struct MenuShopAgwsListState {
    unsigned char unmodeled_00[0x160];
    unsigned short itemIds[4];
};



typedef struct AgwsModelTransform {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    Vector4 rotation;
    unsigned char unmodeled_30[0x60 - 0x30];
    float fade;
} AgwsModelTransform;

typedef struct AgwsModelActor {
    unsigned char unmodeled_00[0xC0];
    AgwsModelTransform transform;
} AgwsModelActor;

typedef struct AgwsModelOrg {
    unsigned char unmodeled_00[0x5A];
    signed char hand[3];
    unsigned char unmodeled_5d[0x5E - 0x5D];
    short weaponId[3];
} AgwsModelOrg;

typedef struct AgwsModelUnit {
    unsigned char unmodeled_00[0x10];
    signed char taskPhase;
    unsigned char active;
    unsigned char unmodeled_12[1];
    unsigned char state;
    struct AgwsModelOrg *org;
    unsigned char unmodeled_18[0x20 - 0x18];
    struct AgwsModelActor *actor;
} AgwsModelUnit;

struct AgwsModelUnit;
extern void MenuModelUnitBreak(struct AgwsModelUnit *unit);

extern void MenuModelUnitOpen(struct AgwsModelUnit *unit, int drawType);

void *func_A191C0(int);

/* Partial unit record used by the shop's repair and equipment operations. */
typedef struct MenuShopUnitData {
    unsigned char unmodeled_00[0x34];
    unsigned short hitPoints;
    unsigned char unmodeled_36[0x20];
    short engineId;
    short frameId;
    unsigned char unmodeled_5a[4];
    short weaponIds[3];
    short accessoryIds[3];
    short attachmentIds[3];
    unsigned char unmodeled_70[0x110];
} MenuShopUnitData;



struct MenuShopListWork {
    unsigned char state;
    unsigned char active;
    unsigned char initialFlags[2];
    int category;
    int color;
    MenuShopWindowSP window;
};



struct MenuShopSortOption {
    unsigned char type;
    unsigned char unmodeled_01[3];
    int order;
};



struct MenuShopSortOptionTable {
    struct MenuShopSortOption entries[9];
};



typedef struct MenuShopLineContext {
    unsigned char unmodeled_00[0x10];
    unsigned char mode;
} MenuShopLineContext;



typedef struct MenuShopLineRow {
    int text_id;
    unsigned char style[4];
    short position;
    short target;
} MenuShopLineRow;



typedef struct MenuShopLineDisplay {
    unsigned char unmodeled_00[3];
    unsigned char phase;
    int text_id;
    short top_position;
    MenuShopLineRow rows[5];
    int unmodeled_48;
} MenuShopLineDisplay;



struct MenuShopRowCountTable {
    unsigned char values[18];
};



extern int MenuShopListChange00(int list);

typedef signed char s8;



typedef int s32;



#define NULL ((void *)0)

typedef struct { unsigned char bytes[8]; } M2C_BLOCK8;



typedef struct MenuShopSortEntry {
    const short *itemIds;
    int highWord;
    int count;
} MenuShopSortEntry;



typedef struct MenuShopSortTable {
    MenuShopSortEntry entries[11];
} MenuShopSortTable;



extern const MenuShopSortTable D_004C7738;

struct MenuShopEquipRecord {
    unsigned char unmodeled_00[0x5e];
    short itemIds[3][3];
};



struct MenuShopEquipScratch {
    int partyIds[15];
    int slotIndex[9];
};



int PartyAgwsGet(int *);

typedef struct { unsigned char bytes[4]; } M2C_BLOCK4;



typedef union {
    struct {
        M2C_BLOCK8 first;
        M2C_BLOCK8 second;
        M2C_BLOCK4 third;
        u16 last;
    } blocks;
    s16 ids[11];
} MenuShopIconIds;



typedef long long s64;



/* The calculator and unit snapshots each copy a complete 0x180-byte record.
 * Only the statistics used by the shop are modeled here. */
typedef struct ShopCalculatedPara {
    s16 maxHp;
    s16 maxEp;
    unsigned short attack;
    unsigned short physicalDefense;
    unsigned short displayStat2;
    unsigned short magicDefense;
    unsigned char byteStats[3];
    unsigned char unmodeled_0f[0x25];
    unsigned short currentHp;
    unsigned char unmodeled_36[0x14A];
} ShopCalculatedPara;



ShopCalculatedPara *func_00A11108(int, int *, int *);

void MenuAgwsParaSet(int, int);

s32 MenuEngineEquipCheck(int, int);

s32 MenuFrameEquipCheck(s32, s32);

/* MenuShopNoSaleCheck reads the halfword at +0x4 (original VA 0x002a3c88). */
typedef struct MenuShopAccessoryData {
    unsigned char unmodeled_00[4];
    unsigned short flags;
    unsigned char unmodeled_06[6];
    unsigned char shopParameter2;
    unsigned char shopParameter3;
} MenuShopAccessoryData;



extern MenuShopAccessoryData *func_A1A548(int item);

#define s8 signed char

#define s32 int

#define s64 long long

int MenuBoxDec(unsigned int);

int MenuBoxInc(unsigned int);

s32 MenuCharHpCheck(s16);

s32 MenuCursorKeepCheck(void);

int MenuSelectMove(int, int, int);

int MenuSelectMove2(int, int, int);





int dataMoneyBoxDec(s32);

int dataMoneyBoxInc(s32);

void xglFontDebugHex(int, int, unsigned int, int);

void xglFontDebugPrintf(int, int, const char *, ...);

extern const char D_004C7A68[];

extern const char D_004C7A78[];

extern const char D_004C7A88[];

extern const char D_004C7A98[];

extern u8 D_004DB238[];

extern u8 D_004DB240[];

extern u8 D_004DB248[];

extern u8 D_004DB250[];

typedef struct MenuShopItemData {
    unsigned char unmodeled_00[4];
    unsigned short flags;
} MenuShopItemData;



typedef struct MenuShopAttackData {
    unsigned char unmodeled_00[10];
    unsigned short flags;
} MenuShopAttackData;



extern MenuShopWeapon *dataWpnGet(int item);

extern MenuShopAttackData *dataAttGet(int item);

extern MenuShopItemData *func_A1A4E8(int item);

#include "main/e_battle_win_open.h"
#include "main/open_close_main.h"
#include "main/e_number_main.h"
#include "main/xgl_sound.h"
extern PadPrefix PadData;


void WindowSPSet(WindowSPCursor *cursor);
void WindowSPMain(WindowSPCursor *cursor);
void WindowSPSelectJump(WindowSPCursor *cursor, int selection);










typedef struct MenuShopPasMessageSlot {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x0c];
    const char *text;
    unsigned char unmodeled_1c[0x28];
} MenuShopPasMessageSlot;



typedef struct MenuShopPasBounds {
    short x1;
    short y1;
    int color;
    short x2;
    short y2;
} MenuShopPasBounds;



typedef struct MenuShopPasWindow {
    WindowDX common;
    unsigned char unmodeled_30[0x194 - 0x30];
} MenuShopPasWindow;



typedef struct MenuShopPasData {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int color;
    MenuShopPasWindow window;
    MenuShopPasMessageSlot messages[16];
    MenuShopPasBounds bounds;
} MenuShopPasData;



/* Referenced original data recovered from this translation unit. */

extern int PartyTakeAgwsCheck(int id);

extern const char *D_004C74B0[4];

extern const char *D_004C7500[4];

extern const char *D_004C7520[3];

extern const char *D_004C7580[4];

extern const char *D_004C75A0[3];

extern const char *D_004C7650[8];

extern const char *D_004DB138[];

extern const char D_004DB140[];

extern const char D_004DB148[];

extern const char D_004DB150[];







typedef struct MenuShopInfoPanel {
    short textOffset[2];
    unsigned char unmodeled_04[8];
    const char *text;
} MenuShopInfoPanel;



typedef struct MenuShopInfoWork {
    unsigned char unmodeled_00[2];
    unsigned char flag;
    unsigned char unmodeled_03;
    int unmodeled_04;
    int ticker;
    WindowDX window;
    MenuShopInfoPanel panel;
    unsigned char unmodeled_1b0[416];
    char message[128];
} MenuShopInfoWork;





extern MenuTextEntry *func_A2C918(unsigned int itemId);

extern MenuTextEntry *func_A2C968(unsigned int itemId);

extern void eMessageCpy(void *message, const char *text);

extern void eMessageCat(const char *text);

extern const char *MenuCharNameGet(int unitId);

extern const char *MenuNumberTextGet(long value, int digits, int flag);

typedef struct MenuShopInfoT0 { const char *p[4]; } MenuShopInfoT0;



typedef struct MenuShopInfoT16 { const char *p[4]; } MenuShopInfoT16;



typedef struct MenuShopInfoT32 { const char *p[3]; } MenuShopInfoT32;



typedef struct MenuShopInfoT48 { const char *p[4]; } MenuShopInfoT48;



typedef struct MenuShopInfoT64 { const char *p[3]; } MenuShopInfoT64;



typedef struct MenuShopInfoT80 { const char *p[8]; } MenuShopInfoT80;



typedef struct MenuShopInfoT112 { const char *p[2]; } MenuShopInfoT112;



struct MenuShopListDisplay {
    unsigned char state;
    unsigned char active;
    unsigned char initialFlags[2];
    unsigned int category;
    int color;
    WindowSPCursor window;
};







extern void eSpriteSet(void *sprite, short spriteId);

extern void eSpriteMain(void *sprite);

struct MenuShopPortrait {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int style;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    unsigned char unmodeled_10[0x14];
    short spriteId;
};



struct MenuShopUnitPanel {
    unsigned char state;
    unsigned char active;
    unsigned char selectionFlags;
    unsigned char flags;
    int depth;
    WindowDXRibbon ribbon;
    BattleWinTagFont label;
    struct MenuShopPortrait portraits[7];
    unsigned char unmodeled_1b0[4];
    BattleWinCursor cursor;
};



extern const char D_004DB1B8[];

extern unsigned short MenuFaceEpidGet(int unitId, int smallPortrait);

struct MenuShopIconIds {
    short ids[11];
};



struct MenuShopIconSprite {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int depth;
    signed char red;
    signed char green;
    signed char blue;
    unsigned char alpha;
    unsigned char unmodeled_10[0x14];
    short spriteId;
};



struct MenuShopIconDisplay {
    signed char state;
    unsigned char unmodeled_01;
    unsigned char slide;
    unsigned char phase;
    short animation;
    short active;
    signed char categoryLift[9];
    unsigned char unmodeled_11[3];
    int depth;
    float scale;
    struct MenuShopIconSprite sprites[11];
};



extern const struct MenuShopIconIds D_004C79F8;

extern const float D_004D7E64;

extern const float D_004D7E68;

struct MenuShopParameterLabel {
    unsigned char state;
    unsigned char subState;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int depth;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
    unsigned char unmodeled_10[12];
    const char *text;
};



struct MenuShopParameterNumber {
    unsigned short x;
    unsigned short y;
    int depth;
    unsigned char color[4];
    unsigned char mode;
    unsigned char unmodeled_0d;
    unsigned char format;
    unsigned char digits;
    unsigned char unmodeled_10;
    unsigned char comparisonColor;
    unsigned char alignment;
    unsigned char firstOnly;
    MenuStatusValuePair value;
    unsigned char unmodeled_18[0x78];
};



struct MenuShopParameterDisplay {
    unsigned char state;
    unsigned char active;
    unsigned char count;
    unsigned char twoColumns;
    int depth;
    WindowDX window;
    struct MenuShopParameterLabel labels[9];
    struct MenuShopParameterNumber numbers[9];
};



struct MenuShopParameterDigits8 { unsigned char digits[8]; };



struct MenuShopParameterDigits7 { signed char digits[7]; };



extern const struct MenuShopParameterDigits8 D_004DB220[];

extern const struct MenuShopParameterDigits7 D_004DB228[];

extern const char *msg00_9[];

extern const char *msg01_10[];

static void MenuShopList(TskObject *task, struct MenuShopListDisplay *work);

static void MenuShopSelect(TskObject *task, void *data);

static void MenuShopEx02(TskObject *task, struct MenuShopUnitPanel *work);

static void MenuShopIcon(TskObject *task, struct MenuShopIconDisplay *work);

struct MenuShopExBullet {
    unsigned char unmodeled_00[4];
    int depth;
    WindowDX window;
    MenuShopMessage message;
};
struct MenuShopExHeader {
    WindowDXRibbon ribbon;
    BattleWinTagFont label;
    BattleWinNumber number;
};
struct MenuShopExPortrait {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int depth;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    unsigned char unmodeled_10[0x14];
    short spriteId;
};
struct MenuShopExParty {
    WindowDXRibbon ribbon;
    BattleWinTagFont label;
    struct MenuShopExPortrait sprites[7];
    unsigned char unmodeled_1a8[4];
};
struct MenuShopExDisplay {
    unsigned char state;
    unsigned char active;
    unsigned char flags;
    unsigned char slideFlags;
    int depth;
    struct MenuShopExHeader headers[2];
    struct MenuShopExParty parties[2];
    struct MenuShopExHeader equipment;
    struct MenuShopExBullet bullet;
};


struct MenuShopSelectConfirm {
    WindowDX window;
    MenuTopMenuPanel panel;
};
struct MenuShopSelectList {
    WindowDX window;
    MenuTopMenuPanel panel;
    MenuTopCommandEntry commands[8];
};
struct MenuShopSelectDisplay {
    unsigned char state;
    unsigned char unmodeled_01[2];
    unsigned char flags;
    int depth;
    struct MenuShopSelectList lists[2];
    struct MenuShopSelectConfirm confirmation[2];
};














extern const char *msg01_8_0036DBA0[];

extern const char D_004DB1A0[];

typedef struct MenuShopSeisan02Work {
    unsigned char state;
    unsigned char flag;
    unsigned char kind;
    unsigned char unmodeled_03;
    int ticker;
    WindowDX window;
    ETagFontMainData tags[2];
    ENumberMainData numbers[3];
    ESprite sprite;
    ERibbonPoint points[2];
    unsigned char unmodeled_972[4];
    int unmodeled_976;
    unsigned char unmodeled_980[4];
} MenuShopSeisan02Work;

struct MenuShopSaleSprite {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int style;
    unsigned char unmodeled_0c[0x18];
    short spriteId;
};

struct MenuShopSaleVertex {
    short x;
    short y;
    int depth;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
};

struct MenuShopSaleRibbon {
    short x;
    short y;
    int depth;
    short width;
    short height;
    signed char mode;
    unsigned char progress;
    unsigned char visible;
    unsigned char colorStyle;
    struct MenuShopSaleVertex points[6];
    unsigned char unmodeled_58[0x70 - 0x58];
};

struct MenuShopSalePanel {
    unsigned char state;
    unsigned char active;
    unsigned char flags;
    unsigned char selectionFlags;
    int depth;
    WindowDX window;
    BattleWinTagFont labels[4];
    BattleWinNumber numbers[6];
    struct MenuShopSaleSprite sprites[2];
    struct MenuShopSaleVertex quantityDivider[2];
    unsigned char unmodeled_5e4[4];
    int quantityDividerState;
    unsigned char unmodeled_5ec[4];
    struct MenuShopSaleVertex moneyDivider[2];
    unsigned char unmodeled_608[4];
    int moneyDividerState;
    unsigned char unmodeled_610[4];
    struct MenuShopSaleRibbon ribbon;
};

extern const char *msg00_6[];

extern const char *msg01_7[4];

typedef enum MenuShopListIndex {
    MENU_SHOP_LIST_NONE = -1
} MenuShopListIndex;

static inline MenuShopWorkData *MenuShopWorkRead(void)
{
    return *(MenuShopWorkData *const *)&MenuShopWork;
}

static inline MenuShopWindowSP *MenuShopWinSPRead(void)
{
    return *(MenuShopWindowSP *const *)&MenuShopWinSP;
}

typedef struct MenuShopSelectCommand {
    const char *text;
    unsigned char unmodeled_04[4];
    unsigned char locked;
    unsigned char unmodeled_09[3];
} MenuShopSelectCommand;

typedef struct MenuShopCommandPanel {
    unsigned char unmodeled_00[2];
    unsigned short displayMode;
    int cursor;
    unsigned char unmodeled_08[4];
    unsigned char unmodeled_0c[4];
    MenuShopSelectCommand *commands;
    unsigned char unmodeled_14[1508];
} MenuShopCommandPanel;

typedef enum MenuShopChoiceCursor {
    MENU_SHOP_CHOICE_FIRST = 0
} MenuShopChoiceCursor;

typedef struct MenuShopChoicePanel {
    unsigned short unmodeled_00;
    unsigned short displayMode : 16;
    MenuShopChoiceCursor cursor;
    const char *choices;
    const char *prompt;
    int unmodeled_10;
    void *unmodeled_14;
    unsigned char unmodeled_18[1504];
} MenuShopChoicePanel;

typedef struct MenuShopCommandPane {
    WindowDX window;
    MenuShopCommandPanel panel;
    MenuShopSelectCommand commands[8];
} MenuShopCommandPane;

typedef struct MenuShopChoicePane {
    WindowDX window;
    MenuShopChoicePanel panel;
} MenuShopChoicePane;

typedef struct MenuShopSelectWork {
    unsigned char unmodeled_00[3];
    unsigned char popupFlags;
    int depth;
    MenuShopCommandPane menus[2];
    MenuShopChoicePane choices[1];
} MenuShopSelectWork;

extern const char *msg00_1_0036DB30[8];

extern const char *msg01_2_0036DB50[4];

extern const char D_004DB168[];

extern const char D_004DB170[];

extern const struct MenuShopSortOptionTable D_004C77D8;

extern const struct MenuShopRowCountTable D_004C7820;

extern const char *msg00_4_0036DB60[];

static void MenuShopModelMain(TskObject *task, void *data);

static inline unsigned char *MenuShopAlignBuffer(unsigned char *buffer,
    unsigned int alignment)
{
    unsigned int address = (unsigned int)buffer;

    return (unsigned char *)((address + alignment - 1) & ~(alignment - 1));
}

static inline void MenuShopAppendPartyId(MenuShopWorkData *work, int *nextSlot,
    int **source)
{
    short *destination = &work->partyIds[*nextSlot];

    ++*nextSlot;
    *destination = (short)*(*source)++;
}

static inline void MenuShopAppendUnitId(MenuShopWorkData *work, int *nextSlot,
    int **source)
{
    short *destination = &work->unitIds[*nextSlot];

    ++*nextSlot;
    *destination = (short)*(*source)++;
}

void MenuShopAgwsListChange(void)
{
    int i;
    int next;

    for (i = 0; i < 4; i++) {
        if (ShopData.tail.agwsSlots[i] != 0 &&
            PartyTakeAgwsCheck(ShopData.tail.agwsSlots[i]) != 0)
            ShopData.tail.agwsSlots[i] = 0;
    }

    for (i = 0; i < 3; i++) {
        if (!ShopData.tail.agwsSlots[i]) {
            for (next = i + 1; next < 4; next++) {
                if (ShopData.tail.agwsSlots[next] != 0) {
                    ShopData.tail.agwsSlots[i] = ShopData.tail.agwsSlots[next];
                    ShopData.tail.agwsSlots[next] = 0;
                    break;
                }
            }
        }
    }
}

static void TskObjectSet3(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

static void tskTskMain3(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (MenuShopWork->state == 0xff) {
        xglTaskWaitRemove(&task->base);
        return;
    }

    if (task->state != 0) {
        if (task->state != 2)
            return;
    } else {
        worker(task, task->data);
        task->state = 2;
    }

    worker(task, task->data);
}

void MenuShopModelDisp(AgwsModelUnit *unit)
{
    AgwsModelActor *actor = unit->actor;
    AgwsModelTransform *transform;
    int state;

    if (actor == 0) {
        if (MenuShopWork->modelPending != 0) {
            MenuModelUnitBreak(unit);
            MenuShopWork->modelPending = 0;
        }
    }

    if (unit->active != 0) {
        transform = &actor->transform;
        if (MenuShopWork->modelPending != 0) {
            unit->state = 0x1e;
            MenuShopWork->modelPending = 0;
        }

        state = unit->state;
        switch (state) {
        case 10:
            break;
        case 0:
            transform->position.y = -0.2f;
            MenuModelUnitOpen(unit, 2);
            unit->state = 10;
            return;
        case 0x1e:
            MenuModelUnitOpen(unit, 1);
            unit->state = 0x1f;
            /* fall through */
        case 0x1f:
            if (transform->fade == 0.0f)
                unit->taskPhase = -1;
            break;
        default:
            break;
        }
    }
}


INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopModelMain);

typedef struct MenuShopHelpWork {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    WindowDX window;
    MenuShopPasMessageSlot messages[16];
    MenuShopPasBounds bounds;
} MenuShopHelpWork;



extern const char *msg00_0_0036DAF0[];
extern void endPrintExtFunc(int depth, int mode, void *vertices);

static void MenuShopPas(TskObject *task, void *data)
{
    MenuShopHelpWork *work = data;
    short messageX[16];
    short windowX;
    int i;
    WindowDX *window;
    short *target;
    const char **text;

    switch (task->state) {
    case 0:
        work->x = -272;
        work->y = 8;
        work->depth = 0xfffff0;
        window = &work->window;
        WindowDXSet((WindowDX *)(void *)(window));
        window->x = work->x;
        work->window.style = work->depth;
        work->window.width = 272;
        work->window.y = work->y;
        work->window.height = 32;
        work->window.state = 1;
        WindowDXMain((WindowDX *)(void *)(window));
        work->window.state = 3;
        text = msg00_0_0036DAF0;
        for (i = 0; i < 16; i++, text++) {
            eMessageSet(&work->messages[i], *text);
            work->messages[i].mode = 32;
            work->messages[i].x = 288;
            work->messages[i].color = work->depth + 2;
            work->messages[i].y = work->y + 3;
        }
        break;
    case 2:
        windowX = -16;
        for (i = 0; i < 16; i++)
            messageX[i] = 288;
        switch (MenuShopWork->state) {
        case 0x20:
            messageX[0] = 16;
            break;
        case 0x30:
        case 0x90:
        case 0xa0:
        case 0xa2:
        case 0xa4:
            messageX[MenuShopWork->category + 3] = 16;
            break;
        case 0xc0:
            messageX[12] = 16;
            break;
        case 0xc2:
        case 0xc4:
        case 0xc6:
            messageX[13] = 16;
            break;
        case 0xd0:
        case 0xd2:
        case 0xd4:
        case 0xd6:
            if (MenuShopWork->sort_mode_selector != 0)
                messageX[14] = 16;
            else
                messageX[15] = 16;
            break;
        default:
            windowX = -272;
            break;
        }
        window = &work->window;
        MoveSlide(&window->x, &windowX, 3.0f);
        WindowDXMain((WindowDX *)(void *)(window));
        work->bounds.x1 = window->x + 3;
        work->bounds.y1 = work->window.y + 3;
        work->bounds.x2 = work->window.width - 6;
        work->bounds.y2 = work->window.height - 6;
        work->bounds.color = work->depth;
        endPrintExtFunc(work->depth, 101, &work->bounds);
        target = messageX;
        for (i = 0; i < 16; i++, target++) {
            MoveSlide(&work->messages[i].x, target, 3.0f);
            if (work->messages[i].x < 256)
                eMessageMain(&work->messages[i]);
        }
        endPrintExtFunc(work->depth, 102, 0);
        break;
    }
}

static void MenuShopInfo(TskObject *task, MenuShopInfoWork *work)
{
    MenuShopInfoT0 tab0;
    MenuShopInfoT16 tab16;
    MenuShopInfoT32 tab32;
    MenuShopInfoT48 tab48;
    MenuShopInfoT64 tab64;
    MenuShopInfoT80 tab80;
    MenuShopInfoT112 tab112;
    short targetY;
    short targetH;
    MenuShopInfoPanel *panel;
    unsigned char *unit;
    short value;

    tab0 = *(MenuShopInfoT0 *)D_004C74B0;
    tab16 = *(MenuShopInfoT16 *)D_004C7500;
    tab32 = *(MenuShopInfoT32 *)D_004C7520;
    tab48 = *(MenuShopInfoT48 *)D_004C7580;
    tab64 = *(MenuShopInfoT64 *)D_004C75A0;
    tab80 = *(MenuShopInfoT80 *)D_004C7650;
    tab112 = *(MenuShopInfoT112 *)D_004DB138;

    switch (task->state) {
    case 0:
        work->ticker = 0x00fffff0;
        WindowDXSet((WindowDX *)(void *)(&work->window));
        work->window.x = -16;
        work->window.y = 480;
        work->window.width = 544;
        work->window.height = 54;
        work->window.style = work->ticker;
        work->window.close_callback = (void (*)(WindowDX *, void *))MenuInfoWindow;
        work->window.state = 1;
        panel = &work->panel;
        work->window.close_callback_arg = panel;
        WindowDXMain((WindowDX *)(void *)(&work->window));
        work->window.state = 3;
        panel->textOffset[0] = 0;
        work->panel.textOffset[1] = 0;
        work->panel.text = 0;
        work->flag = 0;
        work->unmodeled_04 = 0;
        memset(work->message, 0, 128);
        return;
    case 2:
        break;
    default:
        return;
    }

    targetY = 386;
    targetH = 54;
    switch (MenuShopWork->state) {
    case 32:
        eMessageCpy(work->message, tab0.p[MenuShopWork->mode]);
        break;
    case 48:
    case 144:
        if (MenuShopWork->itemId == 0) {
            eMessageCpy(work->message, D_004DB140);
        } else if ((MenuShopWork->itemId >> 16) == 7) {
            if (MenuShopWork->modelId >= 23)
                eMessageCpy(work->message, tab112.p[0]);
            else
                eMessageCpy(work->message, tab112.p[1]);
        } else {
            eMessageCpy(work->message, (const char *)MenuTextGet(MenuShopWork->itemId)->auxiliary);
        }
        break;
    case 160:
        if (MenuShopWork->itemId != 0) {
            eMessageCpy(work->message, tab16.p[2]);
            eMessageCat(tab16.p[3]);
        }
        break;
    case 162:
        if (MenuShopWork->itemId != 0) {
            long money;

            eMessageCpy(work->message, MenuNumberTextGet(MenuShopWork->quantity, 2, 0));
            eMessageCat(tab32.p[2]);
            eMessageCat(MenuTextGet(MenuShopWork->itemId)->name);
            eMessageCat(tab32.p[0]);
            switch (MenuShopWork->mode) {
            case 0:
                money = MenuBoxMoneyGet(MenuShopWork->itemId, 0) * MenuShopWork->quantity;
                break;
            default:
                money = MenuBoxMoneyGet(MenuShopWork->itemId, 1) * MenuShopWork->quantity;
                break;
            }
            eMessageCat(MenuNumberTextGet(money, 10, 0));
            eMessageCat(tab32.p[1]);
        }
        break;
    case 164:
        if (MenuShopWork->itemId != 0) {
            eMessageCpy(work->message, MenuNumberTextGet(MenuShopWork->quantity, 2, 0));
            eMessageCat(tab64.p[0]);
            eMessageCat(MenuTextGet(MenuShopWork->itemId)->name);
            eMessageCat(D_004DB140);
            eMessageCat(tab64.p[MenuShopWork->mode + 1]);
        }
        break;
    case 192:
        eMessageCpy(work->message, tab48.p[MenuShopWork->sortOption]);
        break;
    case 194:
    case 196:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        eMessageCat(tab80.p[0]);
        break;
    case 198:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        eMessageCat(tab80.p[7]);
        break;
    case 208:
        if (MenuShopWork->listIndex < 0 || MenuShopWork->equipmentId == 0) {
            eMessageCpy(work->message, D_004DB140);
        } else {
            eMessageCpy(work->message, (const char *)func_A2C968(MenuShopWork->equipmentId)->auxiliary);
        }
        break;
    case 210:
    case 212:
        if (MenuShopWork->listIndex >= 0) {
            unit = func_A191C0(MenuShopWork->unitId);
            eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
            eMessageCat(tab80.p[1]);
            if (MenuShopWork->sort_mode_selector != 0) {
                value = *(short *)(unit + 88);
                if (value != 0)
                    eMessageCat(func_A2C968(value)->name);
                eMessageCat(tab80.p[2]);
                eMessageCat(tab80.p[3]);
                if (MenuShopWork->equipmentId != 0)
                    eMessageCat(func_A2C968(MenuShopWork->equipmentId)->name);
            } else {
                value = *(short *)(unit + 86);
                if (value != 0)
                    eMessageCat(func_A2C918(value)->name);
                eMessageCat(tab80.p[2]);
                eMessageCat(tab80.p[3]);
                if (MenuShopWork->equipmentId != 0)
                    eMessageCat(func_A2C918(MenuShopWork->equipmentId)->name);
            }
            eMessageCat(D_004DB148);
        } else {
            eMessageCpy(work->message, D_004DB140);
        }
        break;
    case 214:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        if (MenuShopWork->sort_mode_selector != 0) {
            eMessageCat(tab80.p[4]);
            eMessageCat(tab80.p[6]);
            if (MenuShopWork->equipmentId != 0)
                eMessageCat(func_A2C968(MenuShopWork->equipmentId)->name);
        } else {
            eMessageCat(tab80.p[5]);
            eMessageCat(tab80.p[6]);
            if (MenuShopWork->equipmentId != 0)
                eMessageCat(func_A2C918(MenuShopWork->equipmentId)->name);
        }
        eMessageCat(D_004DB150);
        break;
    default:
        targetY = 480;
        break;
    }
    MoveSlide(&work->window.y, &targetY, 3.0f);
    MoveSlide(&work->window.height, &targetH, 3.0f);
    work->panel.text = work->message;
    WindowDXMain((WindowDX *)(void *)(&work->window));
}



static void MenuShopSelect(TskObject *task, void *data)
{
    MenuShopSelectWork *work = data;

    MenuShopSelectCommand *ListMake(int menuIndex)
    {
        MenuShopCommandPane *pane = (MenuShopCommandPane *)(menuIndex * sizeof(MenuShopCommandPane) + (int)work + 8);

        memset(pane->commands, 0, sizeof(pane->commands));
        {
            int commandIndex;

            for (commandIndex = 0; commandIndex < 4; commandIndex++) {
                work->menus[menuIndex].commands[commandIndex].text =
                    msg00_1_0036DB30[menuIndex * 4 + commandIndex];
                work->menus[menuIndex].commands[commandIndex].locked = 0;
            }
            work->menus[menuIndex].commands[commandIndex].text = 0;
        }
        if (menuIndex == 0) {
            if (MenuShopWork->noCategories != 0)
                work->menus[menuIndex].commands[0].locked = 1;
            if (MenuShopWork->equipmentStatus == 0)
                work->menus[menuIndex].commands[2].locked = 1;
        }
        return work->menus[menuIndex].commands;
    }

    int i;

    switch (task->state) {
    case 0:
        work->depth = 0xfffff4;
        for (i = 0; i < 2; i++) {
            WindowDXSet(&work->menus[i].window);
            work->menus[i].window.x = 528;
            work->menus[i].window.style = work->depth;
            if (i == 0)
                work->menus[0].window.width = 205;
            else
                work->menus[i].window.width = 205;
            work->menus[i].window.height = 102;
            work->menus[i].window.close_callback_arg = &work->menus[i].panel;
            work->menus[i].window.close_callback = (void (*)(WindowDX *, void *))MenuSelectWindow;
            work->menus[i].window.tag_id = D_004DB168;
            work->menus[i].panel.cursor = 0;
            work->menus[i].panel.commands = ListMake(i);
            work->menus[i].panel.displayMode = 2;
            work->menus[i].window.state = 1;
            WindowDXMain(&work->menus[i].window);
            work->menus[i].window.state = 3;
        }
        for (i = 0; i < 1; i++) {
            WindowDXSet(&work->choices[i].window);
            work->choices[i].window.x = 528;
            work->choices[i].window.width = 169;
            work->choices[i].window.height = 102;
            work->choices[i].window.style = work->depth;
            work->choices[i].window.tag_id = D_004DB170;
            work->choices[i].panel.choices = msg01_2_0036DB50[3];
            work->choices[i].panel.prompt = msg01_2_0036DB50[0];
            work->choices[i].window.close_callback_arg = &work->choices[i].panel;
            work->choices[i].window.close_callback = (void (*)(WindowDX *, void *))MenuSelectWindow;
            work->choices[i].panel.cursor = 0;
            work->choices[i].panel.displayMode = 1;
            work->choices[i].window.state = 1;
            WindowDXMain(&work->choices[i].window);
            work->choices[i].window.state = 3;
        }
        work->popupFlags = 0;
        break;
    case 2: {
        short menuX[2];
        short choiceX[2];
        MenuShopWorkData *shop;

        for (i = 0; i < 2; i++) {
            unsigned char popupVisible;

            menuX[i] = 528;
            popupVisible = work->popupFlags & 1;
            if (popupVisible)
                choiceX[i] = -176;
            else
                choiceX[i] = 528;
        }
        shop = MenuShopWork;
        switch (shop->state) {
        case 0x20:
            if (shop->stateFlags & 1) {
                work->menus[0].window.x = 528;
                work->menus[0].window.y = 192;
            }
            menuX[0] = 152;
            work->menus[0].panel.cursor = shop->mode;
            break;
        case 0xc0:
            if (shop->stateFlags & 1) {
                work->menus[1].window.x = 528;
                work->menus[1].window.y = 192;
            }
            menuX[1] = 160;
            work->menus[1].panel.cursor = shop->sortOption;
            break;
        case 0xa2: {
            unsigned char *mode = &shop->mode;
            unsigned char updatePopup;

            updatePopup = shop->stateFlags & 1;

            if (updatePopup) {
                switch (*mode) {
                case 0:
                    work->choices[0].panel.prompt = msg01_2_0036DB50[0];
                    switch (mode[1]) {
                    case 0:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    case 7:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 208;
                        work->popupFlags |= 1;
                        break;
                    case 8:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    default:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 80;
                        work->popupFlags |= 1;
                        break;
                    }
                    break;
                default:
                    work->choices[0].panel.prompt = msg01_2_0036DB50[1];
                    switch (mode[1]) {
                    case 0:
                    case 8:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    case 1:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 272;
                        work->popupFlags |= 1;
                        break;
                    case 4:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 272;
                        work->popupFlags |= 1;
                        break;
                    case 7:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 192;
                        work->popupFlags |= 1;
                        break;
                    default:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 80;
                        work->popupFlags |= 1;
                        break;
                    }
                    break;
                }
            }
            switch (MenuShopWork->category) {
            case 0:
            case 8:
                choiceX[0] = 304;
                break;
            case 7:
                choiceX[0] = 48;
                break;
            default:
                choiceX[0] = 32;
                break;
            }
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        }
        case 0xc4:
            if (shop->stateFlags & 1) {
                work->choices[0].panel.prompt = msg01_2_0036DB50[2];
                work->choices[0].window.x = -136;
                work->choices[0].window.y = 192;
                work->popupFlags |= 1;
            }
            choiceX[0] = 88;
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        case 0xd4:
            if (shop->stateFlags & 1) {
                work->choices[0].panel.prompt = msg01_2_0036DB50[0];
                work->choices[0].window.x = 528;
                work->choices[0].window.y = 176;
                work->popupFlags &= ~1;
            }
            choiceX[0] = 304;
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        }
        for (i = 0; i < 2; i++) {
            MoveSlide(&work->menus[i].window.x, &menuX[i], 3.0f);
            WindowDXMain(&work->menus[i].window);
        }
        MoveSlide(&work->choices[0].window.x, &choiceX[0], 3.0f);
        WindowDXMain(&work->choices[0].window);
        break;
    }
    }
}

void MenuShopSortSet(int list, int type, int order, int option)
{
    MenuShopSortTable table;
    int *output = MenuSortAddrGet(list);

    table = D_004C7738;

    if (type == 0x12)
        type = 9;
    if (type == 0x13)
        type = 10;

    if (type == 8) {
        if (MenuScenarioNo >= 0x73) {
            *output++ = 0x20035;
            *output++ = 0x20036;
        }
        if (MenuScenarioNo >= 0x12D)
            *output++ = 0x20037;
    } else {
        int i = 0;

        if (table.entries[type].count > 0) {
            const short *itemIds = table.entries[type].itemIds;

            do {
                int itemId = *itemIds++;
                unsigned int itemValue = (unsigned short)itemId;

                if (itemId == 0)
                    break;
                *output++ = itemValue + ((unsigned int)table.entries[type].highWord << 16);
                i++;
            } while (i < table.entries[type].count);
        }
    }

    *output = 0;
}

int subMenuShopEquipCheck(int item, int category)
{
    int partyIds[16];
    int selectedSlot[8];
    short selectedItem = (short)item;
    int partyCount = 0;
    int matches = 0;
    int party;
    int remaining;
    short *equipment;
    struct MenuShopEquipRecord *unit;

    memset(selectedSlot, 0, sizeof(selectedSlot));
    selectedSlot[1] = 1;
    selectedSlot[4] = 1;
    switch (MenuShopWork->category) {
    case 1:
    case 2:
    case 3:
        partyCount = PartyAllPartyGet(partyIds, 0);
        break;
    case 4:
    case 5:
    case 6:
        partyCount = PartyAgwsGet(partyIds);
        break;
    }
    if (partyCount == 0)
        return 0;
    switch (MenuShopWork->category) {
    case 1:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            if (unit->itemIds[0][selectedSlot[partyIds[party] - 1]] == selectedItem)
                matches++;
        }
        break;
    case 3:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            if (unit->itemIds[2][selectedSlot[partyIds[party] - 1]] == selectedItem)
                matches++;
        }
        break;
    case 2:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            for (equipment = unit->itemIds[1], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 4:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            for (equipment = unit->itemIds[0], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 6:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            for (equipment = unit->itemIds[2], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 5:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == NULL)
                return 0;
            for (equipment = unit->itemIds[1], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    }
    return matches;
}

int MenuShopNoSaleCheck(int item)
{
    int itemId;
    int category;
    int flags;

    itemId = item & 0xFFFF;
    category = (unsigned int)item >> 16;
    flags = 0;

    if (itemId == 0)
        return 0;

    switch (category) {
    case 1:
        flags = func_A1A4E8(itemId)->flags;
        break;
    case 3:
        flags = dataWpnGet(itemId)->flags;
        break;
    case 4:
        flags = dataAttGet(itemId)->flags;
        break;
    case 5:
        flags = func_A1A548(itemId)->flags;
        break;
    }

    if ((flags & 0x1000) != 0)
        return 1;
    return 0;
}

void MenuShopListColorChange(int list, int kind)
{
    int *sorted = MenuSortAddrGet(list);
    int count = MenuSortCheck(list);
    MenuShopListRow *entry = MenuListGet(list);
    int i;

    if (MenuShopWork->mode == 0 || kind == 2) {
        for (i = 0; i < count; i++) {
            if (MenuBoxMoneyGet(MenuSortGet(list, i), 0) > dataMoneyBoxChk())
                entry[i].flag = 1;
        }
    } else {
        for (i = 0; i < count; i++) {
            int noSale;

            MenuBoxChk(MenuSortGet(list, i));
            subMenuShopEquipCheck(MenuSortGet(list, i), 0);
            noSale = MenuShopNoSaleCheck(sorted[i]);
            if (noSale == 1)
                entry[i].flag = noSale;
        }
    }
}


int MenuShopListChange00(int list)
{
    struct MenuShopSortOptionTable sortOptions;
    struct MenuShopRowCountTable rowCounts;
    MenuShopWorkData *shop;
    MenuShopWindowSP *window;
    short width;
    unsigned int columns;
    unsigned int visibleRows;

    if (MenuShopWork->mode == 0) {
        MenuShopSortSet(0, MenuShopWork->category, 0, 0);
        MenuListMake(0, -10);
        MenuShopListColorChange(0, 0);
    } else {
        sortOptions = D_004C77D8;
        MenuSortSet(0, sortOptions.entries[MenuShopWork->category].type,
                    sortOptions.entries[MenuShopWork->category].order);
        MenuListMake(0, 0);
        MenuShopListColorChange(0, 0);
    }
    rowCounts = D_004C7820;
    shop = MenuShopWorkRead();
    columns = 1;
    visibleRows = 7;
    width = 272 - shop->mode * 32;
    window = MenuShopWinSPRead();
    window->width = width;
    window->height =
        rowCounts.values[shop->category + shop->mode * 9] * 24 + 6;
    window->rows =
        rowCounts.values[shop->category + shop->mode * 9];
    MenuShopWinSP->columns = columns;
    MenuShopWinSP->title = msg00_4_0036DB60[MenuShopWork->mode];
    MenuShopWinSP->row_count = visibleRows;
    MenuShopWinSP->items = MenuListGet(0);
    WindowSPItemChange((void *)MenuShopWinSP);
    WindowSPSetSelect((void *)MenuShopWinSP,
        &MenuKeepSelect[(MenuShopWork->category + MenuShopWork->mode * 9) * 5]);
    if (MenuShopWork->mode == 0)
        MenuShopWinSP->state = 6;
    return WindowSPSelect((void *)MenuShopWinSP, 0);
}

int MenuShopListChange01(int list)
{
    int type = 18;
    MenuShopWindowSP *window;
    short window_width;

    if (MenuShopWork->sort_mode_selector == 0)
        type = 19;

    MenuShopSortSet(0, type, 0, 0);
    MenuListMake(0, -10);
    MenuShopListColorChange(0, 2);

    window = MenuShopWinSP;
    MenuShopWinSP->rows = 9;
    window_width = 272;
    window->height = 222;
    window->width = window_width;
    MenuShopWinSP->columns = 1;
    MenuShopWinSP->title = D_004C7858;
    MenuShopWinSP->row_count = 7;
    MenuShopWinSP->items = MenuListGet(0);
    WindowSPItemChange((WindowSPCursor *)(void *)((void *)MenuShopWinSP));
    WindowSPSetSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), &MenuKeepSelect[type * 5]);

    MenuShopWinSP->state = 6;
    return WindowSPSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), 0);
}

static void MenuShopList(TskObject *task, struct MenuShopListDisplay *work)
{
    MenuShopWorkData *selection;
    short closeX;
    short secondTargetX;

    switch (task->state) {
    case 0:
        work->window.x = -0x110;
        work->window.y = 0x50;
        work->color = 0xffff0;
        work->window.style = 0xffff0;
        work->window.draw_flags = 7;
        work->window.title = D_004DB140;
        work->window.rows = 0xb;
        work->window.height = 0x10e;
        work->window.state = 0;
        work->window.items = 0;
        WindowSPSet((WindowSPCursor *)(void *)(&work->window));
        work->initialFlags[0] = 0;
        work->initialFlags[1] = 0;
        work->state = 0;
        work->active = 0;
        return;
    case 2:
        switch (work->state) {
        case 0:
            work->active = 0;
            work->state = 1;
            /* fall through */
        case 1:
            switch (MenuShopWork->state) {
            case 0x30:
            case 0x90:
                work->state = 0xa;
                break;
            case 0xd0:
                work->state = 0x1e;
                break;
            }
            break;
        case 10:
            work->window.x = -0x120;
            work->window.y = 0x50;
            MenuShopListChange00(0);
            work->window.state = 0x11;
            work->state = 0xb;
            work->active = 1;
            selection = MenuShopWork;
            work->category = selection->category;
            goto update_open_window;
        case 11:
            selection = MenuShopWork;
        update_open_window:
        {
            short targetX = 0x10;
            if (selection->state == 0xa2) {
                unsigned char category = selection->category;

                if (selection->mode == 0) {
                    if (category != 0 && category != 8 && category != 7)
                        targetX = -0x120;
                } else if (category != 0 && category != 8 && category != 1 && category != 4) {
                    targetX = -0x120;
                }
            }
            MoveSlide(&work->window.x, &targetX, 3.0f);
            selection = MenuShopWork;
            if (selection->state == 0x20) {
                work->state = 0xc;
                selection = MenuShopWork;
            }
            if (work->category != selection->category)
                work->state = 0xa;
            break;
        }
        case 12:
            closeX = -((unsigned short)work->window.width + 0x10);
            MoveSlide(&work->window.x, &closeX, 3.0f);
            if (work->window.x == closeX)
                work->state = 0;
            break;
        case 30:
            work->window.x = -0x120;
            work->window.y = 0x50;
            MenuShopListChange01(0);
            work->window.state = 0x11;
            work->state = 0x1f;
            work->active = 1;
            /* fall through */
        case 31:
            secondTargetX = 0x10;
            MoveSlide(&work->window.x, &secondTargetX, 3.0f);
            if (MenuShopWork->state == 0xc0)
                work->state = 0xc;
            break;
        }
        if (work->active != 0)
            WindowSPMain((WindowSPCursor *)(void *)(&work->window));
        break;
    }
}

void MenuShopEx(void *task, void *data);

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopEx);

static void MenuShopSeisan(TskObject *task, struct MenuShopSalePanel *work)
{
    short targetX[1];
    int i;

    switch (task->state) {
    case 0:
        work->depth = 0xfffff0;
        WindowDXSet(&work->window);
        work->window.x = 0x210;
        work->window.style = work->depth;
        work->window.y = 0x50;
        work->window.width = 0xc0;
        work->window.height = 0x98;
        work->window.tag_id = msg00_6[0];
        work->window.state = 1;
        WindowDXMain(&work->window);
        work->window.state = 3;
        for (i = 0; i < 4; i++) {
            eTagFontSet((ETagFont *)(void *)&work->labels[i], msg01_7[i]);
            work->labels[i].style = work->depth + 2;
        }
        for (i = 0; i < 6; i++) {
            eNumberSet(&work->numbers[i], 0);
            work->numbers[i].style = work->depth + 2;
        }
        for (i = 0; i < 2; i++) {
            eSpriteSet(&work->sprites[i], i + 0x705);
            work->sprites[i].style = work->depth + 2;
        }
        for (i = 0; i < 2; i++) {
            work->quantityDivider[i].red =
                work->quantityDivider[i].green =
                work->quantityDivider[i].blue = -0x60;
            work->quantityDivider[i].alpha = -0x80;
            work->quantityDivider[i].depth = work->depth + 2;
        }
        work->quantityDividerState = 0;
        for (i = 0; i < 2; i++) {
            work->moneyDivider[i].red =
                work->moneyDivider[i].green =
                work->moneyDivider[i].blue = -0x60;
            work->moneyDivider[i].alpha = -0x80;
            work->moneyDivider[i].depth = work->depth + 2;
        }
        work->moneyDividerState = 0;
        eRibbonSet((ERibbonSetData *)(void *)&work->ribbon, 0);
        {
        int cellIndex;
        for (cellIndex = 0; cellIndex < 6; cellIndex++) {
            if (cellIndex < 4) {
                work->ribbon.points[cellIndex].red = 0x20;
                work->ribbon.points[cellIndex].green = 0x34;
                work->ribbon.points[cellIndex].blue = 0x34;
            } else {
                work->ribbon.points[cellIndex].blue = 0;
                work->ribbon.points[cellIndex].green = 0;
                work->ribbon.points[cellIndex].red = 0;
            }
            if ((unsigned int)(cellIndex - 2) < 2)
                work->ribbon.points[cellIndex].alpha = 0x20;
            else
                work->ribbon.points[cellIndex].alpha = 0;
        }
        }
        work->flags = 0;
        work->selectionFlags = 0;
        break;
    case 2:
        targetX[0] = 0x210;
        if (MenuShopWork->state == 0xa0 || MenuShopWork->state == 0xa2)
            targetX[0] = 0x130;
        MoveSlide(&work->window.x, targetX, 3.0f);
        /* The final four bytes of the title font record hold its text pointer. */
        __builtin_memcpy(&work->window.title_font.unmodeled_10[12],
                         &msg00_6[MenuShopWork->mode & 1], sizeof(const char *));
        WindowDXMain(&work->window);
        for (i = 0; i < 4; i++) {
            work->labels[i].x = (unsigned short)work->window.x + 0x43;
            work->labels[i].y = (unsigned short)work->window.y +
                ((i % 2) * 0x30 + (i / 2) * 0x4c) + 4;
            eTagFontMain((ETagFontMainData *)(void *)&work->labels[i]);
        }
        for (i = 0; i < 2; i++) {
            if (MenuShopWork->mode == 0)
                work->sprites[i].spriteId = i + 0x705;
            else
                work->sprites[i].spriteId = 0x706 - i;
            work->sprites[i].x = (unsigned short)work->window.x + 0x39;
            work->sprites[i].y = (unsigned short)work->window.y + i * 0x4c + 0x1c;
            eSpriteMain(&work->sprites[i]);
        }
        work->ribbon.x = (unsigned short)work->window.x + 0x37;
        work->ribbon.y = (unsigned short)work->window.y + 0x1c;
        work->ribbon.width = 0x2c;
        work->ribbon.height = 0x10;
        work->ribbon.depth = work->depth + 1;
        for (i = 0; i < 6; i++)
            eRibbonMain((ERibbonMainData *)(void *)&work->ribbon);
        {
        MenuShopWorkData *selection = MenuShopWork;
        if (selection->listIndex >= 0) {
            unsigned char quantityLimit;
            signed char quantity;
            int unitPrice;
            work->numbers[0].places = work->numbers[1].places = work->numbers[2].places = 2;
            work->numbers[3].places = work->numbers[4].places = work->numbers[5].places = 9;
            quantityLimit = MenuShopWork->quantityLimit;
            work->numbers[0].value = quantityLimit;
            quantity = MenuShopWork->quantity;
            work->numbers[1].value = quantity;
            if (MenuShopWork->mode == 0) {
                work->numbers[2].value = quantityLimit + quantity;
                work->numbers[3].value = MenuShopWork->availableMoney;
                unitPrice = MenuBoxMoneyGet(MenuSortGet(0, MenuShopWork->listIndex), 0);
                selection = MenuShopWork;
                work->numbers[4].value = unitPrice * selection->quantity;
                work->numbers[5].value = (int)selection->availableMoney - work->numbers[4].value;
            } else {
                work->numbers[2].value = quantityLimit - quantity;
                work->numbers[3].value = MenuShopWork->availableMoney;
                unitPrice = MenuBoxMoneyGet(MenuSortGet(0, MenuShopWork->listIndex), 1);
                selection = MenuShopWork;
                work->numbers[4].value = unitPrice * selection->quantity;
                work->numbers[5].value = (int)selection->availableMoney + work->numbers[4].value;
            }
        }
        i = 0;
        if (*(MenuShopListIndex *)&selection->listIndex >= 0) {
            do {
                work->numbers[i].x = (unsigned short)work->window.x + 0x47;
                work->numbers[i].y = (unsigned short)work->window.y + ((i % 3) * 0x18 + (i / 3) * 0x4c) + 4;
                work->numbers[i].option = 0;
                eNumberMain((ENumberMainData *)(void *)&work->numbers[i]);
                if (++i >= 6)
                    break;
            } while (MenuShopWork->listIndex >= 0);
        }
        }
        work->quantityDivider[0].x = (unsigned short)work->window.x + 0x1a;
        work->quantityDivider[1].x = (unsigned short)work->window.x + 0x5d;
        work->quantityDivider[1].y = (unsigned short)work->window.y + 0x30;
        work->quantityDivider[0].y = (unsigned short)work->window.y + 0x30;
        work->moneyDivider[0].x = (unsigned short)work->window.x + 0x14;
        work->moneyDivider[1].x = (unsigned short)work->window.x + 0xaa;
        work->moneyDivider[1].y = (unsigned short)work->window.y + 0x7c;
        work->moneyDivider[0].y = (unsigned short)work->window.y + 0x7c;
        endPrintExtFunc(work->depth, 0, work->quantityDivider);
        endPrintExtFunc(work->depth, 0, work->moneyDivider);
        break;
    }
}

static void MenuShopSeisan02(TskObject *task, MenuShopSeisan02Work *work)
{
    int i;
    unsigned char *spriteBase;
    short slideTarget48;
    short slideTargetBack;
    short slideTarget288;
    short slideTarget528;
    const char **tagSlot;
    int *spriteWork;

    switch (task->state) {
    case 0:
        work->ticker = 0x00fffff0;
        WindowDXSet(&work->window);
        work->window.x = 528;
        work->window.y = 80;
        work->window.style = work->ticker;
        work->window.width = 208;
        work->window.height = 80;
        work->window.tag_id = D_004DB1A0;
        tagSlot = &work->window.tag_id;
        work->window.state = 1;
        WindowDXMain(&work->window);
        work->window.state = 3;
        for (i = 0; i < 2; i++) {
            eTagFontSet((ETagFont *)(void *)&work->tags[i], msg01_8_0036DBA0[i]);
            work->tags[i].work = work->ticker + 2;
        }
        for (i = 0; i < 3; i++) {
            eNumberSet(&work->numbers[i], 0);
            work->numbers[i].work = work->ticker + 2;
        }
        spriteWork = (int *)((unsigned char *)tagSlot + 896);
        eSpriteSet(&work->sprite, 1798);
        *spriteWork = work->ticker + 2;
        for (i = 0; i < 2; i++) {
            int off = 928 + 12 * i;
            unsigned char *ent = (unsigned char *)work + off;
            unsigned char *stateBase;

            ent[30] = -96;
            ent[29] = -96;
            ent[28] = -96;
            ent[31] = -128;
            stateBase = &work->window.state;
            *(int *)(stateBase + off) = work->ticker + 2;
        }
        work->unmodeled_976 = 0;
        work->unmodeled_03 = 0;
        work->state = 0;
        break;
    case 2:
        switch (work->state) {
        case 0:
            work->flag = 0;
            work->state = 1;
            /* fall through */
        case 1:
            switch (MenuShopWork->state) {
            case 0xC2:
                work->state = 10;
                break;
            case 0xD4:
                work->state = 20;
                break;
            }
            break;
        case 10:
            work->window.x = -224;
            work->window.y = 80;
            work->state = 11;
            work->kind = 1;
            work->flag = 1;
            /* fall through */
        case 11:
            slideTarget48 = 48;
            MoveSlide(&work->window.x, &slideTarget48, 3.0f);
            if (MenuShopWork->state == 0xC0)
                work->state = 12;
            break;
        case 12:
            slideTargetBack = -224;
            MoveSlide(&work->window.x, &slideTargetBack, 3.0f);
            if (work->window.x == slideTargetBack)
                work->state = 0;
            break;
        case 20:
            work->window.x = 528;
            work->window.y = 64;
            work->kind = 2;
            work->state = 21;
            work->flag = 1;
            /* fall through */
        case 21:
            slideTarget288 = 288;
            MoveSlide(&work->window.x, &slideTarget288, 3.0f);
            switch (MenuShopWork->state) {
            case 0xC0:
            case 0xD0:
            case 0xD2:
                work->state = 22;
                break;
            }
            break;
        case 22:
            slideTarget528 = 528;
            MoveSlide(&work->window.x, &slideTarget528, 3.0f);
            if (work->window.x == slideTarget528)
                work->state = 0;
            break;
        }

        if (!work->flag)
            return;
        WindowDXMain(&work->window);
        for (i = 0; i < 2; i++) {
            work->tags[i].x = work->window.x + 75;
            work->tags[i].y = work->window.y + (48 * (i % 2) + 76 * (i / 2)) + 7;
            eTagFontMain(&work->tags[i]);
        }
        i = 1;
        spriteBase = (unsigned char *)work + 896;
        *(short *)(spriteBase + 16) = work->window.x + 65;
        *(short *)(spriteBase + 18) = work->window.y + 31;
        eSpriteMain(&work->sprite);
        work->numbers[0].value.integer = dataMoneyBoxChk();
        if (work->kind == i) {
            work->numbers[1].value.integer = 100;
        } else {
            int item;

            if (MenuShopWork->sort_mode_selector != 0)
                item = 0xB0000 + (unsigned short)MenuShopWork->equipmentId;
            else
                item = 0xC0000 + (unsigned short)MenuShopWork->equipmentId;
            work->numbers[1].value.integer = MenuBoxMoneyGet(item, 0);
        }
        work->numbers[2].value.integer = work->numbers[0].value.integer - work->numbers[1].value.integer;
        for (i = 0; i < 3; i++) {
            work->numbers[i].digits = 9;
            work->numbers[i].x = work->window.x + 83;
            work->numbers[i].y = work->window.y + 24 * i + 7;
            work->numbers[i].format = 0;
            eNumberMain(&work->numbers[i]);
        }
        work->points[0].x = work->window.x + 28;
        work->points[1].x = work->window.x + 181;
        work->points[1].y = work->window.y + 51;
        work->points[0].y = work->window.y + 51;
        endPrintExtFunc(work->ticker, 0, &work->points[0]);
        break;
    }
}

static void MenuShopEx02(TskObject *task, struct MenuShopUnitPanel *work)
{
    short openX;
    short closeX;
    int i;

    switch (task->state) {
    case 0:
    {
        short *unitIds;
        work->depth = 0xffff0;
        eRibbonSet((ERibbonSetData *)(void *)(&work->ribbon), 3);
        work->ribbon.x = 0x210;
        work->ribbon.y = 0x40;
        work->ribbon.style = work->depth;
        work->ribbon.width = 0x100;
        work->ribbon.height = 0x90;
        eTagFontSet((ETagFont *)(void *)(&work->label), D_004DB1B8);
        work->label.style = work->depth + 2;
        for (i = 0; i < MenuShopWork->unitCount; i++) {
            unitIds = MenuShopWork->unitIds;
            eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
            work->portraits[i].style = work->depth + 2;
        }
        eCursolSet((ECursol *)(void *)(&work->cursor), 0);
        work->cursor.style = work->depth + 4;
        work->cursor.mode = 0;
        work->active = work->flags = work->selectionFlags = 0;
        work->state = 0;
        return;
    }
    case 2:
        switch (work->state) {
        case 0:
            work->active = 0;
            work->state = 1;
            /* fall through */
        case 1:
            switch (MenuShopWork->state) {
            case 0xd0:
            case 0xd2:
                work->state = 20;
                break;
            case 0xc2:
                work->state = 10;
                break;
            }
            break;
        case 10:
        case 20:
            work->ribbon.x = 0x210;
            work->active = 1;
            if (work->state == 10) {
                work->state = 11;
                work->cursor.mode = 0x20;
            } else {
                work->state = 21;
            }
            /* fall through */
        case 11:
        case 21:
        {
            unsigned char shopState;
            openX = 0x130;
            MoveSlide(&work->ribbon.x, &openX, 3.0f);
            shopState = MenuShopWork->state;
            if (shopState == 0xc0) {
                work->state = 12;
            }
            if (MenuShopWork->state == 0xd4) {
                work->state = 12;
            }
            if (work->state == 21) {
                for (i = 0; i < MenuShopWork->unitCount; i++) {
                    int canEquip = 0;
                    short *unitIds = MenuShopWork->unitIds;
                    eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
                    if (MenuShopWork->sort_mode_selector != 0) {
                        short *currentIds = MenuShopWork->unitIds;
                        if (MenuFrameEquipCheck(currentIds[i], MenuShopWork->equipmentId) != 0)
                            canEquip = 1;
                    } else {
                        short *currentIds = MenuShopWork->unitIds;
                        canEquip = MenuEngineEquipCheck(currentIds[i], MenuShopWork->equipmentId) != 0;
                    }
                    if (canEquip == 0) {
                        work->portraits[i].blue = 0x40;
                        work->portraits[i].green = 0x40;
                        work->portraits[i].red = 0x40;
                    } else {
                        work->portraits[i].blue = -0x80;
                        work->portraits[i].green = -0x80;
                        work->portraits[i].red = -0x80;
                    }
                }
                if (MenuShopWork->state == 0xd2 && work->cursor.mode == 0)
                    work->cursor.mode = 0x20;
                if (MenuShopWork->state == 0xd0)
                    work->cursor.mode = 0;
            } else {
                for (i = 0; i < MenuShopWork->unitCount; i++) {
                    short *unitIds = MenuShopWork->unitIds;
                    eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
                    unitIds = MenuShopWork->unitIds;
                    if (MenuCharHpCheck(unitIds[i]) != 0) {
                        work->portraits[i].blue = 0x40;
                        work->portraits[i].green = 0x40;
                        work->portraits[i].red = 0x40;
                    } else {
                        work->portraits[i].blue = -0x80;
                        work->portraits[i].green = -0x80;
                        work->portraits[i].red = -0x80;
                    }
                }
            }
            break;
        }
        case 12:
            closeX = 0x210;
            MoveSlide(&work->ribbon.x, &closeX, 3.0f);
            if (work->ribbon.x == closeX)
                work->state = 0;
            work->cursor.mode = 0;
            break;
        }
        if (work->active != 0) {
            eRibbonMain((ERibbonMainData *)(void *)(&work->ribbon));
            work->label.x = work->ribbon.x + 8;
            work->label.y = (unsigned short)work->ribbon.y + 0x44;
            eTagFontMain((ETagFontMainData *)(void *)(&work->label));
            for (i = 0; i < MenuShopWork->unitCount; i++) {
                work->portraits[i].x = work->ribbon.x + (i & 1) * 0x40 + 0x40;
                work->portraits[i].y = (unsigned short)work->ribbon.y + (i / 2) * 0x30;
                work->portraits[i].style = work->depth + 2;
                eSpriteMain(&work->portraits[i]);
            }
            work->cursor.x = work->ribbon.x + ((unsigned char)MenuShopWork->unitSelection & 1) * 0x40 + 0x40;
            work->cursor.y = (unsigned short)work->ribbon.y + (MenuShopWork->unitSelection / 2) * 0x30 + 0x10;
            work->cursor.style = work->depth + 4;
            eCursolMain((ECursolMainData *)(void *)(&work->cursor));
        }
        break;
    }
}

static void MenuShopIcon(TskObject *task, struct MenuShopIconDisplay *work)
{
    int i;

    switch (task->state) {
    case 0:
        {
            struct MenuShopIconIds spriteIds = D_004C79F8;
            work->depth = 0xfffff0;
            for (i = 0; i < 11; i++) {
                eSpriteSet(&work->sprites[i], spriteIds.ids[i]);
                work->sprites[i].depth = work->depth;
            }
        }
        {
            short topY = 12;
            for (i = 1; i >= 0; i--)
                work->sprites[i].y = topY;
        }
        for (i = 2; i < 11; i++) {
            work->sprites[i].y = 12;
            if (((MenuShopWork->categoryMask >> (i - 2)) & 1) == 0) {
                work->sprites[i].blue = 0x40;
                work->sprites[i].green = 0x40;
                work->sprites[i].red = 0x40;
            }
        }
        work->active = 1;
        work->slide = 16;
        work->scale = 1.0f;
        work->animation = 0;
        work->phase = 0;
        for (i = 0; i < 9; i++)
            work->categoryLift[i] = 0;
        work->state = 2;
        return;
    case 2:
        switch (MenuShopWork->state) {
        case 0x20:
            if (MenuShopWork->stateFlags & 1) {
                for (i = 2; i < 11; i++) {
                    work->sprites[i].blue = -0x80;
                    work->sprites[i].green = -0x80;
                    work->sprites[i].red = -0x80;
                }
            }
            if (work->slide != 0)
                work->slide--;
            if (work->scale != 1.0f)
                work->scale += 0.2;
            break;
        case 0xa0:
        case 0xa4:
        case 0xa2:
        case 0x30:
        case 0x90:
            if (MenuShopWork->stateFlags & 1) {
                for (i = 2; i < 11; i++) {
                    if (((MenuShopWork->categoryMask >> (i - 2)) & 1) == 0) {
                        work->sprites[i].blue = 0x40;
                        work->sprites[i].green = 0x40;
                        work->sprites[i].red = 0x40;
                    } else {
                        work->sprites[i].blue = -0x80;
                        work->sprites[i].green = -0x80;
                        work->sprites[i].red = -0x80;
                    }
                }
            }
            if (work->slide != 0)
                work->slide--;
            work->scale += D_004D7E64;
            if (work->scale > 1.0f)
                work->scale = 1.0f;
            break;
        case 0xf0:
            work->active = 0;
            /* fall through */
        case 0xff:
            if (work->slide != 16)
                work->slide++;
            break;
        default:
            if (work->slide != 0)
                work->slide--;
            work->scale -= D_004D7E68;
            if (work->scale < 0.0f)
                work->scale = 0.0f;
            break;
        }
        for (i = 0; i < 9; i++) {
            if (i == MenuShopWork->category) {
                unsigned char state = MenuShopWork->state;
                if (state != 0x20 && state != 0xf0 &&
                    (state >> 4) != 0xc && (state >> 4) != 0xd) {
                    if (work->categoryLift[i] != 6)
                        work->categoryLift[i]++;
                    continue;
                }
            }
            if (work->categoryLift[i] != 0)
                work->categoryLift[i]--;
        }
        work->phase++;
        {
        for (i = 0; i < 2; i++) {
            float cosine;
            float alpha;
            work->sprites[i].x = 0x102 + i * 0xdd;
            cosine = xglCos((float)i * 3.1415927f);
            work->sprites[i].x = (int)((float)work->sprites[i].x + cosine * (float)-(work->slide * 2));
            work->sprites[i].y = 14;
            alpha = (128.0f - (float)work->slide * 0.0625f * 128.0f) * work->scale;
            work->sprites[i].alpha = alpha;
        }
        }
        {
        for (i = 2; i < 11; i++) {
            float alpha;
            work->sprites[i].x = 0x121 + (i - 2) * 0x15;
            work->sprites[i].x = (int)((float)work->sprites[i].x +
                xglCos((float)(i - 2) * 0.448798954f) * (float)-(work->slide * 2));
            work->sprites[i].y = work->categoryLift[i - 2] + 12;
            alpha = (128.0f - (float)work->slide * 0.0625f * 128.0f) * work->scale;
            work->sprites[i].alpha = alpha;
        }
        }
        for (i = 0; i < 11; i++)
            eSpriteMain(&work->sprites[i]);
        break;
    }
}

/* The display is seven halfwords; foreign MenuAgwsPara uses the same layout. */
typedef struct AgwsParaDisplay {
    short value[7];
} AgwsParaDisplay;



/* Partial equipment records: the four contiguous values are displayed by
 * frame selection, and the engine selection reads the first value at +8. */
typedef struct MenuShopEquipmentValues {
    unsigned char unmodeled_00[8];
    unsigned short value[4];
} MenuShopEquipmentValues;



typedef struct MenuShopEngineValue {
    unsigned char unmodeled_00[8];
    unsigned short value;
} MenuShopEngineValue;



extern AgwsParaDisplay MenuShopPara;
extern AgwsParaDisplay MenuShopPara2;
extern AgwsParaDisplay MenuAgwsPara;
extern MenuShopEquipmentValues *func_A1A598(int item);
extern MenuShopEngineValue *func_A1A5E8(int item);
extern unsigned short *func_A1A428(int item);
void MenuShopParaSet2(int item, int mode)
{
    int attack[4];
    int defense[4];
    MenuShopUnitData savedUnit;
    ShopCalculatedPara previous;
    MenuShopUnitData *unit;
    ShopCalculatedPara *calculated;
    int i;

    unit = func_A191C0(3);
    savedUnit = *unit;
    for (i = 0; i < 3; i++) {
        unit->weaponIds[i] = unit->accessoryIds[i] = unit->attachmentIds[i] = 0;
    }
    previous = *func_00A11108(3, attack, defense);
    previous.attack += (unsigned short)attack[0];
    previous.physicalDefense += (unsigned short)defense[0];
    previous.magicDefense += (unsigned short)defense[1];

    switch (mode) {
    case 0:
        unit->accessoryIds[0] = item;
        break;
    case 1:
        unit->accessoryIds[0] = item;
        break;
    case 2:
        unit->attachmentIds[0] = item;
        break;
    }

    calculated = func_00A11108(3, attack, defense);
    calculated->attack += (unsigned short)attack[0];
    calculated->physicalDefense += (unsigned short)defense[0];
    calculated->magicDefense += (unsigned short)defense[1];
    MenuShopPara.value[0] = calculated->attack - previous.attack;
    MenuShopPara.value[1] = calculated->physicalDefense - previous.physicalDefense;
    MenuShopPara.value[2] = calculated->displayStat2 - previous.displayStat2;
    MenuShopPara.value[3] = calculated->magicDefense - previous.magicDefense;
    MenuShopPara.value[4] = (signed char)calculated->byteStats[0] - (signed char)previous.byteStats[0];
    MenuShopPara.value[5] = (signed char)calculated->byteStats[1] - (signed char)previous.byteStats[1];
    MenuShopPara.value[6] = (signed char)calculated->byteStats[2] - (signed char)previous.byteStats[2];
    *unit = savedUnit;
}

void MenuShopParaSet(unsigned int mode)
{
    int attack[4];
    int defense[4];
    MenuShopEquipmentValues *frame;
    MenuShopEngineValue *engine;
    ShopCalculatedPara *calculated;
    MenuShopUnitData *unit;
    MenuShopWorkData *selection;
    MenuShopWeapon *weapon;
    MenuShopAccessoryData *accessory;
    short oldEquipment;
    int i;

    memset(&MenuShopPara, 0, sizeof(MenuShopPara));
    memset(&MenuShopPara2, 0, sizeof(MenuShopPara2));
    switch (mode) {
    case 0:
        if (MenuShopWork->equipmentId != 0) {
            frame = func_A1A598(MenuShopWork->equipmentId);
            MenuShopPara.value[0] = frame->value[0];
            MenuShopPara.value[2] = frame->value[1];
            MenuShopPara.value[3] = frame->value[2];
            MenuShopPara.value[4] = frame->value[3];
        }
        break;
    case 1:
        if (MenuShopWork->equipmentId != 0) {
            engine = func_A1A5E8(MenuShopWork->equipmentId);
            MenuShopPara.value[1] = engine->value;
        }
        break;
    case 2:
        if (MenuShopWork->unitId != 0) {
            calculated = func_00A11108(MenuShopWork->unitId, attack, defense);
            MenuAgwsParaSet(MenuShopWork->unitId, 0);
            for (i = 0; i < 6; i++) {
                MenuShopPara.value[i] = MenuAgwsPara.value[i];
                MenuShopPara2.value[i] = MenuAgwsPara.value[i];
            }
            MenuShopPara.value[0] = calculated->currentHp;
        }
        break;
    case 3:
    case 4:
        unit = func_A191C0(MenuShopWork->unitId);
        MenuAgwsParaSet(MenuShopWork->unitId, 10);
        for (i = 0; i < 6; i++)
            MenuShopPara.value[i] = MenuAgwsPara.value[i];
        if (mode == 3) {
            if (MenuFrameEquipCheck(MenuShopWork->unitId,
                                   MenuShopWork->equipmentId)) {
                selection = MenuShopWork;
                oldEquipment = unit->frameId;
                unit->frameId = selection->equipmentId;
            } else {
                for (i = 0; i < 6; i++)
                    MenuShopPara2.value[i] = MenuShopPara.value[i];
                break;
            }
        } else {
            if (MenuEngineEquipCheck(MenuShopWork->unitId,
                                    MenuShopWork->equipmentId)) {
                selection = MenuShopWork;
                oldEquipment = unit->engineId;
                unit->engineId = selection->equipmentId;
            } else {
                for (i = 0; i < 6; i++)
                    MenuShopPara2.value[i] = MenuShopPara.value[i];
                break;
            }
        }
        MenuAgwsParaSet(selection->unitId, 10);
        for (i = 0; i < 6; i++)
            MenuShopPara2.value[i] = MenuAgwsPara.value[i];
        if (mode == 3)
            unit->frameId = oldEquipment;
        else
            unit->engineId = oldEquipment;
        break;
    case 10:
        if ((short)(MenuShopWork->itemId >> 16) != 3)
            return;
        MenuShopParaSet2(MenuShopWork->modelId, 0);
        return;
    case 11:
        if ((short)(MenuShopWork->itemId >> 16) != 5)
            return;
        MenuShopParaSet2(MenuShopWork->modelId, 1);
        return;
    case 12:
        if ((short)(MenuShopWork->itemId >> 16) == 4)
            MenuShopPara.value[0] = *func_A1A428(MenuShopWork->modelId);
        break;
    case 13:
        if ((short)(MenuShopWork->itemId >> 16) == 3) {
            weapon = func_A1A3D8(MenuShopWork->modelId);
            MenuShopPara.value[1] = weapon->shopParameter1;
            MenuShopPara.value[2] = weapon->shopParameter2;
            MenuShopPara.value[5] = weapon->wagl;
        }
        break;
    case 14:
        if ((short)(MenuShopWork->itemId >> 16) == 5) {
            accessory = func_A1A548(MenuShopWork->modelId);
            MenuShopPara.value[2] = accessory->shopParameter2;
            MenuShopPara.value[3] = accessory->shopParameter3;
        }
        break;
    case 15:
        if ((short)(MenuShopWork->itemId >> 16) == 4)
            MenuShopPara.value[1] = *func_A1A428(MenuShopWork->modelId);
        break;
    case 16:
        if ((short)(MenuShopWork->itemId >> 16) == 7) {
            MenuAgwsParaSet(MenuShopWork->modelId, 0);
            for (i = 0; i < 6; i++)
                MenuShopPara.value[i] = MenuAgwsPara.value[i];
        }
        break;
    }
}

static void MenuShopParameter(TskObject *task, void *data);

static void MenuShopParameter(TskObject *task, void *data)
{
    struct MenuShopParameterDisplay *work = data;
    struct MenuShopParameterDigits8 characterDigits = D_004DB220[0];
    struct MenuShopParameterDigits7 agwsDigits = D_004DB228[0];
    short targetX;
    int i;

    switch (task->state) {
    case 0:
        work->depth = 0xffff00;
        WindowDXSet((WindowDX *)(void *)(&work->window));
        work->window.x = -0x1f0;
        work->window.style = work->depth;
        work->window.y = 0x149;
        work->window.width = 0x1e0;
        work->window.height = 0x36;
        work->window.state = 1;
        WindowDXMain((WindowDX *)(void *)(&work->window));
        work->window.state = 3;
        for (i = 0; i < 9; i++) {
            eTagFontSet((ETagFont *)(void *)(&work->labels[i]), 0);
            work->labels[i].depth = work->depth + 2;
            work->labels[i].red = 0x70;
        }
        for (i = 0; i < 9; i++) {
            eNumberSet(&work->numbers[i], 0);
            work->numbers[i].format = 5;
            work->numbers[i].depth = work->depth + 2;
        }
        work->count = 0;
        work->twoColumns = 0;
        break;
    case 2:
        targetX = -0x1f0;
        switch (MenuShopWork->state) {
        case 0x30:
        {
            int category = MenuShopWork->category;
            switch (category) {
            case 1:
            case 2:
            case 3:
                    switch (category) {
                    case 1: MenuShopParaSet(10); break;
                    case 2: MenuShopParaSet(11); break;
                    case 3: MenuShopParaSet(12); break;
                    }
                    work->twoColumns = 0;
                    work->count = 7;
                    for (i = 0; i < work->count; i++)
                        work->labels[i].text = msg00_9[i];
                    for (i = 0; i < work->count; i++) {
                        work->numbers[i].format = 5;
                        work->numbers[i].digits = characterDigits.digits[i];
                        work->numbers[i].comparisonColor = 0;
                        work->numbers[i].firstOnly = 1;
                        work->numbers[i].value.scalar = MenuShopPara.value[i];
                    }
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                    switch (category) {
                    case 4: MenuShopParaSet(13); break;
                    case 5: MenuShopParaSet(14); break;
                    case 6: MenuShopParaSet(15); break;
                    case 7: MenuShopParaSet(16); break;
                    }
                    work->count = 6;
                    work->twoColumns = 1;
                    for (i = 0; i < work->count; i++)
                        work->labels[i].text = msg01_10[i];
                    for (i = 0; i < work->count; i++) {
                        work->numbers[i].format = 5;
                        work->numbers[i].digits = agwsDigits.digits[i];
                        work->numbers[i].comparisonColor = 0;
                        work->numbers[i].firstOnly = 1;
                        work->numbers[i].value.scalar = MenuShopPara.value[i];
                    }
                break;
            }
        }
            /* fall through */
        case 0xa0:
        case 0xa2:
        case 0xa4:
            if (MenuShopWork->mode == 0 && MenuShopWork->category != 0 && MenuShopWork->category != 8)
                targetX = 0x10;
            break;
        case 0xd0:
        case 0xd2:
        case 0xd4:
        case 0xd6:
            if (MenuShopWork->sort_mode_selector != 0)
                MenuShopParaSet(3);
            else
                MenuShopParaSet(4);
            work->count = 6;
            work->twoColumns = 1;
            for (i = 0; i < work->count; i++)
                work->labels[i].text = msg01_10[i];
            for (i = 0; i < work->count; i++) {
                work->numbers[i].format = 5;
                work->numbers[i].digits = agwsDigits.digits[i];
                work->numbers[i].value.scalar = (unsigned int)MenuShopPara.value[i] +
                    ((unsigned int)MenuShopPara2.value[i] << 16);
                work->numbers[i].comparisonColor = 0;
                work->numbers[i].firstOnly = 1;
                if (MenuShopPara.value[i] < MenuShopPara2.value[i]) {
                    work->numbers[i].comparisonColor = 2;
                    work->numbers[i].firstOnly = 0;
                } else if (MenuShopPara2.value[i] < MenuShopPara.value[i]) {
                    work->numbers[i].comparisonColor = 1;
                    work->numbers[i].firstOnly = 0;
                }
            }
            targetX = 0x10;
            break;
        case 0xc2:
        case 0xc4:
        case 0xc6:
            MenuShopParaSet(2);
            work->count = 6;
            work->twoColumns = 1;
            for (i = 0; i < work->count; i++)
                work->labels[i].text = msg01_10[i];
            for (i = 0; i < work->count; i++) {
                work->numbers[i].format = 5;
                work->numbers[i].comparisonColor = 0;
                work->numbers[i].digits = agwsDigits.digits[i];
                work->numbers[i].firstOnly = 1;
                work->numbers[i].value.scalar = (unsigned int)MenuShopPara.value[i] +
                    ((unsigned int)MenuShopPara2.value[i] << 16);
            }
            work->numbers[0].format = 3;
            work->numbers[0].comparisonColor = 0;
            work->numbers[0].firstOnly = 0;
            targetX = 0x10;
            break;
        }
        MoveSlide(&work->window.x, &targetX, 3.0f);
        if (work->twoColumns == 0) {
            work->window.y = 0x139;
            work->window.height = 0x36;
        } else {
            work->window.y = 0x149;
            work->window.height = 0x26;
        }
        WindowDXMain((WindowDX *)(void *)(&work->window));
        for (i = 0; i < work->count; i++) {
            if (work->twoColumns == 0) {
                work->labels[i].x = (unsigned short)work->window.x + (i / 3) * 0xa0 + 0x30;
                work->labels[i].y = (unsigned short)work->window.y + (i % 3) * 0x10 + 3;
            } else {
                work->labels[i].x = (unsigned short)work->window.x + (i / 2) * 0xa0 + 0x30;
                work->labels[i].y = (unsigned short)work->window.y + (i % 2) * 0x10 + 3;
            }
            eTagFontMain((ETagFontMainData *)(void *)(&work->labels[i]));
        }
        for (i = 0; i < work->count; i++) {
            if (work->twoColumns == 0) {
                work->numbers[i].x = (unsigned short)work->window.x + (i / 3) * 0xa0 + 0x60;
                work->numbers[i].y = (unsigned short)work->window.y + (i % 3) * 0x10 + 3;
            } else {
                work->numbers[i].x = (unsigned short)work->window.x + (i / 2) * 0xa0 + 0x60;
                work->numbers[i].y = (unsigned short)work->window.y + (i % 2) * 0x10 + 3;
            }
            eNumberMain((ENumberMainData *)(void *)(&work->numbers[i]));
        }
        return;
    }
}

static void MenuShopEx2(TskObject *task, MenuShopEx2Work *work)
{
    short x;
    MenuShopWeapon *weapon;

    switch (task->state) {
    case 0:
        work->color = 0xFFFF00;
        WindowDXSet((WindowDX *)(void *)((void *)&work->window));
        work->window.x = -0xB6;
        work->window.color = work->color;
        work->window.y = 0x100;
        work->window.width = 0xA6;
        work->window.height = 0x1E;
        work->window.title = D_004DB230;
        work->window.state = 1;
        WindowDXMain((WindowDX *)(void *)((void *)&work->window));
        work->window.state = 3;
        eMessageSet(&work->message, 0);
        work->message.mode = 0x20;
        work->message.color = work->color + 2;
        break;

    case 2:
        x = -0xB6;
        if (MenuShopWork->state == 0x30 && MenuShopWork->category == 4 && MenuShopWork->listIndex >= 0) {
            weapon = func_A1A3D8((short)MenuSortGet(0, MenuShopWork->listIndex));
            if (weapon != 0 && (weapon->flags & 0x100) && weapon->bullet != 0) {
                work->message.text = *func_A2C738(weapon->bullet);
                x = 0x10;
            }
        }
        MoveSlide(&work->window.x, &x, 3.0f);
        WindowDXMain((WindowDX *)(void *)((void *)&work->window));
        work->message.x = work->window.x + 3;
        work->message.y = work->window.y + 3;
        eMessageMain(&work->message);
        break;
    }
}

/* MenuShopEx2 takes the window title by address; the string follows it in .sdata. */
const char D_004DB230[8] = "Bullet";

/* PrintLine consumes x/y, depth and RGBA records until a zero depth. */
typedef struct MenuShopDividerVertex {
    short x;
    short y;
    int depth;
    union {
        unsigned char color[4];
        struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            unsigned char a;
        } rgba;
    };
} MenuShopDividerVertex;



typedef struct MenuShopDividerWork {
    unsigned char unmodeled_00[3];
    unsigned char alpha;
    int depth;
    MenuShopDividerVertex vertices[6];
} MenuShopDividerWork;



extern void endPrintExtFunc(int depth, int mode, void *vertices);

static void MenuShopLine(TskObject *task, MenuShopDividerWork *work)
{
    short targets[4];
    short verticalTarget[1];
    int i;
    short *target;

    switch (task->state) {
    case 0:
        work->depth = 0xfffff0;
        for (i = 0; i < 5; i++) {
            work->vertices[i].rgba.r = work->vertices[i].rgba.g =
                work->vertices[i].rgba.b = 96;
            work->vertices[i].rgba.a = 128;
            work->vertices[i].x = 0;
            work->vertices[i].y = 48;
            work->vertices[i].depth = work->depth;
        }
        work->vertices[5].depth = 0;
        break;
    case 2:
        verticalTarget[0] = 48;
        targets[0] = MenuShopWork->category * 21 + 294;
        targets[1] = MenuShopWork->category * 21 + 298;
        targets[2] = MenuShopWork->category * 21 + 302;
        targets[3] = 512;
        switch (MenuShopWork->state) {
        case 0x30:
        case 0x90:
        case 0xa0:
        case 0xa2:
        case 0xa4:
            if (targets[0] == work->vertices[1].x)
                verticalTarget[0] = 40;
            if (work->alpha != 120)
                work->alpha += 8;
            break;
        default:
            if (work->alpha != 0)
                work->alpha -= 8;
            break;
        }
        work->vertices[0].x = 0;
        work->vertices[5].x = 512;
        target = targets;
        for (i = 1; i < 5; i++, target++) {
            MoveSlide(&work->vertices[i].x, target, 3.0f);
            work->vertices[i].rgba.a = work->alpha;
        }
        work->vertices[0].rgba.a = work->alpha;
        MoveSlide(&work->vertices[2].y, verticalTarget, 8.0f);
        endPrintExtFunc(work->depth, 0, work->vertices);
        break;
    }
}

void MenuShopInfoSet(void)
{
    MenuShopWork->categoryMask = 0;

    if (MenuShopWork->mode == 0) {
        if (MenuShopWork->noCategories != 0)
            return;
        if (ShopData.records[0].count != 0)
            MenuShopWork->categoryMask = 1;
        if (ShopData.records[1].count != 0)
            MenuShopWork->categoryMask |= 0x2;
        if (ShopData.records[2].count != 0)
            MenuShopWork->categoryMask |= 0x4;
        if (ShopData.records[3].count != 0)
            MenuShopWork->categoryMask |= 0x8;
        if (ShopData.records[4].count != 0)
            MenuShopWork->categoryMask |= 0x10;
        if (ShopData.records[5].count != 0)
            MenuShopWork->categoryMask |= 0x20;
        if (ShopData.records[6].count != 0)
            MenuShopWork->categoryMask |= 0x40;
        if (ShopData.tail.count != 0)
            MenuShopWork->categoryMask |= 0x80;
        if (MenuScenarioNo >= 115)
            MenuShopWork->categoryMask |= 0x100;
    } else {
        MenuSortSet(0, 1, 0);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x1;
        MenuSortSet(0, 4, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x2;
        MenuSortSet(0, 16, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x4;
        MenuSortSet(0, 8, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x8;
        MenuSortSet(0, 4, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x10;
        MenuSortSet(0, 16, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x20;
        MenuSortSet(0, 8, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x40;
        if (dataEvtBoxChk(53) != 0 || dataEvtBoxChk(54) != 0 || dataEvtBoxChk(55) != 0)
            MenuShopWork->categoryMask |= 0x100;
    }
}


/* Each list selection updates the item and its signed model key together. */
#define MENU_SHOP_PREVIEW_ITEM(listIndex) \
    { \
        MenuShopWork->itemId = MenuSortGet(0, (listIndex)); \
        MenuShopWork->modelId = (s16) MenuShopWork->itemId; \
    }

typedef struct MenuShopSelection {
    u8 mode;
    u8 category;
    s8 quantity;
    u8 quantityAction;
    u8 quantityLimit;
} MenuShopSelection;



typedef struct MenuShopItem {
    int listIndex;
    int itemId;
    int modelId;
} MenuShopItem;



typedef struct MenuShopAgws {
    short unitId;
    short equipmentId;
    signed char equipmentStatus;
    signed char unitCount;
    signed char allPartyCount;
    signed char unitSelection;
    signed char sort_mode_selector;
} MenuShopAgws;



static inline MenuShopSelection *MenuShopSelectionGet(void)
{
    return (MenuShopSelection *)&MenuShopWork->mode;
}

static inline MenuShopItem *MenuShopItemGet(void)
{
    return (MenuShopItem *)&MenuShopWork->listIndex;
}

static inline MenuShopAgws *MenuShopAgwsGet(void)
{
    return (MenuShopAgws *)&MenuShopWork->unitId;
}

typedef struct MenuShopParty {
    short unitIds[6];
} MenuShopParty;



static inline MenuShopParty *MenuShopPartyGet(void)
{
    return (MenuShopParty *)MenuShopWork->unitIds;
}

void MenuShopCore(void) {
    if (MenuShopWork->state != MenuShopWork->nextState) {
        MenuShopWork->state = MenuShopWork->nextState;
        MenuShopWork->stateFlags |= 1;
    } else {
        MenuShopWork->stateFlags &= 0xFE;
    }
    if (MenuShopWork->waitFrames != 0) {
        MenuShopWork->waitFrames--;
    }

    switch (MenuShopWork->state) {
    case 0x20:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->stateFlags |= 0x10;
            MenuShopWork->waitFrames = 8;
            break;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->mode = MenuSelectMove(MenuShopWork->mode, 4, 0);
        if (PadData.half_2a == 0x20)
            switch (MenuShopWork->mode) {
            case 0:
            case 1: {
                u8 listStates[2] = { 0x30, 0x90 };
                u8 *nextStates = listStates;
                int categoryIndex;

                MenuShopInfoSet();
                if (MenuShopWork->categoryMask != 0) {
                    for (categoryIndex = 0; categoryIndex < 8; categoryIndex++) {
                        if ((MenuShopWork->categoryMask >> categoryIndex) & 1) {
                            MenuShopWork->category = categoryIndex;
                            break;
                        }
                    }
                    MenuShopWork->nextState = nextStates[MenuShopWork->mode];
                    MenuShopWork->stateFlags |= 0xC;
                    xglSoundEffectNormalID(1, 0);
                    MenuShopWork->waitFrames = 0xC;
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
                break;
            }
            case 2:
                if (MenuShopWork->equipmentStatus != 0) {
                    xglSoundEffectNormalID(1, 0);
                    MenuShopWork->nextState = 0xC0;
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
                break;
            case 3:
                xglSoundEffectNormalID(2, 0);
                MenuShopWork->nextState = 0xF0;
                break;
            }
        if (PadData.half_2a & 0x40) {
            MenuShopWork->nextState = 0xF0;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0x30:
    case 0x90:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->listIndex = MenuShopListChange00(1);
            MenuShopWork->itemId = MenuShopWork->modelId = 0;
            MenuShopWork->availableMoney = dataMoneyBoxChk();
        }
        if (MenuShopWork->waitFrames == 0) {
            MenuShopWork->listIndex = WindowSPSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), PadData.mappedRepeat);
            WindowSPKeepSelect((WindowSPCursor *)(void *)(MenuShopWinSP), &MenuKeepSelect[(MenuShopWork->category + MenuShopWork->mode * 9) * 5]);
            if (MenuShopWork->listIndex >= 0) {
                MenuShopWork->canEquip = subMenuShopEquipCheck(MenuSortGet(0, MenuShopWork->listIndex), MenuShopWork->category);
            } else {
                MenuShopWork->canEquip = 0;
            }
            if (MenuShopWork->listIndex >= 0) {
                MENU_SHOP_PREVIEW_ITEM(MenuShopWork->listIndex);
            } else {
                MenuShopWork->itemId = *(unsigned int *)&MenuShopWork->modelId = 0;
            }
            if (PadData.mappedRepeat & 0xA000) {
                u8 previousCategory;
                int categoryStep;
                int category;
                int count;

                previousCategory = MenuShopWork->category;
                WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[(previousCategory + MenuShopWork->mode * 9) * 5]));
                categoryStep = (PadData.mappedRepeat & 0x2000) ? 1 : -1;
                category = MenuShopWork->category;
                for (count = 0; count < 9; count += categoryStep) {
                    category += categoryStep;
                    if (category < 0) {
                        category = 8;
                    }
                    if (category >= 9) {
                        category = 0;
                    }
                    if ((MenuShopWork->categoryMask >> category) & 1) {
                        MenuShopWork->category = category;
                        break;
                    }
                }
                if (previousCategory != MenuShopWork->category) {
                    xglSoundEffectNormalID(1, 0);
                    MenuShopWork->listIndex = MenuShopListChange00(0);
                    if (MenuShopWork->listIndex >= 0) {
                        MenuShopWork->canEquip = subMenuShopEquipCheck(MenuSortGet(0, MenuShopWork->listIndex), MenuShopWork->category);
                        MENU_SHOP_PREVIEW_ITEM(MenuShopWork->listIndex);
                    }
                    MenuShopWork->waitFrames = 8;
                }
            } else if (PadData.half_2a & 0x20) {
                if (MenuSortCheck(0) != 0) {
                    if (MenuShopWork->mode == 0) {
                        if (MenuBoxMoneyGet(MenuShopWork->itemId, 0) > MenuShopWork->availableMoney
                            || MenuBoxChk(MenuShopWork->itemId) >= 99) {
                            xglSoundEffectNormalID(5, 0);
                        } else {
                            MenuShopWork->nextState = 0xA0;
                            MenuShopWork->quantity = 1;
                            MenuShopWork->quantityLimit = MenuBoxChk(MenuShopWork->itemId);
                            xglSoundEffectNormalID(1, 0);
                        }
                    } else if (MenuShopNoSaleCheck(MenuShopWork->itemId) != 0) {
                        xglSoundEffectNormalID(5, 0);
                    } else {
                        MenuShopWork->nextState = 0xA0;
                        MenuShopWork->quantity = 1;
                        MenuShopWork->quantityLimit = MenuBoxChk(MenuShopWork->itemId);
                        xglSoundEffectNormalID(1, 0);
                    }
                }
            } else if (PadData.half_2a & 0x40) {
                if (MenuCursorKeepCheck() == 0) {
                    MenuShopWork->mode = 0;
                }
                MenuShopWork->nextState = 0x20;
                xglSoundEffectNormalID(2, 0);
            }
        } else {
            MenuShopWork->listIndex = WindowSPSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), 0);
        }
        break;

    case 0xA0: {
        struct MenuShopQuantityChangeState {
            int maximumQuantity;
            int calculatedTotal;
            int playChangeSound;
            int minimumQuantity;
        } quantityState;
        int i;

        void subMenuShopNumerInc(void)
        {
            int unitCost;

            if (MenuShopWork->quantity < quantityState.maximumQuantity) {
                unitCost = MenuBoxMoneyGet(MenuShopWork->itemId, 0);
                quantityState.calculatedTotal = unitCost * (MenuShopWork->quantity + 1);
                if (MenuShopWork->availableMoney >= quantityState.calculatedTotal) {
                    MenuShopWork->quantity++;
                    if (quantityState.playChangeSound != 0) {
                        xglSoundEffectNormalID(3, 0);
                        quantityState.playChangeSound = 0;
                    }
                }
            }
        }

        void subMenuShopNumerDec(void)
        {
            if (quantityState.minimumQuantity < MenuShopWork->quantity) {
                MenuShopWork->quantity = MenuShopWork->quantity - 1;
                if (quantityState.playChangeSound != 0) {
                    xglSoundEffectNormalID(3, 0);
                    quantityState.playChangeSound = 0;
                }
            }
        }

        if (MenuShopWork->stateFlags & 1) {
            if (MenuShopWork->category == 7) {
                MenuShopWork->nextState = 0xA2;
                MenuShopWork->waitFrames = 1;
            } else {
                MenuShopWork->waitFrames = 8;
            }
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        if (MenuShopWork->mode == 0) {
            quantityState.maximumQuantity = 99 - MenuShopWork->quantityLimit;
            quantityState.minimumQuantity = 1;
            quantityState.playChangeSound = 1;
            quantityState.calculatedTotal = 0;
            if (PadData.mappedRepeat & 0x4000) {
                subMenuShopNumerDec();
            }
            if (PadData.mappedRepeat & 0x1000) {
                subMenuShopNumerInc();
            }
            if (PadData.mappedRepeat & 0x2000) {
                for (i = 0; i < 10; i++) {
                    subMenuShopNumerInc();
                }
            }
            if (PadData.mappedRepeat & 0x8000) {
                for (i = 0; i < 10; i++) {
                    subMenuShopNumerDec();
                }
            }
        } else {
            int minimumQuantity = 1;
            u8 quantityLimit;

            quantityLimit = MenuShopSelectionGet()->quantityLimit;
            if ((PadData.mappedRepeat & 0x4000) && MenuShopSelectionGet()->quantity != minimumQuantity) {
                MenuShopSelectionGet()->quantity--;
                xglSoundEffectNormalID(3, 0);
            }
            if ((PadData.mappedRepeat & 0x1000) && MenuShopSelectionGet()->quantity != quantityLimit) {
                MenuShopSelectionGet()->quantity++;
                xglSoundEffectNormalID(3, 0);
            }
            if ((PadData.mappedRepeat & 0x2000) && MenuShopSelectionGet()->quantity != quantityLimit) {
                MenuShopSelectionGet()->quantity += 10;
                if (MenuShopSelectionGet()->quantity > quantityLimit) {
                    MenuShopSelectionGet()->quantity = quantityLimit;
                }
                xglSoundEffectNormalID(3, 0);
            }
            if ((PadData.mappedRepeat & 0x8000) && MenuShopSelectionGet()->quantity != minimumQuantity) {
                MenuShopSelectionGet()->quantity -= 10;
                if (MenuShopSelectionGet()->quantity < minimumQuantity) {
                    MenuShopSelectionGet()->quantity = minimumQuantity;
                }
                xglSoundEffectNormalID(3, 0);
            }
        }
        if (PadData.half_2a & 0x20) {
            MenuShopWork->nextState = 0xA2;
            xglSoundEffectNormalID(1, 0);
        }
        if (PadData.half_2a & 0x40) {
            if (MenuShopWork->mode == 0) {
                MenuShopWork->nextState = 0x30;
            } else {
                MenuShopWork->nextState = 0x90;
            }
            xglSoundEffectNormalID(2, 0);
        }
        break;
    }

    case 0xA2:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->quantityAction = 0;
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->quantityAction = MenuSelectMove(MenuShopWork->quantityAction, 2, 0);
        if (PadData.half_2a & 0x20) {
            if (MenuShopSelectionGet()->quantityAction == 0) {
                int i;

                if (MenuShopSelectionGet()->mode == 0) {
                    for (i = 0; i < MenuShopSelectionGet()->quantity; i++) {
                        MenuBoxInc(MenuSortGet(0, MenuShopWork->listIndex));
                    }
                    dataMoneyBoxDec(MenuBoxMoneyGet(MenuShopWork->itemId, 0) * MenuShopWork->quantity);
                } else {
                    for (i = 0; i < MenuShopSelectionGet()->quantity; i++) {
                        MenuBoxDec(MenuShopWork->itemId);
                    }
                    dataMoneyBoxInc(MenuBoxMoneyGet(MenuShopWork->itemId, 1) * MenuShopWork->quantity);
                    MenuShopWork->stateFlags |= 0x10;
                }
                MenuShopWork->nextState = 0xA4;
            } else if (MenuShopSelectionGet()->category == 7) {
                u8 listStates[2] = { 0x30, 0x90 };

                MenuShopWork->nextState = *(listStates + MenuShopSelectionGet()->mode);
            } else {
                MenuShopWork->nextState = 0xA0;
            }
            xglSoundEffectNormalID(1, 0);
        }
        if (PadData.half_2a & 0x40) {
            if (MenuShopSelectionGet()->category == 7) {
                u8 listStates[2] = { 0x30, 0x90 };

                MenuShopWork->nextState = *(listStates + MenuShopSelectionGet()->mode);
            } else {
                MenuShopWork->nextState = 0xA0;
            }
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0xA4:
        if (MenuShopWork->stateFlags & 1) {
            if (MenuShopWork->mode == 0 && MenuShopWork->category == 7) {
                int partyUnitIds[16];
                int i;

                MenuShopWork->unitCount = PartyAgwsGet(partyUnitIds);
                for (i = 0; i < MenuShopWork->unitCount; i++) {
                    MenuShopPartyGet()->unitIds[i] = partyUnitIds[i];
                }
                MenuShopAgwsListChange();
            }
            MenuShopWork->listIndex = MenuShopListChange00(1);
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        if (PadData.half_2a & 0x20) {
            u8 listStates[2] = { 0x30, 0x90 };

            MenuShopWork->nextState = *(listStates + MenuShopWork->mode);
            if (MenuShopWork->mode == 0) {
                MenuShopListColorChange(0, 0);
            }
            xglSoundEffectNormalID(1, 0);
        }
        break;

    case 0xC0:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->sortOption = MenuSelectMove(MenuShopWork->sortOption, 4, 0);
        if (PadData.mappedPressed == 0x20) {
            switch (MenuShopWork->sortOption) {
            case 0:
            case 1:
                if (MenuShopWork->sortOption == 0) {
                    MenuShopWork->sort_mode_selector = 1;
                } else {
                    MenuShopWork->sort_mode_selector = 0;
                }
                xglSoundEffectNormalID(1, 0);
                if (MenuShopWork->sort_mode_selector != 0) {
                    WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[0x5A]));
                } else {
                    WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[0x5F]));
                }
                MenuShopWork->listIndex = MenuShopListChange01(0);
                MenuShopWork->nextState = 0xD0;
                MenuShopWork->waitFrames = 0xC;
                break;
            case 2:
                if (dataMoneyBoxChk() >= 100) {
                    xglSoundEffectNormalID(1, 0);
                    MenuShopWork->nextState = 0xC2;
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
                MenuShopWork->waitFrames = 0xC;
                break;
            case 3:
                xglSoundEffectNormalID(2, 0);
                MenuShopWork->nextState = 0x20;
                break;
            }
        }
        if (PadData.mappedPressed == 0x40) {
            if (MenuCursorKeepCheck() == 0) {
                MenuShopWork->mode = 0;
            }
            MenuShopWork->nextState = 0x20;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0xC2:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->waitFrames = 0xC;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->unitSelection = MenuSelectMove2(MenuShopAgwsGet()->unitSelection, MenuShopAgwsGet()->unitCount, 0);
        MenuShopAgwsGet()->unitId = MenuShopPartyGet()->unitIds[MenuShopAgwsGet()->unitSelection];
        if (PadData.mappedPressed == 0x20) {
            if (dataMoneyBoxChk() >= 100 && MenuCharHpCheck(MenuShopWork->unitId) == 0) {
                MenuShopWork->nextState = 0xC4;
                xglSoundEffectNormalID(1, 0);
            } else {
                xglSoundEffectNormalID(5, 0);
            }
        }
        if (PadData.mappedPressed == 0x40) {
            if (MenuCursorKeepCheck() == 0) {
                MenuShopWork->sortOption = 0;
            }
            MenuShopWork->nextState = 0xC0;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0xC4:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->quantityAction = 0;
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->quantityAction = MenuSelectMove(MenuShopWork->quantityAction, 2, 0);
        if (PadData.mappedPressed == 0x20) {
            if (MenuShopWork->quantityAction == 0) {
                MenuShopWork->nextState = 0xC6;
                xglSoundEffectNormalID(1, 0);
            } else {
                MenuShopWork->nextState = 0xC2;
                xglSoundEffectNormalID(2, 0);
            }
        }
        if (PadData.mappedPressed == 0x40) {
            MenuShopWork->nextState = 0xC2;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0xC6:
        if (MenuShopWork->stateFlags & 1) {
            int repairAttack[4];
            int repairDefense[4];
            MenuShopUnitData *unit;
            ShopCalculatedPara *para;

            unit = func_A191C0(MenuShopPartyGet()->unitIds[MenuShopWork->unitSelection]);
            para = func_00A11108(MenuShopPartyGet()->unitIds[MenuShopWork->unitSelection], repairAttack, repairDefense);
            unit->hitPoints = para->maxHp;
            dataMoneyBoxDec(100);
            MenuShopWork->waitFrames = 2;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        if (PadData.mappedPressed == 0x20) {
            if (dataMoneyBoxChk() >= 100) {
                MenuShopWork->nextState = 0xC2;
            } else {
                MenuShopWork->nextState = 0xC0;
            }
            xglSoundEffectNormalID(1, 0);
        }
        break;

    case 0xD0:
        if (MenuShopWork->waitFrames == 0) {
            MenuShopWork->listIndex = WindowSPSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), PadData.mappedRepeat);
            if (MenuShopWork->sort_mode_selector != 0) {
                WindowSPKeepSelect((WindowSPCursor *)(void *)(MenuShopWinSP), &MenuKeepSelect[0x5A]);
            } else {
                WindowSPKeepSelect((WindowSPCursor *)(void *)(MenuShopWinSP), &MenuKeepSelect[0x5F]);
            }
            if (MenuShopWork->listIndex >= 0) {
                int itemId;
                int unitId;

                itemId = MenuSortGet(0, MenuShopWork->listIndex);
                MenuShopItemGet()->itemId = itemId;
                MenuShopItemGet()->modelId = (s16)MenuShopItemGet()->itemId;
                if (MenuShopItemGet()->listIndex >= 0) {
                    MenuShopAgwsGet()->equipmentId = MenuShopItemGet()->modelId;
                    MenuShopAgwsGet()->unitId = 0x11;
                    for (unitId = 0x11; unitId < 0x21; unitId++) {
                        if (MenuFrameEquipCheck(unitId, MenuShopWork->equipmentId) != 0) {
                            MenuShopWork->unitId = unitId;
                            break;
                        }
                    }
                    if (PadData.mappedPressed == 0x20) {
                        int money;

                        money = dataMoneyBoxChk();
                        if (money >= MenuBoxMoneyGet(MenuShopWork->itemId, 0)) {
                            MenuShopWork->nextState = 0xD4;
                            xglSoundEffectNormalID(1, 0);
                            MenuShopWork->waitFrames = 2;
                            if (MenuCursorKeepCheck() == 0) {
                                MenuShopWork->unitSelection = 0;
                            }
                        } else {
                            xglSoundEffectNormalID(5, 0);
                            MenuShopWork->waitFrames = 2;
                        }
                    }
                }
            }
            if (PadData.mappedPressed == 0x40) {
                if (MenuCursorKeepCheck() == 0) {
                    MenuShopWork->sortOption = 0;
                }
                MenuShopWork->nextState = 0xC0;
                MenuShopWork->waitFrames = 2;
                xglSoundEffectNormalID(2, 0);
            }
        } else {
            MenuShopWork->listIndex = WindowSPSelect((WindowSPCursor *)(void *)((void *)MenuShopWinSP), 0);
            if (MenuShopWork->listIndex >= 0) {
                int itemId;

                itemId = MenuSortGet(0, MenuShopWork->listIndex);
                MenuShopItemGet()->itemId = itemId;
                MenuShopItemGet()->modelId = (s16)MenuShopItemGet()->itemId;
                MenuShopWork->equipmentId = MenuShopItemGet()->modelId;
            }
        }
        break;

    case 0xD2: {
        MenuShopUnitData *unit;
        int unitSelection;
        int canEquip;
        s16 previousEquipmentId;

        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        unitSelection = MenuSelectMove2(MenuShopAgwsGet()->unitSelection, MenuShopAgwsGet()->unitCount, 0);
        if (MenuShopAgwsGet()->unitSelection != unitSelection) {
            MenuAgwsParaSet(MenuShopAgwsGet()->unitId, 10);
        }
        MenuShopWork->unitSelection = unitSelection;
        MenuShopAgwsGet()->unitId = MenuShopPartyGet()->unitIds[MenuShopAgwsGet()->unitSelection];
        if (PadData.mappedPressed == 0x20) {
            if (MenuShopAgwsGet()->sort_mode_selector != 0) {
                canEquip = MenuFrameEquipCheck(MenuShopAgwsGet()->unitId, MenuShopAgwsGet()->equipmentId);
            } else {
                canEquip = MenuEngineEquipCheck(MenuShopAgwsGet()->unitId, MenuShopAgwsGet()->equipmentId);
            }
            if (canEquip != 0) {
                MenuShopWork->nextState = 0xD4;
                xglSoundEffectNormalID(1, 0);
            } else {
                xglSoundEffectNormalID(5, 0);
            }
        }
        if (PadData.mappedPressed == 0x40) {
            MenuShopWork->nextState = 0xD0;
            xglSoundEffectNormalID(2, 0);
        }
        unit = func_A191C0(MenuShopWork->unitId);
        if (MenuShopAgwsGet()->sort_mode_selector != 0) {
            previousEquipmentId = unit->frameId;
            unit->frameId = MenuShopAgwsGet()->equipmentId;
        } else {
            previousEquipmentId = unit->engineId;
            unit->engineId = MenuShopAgwsGet()->equipmentId;
        }
        MenuAgwsParaSet(MenuShopWork->unitId, 1);
        if (MenuShopWork->sort_mode_selector != 0) {
            unit->frameId = previousEquipmentId;
        } else {
            unit->engineId = previousEquipmentId;
        }
        break;
    }

    case 0xD4:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->quantityAction = 0;
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->quantityAction = MenuSelectMove(MenuShopWork->quantityAction, 2, 0);
        if (PadData.mappedPressed == 0x20) {
            if (MenuShopWork->quantityAction == 0) {
                MenuShopWork->nextState = 0xD6;
                xglSoundEffectNormalID(1, 0);
            } else {
                MenuShopWork->nextState = 0xD0;
                xglSoundEffectNormalID(2, 0);
            }
        }
        if (PadData.mappedPressed == 0x40) {
            MenuShopWork->nextState = 0xD0;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    case 0xD6:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopUnitData *unit;
            int equipmentKey;

            unit = func_A191C0(MenuShopWork->unitId);
            if (MenuShopAgwsGet()->sort_mode_selector != 0) {
                unit->frameId = MenuShopAgwsGet()->equipmentId;
            } else {
                unit->engineId = MenuShopAgwsGet()->equipmentId;
            }
            MenuAgwsParaSet(MenuShopWork->unitId, 0);
            if (MenuShopAgwsGet()->sort_mode_selector != 0) {
                equipmentKey = (u16)MenuShopAgwsGet()->equipmentId + 0xB0000;
            } else {
                equipmentKey = (u16)MenuShopAgwsGet()->equipmentId + 0xC0000;
            }
            dataMoneyBoxDec(MenuBoxMoneyGet(equipmentKey, 0));
            MenuShopListColorChange(0, 2);
            MenuShopWork->waitFrames = 8;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        if (PadData.mappedPressed == 0x20) {
            MenuShopWork->nextState = 0xD0;
            xglSoundEffectNormalID(1, 0);
        }
        break;

    case 0xF0:
        if (MenuShopWork->stateFlags & 1) {
            MenuShopWork->waitFrames = 0x10;
        }
        if (MenuShopWork->waitFrames != 0) {
            break;
        }
        MenuShopWork->nextState = 0xFF;
        break;
    }
    MenuShopWork->stateFlags &= 0xFD;
    xglFontDebugHex(0, 0x48, MenuShopWork->state, 2);
    xglFontDebugHex(0, 0x50, MenuShopWork->nextState, 2);
    xglFontDebugPrintf(0, 0x60, D_004C7A68, MenuShopWork->mode);
    xglFontDebugPrintf(0, 0x68, D_004C7A78, MenuShopWork->category);
    xglFontDebugPrintf(0, 0x70, D_004C7A88, MenuShopWork->listIndex);
    xglFontDebugPrintf(0, 0x80, D_004C7A98, MenuShopWork->unitSelection);
}

void MenuShopMain(int shopType)
{
    int partyIds[16];
    unsigned char *workspace;
    unsigned char *modelMemory;
    int memberIndex;
    int unitCount;

    extern unsigned int GameLoopState[];
    extern XglTaskScheduler *MenuShopTask;
    extern unsigned char *ShopDataBufTop;
    extern const char D_004C7AA8[];
    extern void *MenuWorkEndGet(void);
    extern unsigned char *MenuBgTaskInit(unsigned char *work, int cameraId);
    extern void *xglTaskInitial(void *pool, int capacity, int flags);
    extern void *MenuModelInit(void *workStart);
    extern unsigned int GameResourceWorkAlloc(int size);
    extern void MenuModelMemoryInit(void *base, int size);
    extern void MenuModelMemorySet(int count);
    extern void MenuModelMenuMotionLoad(void);
    extern void MenuWorkEndCheck(void *workEnd);
    extern void MenuLoadInit(void);
    extern void MenuLoadCancel(void);
    extern int MenuLoadSync(void);
    extern void MenuLoadFile(const char *name, void *buffer);
    extern void MenuBgTaskMain(void);
    extern void MenuBgTaskBreak(void);
    extern void MenuCfTaikiPush(void);
    extern void MenuCfTaikiPop(void);
    extern void MenuGameDataPush(void);
    extern void MenuGameDataPop(void);
    extern void endPrintInit(void);
    extern void endBackTexDraw(void *color);
    extern void MenuFontLoad(int font, int option);
    extern int WindowTexLoad(unsigned char *buffer, unsigned int request);
    extern int MenuEquipStealMaskCheck(void);
    extern void MenuModelMain(void);
    extern void MenuModelAllBreak(void);
    extern void nmlModelFlush(void);
    extern void GameResourceWorkReload(void);
    extern void GameCfPlayerLoadResource(int mode);
    extern void MenuShopCore(void);
    extern void endPrintExtFunc(int depth, int mode, void *vertices);
    extern void endPrintDirectFrameCopy(XglPacket *packet, int x, int y);
    extern void xglStudioChange(int mode);

    endPrintDirectFrameCopy(xglPacketGetCurrent(), (int)sRender.width << 5,
        (int)sRender.height << 5);
    GameLoopState[4] |= 0x04000000;
    xglSleep();
    endPrintInit();
    GameLoopState[4] |= 0x04000000;
    MenuCfTaikiPush();
    MenuGameDataPush();
    xglStudioChange(0);
    endPrintDirectFrameCopy(xglPacketGetCurrent(), (int)sRender.height << 5,
        (int)sRender.flip_base << 5);

    workspace = (unsigned char *)(((unsigned int)MenuWorkEndGet() + 127) & ~127U);
    ShopDataBufTop = workspace;
    {
        MenuShopWorkData *initialWork =
            (MenuShopWorkData *)MenuShopAlignBuffer(workspace + 8192, 2048);
        MenuShopWork = initialWork;
        workspace = (unsigned char *)initialWork + 128;
        memset(initialWork, 0, 128);
    }
    workspace = MenuBgTaskInit(workspace, 3);
    MenuShopTask = (XglTaskScheduler *)workspace;
    workspace = xglTaskInitial(workspace, 16, 0);

    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopPas, workspace);
    workspace += 1516;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopInfo, workspace);
    workspace += 976;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopList, workspace);
    MenuShopWinSP = (MenuShopWindowSP *)(workspace + 12);
    workspace += 5956;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopSelect, workspace);
    workspace += 7928;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopEx, workspace);
    workspace += 1728;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopEx2, workspace);
    workspace += 480;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopEx02, workspace);
    workspace += 472;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopSeisan, workspace);
    workspace += 1668;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopSeisan02, workspace);
    workspace += 984;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopIcon, workspace);
    workspace += 508;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopParameter, workspace);
    workspace += 1996;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopLine, workspace);
    workspace += 80;
    TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask,
        (int (*)(XglTaskPrefix *))tskTskMain3,
        MenuShopTask != 0 ? MenuShopTask->active_tail : 0),
        MenuShopModelMain, 0);
    workspace = MenuModelInit(workspace);

    modelMemory = (unsigned char *)GameResourceWorkAlloc(0x180000);
    xglStudioChange(0);
    MenuModelMemoryInit(modelMemory, 0x180000);
    MenuModelMemorySet(20);
    MenuModelMenuMotionLoad();
    MenuWorkEndCheck(workspace);
    MenuLoadInit();
    endBackTexDraw((void *)-1);
    xglSleep();
    MenuFontLoad(1, 1);
    WindowTexLoad(0, 0x10001);
    WindowTexLoad(0, 0x10002);
    MenuLoadFile(D_004C7AA8, ShopDataBufTop);
    xglSoundEffectNormalID(1, 0);

    while (MenuLoadSync() != 0) {
        MenuBgTaskMain();
        xglSleep();
    }

    ShopData = ((const MenuShopData *)ShopDataBufTop)[shopType - 1];
    if (shopType == 20)
        MenuShopWork->noCategories = 1;
    if (shopType == 6 && MenuEquipStealMaskCheck() == 1)
        MenuShopWork->noCategories = 1;
    MenuShopAgwsListChange();
    MenuShopWork->nextState = 32;
    MenuKeepSelectReset();
    MenuShopWork->partyCount = PartyAllPartyGet(partyIds, 1);
    {
        MenuShopWorkData *partyWork = MenuShopWork;
        memberIndex = 0;
        if (partyWork->partyCount > 0) {
            int *partySource = partyIds;
            do {
                MenuShopAppendPartyId(partyWork, &memberIndex, &partySource);
            } while (memberIndex < partyWork->partyCount);
        }
    }
    MenuShopWork->unitCount = PartyAgwsGet(partyIds);
    memberIndex = 0;
    unitCount = MenuShopWork->unitCount;
    if (unitCount > 0) {
        MenuShopWorkData *unitWork = MenuShopWork;
        int *unitSource = partyIds;
        do {
            MenuShopAppendUnitId(unitWork, &memberIndex, &unitSource);
        } while (memberIndex < unitWork->unitCount);
        unitCount = MenuShopWork->unitCount;
    }

    if (unitCount != 0 && ShopData.records[7].count != 0)
        MenuShopWork->equipmentStatus = 1;
    else
        MenuShopWork->equipmentStatus = 0;
    if (ShopData.tail.count != 0)
        MenuShopWork->modelBreakPending = 1;
    else
        MenuShopWork->modelBreakPending = 0;
    MenuScenarioNo = MenuScenarioNoGet();

    while ((GameSnapShotCheck(), MenuShopWork->state != 0xff) &&
        (MenuShopCore(), 1)) {
        endPrintExtFunc(0, 100, 0);
        MenuBgTaskMain();
        nmlModelFlush();
        xglTaskExecute(MenuShopTask);
        MenuModelMain();
        xglSleep();
    }

    MenuLoadCancel();
    MenuBgTaskBreak();
    MenuShopWork->state = 0xff;
    xglTaskExecute(MenuShopTask);
    if (MenuShopWork->modelBreakPending != 0)
        MenuModelAllBreak();
    MenuCfTaikiPop();
    MenuFontLoad(0, 0);
    WindowTexLoad(0, 0);
    GameResourceWorkReload();
    GameCfPlayerLoadResource(1);
    MenuGameDataPop();
    GameLoopState[4] &= ~0x04000000;
}

/* MenuShopModelMain copies all four coordinates to the camera at +0xD0. */
const float D_004C7390[4] = { -2.0f, 1.8f, 8.5f, 1.0f };

extern const char D_004C7670[];
extern const char D_004C7698[];

/* MenuShopInfo copies this pair and chooses the AG or VX maker prompt. */
const char *D_004DB138[2] = { D_004C7670, D_004C7698 };

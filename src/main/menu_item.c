#include "common.h"

#include "shared.h"
#include "main/menu_1.h"
#include "main/e_battle_win_open.h"
#include "main/menu_top_command_check.h"

#define MENU_ITEM_SPECIAL_ID 36

#define MENU_ITEM_USE_ALLOWED 0x8000

#define MENU_ITEM_USE_CATEGORY_BITS 0x1e43

#define MENU_ITEM_USE_SPECIAL_BIT 0x2000

#define MENU_ITEM_USE_STATE_BIT 0x10

#define MENU_ITEM_SPECIAL_ENABLED_BITS 0x20400000



typedef struct MenuItemPasWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    u8 unmodeled_0c[0x10 - 0x0C];
    signed char state;
    u8 unmodeled_11[0x14 - 0x11];
    void (*callback)(WindowDX *window, void *work);
    void *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
} MenuItemPasWindow;

typedef struct MenuItemInfoText {
    unsigned short x;
    unsigned short y;
    unsigned char unmodeled_04[8];
    const char *message;
    unsigned char unmodeled_10[0x1B0 - 0x10];
} MenuItemInfoText;

typedef struct MenuItemInfoWork {
    unsigned char state;
    unsigned char unmodeled_01[2];
    unsigned char help;
    unsigned char unmodeled_04[4];
    int color;
    MenuItemPasWindow window;
    MenuItemInfoText text;
    char buffer[128];
} MenuItemInfoWork;

typedef struct MenuItemInfoLabels {
    const char *sortMethods[3];
    const char *sortPrefix;
    const char *cancel;
} MenuItemInfoLabels;

extern const MenuItemInfoLabels D_004C4ED0;

typedef struct MenuItemInfoHelpLabels {
    const char *unavailable;
    const char *restricted;
} MenuItemInfoHelpLabels;

extern const MenuItemInfoHelpLabels D_004DAAC8[];

typedef struct MenuItemInfoPad {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2C[8];
    unsigned short activity;
} MenuItemInfoPad;

extern const unsigned char D_004DAAD0[];

extern const char D_004DAAD8[];

extern MenuItemInfoPad PadData;



typedef struct {
    u8 unmodeled_00[4];
    u16 use_flags; /* The high bit gates all item availability checks. */
    u8 unmodeled_06[2];
    u8 use_state;
    u8 unmodeled_09[3];
    u16 attributes;
} MenuItemUseData;

typedef struct {
    u8 unmodeled_00[0x10];
    u64 flags;
} MenuItemLoopState;

extern MenuItemUseData *func_A1A4E8(s16 itemId);

extern MenuItemLoopState GameLoopState;

typedef struct MenuItemListRow {
    u8 unmodeled_00[8];
    u8 flag;
    u8 state;
    u8 unmodeled_0a[2];
} MenuItemListRow;

typedef struct MenuItemWindowSP {
    u8 unmodeled_00;
    u8 rowCount;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    signed char columns;
    signed char rows;
    u8 unmodeled_16[6];
    MenuItemListRow *items;
} MenuItemWindowSP;

typedef struct MenuItemListWork {
    u8 unmodeled_00[0x0C];
    MenuItemWindowSP window;
} MenuItemListWork;

typedef struct MenuItemMessage {
    u8 active;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
} MenuItemMessage;

typedef struct MenuItemExWork {
    u8 state;
    u8 unmodeled_01[3];
    int color;
    MenuItemMessage message;
} MenuItemExWork;


typedef struct MenuItemPasMessage {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
    u8 unmodeled_0c[0x44 - 0x0C];
} MenuItemPasMessage;

typedef struct MenuItemPasBox {
    short x;
    short y;
    int color;
    short width;
    short height;
} MenuItemPasBox;

typedef struct MenuItemPasWork {
    u8 state;
    u8 unmodeled_01[3];
    short x;
    short y;
    int color;
    MenuItemPasWindow window;
    MenuItemPasMessage message[9];
    MenuItemPasBox box;
    u8 callbackWork[1];
} MenuItemPasWork;

typedef struct MenuItemWorkData {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    unsigned char unmodeled_021[0x22 - 0x21];
    u8 busy; /* +0x22 */
    unsigned char unmodeled_023[0x30 - 0x23];
    signed char listTab;
    signed char sortAbility;
    signed char sortNormal;
    signed char sortCategory[3];
    u8 unmodeled_36[0x1A];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x72 - 0x65]; /* +0x65 */
    signed char sortSelection; /* +0x72 */
} MenuItemWorkData;

extern MenuItemPasWork *MenuItemPas;

extern void *MenuItemInfo;

extern void *MenuItemLine;

extern void *MenuItemStatus;

extern void *MenuItemStatusAgws;

extern MenuItemListWork *MenuItemList;

extern void *MenuItemSelect;

extern void *MenuItemIcon;

extern MenuItemExWork *MenuItemEx;

extern void *MenuItemSegment;

extern MenuItemWorkData MenuWork;







extern void endPrintExtFunc(int color, int id, void *data);

extern unsigned char MenuKeepSelect[];

const char D_004C4F50[16] = "List\x03\x8d" "Num";

const char D_004C4F60[32] = "List\x03\x99Num\x03\x99\x03" "3Num";



extern int *MenuSortAddrGet(int list);

extern MenuItemListRow *MenuListGet(int list);

extern void MenuListMake(int list, int mode);

extern int MenuSortCheck(int list);

extern void MenuSortSet(int list, int type, int order);

extern void MenuSortChange(int list, int index);





extern void MoveSlide(short *current, short *target, float rate);

extern void eMessageSet(void *message, const char *text);

extern void eMessageMain(void *message);

extern int xglFlagsGet(int flag, int mode);

int MenuItemUseCheck(s16 itemId)
{
    MenuItemUseData *item;
    u16 attributes;
    int result;
    /* A special item is unavailable when both game-loop bits are clear. */
    int specialUnavailable = -1;

    if (itemId == 0) {
        return 0;
    }
    item = func_A1A4E8(itemId);
    if ((item->use_flags & MENU_ITEM_USE_ALLOWED) == 0) {
        return 0;
    }
    if (itemId == MENU_ITEM_SPECIAL_ID) {
        result = (GameLoopState.flags & MENU_ITEM_SPECIAL_ENABLED_BITS) ? 2 : specialUnavailable;
    } else {
        attributes = item->attributes;
        if (attributes & MENU_ITEM_USE_CATEGORY_BITS) {
            result = (item->use_state & MENU_ITEM_USE_STATE_BIT) ? 2 : 1;
        } else {
            result = (attributes & MENU_ITEM_USE_SPECIAL_BIT) ? 10 : 0;
        }
    }
    return result;
}

void MenuItemPasMain(void)
{
    static const char *msg00[9] = {
        (const char *)0x004DAAB8,
        (const char *)0x004C4E70,
        (const char *)0x004C4E58,
        (const char *)0x004C4E40,
        (const char *)0x004C4E30,
        (const char *)0x004C4E18,
        (const char *)0x004C4E00,
        (const char *)0x004C4DF0,
        (const char *)0x004C4DE0,
    };
    MenuItemPasWork *pas = MenuItemPas;
    short tabTarget[9];
    short slideTarget;
    int i;

    switch (pas->state) {
    case 0:
        pas->x = -272;
        pas->y = 8;
        pas->color = 0xFFFFF0;
        WindowDXSet((WindowDX *)&pas->window);
        pas->window.x = pas->x;
        pas->window.y = pas->y;
        pas->window.color = pas->color;
        pas->window.width = 272;
        pas->window.height = 30;
        pas->window.callback = (void (*)(WindowDX *, void *))MenuPasWindow;
        pas->window.callbackArg = pas->callbackWork;
        pas->window.state = 1;
        WindowDXMain((WindowDX *)&pas->window);
        pas->window.state = 3;
        for (i = 0; i < 9; i++) {
            eMessageSet(&pas->message[i], msg00[i]);
            pas->message[i].mode = 32;
            pas->message[i].x = 288;
            pas->message[i].color = pas->color + 2;
            pas->message[i].y = pas->y + 3;
        }
        pas->state = 2;
        /* fallthrough */
    case 2:
        break;
    default:
        return;
    }

    slideTarget = -16;
    for (i = 0; i < 9; i++)
        tabTarget[i] = 288;

    switch (MenuWork.mode) {
    case 0x40:
    case 0x80:
    case 0x20:
    case 0x22:
        tabTarget[MenuWork.listTab] = 16;
        break;
    case 0x30:
        tabTarget[8] = 16;
        break;
    default:
        slideTarget = -272;
        break;
    }

    MoveSlide(&pas->window.x, &slideTarget, 3.0f);
    WindowDXMain((WindowDX *)&pas->window);
    pas->box.x = pas->window.x + 3;
    pas->box.y = pas->window.y + 3;
    pas->box.color = pas->color;
    pas->box.width = pas->window.width - 6;
    pas->box.height = pas->window.height - 6;
    endPrintExtFunc(pas->color, 0x65, &pas->box);
    for (i = 0; i < 9; i++) {
        MoveSlide(&pas->message[i].x, &tabTarget[i], 3.0f);
        if (pas->message[i].x < 256)
            eMessageMain(&pas->message[i]);
    }
    endPrintExtFunc(pas->color, 0x66, 0);
}

void MenuItemInfoMain(void)
{
    MenuItemInfoWork *info = MenuItemInfo;
    MenuItemInfoLabels labels = D_004C4ED0;
    MenuItemInfoHelpLabels helpLabels = D_004DAAC8[0];
    short targetY;
    short targetHeight;

    switch (info->state) {
    case 0: {
        signed char windowState;
        unsigned short *textX;

        info->color = 0xFFFFF0;
        WindowDXSet((WindowDX *)&info->window);
        info->window.x = -16;
        info->window.y = 480;
        info->window.width = 544;
        info->window.height = 54;
        info->window.color = info->color;
        info->window.callback = (void (*)(WindowDX *, void *))MenuInfoWindow;
        windowState = 1;
        info->window.callbackArg = &info->text;
        textX = info->window.callbackArg;
        *textX = 0;
        info->window.state = windowState;
        info->text.y = 0;
        info->text.message = 0;
        WindowDXMain((WindowDX *)&info->window);
        info->window.state = 3;
        info->help = 0;
        memset(info->buffer, 0, sizeof(info->buffer));
        info->state = 2;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    targetY = 384;
    targetHeight = 54;
    if (MenuWork.flags & 1) {
        info->help = 0;
    }
    if (info->help == 0) {
        if (PadData.pressed & 32) {
            info->help = 1;
        }
    } else if (PadData.activity != 0) {
        info->help = 0;
    }

    switch (MenuWork.mode) {
    case 32:
    case 34:
    case 64:
        if (MenuWork.busy == 0) {
            if (MenuWork.selectDir >= 0) {
                strcpy(info->buffer, (const char *)MenuTextGet(MenuSortGet(0, MenuWork.selectDir))->auxiliary);
                if (MenuWork.listTab == 0 && info->help == 1) {
                    signed char use = MenuListGet(0)[MenuWork.selectDir].state;

                    if (use == 1) {
                        strcpy(info->buffer, helpLabels.unavailable);
                    } else if (use == 2) {
                        strcpy(info->buffer, helpLabels.restricted);
                    }
                }
            } else {
                info->buffer[0] = D_004DAAD0[0];
            }
        }
        break;
    case 128:
        if (MenuWork.sortSelection == 3) {
            strcpy(info->buffer, labels.cancel);
        } else {
            strcpy(info->buffer, labels.sortPrefix);
            strcat(info->buffer, labels.sortMethods[MenuWork.sortSelection]);
            strcat(info->buffer, D_004DAAD8);
        }
        break;
    case 48:
        targetY = 408;
        targetHeight = 30;
        break;
    default:
        targetY = 464;
        break;
    }

    info->text.message = info->buffer;
    MoveSlide(&info->window.y, &targetY, 3.0f);
    MoveSlide(&info->window.height, &targetHeight, 3.0f);
    WindowDXMain((WindowDX *)&info->window);
}


INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemLineMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemStatusMain);

typedef struct MenuItemAgwsTag {
    u8 unmodeled_00[4];
    short x;
    short y;
    int work;
    signed char color[4];
    struct {
        short x;
        short y;
        int work;
        signed char color[4];
        const char *text;
    } render;
} MenuItemAgwsTag;

typedef struct MenuItemAgwsNumber {
    unsigned short x;
    unsigned short y;
    int work;
    u8 unmodeled_08[6];
    u8 format;
    u8 digitCount;
    u8 unmodeled_10[4];
    int value;
    u8 unmodeled_18[0x90 - 0x18];
} MenuItemAgwsNumber;

typedef struct MenuItemAgwsRow {
    MenuItemPasWindow window;
    MenuItemPasMessage name;
    MenuItemAgwsTag hpLabel;
    MenuItemAgwsNumber hpNumber;
} MenuItemAgwsRow;

typedef struct MenuItemAgwsCursor {
    u8 mode;
    u8 unmodeled_01[3];
    short x;
    short y;
    int work;
} MenuItemAgwsCursor;

typedef struct MenuItemAgwsStatusWork {
    u8 state;
    u8 unmodeled_01[3];
    int color;
    MenuItemAgwsRow rows[6];
    MenuItemAgwsCursor cursor;
} MenuItemAgwsStatusWork;

typedef struct MenuItemHpParameters {
    short maxHp;
    u8 unmodeled_02[0x34 - 0x02];
    short hp;
} MenuItemHpParameters;

extern short MenuItemAgws[];
extern const char D_004DAAE0[];
extern const char *MenuCharNameGet(int character);
extern void eTagFontSet(void *tag, const char *text);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *number, int mode);
extern void eNumberMain(void *number);
extern void eCursolSet(void *cursor, int mode);
extern void eCursolMain(void *cursor);
extern MenuItemHpParameters *func_00A11108(int character, int *attack, int *defense);

void MenuItemStatusAgwsMain(void)
{
    MenuItemAgwsStatusWork *status = MenuItemStatusAgws;
    short targetX[6];
    int attack[3];
    int defense[2];
    register int i;

    switch (status->state) {
    case 0: {
        status->color = 0xF000F0;
        for (i = 0; i < MenuWork.agwsCount; i++) {
            WindowDXSet((WindowDX *)&status->rows[i].window);
            status->rows[i].window.width = 240;
            status->rows[i].window.height = 50;
            status->rows[i].window.color = status->color;
            status->rows[i].window.x = -272;
            status->rows[i].window.state = 1;
            status->rows[i].window.y = i * status->rows[i].window.height + 64;
            WindowDXMain((WindowDX *)&status->rows[i].window);
            status->rows[i].window.state = 3;
            eMessageSet(&status->rows[i].name, MenuCharNameGet(MenuItemAgws[i]));
            status->rows[i].name.mode = 32;
            status->rows[i].name.color = status->rows[i].window.color + 2;
            eTagFontSet(&status->rows[i].hpLabel, D_004DAAE0);
            status->rows[i].hpLabel.work = status->color + 2;
            status->rows[i].hpLabel.color[0] = 112;
            eNumberSet(&status->rows[i].hpNumber, 0);
            status->rows[i].hpNumber.work = status->color + 2;
            status->rows[i].hpNumber.format = 12;
            status->rows[i].hpNumber.digitCount = 4;
        }
        eCursolSet(&status->cursor, 0);
        status->cursor.mode = 32;
        status->state = 2;
        status->cursor.work = status->color + 4;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    for (i = 0; i < MenuWork.agwsCount; i++) {
        targetX[i] = -272;
    }
    if (MenuWork.mode == 32 || MenuWork.mode == 34) {
        if (MenuWork.flags & 1) {
            if (MenuWork.mode == 32) {
                status->cursor.mode = 0;
            } else {
                status->cursor.mode = 32;
            }
        }
        if (MenuWork.agwsMode == 1) {
            for (i = 0; i < MenuWork.agwsCount; i++) {
                targetX[i] = 16;
            }
        }
    }
    i = 0;
    while (i < MenuWork.agwsCount) {
        MenuItemHpParameters *parameters = func_00A11108(MenuItemAgws[i], attack, defense);

        MoveSlide(&status->rows[i].window.x, &targetX[i], 3.0f);
        WindowDXMain((WindowDX *)&status->rows[i].window);
        status->rows[i].name.x = status->rows[i].window.x + 19;
        status->rows[i].name.y = status->rows[i].window.y + 3;
        eMessageMain(&status->rows[i].name);
        status->rows[i].hpLabel.x = status->rows[i].window.x + 123;
        status->rows[i].hpLabel.y = status->rows[i].window.y + 29;
        eTagFontMain(&status->rows[i].hpLabel);
        {
            unsigned int packedHp = parameters->hp + ((unsigned int)parameters->maxHp << 16);

            status->rows[i].hpNumber.value = packedHp;
            status->rows[i].hpNumber.x = status->rows[i].hpLabel.x + 52;
            status->rows[i].hpNumber.y = status->rows[i].hpLabel.y;
        }
        eNumberMain(&status->rows[i].hpNumber);
        i++;
    }
    if (MenuWork.agwsCount != 0) {
        status->cursor.x = status->rows[MenuWork.selectedAgws].window.x + 3;
        status->cursor.y = status->rows[MenuWork.selectedAgws].window.y + 19;
        eCursolMain(&status->cursor);
    }
}

void MenuItemListMake00(void)
{
    MenuItemListRow *rows = MenuListGet(0);
    int *sortRow = MenuSortAddrGet(0);
    MenuItemListWork *work;
    int count;
    int i;

    MenuSortSet(0, 1, 0);
    MenuSortChange(0, 0);
    MenuSortChange(0, MenuWork.sortAbility);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        int use = MenuItemUseCheck((short) sortRow[i]);

        switch (use) {
        case -1:
            rows[i].flag = 1;
            rows[i].state = 2;
            break;
        case 0:
            rows[i].flag = 1;
            rows[i].state = 1;
            break;
        case 1:
        case 2:
        default:
            rows[i].flag = 0;
            rows[i].state = 0;
            break;
        }
    }
    work = MenuItemList;
    work->window.rowCount = 7;
    work->window.title = D_004C4F50;
    MenuItemList->window.columns = 1;
    MenuItemList->window.rows = 11;
    MenuItemList->window.width = 224;
    MenuItemList->window.height = MenuItemList->window.rows * 24 + 6;
    MenuItemList->window.items = MenuListGet(0);
    WindowSPItemChange((WindowSPCursor *)&MenuItemList->window);
    WindowSPSetSelect((WindowSPCursor *)&MenuItemList->window, MenuKeepSelect);
}

void MenuItemListMake00_1(void)
{
    MenuItemListRow *rows = MenuListGet(0);
    int *sortRow = MenuSortAddrGet(0);
    MenuItemListWork *work;
    int count;
    int i;

    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        int use = MenuItemUseCheck((short) sortRow[i]);

        switch (use) {
        case -1:
            rows[i].flag = 1;
            rows[i].state = 2;
            break;
        case 0:
            rows[i].flag = 1;
            rows[i].state = 1;
            break;
        case 1:
        case 2:
            rows[i].flag = 0;
            rows[i].state = 0;
            break;
        }
    }
    work = MenuItemList;
    work->window.rowCount = 7;
    work->window.title = D_004C4F50;
    MenuItemList->window.columns = 1;
    MenuItemList->window.rows = 11;
    MenuItemList->window.width = 224;
    MenuItemList->window.height = MenuItemList->window.rows * 24 + 6;
    MenuItemList->window.items = MenuListGet(0);
    WindowSPItemChange((WindowSPCursor *)&MenuItemList->window);
    WindowSPSetSelect((WindowSPCursor *)&MenuItemList->window, MenuKeepSelect);
}

void MenuItemListMake01(int category)
{
    MenuItemListWork *work;

    switch (category) {
    case 0:
        MenuSortSet(0, 4, -1);
        break;
    case 1:
        MenuSortSet(0, 16, -1);
        break;
    case 2:
        MenuSortSet(0, 8, -1);
        break;
    case 3:
        MenuSortSet(0, 4, -2);
        break;
    case 4:
        MenuSortSet(0, 16, -2);
        break;
    case 5:
        MenuSortSet(0, 8, -2);
        break;
    }
    MenuSortChange(0, 0);
    MenuSortChange(0, MenuWork.sortCategory[category]);
    MenuListMake(0, 0);
    work = MenuItemList;
    work->window.rowCount = 7;
    work->window.title = D_004C4F60;
    MenuItemList->window.columns = 2;
    MenuItemList->window.rows = 11;
    MenuItemList->window.width = 468;
    MenuItemList->window.height = MenuItemList->window.rows * 24 + 6;
    MenuItemList->window.items = MenuListGet(0);
    WindowSPItemChange((WindowSPCursor *)&MenuItemList->window);
    WindowSPSetSelect((WindowSPCursor *)&MenuItemList->window, &MenuKeepSelect[category * 5 + 10]);
}

void MenuItemListMake02(void)
{
    struct MenuItemListWork *list;

    MenuSortSet(0, 2, 0);
    MenuSortChange(0, 0);
    MenuSortChange(0, MenuWork.sortNormal);
    MenuListMake(0, 0);

    list = MenuItemList;
    list->window.rowCount = 7;
    list->window.title = D_004C4F60;
    MenuItemList->window.columns = 2;
    MenuItemList->window.rows = 11;
    MenuItemList->window.width = 0x1D4;
    MenuItemList->window.height = MenuItemList->window.rows * 0x18 + 6;
    MenuItemList->window.items = MenuListGet(0);

    WindowSPItemChange((WindowSPCursor *)&MenuItemList->window);
    WindowSPSetSelect((WindowSPCursor *)&MenuItemList->window, &MenuKeepSelect[5]);
}

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemSelectMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemIconMain);


INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemExMain);

int subRoboPartsCheck(int part)
{
    /* Reading the result back through its address keeps it in memory until
     * the address is purged, so case 3 becomes a conditional move only in
     * if-conversion, which keeps the original xori v0,v0,0. */
    int *resultAddress;
    int result = 0;

    switch (part) {
    case 1:
        result = 1;
        if (xglFlagsGet(3301, 1)) {
            result = 65;
        }
        break;
    case 2:
        result = 2;
        if (xglFlagsGet(3303, 1)) {
            result = 130;
        }
        break;
    case 9:
        result = 4;
        if (xglFlagsGet(3302, 1)) {
            result = 268;
        }
        break;
    case 7:
        result = 8;
        if (xglFlagsGet(3302, 1)) {
            result = 268;
        }
        break;
    case 8:
        result = 16;
        if (xglFlagsGet(3304, 1)) {
            result = 560;
        }
        break;
    case 3:
        result = (xglFlagsGet(3304, 1) == 0) ? (32) : (560);
        break;
    }
    resultAddress = &result;
    return *resultAddress;
}

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemSegmentMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItem);

/*
 * OV01 original TU 9: 0x00a26018..0x00a2c3a0 (91 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

typedef struct MenuEtherStatusEntry {
    unsigned char unmodeled_00[0xE];
    short etherStatus;
} MenuEtherStatusEntry;

extern const signed char D_00A46AF8[];
extern const signed char D_00A46BE0[];
/*
 * grGpInit's argument, restated from src/ov01/gr_gp_init.c (the owner keeps
 * it TU-local; header proposal in the PR): a data pointer and a length. A
 * local packet must be the whole record, because grGpInit writes both words.
 */
typedef struct GrPacket {
    void *data;
    int count;
} GrPacket;

void menuStatSprite(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY);
extern int rnd(int max);
extern float ETRotDist;

/*
 * statTbl: an array of pointers, indexed by charaId, each pointing at a
 * 0-terminated run of 0xC-byte entries (data still asm-owned). menuStatIconChk
 * (0x00A2A4C8..0x00A2A4E0) loads each entry's first word only, as a bitmask
 * tested against the statFlags mask argument.
 */
typedef struct StatIconEntry {
    int flags; /* +0x00 */
    int srcX;  /* +0x04: menuStatIconPut (0x00A2A56C..0x00A2A574) */
    int srcY;  /* +0x08 */
} StatIconEntry;
extern StatIconEntry *statTbl[];

typedef struct MenuRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuRGBA;

/* The sprite colour: menuStatSpriteRGB copies a caller's MenuRGBA into it
 * byte-wise (lwl/lwr, swl/swr), while menuStatSprite/menuStatSprite2 copy the
 * whole word from an aligned local (lw/sw), so the field is word-aligned. */
typedef union MenuColor {
    unsigned int word;
    MenuRGBA rgba;
} MenuColor;

/* grPutSpr's sprite record: two corner vertices, a colour and the texel rectangle. */
typedef struct GrSprite {
    float x0, y0, z0, w0; /* +0x00 */
    float x1, y1, z1, w1; /* +0x10 */
    MenuColor color;      /* +0x20 */
    short u0;             /* +0x24 */
    short u1;             /* +0x26 */
    short v0;             /* +0x28 */
    short v1;             /* +0x2A */
} GrSprite;

#define SPRITE_Z 16777212.0f

extern void grPutSpr(GrPacket *packet, GrSprite *sprite, int flags);
/* The shared sprite colour, .word 0x80808080 (build/ov01/asm/data/ov01/menu.rodata.s:587;
 * rodata still asm-owned). */
extern const MenuRGBA D_00A46C10;

/* The event timer object's work, as far as menuEventTimerDisp reads it:
 * its x/z position (lwc1 at +0x90/+0x98). */
typedef struct MenuEventTimerWork {
    unsigned char unmodeled_00[0x90];
    float x; /* +0x90 */
    unsigned char unmodeled_94[4];
    float z; /* +0x98 */
} MenuEventTimerWork;

void menuATBEnvMake(void *packet);
void menuEventTimerDispSub(ObjectTask *task, GrPacket *packet);

/* Keep source-owned local BSS objects in their original address order;
 * intervening original-only storage remains scaffold-owned between runs. */
static ObjectTask *pMenuBat;
static ObjectTask *pPreObj;
static ObjectTask *pAtbObj;
static ObjectTask *pKeyGuideObj;
static ObjectTask *pBoostObj;
static ObjectTask *pCritObj;
static ObjectTask *pStatObj[4];
static ObjectTask *pStockObj[5];
static int eventTimer;
static int eventTimerX;
static float RingRot;
static int mapHDist;
static int menuResBat;
static unsigned char ethWin[0x28];
static unsigned char itmWin[0x28];

void menuInitBat(void) {
    pMenuBat = 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuOpenBat);

void objRemove(ObjectTask *task);
int sndSysSePlay(int soundId);

void menuCloseBat(void) {
    menuResBat = 0;
    objRemove(pMenuBat);
    pMenuBat = 0;
    sndSysSePlay(8);
}

int menuResultBat(void) {
    return menuResBat;
}

int menuOpenChkBat(void) {
    return pMenuBat != 0;
}

/* obj.c (ov01/tu001), still INCLUDE_ASM: returns the address of the
 * 20-byte command entry the task's work block currently selects. */
void *objCmdPtrGet(ObjectTask *task);
void menuBatObjFree(ObjectTask *task);

void menuBatObj(ObjectTask *task) {
    void *work;

    objCmdPtrGet(task);
    menuBatObjFree(task);
    /* The original keeps jal + epilogue for this last call rather than a
     * tail jump (CP-0132, config/compiler-patterns.json): a post-call read
     * blocks the compiler's sibcall lowering and is then removed as dead
     * code. */
    work = task->work;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuBatObjFree);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuBatObjDraw);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuCmdExecChk);

int menuEtherNumChk(ObjectTask *unit)
{
    int count;
    int slot;
    int offset;
    int countIfPresent;
    MenuEtherStatusEntry *etherEntry;

    count = 0;
    offset = 0x80;
    for (slot = 0xB; slot >= 0; slot--) {
        etherEntry = (MenuEtherStatusEntry *)((char *)calcUPGet(unit) + offset);
        countIfPresent = count + 1;
        offset += 2;
        count = (etherEntry->etherStatus != 0) ? countIfPresent : count;
    }
    return count;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuOpenEth);

void eBattleWinClose(void *window);

void menuCloseEth(void) {
    eBattleWinClose(ethWin);
    sndSysSePlay(8);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuResultEth);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuOpenItm);

void menuCloseItm(void) {
    eBattleWinClose(itmWin);
    sndSysSePlay(8);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuResultItm);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStatDispCreate);

void menuStatDispRemove(void) {
    int i;

    if (pPreObj != 0) {
        objRemove(pPreObj);
        pPreObj = 0;
    }
    if (pAtbObj != 0) {
        objRemove(pAtbObj);
        pAtbObj = 0;
    }
    if (pKeyGuideObj != 0) {
        objRemove(pKeyGuideObj);
        pKeyGuideObj = 0;
    }
    if (pBoostObj != 0) {
        objRemove(pBoostObj);
        pBoostObj = 0;
    }
    if (pCritObj != 0) {
        objRemove(pCritObj);
        pCritObj = 0;
    }
    for (i = 0; i < 5; i++) {
        if (pStockObj[i] != 0) {
            objRemove(pStockObj[i]);
            pStockObj[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (pStatObj[i] != 0) {
            objRemove(pStatObj[i]);
            pStatObj[i] = 0;
        }
    }
    if (pMenuBat != 0) {
        menuCloseBat();
    }
    menuCloseEth();
    menuCloseItm();
}

/* 0x00a27278: `jr $31; nop`, an empty body in the original. */
void menuStatPreObj(void) {
}

void menuStatTexTrans(void);

void menuStatPreObjDraw(void) {
    menuStatTexTrans();
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuATBObj);

/* 0x00a27438: `jr $31; nop`, an empty body in the original. */
void menuKeyGuideObj(void) {
}

/* ManWork and manWorkGet come from their definer, ov01/tu003 battle_init.c,
 * through include/ov01/battle_init.h. menuKeyGuideDisp null-checks
 * unitTask, then reads a flag (bit 3) from unitTask->work, and compares
 * controlPhase against 0xb and 0x1a. */
void menuKeyGuideDisp(GrPacket *packet, ObjectTask *task, int phase);

void menuKeyGuideObjDraw(GrPacket *packet) {
    ObjectTask *task;
    int phase;

    task = manWorkGet()->unitTask;
    phase = manWorkGet()->controlPhase;
    menuKeyGuideDisp(packet, task, phase);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStatObj);

void menuBoostObj(ObjectTask *task) {
    float twoPi;
    float rot;

    objCmdPtrGet(task);
    twoPi = 6.2831855f;
    rot = RingRot - 0.034906585f;
    RingRot = rot;
    if (rot > twoPi) {
        RingRot = rot - twoPi;
    }
}

/*
 * actStockGet (ov01/tu003 battle_init.c, exact_c) returns &actStock; calcBoost's
 * own bounded view of the same record (src/ov01/calc.h CalcActStock) already
 * names the per-mode ObjectTask *slot[2] at +0x10. menuBoostObjDraw reads
 * the same two words only to test them for null, so it indexes actStockGet()
 * the same way rather than repeating a third bounded view of the record.
 */
extern int *actStockGet(void);
void menuBoostObjDrawCha(ObjectTask *task);
void menuBoostObjDrawSel(ObjectTask *task);

void menuBoostObjDraw(ObjectTask *task) {
    if (actStockGet()[4] == 0 && actStockGet()[5] == 0) {
        menuBoostObjDrawSel(task);
        return;
    }
    menuBoostObjDrawCha(task);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuBoostObjDrawCha);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuBoostObjDrawSel);

/*
 * The boost-ring display object (data still asm-owned; original ELF symbol
 * name).
 * Its work record's first word is a flag set; bit 2 (0x4) hides the object.
 */
extern ObjectTask *pBoostObj;

void menuBoostDisp(int enabled)
{
    if (enabled != 0) {
        *(int *)pBoostObj->work &= ~4; /* clear hidden bit 2 */
        return;
    }
    *(int *)pBoostObj->work |= 4; /* set hidden bit 2 */
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuCritDispOn);

/*
 * The critical-indicator display object (data still asm-owned; original
 * ELF symbol name). Its work record's first word is a flag set; bit 2 (0x4) hides it.
 */
extern ObjectTask *pCritObj;

void menuCritDispOff(void)
{
    *(int *)pCritObj->work |= 4; /* set hidden bit 2 */
}

int objCmdNext(ObjectTask *task);
int objHatoVec(ObjectTask *task, int direction);
int objStdMove(ObjectTask *task);

/*
 * objCmdPtrGet's return, as far as menuCritObj reads it: `lw $3,0x0($2)`
 * loads the word at +0x00. obj.c's own view of the same 20-byte record
 * (src/ov01/obj.h ObjectCommandEntry.selector) already names that word;
 * this TU cannot redefine that tag, so it names its own bounded view of
 * the same record instead.
 */
typedef struct MenuCmdEntryView MenuCmdEntryView;
struct MenuCmdEntryView {
    int selector; /* +0x00 */
};

/*
 * The critical-indicator object's work, one record for menuCritObj and
 * menuCritObjDraw: menuStatDispCreate registers both on the same task
 * (the addresses of menuCritObj and menuCritObjDraw are formed at
 * 0x00A26E94..0x00A26EB8 and passed to objEntry2; the result is stored to
 * pCritObj) and menuCritDispOn writes pCritObj's work.
 * +0x80 is an int count: menuCritDispOn compares it with its int argument
 * (0x00A28128 lw $2,0x80($3); beql $2,$16) and stores it (0x00A28174
 * sw $16,0x80($2)); menuCritObjDraw tests it and draws digit count - 1
 * (0x00A282E8 lw $2,0x80($3); 0x00A282FC addiu $3,$2,-1); menuCritObj only
 * tests it against zero (0x00A28200 lw $6,0x80($2); movn $5,$3,$6).
 * This extends, and does not preserve the size of, the local view master
 * carried as MenuCritActorView (0x84 bytes: 0x80 unmodeled bytes and a task
 * pointer at +0x80 that menuCritObj only tested for null): the record is now
 * 0xA0 bytes, flags at +0x00 and position at +0x90 are new members, and the
 * +0x80 word is the int count rather than a pointer.
 */
typedef struct MenuCritWork {
    u32 flags;                        /* +0x00: bit 2 hides the indicator */
    unsigned char unmodeled_04[0x7C];
    int count;                        /* +0x80: 0 draws the plain banner */
    unsigned char unmodeled_84[0xC];
    Vector4 position;                 /* +0x90 */
} MenuCritWork;

/* Bit of MenuCritWork.flags that hides the indicator: menuCritDispOff sets
 * it (|= 4), menuCritObjDraw skips drawing while it is set (andi $2,$2,0x4).
 * An enum constant, not a macro (a macro can never be published by
 * header_harvest.py); it stays TU-local until a declaration needs it. */
enum MenuCritWorkFlag {
    MENU_CRIT_HIDDEN = 0x4
};

/* The work slot holds the address of the indicator's MenuCritWork.
 * menuCritObj reads the slot once (lw $2,0x10($16) at 0x00A281F4) and so
 * keeps the record in a local; menuCritObjDraw loads it afresh before each
 * of its reads (lw 0x10($18) at 0x00A28254, 0x00A2828C and 0x00A282D8), so
 * every access there casts the slot itself instead of caching it. */

int menuCritObj(ObjectTask *task)
{
    MenuCmdEntryView *entry;
    MenuCritWork *work;
    int result;

    result = 1;
    entry = objCmdPtrGet(task);
    if (entry->selector == 1) {
        work = task->work;
        objHatoVec(task, (work->count == 0) ? 4 : 2);
        result = objStdMove(task);
        if (result != 0) {
            result = objCmdNext(task);
        }
    }
    return result;
}

void menuStatIconEnvMake(void *packet);
/*
 * The battle map's height value (bss still scaffold-owned, like ETRotDist;
 * original ELF symbol name). menuStatDispCreate starts it and mapHDist at
 * 0x40 (`sw $5,%lo(mapH)` / `sw $5,%lo(mapHDist)` at 0x00a26df0..0x00a26df4),
 * menuMapHSet below moves mapHDist to 0x14 or 0x40, and menuATBObj (still
 * asm) steps mapH by 8 per update toward mapHDist (0x00a27320..0x00a2733c).
 * menuMapDisp offsets the map by 0x40 - mapH (0x00a2848c); menuATBObjDraw
 * draws the map item and target line only while it is 0x40 (0x00a283d4),
 * menuCritObjDraw the critical indicator only while it is 0x14
 * (`bnel` against 0x14). The two rest values are MENU_MAP_H_SET/RESET.
 */
extern int mapH;

/* The two values mapH rests at: menuMapHSet(nonzero) sends it to 0x14
 * (`addiu $2,$0,0x14` at 0x00a28428), menuMapHSet(0) and menuStatDispCreate
 * to 0x40 (0x00a28438, 0x00a26ddc). */
enum MenuMapH {
    MENU_MAP_H_SET = 0x14,
    MENU_MAP_H_RESET = 0x40
};

/* Defined by ov01/tu008 (src/ov01/gr_gp_init.c). Declared here because
 * menuCritObjDraw calls them ahead of the declarations further down; without
 * them gcc 2.96 reports "type mismatch with previous implicit declaration". */
extern void grGpInit(GrPacket *packet);
extern void grOpenSpr(GrPacket *packet);
extern void grCloseSpr(void);
extern void grPacketSend(GrPacket *packet);

/*
 * Draws the critical banner and, for a nonzero count, its digit cell
 * (0x18 wide, starting at sheet x 0xB1). The digit's sheet offset is its
 * own statement ahead of the first call: the original keeps (count - 1) * 3
 * in $16 across that call and finishes *8 and +0xB1 after it; one
 * (count - 1) * 0x18 + 0xB1 expression folds to count * 0x18 + 0x99.
 */
void menuCritObjDraw(ObjectTask *task)
{
    GrPacket packet;
    int x;
    int z;
    int count;
    int digit;
    int digitX;

    if ((((MenuCritWork *)task->work)->flags & MENU_CRIT_HIDDEN) == 0 && mapH == MENU_MAP_H_SET) {
        grGpInit(&packet);
        menuStatIconEnvMake(&packet);
        x = (int)((MenuCritWork *)task->work)->position.x;
        z = (int)((MenuCritWork *)task->work)->position.z;
        grOpenSpr(&packet);
        menuStatSprite(&packet, x, z, 0x84, 0x18, 0x7C, 0);
        count = ((MenuCritWork *)task->work)->count;
        if (count == 0) {
            menuStatSprite(&packet, x + 0x12, z + 4, 0x61, 0x10, 0x9B, 0x19);
        } else {
            digit = count - 1;
            digitX = digit * 0x18;
            menuStatSprite(&packet, x + 0x12, z + 4, 0x41, 0x10, 0xB1, 0x2A);
            menuStatSprite(&packet, x + 0x53, z + 4, 0x18, 0x10,
                           digitX + 0xB1, 0x3B);
        }
        grCloseSpr();
        grPacketSend(&packet);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuATBObjDraw);

/*
 * The battle map's height-scan distance (data still asm-owned; original
 * ELF symbol name).
 */
void menuMapHSet(int enable)
{
    if (enable != 0) {
        mapHDist = MENU_MAP_H_SET;
        return;
    }
    mapHDist = MENU_MAP_H_RESET;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuMapDisp);

/* Defined by ov01/tu008 (src/ov01/gr_gp_init.c). */
extern void grCloseSpr(void);
extern void grGsRegSet(GrPacket *packet, unsigned long long reg, unsigned long long value);

void menuATBEnvPalSet(GrPacket *packet, int palette);
void menuMapItemDispSub(ObjectTask *task, GrPacket *packet, int index, int x, int z);

/*
 * task->work's own float pair at these offsets is the map item's world
 * position; only x and z feed this draw call.
 */
#define MAP_ITEM_POS_X_OFF 0x90
#define MAP_ITEM_POS_Z_OFF 0x98

void menuMapItemDisp(ObjectTask *task, GrPacket *packet)
{
    void *work;
    int x;
    int z;
    int zFar;

    work = task->work;
    z = (int)(*(float *)((char *)work + MAP_ITEM_POS_Z_OFF));
    x = (int)(*(float *)((char *)work + MAP_ITEM_POS_X_OFF));
    grGsRegSet(packet, 0x42, 0);
    zFar = z + 0x4C;
    grGsRegSet(packet, 0x14, 0);
    menuATBEnvPalSet(packet, 1);
    grOpenSpr(packet);
    menuMapItemDispSub(task, packet, 0, x, zFar);
    menuMapItemDispSub(task, packet, 1, x, z);
    grCloseSpr();
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuTgtLineDisp);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockStart);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockNext);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockBoost);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockDead);

typedef struct MenuStockWork {
    u32 flags;                         /* +0x00: bit 0x4 hides the face
                                        * (menuStockObjDraw, andi 0x4) */
    unsigned char unmodeled_04[0x6C];
    int frameCount;                    /* +0x70: menuStockObj adds 1 every
                                        * update; command 3 moves the face
                                        * only once it is nonzero */
    unsigned char unmodeled_74[4];
    /* A code-shape device, not data: width is only ever read and written as
     * `value`. `alias` is never accessed; it makes the int store to width
     * conflict with the void * load of ObjectTask.work in gcc 2.96's alias
     * analysis, so menuStockObj reloads the slot after storing width
     * (lw $4,0x10($16) after sw $3,0x78($5)) as the original does. */
    union {
        int value;
        void *alias;
    } width;                           /* +0x78: menuStockStart seeds 4
                                        * (sw $22,0x78); menuStockObj adds 4
                                        * while it is below 0x14 and writes
                                        * 0x14 otherwise; menuStockObjDraw
                                        * passes it as the sprite width */
    unsigned char unmodeled_7c[4];
    int index;                         /* +0x80: the face's pStockObj slot
                                        * (sw $18,0x80) */
    unsigned char unmodeled_84[0xC];
    Vector4 position;                  /* +0x90: addiu $16,$16,0x90, then
                                        * swc1 0x0/0x8 and sw $0,0x4 */
} MenuStockWork;

void menuStockObj(ObjectTask *task)
{
    MenuCmdEntryView *entry;
    MenuStockWork *stock;

    entry = objCmdPtrGet(task);
    /* Compared as an unsigned equality chain (beq 1, beql 0, beq 2, beq 3);
     * a signed selector splits it with slti $2,$3,2 / bnel instead. */
    switch ((unsigned int)entry->selector) {
    case 0:
        break;
    case 1:
        objHatoVec(task, 2);
        if (objStdMove(task) != 0) {
            objCmdNext(task);
        }
        break;
    case 2:
        break;
    case 3:
        if (((MenuStockWork *)task->work)->frameCount != 0) {
            objHatoVec(task, 2);
            if (objStdMove(task) != 0) {
                objCmdNext(task);
            }
        }
        break;
    }
    /* Not a clamp: a width below 0x14 grows by 4 (so 0x12 becomes 0x16),
     * any other value is replaced by 0x14 (slti $2,$2,0x14; movn $3,$4,$2). */
    stock = ((MenuStockWork *)task->work);
    stock->width.value = (stock->width.value < 0x14) ? stock->width.value + 4 : 0x14;
    ((MenuStockWork *)task->work)->frameCount++;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockObjDraw);

/*
 * The 3x11 face-icon sheet's per-character source cell: chrId<32 indexes a
 * grid of 21x25 cells (column = chrId%3, row = chrId/3); chrId>=32 (no face
 * tile) leaves the row at 0 and picks one of three fixed columns from
 * CalcUnitParam.flags (lhu $3,0x1C($2) at 0x00A29144): bit 0x4000 set
 * (andi $3,$3,0x4000 at 0x00A2914C) selects sheet x 0xC0 (0x00A2915C),
 * otherwise bit 0x2000 set (andi $2,$3,0x2000 at 0x00A29148) selects 0xD5
 * (0x00A29164), otherwise 0xEA (0x00A29168). Neither bit has an established
 * name (include/ov01/calc.h and src/ov01/calc.h name other bits of this
 * halfword only), so they stay numeric; which characters carry them is not
 * evidenced here.
 */
void faceUVGet(ObjectTask *task, int *srcX, int *srcY)
{
    int chrId;
    CalcUnitParam *up;
    u16 flags;

    chrId = menuStatChrIdGet(task);
    if (chrId < 0x20) {
        *srcX = (chrId % 3) * 21 + 0x40;
        *srcY = (chrId / 3) * 25;
        return;
    }
    up = calcUPGet(task);
    *srcY = 0;
    flags = up->flags;
    if (flags & 0x4000) {
        *srcX = 0xC0;
    } else if (flags & 0x2000) {
        *srcX = 0xD5;
    } else {
        *srcX = 0xEA;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuMapItemDispSub);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuSelLine);

void menuEventTimerDisp(ObjectTask *task, GrPacket *packet)
{
    MenuEventTimerWork *work;
    int x;
    int y;

    work = task->work;
    x = work->x;
    y = work->z;
    menuATBEnvMake(packet);
    menuEventTimerDispSub(task, packet);
    x -= 0x18;
    y += 0x38;
    grOpenSpr(packet);
    menuStatSprite(packet, x, y, 0x18, 0x4E, 0x80, 0x32);
    grCloseSpr();
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuEventTimerDispSub);

void menuTimeSet(int time) {
    int timerRoll;

    eventTimer = time;
    timerRoll = rnd(100);
    if (timerRoll >= 0x60) {
        eventTimerX = 10;
    } else if (timerRoll >= 0x3D) {
        eventTimerX = 4;
    } else {
        eventTimerX = 2;
    }

    ETRotDist = (float)eventTimer * 1.5707964f;
    if (ETRotDist > 6.2831855f) {
        ETRotDist -= 6.2831855f;
    }
}

int menuTimeNext(void) {
    eventTimer++;
    if (eventTimer >= 4) {
        eventTimer = 0;
    }
    menuTimeSet(eventTimer);
    return eventTimer;
}

int menuTimeGet(void) {
    return eventTimer;
}

int menuTimeXGet(void) {
    return eventTimerX;
}

extern BattleUnit *actUnitGet(void);
void grGpInit(GrPacket *packet);
void grPacketSend(GrPacket *packet);
void menuPlStatPut(ObjectTask *task, GrPacket *packet, BattleUnit *unit);
void menuStatNamePut(ObjectTask *task);

/*
 * The status-icon task's own work record, as far as menuStatObjDraw and
 * menuStatNamePut read it. menuStatNamePut (0x00a2a5c0..0x00a2a5c8) loads
 * `lw $17,0x78($2)` / `lw $5,0x7c($2)` from task->work and passes them to
 * menuStatIconChk/dataStatNameGet as the character id and the icon slot to
 * check; menuStatObjDraw (0x00a29bf4..0x00a29c00) loads `lw $17,0x80($2)`
 * from the same block and compares it against actUnitGet()'s BattleUnit *,
 * showing the name popup only for the currently active unit.
 */
typedef struct MenuStatObjWork MenuStatObjWork;
struct MenuStatObjWork {
    unsigned char unmodeled_00[0x78];
    int charaId;       /* +0x78 */
    int iconSlot;       /* +0x7C */
    BattleUnit *unit;   /* +0x80 */
};

void menuStatObjDraw(ObjectTask *task) {
    GrPacket packet;
    MenuStatObjWork *work;
    BattleUnit *unit;

    grGpInit(&packet);
    work = task->work;
    unit = work->unit;
    menuPlStatPut(task, &packet, unit);
    grPacketSend(&packet);
    if (unit == actUnitGet()) {
        menuStatNamePut(task);
    }
}


/* calcUPGet's return type comes from its definer, ov01/tu004 calc.c
 * (published include/ov01/calc.h); calcUPGet itself is still asm there, so
 * this TU declares it the same way ov01/tu002 unit_cmd.c and ov01/tu013
 * snd.c do. */
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

/*
 * unitLoad and unitActupdate test this bit of the battle actor's flags word
 * to tell an enemy-controlled unit from a player-controlled one
 * (src/ov01/unit_cmd.h documents it as ACTOR_FLAG_ENEMY); ov01/tu013 snd.c's
 * sndConvSe already reuses the same evidenced mask off calcUPGet(unit)->flags.
 */
#define ACTOR_FLAG_ENEMY 0x40u

/* An enemy unit, or one whose charaId falls in the boss range ov01/tu013
 * snd.c's sndConvSe documents (0xBB..0xC2), reports the linked unit's
 * charaId instead of its own. */
int menuStatChrIdGet(ObjectTask *unit)
{
    int charaId;
    CalcUnitParam *up;

    up = calcUPGet(unit);
    charaId = up->charaId;
    if (!(calcUPGet(unit)->flags & ACTOR_FLAG_ENEMY)) {
        if (calcUPGet(unit)->charaId >= 0xBB) {
            if (calcUPGet(unit)->charaId < 0xC3) {
                charaId = up->linkedUnit->charaId;
            }
        }
    } else {
        charaId = up->linkedUnit->charaId;
    }
    return charaId - 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuPlStatPut);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuNumPut);

void menuApBarPut(GrPacket *packet, int x, int y, int amount)
{
    int i;
    int xPosition;

    xPosition = x;
    for (i = 0; i < 6; i++) {
        int sourceX;

        sourceX = 24;
        if (amount > 0) {
            sourceX = 0;
        }
        menuStatSprite(packet, xPosition, y, 24, 16, sourceX, 240);
        amount--;
        xPosition += 16;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuBpBarPut);

/*
 * Index of the first icon entry of the character's table whose flags share a
 * bit with statFlags (a status flag mask), or -1 when none does.
 */
int menuStatIconChk(int charaId, int statFlags)
{
    StatIconEntry *icons;
    int index;

    icons = statTbl[charaId];
    if (icons == 0 || statFlags == 0) {
        return -1;
    }
    for (index = 0; icons[index].flags != 0; index++) {
        if (icons[index].flags & statFlags) {
            return index;
        }
    }
    return -1;
}

int menuStatIconChk(int charaId, int statFlags);

void menuStatIconPut(GrPacket *packet, int x, int y, int charaId, int statFlags)
{
    StatIconEntry *icons;
    int index;

    icons = statTbl[charaId];
    index = menuStatIconChk(charaId, statFlags);
    if (index != -1) {
        menuStatSprite(packet, x + 0x90, y + 0x32, 0x16, 0x18, icons[index].srcX,
                       icons[index].srcY);
    }
}

/*
 * eBattleWinOpen2's own argument words (src/main/e_battle_win_open.c, still
 * asm there, no published parameter type). The header shape (textId/
 * speaker/flags/priority/target) and the flags constant 0x00FFFFFD match
 * src/ov01/cmd.c's BattleMsgOpenParams, the same window family's argument
 * record for the sibling eBattleWinOpen4; only flags and target are
 * confirmed for this callee, the textId/speaker/priority names are carried
 * over from that sibling record's field order. This call adds one more
 * word, an icon graphic table pointer, that eBattleWinOpen4 does not take.
 */
struct StatNameOpenParams {
    int textId;                /* +0x00: 8 at this call site */
    int speaker;                /* +0x04: 0x68 at this call site */
    int flags;                   /* +0x08: 0x00FFFFFD */
    int priority;                 /* +0x0C: 0x800000B0 at this call site */
    int target;                    /* +0x10: dataStatNameGet's resolved name id */
    const char *iconGraphic;       /* +0x14: D_00A468E8 */
};
int *dataStatNameGet(int charaId, int iconSlot);
void eBattleWinOpen2(struct StatNameOpenParams *params);
int menuStatIconChk(int charaId, int iconSlot);
const char D_00A468E8[8] = "Status";
extern void eBattleWinMain2(void);

void menuStatNamePut(ObjectTask *task)
{
    struct StatNameOpenParams params;
    MenuStatObjWork *work;
    int charaId;
    int icon;

    work = task->work;
    charaId = work->charaId;
    icon = menuStatIconChk(charaId, work->iconSlot);
    if (icon != -1 && menuOpenChkBat() != 0) {
        params.textId = 8;
        params.speaker = 0x68;
        params.flags = 0xFFFFFD;
        params.priority = 0x800000B0;
        params.target = *dataStatNameGet(charaId, icon);
        params.iconGraphic = D_00A468E8;
        eBattleWinOpen2(&params);
        eBattleWinMain2();
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuKeyGuideDisp);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", keyGideDisp);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuGuideFrameDisp);

extern const char **dataEnemyNameGet(int index);
extern const char **dataPlayerNameGet(int id);
extern unsigned int strlen(const char *s);
void menuGuideFrameDisp(void *packet, int x, int y, int width, int height);
void menuFontEnvMake(void *packet);
void menuStatEnvMake(void *packet);
void menuFontPut(GrPacket *packet, int x, int y, const char *text);

void monsNameDisp(void *packet, ObjectTask *unitTask, int x, int y, int isEnemy)
{
    const char *name;
    int len;

    menuStatEnvMake(packet);
    menuGuideFrameDisp(packet, x, y, 0x50, 2);
    x = x + 2;
    y = y + 3;
    menuFontEnvMake(packet);
    if (isEnemy == 1) {
        name = *dataEnemyNameGet(calcUPGet(unitTask)->charaId);
    } else {
        name = *dataPlayerNameGet(calcUPGet(unitTask)->charaId);
    }
    len = strlen(name);
    menuFontPut(packet, x + (0x10 - len) * 5, y, name);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuKeyGuideHelp);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuKeyGuideVal);

/* obj.c (ov01/tu001), exact_c: allocates a task from the free list and
 * stores the caller's callback into its ->exec slot. */
extern ObjectTask *objEntry2(void *argument, void (*callback)(ObjectTask *task));
/* obj.c (ov01/tu001), still INCLUDE_ASM there. */
extern void objStdInit(ObjectTask *task);

void objDmgNum(ObjectTask *task);
void objDmgNumDraw(ObjectTask *task);

/* The work block objWorkGet allocates for an objDmgNum/objDmgNumDraw task.
 * Only the fields objDmgNumCreate itself writes are evidenced; the span
 * before them is read by neither this function nor any other claimed one. */
typedef struct {
    unsigned char unmodeled_00[0x70];
    int countdown;         /* +0x70: objDmgNumCreate always sets 30 */
    int value;              /* +0x74 */
    int flags;               /* +0x78 */
    unsigned char unmodeled_7C[4];
    void *target;           /* +0x80 */
} DmgNumWork;

ObjectTask *objDmgNumCreate(void *target, int value, int flags)
{
    ObjectTask *task;
    DmgNumWork *work;

    task = objEntry2(objDmgNum, objDmgNumDraw);
    if (task != 0) {
        work = task->work;
        objStdInit(task);
        work->target = target;
        work->value = value;
        work->flags = flags;
        work->countdown = 30;
        return task;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", objDmgNum);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", objDmgNumDraw);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", dmgMsgPut);

INCLUDE_ASM("asm/nonmatchings/ov01/menu", dmgNumPut);

int menuFontIdxGet(const signed char *text)
{
    static const signed char *font = D_00A46AF8;
    int index;

    for (index = 0; index < 230; index++) {
        if (text[0] == font[index]) {
            return index;
        }
    }
    return -1;
}

int menuDFontIdxGet(const signed char *text)
{
    static const signed char *font = D_00A46BE0;
    int index;

    for (index = 0; index < 230; index++) {
        if (text[0] == font[index * 2] && text[1] == font[index * 2 + 1]) {
            return index + 175;
        }
    }
    return -1;
}


/* The font sheet is a 25-column grid of 0xA x 0x12 (10x18) glyph cells. */
#define FONT_GLYPH_COLUMNS 25
#define FONT_GLYPH_WIDTH 0xA
#define FONT_GLYPH_HEIGHT 0x12

void menuFontPutSub(GrPacket *packet, int x, int y, int charCode) {
    int row;
    int col;

    row = charCode / FONT_GLYPH_COLUMNS;
    col = charCode % FONT_GLYPH_COLUMNS;
    menuStatSprite(packet, x, y, FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT,
                   col * FONT_GLYPH_WIDTH, row * FONT_GLYPH_HEIGHT);
}

extern int sscanf(const char *string, const char *format, ...);
extern const char D_00A46C08[]; /* "%X": menuFontPut parses a '#'-escaped hex glyph code */

/*
 * Draws text one glyph per FONT_GLYPH_WIDTH: "#XX" is a hex glyph code, a byte
 * with bit 7 set starts a two-byte glyph, anything else is a single-byte one.
 * The loop indexes text[consumed] and takes the glyph cursor at the top of
 * the body: the original tests text[0] through the text register and reloads
 * the byte through the cursor (a cursor-driven loop shares that load).
 */
void menuFontPut(GrPacket *packet, int x, int y, const char *text)
{
    char hex[3];
    u32 glyph; /* sscanf "%X" target; menuFontIdxGet/menuDFontIdxGet store
                * their index, -1 for none, into the same slot (sw $2,0x10) */
    int consumed;
    const char *cursor;

    grOpenSpr(packet);
    for (consumed = 0; text[consumed] != '\0'; consumed++, x += FONT_GLYPH_WIDTH) {
        cursor = &text[consumed];
        if (*cursor == '#') {
            hex[0] = cursor[1];
            consumed += 2;
            hex[1] = cursor[2];
            hex[2] = '\0';
            sscanf(hex, D_00A46C08, &glyph);
        } else if (*cursor & 0x80) {
            consumed++;
            glyph = menuDFontIdxGet((const signed char *)cursor);
        } else {
            glyph = menuFontIdxGet((const signed char *)cursor);
        }
        if ((int)glyph != -1) {
            menuFontPutSub(packet, x, y, (int)glyph);
        }
    }
    grCloseSpr();
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuActCurPut);

void menuStatSprite(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY) {
    GrSprite sprite;
    MenuColor color;

    color.rgba = D_00A46C10;
    sprite.x0 = x;
    sprite.y0 = y;
    sprite.z0 = SPRITE_Z;
    sprite.x1 = x + width;
    sprite.y1 = y + height;
    sprite.z1 = SPRITE_Z;
    sprite.u0 = srcX;
    sprite.u1 = srcX + width + 1;
    sprite.v0 = srcY;
    sprite.v1 = srcY + height + 1;
    sprite.color = color;
    grPutSpr(packet, &sprite, 0);
}

void menuStatSprite2(GrPacket *packet, int x, int y, int width, int height, int srcLeft, int srcTop, int srcRight, int srcBottom) {
    GrSprite sprite;
    MenuColor color;

    color.rgba = D_00A46C10;
    sprite.u0 = srcLeft;
    sprite.x0 = x;
    sprite.y0 = y;
    sprite.v0 = srcTop;
    sprite.z0 = SPRITE_Z;
    sprite.x1 = x + width;
    sprite.y1 = y + height;
    sprite.z1 = SPRITE_Z;
    sprite.u1 = srcRight;
    sprite.v1 = srcBottom;
    sprite.color = color;
    grPutSpr(packet, &sprite, 0);
}

void menuStatSpriteRGB(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY, const MenuRGBA *rgba) {
    GrSprite sprite;

    sprite.x0 = x;
    sprite.y0 = y;
    sprite.z0 = SPRITE_Z;
    sprite.x1 = x + width;
    sprite.y1 = y + height;
    sprite.z1 = SPRITE_Z;
    sprite.u0 = srcX;
    sprite.u1 = srcX + width + 1;
    sprite.v0 = srcY;
    sprite.v1 = srcY + height + 1;
    sprite.color.rgba = *rgba;
    grPutSpr(packet, &sprite, 0);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStatSprite2RGB);

/* dataXtxFileGet's record: only the texture id at +0x4 is read here. */
extern int *dataXtxFileGet(int index);
extern void xglPacketTextureTrans(int textureId);

void menuTexTrans(void) {
    GrPacket packet;

    xglPacketTextureTrans(dataXtxFileGet(1)[1]);
    grGpInit(&packet);
    grGsRegSet(&packet, 0x3F, 1);
    grPacketSend(&packet);
}

void menuStatTexTrans(void) {
    GrPacket packet;

    xglPacketTextureTrans(dataXtxFileGet(0)[1]);
    grGpInit(&packet);
    grGsRegSet(&packet, 0x3F, 1);
    grPacketSend(&packet);
}

/* Defined by ov01/tu008 (src/ov01/gr_gp_init.c). The six *EnvMake callers keep
 * their packet parameter as void *, which converts here. */
extern void GS_REG_SET(GrPacket *packet, long long register_id, long long control);
extern unsigned short dataXtxAdrGet(int index);

/*
 * The six *EnvMake functions below write one GIF packet each. No neighbouring
 * code names the GS registers or the packed words (menuStatTexTrans above and
 * encEnvMake pass raw numbers to grGsRegSet/GS_REG_SET), so they stay numeric:
 * - 0xE is the GIF A+D descriptor; its word is the GIFtag (NLOOP 5 for the five
 *   register writes that follow, EOP set, NREG 1), the same word
 *   gr_gp_init.c's grGifAppendRegister(packet, 14, ...) opens its packets with;
 * - 0x06 TEX0_1: tbp0 is the texture base, tbp1 << 37 the CLUT base (CBP); the
 *   constant carries the buffer width, pixel formats, size and function bits;
 * - 0x42 ALPHA_1 (blend equation; menuFontEnvMake's 0x44 differs from the 0 of
 *   the others), 0x47 TEST_1 (0x3000D: alpha test on, depth test on),
 *   0x08 CLAMP_1 and 0x14 TEX1_1 both 0.
 * The dataXtxAdrGet indices select which texture/CLUT pair each environment
 * binds.
 */

void menuEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(0xD);
    unsigned short tbp1 = dataXtxAdrGet(0xE);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

void menuStatEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(0);
    unsigned short tbp1 = dataXtxAdrGet(3);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

void menuFaceEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(0x8);
    unsigned short tbp1 = dataXtxAdrGet(0x9);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

void menuATBEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(4);
    unsigned short tbp1 = dataXtxAdrGet(7);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

void menuFontEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(0xB);
    unsigned short tbp1 = dataXtxAdrGet(0xC);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0x44);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

void menuStatIconEnvMake(void *packet)
{
    unsigned short tbp0 = dataXtxAdrGet(0);
    unsigned short tbp1 = dataXtxAdrGet(1);

    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x6,
               tbp0 | ((long long)tbp1 << 37) | 0x2000000629340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x3000D);
    GS_REG_SET(packet, 0x8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuATBEnvPalSet);



const signed char D_00A46AF8[232] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz                                                                                    .    '                      /                   -                                 ";



const signed char D_00A46BE0[40] = "\241\277\241\241\241\246\241\241\241\260\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241\241";

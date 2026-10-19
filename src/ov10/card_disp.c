/*
 * OV10 original TU 6: 0x00a1e5d8..0x00a21348 (27 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov10/cgp.h"

/*
 * 16-byte float vector the original copies with doubleword moves (ld/sd pairs): the u64 member gives
 * the aggregate the 8-byte alignment that avoids ldl/ldr pairs (same shape as CardTutorialTranslation
 * in card_tutorial.c).
 */
typedef union CardHelpFloat4 {
    float v[4];
    u64 words[2];
} CardHelpFloat4;

void xglMatrixScale(Matrix4 destination, const Matrix4 source,
                            const float scale[4]);
void xglMatrixTrans(Matrix4 destination, const Matrix4 source,
                            const float translation[4]);
void CardDispLight(Matrix4 matrix, s16 card, s16 enabled, u8 color);
extern void nmlModelEntry(u32 model);
extern void CardDisp(Matrix4 matrix, s16 kind, s16 flagA, s16 flagB);
extern void nmlModelEntryCard(void *model);
extern void nmlModelSetPlace(const Vector4 *matrix);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelSetTexture(const char *texture);

extern const CardHelpFloat4 D_00A4E0A0;
extern const CardHelpFloat4 D_00A4E0D0;
extern const CardHelpFloat4 D_00A4E0E0;
extern const CardHelpFloat4 D_00A4E170;
extern const CardHelpFloat4 D_00A4E180;
extern const CardHelpFloat4 D_00A4E190;
extern const CardHelpFloat4 D_00A4E1A0;
extern const CardHelpFloat4 D_00A4E1C0;
extern const CardHelpFloat4 D_00A4E1D0;
extern const char *cardxtx_tbl[];
extern u32 cardlex_tbl[];

extern const CardHelpFloat4 D_00A4E0B0;
extern const CardHelpFloat4 D_00A4E0F0;
extern const CardHelpFloat4 D_00A4E100;
extern const CardHelpFloat4 D_00A4E110;
extern const CardHelpFloat4 D_00A4E0C0;

/* Original 16-byte vectors (translation, scale and place constants) of the help screen. */
const CardHelpFloat4 D_00A4E0A0 = {{ 1.48f, 0.16f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E0B0 = {{ 1.37f, 0.2f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E0C0 = {{ 0.9f, 0.9f, 1.0f, 1.0f }};
const CardHelpFloat4 D_00A4E0D0 = {{ 1.5f, 0.18f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E0E0 = {{ 0.9f, 0.9f, 1.0f, 1.0f }};
const CardHelpFloat4 D_00A4E0F0 = {{ 1.397f, -0.317f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E100 = {{ 1.397f, 1.457f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E110 = {{ 0.9f, 0.9f, 1.0f, 1.0f }};
const CardHelpFloat4 D_00A4E140 = {{ 1.397f, 0.863f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E150 = {{ 0.6f, 0.6f, 1.0f, 1.0f }};
const CardHelpFloat4 D_00A4E170 = {{ 0.17f, 0.17f, 1.0f, 1.0f }};
const CardHelpFloat4 D_00A4E180 = {{ -2.64f, 1.16f, 0.02f, 1.0f }};
const CardHelpFloat4 D_00A4E190 = {{ -2.2f, 0.58f, 0.02f, 1.0f }};
const CardHelpFloat4 D_00A4E1A0 = {{ -1.1f, 1.53f, 0.02f, 1.0f }};
const CardHelpFloat4 D_00A4E1B0 = {{ 1.397f, 0.179f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E1C0 = {{ 1.36f, 0.15f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E1D0 = {{ 1.36f, 0.15f, 0.0f, 1.0f }};
const char *cardxtx_tbl[64] = {0};
u32 cardlex_tbl[64] = {0};
extern char *strcpy(char *destination, const char *source);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

/* 16-byte float vector with doubleword alignment (see CardDispHelpSlide2/3). */
typedef unsigned int Quadword __attribute__((mode(TI)));
typedef union CardHelpVector {
    float v[4];
    Quadword q;
} CardHelpVector;
extern void nmlModelSetLight(Matrix4 color, Matrix4 direction);
extern float xglSin(float angle);
/*
 * Help resources: index of each file CardHelpFileLoad reads, in the order of its name list,
 * which is also the index into cardlex_tbl[] / cardxtx_tbl[]. The names are the disc file
 * names without the "h_" prefix.
 */
enum CardHelpResource {
    CARD_HELP_RES_TITTLE,
    CARD_HELP_RES_PH1,
    CARD_HELP_RES_PH2,
    CARD_HELP_RES_TEXT,
    CARD_HELP_RES_EVENT,
    CARD_HELP_RES_BATTLE1,
    CARD_HELP_RES_BATTLE2,
    CARD_HELP_RES_DOWN,
    CARD_HELP_RES_EMARK,
    CARD_HELP_RES_EBLAKE,
    CARD_HELP_RES_OPEN,
    CARD_HELP_RES_VIEWER,
    CARD_HELP_RES_DECKSAVE,
    CARD_HELP_RES_HELP5,
    CARD_HELP_RES_HELP6,
    CARD_HELP_RES_H1P2P,
    CARD_HELP_RES_BASE,
    CARD_HELP_RES_BATTL,
    CARD_HELP_RES_FIELD,
    CARD_HELP_RES_HAND,
    CARD_HELP_RES_HAND2,
    CARD_HELP_RES_JUNK,
    CARD_HELP_RES_LOST,
    CARD_HELP_RES_SITUATION,
    CARD_HELP_RES_STANDBY,
    CARD_HELP_RES_DECKMAKE,
    CARD_HELP_RES_DECKLIST,
    CARD_HELP_RES_PHASE1,
    CARD_HELP_RES_PHASE2,
    CARD_HELP_RES_PHASE3,
    CARD_HELP_RES_PHASE4,
    CARD_HELP_RES_PHASE5,
    CARD_HELP_RES_PHASE6,
    CARD_HELP_RES_PHASE7,
};

/*
 * Help pages: page ids are the indices of CardTitleTxtLst (0x00a483e8, 59 entries) and of the
 * jump table in CardDispHelpCard; each name spells that page's title string. Pages whose
 * title repeats get an _A/_B/_C suffix in id order.
 */
enum CardHelpPage {
    CARD_HELP_PAGE_WHAT_IS_XENO_CARD = 0,
    CARD_HELP_PAGE_CONTROLS = 1,
    CARD_HELP_PAGE_CARD_OUTLINE = 2,
    CARD_HELP_PAGE_GAME_PROGRESSION = 3,
    CARD_HELP_PAGE_PROGRESSION_PHASE = 4,
    CARD_HELP_PAGE_PROGRESSION_TURN = 5,
    CARD_HELP_PAGE_POSITIONING = 6,
    CARD_HELP_PAGE_DECK = 7,
    CARD_HELP_PAGE_LOST_PILE = 8,
    CARD_HELP_PAGE_HAND = 9,
    CARD_HELP_PAGE_JUNK_PILE = 10,
    CARD_HELP_PAGE_STANDBY_AREA = 11,
    CARD_HELP_PAGE_SITUATION_AREA = 12,
    CARD_HELP_PAGE_BATTLEFIELD = 13,
    CARD_HELP_PAGE_FIELD = 14,
    CARD_HELP_PAGE_CARD = 15,
    CARD_HELP_PAGE_BATTLE_CARD = 16,
    CARD_HELP_PAGE_EVENT_CARD = 17,
    CARD_HELP_PAGE_SITUATION_CARD = 18,
    CARD_HELP_PAGE_DETAILS = 19,
    CARD_HELP_PAGE_TYPES = 20,
    CARD_HELP_PAGE_NAMES = 21,
    CARD_HELP_PAGE_ATTRIBUTES = 22,
    CARD_HELP_PAGE_REQUIREMENTS_A = 23,
    CARD_HELP_PAGE_REQUIREMENTS_B = 24,
    CARD_HELP_PAGE_COST = 25,
    CARD_HELP_PAGE_HP = 26,
    CARD_HELP_PAGE_ATK = 27,
    CARD_HELP_PAGE_ATTACK_TYPE = 28,
    CARD_HELP_PAGE_TEXT = 29,
    CARD_HELP_PAGE_HOW_TO_PLAY = 30,
    CARD_HELP_PAGE_TURNS_A = 31,
    CARD_HELP_PAGE_TURNS_B = 32,
    CARD_HELP_PAGE_DRAW_PHASE = 33,
    CARD_HELP_PAGE_MOVE_PHASE = 34,
    CARD_HELP_PAGE_EVENT_PHASE_A = 35,
    CARD_HELP_PAGE_EVENT_PHASE_B = 36,
    CARD_HELP_PAGE_EVENT_PHASE_C = 37,
    CARD_HELP_PAGE_SET_PHASE_A = 38,
    CARD_HELP_PAGE_SET_PHASE_B = 39,
    CARD_HELP_PAGE_BLOCK_PHASE = 40,
    CARD_HELP_PAGE_BATTLE_PHASE_A = 41,
    CARD_HELP_PAGE_BATTLE_PHASE_B = 42,
    CARD_HELP_PAGE_ADJUST_PHASE = 43,
    CARD_HELP_PAGE_SPECIAL_CARD = 44,
    CARD_HELP_PAGE_WEAPONS_A = 45,
    CARD_HELP_PAGE_WEAPONS_B = 46,
    CARD_HELP_PAGE_GNOSIS = 47,
    CARD_HELP_PAGE_TERMINOLOGY = 48,
    CARD_HELP_PAGE_PENETRATE = 49,
    CARD_HELP_PAGE_DOWN = 50,
    CARD_HELP_PAGE_E_MARK = 51,
    CARD_HELP_PAGE_EVENT_BLOCK = 52,
    CARD_HELP_PAGE_OPENING_CARDS = 53,
    CARD_HELP_PAGE_VIEWING_CARDS = 54,
    CARD_HELP_PAGE_CREATING_DECKS = 55,
    CARD_HELP_PAGE_SAVING_DECKS = 56,
    CARD_HELP_PAGE_TWO_PLAYER_BATTLE = 57,
    CARD_HELP_PAGE_SELECTING_DECKS = 58,
    CARD_HELP_PAGE_COUNT = 59  /* slti 0x3B at 0x00a20d68 */
};
#define CARD_HELP_PAGE_LAST (CARD_HELP_PAGE_COUNT - 1)  /* li 0x3A at 0x00a20da8 */

static int ChangeCnt = 0;
extern Matrix4 asDir;

/*
 * Help-title list state CHPTitleInitSub resets when CardHelpProc
 * (0x00a20908) opens a help category; CHPMenuInitSub (0x00a1e640, not part
 * of this TU's claimed source) runs immediately after it. TitleMaxList
 * (config/symbols/ov10.txt, 7 bytes) holds one entry-count byte per
 * category, read here through a pointer captured early and dereferenced
 * once per field (count and limit each get their own read). Only the
 * members CHPTitleInitSub writes are named; +0x04 stays unmodeled because
 * this function does not touch it.
 */
typedef struct CHPTitleList {
    s16 cursor;      /* +0x00: selected row, reset to the top */
    s8 flagA;        /* +0x02 */
    s8 flagB;        /* +0x03 */
    u8 unmodeled_04;
    s8 flagC;        /* +0x05 */
    s16 x;           /* +0x06: screen column */
    s16 y;           /* +0x08: screen row */
    s16 rowHeight;   /* +0x0A */
    s16 scroll;      /* +0x0C: top row offset, reset */
    s16 step;        /* +0x0E: row step */
    s16 count;       /* +0x10: TitleMaxList[category], sign-extended */
    s16 limit;       /* +0x12: TitleMaxList[category], sign-extended */
} CHPTitleList;

extern const u8 TitleMaxList[7];

void CHPTitleInitSub(CHPTitleList *list, int category)
{
    const u8 *entry;
    u8 raw;

    list->x = 266;
    list->y = 50;
    entry = &TitleMaxList[category];
    list->rowHeight = 24;
    list->cursor = 0;
    list->flagA = 0;
    list->flagB = 0;
    list->flagC = 0;
    list->scroll = 0;
    raw = *entry;
    list->step = 1;
    list->count = (s8)raw;
    raw = *entry;
    list->limit = (s8)raw;
}

/*
 * CHPMenuInitSub (0x00a1e640, 0x64 bytes): resets the help-menu list at
 * D_00A578C8 (same field layout as CHPTitleList), then re-initialises the
 * title list D_00A57B38 for category 0 (TitleMaxList[0] = 6) by a tail jump
 * (0x00a1e69c `j CHPTitleInitSub`, a0 = 2nd argument, a1 = 0).
 * Callers: CardHelpProc 0x00a209ec and 0x00a21df0 (a0 = D_00A578C8, a1 = D_00A57B38).
 * CHPTitleInitSub is defined in this TU and returns void.
 */
void CHPMenuInitSub(CHPTitleList *menu, CHPTitleList *title)
{
    menu->x = 6;            /* 0x00a1e674 sh $6,0x6 */
    menu->y = 50;           /* 0x00a1e678 sh $7,0x8 */
    menu->rowHeight = 24;   /* 0x00a1e67c sh $8,0xA */
    menu->cursor = 0;
    menu->flagA = 0;
    menu->flagB = 0;
    menu->flagC = 0;
    menu->scroll = 0;
    menu->count = 9;        /* 0x00a1e680 sh $9,0x10 */
    menu->step = 1;         /* 0x00a1e684 sh $10,0xE */
    menu->limit = 7;        /* 0x00a1e670 sh $2,0x12 */
    CHPTitleInitSub(title, 0);
}

/*
 * CardHelpFileLoad (0x00a1e6a8, 0x294 bytes): loads the help-viewer resources.
 * For every name in the NULL-terminated list it reads "<dir><name>.lex" and then
 * "<dir><name>.xtx" from the disc into consecutive 16-byte aligned areas starting
 * at 0x01720000 and records each start address in cardlex_tbl[] / cardxtx_tbl[].
 * Same shape as CardTutorialInit (ov10/tu007).
 * Caller: CardMainInit 0x00a01a9c (no arguments, result unused). xglCdReadFile
 * returns the byte count (tested with blez v0 at 0x00a1e878 / 0x00a1e8e0); memset,
 * strcpy and strcat results are not read. `dir` and `names` are initialised
 * automatics: the compiler copies them from rodata (20 bytes + memset of the rest
 * at 0x00a1e6e4..0x00a1e714, 35 words at 0x00a1e738..0x00a1e7e4).
 */
#define CARD_HELP_FILE_BUFFER   0x01720000  /* lui s0,0x172 at 0x00a1e7f0 */
#define CARD_HELP_FILE_ALIGN    16          /* addiu 15 / and -16 at 0x00a1e884 */
#define CARD_HELP_FILE_MAX      64          /* loop bound s6 + 256 at 0x00a1e8e4 = sizeof cardlex_tbl / 4 */
#define CARD_HELP_NAME_COUNT    35          /* 34 names plus the NULL terminator (140-byte copy) */

void CardHelpFileLoad(void)
{
    char dir[64] = "data\\carddata\\help\\";
    char path[128];
    const char *names[CARD_HELP_NAME_COUNT] = {
        "h_tittle", "h_ph1", "h_ph2", "h_text", "h_event", "h_battle1", "h_battle2",
        "h_down", "h_Emark", "h_Eblake", "h_open", "h_viewer", "h_decksave", "help5",
        "help6", "h_1P2P", "h_base", "h_battl", "h_field", "h_hand", "h_hand2", "h_junk",
        "h_lost", "h_situation", "h_standby", "h_deckmake", "h_decklist", "h_Phase1",
        "h_Phase2", "h_Phase3", "h_Phase4", "h_Phase5", "h_Phase6", "h_Phase7", 0
    };
    u32 dest;
    int size;
    int i;

    dest = CARD_HELP_FILE_BUFFER;
    for (i = 0; i < CARD_HELP_FILE_MAX && names[i] != 0; i++) {
        memset(path, 0, sizeof(path));
        strcpy(path, dir);
        strcat(path, names[i]);
        strcat(path, ".lex");
        cardlex_tbl[i] = dest;
        size = xglCdReadFile(path, (void *)dest, 0, 1);
        if (size > 0) {
            dest += size;
            dest = (dest + (CARD_HELP_FILE_ALIGN - 1)) & ~(CARD_HELP_FILE_ALIGN - 1);
        }

        memset(path, 0, sizeof(path));
        strcpy(path, dir);
        strcat(path, names[i]);
        strcat(path, ".xtx");
        cardxtx_tbl[i] = (const char *)dest;
        size = xglCdReadFile(path, (void *)dest, 0, 1);
        if (size > 0) {
            dest += size;
            dest = (dest + (CARD_HELP_FILE_ALIGN - 1)) & ~(CARD_HELP_FILE_ALIGN - 1);
        }
    }
}

/*
 * CHPMenuBackDispSub (0x00a1e940, 0x130 bytes): three help-menu back panels, each
 * placed with xglMatrixTrans and entered with nmlModelEntryCard after
 * nmlModelSetTexture / nmlModelSetPlace / nmlModelSetRenderLevel(2).
 * Caller: CardHelpProc 0x00a20d34 (a0 = sp). All callees void (no $v0 reads).
 * The two float locals `left` and `zero` stay in callee-saved FP registers across
 * the calls; z is assigned before y in each position block.
 */
static void CHPMenuBackDispSub(Matrix4 matrix)
{
    Matrix4 transformed;
    Vector4 position;
    float left = -1.4184f;     /* 0xBFB58E22 */
    float zero = 0.0f;

    position.x = left;
    position.z = zero;
    position.y = zero;
    position.w = 1.0f;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, (const float *)&position);
    nmlModelSetTexture((const char *)0x01CB0000);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA2B00);

    position.x = 1.4184f;      /* 0x3FB58E22 */
    position.z = zero;
    position.y = zero;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, (const float *)&position);
    nmlModelSetTexture((const char *)0x01CB0000);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA1000);

    position.x = left;
    position.y = -1.832f;      /* 0xBFEA7EFA */
    xglMatrixTrans(transformed, (const float (*)[4])matrix, (const float *)&position);
    nmlModelSetTexture((const char *)0x01CC0050);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA4000);
}

/*
 * CardDispHelpCurrySet (0x00a1ea70, 0x140 bytes): three curry card models drawn at
 * translation (1.5143, y, 0, 1), scale (0.46, 0.46, 1, 1); y = 1.186, 0.215, -0.757
 * with card kinds 0, 0x54 (84), 0x78 (120) passed to CardDisp(.., 1, 0).
 * Caller: CardDispHelpCard 0x00a205f8. CardDisp is void (no $v0 read after
 * 0x00a1eb08/0x00a1eb4c/0x00a1eb90).
 */
void CardDispHelpCurrySet(Matrix4 matrix)
{
    Matrix4 transformed;
    float translation[4];
    float scale[4];

    translation[0] = 1.5143f;     /* 0x3FC1D495 */
    translation[1] = 1.186f;      /* 0x3F97CED9 */
    translation[2] = 0.0f;        /* sw $0 */
    translation[3] = 1.0f;
    scale[0] = 0.46f;             /* 0x3EEB851F */
    scale[1] = 0.46f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    CardDisp(transformed, 0, 1, 0);

    translation[1] = 0.215f;      /* 0x3E5C28F6 */
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    CardDisp(transformed, 84, 1, 0);

    translation[1] = -0.757f;     /* 0xBF41CAC1 */
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    CardDisp(transformed, 120, 1, 0);
}

/*
 * Help-slide animation (shared by CardDispHelpSlide1/2/3): the counter ChangeCnt wraps
 * around the list length and the entry shown changes every HELP_SLIDE_ENTRY_TICKS.
 */
#define HELP_SLIDE_ENTRY_TICKS      64                                  /* sra 6 at 0x00a1ec6c */
#define HELP_SLIDE_CYCLE_4          (4 * HELP_SLIDE_ENTRY_TICKS)        /* 256: sra 8 / sll 8 at 0x00a1ec4c */
#define HELP_SLIDE_CYCLE_3          (3 * HELP_SLIDE_ENTRY_TICKS)        /* 192: li a3,0xc0 at 0x00a1ec9c */
#define HELP_SLIDE_CYCLE_6          (6 * HELP_SLIDE_ENTRY_TICKS)        /* 384: six-entry lists of CardDispHelpSlide2 */

/*
 * CardDispHelpSlide1 (0x00a1ebb0, 0x1e0 bytes): help pages 16..18 draw one card of a
 * short list at translation `trans`, scale (0.368, 0.368, 1, 1), picking the entry from
 * ChangeCnt. The lists (card kinds) and `trans` are the function's initialised statics
 * (list16_0 .. trans_3 at 0x00a43bd0..0x00a43bf0 in the original symbol table).
 * Callers: CardDispHelpCard 0x00a2060c, 0x00a20654, 0x00a20684 (a0 = page id from the
 * page jump table, a1 = matrix). CardDisp is void: no $v0 read after the calls at
 * 0x00a1ed00 and 0x00a1ed78.
 */
void CardDispHelpSlide1(s8 page, Matrix4 matrix)
{
    static s16 list16[4] = { 0, 42, 2, 29 };
    static s16 list17[3] = { 84, 85, 86 };
    static s16 list18[3] = { 120, 121, 122 };
    static float trans[4] = { 0.55715f, 1.246f, 0.0f, 1.0f };
    Matrix4 transformed;
    float scale[4];
    int i;

    scale[0] = 0.368f;     /* 0x3EBC6A7F, lui/ori at 0x00a1ebb4 */
    scale[1] = 0.368f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    switch (page) {
    case CARD_HELP_PAGE_BATTLE_CARD:               /* body at 0x00a1ec28 */
        ChangeCnt %= HELP_SLIDE_CYCLE_4;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, trans);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list16[i], 1, 0);
        break;
    case CARD_HELP_PAGE_EVENT_CARD:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, trans);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list17[i], 1, 0);
        break;
    case CARD_HELP_PAGE_SITUATION_CARD:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, trans);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list18[i], 1, 0);
        break;
    }
}

/*
 * CardDispHelpSlide2 (0x00a1ed90, 0x578 bytes): help pages 19..22, 25..28 and 45..47 draw
 * one card of a list at translation (1.397, 0.179, 0, 1), scale (0.9, 0.9, 1, 1); pages
 * 22 and 25..28 light it through CardDispLight with a colour mask. The cycle is the entry
 * count times HELP_SLIDE_ENTRY_TICKS: 192 for three entries (li 0xc0, e.g. 0x00a1ee0c),
 * 384 for six (li 0x180 at 0x00a1ef44) and 256 for four (sra/sll 8 at 0x00a1f144/48). As in
 * CardDispHelpSlide3, the place is a CardHelpVector so the copy is the doubleword
 * ld/sd pair (sd $3,0x40($29) at 0x00a1edd4). Callers: CardDispHelpCard 0x00a206bc,
 * 0x00a207f4, 0x00a2080c.
 */
void CardDispHelpSlide2(s8 page, Matrix4 matrix)
{
    static s16 list19[3] = { 0, 84, 120 };
    static s16 list20[3] = { 0, 84, 120 };
    static s16 list21[3] = { 0, 84, 120 };
    static s16 list22[6] = { 0, 39, 67, 3, 49, 29 };
    static s16 list25[6] = { 0, 84, 120 };
    static s16 list26[6] = { 0, 3, 18, 33, 25, 66 };
    static s16 list27[6] = { 0, 3, 18, 33, 43, 66 };
    static s16 list28[4] = { 2, 0, 40, 41 };
    static s16 list45[6] = { 49, 50, 51, 42, 43, 44 };
    static s16 list46[6] = { 49, 50, 51, 42, 43, 44 };
    static s16 list47[4] = { 67, 69, 74, 75 };
    Matrix4 transformed;
    CardHelpVector translation = { { 1.397f, 0.179f, 0.0f, 1.0f } };
    float scale[4];
    int i;

    scale[0] = 0.9f;
    scale[1] = 0.9f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    switch (page) {
    case CARD_HELP_PAGE_DETAILS:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list19[i], 1, 0);
        break;
    case CARD_HELP_PAGE_TYPES:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list20[i], 1, 0);
        break;
    case CARD_HELP_PAGE_NAMES:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list21[i], 1, 0);
        break;
    case CARD_HELP_PAGE_ATTRIBUTES:
        ChangeCnt %= HELP_SLIDE_CYCLE_6;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDispLight(transformed, list22[i], 1, 0x10);
        break;
    case CARD_HELP_PAGE_COST:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDispLight(transformed, list25[i], 1, 0x8);
        break;
    case CARD_HELP_PAGE_HP:
        ChangeCnt %= HELP_SLIDE_CYCLE_6;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDispLight(transformed, list26[i], 1, 0x2);
        break;
    case CARD_HELP_PAGE_ATK:
        ChangeCnt %= HELP_SLIDE_CYCLE_6;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDispLight(transformed, list27[i], 1, 0x4);
        break;
    case CARD_HELP_PAGE_ATTACK_TYPE:
        ChangeCnt %= HELP_SLIDE_CYCLE_4;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDispLight(transformed, list28[i], 1, 0x40);
        break;
    case CARD_HELP_PAGE_WEAPONS_A:
        ChangeCnt %= HELP_SLIDE_CYCLE_6;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list45[i], 1, 0);
        break;
    case CARD_HELP_PAGE_WEAPONS_B:
        ChangeCnt %= HELP_SLIDE_CYCLE_6;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list46[i], 1, 0);
        break;
    case CARD_HELP_PAGE_GNOSIS:
        ChangeCnt %= HELP_SLIDE_CYCLE_4;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list47[i], 1, 0);
        break;
    }
}

/*
 * CardDispHelpSlide3 (0x00a1f308, 0x164 bytes): help pages 23 and 24 draw one of the
 * three cards of list23/list24 (both {0, 84, 120}, the card kinds CardDispHelpCurrySet
 * draws) at a fixed place, scale (0.7, 0.7, 1, 1); the entry follows ChangeCnt like
 * CardDispHelpSlide1. The place is an automatic initialised from a constant 16-byte
 * vector; the u64 member of CardHelpVector gives the aggregate the 8-byte alignment
 * that yields the doubleword copy (ld/ld/sd/sd at 0x00a1f330..0x00a1f34c) instead of
 * ldl/ldr pairs. No caller exists in the overlay (the jal word 0x0c287cc2 does not
 * occur): CardDispHelpCard reaches pages 23/24 through CardDispFieldPoint. CardDisp is
 * void: no $v0 read after the calls at 0x00a1f3dc and 0x00a1f454.
 */
void CardDispHelpSlide3(s8 page, Matrix4 matrix)
{
    static s16 list23[3] = { 0, 84, 120 };
    static s16 list24[3] = { 0, 84, 120 };
    Matrix4 transformed;
    CardHelpVector translation = { { 1.733f, 0.671f, 0.0f, 1.0f } };
    float scale[4];
    int i;

    scale[0] = 0.7f;       /* 0x3F333333, lui/ori at 0x00a1f320 */
    scale[1] = 0.7f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    switch (page) {
    case CARD_HELP_PAGE_REQUIREMENTS_A:               /* body at 0x00a1f378 */
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list23[i], 1, 0);
        break;
    case CARD_HELP_PAGE_REQUIREMENTS_B:
        ChangeCnt %= HELP_SLIDE_CYCLE_3;
        i = ChangeCnt / HELP_SLIDE_ENTRY_TICKS;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation.v);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        CardDisp(transformed, list24[i], 1, 0);
        break;
    }
}

void CardDispCtr(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;

    translation = D_00A4E0A0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_HELP5]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_HELP5]);
}

void CardDispField(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;

    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_FIELD]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_FIELD]);
}

void CardDispDeckMake(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    CardHelpFloat4 scale;

    translation = D_00A4E0B0;
    scale = D_00A4E0C0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   scale.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_DECKMAKE]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_DECKMAKE]);
}

void CardDispH1P2P(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;

    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_H1P2P]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_H1P2P]);
}

void CardDispTurn(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    CardHelpFloat4 scale;

    translation = D_00A4E0D0;
    scale = D_00A4E0E0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   scale.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_HELP6]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_HELP6]);
}

void CardDispFieldPointScale(Matrix4 matrix, const float translation[4],
                              const float scale[4], int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    float light;

    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

/*
 * CardDispTejyun (0x00a1f780, 0x5f0 bytes): the "tejyun" (procedure) help pages
 * DRAW_PHASE..ADJUST_PHASE (CardDispHelpCard passes the phase resource CARD_HELP_RES_PHASE1..7
 * as `kind`). Every page draws the frame card CARD_HELP_RES_FIELD at
 * D_00A4E0F0 scaled by D_00A4E110, then a column of three phase cards starting at
 * kind, placed at D_00A4E100, each 0.3 lower than the one above it, with
 * the page's own card lit by nmlModelSetLight and the extra cards the page
 * explains drawn by CardDispFieldPointScale. kind indexes a seven-entry jump
 * table (jtbl_00A4E120); an out-of-range kind draws the frame only.
 * The two card tables are read directly, so GCC rematerialises their bases per
 * case, and the three-card tails of the PHASE1 and PHASE2 kinds are identical and get
 * cross-jumped.
 */
void CardDispTejyun(Matrix4 matrix, int kind)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    CardHelpFloat4 offset;
    CardHelpFloat4 scale;
    Matrix4 color;
    float light;

    translation = D_00A4E0F0;
    offset = D_00A4E100;
    scale = D_00A4E110;
    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   scale.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_FIELD]);
    nmlModelEntry(cardlex_tbl[CARD_HELP_RES_FIELD]);
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   offset.v);

    switch (kind) {
    case CARD_HELP_RES_PHASE1:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_BASE);
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_HAND);
        nmlModelSetLight(color, asDir);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE1]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE1]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE2]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE2]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE3]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE3]);
        break;
    case CARD_HELP_RES_PHASE2:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_BATTL);
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_STANDBY);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE1]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE1]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE2]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE2]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE3]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE3]);
        break;
    case CARD_HELP_RES_PHASE3:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_HAND);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE2]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE2]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE3]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE3]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE4]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE4]);
        break;
    case CARD_HELP_RES_PHASE4:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_HAND);
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_STANDBY);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE3]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE3]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE4]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE4]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE5]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE5]);
        break;
    case CARD_HELP_RES_PHASE5:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_HAND2);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE4]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE4]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE5]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE5]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE6]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE6]);
        break;
    case CARD_HELP_RES_PHASE6:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_BATTL);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE5]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE5]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE6]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE6]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE7]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE7]);
        break;
    case CARD_HELP_RES_PHASE7:
        CardDispFieldPointScale(matrix, translation.v,
                                scale.v, CARD_HELP_RES_HAND);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE5]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE5]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE6]);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE6]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[CARD_HELP_RES_PHASE7]);
        nmlModelSetLight(color, asDir);
        nmlModelEntry(cardlex_tbl[CARD_HELP_RES_PHASE7]);
        break;
    }
}

void CardDispFieldPoint(Matrix4 matrix, const float translation[4], int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    float light;

    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

/*
 * CardDispGun (0x00a1fe60, 0xb8 bytes): translation D_00A4E140 {1.397, 0.863, 0, 1},
 * scale D_00A4E150 {0.6, 0.6, 1, 1}; CardDisp(kind 0x31, 1, 0); then the model is
 * moved down by 1.33 (double: fptodp 0x00a1fecc, ld D_00A4E160, dpsub, dptofp,
 * stored back at sp+0x34 = transformed[3][1]) and CardDisp(kind 0x44, 1, 0).
 * Caller: CardDispHelpCard 0x00a207e0. CardDisp (0x00a0a2f0) is void (no $v0 read
 * at 0x00a1fec8/0x00a1ff08).
 */
void CardDispGun(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    CardHelpFloat4 scale;

    translation = D_00A4E140;
    scale = D_00A4E150;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   scale.v);
    CardDisp(transformed, 49, 1, 0);      /* 0x00a1feb8 addiu a1,0x31 */
    transformed[3][1] -= 1.33;            /* D_00A4E160 (double) */
    CardDisp(transformed, 68, 1, 0);      /* 0x00a1fef4 addiu a1,0x44 */
}


void CardDispShineCurry(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;
    CardHelpFloat4 scale;

    scale = D_00A4E170;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   scale.v);
    CardDispLight(transformed, 84, 1, 255);
}

void CardDispAttr(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    const float step = -0.38f;

    translation = D_00A4E180;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetPlace((const Vector4 *)transformed);

    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87400);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87C00);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88000);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87800);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88800);
    transformed[3][1] += -0.57f;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88400);
}

void CardDispNazo(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;

    translation = D_00A4E190;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C89000);
}

void CardDispGuno(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;

    translation = D_00A4E1A0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88000);
}

/*
 * CardDispOnce (0x00a201d0, 0x94 bytes): draws one card model at a fixed
 * offset (D_00A4E1B0 = {1.397, 0.179, 0, 1}) with scale 0.9/0.9/1/1 and lights
 * it through CardDispLight(card id, enabled = 1, colour 32).
 * Callers: CardDispHelpCard 0x00a206dc (card 7) and 0x00a206fc (card 0x1D).
 * `card` is a plain int: the sll at 0x00a20210 and sra at 0x00a20240 widen it to
 * the s16 `card` parameter of CardDispLight, so the callers pass a plain int.
 */
void CardDispOnce(Matrix4 matrix, int card)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;
    float scale[4];

    translation = D_00A4E1B0;      /* ld 0x00a201f8/0x00a20200, sd 0x00a20214/0x00a20218 */
    scale[0] = 0.9f;               /* 0x3F666666 */
    scale[1] = 0.9f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    CardDispLight(transformed, card, 1, 32);   /* 0x00a2024c: a2 = 1, a3 = 0x20 */
}


void CardDispComm(Matrix4 matrix, int kind)
{
    Matrix4 transformed;
    CardHelpFloat4 translation;

    translation = D_00A4E1C0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

void CardDispPhase(Matrix4 matrix, int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    CardHelpFloat4 translation;
    float light;

    translation = D_00A4E1D0;
    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   translation.v);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

const CardHelpFloat4 D_00A4E1E0 = {{ 1.397f, 0.215f, 0.0f, 1.0f }};
const CardHelpFloat4 D_00A4E1F0 = {{ 1.397f, -0.188f, 0.0f, 1.0f }};

void CardDispTejyun(Matrix4 matrix, int kind);

#define CARD_HELP_CARD_REQUIREMENTS_A    7     /* card shown on the first REQUIREMENTS page: li a1,7 at 0x00a206e0 */
#define CARD_HELP_CARD_REQUIREMENTS_B    29    /* card shown on the second REQUIREMENTS page: li a1,0x1D at 0x00a20700 */

/*
 * Help-card page drawer: `page` selects one of 59 page ids; `matrix` is the
 * page's world matrix. Each arm is the original call sequence for that page id,
 * grouped by the callee names; the page ids are the CardHelpPage values, i.e. the
 * index of the original jump table at 0x00a4e200 (59 entries); ids 48, 57 and out-of-range
 * values fall through to the common exit. The two vectors are copied to the
 * stack before the switch: pair[0] at sp+0, pair[1] at sp+16.
 */
void CardDispHelpCard(s8 page, Matrix4 matrix)
{
    CardHelpFloat4 pair[2];

    ChangeCnt++;
    pair[0] = D_00A4E1E0;
    pair[1] = D_00A4E1F0;
    switch (page) {
    case CARD_HELP_PAGE_WHAT_IS_XENO_CARD:
        CardDispComm(matrix, CARD_HELP_RES_TITTLE);
        break;
    case CARD_HELP_PAGE_CONTROLS:
        CardDispCtr(matrix);
        break;
    case CARD_HELP_PAGE_GAME_PROGRESSION:
        CardDispComm(matrix, CARD_HELP_RES_PH1);
        break;
    case CARD_HELP_PAGE_PROGRESSION_PHASE:
        CardDispComm(matrix, CARD_HELP_RES_PH1);
        CardDispPhase(matrix, CARD_HELP_RES_PH2);
        break;
    case CARD_HELP_PAGE_POSITIONING:
        CardDispField(matrix, pair[0].v);
        CardDispH1P2P(matrix, pair[0].v);
        break;
    case CARD_HELP_PAGE_DECK:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_BASE);
        break;
    case CARD_HELP_PAGE_LOST_PILE:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_LOST);
        break;
    case CARD_HELP_PAGE_HAND:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_HAND);
        break;
    case CARD_HELP_PAGE_JUNK_PILE:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_JUNK);
        break;
    case CARD_HELP_PAGE_STANDBY_AREA:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_STANDBY);
        break;
    case CARD_HELP_PAGE_SITUATION_AREA:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_SITUATION);
        break;
    case CARD_HELP_PAGE_BATTLEFIELD:
        CardDispField(matrix, pair[0].v);
        CardDispFieldPoint(matrix, pair[0].v, CARD_HELP_RES_BATTL);
        break;
    case CARD_HELP_PAGE_FIELD:
        CardDispField(matrix, pair[0].v);
        break;
    case CARD_HELP_PAGE_CARD_OUTLINE:
    case CARD_HELP_PAGE_CARD:
        CardDispHelpCurrySet(matrix);
        break;
    case CARD_HELP_PAGE_BATTLE_CARD:
        CardDispHelpSlide1(page, matrix);
        CardDispField(matrix, pair[1].v);
        CardDispFieldPoint(matrix, pair[1].v, CARD_HELP_RES_BATTL);
        CardDispFieldPoint(matrix, pair[1].v, CARD_HELP_RES_STANDBY);
        break;
    case CARD_HELP_PAGE_EVENT_CARD:
        CardDispHelpSlide1(page, matrix);
        CardDispField(matrix, pair[1].v);
        CardDispShineCurry(matrix, pair[1].v);
        break;
    case CARD_HELP_PAGE_SITUATION_CARD:
        CardDispHelpSlide1(page, matrix);
        CardDispField(matrix, pair[1].v);
        CardDispFieldPoint(matrix, pair[1].v, CARD_HELP_RES_SITUATION);
        break;
    case CARD_HELP_PAGE_ATTRIBUTES:
        CardDispHelpSlide2(page, matrix);
        CardDispAttr(matrix);
        break;
    case CARD_HELP_PAGE_REQUIREMENTS_A:
        CardDispOnce(matrix, CARD_HELP_CARD_REQUIREMENTS_A);
        break;
    case CARD_HELP_PAGE_REQUIREMENTS_B:
        CardDispNazo(matrix);
        CardDispOnce(matrix, CARD_HELP_CARD_REQUIREMENTS_B);
        break;
    case CARD_HELP_PAGE_TEXT:
        CardDispComm(matrix, CARD_HELP_RES_TEXT);
        break;
    case CARD_HELP_PAGE_PROGRESSION_TURN:
    case CARD_HELP_PAGE_HOW_TO_PLAY:
    case CARD_HELP_PAGE_TURNS_A:
    case CARD_HELP_PAGE_TURNS_B:
        CardDispTurn(matrix);
        break;
    case CARD_HELP_PAGE_DRAW_PHASE:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE1);
        break;
    case CARD_HELP_PAGE_MOVE_PHASE:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE2);
        break;
    case CARD_HELP_PAGE_EVENT_PHASE_A:
    case CARD_HELP_PAGE_EVENT_PHASE_B:
    case CARD_HELP_PAGE_EVENT_PHASE_C:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE3);
        break;
    case CARD_HELP_PAGE_SET_PHASE_A:
    case CARD_HELP_PAGE_SET_PHASE_B:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE4);
        break;
    case CARD_HELP_PAGE_BLOCK_PHASE:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE5);
        break;
    case CARD_HELP_PAGE_BATTLE_PHASE_A:
    case CARD_HELP_PAGE_BATTLE_PHASE_B:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE6);
        break;
    case CARD_HELP_PAGE_ADJUST_PHASE:
        CardDispTejyun(matrix, CARD_HELP_RES_PHASE7);
        break;
    case CARD_HELP_PAGE_SPECIAL_CARD:
        CardDispGun(matrix);
        break;
    case CARD_HELP_PAGE_DETAILS:
    case CARD_HELP_PAGE_TYPES:
    case CARD_HELP_PAGE_NAMES:
    case CARD_HELP_PAGE_COST:
    case CARD_HELP_PAGE_HP:
    case CARD_HELP_PAGE_ATK:
    case CARD_HELP_PAGE_ATTACK_TYPE:
    case CARD_HELP_PAGE_WEAPONS_A:
    case CARD_HELP_PAGE_WEAPONS_B:
        CardDispHelpSlide2(page, matrix);
        break;
    case CARD_HELP_PAGE_GNOSIS:
        CardDispHelpSlide2(page, matrix);
        CardDispGuno(matrix);
        break;
    case CARD_HELP_PAGE_PENETRATE:
        CardDispComm(matrix, CARD_HELP_RES_BATTLE2);
        break;
    case CARD_HELP_PAGE_DOWN:
        CardDispComm(matrix, CARD_HELP_RES_DOWN);
        break;
    case CARD_HELP_PAGE_E_MARK:
        CardDispComm(matrix, CARD_HELP_RES_EMARK);
        break;
    case CARD_HELP_PAGE_EVENT_BLOCK:
        CardDispComm(matrix, CARD_HELP_RES_EBLAKE);
        break;
    case CARD_HELP_PAGE_OPENING_CARDS:
        CardDispComm(matrix, CARD_HELP_RES_OPEN);
        break;
    case CARD_HELP_PAGE_VIEWING_CARDS:
        CardDispComm(matrix, CARD_HELP_RES_VIEWER);
        break;
    case CARD_HELP_PAGE_CREATING_DECKS:
        CardDispComm(matrix, CARD_HELP_RES_DECKMAKE);
        break;
    case CARD_HELP_PAGE_SAVING_DECKS:
        CardDispComm(matrix, CARD_HELP_RES_DECKSAVE);
        break;
    case CARD_HELP_PAGE_SELECTING_DECKS:
        CardDispComm(matrix, CARD_HELP_RES_DECKLIST);
        break;
    default:  /* 48, 57 and out-of-range ids: nothing to draw */
        break;
    }
}

/* Help-page data and callees used by CardHelpProc. */

/*
 * CardHelpProc states: the high nibble of CardGameWork.helpState selects the screen
 * (`andi $4,$2,0xF0` at 0x00a209ac); every other value shows a help page.
 */
#define CARD_HELP_STATE_MASK    0xF0    /* 0x00a209ac */
#define CARD_HELP_STATE_OFF     0x00    /* sb $0 at 0x00a20e34: back out of the first page, help ends */
#define CARD_HELP_STATE_PAGE    0x01    /* sb 1 at 0x00a20a20, 0x00a20c60: show helpTopic */
#define CARD_HELP_STATE_INDEX   0x10    /* sb at 0x00a209f4, 0x00a20b88, 0x00a20e0c: category list ("Help Index") */
#define CARD_HELP_STATE_TOPICS  0x20    /* sb at 0x00a20abc, case at 0x00a209a8: topic list of one category */
#define CARD_HELP_STATE_OPEN    0x30    /* case at 0x00a209bc: reset both lists, then run the INDEX screen */

/* Pad bits CardHelpProc tests; input[].pressed unless noted. */
#define CARD_HELP_PAD_NEXT            0x0008    /* andi 0x8 at 0x00a209c8: next page (CardTutorialProc's NEXT bit) */
#define CARD_HELP_PAD_PREV            0x0004    /* andi 0x4 at 0x00a20d80: previous page (CardTutorialProc's PREV bit) */
#define CARD_HELP_PAD_BACK            0x0040    /* andi 0x40 at 0x00a20a08, 0x00a20e10: cancel (sound 2), leave the screen / page */
#define CARD_HELP_PAD_BACK_OR_LEFT    0x8040    /* andi 0x8040 at 0x00a20b70: cancel the topic list (sound 2) */
#define CARD_HELP_PAD_SELECT          0x0020    /* andi 0x20 at 0x00a20bd0: open the chosen topic (sound 1) */
#define CARD_HELP_PAD_SELECT_OR_RIGHT 0x2020    /* andi 0x2020 at 0x00a20a5c: open the chosen category's topics (sound 1) */
#define CARD_HELP_PAD_OPEN_INDEX      0x0080    /* andi 0x80 at 0x00a20d88: open the help index (sound 1) */
#define CARD_HELP_DIR_UP              0x1000    /* input[].directions, andi at 0x00a20a24, 0x00a20b8c: previous row */
#define CARD_HELP_DIR_DOWN            0x4000    /* input[].directions, andi at 0x00a20a2c, 0x00a20bc8: next row */

/* Sound effect ids (first argument of xglSoundEffectNormalID; the second is 0). */
#define CARD_HELP_SE_SELECT     1       /* li a0,1 at 0x00a20aac, 0x00a20c18, 0x00a20df8 */
#define CARD_HELP_SE_CANCEL     2       /* li a0,2 at 0x00a20a10, 0x00a20b78, 0x00a20e18 */
#define CARD_HELP_SE_CURSOR     3       /* li a0,3 at 0x00a20a3c, 0x00a20bf0, 0x00a20d4c */

#define CARD_HELP_CATEGORY_LAST     6       /* slti 6 at 0x00a20a68: seven categories (TitleMaxList[7]) */
#define CARD_HELP_TOPICS_PER_CATEGORY 14    /* SkipPageLst row stride, x14 at 0x00a20c3c..0x00a20c48 (7 rows, 0x62 bytes) */
#define CARD_HELP_HISTORY_MAX       100     /* sltiu 0x64 at 0x00a20c4c, 0x00a20db4: bound of activeHelpPage */
#define CARD_HELP_RADIX             10      /* page-number digits: addiu 0xA at 0x00a20e90 */

extern CHPTitleList D_00A578C8;
extern CHPTitleList D_00A57B38;
extern u8 SkipPageLst[];
extern s8 SkipStartList[];
extern s8 PageStatusLst[];
extern char *CardSkipTitleTxtLst[];
extern char *CardBookMarkTxtLst[];
extern char *CardTitleTxtLst[];
extern char *CardHelpTxtLst[];
extern void CardStrListDisp(CHPTitleList *list, char **strings, int index);
extern void CardDispTxtSub(int x, int y, char *text, int color);
extern void CardDispTxtSub2(u16 x, u16 y, const char *text);
extern void xglMatrixUnit(float matrix[4][4]);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglFontPrint(int x, int y, int color, const char *text);

void CardHelpProc(CardGameWork *work)
{
    Matrix4 matrix;
    Matrix4 transformed;
    float translation[4];
    float scale[4];
    u16 pressed;
    u16 directions;
    int state;
    int cardModel;
    int digit;

    xglMatrixUnit(matrix);
    scale[0] = scale[1] = 0.97f;
    scale[2] = scale[3] = 1.0f;
    translation[0] = translation[1] = translation[2] = 0.0f;
    translation[3] = 1.0f;
    xglMatrixScale(matrix, (const float (*)[4])matrix, scale);

    pressed = work->command.input[0].pressed;
    directions = work->command.input[0].directions;
    if (work->menuPlayer != 0) {
        pressed = work->command.input[1].pressed;
        directions = work->command.input[1].directions;
    }

    state = work->helpState & CARD_HELP_STATE_MASK;
    switch (state) {
    case CARD_HELP_STATE_OPEN:
        CHPMenuInitSub(&D_00A578C8, &D_00A57B38);
        work->helpState = CARD_HELP_STATE_INDEX;
        /* falls through into the INDEX body */
    case CARD_HELP_STATE_INDEX: {
        if (pressed & CARD_HELP_PAD_BACK) {
            xglSoundEffectNormalID(CARD_HELP_SE_CANCEL, 0);
            work->helpState = CARD_HELP_STATE_PAGE;
        }
        if (directions & CARD_HELP_DIR_UP) {
            if (D_00A578C8.flagC != 0) {
                xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
                D_00A578C8.flagC--;
                CHPTitleInitSub(&D_00A57B38, D_00A578C8.flagC);
            }
        } else if (directions & CARD_HELP_DIR_DOWN) {
            if (D_00A578C8.flagC < CARD_HELP_CATEGORY_LAST) {
                xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
                D_00A578C8.flagC++;
                CHPTitleInitSub(&D_00A57B38, D_00A578C8.flagC);
            }
        } else if (pressed & CARD_HELP_PAD_SELECT_OR_RIGHT) {
            xglSoundEffectNormalID(CARD_HELP_SE_SELECT, 0);
            work->helpState = CARD_HELP_STATE_TOPICS;
        }
        xglFontPrint(0, 0, 0, "\031\003");
        xglFontPrint(6, 0x19, 0xFFF0, "\r");
        xglFontPrint(6, 0x19, 0xFFF0, "Help Index");
        xglFontPrint(0x10A, 0x19, 0xFFF0, "Topics");
        CardStrListDisp(&D_00A578C8, CardBookMarkTxtLst, D_00A578C8.flagC);
        CardStrListDisp(&D_00A57B38, &CardSkipTitleTxtLst[SkipStartList[D_00A578C8.flagC]], -1);
        CHPMenuBackDispSub(matrix);
    } break;
    case CARD_HELP_STATE_TOPICS: {
        if (pressed & CARD_HELP_PAD_BACK_OR_LEFT) {
            xglSoundEffectNormalID(CARD_HELP_SE_CANCEL, 0);
            work->helpState = CARD_HELP_STATE_INDEX;
        }
        if (directions & CARD_HELP_DIR_UP) {
            if (D_00A57B38.flagC != 0) {
                xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
                D_00A57B38.flagC--;
            }
        } else if (directions & CARD_HELP_DIR_DOWN) {
            if (D_00A57B38.flagC < D_00A57B38.count - 1) {
                xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
                D_00A57B38.flagC++;
            }
        } else if (pressed & CARD_HELP_PAD_SELECT) {
            xglSoundEffectNormalID(CARD_HELP_SE_SELECT, 0);
            work->helpTopic = SkipPageLst[D_00A57B38.flagC + D_00A578C8.flagC * CARD_HELP_TOPICS_PER_CATEGORY];
            work->helpState = CARD_HELP_STATE_PAGE;
            if (work->activeHelpPage < CARD_HELP_HISTORY_MAX) {
                work->activeHelpPage++;
            }
            work->helpPage[work->activeHelpPage] = work->helpTopic;
        }
        CardStrListDisp(&D_00A578C8, CardBookMarkTxtLst, D_00A578C8.flagC);
        CardStrListDisp(&D_00A57B38, &CardSkipTitleTxtLst[SkipStartList[D_00A578C8.flagC]], D_00A57B38.flagC);
        xglFontPrint(0, 0, 0, "\031\003");
        xglFontPrint(6, 0x19, 0xFFF0, "\r");
        xglFontPrint(6, 0x19, 0xFFF0, "Help Index");
        xglFontPrint(0x10A, 0x19, 0xFFF0, "Topics");
        CHPMenuBackDispSub(matrix);
    } break;
    default: {
        if (pressed & CARD_HELP_PAD_NEXT) {
            xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
            work->helpTopic++;
            if ((s8)work->helpTopic >= CARD_HELP_PAGE_COUNT) {
                work->helpTopic = 0;
            }
            if (work->activeHelpPage < CARD_HELP_HISTORY_MAX) {
                work->activeHelpPage++;
            }
            work->helpPage[work->activeHelpPage] = work->helpTopic;
        } else if (pressed & CARD_HELP_PAD_PREV) {
            xglSoundEffectNormalID(CARD_HELP_SE_CURSOR, 0);
            work->helpTopic = (s8)work->helpTopic - 1;
            if ((s8)work->helpTopic < 0) {
                work->helpTopic = CARD_HELP_PAGE_LAST;
            }
            if (work->activeHelpPage < CARD_HELP_HISTORY_MAX) {
                work->activeHelpPage++;
            }
            work->helpPage[work->activeHelpPage] = work->helpTopic;
        } else if (pressed & CARD_HELP_PAD_OPEN_INDEX) {
            CHPMenuInitSub(&D_00A578C8, &D_00A57B38);
            xglSoundEffectNormalID(CARD_HELP_SE_SELECT, 0);
            work->helpState = CARD_HELP_STATE_INDEX;
        } else if (pressed & CARD_HELP_PAD_BACK) {
            xglSoundEffectNormalID(CARD_HELP_SE_CANCEL, 0);
            if (work->activeHelpPage == 0) {
                work->helpState = CARD_HELP_STATE_OFF;
            } else {
                work->activeHelpPage--;
                work->helpTopic = work->helpPage[work->activeHelpPage];
            }
        }

        xglFontPrint(6, 0x19, 0xFFF0, "");
        CardDispTxtSub(6, 0x19, CardTitleTxtLst[(s8)work->helpTopic], 0xC);
        CardDispTxtSub2(6, 0x32, CardHelpTxtLst[(s8)work->helpTopic]);
        CardDispTxtSub(0x11E, 0x19, CardBookMarkTxtLst[PageStatusLst[3 * (s8)work->helpTopic]], 0xC);
        CardDispHelpCard((s8)work->helpTopic, matrix);

        translation[1] = -1.496f;
        translation[2] = 0.0f;
        translation[3] = 1.0f;
        scale[0] = scale[1] = 0.9f;
        scale[2] = scale[3] = 1.0f;
        translation[0] = 1.0064001f;
        cardModel = 0x01C8A800 + (PageStatusLst[3 * (s8)work->helpTopic] << 10);
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture((const char *)0x01C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);

        digit = (PageStatusLst[3 * (s8)work->helpTopic + 1] + 1) / CARD_HELP_RADIX;
        if (digit != 0) {
            translation[0] = 1.3564f;
            cardModel = 0x01C8A400 + (digit << 10);
            xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
            xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
            nmlModelSetPlace((const Vector4 *)transformed);
            nmlModelSetTexture((const char *)0x01C990A0);
            nmlModelSetRenderLevel(2);
            nmlModelEntryCard((void *)cardModel);
        }

        cardModel = 0x01C8A400 + (((PageStatusLst[3 * (s8)work->helpTopic + 1] + 1) % CARD_HELP_RADIX) << 10);
        translation[0] = 1.4824001f;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture((const char *)0x01C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);

        digit = ((s8)work->helpTopic + 1) / CARD_HELP_RADIX;
        if (digit != 0) {
            translation[0] = 1.8304f;
            cardModel = 0x01C8A400 + (digit << 10);
            xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
            xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
            nmlModelSetPlace((const Vector4 *)transformed);
            nmlModelSetTexture((const char *)0x01C990A0);
            nmlModelSetRenderLevel(2);
            nmlModelEntryCard((void *)cardModel);
        }

        cardModel = 0x01C8A400 + ((((s8)work->helpTopic + 1) % CARD_HELP_RADIX) << 10);
        translation[0] = 1.9564f;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture((const char *)0x01C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);

        translation[0] = -1.4184f;
        translation[1] = translation[2] = 0.0f;
        translation[3] = 1.0f;
        cardModel = 0x01CA1000;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        nmlModelSetTexture((const char *)0x01CB0000);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);

        translation[0] = 1.4184f;
        translation[1] = 1.832f;
        cardModel = 0x01CA1C00;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        nmlModelSetTexture((const char *)0x01CC0050);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);

        translation[0] = 1.4184f;
        translation[1] = -1.832f;
        cardModel = 0x01CA6000;
        xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
        nmlModelSetTexture((const char *)0x01CC0050);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)cardModel);
    } break;
    }
}

/*
 * CardHelpPageInit: clears the 256-byte help page history helpPage[] (+0x59c0..+0x5abf,
 * stored downwards) and then the history index activeHelpPage (+0x59be).
 */
void CardHelpPageInit(CardGameWork *work)
{
    int i;

    for (i = 255; i >= 0; i--) {
        work->helpPage[i] = 0;
    }
    work->activeHelpPage = 0;
}

#include "common.h"
#include "shared.h"
#include "sdv.h"

extern void RES_GetMdlFileName(char *destination, int modelId);


#include "main/xgl_cd.h"

#include "menu_1.h"
int MenuIdChange(short id, int highPart);


typedef struct MenuNumberDigits {
    const char *text[10];
} MenuNumberDigits;

extern const MenuNumberDigits D_004C5180;

extern const char D_004DAB88[];

extern const unsigned short D_004C51E8[12];

extern const unsigned short D_004C5200[12];

extern const unsigned short D_004C5218[16];

extern const unsigned short D_004C5238[12];

extern void (*f_11[])(int *first, int *second);

extern const unsigned char D_004C5438[9][2];

extern const char *f_base_14[];

extern const char *f_exten_15[];

extern const char *f_ukn_name_16[];

extern const char *f_char_mot_17[];

extern char *f_mons_mot_18[];

static char f_name_0_0036D6C8[21] = "data\\endou\\mapex.bin";

static char f_name_1[23] = "data\\endou\\savemap.bin";

int map_ex_text = 0;

int save_map_text = 0;

/* MenuNumberTextGet's same-TU assembly writes and returns this buffer. */

static char msg_2_00532A78[0x18];

/* MenuFileNameGet builds names into this same-TU filename buffer. */

static char fname_13[0x80];

static int sort[2][0x100];

static MenuListEntry mw_list[2][256];

char D_004C51D8[16] = "Combatant";

char D_004DABB0[8] = "Reserve";

char D_004C51C0[24] = "Registered Combatant";

char D_004C51A8[24] = "Registered Reserve";

char D_004DABA8[8] = "User";

char D_004DABA0[8] = "From";

char D_004DAB98[8] = "Target";

char D_004DAB90[8] = "To";

char D_004DABC8[8] = "HP";

char D_004DABC0[8] = "EP";

char D_004C5290[16] = "Strength";

char D_004C5280[16] = "Vitality";

char D_004C5270[16] = "Ether Attack";

char D_004C5260[16] = "Ether Defense";

char D_004C5250[16] = "Dexterity";

char D_004DABB8[8] = "Evasion";

char D_004DABF8[8] = "STR";

char D_004DABF0[8] = "VIT";

char D_004DABE8[8] = "EATK";

char D_004DABE0[8] = "EDEF";

char D_004DABD8[8] = "DEX";

char D_004DABD0[8] = "EVA";

static char *tag_name_3[9] = {
    0, D_004C51D8, D_004DABB0, D_004C51C0, D_004C51A8,
    D_004DABA8, D_004DABA0, D_004DAB98, D_004DAB90
};

static char *para_name_4[8] = {
    D_004DABC8, D_004DABC0, D_004C5290, D_004C5280,
    D_004C5270, D_004C5260, D_004C5250, D_004DABB8
};

static char *para_name2_5[8] = {
    D_004DABC8, D_004DABC0, D_004DABF8, D_004DABF0,
    D_004DABE8, D_004DABE0, D_004DABD8, D_004DABD0
};

static unsigned short opt_mnt_tbl[8] = {
    0x20, 0x21, 0x22, 0x23, 0x18, 0x19, 0x1A, 0x1B
};

static char msg_12[9] = "No data.";

unsigned char ParaDataChangeTbl[8] __attribute__((section(".data"))) = {
    6, 7, 0, 1, 2, 3, 4, 5
};

/* OptMntToPosEquip2 maps the mount code through this asm sibling first. */

int OptMntToPosEquip(int mountCode);

extern unsigned char SaveData[];

int MenuScenarioNoGet(void);

/*
 * dataEvtBoxChk is defined in src/main/data.c and xglFlagsGet in
 * src/main/xgl_flags.c; neither is published in include/main/ yet (only
 * xglFlagsInitial is), so both are forward-declared here like
 * MenuScenarioNoGet above and OptMntToPosEquip at the top of this file.
 */

int dataEvtBoxChk(int eventId);

int xglFlagsGet(int bitOffset, int bitCount);

/* MenuLoadFile is defined in src/main/window_tex_load.c. */

extern void MenuLoadFile(const char *name, void *buffer);

/* MenuLoadFile is defined in src/main/window_tex_load.c. */

typedef struct WeaponStatus WeaponStatus;

/*
 * dataWpnGet (ov01/data_unit_org_get.c, still asm) returns weapon weaponId's
 * status record (it rejects ids <= 0); MenuBulletCheck only reads the flag
 * halfword at +0x6, whose bit 0x100 marks a weapon that needs bullets.
 */

struct WeaponStatus {
    unsigned char unmodeled_00[6];
    unsigned short flags; /* +0x6: bit 0x100 - weapon needs bullets */
    unsigned short characterEquipMask; /* +0x8 */
    unsigned char unmodeled_0a[2];
    short weaponClassMask; /* +0xc */
};

extern WeaponStatus *dataWpnGet(int weaponId);

typedef struct BulletData BulletData;

/*
 * dataAttGet (same TU, still asm) indexes the bullet/attachment table by
 * ammoId; only the owning weapon id short at +0xC is read here.
 */

struct BulletData {
    unsigned char unmodeled_00[0xC];
    short weaponId; /* +0xC: the weapon this ammunition belongs to */
};

extern BulletData *dataAttGet(int ammoId);

typedef struct PartEquipData PartEquipData;

/*
 * dataFrmGet and dataEngGet (ov01/data_unit_org_get.c, still asm) return
 * per-entry AGWS part records that share this layout as far as
 * MenuFrameEquipCheck/MenuEngineEquipCheck read it: a halfword at +0x6
 * bitmask of the characters that can equip the part, tested against
 * chrEquipGet's per-character bit.
 */

struct PartEquipData {
    unsigned char price[4];
    unsigned char unmodeled_04[2];
    unsigned short equipMask; /* +0x6 */
    unsigned char unmodeled_08[2];
    unsigned short category; /* +0xa */
    unsigned char unmodeled_0c[2];
    unsigned short associatedId; /* +0xe */
};

extern PartEquipData *dataFrmGet(int frameId);

extern PartEquipData *dataEngGet(int engineId);

/*
 * dataUnitOrgGet's own 0x180-byte per-character record (ov01 VA 0x00a191c0,
 * cross-overlay call under the scaffold label func_A191C0, matching
 * src/main/window_tex_load.c and src/main/menu_para_pt_rate_get.c; not in
 * this image). The three equipped-weapon id halfwords at +0x5E immediately
 * precede the accessory id halfwords src/main/menu_para_pt_rate_get.c models
 * at +0x64 as CharParaData's accessory[3]; only that weapon array is read
 * here.
 */

typedef struct {
    unsigned char unmodeled_00[0x54];
    short agwsId; /* +0x54 */
    unsigned char unmodeled_056[0x5a - 0x56];
    signed char weaponMount[3]; /* +0x5a */
    unsigned char unmodeled_05d[0x5e - 0x5d];
    short weapon[3]; /* +0x5E */
    short accessoryId[3]; /* +0x64 */
    unsigned char unmodeled_06a[0x8e - 0x6a];
    short etherId[12]; /* +0x8e */
    short skillId[3]; /* +0xa6 */
} UnitOrgWeapons;

extern UnitOrgWeapons *func_A191C0(int chrNo);

/*
 * dataWpnGet's own record (ov01 VA 0x00a1a3d8). MenuBulletCheck above models
 * its +0x6 flags halfword as WeaponStatus, already accepted and unable to
 * grow, so this call site keeps its own scaffold label func_A1A3D8 (matching
 * src/main/menu_shop.c) for the different +0x12 halfword summed here.
 */

typedef struct {
    unsigned char unmodeled_000[2];
    unsigned char price[4];
    unsigned char unmodeled_006[2];
    unsigned short characterEquipMask; /* +0x8 */
    unsigned char unmodeled_00a[0xc - 0xa];
    unsigned short categoryMask; /* +0xc */
    unsigned char mountMask; /* +0xe */
    unsigned char unmodeled_00f[0x11 - 0xf];
    unsigned char equipWeight; /* +0x11 */
    short wagl; /* +0x12 */
} WeaponWaglData;

extern WeaponWaglData *func_A1A3D8(int weaponId);


#include "main/party.h"

struct MenuScenarioRange {
    int scenario;
    int flag;
    int minimumFlag;
};

struct MenuScenarioRangeSet {
    struct MenuScenarioRange ranges[5];
};

struct MenuScenarioTable {
    struct MenuScenarioRangeSet rangeSet;
    unsigned char unmodeled_3c[4];
};

const struct MenuScenarioTable D_004C5140[1] = {
    {{{{500, 500, 500}, {400, 400, 399}, {399, 399, 300},
       {197, 197, 100}, {56, 56, 1}}}, 0}
};

extern const char D_004DACC0[16];

extern short func_A197E8(int characterId, int specialId);

extern MenuTextEntry *func_A2C5A8(unsigned int itemId);

extern MenuTextEntry *func_A2C5F8(unsigned int itemId);

extern MenuTextEntry *func_A2C648(unsigned int itemId);

extern MenuTextEntry *func_A2C698(unsigned int itemId);

extern MenuTextEntry *func_A2C6E8(unsigned int itemId);

extern MenuTextEntry *func_A2C738(unsigned int itemId);

extern MenuTextEntry *func_A2C7D8(unsigned int itemId);

extern MenuTextEntry *func_A2C918(unsigned int itemId);

extern MenuTextEntry *func_A2C968(unsigned int itemId);

extern char *dataEvtItmNameGet(unsigned int itemId);

char *MenuCharNameGet(unsigned int charId);

int dataItmBoxChk(int id);

int dataWpnBoxChk(int id);

int dataBltBoxChk(int id);

int dataAccBoxChk(int id);

int dataItmBoxInc(int id);

int dataEvtBoxInc(int id);

int dataWpnBoxInc(int id);

int dataBltBoxInc(int id);

int dataAccBoxInc(int id);

void dataItmBoxDec(int id);

void dataEvtBoxDec(int id);

void dataWpnBoxDec(int id);

void dataBltBoxDec(int id);

void dataAccBoxDec(int id);

void PartyTakeAgwsOn(int id);

/* dataUnitOrgGet's per-character record fields used by this TU are at
 * +0x5A, +0x5E, +0x64, and +0xA6. */

extern PartEquipData *func_A1A548(int accessoryId);

/* MenuNumberTextGet's same-TU assembly writes and returns this buffer. */

/* MenuFileNameGet builds names into this same-TU filename buffer. */

static MenuTextEntry dumm_msg_8 = {
    D_004DACC0,
    (const unsigned char *)(D_004DACC0 + 8),
    (const unsigned char *)(D_004DACC0 + 8)
};

static MenuTextEntry dumm_9 = {
    0,
    (const unsigned char *)(D_004DACC0 + 8),
    (const unsigned char *)(D_004DACC0 + 8)
};

static MenuTextEntry dumm2_10 = {0, 0, 0};

extern const int D_004C5418[8];

struct MenuPriceRecord {
    unsigned char price[4];
};

struct MenuBulletPriceRecord {
    unsigned char unmodeled_00[6];
    unsigned char price[4]; /* +0x6 */
    unsigned char unmodeled_0a[2];
    short weaponId;
};

extern struct MenuPriceRecord *func_A1A4E8(int itemId);

extern struct MenuBulletPriceRecord *func_A1A428(int bulletId);

extern struct MenuPriceRecord *func_A1A598(int frameId);

extern struct MenuPriceRecord *func_A1A5E8(int engineId);

extern const int D_004C5388[3];

extern void *memset(void *destination, int value, unsigned int bytes);

extern int PartyAllPartyGet(int *party, int selection);

unsigned char *MenuSaveDataGet(void)
{
    return SaveData + 0x10254;
}

int MenuShionMwsCheck(void)
{
    if (MenuScenarioNoGet() < 0xA) {
        return 1;
    }
    if (xglFlagsGet(0x3FF, 1) != 0) {
        return 1;
    }
    return dataEvtBoxChk(0xA) != 0;
}

int MenuEquipStealMaskCheck(void)
{
    return xglFlagsGet(0x3F9, 1) != 0;
}

int MenuScenarioNoGet(void)
{
    struct MenuScenarioRangeSet ranges = D_004C5140[0].rangeSet;
    int scenario = 0;
    int group = 0;

    do {
        int flag = ranges.ranges[group].flag;

        scenario = ranges.ranges[group].scenario;
        if (flag >= ranges.ranges[group].minimumFlag) {
            do {
                if (xglFlagsGet(flag, 1) != 0) {
                    return scenario;
                }
                flag--;
                scenario--;
            } while (flag >= ranges.ranges[group].minimumFlag);
        }
        group++;
    } while (group < 5);
    return scenario;
}

int MenuCursorKeepCheck(void)
{
    return SaveData[0x44] == 0;
}

char *MenuSegmentInfoTextGet(int segment)
{
    unsigned char *text = (unsigned char *)map_ex_text;

    while (segment >= 2) {
        while (*text++ != 0) {
        }
        segment--;
    }
    return (char *)text;
}

char *MenuSegmentMapNameGet(int segment)
{
    unsigned char *text = (unsigned char *)map_ex_text + 0x540;

    while (segment >= 2) {
        while (*text++ != 0) {
        }
        segment--;
    }

    return (char *)text;
}

char *MenuSegmentItemNameGet(int segment)
{
    unsigned char *text = (unsigned char *)map_ex_text + 0x3C0;

    while (segment >= 2) {
        while (*text++ != 0) {
        }
        segment--;
    }

    return (char *)text;
}

int MenuMapExTextLoad(int work, int mode)
{
    int aligned = (work + 0xF) & ~0xF;
    int next = aligned + 0x2000;

    map_ex_text = aligned;
    if (mode == 0) {
        xglCdReadFile(f_name_0_0036D6C8, (void *)aligned, 0, 1);
    } else {
        MenuLoadFile(f_name_0_0036D6C8, (void *)aligned);
    }
    return next;
}

const unsigned char *MenuSaveMapNameGet(int mapId)
{
    typedef struct {
        unsigned char names[2][0x21];
        short map_id;
        unsigned char unknown[2];
    } SaveMapNameEntry;

    SaveMapNameEntry *entry = (SaveMapNameEntry *)save_map_text;
    int count = 0;
    SaveMapNameEntry *first = entry;

    do {
        if (entry->map_id == mapId) {
            return entry->names[0];
        }
        entry++;
        count++;
    } while (count < 0x25);

    return first->names[0];
}

int MenuSaveMapTextLoad(int work, int mode)
{
    int aligned = (work + 0xF) & ~0xF;
    int next = aligned + 0x1000;

    save_map_text = aligned;
    if (mode == 0) {
        xglCdReadFile(f_name_1, (void *)aligned, 0, 1);
    } else {
        MenuLoadFile(f_name_1, (void *)aligned);
    }
    return next;
}

extern struct MenuCharParameters *func_00A11108(int charId, int *attack, int *defense);

int MenuCharHpCheck(int charId)
{
    int attack[4];
    int defense[4];
    int party[16];
    int index;
    int count;
    struct MenuCharParameters *parameters;

    if (charId == 0) {
        return 0;
    }
    if (charId < 0) {
        count = PartyAllPartyGet(party, 0);
        for (index = 0; index < count; index++) {
            parameters = func_00A11108(party[index], attack, defense);
            if (parameters->hp != parameters->maxHp) {
                break;
            }
        }
        if (index == count) {
            return 1;
        }
    } else {
        func_A191C0(charId);
        parameters = func_00A11108(charId, attack, defense);
        if (parameters->hp == parameters->maxHp) {
            return 1;
        }
    }
    return 0;
}

int MenuCharEpCheck(int charId)
{
    int attack[4];
    int defense[4];
    int party[16];
    int index;
    int count;
    struct MenuCharParameters *parameters;

    if (charId == 0) {
        return 0;
    }
    if (charId < 0) {
        count = PartyAllPartyGet(party, 0);
        for (index = 0; index < count; index++) {
            parameters = func_00A11108(party[index], attack, defense);
            if (parameters->ep != parameters->maxEp) {
                break;
            }
        }
        if (index == count) {
            return 1;
        }
    } else {
        parameters = func_00A11108(charId, attack, defense);
        if (parameters->ep == parameters->maxEp) {
            return 1;
        }
    }
    return 0;
}

int MenuMaryIdChange(int characterId)
{
    if (characterId == 20) {
        return 11;
    }
    if (characterId == 19) {
        return 12;
    }
    if (characterId < 17) {
        return characterId;
    }
    return 3;
}

int MenuMainCharCheck(int charId)
{
    return (unsigned int)(charId - 1) < 7U;
}

int MenuMainAgwsCheck(int charId)
{
    unsigned int isMainAgws;

    isMainAgws = (unsigned int)(charId - 0x13) < 2U;
    if (isMainAgws) {
        return 0;
    }
    return 1;
}

char *MenuNumberTextGet(long long number, int digitsBeforeUnits, int leadingZeroes)
{
    MenuNumberDigits digits = D_004C5180;
    long long divisor;
    long long remaining;
    int digitCount;
    int places;
    int started;

    msg_2_00532A78[0] = D_004DAB88[0];
    if (digitsBeforeUnits < 0) {
        divisor = 10;
        digitCount = 1;
        while (number / divisor != 0) {
            divisor *= 10;
            digitCount++;
        }
        digitsBeforeUnits = digitCount;
    }
    if (digitsBeforeUnits < 11) {
        places = digitsBeforeUnits;
    } else {
        places = 10;
    }
    started = leadingZeroes == 1;
    remaining = number;
    while (places > 0) {
        int exponent;
        divisor = 10;
        for (exponent = 1; exponent < places; exponent++) {
            divisor *= 10;
        }
        if (started || remaining / divisor != 0) {
            started = 1;
            strcat(msg_2_00532A78, digits.text[(int)(remaining / divisor)]);
        }
        places--;
        remaining %= divisor;
    }
    strcat(msg_2_00532A78, digits.text[(int)(remaining % 10)]);
    return msg_2_00532A78;
}

char *MenuTagTextGet(int index)
{
    return tag_name_3[index];
}

unsigned short MenuFaceEpidGet(int charId, int smallPortrait)
{
    unsigned short largeFaces[12];
    unsigned short largeAgwsFaces[12];
    unsigned short smallFaces[16];
    unsigned short smallAgwsFaces[12];

    if (charId == 2) {
        unsigned short epid = 0x401;
        if (smallPortrait == 0) {
            epid = 0xC01;
        }
        return epid;
    }
    if (smallPortrait == 0) {
        if ((unsigned int)(charId - 1) < 12U) {
            __builtin_memcpy(largeFaces, D_004C51E8, sizeof(largeFaces));
            return largeFaces[charId - 1];
        }
        if ((unsigned int)(charId - 17) < 12U) {
            __builtin_memcpy(largeAgwsFaces, D_004C5200, sizeof(largeAgwsFaces));
            return largeAgwsFaces[charId - 17];
        }
    } else {
        if ((unsigned int)(charId - 1) < 12U) {
            __builtin_memcpy(smallFaces, D_004C5218, sizeof(smallFaces));
            return smallFaces[charId - 1];
        }
        __builtin_memcpy(smallAgwsFaces, D_004C5238, sizeof(smallAgwsFaces));
        return smallAgwsFaces[charId - 17];
    }
    return 0;
}

char *MenuParaNameGet(int index)
{
    return para_name_4[index];
}

char *MenuParaNameGet2(int index)
{
    return para_name2_5[index];
}

extern char *char_name_6[30];
extern char *char_name2_7[];

char *MenuCharNameGet(unsigned int charId)
{
    char *name = char_name_6[charId];

    if (charId == 5) {
        if (MenuScenarioNoGet() < 108) {
            name = char_name2_7[0];
        }
    }
    return name;
}

MenuTextEntry *MenuTextGet(int index)
{
    unsigned int encodedIndex = (unsigned int)index;
    unsigned int itemId = encodedIndex & 0xFFFF;
    unsigned int highIndex = encodedIndex >> 16;
    MenuTextEntry *result = 0;

    if (itemId == 0) {
        return &dumm_msg_8;
    } else {
        unsigned int textType = highIndex - 1;

        switch (textType) {
        case 0:
            result = func_A2C5F8(itemId);
            break;
        case 1: {
            const char *itemName = dataEvtItmNameGet(itemId);
            const unsigned char *end = (const unsigned char *)itemName;

            dumm2_10.name = itemName;
            dumm2_10.sortName = (const unsigned char *)itemName;
            while (*end++ != '\0') {
            }
            dumm2_10.auxiliary = end;
            result = &dumm2_10;
            break;
        }
        case 2:
            result = &dumm_msg_8;
            if (MenuEquipStealMaskCheck() == 0) {
                if (itemId == 0x45) {
                    if (MenuShionMwsCheck() != 0) {
                        result = func_A2C6E8(0x45);
                    }
                } else {
                    result = func_A2C6E8(itemId);
                }
            }
            break;
        case 3:
            if (MenuEquipStealMaskCheck() != 0) {
                result = &dumm_msg_8;
            } else {
                result = func_A2C738(itemId);
            }
            break;
        case 4:
            if (MenuEquipStealMaskCheck() == 0) {
                result = func_A2C698(itemId);
            } else {
                result = &dumm_msg_8;
            }
            break;
        case 5:
        case 6:
            dumm_9.name = MenuCharNameGet(itemId);
            result = &dumm_9;
            break;
        case 7:
            result = func_A2C648(itemId);
            break;
        case 8:
            result = func_A2C5A8(itemId);
            break;
        case 9:
            result = func_A2C7D8(itemId);
            break;
        case 10:
            result = func_A2C968(itemId);
            break;
        case 11:
            result = func_A2C918(itemId);
            break;
        default:
            break;
        }
    }
    return result;
}

int MenuBoxChk(int index)
{
    unsigned int category = (unsigned int)index >> 16;
    int id = index & 0xFFFF;
    int result = 0;

    switch (category) {
    case 1:
        result = dataItmBoxChk(id);
        break;
    case 2:
        result = dataEvtBoxChk(id);
        break;
    case 3:
        result = dataWpnBoxChk(id);
        break;
    case 4:
        result = dataBltBoxChk(id);
        break;
    case 5:
        result = dataAccBoxChk(id);
        break;
    case 6:
        result = 0;
        break;
    }
    return result;
}

int MenuBoxInc(unsigned int index)
{
    unsigned int category = index >> 16;
    int id = index & 0xFFFF;
    int result = 0;

    switch (category) {
    case 1:
        dataItmBoxInc(id);
        break;
    case 2:
        dataEvtBoxInc(id);
        break;
    case 3:
        dataWpnBoxInc(id);
        break;
    case 4:
        dataBltBoxInc(id);
        break;
    case 5:
        dataAccBoxInc(id);
        break;
    case 6:
        result = -1;
        break;
    case 7:
        PartyTakeAgwsOn(id);
        break;
    }
    return result;
}

int MenuBoxDec(unsigned int index)
{
    unsigned int category = index >> 16;
    int id = index & 0xFFFF;
    int result = 0;

    switch (category) {
    case 1:
        dataItmBoxDec(id);
        break;
    case 2:
        dataEvtBoxDec(id);
        break;
    case 3:
        dataWpnBoxDec(id);
        break;
    case 4:
        dataBltBoxDec(id);
        break;
    case 5:
        dataAccBoxDec(id);
        break;
    case 6:
        result = -1;
        break;
    }
    return result;
}

int MenuBoxMoneyGet(int index, int priceMode)
{
    float rate[5];
    int priceValues[3];
    int agwsPrices[12];
    int itemId = index & 0xffff;
    int price = 0;
    int category = (unsigned int)index >> 16;
    int rateIndex;
    short weaponId;

    if (itemId == 0) {
        return 0;
    }
    if (priceMode == 0) {
        const unsigned char *discountLevel = &SaveData[0x102dc];
        for (rateIndex = 0; rateIndex < 5; rateIndex++) {
            rate[rateIndex] = 1.0f - discountLevel[rateIndex] * 0.1f;
            if (rate[rateIndex] < 0.5f) {
                rate[rateIndex] = 0.5f;
            }
        }
    } else {
        for (rateIndex = 4; rateIndex >= 0; rateIndex--) {
            rate[rateIndex] = 1.0f;
        }
    }
    if ((unsigned int)(category - 1) < 12U) {
    switch (category) {
    case 1:
        {
            int rawPrice = ((BattleItemPriceRecord *)func_A1A4E8(itemId))->price.value;
            price = (int)(rawPrice * rate[0]);
        }
        break;
    case 2:
        if ((unsigned int)(itemId - 53) < 3U) {
            __builtin_memcpy(priceValues, D_004C5388, sizeof(priceValues));
            price = priceValues[itemId - 53];
        }
        break;
    case 3:
        price = ((BattleWeaponPriceRecord *)func_A1A3D8(itemId))->price.value;
        if (func_A1A3D8(itemId)->categoryMask & 0x4000) {
            price = (int)(price * rate[4]);
        } else {
            price = (int)(price * rate[2]);
        }
        break;
    case 4:
        price = ((BattleBulletPriceRecord *)func_A1A428(itemId))->price.value;
        weaponId = func_A1A428(itemId)->weaponId;
        if (weaponId != 0) {
            if (func_A1A3D8(weaponId)->categoryMask & 0x4000) {
                price = (int)(price * rate[4]);
            } else {
                price = (int)(price * rate[2]);
            }
        }
        break;
    case 5:
        price = ((BattleAccessoryPriceRecord *)func_A1A548(itemId))->price.value;
        if (itemId < 129) {
            price = (int)(price * rate[4]);
        } else {
            price = (int)(price * rate[3]);
        }
        break;
    case 11:
        price = ((BattleFramePriceRecord *)func_A1A598(itemId))->price.value;
        break;
    case 12:
        price = ((BattleEnginePriceRecord *)func_A1A5E8(itemId))->price.value;
        break;
    case 7:
        memset(agwsPrices, 0, sizeof(agwsPrices));
        agwsPrices[5] = 50000;
        agwsPrices[9] = 80000;
        agwsPrices[10] = 300000;
        price = (int)(agwsPrices[itemId - 17] * rate[1]);
        break;
    }
    }
    if (priceMode == 1) {
        price = (int)(price * 0.5f);
    }
    return price;
}

int MenuSkillEquipCheck(short charId, short skillId)
{
    UnitOrgWeapons *unitWeapons = func_A191C0(charId);
    int slot;

    for (slot = 0; slot < 3; slot++) {
        if (unitWeapons->skillId[slot] == skillId) {
            return slot + 1;
        }
    }
    return 0;
}

void MenuSkillEquip(short charId, short skillId, int slot)
{
    UnitOrgWeapons *unitWeapons = func_A191C0(charId);

    if (slot >= 0) {
        unitWeapons->skillId[slot] = skillId;
    } else {
        int index;

        for (index = 0; index < 3; index++) {
            if (unitWeapons->skillId[index] != 0) {
                continue;
            }
            unitWeapons->skillId[index] = skillId;
            break;
        }
    }
}

int MenuRWeaponCheck(int weaponId, long equipType)
{
    if (equipType < 0x24) {
        if (0x1F < equipType) {
            switch (weaponId) {
            case 0x5B:
                weaponId = 0x70;
                break;
            case 0x5C:
                weaponId = 0x71;
                break;
            case 0x5E:
                weaponId = 0x72;
                break;
            }
        }
    }
    return weaponId;
}

int MenuRWeaponCheck2(int weaponId)
{
    if (weaponId != 0x71) {
        if (weaponId < 0x72) {
            if (weaponId == 0x70) {
                weaponId = 0x5B;
            }
        } else if (weaponId == 0x72) {
            weaponId = 0x5E;
        }
    } else {
        weaponId = 0x5C;
    }
    return weaponId;
}

unsigned char PosEquipToOptMnt(int posEquip)
{
    int bit;

    for (bit = 0; bit < 8; bit++) {
        if (((posEquip & 0xFF) >> bit) & 1) {
            return opt_mnt_tbl[bit];
        }
    }
    return 0;
}

int OptMntToPosEquip(int mountCode)
{
    int position = 0;
    int equipmentPosition;

    mountCode &= 0xff;
    if (opt_mnt_tbl[0] == mountCode)
        return 1;
    goto advance_position;
    do {
advance_position:
        position++;
        if (position >= 8)
            goto no_mount;
    } while (opt_mnt_tbl[position] != mountCode);
    equipmentPosition = (1 << position) & 0xff;
    goto return_equipment_mask;
no_mount:
    equipmentPosition = 0;
return_equipment_mask:
    return equipmentPosition;
}

int OptMntToPosEquip2(int mountCode)
{
    int posEquip = OptMntToPosEquip(mountCode & 0xFF);

    switch (posEquip) {
    case 1:
        posEquip = 3;
        break;
    case 2:
        posEquip = 3;
        break;
    case 16:
    case 32:
        posEquip = 0x30;
        break;
    }

    return posEquip;
}

int OptMntToWpnPos(int mountCode)
{
    int weaponPos = 0;

    switch (mountCode) {
    case 32:
    case 33:
        weaponPos = 1;
        break;
    case 24:
    case 25:
        weaponPos = 2;
        break;
    case 34:
        weaponPos = 3;
        break;
    case 26:
        weaponPos = 4;
        break;
    case 35:
        weaponPos = 5;
        break;
    case 27:
        weaponPos = 6;
        break;
    }
    return weaponPos;
}

int WpnPosToOptMnt(int mountId, int weaponId)
{
    WeaponWaglData *weapon = 0;
    int result = 0;

    if (weaponId != 0) {
        weapon = func_A1A3D8(weaponId);
    }

    switch (mountId) {
    case 1:
        if (weapon != 0) {
            result = (weapon->mountMask & 1) != 0 ? 0x20 : 0x21;
        }
        break;
    case 2:
        if (weapon != 0) {
            result = (weapon->mountMask & 0x10) != 0 ? 0x18 : 0x19;
        }
        break;
    case 3:
        result = 0x22;
        break;
    case 4:
        result = 0x1A;
        break;
    case 5:
        result = 0x23;
        break;
    case 6:
        result = 0x1B;
        break;
    }

    return result;
}

int MenuWpnMountIdChange(int unused, int mountIndex)
{
    int mountIds[8];

    __builtin_memcpy(mountIds, D_004C5418, sizeof(mountIds));
    (void)unused;
    if (mountIndex < 2) {
        return mountIds[mountIndex + 5];
    }
    return mountIds[mountIndex - 1];
}

int PosEquipToMntPos(void)
{
    return 0;
}

const int D_004C5418[8] = {32, 24, 34, 26, 35, 27, 33, 25};

static unsigned short chrEquipGet(int charId)
{
    if (charId < 0x11) {
        return 0x8000 >> (charId - 1);
    }
    return 0x8000 >> (charId - 0x11);
}

static int wpnInfoGet(int weaponType)
{
    return (weaponType < 0x11) ? 0x4000 : 0x2000;
}

int MenuWeaponEquipCheck(int charId, int weaponId)
{
    unsigned short characterMask;
    int weaponClassMask;
    WeaponStatus *weapon;

    if (charId == 0) {
        return 0;
    }
    characterMask = chrEquipGet(charId);
    weaponClassMask = wpnInfoGet(charId);
    if (weaponId == 0) {
        return 0;
    }

    weapon = dataWpnGet(weaponId);
    if ((weapon->weaponClassMask & weaponClassMask) == 0) {
        return 0;
    }
    return (weapon->characterEquipMask & characterMask) != 0;
}

int MenuWeaponEquipPosCheck(int charId, int weaponId, int weaponPosition, int replacementSlot)
{
    UnitOrgWeapons *character;
    WeaponWaglData *weapon;
    unsigned char occupiedPositions = 0;
    unsigned short blockedPositions = 0;
    int weight;
    int slot;
    int equipmentIndex;
    int requestedPosition;
    int supportedPositions;
    int result = 0;
    int capacity;
    int mountCode;
    int bitIndex;
    int bit;
    unsigned char alternatives;
    unsigned char mountMask;

    character = func_A191C0(charId);
    if ((unsigned int)(weaponId - 1) >= 115U) {
        return 0;
    }
    weapon = func_A1A3D8(weaponId);
    supportedPositions = weapon->mountMask;
    if (!(weapon->characterEquipMask & chrEquipGet(charId))) {
        return 0;
    }
    capacity = 3;
    if (charId == 0x1b) {
        capacity = 2;
    }
    weight = 0;
    for (equipmentIndex = 0; equipmentIndex < 3; equipmentIndex++) {
        if (character->weapon[equipmentIndex] != 0 && equipmentIndex != replacementSlot) {
            weight += func_A1A3D8(character->weapon[equipmentIndex])->equipWeight;
        }
    }
    if (capacity < weight + func_A1A3D8(weaponId)->equipWeight) {
        return 0;
    }
    requestedPosition = OptMntToPosEquip(WpnPosToOptMnt(weaponPosition, weaponId) & 0xff);
    OptMntToPosEquip2(WpnPosToOptMnt(weaponPosition, weaponId) & 0xff);
    for (equipmentIndex = 0; equipmentIndex < 3; equipmentIndex++) {
        if (equipmentIndex != replacementSlot) {
            occupiedPositions = (occupiedPositions | OptMntToPosEquip(character->weaponMount[equipmentIndex] & 0xff)) & 0xff;
        }
    }
    for (equipmentIndex = 0; equipmentIndex < 8; equipmentIndex++) {
        if ((occupiedPositions >> equipmentIndex) & 1) {
            mountCode = PosEquipToOptMnt((1 << equipmentIndex) & 0xff);
            for (slot = 0; slot < 3; slot++) {
                if (character->weaponMount[slot] == mountCode && character->weapon[slot] != 0) {
                    int existingBit;

                    weapon = func_A1A3D8(character->weapon[slot]);
                    mountMask = weapon->mountMask;
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        unsigned short alternatives;
                        existingBit = 0x80 >> bitIndex;
                        alternatives = mountMask & ~existingBit & 0xf0;
                        if (weapon->mountMask & existingBit) {
                            if (alternatives & 0x10) {
                                alternatives |= 0x20;
                            }
                            if (alternatives & 0x20) {
                                alternatives |= 0x10;
                            }
                            blockedPositions |= alternatives;
                        }
                    }
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        unsigned char lowMountMask = weapon->mountMask;
                        existingBit = 8 >> bitIndex;
                        alternatives = mountMask & ~existingBit & 0x0f;
                        if (lowMountMask & existingBit) {
                            if (alternatives & 1) {
                                alternatives |= 2;
                            }
                            if (alternatives & 2) {
                                alternatives |= 1;
                            }
                            blockedPositions |= alternatives;
                        }
                    }
                }
            }
            for (slot = 0; slot < 3; slot++) {
                if (character->weapon[slot] == 0x4f || character->weapon[slot] == 0x54) {
                    blockedPositions |= 0x30;
                }
            }
        }
    }
    weapon = func_A1A3D8(weaponId);
    mountMask = weapon->mountMask;
    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
        unsigned short alternatives;
        bit = 0x80 >> bitIndex;
        alternatives = mountMask & ~bit & 0xf0;
        if (weapon->mountMask & bit) {
            if (alternatives & 0x10) {
                alternatives |= 0x20;
            }
            if (alternatives & 0x20) {
                alternatives |= 0x10;
            }
            if (occupiedPositions & alternatives) {
                blockedPositions = (blockedPositions | bit) & 0xff;
            }
        }
    }
    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
        unsigned char lowMountMask = weapon->mountMask;
        bit = 8 >> bitIndex;
        alternatives = mountMask & ~bit & 0x0f;
        if (lowMountMask & bit) {
            if (alternatives & 1) {
                alternatives |= 2;
            }
            if (alternatives & 2) {
                alternatives |= 1;
            }
            if (occupiedPositions & alternatives) {
                blockedPositions = (blockedPositions | bit) & 0xff;
            }
        }
    }
    if ((weaponId == 0x4f || weaponId == 0x54) && (occupiedPositions & 0x30)) {
        blockedPositions |= 3;
    }
    if ((requestedPosition & supportedPositions) && (requestedPosition & ~blockedPositions)) {
        result = WpnPosToOptMnt(weaponPosition, weaponId) & 0xff;
    }
    return result;
}

int MenuAccessoryEquipCheck(int charId, int accessoryId, int slotIndex)
{
    unsigned short characterMask;
    UnitOrgWeapons *characterWeapons;
    PartEquipData *accessory;
    unsigned short categoryToScan;
    int index;

    if (charId == 0) {
        return 0;
    }

    characterMask = chrEquipGet(charId);
    characterWeapons = func_A191C0(charId);

    if (accessoryId == 0) {
        return 0;
    }

    accessory = func_A1A548(accessoryId);
    if ((accessory->equipMask & characterMask) == 0) {
        return 0;
    }

    if (slotIndex < 0) {
        return 1;
    }

    if (characterWeapons->accessoryId[slotIndex] != 0) {
        PartEquipData *equipped =
            func_A1A548(characterWeapons->accessoryId[slotIndex]);

        if (equipped->category == accessory->category) {
            if (equipped->category != 2) {
                return 1;
            }
            if (characterWeapons->accessoryId[slotIndex] == accessoryId) {
                return 1;
            }
        }
    }

    categoryToScan =
        charId < 0x11 ? accessory->category : 2;

    if (categoryToScan == 2) {
        for (index = 0; index < 3; index++) {
            if (characterWeapons->accessoryId[index] == accessoryId) {
                return 0;
            }
        }
    } else if (accessory->category != 2) {
        for (index = 0; index < 3; index++) {
            short equippedId = characterWeapons->accessoryId[index];

            if (equippedId != 0) {
                PartEquipData *equipped = func_A1A548(equippedId);

                if (equipped->category == accessory->category) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

int MenuBulletCheck(int weaponId, int ammoId)
{
    if (weaponId == 0) {
        return 0;
    }
    if ((dataWpnGet(weaponId)->flags & 0x100) == 0) {
        return 0;
    }
    if (ammoId < 0) {
        return 1;
    }
    if (ammoId == 0) {
        return 0;
    }
    return (dataAttGet(ammoId)->weaponId ^ weaponId) == 0 ? ammoId : 0;
}

int MenuBulletCheck2(int weaponId, int charNo)
{
    switch (charNo) {
    case 1:
    case 6:
        return 0;
    case 3:
        return func_A197E8(3, 0x3E) != 0;
    default:
        return MenuBulletCheck(weaponId, -1);
    }
}

int MenuFrameEquipCheck(int charId, int frameId)
{
    if (frameId == 0) {
        return 0;
    }
    return (dataFrmGet(frameId)->equipMask & chrEquipGet(charId)) != 0;
}

int MenuEngineEquipCheck(int charId, int engineId)
{
    if (engineId == 0) {
        return 0;
    }
    return (dataEngGet(engineId)->equipMask & chrEquipGet(charId)) != 0;
}

int MenuAgwsPilotCheck(int agwsId)
{
    int charId = 1;

    do {
        if (func_A191C0(charId)->agwsId == agwsId) {
            return charId;
        }
        charId++;
    } while (charId < 13);
    return 0;
}

int MenuAgwsWaglGet(int chrNo)
{
    UnitOrgWeapons *org;
    short weaponId;
    int result;
    int total;
    int slot;

    result = 0;
    total = 0;
    if (chrNo != 0) {
        org = func_A191C0(chrNo);
        for (slot = 0; slot < 3; slot++) {
            weaponId = org->weapon[slot];
            if (weaponId != 0) {
                total += func_A1A3D8(weaponId)->wagl;
            }
        }
        result = total;
    }
    return result;
}

static void MenuSortSubType05(int *first, int *second)
{
    const unsigned char *firstName = MenuTextGet(*first)->sortName;
    const unsigned char *secondName = MenuTextGet(*second)->sortName;
    unsigned char firstChar = *firstName++;

    for (;;) {
        unsigned char secondChar;

        secondChar = *secondName++;
        if (firstChar < secondChar) {
            int savedIndex = *first;
            *first = *second;
            *second = savedIndex;
            return;
        }
        if (firstChar != secondChar || firstChar == 0) {
            return;
        }
        firstChar = *firstName++;
    }
}

static void MenuSortSubType04(int *first, int *second)
{
    const unsigned char *firstName = MenuTextGet(*first)->sortName;
    const unsigned char *secondName = MenuTextGet(*second)->sortName;
    unsigned char firstChar;
    unsigned char secondChar;

    for (;;) {
        secondChar = *secondName++;
        firstChar = *firstName++;
        if (secondChar < firstChar) {
            int savedIndex = *first;
            *first = *second;
            *second = savedIndex;
            return;
        }
        if (firstChar != secondChar || firstChar == 0) {
            return;
        }
    }
}

static void MenuSortSubType03(int *first, int *second)
{
    int firstCount = MenuBoxChk(*first);

    if (MenuBoxChk(*second) < firstCount) {
        int savedIndex = *first;
        *first = *second;
        *second = savedIndex;
    }
}

static void MenuSortSubType02(int *first, int *second)
{
    int firstCount = MenuBoxChk(*first);

    if (firstCount < MenuBoxChk(*second)) {
        int savedIndex = *first;
        *first = *second;
        *second = savedIndex;
    }
}

static void MenuSortSubType01(unsigned int *first, unsigned int *second)
{
    unsigned int firstValue = *first;
    unsigned int secondValue = *second;

    if (firstValue < secondValue) {
        *first = secondValue;
        *second = firstValue;
    }
}

static void MenuSortSubType00(unsigned int *first, unsigned int *second)
{
    unsigned int firstValue = *first;
    unsigned int secondValue = *second;

    if (secondValue < firstValue) {
        *first = secondValue;
        *second = firstValue;
    }
}

void MenuSortChange(int bufferIndex, int sortMode)
{
    int *entries = sort[bufferIndex];
    int count = 0;
    void (*compare)(int *, int *) = f_11[sortMode];
    int *next = &entries[1];
    int entry;
    int first;
    int second;

    if (entries[0] != 0) {
        do {
            entry = *next++;
            count++;
        } while (entry != 0);
    }
    if (count != 0) {
        for (first = 0; first < count - 1; first++) {
            for (second = first + 1; second < count; second++) {
                compare(&entries[first], &entries[second]);
            }
        }
    }
}

int MenuSortCheck(int listIndex)
{
    int *row = sort[listIndex];
    int *entry = row + 1;
    int value;
    int count = 0;

    if (*row != 0) {
        do {
            value = *entry;
            entry++;
            count++;
        } while (value != 0);
    }
    return count;
}

void MenuSortSet(int bufferIndex, int categoryMask, int filter)
{
    struct MenuWeaponRangePair {
        unsigned char first;
        unsigned char last;
    } weaponRanges[9];
    int party[8];
    int *entry = sort[bufferIndex];
    int id;
    int first;
    int last;
    int range;
    int weaponCategory;
    int weaponPosition;
    int bitIndex;
    unsigned int mountBits;
    int availableMounts;
    int accessoryCategory;
    int requiresAssociatedId;
    int partyCount;
    int *partyMember;
    int currentMember;
    WeaponWaglData *weapon;
    PartEquipData *accessory;
    UnitOrgWeapons *character;
    short selectedCharId;
    short *etherId;
    int currentEtherId;

    memset(entry, 0, sizeof(sort[bufferIndex]));
    if (categoryMask & 1) {
        if (filter == -10) {
            for (id = 1; id < 73; id++) {
                *entry++ = MenuIdChange((short)id, 1);
            }
        } else {
            id = 1;
            goto inventory_bound;
            do {
                if (dataItmBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 1);
                }
                id++;
inventory_bound:
                ;
            } while (id < 73);
        }
    }
    if (categoryMask & 2) {
        first = 1;
        last = 254;
        if (filter == -1) {
            first = 53;
            last = 55;
        }
        for (id = first; id <= last; id++) {
            if (dataEvtBoxChk(id) > 0) {
                if (id != 10 || MenuEquipStealMaskCheck() != 1) {
                    *entry++ = (id & 0xffff) + 0x20000;
                }
            }
        }
    }
    if ((categoryMask & 4) && MenuEquipStealMaskCheck() == 0) {
        __builtin_memcpy(weaponRanges, D_004C5438, sizeof(weaponRanges));
        weaponPosition = 0;
        switch (filter) {
        case -2:
            range = 7;
            weaponCategory = 0x2000;
            break;
        case -1:
            weaponCategory = 0x4000;
            range = 8;
            break;
        default:
            if (filter & 0x2000) {
                weaponCategory = 0x2000;
                weaponPosition = filter & ~0x2000;
                range = 7;
            } else {
                weaponCategory = 0x4000;
                range = filter - 1;
            }
            break;
        }
        for (id = weaponRanges[range].first; id <= weaponRanges[range].last; id++) {
            if (dataWpnBoxChk(id) > 0) {
                weapon = func_A1A3D8(id);
                if (!(weapon->categoryMask & weaponCategory)) {
                    continue;
                }
                if (weaponPosition != 0) {
                    availableMounts = weapon->mountMask;
                    mountBits = 0x88;
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        if (availableMounts & mountBits) {
                            availableMounts &= mountBits;
                            break;
                        }
                        mountBits >>= 1;
                    }
                    if (!(availableMounts & OptMntToPosEquip2(WpnPosToOptMnt(weaponPosition, id) & 0xff))) {
                        continue;
                    }
                }
                *entry++ = MenuIdChange((short)id, 3);
            }
        }
    }
    if (categoryMask & 8) {
        id = 0;
        last = 0;
        if (MenuEquipStealMaskCheck() == 0) {
            switch (filter) {
            case 3: id = 1; last = 15; break;
            case 2: id = 16; last = 30; break;
            case 5: id = 31; last = 45; break;
            case 7: id = 46; last = 63; break;
            case -1: id = 1; last = 63; break;
            case -2: id = 64; last = 178; break;
            case -3: id = 1; last = 178; break;
            }
            for (; id <= last; id++) {
                if (dataBltBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 4);
                }
            }
        }
    }
    if ((categoryMask & 0x10) && MenuEquipStealMaskCheck() == 0) {
        accessoryCategory = filter;
        requiresAssociatedId = 0;
        if (filter > 0) {
            accessoryCategory = filter & 0xff;
            requiresAssociatedId = filter >> 8;
        }
        switch (accessoryCategory) {
        case -1:
            for (id = 1; id < 129; id++) {
                if (dataAccBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 5);
                }
            }
            break;
        case -2:
            for (id = 129; id < 181; id++) {
                if (dataAccBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 5);
                }
            }
            break;
        case 0:
        case 1:
        case 2:
            for (id = 1; id < 129; id++) {
                if (dataAccBoxChk(id) <= 0) {
                    continue;
                }
                accessory = func_A1A548(id);
                if (requiresAssociatedId != 0 && accessory->associatedId == 0) {
                    continue;
                }
                switch (accessoryCategory) {
                case 0:
                    if (accessory->category == 1) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                case 1:
                    if (accessory->category == 0) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                case 2:
                    if (accessory->category == 2) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                }
            }
            break;
        }
    }
    if ((categoryMask & 0x20) && filter == 1) {
        partyMember = party;
        partyCount = PartyAllPartyGet(partyMember, 1);
        id = 0;
        if (partyCount > 0) {
            do {
                currentMember = *partyMember;
                if (currentMember == 0) {
                    break;
                }
                if (currentMember != 2 && currentMember != 5) {
                    *entry++ = MenuIdChange((short)*partyMember, 6);
                }
                id++;
                partyMember++;
            } while (id < partyCount);
        }
    }
    if (categoryMask & 0x40) {
        selectedCharId = filter;
        for (id = 1; id < 91; id++) {
            if (func_A19698(selectedCharId, id) != 0) {
                *entry++ = (id & 0xffff) + 0x80000;
            }
        }
    }
    if (categoryMask & 0x80) {
        selectedCharId = filter;
        if (((unsigned int)filter >> 16) == 0) {
            for (id = 1; id < 80; id++) {
                if (func_A19578(selectedCharId, id) != 0) {
                    *entry++ = (id & 0xffff) + 0x90000;
                }
            }
        } else {
            id = 0;
            character = func_A191C0(selectedCharId);
            etherId = character->etherId;
            currentEtherId = *etherId;
            if (currentEtherId != 0) {
                currentEtherId = (currentEtherId & 0xffff) + 0x90000;
                do {
                    do {
                        *entry++ = currentEtherId;
                        ++id;
                        ++etherId;
                    } while (0);
                    if (id < 12) {
                        currentEtherId = *etherId;
                        if (currentEtherId == 0) {
                            break;
                        }
                        currentEtherId = (currentEtherId & 0xffff) + 0x90000;
                        continue;
                    }
                    break;
                } while (1);
            }
        }
    }
    *entry = 0;
}

void *MenuSortAddrGet(int listIndex)
{
    return sort[listIndex];
}

int MenuSortGet(int listIndex, int entryIndex)
{
    return sort[listIndex][entryIndex];
}

int MenuIdChange(short id, int highPart)
{
    return id + (highPart << 16);
}

void subListMake00(MenuListEntry *entry, int index)
{
    entry->name = MenuTextGet(index)->name;
    entry->amount = MenuBoxChk(index);
    entry->flag = 0;
}

void subListMake01(MenuListEntry *entry, int index)
{
    entry->name = MenuTextGet(index)->name;
    entry->amount = MenuBoxMoneyGet(index, 0);
    entry->flag = 0;
}

MenuListEntry *MenuListMake(int listIndex, int mode)
{
    MenuListEntry *list = mw_list[listIndex];
    MenuListEntry *entry = list;
    int *row = sort[listIndex];
    void (*fill)(MenuListEntry *, int);
    int index;
    int next;

    fill = (mode == -0xA) ? subListMake01 : subListMake00;

    index = *row;
    if (index != 0) {
        do {
            fill(entry, index);
            entry++;
            row++;
            next = *row;
            index = next;
        } while (next != 0);
    }
    entry->amount = -1;
    entry->name = msg_12;
    entry->flag = 0;
    entry[1].name = 0;
    return list;
}

MenuListEntry *MenuListGet(int listIndex)
{
    return mw_list[listIndex];
}

char *MenuFileNameGet(int id, unsigned int resourceKind)
{
    short modelId = id;

    switch (resourceKind) {
    case 0:
    case 1:
    case 2:
        if (modelId == 0x2201) {
            strcpy(fname_13, f_base_14[2]);
            strcat(fname_13, f_ukn_name_16[0]);
        } else {
            RES_GetMdlFileName(fname_13, modelId);
        }
        strcat(fname_13, f_exten_15[resourceKind]);
        break;
    case 3:
        switch ((modelId >> 12) & 15) {
        case 0:
            strcpy(fname_13, f_base_14[0]);
            strcat(fname_13, f_char_mot_17[0]);
            strcat(fname_13, f_exten_15[3]);
            break;
        case 2:
            if (modelId == 0x2201) {
                strcpy(fname_13, f_base_14[0]);
                strcat(fname_13, f_char_mot_17[1]);
                strcat(fname_13, f_exten_15[3]);
            } else {
                strcpy(fname_13, f_base_14[0]);
                strcat(fname_13, f_char_mot_17[2]);
                strcat(fname_13, f_exten_15[3]);
            }
            break;
        case 4:
            {
                int monsterId = modelId == 0x4037 ? 114 : modelId - 0x4000;
                char *motionName = f_mons_mot_18[0];

                strcpy(fname_13, f_base_14[1]);
                motionName[7] = monsterId % 10 + '0';
                monsterId /= 10;
                motionName[6] = monsterId % 10 + '0';
                monsterId /= 10;
                motionName[5] = monsterId % 10 + '0';
                strcat(fname_13, motionName);
                strcat(fname_13, f_exten_15[4]);
            }
            break;
        }
        break;
    case 4:
    case 5:
        RES_GetMdlFileName(fname_13, modelId + 0x10000);
        if (resourceKind == 4) {
            strcat(fname_13, f_exten_15[0]);
        } else {
            strcat(fname_13, f_exten_15[2]);
        }
        break;
    }
    return fname_13;
}

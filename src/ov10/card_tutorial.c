/*
 * OV10 original TU 7: 0x00a21348..0x00a21768 (3 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov10/cgp.h"

extern char *strcat(char *destination, const char *source);
extern char *strcpy(char *destination, const char *source);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern int printf(const char *format, ...);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglMatrixTrans(Matrix4 destination, Matrix4 source, const float translation[4]);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelSetTexture(const void *texture);
extern void nmlModelSetPlace(Matrix4 matrix);
extern void nmlModelEntry(u32 model);
extern int tutopage;       /* 0x00A487C8 */
extern u32 tulex_tbl[];    /* 0x00A57DB0, 2 entries */
extern void *tuxtx_tbl[];  /* 0x00A57DB8, 2 entries */
extern u32 lex;            /* 0x00A57DA8 */

/* "data\carddata\tuto\" */
char datapath[0x14] = "data\\carddata\\tuto\\";
extern const char D_00A4E320[];
extern const char D_00A4E328[];
extern const char D_00A4E330[];
extern const char D_00A4E338[];
extern const char D_00A4E340[];
extern const char D_00A4E348[];
extern const char D_00A4E350[];
extern const char D_00A4E358[];
extern const char D_00A4E360[];
extern const char D_00A4E368[];
extern const char D_00A4E370[];
extern const char D_00A4E378[];
extern const char D_00A4E380[];
extern const char D_00A4E388[];
extern const char D_00A4E390[];
extern const char D_00A4E398[];
extern const char D_00A4E3A0[];
extern const char D_00A4E3A8[];
extern const char D_00A4E3B0[];
extern const char D_00A4E3B8[];
extern const char D_00A4E3C0[];
extern const char D_00A4E3C8[];
extern const char D_00A4E3D0[];
extern const char D_00A4E3D8[];
extern const char D_00A4E3E0[];
extern const char D_00A4E3E8[];
extern const char D_00A4E3F0[];
extern const char D_00A4E3F8[];
extern const char D_00A4E400[];
extern const char D_00A4E408[];
extern const char D_00A4E410[];
extern const char D_00A4E418[];
extern const char D_00A4E420[];
extern const char D_00A4E428[];
extern const char D_00A4E430[];
extern const char D_00A4E438[];
extern const char D_00A4E440[];
extern const char D_00A4E448[];
extern const char D_00A4E450[];
extern const char D_00A4E458[];
extern const char D_00A4E460[];
extern const char D_00A4E468[];
extern const char D_00A4E470[];
extern const char D_00A4E478[];
extern const char D_00A4E480[];
extern const char D_00A4E488[];
extern const char D_00A4E490[];
static const char *tutoxtx_tbl[64] = {
    D_00A4E490, D_00A4E488, D_00A4E480, D_00A4E478,
    D_00A4E470, D_00A4E468, D_00A4E460, D_00A4E458,
    D_00A4E450, D_00A4E448, D_00A4E440, D_00A4E438,
    D_00A4E430, D_00A4E428, D_00A4E420, D_00A4E418,
    D_00A4E410, D_00A4E408, D_00A4E400, D_00A4E3F8,
    D_00A4E3F0, D_00A4E3E8, D_00A4E3E0, D_00A4E3D8,
    D_00A4E3D0, D_00A4E3C8, D_00A4E3C0, D_00A4E3B8,
    D_00A4E3B0, D_00A4E3A8, D_00A4E3A0, D_00A4E398,
    D_00A4E390, D_00A4E388, D_00A4E380, D_00A4E378,
    D_00A4E370, D_00A4E368, D_00A4E360, D_00A4E358,
    D_00A4E350, D_00A4E348, D_00A4E340, D_00A4E338,
    D_00A4E330, D_00A4E328, D_00A4E320,
};
const char D_00A4E498[8] = ".xtx";
static void *tuxtx;

int CardTutorialFileLoad(int tutorialIndex)
{
    char path[0x80];

    memset(path, 0, sizeof(path));
    strcpy(path, datapath);
    strcat(path, tutoxtx_tbl[tutorialIndex]);
    strcat(path, D_00A4E498);
    return xglCdReadFile(path, tuxtx, 0, 1);
}

/* Two-entry, NULL-terminated list of tutorial base names; the original pair is at 0x00A4E4B0 ({"Tutorial", NULL}). */
typedef struct {
    const char *name[2];
} TutorialNameList;

extern const TutorialNameList D_00A4E4B0;
extern const char D_00A4E4B8[];   /* "Tu_000.lex" at 0x00A4E4B8 */
extern const char D_00A4E4C8[];   /* ".lex" at 0x00A4E4C8 */

void CardTutorialInit(void)
{
    char path[0x80];
    char *buffer;
    int count;
    int i;
    TutorialNameList names;

    buffer = (char *)0x01a00000;
    tutopage = 0;
    lex = (u32)buffer;
    names = D_00A4E4B0;

    memset(path, 0, sizeof(path));
    strcpy(path, datapath);
    strcat(path, D_00A4E4B8);
    count = xglCdReadFile(path, (void *)0x01a00000, 0, 1);
    if (count > 0) {
        buffer += count;
        buffer = (char *)(((unsigned int)buffer + 15) & ~15u);
    }

    for (i = 0; i < 2 && names.name[i] != 0; i++) {
        memset(path, 0, sizeof(path));
        strcpy(path, datapath);
        strcat(path, names.name[i]);
        strcat(path, D_00A4E4C8);
        tulex_tbl[i] = (u32)buffer;
        count = xglCdReadFile(path, buffer, 0, 1);
        if (count > 0) {
            buffer += count;
            buffer = (char *)(((unsigned int)buffer + 15) & ~15u);
        }

        memset(path, 0, sizeof(path));
        strcpy(path, datapath);
        strcat(path, names.name[i]);
        strcat(path, D_00A4E498);
        tuxtx_tbl[i] = buffer;
        count = xglCdReadFile(path, buffer, 0, 1);
        if (count > 0) {
            buffer += count;
            buffer = (char *)(((unsigned int)buffer + 15) & ~15u);
        }
    }

    tuxtx = buffer;
    CardTutorialFileLoad(0);
}

/* Translation {1.2f, -1.82f, 0.0f, 1.0f} as two packed doublewords. */
typedef struct CardTutorialTranslation {
    u64 words[2];
} CardTutorialTranslation;
extern const CardTutorialTranslation D_00A4E4D0;
extern const char D_00A4E4E0[];

int CardTutorialProc(CardGameWork *work)
{
    Matrix4 source;
    Matrix4 placed;
    CardTutorialTranslation translation = D_00A4E4D0;
    unsigned short pressed;

    xglMatrixUnit(source);
    pressed = work->command.input[0].pressed;
    if (pressed & 0x8) {
        xglSoundEffectNormalID(1, 0);
        tutopage++;
        if (tutopage >= 47) {
            printf(D_00A4E4E0);
            return 0;
        }
        if (tutoxtx_tbl[tutopage] == 0) {
            return 0;
        }
        if (CardTutorialFileLoad(tutopage) < 0) {
            return 0;
        }
        pressed = work->command.input[0].pressed;
    }
    if (pressed & 0x4) {
        tutopage--;
        if (tutopage < 0) {
            tutopage = 0;
        } else {
            xglSoundEffectNormalID(2, 0);
            CardTutorialFileLoad(tutopage);
            pressed = work->command.input[0].pressed;
        }
    }
    if (pressed & 0x800) {
        return 0;
    }
    xglMatrixTrans(placed, source, (const float *)translation.words);
    nmlModelSetTexture(tuxtx_tbl[0]);
    nmlModelSetPlace(placed);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(tulex_tbl[0]);
    xglMatrixUnit(source);
    source[3][2] = -1.0f;
    nmlModelSetPlace(source);
    nmlModelSetTexture(tuxtx);
    nmlModelEntry(lex);
    return 1;
}



const char D_00A4E320[8] = "Tu_046";

const char D_00A4E328[8] = "Tu_045";

const char D_00A4E330[8] = "Tu_044";

const char D_00A4E338[8] = "Tu_043";

const char D_00A4E340[8] = "Tu_042";

const char D_00A4E348[8] = "Tu_041";

const char D_00A4E350[8] = "Tu_040";

const char D_00A4E358[8] = "Tu_039";

const char D_00A4E360[8] = "Tu_038";

const char D_00A4E368[8] = "Tu_037";

const char D_00A4E370[8] = "Tu_036";

const char D_00A4E378[8] = "Tu_035";

const char D_00A4E380[8] = "Tu_034";

const char D_00A4E388[8] = "Tu_033";

const char D_00A4E390[8] = "Tu_032";

const char D_00A4E398[8] = "Tu_031";

const char D_00A4E3A0[8] = "Tu_030";

const char D_00A4E3A8[8] = "Tu_029";

const char D_00A4E3B0[8] = "Tu_028";

const char D_00A4E3B8[8] = "Tu_027";

const char D_00A4E3C0[8] = "Tu_026";

const char D_00A4E3C8[8] = "Tu_025";

const char D_00A4E3D0[8] = "Tu_024";

const char D_00A4E3D8[8] = "Tu_023";

const char D_00A4E3E0[8] = "Tu_022";

const char D_00A4E3E8[8] = "Tu_021";

const char D_00A4E3F0[8] = "Tu_020";

const char D_00A4E3F8[8] = "Tu_019";

const char D_00A4E400[8] = "Tu_018";

const char D_00A4E408[8] = "Tu_017";

const char D_00A4E410[8] = "Tu_016";

const char D_00A4E418[8] = "Tu_015";

const char D_00A4E420[8] = "Tu_014";

const char D_00A4E428[8] = "Tu_013";

const char D_00A4E430[8] = "Tu_012";

const char D_00A4E438[8] = "Tu_011";

const char D_00A4E440[8] = "Tu_010";

const char D_00A4E448[8] = "Tu_009";

const char D_00A4E450[8] = "Tu_008";

const char D_00A4E458[8] = "Tu_007";

const char D_00A4E460[8] = "Tu_006";

const char D_00A4E468[8] = "Tu_005";

const char D_00A4E470[8] = "Tu_004";

const char D_00A4E478[8] = "Tu_003";

const char D_00A4E480[8] = "Tu_002";

const char D_00A4E488[8] = "Tu_001";

const char D_00A4E490[8] = "Tu_000";

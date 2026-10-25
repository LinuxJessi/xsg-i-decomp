/* UMN code embedded in MAIN, original OV02 translation unit 12. */
#include "common.h"
#include "shared.h"

typedef unsigned char u8;
typedef signed short s16;

typedef struct TyaUmlDispParam {
    int resource_id;
    u8 unmodeled_04[4];
    s16 render_x;
    s16 render_y;
    s16 render_width;
    s16 render_height;
    u64 color;
    s16 clip_x;
    s16 clip_y;
    s16 clip_width;
    s16 clip_height;
    int loaded_resource_id;
    u8 unmodeled_24[4];
    u8 render_pending;
} TyaUmlDispParam;

typedef struct TyaUmlImage {
    u8 unmodeled_00[4];
    u16 width;          /* 0x04: masked to an even value before use */
    s16 height;         /* 0x06 */
    void *environment;  /* 0x08: cleared by tyaUmlDispInit2 */
    int count;           /* 0x0C: sceVif1PkRef's transfer count */
} TyaUmlImage;

/* Original embedded-OV02 storage; no new data definitions. */
extern u8 *D_00A13660;
extern s16 base_tbl[0x140];
extern TyaUmlImage image_00A10BC0[8];
extern void *memset(void *dest, int value, unsigned int size);
extern int tyaUmlDispLoad(TyaUmlDispParam *parameter);
extern void tyaUmlDispType3(TyaUmlDispParam *parameter);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", mark);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tail);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispLoad);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", texture_trans);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispType3Sub0);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispType3Sub1);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispType3);

int tyaUmlDispMain(TyaUmlDispParam *parameter)
{
    if (parameter == 0)
        return -1;

    if (parameter->resource_id < 0) {
        memset(D_00A13660, 0, 100);
        memset(image_00A10BC0, 0, 128);
        tyaUmlDispType3(parameter);
        return 0;
    }
    if (parameter->resource_id != parameter->loaded_resource_id)
        return tyaUmlDispLoad(parameter);

    tyaUmlDispType3(parameter);
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", find_text);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", get_fileno);

static int get_indexno(int target_id)
{
    s16 *entry = base_tbl;
    int index = 0;

    while (*entry != -2) {
        s16 value = *entry;

        if (value == -1)
            index++;
        if (value != -1 && value != -3 && value - 9000 == target_id)
            return index;
        entry++;
    }

    return -1;
}

static int get_selectno(int n)
{
    s16 *entry = base_tbl;
    int index = 0;

    while (*entry != -2) {
        s16 value = *entry;

        if (value == -1)
            n--;
        if (value != -1 && value != -3) {
            if (n == 0)
                return index;
            index++;
        }
        entry++;
    }

    return -1;
}

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", dbfileno_load);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", dbheader_load);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", disp_index);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDatabaseMain);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispParamReset);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispInit2);

INCLUDE_ASM("asm/main/nonmatchings/ov02/12_tya_uml", tyaUmlDispInit);

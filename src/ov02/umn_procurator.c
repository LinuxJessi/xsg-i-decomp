/*
 * OV02 original TU 8: 0x00a04170..0x00a05738 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "umn_procurator.h"

#include "shared.h"
extern PadPrefix PadData;
extern int eMessageNextPage(void *page, int arg);
extern void xglSoundEffectNormalID(int id, int arg);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern const char D_00A12060[];

int UmnManzaiControl(void)
{
    unsigned int flag = UmnManzaiFlag;
    short *next;
    unsigned int cleared;

    if (!(flag & 0x40)) {
        if ((flag & 0x30) == 0x30) {
            cleared = flag & ~1;
            UmnManzaiFlag = cleared & ~8;
            if ((flag & 0x206) == 0) {
                if (PadData.half_2a == 0x20) {
                    UmnManzaiFlag |= 1;
                    if (eMessageNextPage(UmnWork + 0x7C + *UmnManzaiText * 164, 0) == 0) {
                        next = UmnEventTextNextGet(1);
                        xglSoundEffectNormalID(1, 0);
                        if (next == 0) {
                            UmnManzaiFlag |= 0x40;
                        } else {
                            UmnManzaiText = next;
                            UmnManzaiFlag |= 8;
                        }
                    }
                }
            }
        }
    } else if ((flag & 0x30) == 0) {
        return 0;
    }
    xglFontDebugPrintf(0x80, 0x40, D_00A12060, UmnManzaiFlag);
    return 1;
}

int UmnManzaiInit(void)
{
    UmnManzaiWinTask *window;
    int index;

    if (UmnEventTextMake() < 0) {
        return -1;
    }
    UmnManzaiText = UmnEventTextNextGet(0);
    if (UmnModelSon == 0) {
        MenuModelCreate(&UmnModelSon, 3);
    }
    MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorManzai, 0);
    MenuModelExtFuncSet(UmnModelSon, UmnProcuratorManzai2, 0);
    index = 0;
    window = UmnManzaiWin;
    do {
        UmnObjectTaskCreate(tskUmnManzaiWin, window);

        {
            int next_index = index + 1;
            window->task_index = next_index;
            index = next_index;
        }
        /* Advance this loop to the next task record. */
        window++;
    } while (index < 2);
    UmnManzaiFlag = 6;
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", tskUmnManzaiWin);

extern void xglLightIntensityParallel(void *light, int index,
                                      const Vector4 *intensity);
extern void xglLightIntensityAmbient(void *light,
                                     const Vector4 *intensity);
extern void xglLightDirection(void *light, unsigned int index,
                              const Vector4 *direction);

typedef struct UmnLightSet {
    Vector4 v[7];
} UmnLightSet __attribute__((aligned(8)));

extern const UmnLightSet D_00A120F0;

static void LocalLightSet(void *model, int unused)
{
    UmnLightSet lights = D_00A120F0;
    void *light = (unsigned char *)model + 0x510;

    xglLightIntensityAmbient(light, &lights.v[0]);
    xglLightIntensityParallel(light, 0, &lights.v[1]);
    xglLightDirection(light, 0, &lights.v[4]);
    xglLightIntensityParallel(light, 1, &lights.v[2]);
    xglLightDirection(light, 1, &lights.v[5]);
    xglLightIntensityParallel(light, 2, &lights.v[3]);
    xglLightDirection(light, 2, &lights.v[6]);
    *(unsigned int *)model |= 0x8000;
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorManzai2);

static void Rotation(float *out, float *position, float *base, float *rotation)
{
    float matrix[16];

    xglMatrixStackUnit();
    xglMatrixStackRotZ(rotation[2]);
    xglMatrixStackRotY(rotation[1]);
    xglMatrixStackRotX(rotation[0]);
    xglMatrixStackTrans(position);
    xglMatrixStackSave((float (*)[4])matrix);

    out[0] = base[0] + matrix[12];
    out[1] = base[1] + matrix[13];
    out[2] = base[2] + matrix[14];
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorManzai);

typedef struct UmnMailVec {
    float x;
    float y;
    float z;
    float w;
} UmnMailVec __attribute__((aligned(8)));

typedef struct UmnMailTask {
    unsigned char unmodeled_00[0x11];
    unsigned char active;               /* 0x11 */
    unsigned char unmodeled_12;
    unsigned char state;                /* 0x13 */
    unsigned char unmodeled_14[0x0C];
    unsigned char *model;               /* 0x20 */
} UmnMailTask;

extern UmnMailVec D_00A12230;

void UmnProcuratorMail(void *argument)
{
    UmnMailTask *task = (UmnMailTask *)argument;
    float *color;
    float *direction;
    float *third;
    int done;
    int state;

    if (task->active == 0)
        return;

    color = (float *)(task->model + 0xC0);
    direction = (float *)(task->model + 0xD0);
    third = (float *)(task->model + 0xE0);

    state = task->state;
    if (state != 1) {
        if (state > 1)
            return;
        if (state != 0)
            return;
        {
        StudioCamera *camera;
        UmnMailVec rotation;
        UmnMailVec position;

        task->state = 1;
        LocalLightSet(task->model, 0);
        camera = xglStudioGetCamera2(0);
        memset(&rotation, 0, 16);
        rotation.w = 1.0f;
        position = D_00A12230;
        *(UmnMailVec *)&camera->position = position;
        *(UmnMailVec *)&camera->rotation = rotation;
        MenuModelUnitOpen(task, 2);
        }
    }
    done = 0;
    third[0] += 0.01f;
    if (third[0] >= 1.0f) {
        third[0] = 1.0f;
        done++;
    }
    third[1] = third[0];
    third[2] = third[0];
    color[0] += 0.02f;
    if (color[0] >= 1.0f) {
        color[0] = 1.0f;
        done++;
    }
    color[1] -= 0.01f;
    if (color[1] <= -0.3f) {
        color[1] = -0.3f;
        done++;
    }
    direction[0] += 0.01f;
    if (direction[0] >= 0.0f) {
        direction[0] = 0.0f;
        done++;
    }
    direction[1] -= 0.01f;
    if (direction[1] <= -0.7853982f) {
        direction[1] = -0.7853982f;
        done++;
    }
    if (done == 5)
        task->state = 2;
}

typedef struct UmnTopTask {
    unsigned char unmodeled_00[0x11];
    unsigned char active;               /* 0x11 */
    unsigned char unmodeled_12;
    unsigned char state;                /* 0x13 */
    unsigned char unmodeled_14[0x0C];
    unsigned char *model;               /* 0x20 */
} UmnTopTask;

void UmnProcuratorTop(void *argument)
{
    UmnTopTask *task = (UmnTopTask *)argument;
    float *color;
    float *direction;
    int state;

    if (task->active == 0)
        return;

    color = (float *)(task->model + 0xC0);
    direction = (float *)(task->model + 0xD0);

    state = task->state;
    if (state != 1) {
        if (state > 1)
            return;
        if (state != 0)
            return;
        color[0] = 1.0f;
        color[1] = -0.3f;
        direction[0] = 0.0f;
        direction[1] = -0.7853982f;
        LocalLightSet(task->model, 0);
        task->state = 1;
    }
    MenuModelUnitOpen(task, 2);
    task->state = 2;
}

typedef struct UmnProcuratorTask {
    unsigned char unmodeled_00[0x10];
    signed char flag10;                 /* 0x10 */
    unsigned char active;               /* 0x11 */
    unsigned char unmodeled_12;
    unsigned char state;                /* 0x13 */
    unsigned char unmodeled_14[0x0C];
    unsigned char *model;               /* 0x20 */
} UmnProcuratorTask;

extern void LocalLightSet();

void UmnProcuratorGoodBy(void *argument)
{
    UmnProcuratorTask *task = (UmnProcuratorTask *)argument;
    float *color;
    float *direction;

    if (task->active == 0)
        return;

    color = (float *)(task->model + 0xC0);
    direction = (float *)(task->model + 0xD0);

    switch (task->state) {
    case 0:
        task->state = 1;
        LocalLightSet(task->model, 0);
        MenuModelUnitOpen(task, 3);
    case 1:
        color[0] += 0.4f;
        direction[1] += 0.4f;
        if (color[0x18] == 0.0f)
            task->flag10 = -1;
        break;
    }
}

void UmnProcuratorSet(void)
{
    switch (UmnWork[1]) {
    case 0:
        if (UmnModelUkn == 0)
            MenuModelCreate(&UmnModelUkn, 0x1D);
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorTop, 0);
        break;
    case 1:
        if (UmnModelUkn == 0)
            MenuModelCreate(&UmnModelUkn, 0x1D);
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorMail, 0);
        break;
    default:
        do { MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorGoodBy, 0); } while (0);
        break;
    }
}

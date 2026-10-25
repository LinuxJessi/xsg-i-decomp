/*
 * TU-local declarations of ov02/tu008 (src/ov02/umn_procurator.c).
 */

#ifndef SRC_OV02_UMN_PROCURATOR_H
#define SRC_OV02_UMN_PROCURATOR_H

#include "main/e_message.h"
#include "ov02/tsk_umn_object_task_main.h"
#ifndef OV02_12_PROCURATOR_PRIVATE_H
#define OV02_12_PROCURATOR_PRIVATE_H

#include "shared.h"

/* Owner proposal: src/main/menu_model.c, main/tu165. */
typedef struct MenuModelActor MenuModelActor;

typedef union MenuModelTransformVector {
    struct {
        float x;
        float y;
        float z;
        float w;
    } value;
    unsigned long long words[2];
} MenuModelTransformVector;

typedef struct MenuModelTransformControl {
    MenuModelTransformVector offset;
    MenuModelTransformVector rotation;
    MenuModelTransformVector scale;
    int enabled;
    u8 unmodeled_34[0x0c];
    MenuModelTransformVector position;
    u8 unmodeled_50[0x10];
    float transparency;
    void (*drawTypeFunction)(MenuModelActor *actor);
    int initialMotionId;
    u8 unmodeled_6c[4];
} MenuModelTransformControl;

typedef unsigned int XglLightQuadword __attribute__((mode(TI)));

typedef struct XglPackedParallelLight {
    XglLightQuadword intensity;
    XglLightQuadword direction;
} XglPackedParallelLight;

typedef struct XglPackedLightSet {
    XglLightQuadword ambientIntensity;
    XglPackedParallelLight parallel[3];
} XglPackedLightSet;

typedef struct MenuModelActorLight {
    XglPackedLightSet source;
    Matrix4 directionMatrix;
    Matrix4 intensityMatrix;
} MenuModelActorLight;

struct MenuModelActor {
    u32 actorFlags;
    u8 unmodeled_04[0xbc];
    MenuModelTransformControl menu;
    u8 unmodeled_130[0x3e0];
    MenuModelActorLight light;
};

/* The menu unit is a scheduler task, distinct from the actor at +0x20. */
typedef struct MenuModelUnit {
    u8 unmodeled_00[0x10];
    signed char taskPhase;
    u8 active;
    u8 unmodeled_12;
    u8 extFuncFlag;
    u8 unmodeled_14[0x0c];
    MenuModelActor *actor;
} MenuModelUnit;

#endif

/* The original array has two 0xa4-byte window records, each with its message. */
typedef struct UmnManzaiWinTask {
    unsigned char unmodeled_00;
    unsigned char task_index;
    unsigned char unmodeled_02[0x5e];
    EMessageParam message;
    unsigned char unmodeled_90[0x14];
} UmnManzaiWinTask;

typedef void (*UmnTaskFunction)(void *task);

extern unsigned int UmnManzaiFlag;
extern short *UmnManzaiText;
extern void *UmnModelSon;
extern void *UmnModelUkn;
extern UmnWorkData UmnWork;
extern UmnManzaiWinTask UmnManzaiWin[2];

int UmnEventTextMake(void);
short *UmnEventTextNextGet(int index);
void UmnObjectTaskCreate(UmnTaskFunction function, void *argument);
void MenuModelCreate(void **model, int model_id);
void MenuModelExtFuncSet(void *model, UmnTaskFunction function, void *argument);
void MenuModelUnitOpen(void *model, int draw_type);

void UmnProcuratorManzai(void *task);
void UmnProcuratorManzai2(void *task);
void UmnProcuratorMail(void *task);
void UmnProcuratorTop(void *task);
void UmnProcuratorGoodBy(void *task);
void tskUmnManzaiWin(void *task);

/*
 * The xglMatrixStack* wrappers Rotation calls come from their definer,
 * main/tu100 xgl_2.c, through include/main/xgl_2.h.
 */

#endif /* SRC_OV02_UMN_PROCURATOR_H */

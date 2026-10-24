/* UMN code embedded in MAIN, original OV02 translation unit 5. */
#include "common.h"
#include "shared.h"
#include "ov02/tsk_umn_object_task_main.h"
extern UmnWorkData UmnWork;

extern void UmnProcuratorSet(void);

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", tskUmnObjectTaskMain);

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", UmnObjectTaskCreate);

void UmnChangeTopLevel(int level) {
    UmnWorkQuadword zero = 0;
    int index = 0;
    int remaining;

    for (remaining = 6; remaining >= 0; remaining--)
        UmnWork.section_state[index++] = zero;

    UmnWork.mode = level;
    UmnWork.stateOrCategory = 0;
    UmnProcuratorSet();
}

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", UmnFirstLoad);

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", UmnInit);

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", UmnMain);

INCLUDE_ASM("asm/main/nonmatchings/ov02/05_tsk_umn_object_task_main", UmnMain2);

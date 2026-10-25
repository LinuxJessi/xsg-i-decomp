#ifndef INCLUDE_OV02_TSK_UMN_OBJECT_TASK_MAIN_H
#define INCLUDE_OV02_TSK_UMN_OBJECT_TASK_MAIN_H

typedef unsigned int UmnWorkQuadword __attribute__((mode(TI)));

typedef struct UmnWorkData {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02;
    unsigned char stateOrCategory;
    unsigned char unmodeled_04[0x0C];
    UmnWorkQuadword section_state[7];
} UmnWorkData;

#endif /* INCLUDE_OV02_TSK_UMN_OBJECT_TASK_MAIN_H */

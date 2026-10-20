/*
 * OV02 original TU 6: 0x00a02d68..0x00a03450 (5 functions)
 */
#include "common.h"

#include "shared.h"
#include "main/xgl_2.h"
#include "main/xgl_task.h"

extern unsigned char UmnWork[];
extern float xglFRand(void);
extern void xglLightIntensityParallel(void *light, int index, const Vector4 *intensity);
extern void xglLightIntensityAmbient(void *light, const Vector4 *intensity);
extern void xglLightDirection(void *light, unsigned int index, const Vector4 *direction);
extern void xglLightCalcMatrix(void *light);
extern void nmlModelSetLight(void *model, void *light);
extern void nmlModelSetTexture(const char *texture);
extern void nmlModelSetPlace(const Vector4 *place);
extern void nmlModelEntry(int entry);

/* A 16-byte vector that can be read as x/y/z/w (Vector4) or as a component
 * array; aligned(8) because the original copies it with ld/sd pairs. */
typedef union UmnBgCubeVec {
    Vector4 v;
    float f[4];
} UmnBgCubeVec __attribute__((aligned(8)));

typedef struct UmnBgCubeLights {
    UmnBgCubeVec v[7];
} UmnBgCubeLights __attribute__((aligned(8)));

extern UmnBgCubeVec D_00A11E60;
extern const UmnBgCubeLights D_00A11E70;

typedef struct UmnBgCubeData {
    UmnBgCubeVec position;
    UmnBgCubeVec rotation;
    UmnBgCubeVec spare;
    UmnBgCubeVec scale;
    Matrix4 matrix;
} UmnBgCubeData;

/* One contiguous 0xF0-byte light workspace: the light object proper (0x00-0x6F)
 * followed by the two matrices xglLightCalcMatrix fills in (0x70 and 0xB0),
 * which are what nmlModelSetLight receives. */
typedef struct UmnBgCubeLightWork {
    unsigned char light[0x70];
    Matrix4 model_set;
    Matrix4 model_light;
} UmnBgCubeLightWork __attribute__((aligned(16)));

typedef struct UmnBgCubeMainTask {
    XglTaskPrefix base;
    unsigned char state;
    unsigned char enabled;
    unsigned char camera_id;
    unsigned char unmodeled_13;
    int texture;
    int model;
    UmnBgCubeData *data;
} UmnBgCubeMainTask;

void tskUmnBgCubeMain(XglTaskPrefix *task_opaque)
{
    UmnBgCubeMainTask *task = (UmnBgCubeMainTask *)task_opaque;
    UmnBgCubeVec *position = &task->data->position;
    UmnBgCubeVec *rotation = &task->data->rotation;
    UmnBgCubeVec *spare = &task->data->spare;
    UmnBgCubeVec *scale = &task->data->scale;
    float (*matrix)[4] = task->data->matrix;
    UmnBgCubeVec zero;
    UmnBgCubeVec scale_init;
    UmnBgCubeLightWork work;
    UmnBgCubeLights lights;
    StudioCamera *camera;
    int i;

    if (UmnWork[1] == 0xFF)
        task->state = 99;

    switch (task->state) {
    case 0:
        task->state = 1;
        task->enabled = 1;
        memset(&zero, 0, 16);
        zero.v.w = 1.0f;
        scale_init = D_00A11E60;
        *position = zero;
        *rotation = zero;
        *spare = zero;
        *scale = scale_init;
        rotation->v.x = xglFRand();
        rotation->v.y = xglFRand();
        rotation->v.z = xglFRand();
        spare->v.x = xglFRand();
        spare->v.y = xglFRand();
        spare->v.z = xglFRand();
        scale->v.z = 7.5f;
        scale->v.y = 7.5f;
        scale->v.x = 7.5f;
    case 1:
        for (i = 0; i < 3; i++)
            rotation->f[i] += 0.001f;
        break;
    case 99:
        xglTaskWaitRemove(task_opaque);
        return;
    }
    if (task->enabled == 0)
        return;
    camera = xglStudioGetCamera2(task->camera_id);
    camera->active = 1;
    xglMatrixStackUnit();
    xglMatrixStackScale(scale->f);
    xglMatrixStackTrans(position->f);
    xglMatrixStackRotZ(rotation->v.z);
    xglMatrixStackRotY(rotation->v.y);
    xglMatrixStackRotX(rotation->v.x);
    xglMatrixStackSave(matrix);
    lights = D_00A11E70;
    xglLightIntensityAmbient(&work, &lights.v[0].v);
    xglLightIntensityParallel(&work, 0, &lights.v[1].v);
    xglLightDirection(&work, 0, &lights.v[4].v);
    xglLightIntensityParallel(&work, 1, &lights.v[2].v);
    xglLightDirection(&work, 1, &lights.v[5].v);
    xglLightIntensityParallel(&work, 2, &lights.v[3].v);
    xglLightDirection(&work, 2, &lights.v[6].v);
    xglLightCalcMatrix(&work);
    nmlModelSetLight(&work.model_light, &work.model_set);
    nmlModelSetTexture((const char *)task->texture);
    nmlModelSetPlace((const Vector4 *)matrix);
    nmlModelEntry(task->model);
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_interface2", UmnBgCubeInit);

#include "main/xgl_packet.h"

extern const char D_00A11EE0[];
extern const char D_00A11EF8[];
extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

typedef struct UmnIf2Render {
    unsigned char unmodeled_00[0x14];
    unsigned short height;
} UmnIf2Render;

extern UmnIf2Render sRender;

typedef struct UmnIf2Words {
    int w[6];
} UmnIf2Words;

typedef struct UmnIf2Context {
    XglPacket *packet;
} UmnIf2Context;

static void DrawUmnInterface2(void *context_arg, void *arg)
{
    UmnIf2Context *context = (UmnIf2Context *)context_arg;
    unsigned char *color = (unsigned char *)arg;
    unsigned long long *gif = (unsigned long long *)((unsigned char *)context + 0x30);
    unsigned int *words = (unsigned int *)gif;
    unsigned int *out;
    UmnIf2Words a;
    UmnIf2Words b;
    int i;

    gif[0] = 0x1000000000000004ULL;
    gif[1] = 0xE;
    gif[2] = 0;
    gif[3] = 0x3F;
    gif[4] = 0x44;
    gif[5] = 0x42;
    gif[6] = (0x24120000 | (sRender.height << 5)) | 0x2000000640000000ULL;
    gif[7] = 6;
    gif[8] = 0x5000D;
    gif[9] = 0x47;
    sceVif1PkAddDirectDataN(context->packet, gif, 5);

    a = *(const UmnIf2Words *)D_00A11EE0;
    b = *(const UmnIf2Words *)D_00A11EF8;

    words[0] = 0x8006;
    words[1] = 0x30AEC000;
    words[2] = 0x531;
    words[3] = 0;
    out = (unsigned int *)((unsigned char *)context + 0x40);

    for (i = 0; i < 6; i++) {
        if (i == 0) {
            out[0] = color[4];
            out[1] = color[5];
            out[2] = color[6];
            out[3] = color[7];
        } else {
            out[0] = color[8];
            out[1] = color[9];
            out[2] = color[10];
            out[3] = color[11];
        }
        out += 4;
        out[0] = a.w[i] << 4;
        out[1] = b.w[i] << 4;
        out[3] = 0;
        out[2] = 0;
        out += 4;
        out[0] = (a.w[i] << 4) + 0x6FF8;
        out[1] = (b.w[i] << 4) + 0x71F8;
        out[2] = *(unsigned int *)color;
        out[3] = 0;
        out += 4;
    }
    sceVif1PkAddDirectDataN(context->packet, gif, 0x13);
}

/*
 * State reached through the global UmnInterface2 pointer; only the two
 * frame counters UmnInterface2Main touches are evidenced.
 */
typedef struct UmnInterface2State {
    unsigned int base_color;
    unsigned char color_a[3];
    unsigned char intensity_a; /* decremented by 8 each frame until zero */
    unsigned char color_b[3];
    unsigned char intensity_b; /* decremented by 2 each frame until zero */
} UmnInterface2State;

/* The font queue calls draw(context, arg) with the arg registered below;
 * the context record (read at +0x00 and +0x30) is not yet recovered. */
static void DrawUmnInterface2(void *context, void *arg);
extern void xglFontPrintExtFunc(unsigned int flags, void (*draw)(void *context, void *arg), void *arg);
extern UmnInterface2State *UmnInterface2;

void UmnInterface2Main(void) {
    if (UmnInterface2->intensity_a != 0) {
        UmnInterface2->intensity_a -= 8;
    }
    if (UmnInterface2->intensity_b != 0) {
        UmnInterface2->intensity_b -= 2;
    }
    if (UmnInterface2->intensity_a != 0 || UmnInterface2->intensity_b != 0) {
        xglFontPrintExtFunc(0x03FFFFF0, DrawUmnInterface2, UmnInterface2);
    }
}

void *UmnInterface2Init(void *memory) {
    unsigned int aligned_address;
    UmnInterface2State *color_a_state;
    void *result;
    UmnInterface2State *color_b_state;

    result = 0;
    if (memory != 0) {
        aligned_address = ((unsigned int)memory + 15) & ~15u;
        UmnInterface2 = (UmnInterface2State *)aligned_address;
        result = (void *)(aligned_address + 12);
    }

    color_a_state = UmnInterface2;
    color_a_state->color_a[0] = 0x80;
    color_a_state->intensity_a = 0x80;
    color_a_state->color_a[2] = 0x80;
    color_a_state->color_a[1] = 0x80;
    color_a_state->base_color = 0x00fffffe;
    color_b_state = UmnInterface2;
    color_b_state->intensity_b = 0x80;
    color_b_state->color_b[2] = 0x80;
    color_b_state->color_b[1] = 0x80;
    color_b_state->color_b[0] = 0x80;
    return result;
}

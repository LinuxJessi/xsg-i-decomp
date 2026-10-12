#include "common.h"

static int mode_004DC5A8;

#include "shared.h"
#include "main/get.h"

#include "db_light_write.h"

/*
 * The retail ELF has a GLOBAL OBJECT named cursor at 0x0099C7F0 with size
 * 0x60. The shared declaration and this TU's uses establish a 16-byte
 * HomogeneousVector slot at +0x10; the callback pair is accessed as raw
 * words at +0x50/+0x54. Six slots of the existing vector type express the
 * recorded object extent without assigning meanings to the other words.
 */

typedef struct CursorState {
    int mode;
    unsigned char unmodeled_04[0x0C];
    HomogeneousVector position;
    unsigned char unmodeled_20[0x30];
    CursorCallback callback;
    void *callback_argument;
    unsigned char unmodeled_58[8];
} CursorState;

HomogeneousVector cursor[6];

#include "main/xgl_studio.h"

/* Owner declarations used by these debug views; storage remains original scaffolding. */

typedef struct {
    unsigned char unmodeled_00[0x50];
    unsigned int matrix_data_offset;
} MapPartListView;

typedef struct {
    unsigned char unmodeled_00[4];
    MapPartListView *parts;
} MapStageModelView;

typedef struct {
    unsigned char unmodeled_00[0x54];
    MapStageModelView *model;
    unsigned char unmodeled_58[0x28];
    Vector4 stage_color;
} MapGameLoopStateView;

typedef struct {
    unsigned char unmodeled_00[0x28];
    unsigned short half_28;
    unsigned short half_2a;
    unsigned short half_2c;
    unsigned char unmodeled_2e[0x66 - 0x2e];
    signed char stick_x;
    signed char stick_y;
} DbLightWritePadSecondPrefix;

typedef struct {
    unsigned char unmodeled_00[0x28];
    unsigned short half_28;
    unsigned short half_2a;
    unsigned char unmodeled_2c[0x68 - 0x2c];
    DbLightWritePadSecondPrefix second;
} DbLightWritePadData;

extern MapGameLoopStateView GameLoopState;

extern DbLightWritePadData PadData;

extern int visible_0;

extern int partsIndex;

extern const char D_004C2158[];

extern const char D_004DA520[];

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

extern void nmlModelSetPartsVisible(MapPartListView *parts,
                                    int part_index, int visible);

extern Vector4 lightDir[6];

extern Vector4 lightCol[6];

extern void xglLightIntensityParallel(void *light, int index,
                                      const Vector4 *intensity);

extern void xglLightIntensityAmbient(void *light,
                                     const Vector4 *intensity);

extern void xglLightDirection(void *light, unsigned int index,
                              const Vector4 *direction);

extern float atan2f(float y, float x);

extern float xglSin(float angle);

extern float xglCos(float angle);


INCLUDE_ASM("asm/main/nonmatchings/db_light_write", DB_lightWrite);

typedef struct {
    Matrix4 matrix;
    Matrix4 world;
    Vector4 screen_scale;
    Vector4 screen_offset;
    Matrix4 *local;
} DebugDrawTransform;

typedef struct {
    unsigned int count, primitive, registers, reserved;
    unsigned int payload[8][4];
} DebugRectPacket;

typedef struct {
    DebugRectPacket packet;
    unsigned char unmodeled_90[0x400 - 0x90];
    DebugDrawTransform transform;
    unsigned char unmodeled_4a4[0x4e0 - 0x4a4];
    Vector4 positions[5];
} DebugRectScratch;

extern unsigned char col[56];
extern const Vector4 vRect[4];
extern void transformVector(Vector4 *destination, const Vector4 *vectors,
                            int count, DebugDrawTransform *params);
extern void xglPrimAddGifTagDirect(XglPacket *packet, const void *tag, int count);

void drawRect2(Matrix4 *matrix, int color_index)
{
    StudioCamera *camera;
    DebugRectScratch *scratch = (DebugRectScratch *)0x70000000;
    Vector4 *position;
    unsigned int *write;
    const unsigned char *red;
    const unsigned char *green;
    const unsigned char *blue;
    const unsigned char *alpha;
    float pixel_scale;

    color_index *= 2;
    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(scratch->transform.world);
    scratch->transform.screen_offset = camera->screenOffset;
    scratch->transform.screen_scale.x = 1.0f;
    scratch->transform.screen_scale.y = -1.0f;
    scratch->transform.screen_scale.z = 1.0f;
    scratch->transform.screen_scale.w = 1.0f;
    scratch->transform.local = matrix;
    color_index = color_index * 4 + 4;
    transformVector(scratch->positions, vRect, 4, &scratch->transform);
    write = (unsigned int *)0x70000000;
    write[0] = 0x8004;
    write[1] = 0x20064000;
    write[2] = 0xffffff51;
    write[3] = 0;
    write += 4;
    position = (Vector4 *)0x700004e0;
    red = &col[color_index];
    green = &col[1] + color_index;
    blue = &col[2] + color_index;
    alpha = &col[3] + color_index;
    pixel_scale = 16.0f;
    {
        int vertex_index = 0;

        for (; vertex_index < 4; vertex_index++, position++) {
            write[0] = *red;
            write[1] = *green;
            write[2] = *blue;
            write[3] = *alpha;
            write += 4;
            write[0] = (unsigned int)(position->x * pixel_scale);
            write[1] = (unsigned int)(position->y * pixel_scale);
            write[2] = ~0U;
            write[3] = 0;
            write += 4;
        }
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 9);
}

extern const Vector4 vBox[5];

void drawBox2(Matrix4 *matrix, int color_index)
{
    StudioCamera *camera;
    DebugRectScratch *scratch = (DebugRectScratch *)0x70000000;
    int vertex_count = 5;
    Vector4 *position;
    unsigned int (*write)[4];
    const unsigned char *red;
    const unsigned char *green;
    const unsigned char *blue;
    const unsigned char *alpha;
    int vertex_index;

    color_index *= 2;
    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(scratch->transform.world);
    scratch->transform.screen_offset = camera->screenOffset;
    scratch->transform.screen_scale.x = 1.0f;
    scratch->transform.screen_scale.y = -1.0f;
    scratch->transform.screen_scale.z = 1.0f;
    scratch->transform.screen_scale.w = 1.0f;
    scratch->transform.local = matrix;
    color_index = color_index * 4 + 4;
    transformVector(scratch->positions, vBox, vertex_count, &scratch->transform);
    write = (unsigned int (*)[4])0x70000000;
    (*write)[0] = 0x8000 | vertex_count;
    (*write)[1] = 0x20054000;
    (*write)[2] = 0xffffff51;
    (*write)[3] = 0;
    write++;
    position = (Vector4 *)0x700004e0;
    red = &col[color_index];
    green = &col[1] + color_index;
    blue = &col[2] + color_index;
    alpha = &col[3] + color_index;
    for (vertex_index = 0; vertex_index < vertex_count; vertex_index++, position++) {
        (*write)[0] = *red;
        (*write)[1] = *green;
        (*write)[2] = *blue;
        (*write)[3] = *alpha;
        write++;
        (*write)[0] = (unsigned int)(position->x * 16.0f);
        (*write)[1] = (unsigned int)(position->y * 16.0f);
        (*write)[2] = ~0U;
        (*write)[3] = 0;
        write++;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, vertex_count * 2 + 1);
}

typedef struct {
    DebugRectPacket packet;
    unsigned char unmodeled_90[0x400 - 0x90];
    DebugDrawTransform transform;
    unsigned char unmodeled_4a4[0x4b0 - 0x4a4];
    Vector4 input[2];
    unsigned char unmodeled_4d0[0x10];
    Vector4 output[2];
} DebugLineScratch;

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawCircle2);

void drawVector2(const Vector3 *vector, Matrix4 *matrix, int color_index)
{
    StudioCamera *camera;
    DebugLineScratch *scratch = (DebugLineScratch *)0x70000000;
    Vector4 *position;
    unsigned int (*write)[4];
    int vertex_index;

    color_index *= 2;
    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(scratch->transform.world);
    scratch->transform.screen_offset = camera->screenOffset;
    scratch->transform.screen_scale.x = 1.0f;
    scratch->transform.screen_scale.y = -1.0f;
    scratch->transform.screen_scale.z = 1.0f;
    scratch->transform.screen_scale.w = 1.0f;
    scratch->input[0].w = 1.0f;
    scratch->input[0].x = 0.0f;
    scratch->input[0].y = 0.0f;
    scratch->input[0].z = 0.0f;
    scratch->transform.local = matrix;
    scratch->input[1].x = vector->x;
    scratch->input[1].y = vector->y;
    scratch->input[1].z = vector->z;
    scratch->input[1].w = 1.0f;
    transformVector(scratch->output, scratch->input, 2, &scratch->transform);
    write = (unsigned int (*)[4])0x70000000;
    (*write)[0] = 0x8002;
    (*write)[1] = 0x2004c000;
    (*write)[2] = 0xffffff51;
    (*write)[3] = 0;
    write++;
    position = (Vector4 *)0x700004e0;
    for (vertex_index = 0; vertex_index < 2;
         vertex_index++, position++, color_index++) {
        (*write)[0] = col[color_index * 4];
        (*write)[1] = col[color_index * 4 + 1];
        (*write)[2] = col[color_index * 4 + 2];
        (*write)[3] = col[color_index * 4 + 3];
        write++;
        (*write)[0] = (unsigned int)(position->x * 16.0f);
        (*write)[1] = (unsigned int)(position->y * 16.0f);
        (*write)[2] = (unsigned int)(4294967296.0f - position->z);
        (*write)[3] = 0;
        write++;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 5);
}



INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawVector4S);

#include "main/xgl_2.h"

extern void xglMatrixStackLoad(Matrix4 matrix);
extern const Vector4 vGridXZ[5];
typedef struct DebugCursorColour {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} DebugCursorColour;

extern DebugCursorColour cCursor[6];

void drawGrid(Matrix4 matrix, float scale)
{
    Vector4 scale_vector;
    Matrix4 local_matrix;
    Matrix4 *local_matrix_pointer;
    StudioCamera *camera;
    StudioCamera *source_camera;
    DebugDrawTransform *params = (DebugDrawTransform *)0x70000400;
    Vector4 *position;
    unsigned int (*write)[4];
    int vertex_index;

    xglStudioGetCamera(&camera, 0);
    source_camera = camera;
    params->screen_offset = source_camera->screenOffset;
    __builtin_memcpy(params->world, source_camera->viewMatrix, sizeof(Matrix4));
    params->screen_scale = source_camera->screenScale;
    local_matrix_pointer = &local_matrix;
    __builtin_memcpy(&params->local, &local_matrix_pointer,
                     sizeof(local_matrix_pointer));
    scale_vector.x = scale;
    scale_vector.y = scale;
    scale_vector.z = scale;
    scale_vector.w = 1.0f;
    xglMatrixStackLoad(matrix);
    xglMatrixStackScale(&scale_vector.x);
    xglMatrixStackSave(local_matrix);
    transformVector((Vector4 *)0x700004e0, vGridXZ, 5, params);
    position = (Vector4 *)0x700004e0;
    for (vertex_index = 0; vertex_index < 5; vertex_index++, position++) {
        if (position->x > 2560.0f)
            return;
        if (position->x < 1536.0f)
            return;
        if (position->y > 2560.0f)
            return;
        if (position->y < 1536.0f)
            return;
        if (position->z < 0.0f)
            return;
    }
    position = (Vector4 *)0x700004e0;
    write = (unsigned int (*)[4])0x70000010;
    ((unsigned int *)0x70000000)[0] = 0x8005;
    ((unsigned int *)0x70000000)[1] = 0x20054000;
    ((unsigned int *)0x70000000)[2] = 0xffffff51;
    ((unsigned int *)0x70000000)[3] = 0;
    for (vertex_index = 0; vertex_index < 5; vertex_index++, position++) {
        (*write)[0] = cCursor[0].red;
        (*write)[1] = cCursor[0].green;
        (*write)[2] = cCursor[0].blue;
        (*write)[3] = cCursor[0].alpha;
        write++;
        (*write)[0] = (unsigned int)(position->x * 16.0f);
        (*write)[1] = (unsigned int)(position->y * 16.0f);
        (*write)[2] = (unsigned int)position->z;
        (*write)[3] = 0;
        write++;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 11);
}

static StudioCamera *getCurrentCamera(void)
{
    int camera_index;
    StudioCamera *camera;

    camera_index = 0;
    for (;;) {
        xglStudioGetCamera(&camera, camera_index);
        camera_index++;
        if (camera->active != 0)
            return camera;
        if (camera_index >= 8)
            return 0;
    }
}

extern const Vector4 vCursor[6];

void drawAxis(Matrix4 matrix, float scale)
{
    Vector4 scale_vector;
    Matrix4 local_matrix;
    Matrix4 *local_matrix_pointer;
    StudioCamera *camera;
    DebugDrawTransform *params = (DebugDrawTransform *)0x70000400;
    Vector4 *position;
    unsigned int (*write)[4];
    int vertex_index;

    camera = getCurrentCamera();
    params->screen_offset = camera->screenOffset;
    __builtin_memcpy(params->world, camera->viewMatrix, sizeof(Matrix4));
    params->screen_scale = camera->screenScale;
    local_matrix_pointer = &local_matrix;
    __builtin_memcpy(&params->local, &local_matrix_pointer,
                     sizeof(local_matrix_pointer));
    scale_vector.x = scale;
    scale_vector.y = scale;
    scale_vector.z = scale;
    scale_vector.w = 1.0f;
    xglMatrixStackLoad(matrix);
    xglMatrixStackScale(&scale_vector.x);
    xglMatrixStackSave(local_matrix);
    transformVector((Vector4 *)0x700004e0, vCursor, 6, params);
    position = (Vector4 *)0x700004e0;
    if (position->x > 2560.0f)
        return;
    if (position->x < 1536.0f)
        return;
    if (position->y > 2560.0f)
        return;
    if (position->y < 1536.0f)
        return;
    if (position->z < 0.0f)
        return;
    ((unsigned int *)0x70000000)[0] = 0x8006;
    ((unsigned int *)0x70000000)[1] = 0x2004c000;
    ((unsigned int *)0x70000000)[2] = 0xffffff51;
    ((unsigned int *)0x70000000)[3] = 0;
    write = (unsigned int (*)[4])0x70000010;
    for (vertex_index = 0; vertex_index < 6; vertex_index++, position++) {
        (*write)[0] = cCursor[vertex_index].red;
        (*write)[1] = cCursor[vertex_index].green;
        (*write)[2] = cCursor[vertex_index].blue;
        (*write)[3] = cCursor[vertex_index].alpha;
        write++;
        (*write)[0] = (unsigned int)(position->x * 16.0f);
        (*write)[1] = (unsigned int)(position->y * 16.0f);
        (*write)[2] = 0x7fffffff;
        (*write)[3] = 0;
        write++;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 13);
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawCircle3);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawTags);


INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawVector4);

float ball2point(const Vector4 *cursor_position,
                 const Vector4 *actor_position, float radius)
{
    Vector4 difference;
    float distance;

    difference.x = cursor_position->x - actor_position->x;
    difference.y = cursor_position->y - actor_position->y;
    difference.z = cursor_position->z - actor_position->z;
    xglVectorLength(&distance, &difference);
    if (distance < radius)
        return distance;
    return -1.0f;
}

void *prevActor(int startIndex)
{
    int i;

    for (i = startIndex; i >= 0; i--) {
        if (actor[i].inUseId != 0 && !(actor[i].flags & 8)) {
            return &actor[i];
        }
    }
    return 0;
}

ActorHead *nextActor(register int startIndex)
{
    register ActorHead *entry;
    ActorHead *end;

    if (startIndex < ACTOR_COUNT) {
        startIndex *= sizeof(ActorHead);
        entry = (ActorHead *)((char *)actor + startIndex);
        end = &actor[ACTOR_COUNT];
        for (;;) {
            if (entry->inUseId != 0 && !(entry->flags & 8)) {
                return entry;
            }
            entry++;
            if ((int)entry >= (int)end) {
                break;
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode1);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode2);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorModePlane);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode0);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", changeCameraMode);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursor);

void drawCursor(void)
{
    Matrix4 matrix;
    HomogeneousVector *position;

    position = &cursor[1];
    position->w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&position->x);
    xglMatrixStackSave(matrix);
    drawAxis(matrix, 1.0f);
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initCursor);

static void printM(int x, int y, const float *matrix)
{
    char buffer[256];
    const float *rowEnd;
    int rowIndex;

    rowEnd = matrix + 3;
    rowIndex = 0;
    do {
        rowIndex++;
        sprintf(buffer, D_004C2158,
                rowEnd[-3], rowEnd[-2], rowEnd[-1], rowEnd[0]);
        xglFontDebugPrintf(x, y, buffer);
        if (rowIndex > 3)
            break;
        rowEnd += 4;
        y += 8;
    } while (1);
}

void MAP_serach(void)
{
    MapStageModelView *model;
    MapPartListView *parts;
    DbLightWritePadSecondPrefix *mapPad;
    unsigned char *matrix_base;
    void *matrix_data;
    char buffer[256];

    model = GameLoopState.model;
    mapPad = &PadData.second;
    visible_0++;
    if (model != 0) {
        parts = model->parts;
        if (parts != 0) {
            matrix_base = (unsigned char *)parts +
                          parts->matrix_data_offset;
            if (mapPad->half_2c & 0x4000) {
                nmlModelSetPartsVisible(parts, partsIndex, 1);
                visible_0 = 0;
                partsIndex--;
            }

            if (mapPad->half_2c & 0x1000) {
                nmlModelSetPartsVisible(model->parts,
                                        partsIndex, 1);
                visible_0 = 0;
                partsIndex++;
            }

            if (partsIndex < 0)
                partsIndex = 0;
            matrix_data = matrix_base + partsIndex * sizeof(Matrix4);
            printM(0x20, 0x40, matrix_data);
            sprintf(buffer, D_004DA520, partsIndex);
            xglFontDebugPrintf(8, 0x20, buffer);
            nmlModelSetPartsVisible(model->parts, partsIndex,
                                    (visible_0 >> 4) & 1);
        }
    }
}

void EvtTools(void)
{
    if ((PadData.half_2a & 0x10) && (PadData.half_28 & 0x100)) {
        mode_004DC5A8 = (mode_004DC5A8 + 1) & 1;
    }
    MAP_serach();
    if (mode_004DC5A8 == 1) {
        JTHREAD_cntl();
        PLAY_ctrl();
    }
    TCAMERA_update();
    if (mode_004DC5A8 == 1) {
        ACT_update();
        MAP_updateUnit();
    }
    updateCursor(0);
    drawCursor();
}









INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initLight3);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initLight2);

void initLight(void)
{
    StudioLight *light;
    Vector4 direction;
    int lightIndex;

    xglStudioGetLight(&light);
    lightIndex = 0;
    do {
        Vector4 *source = &lightDir[lightIndex];

        if (__builtin_sqrtf((source->x * source->x) +
                            (source->y * source->y)) > 1.0f) {
            float angle = atan2f(source->x, source->y);
            source->x = xglSin(angle);
            source->y = xglCos(angle);
        }

        direction.x = source->x;
        direction.y = source->y;
        direction.z = __builtin_sqrtf(
            1.0f - ((direction.x * direction.x) +
                    (direction.y * direction.y)));
        if (source->z < 0.0f)
            direction.z = -direction.z;

        xglLightIntensityParallel(light, lightIndex, &lightCol[lightIndex]);
        xglLightDirection(light, lightIndex, &direction);
    } while (++lightIndex < 3);

    xglLightIntensityAmbient(light, &lightCol[3]);
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateLight);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateWind);

void VW_setCursorMode(int mode)
{
    CursorState *state = (void *)cursor;

    state->mode = mode;
    changeCameraMode();
}

void VW_setCursorFunc(CursorCallback callback, void *argument)
{
    CursorState *state = (void *)cursor;

    state->callback = callback;
    state->callback_argument = argument;
}

void VW_setCursor(const Vector4 *position)
{
    cursor[1].x = position->x;
    cursor[1].y = position->y;
    cursor[1].z = position->z;
}

void VW_getCursor(HomogeneousVector *destination)
{
    destination->x = cursor[1].x;
    destination->y = cursor[1].y;
    destination->z = cursor[1].z;
    destination->w = 1.0f;
}

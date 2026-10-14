/*
 * OV01 original TU 25: 0x00a34e90..0x00a358a0 (7 functions)
 */
#include "common.h"
#include "ov01/m_ef_create.h"
#include "shared.h"
#include "m_ef_create_smp_01.h"

extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from,
                          const Vector4 *to);
extern void *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                              const Vector4 *point);

extern void MEfGetActorMatrix(Vector4 *destination, int actor, int part);
extern void MEfGetActorCoord(Vector4 *destination, int actor, int part);
extern Vector4 *MEfCalcCircumXZ(Vector4 *destination, const Vector4 *center,
                                float radius);
extern float MMathMakeRandom2PI(void);
extern void MGsGPInit(Smp01Packet *packet, void *address, int size);
extern Smp01GsParameter *svGetPrmFromName(const char *name,
                                          Smp01GsParameter *output);
extern const char D_00A51480[];
extern const char D_00A51490[];

static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work);
static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work);
static void fnSMP01_DP000(Smp01Object *self, Smp01Work *work);
static void fnSMP01_PO000(void *object, void *work);

/* Place the smoke at the actor, aim it at a point on a circle around the
 * target and reset the smoke trails. */
int MEfCreate_SMP01(MEfObjRecord *self)
{
    /* This constructor interprets its own variant of the pooled work. */
    Smp01Object *object = (Smp01Object *)self;
    Smp01Work *work = &object->work;
    Smp01GsParameter parameter;
    Vector4 actorCoordinate;
    Vector4 actorMatrix[4];

    work->frame = 0;
    work->phase = 0;
    work->phaseCount = 0;
    MEfGetActorMatrix(actorMatrix, work->sourceActor, work->sourcePart);
    MMathApplyMatrix(&work->spawnPoint, actorMatrix, &work->sourceOffset);
    MEfGetActorCoord(&actorCoordinate, work->coordinateActor, -1);
    MEfCalcCircumXZ(&work->targetPosition, &actorCoordinate, 1.0f);
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->position), "r"(&work->spawnPoint)
                         : "$8", "memory");
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->previousPosition), "r"(&work->spawnPoint)
                         : "$8", "memory");
    MEfCalcAngle(&work->angles, &work->targetPosition, &work->spawnPoint);
    work->phaseAngle = MMathMakeRandom2PI();

    MGsGPInit(&work->packet, 0, 0);
    svGetPrmFromName(D_00A51480, &parameter);
    work->smokeTexture0First = parameter.texture0;
    svGetPrmFromName(D_00A51490, &parameter);
    work->smokeTexture0Second = parameter.texture0;
    memset(work->smokeActive, 0, sizeof(work->smokeActive));
    memset(work->smokePoints, 0, sizeof(work->smokePoints));
    memset(work->smokeAngles, 0, sizeof(work->smokeAngles));

    object->updateCallback = fnSMP01_PR000;
    object->drawCallback = fnSMP01_DM000;
    object->packetCallback = fnSMP01_DP000;
    object->postCallback = fnSMP01_PO000;
    return 1;
}

extern float MMathMakeRandom(void);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);

/* Smoke is emitted for as many frames as the model is drawn. */
enum { SMP01_DRAW_FRAME_LIMIT = 15 };

/* Age every smoke trail by one slot and, while the model is still drawn,
 * emit a fresh trail of jittered points along the last movement step. */
static void updateSmoke(Smp01Work *work)
{
    Vector4 matrix[4];
    Vector4 offset;
    int trail;
    int point;

    for (trail = SMP01_SMOKE_TRAILS - 2; trail >= 0; trail--) {
        work->smokeActive[trail + 1] = work->smokeActive[trail];
        for (point = 0; point < SMP01_SMOKE_POINTS; point++) {
            __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                                 :
                                 : "r"(&work->smokePoints[trail + 1][point]),
                                   "r"(&work->smokePoints[trail][point])
                                 : "$8", "memory");
            work->smokeAngles[trail + 1][point] = work->smokeAngles[trail][point];
        }
    }

    work->smokeActive[0] = 0;
    for (point = 0; point < SMP01_SMOKE_POINTS; point++) {
        __asm__ __volatile__("sqc2 vf0, 0(%0)"
                             :
                             : "r"(&work->smokePoints[0][point])
                             : "memory");
    }

    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        work->smokeActive[0] = 1;
        for (point = 0; point < SMP01_SMOKE_POINTS; point++) {
            MMathVectorInterpolation(&offset, &work->position,
                                     &work->previousPosition,
                                     (float)point / 3.0f);
            MMathRotateMatrixYX(matrix, 0, &work->angles);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&offset) : "memory");
            __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
            offset.x = MMathMakeRandom() * 0.06f - 0.03f;
            offset.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&work->smokePoints[0][point], matrix, &offset);
            work->smokeAngles[0][point] = MMathMakeRandom2PI();
        }
    }
}

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work);

static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work)
{
    Vector4 offset;
    Vector4 matrix[4];
    int phase;
    float sine;
    float cosine;

    phase = work->phase;
    work->phase = phase + 1;
    work->phaseCount = work->phaseCount - phase + 5;

    __asm__ __volatile__("lq $8, 0(%1)\n\t"
                         "sq $8, 0(%0)"
                         :
                         : "r"(&work->previousPosition), "r"(&work->position)
                         : "$8", "memory");
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0x20\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(sine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.x = (float)work->phase * 0.06f * sine;

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0xe8\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(cosine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.y = (float)work->phase * 0.06f * cosine;
    offset.z = (float)work->phaseCount * -0.02f;

    MMathRotateMatrixYX(matrix, 0, &work->angles);
    {
        /* Keep the translation load's address in v1, as at 0x00a352ec. */
        register Vector4 *spawnPoint asm("$3") = &work->spawnPoint;

        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(spawnPoint) : "memory");
    }
    __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
    MMathApplyMatrix(&work->position, matrix, &offset);

    if (work->phase >= 4) {
        updateSmoke(work);
    }

    if (work->phase >= 5) {
        work->phase = 0;
        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->phaseStart), "r"(&work->position)
                             : "$8", "memory");
        MEfCalcAngle(&work->angles, &work->targetPosition, &work->phaseStart);
        object->updateCallback = fnSMP01_PR010;
    }
}

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work)
{
    (void)object;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        float phase;

        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->previousPosition), "r"(&work->position)
                             : "$8", "memory");
        phase = ((float)(work->phase + 1) / 10.0f) * 1.5707964f + 4.712389f;
        __asm__ __volatile__("mfc1 $8, %0\n\t"
                             "qmtc2.ni $8, vf4\n\t"
                             "vcallms 0x20\n\t"
                             "qmfc2.i $8, vf1\n\t"
                             "mtc1 $8, %0"
                             : "+f"(phase)
                             :
                             : "$8", "memory");
        MMathVectorInterpolation(&work->position, &work->phaseStart,
                                 &work->targetPosition, phase + 1.0f);
        work->angles.z += 0.5235988f;
    }
    updateSmoke(work);
    work->phase++;
}

extern Vector4 *MMathRotateMatrixYXZ(Vector4 *out, const Vector4 *matrix, const Vector4 *angles);
extern Vector4 *MMathScaleMatrix(Vector4 *out, const Vector4 *matrix, const Vector4 *scale);
extern void MEfDrawModel(const Vector4 *place, int entry, const char *texture);
static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work)
{
    static const Vector4 scale = { 0.038f, 0.038f, 0.15f, 1.0f };
    Vector4 matrix[4];

    (void)self;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        MMathRotateMatrixYXZ(matrix, 0, &work->angles);
        MMathScaleMatrix(matrix, matrix, &scale);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&work->position) : "memory");
        __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
        __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
        MEfDrawModel(matrix, work->modelEntry, work->textureName);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_ef_create_smp_01", fnSMP01_DP000);

extern void sefHitEffect(void);

static void fnSMP01_PO000(void *object, void *work)
{
    Smp01State *state = (Smp01State *)work;

    state->frame++;
    if (state->frame == 15) {
        sefHitEffect();
    }
    if (state->frame >= 25) {
        MEfObjDestroy(object);
    }
}

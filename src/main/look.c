#include "common.h"
#include "look.h"

                                              /* MARK provisional: .lit4 0x004d8564 */

extern StudioCamera *xglStudioGetActiveCamera(void);

typedef struct LookEnemyPlace {
    unsigned long long words[2];
} LookEnemyPlace;

typedef struct LookEnemyTargetSlot {
    unsigned char unmodeled_000[0x0c];
    LookEnemyPlace *lookPlaceTarget;
} LookEnemyTargetSlot;

typedef struct LookEnemyWorkEntry {
    unsigned char unmodeled_000[0x3860];
    LookEnemyTargetSlot targetSlot;
    unsigned char unmodeled_3870[0x40];
} LookEnemyWorkEntry;

extern LookEnemyWorkEntry enepc[16];

static inline void look_copy_place(LookAlignedVector4 *place, const Vector4 *source)
{
    __asm__ __volatile__("lq $2, 0(%0)\n"
                         "sq $2, 0(%1)\n"
                         : : "r" (source), "r" (place) : "$2", "memory");
}

float xglAsin(float value)
{
    /* MARK: explicit NaN-fallback call structure -- emits sqrt.s + c.eq.s/bc1t
     * plus jal sqrtf@0x003240f8 on the NaN path; a plain sqrtf() call cannot
     * reproduce this (S2 evidence). */
    float root = __builtin_sqrtf(1.0f - value);
    float result;
    float cubic = xglAsin_cubic;
    float quadratic = xglAsin_quadratic;
    float linear = xglAsin_linear;
    float pi_over_two_approx = xglAsin_pi_over_two_approx;

    result = value * cubic;
    result += quadratic;
    result *= value;
    result -= linear;
    result *= value;
    result += pi_over_two_approx;
    return xglAsin_pi_over_two - root * result;
}

void look_limit(float *angle, float limit)
{
    /* MARK: __builtin_fabsf required -- emits inline abs.s, no call (S1: plain
     * fabsf() adds jal fabsf and mismatches). */
    if (__builtin_fabsf(*angle) < limit) {
        *angle = 0.0f;
        return;
    }
    if (limit <= *angle) {
        *angle -= limit;
    }
    if (*angle <= -limit) {
        *angle += limit;
    }
}

void look_limit_add(float *angle, float target, float step)
{
    /* MARK: __builtin_fabsf required -- emits inline abs.s, no call (S1). */
    float delta = *angle - target;

    if (__builtin_fabsf(delta) < step) {
        *angle = target;
        return;
    }
    if (step <= delta) {
        *angle -= step;
    }
    delta = *angle - target;
    if (delta <= -step) {
        *angle += step;
    }
}

static int look_get_place(LookCharacter *actor, LookAlignedVector4 *place)
{
    LookCharacter *target;
    int result = 1;
    LookEnemyTargetSlot *enemyEntry = 0;
    unsigned short mode = actor->lookMode;

    if (actor->number < 16)
        enemyEntry = &enepc[actor->number].targetSlot;

    switch (mode) {
    case 1: {
        LookEnemyPlace *enemyPlace;

        if (enemyEntry != 0) {
            enemyPlace = enemyEntry->lookPlaceTarget;
            if (enemyPlace != 0) {
                result = 0;
                *(LookEnemyPlace *)place = *enemyPlace;
            }
        }
        break;
    }
    case 2:
        look_copy_place(place, &xglStudioGetActiveCamera()->position);
        result = 0;
        break;
    case 3:
        look_copy_place(place, &actor->worldPosition);
        result = 0;
        break;
    case 4: {
        LookModelRecord *model;

        target = actor->lookTarget;
        if (target == 0)
            break;
        model = target->model;
        if (model != 0) {
            if (model->detail >= 8)
                look_copy_place(place, &target->headPosition);
            else
                look_copy_place(place, &target->nearPosition);
        }
        result = 0;
        break;
    }
    case 5:
        target = actor->lookTarget;
        if (target == 0)
            break;
        look_copy_place(place, &target->eyePosition);
        result = 0;
        break;
    }
    return result;
}

void LOOK_target_init(void)
{
}

void LOOK_target_doit(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/look", LOOK_sebo_cont);

int look_get_place(LookCharacter *actor, LookAlignedVector4 *place);

void *LOOK_head_cont(LookJointWork *work, void *channels, LookCharacter *actor)
{
    Matrix4 matrix;
    Matrix4 rootMatrix;
    LookAlignedVector4 place;
    Matrix4 currentMatrix;
    Vector4 rotation;
    Vector4 localPlace;
    LookJointElement *element;
    float weight;
    float horizontalDistance;

    if (actor != 0) {
        weight = actor->lookWeight;
        actor->headFlags &= ~0x80U;
        element = &work->elements[work->jointIndex];
        if (!(__builtin_fabsf(weight) < D_004D8578)) {
            if (weight <= 0.0f)
                weight = 1.0f;
            CUR_MATRIX_Set(work->matrixBuffer[work->jointIndex - 1]);
            CUR_MATRIX_Txyz4s(element->translation);
            CUR_MATRIX_Rzyx4s(element->rotation);
            CUR_MATRIX_Get(currentMatrix);
            MATRIX_convert4(matrix, currentMatrix);
            MATRIX_convert4(rootMatrix, work->rootMatrix);
            xglMatrixMul(matrix, rootMatrix, matrix);
            /* The SDK copies the joint's translated row through one scratch register. */
            __asm__ __volatile__("lq $2, 0(%1)\n"
                                 "sq $2, 0(%0)\n"
                                 : : "r" (&actor->headPosition), "r" (matrix[3])
                                 : "$2", "memory");
            actor->headPositionValid = 1;
            actor->headPosition.y += D_004D857C;
            if (look_get_place(actor, &place) == 0) {
                place.v.w = 1.0f;
                __asm__ __volatile__("sq $0, 0(%0)\n"
                                     : : "r" (&rotation) : "memory");
                xglMatrixInverse(matrix, matrix);
                xglVectorMulMat(&localPlace, matrix, &place.v);
                rotation.y = xglAtan2(localPlace.z, localPlace.x);
                horizontalDistance = __builtin_sqrtf(localPlace.z * localPlace.z +
                                                     localPlace.x * localPlace.x);
                rotation.z = xglAtan2(localPlace.y, horizontalDistance);
                if (rotation.z > 0.0f) {
                    if (rotation.z > D_004D8580)
                        rotation.z = D_004D8580;
                } else if (rotation.z < D_004D8584) {
                    rotation.z = D_004D8584;
                }
                if (rotation.y > 0.0f) {
                    if (rotation.y > D_004D8588)
                        rotation.y = D_004D8588;
                } else if (rotation.y < D_004D858C) {
                    rotation.y = D_004D858C;
                }
                rotation.y *= D_004D8590;
                rotation.z *= D_004D8594;
                look_limit_add(&actor->lookAngles[1].y, rotation.y, weight * D_004D8598);
                look_limit_add(&actor->lookAngles[1].z, rotation.z, weight * D_004D859C);
            } else {
                look_limit(&actor->lookAngles[1].y, weight * D_004D85A0);
                look_limit(&actor->lookAngles[1].z, weight * D_004D85A4);
            }
            xglMatrixUnit(matrix);
            xglMatrixRotX(matrix, matrix, actor->lookAngles[1].z);
            xglMatrixRotY(matrix, matrix, actor->lookAngles[1].y);
            element->rotation[0] += xglAtan2(matrix[1][2], matrix[2][2]);
            element->rotation[1] -= xglAsin(matrix[0][2]);
            element->rotation[2] += xglAtan2(matrix[0][1], matrix[0][0]);
        }
    }
    return channels;
}


void LOOK_eye_cont(int model, int target, int step, int side);
static float s_fEyeOffset = 0.0f;

void LOOK_eyeL_cont(int model, int target, int step)
{
    s_fEyeOffset = D_004D85A8;
    LOOK_eye_cont(model, target, step, 0);
}

void LOOK_eyeR_cont(int model, int target, int step)
{
    s_fEyeOffset = D_004D85AC;
    LOOK_eye_cont(model, target, step, 1);
}

INCLUDE_ASM("asm/main/nonmatchings/look", LOOK_eye_cont);

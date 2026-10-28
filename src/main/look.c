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

static inline void look_zero_vector(Vector4 *destination)
{
    __asm__ __volatile__("sq $0, 0(%0)"
                         : : "r" (destination) : "memory");
}

static inline void look_copy_matrix(Matrix4 destination, Matrix4 source)
{
    __asm__ __volatile__("lq $2, 0(%1)\n"
                         "sq $2, 0(%0)\n"
                         "lq $2, 16(%1)\n"
                         "sq $2, 16(%0)\n"
                         "lq $2, 32(%1)\n"
                         "sq $2, 32(%0)\n"
                         "lq $2, 48(%1)\n"
                         "sq $2, 48(%0)\n"
                         : : "r" (destination), "r" (source) : "$2", "memory");
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

void *LOOK_sebo_cont(LookJointWork *work, void *channels, LookCharacter *actor)
{
    Matrix4 matrix;
    Matrix4 rootMatrix;
    LookAlignedVector4 place;
    Vector4 localPlace;

    if (actor != 0) {
        float weight = actor->lookWeight;
        LookJointElement *element = &work->elements[work->jointIndex];

        if (!(__builtin_fabsf(weight) < D_004D8568)) {

            do {
            if (weight <= 0.0f)
                weight = 1.0f;
            {
                int placeStatus = look_get_place(actor, &place);

                if (placeStatus == 0) {
                    float *angle;

                    place.v.w = 1.0f;
                    look_copy_matrix(matrix, work->matrixBuffer[work->jointIndex - 1]);
                    look_copy_matrix(rootMatrix, work->rootMatrix);
                    matrix[0][3] = 0.0f;
                    matrix[1][3] = 0.0f;
                    matrix[2][3] = 0.0f;
                    rootMatrix[0][3] = 0.0f;
                    rootMatrix[1][3] = 0.0f;
                    rootMatrix[2][3] = 0.0f;
                    xglMatrixMul(matrix, rootMatrix, matrix);
                    xglMatrixInverse(matrix, matrix);
                    {
                        Vector4 targetAngle;

                        look_zero_vector(&targetAngle);
                        xglVectorMulMat(&localPlace, matrix, &place.v);
                        angle = &actor->lookAngles[0].y;
                        targetAngle.y = xglAtan2(localPlace.x, localPlace.z);
                        look_limit_add(angle, targetAngle.y, weight * D_004D8570);
                    }
                    element->rotation[2] += *angle * D_004D8574;
                } else {
                    float *angle = &actor->lookAngles[0].y;

                    look_limit(angle, weight * D_004D856C);
                    element->rotation[2] += *angle * D_004D8574;
                }
            }
            } while (0);
        }
    }
    return channels;
}



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


void *LOOK_eye_cont(LookJointWork *work, void *channels, LookCharacter *actor, int side);
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

enum LookEyeComponent { EYE_YAW, EYE_PITCH, EYE_DURATION, EYE_STEP };

extern float s_aEyeIdling[22][4];
extern float s_aEyeControl[15][4];
extern float s_aEyeDefault[1][4];
extern const char D_004DC258[];
extern const char D_004DC260[];
extern const char D_004DC268[];
extern char *strstr(const char *haystack, const char *needle);

void *LOOK_eye_cont(LookJointWork *work, void *channels, LookCharacter *actor, int side)
{
    Matrix4 jointMatrix;
    Matrix4 rootMatrix;
    LookAlignedVector4 targetPlace;
    Vector4 localPlace;
    Vector4 angles;
    LookJointElement *element;
    LookEyeState *eyes;
    LookAngleView *view;
    float eyeWeight;
    float yawScale;
    float pitchScale;
    float horizontalDistance;
    float step;
    int controlled;
    unsigned int mode;
    LookCharacter *owner;
    LookModelRecord *model;

    owner = actor->eyeOwner;
    if (owner != 0)
        actor = owner;
    if (actor != 0) {
    eyeWeight = actor->worldPosition.w;
    controlled = 0;
    element = work->elements + work->jointIndex;
    eyes = actor->eyes;
    view = (LookAngleView *)actor;
    pitchScale = 1.0f;
    yawScale = pitchScale;
    if (eyeWeight <= 0.0f)
        eyeWeight = 1.0f;
    model = actor->model;
    if (model != 0) {
        const char *name = model->name;
        if (strstr(name, D_004DC258) ||
            strstr(name, D_004DC260) ||
            strstr(name, D_004DC268)) {
            pitchScale *= D_004D85B0;
            yawScale *= D_004D85B0;
        }
    }
    mode = actor->eyeMode;
    switch (mode) {
    case 1:
        look_limit_add(&eyes[side].yaw, s_aEyeIdling[eyes[side].frame][EYE_YAW],
                       s_aEyeIdling[eyes[side].frame][EYE_STEP]);
        look_limit_add(&eyes[side].pitch, s_aEyeIdling[eyes[side].frame][EYE_PITCH],
                       s_aEyeIdling[eyes[side].frame][EYE_STEP]);
        eyes[side].timer++;
        if (eyes[side].timer >= (int)s_aEyeIdling[eyes[side].frame][EYE_DURATION]) {
            eyes[side].timer = 0;
            eyes[side].frame++;
            if (s_aEyeIdling[eyes[side].frame][EYE_DURATION] < 0.0f)
                eyes[side].frame = 0;
        }
        controlled = 1;
        break;
    case 14:
        yawScale = D_004D85B4;
        yawScale = eyeWeight * yawScale;
        look_limit_add(&eyes[side].yaw, actor->eyeYaw, yawScale);
        look_limit_add(&eyes[side].pitch, actor->eyePitch, yawScale);
        controlled = 1;
        break;
    case 7:
    case 8:
    case 9:
        if (side != 0)
            yawScale = -yawScale;
        /* fall through */
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 10:
    case 11:
    case 12:
        look_limit_add(&eyes[side].yaw,
                       s_aEyeControl[mode][EYE_YAW] * yawScale,
                       s_aEyeControl[mode][EYE_STEP]);
        look_limit_add(&eyes[side].pitch,
                       s_aEyeControl[actor->eyeMode][EYE_PITCH] * pitchScale,
                       s_aEyeControl[actor->eyeMode][EYE_STEP]);
        controlled = 1;
        break;
    case 13:
        controlled = 1;
        break;
    case 0:
        break;
    }
    if (!controlled) {
        look_limit_add(&eyes[side].yaw, s_aEyeDefault[0][EYE_YAW], s_aEyeDefault[0][EYE_STEP]);
        look_limit_add(&eyes[side].pitch, s_aEyeDefault[0][EYE_PITCH], s_aEyeDefault[0][EYE_STEP]);
    }
    if (look_get_place(actor, &targetPlace) == 0) {
        targetPlace.v.w = 1.0f;
        look_copy_matrix(jointMatrix, work->matrixBuffer[work->jointIndex - 1]);
        look_copy_matrix(rootMatrix, work->rootMatrix);
        jointMatrix[0][3] = 0.0f;
        jointMatrix[1][3] = 0.0f;
        jointMatrix[2][3] = 0.0f;
        rootMatrix[0][3] = 0.0f;
        rootMatrix[1][3] = 0.0f;
        rootMatrix[2][3] = 0.0f;
        xglMatrixMul(jointMatrix, rootMatrix, jointMatrix);
        xglMatrixInverse(jointMatrix, jointMatrix);
        xglVectorMulMat(&localPlace, jointMatrix, &targetPlace.v);
        localPlace.z += s_fEyeOffset;
        angles.y = xglAtan2(localPlace.z, localPlace.x);
        horizontalDistance = __builtin_sqrtf(localPlace.z * localPlace.z +
                                             localPlace.x * localPlace.x);
        step = D_004D85B8;
        step = eyeWeight * step;
        angles.z = xglAtan2(localPlace.y, horizontalDistance);
        look_limit_add(&view->lookAngles[side][0], angles.y, step);
        look_limit_add(&view->lookAngles[side][3], angles.z, step);
        element->rotation[1] -= view->lookAngles[side][0] * D_004D85C0 * D_004D85C4;
        element->rotation[0] += view->lookAngles[side][3] * D_004D85C0 * D_004D85C0;
    } else {
        step = D_004D85BC;
        step = eyeWeight * step;
        look_limit(&view->lookAngles[side][0], step);
        look_limit(&view->lookAngles[side][3], step);
        element->rotation[1] -= view->lookAngles[side][0] * D_004D85C0 * D_004D85C4;
        element->rotation[0] += view->lookAngles[side][3] * D_004D85C0 * D_004D85C0;
    }
    element->rotation[1] += view->eyeAngles[side][2];
    element->rotation[0] += view->eyeAngles[side][3];
    }
    return channels;
}

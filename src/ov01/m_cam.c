/*
 * OV01 original TU 17: 0x00a314b8..0x00a32db8 (36 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/data_unit_org_get.h"
#include "main/act_1.h"

extern void MOutputDebugStringWarn(const char *format, ...);
/* The message body after the ASCII prefix is encoded in EUC-JP. */
const char D_00A50EF0[48] =
    "MCamReleaseControl: \xA5\xAB\xA5\xE1\xA5\xE9\xA4\xCF\xC0\xA9\xB8\xE6\xC3\xE6\xA4\xC7\xA4\xCF\xA4\xCA\xA4\xA4";
const char D_00A51000[40] = "MCamGetAtkRange: Invalid ID (%d)";
const char D_00A51028[40] = "MCamGetDefRange: Invalid ID (%d)";
const char zero_duration_message[40] = "MCamMoveProc_CalcRatio: d == 0 (IP:%d)";
const char invalid_interpolation_message[40] = "MCamMoveProc_CalcRatio: Invalid IPTYPE";

extern const char D_00A50F98[]; /* "MCamStopMove: Invalid point" */
extern const char D_00A50FB8[]; /* "MCamIsMoving: Invalid point" */
extern const char D_00A50FD8[]; /* "MCamGetActorCoord: actor is NULL" */

extern void bcopy(const void *source, void *destination, unsigned int count);
/*
 * Linear interpolation state shared by the per-parameter MoveProc callbacks
 * (mode/duration/elapsed feed MCamMoveProc_CalcRatio; start/end/current hold
 * the eased scalar value).
 */
typedef struct MCamMoveState {
    unsigned int mode;
    unsigned int duration;
    unsigned int elapsed;
    float start;
    float end;
    float current;
} MCamMoveState;

/*
 * Per-point camera parameters. Only the coordinate-offset block that
 * MCamCalcCoord applies after MCamCalcBaseCoord is recovered.
 */
typedef struct MCamCoordOffset {
    unsigned int mode;
    unsigned char unmodeled_04[0xC];
    Vector4 offset;
    float distance;
    float pitch;
    float yaw;
    unsigned char unmodeled_2C[0x14];
} MCamCoordOffset;

typedef struct MCamPointParams {
    unsigned char unmodeled_00[0x70];
    MCamCoordOffset coordOffset;
} MCamPointParams;

/*
 * MCamInit clears this complete 0x5B0-byte camera-parameter store. The two
 * point records, the derived base coordinates, coordinates, angles, eye and
 * rotation are the views MCamExec_SetParams_Base and the getters address; the
 * other bytes remain part of the live store but their individual meanings are
 * not recovered yet.
 */
typedef struct MCamParameterStorage {
    MCamPointParams point[2];
    Vector4 angle[2];
    Vector4 baseCoord[2];
    float actorLength[2];
    unsigned char unmodeled_1A8[0x8];
    Vector4 coord[2];
    Vector4 eye;
    Vector4 rotation;
    float bank;
    float perspectiveStart;
    unsigned char unmodeled_1F8[0x380];
    MCamMoveState perspectiveMove;
    unsigned char unmodeled_590[0x20];
} MCamParameterStorage;

extern Vector4 *MMathCalcOffset(Vector4 *destination, const Vector4 *first, const Vector4 *second);
extern Vector4 *MMathCalcAngleCam(Vector4 *out, Vector4 *source, Vector4 *target);
extern void sefGetNullPosition(Vector4 *position, void *actor, int part, int flag);
extern float fmodf(float value, float modulus);
extern Vector4 *MMathCalcOffsetXYZ(Vector4 *destination, const Vector4 *first, const Vector4 *second);

static void *sysCam;
/* MCamTakeControl saves the StudioCamera returned by xglStudioGetCamera2. */
static StudioCamera sysCamBuff;
static int camFlags;
static MCamParameterStorage camParams;

/* Original local labels are interior views in camParams, not extra storage. */
#define D_00A5B220 (camParams.actorLength[0])
#define D_00A5B224 (camParams.actorLength[1])
#define D_00A5B5F8 (camParams.perspectiveMove)

#define CAM_FLAG_ACTIVE 0x1
#define CAM_FLAG_DUMP_DISABLED 0x10
#define CAM_FLAG_MOVE_COORD 0x100
#define CAM_FLAG_MOVE_TARGET 0x200
#define CAM_FLAG_MOVE_BANK 0x400
#define CAM_FLAG_MOVE_PERS 0x800
#define CAM_SHAKE_BIT 12

void *MCamGetSysCam(void)
{
    return sysCam;
}

/*
 * R5900 native 128-bit vector copy (lq/sq through $8). The getters below copy
 * through this one inline helper: the original shares a single lq/sq tail
 * between both cases, which gcc's cross-jumping only produces when the two
 * inlined asm statements come from the same source line.
 */
static __inline__ void MCamCopyVector(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(destination), "r"(source)
                         : "$8", "memory");
}

Vector4 *MCamGetCurrentBaseCoord(Vector4 *baseCoord, int point)
{
    __asm__ __volatile__("sq $0,0(%0)" : : "r"(baseCoord) : "memory");
    switch (point) {
    case 0:
        MCamCopyVector(baseCoord, &camParams.baseCoord[0]);
        break;
    case 1:
        MCamCopyVector(baseCoord, &camParams.baseCoord[1]);
        break;
    }
    return baseCoord;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamGetCurrentCoord);

Vector4 *MCamGetCurrentAngle(Vector4 *angle, int point)
{
    __asm__ __volatile__("sq $0,0(%0)" : : "r"(angle) : "memory");
    switch (point) {
    case 0:
        MCamCopyVector(angle, &camParams.angle[0]);
        break;
    case 1:
        MCamCopyVector(angle, &camParams.angle[1]);
        break;
    }
    return angle;
}

/* Vector copies below are the R5900 native 128-bit lq/sq pair through $8. */
Vector4 *MCamGetCurrentEye(Vector4 *eye)
{
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(eye), "r"(&camParams.eye)
                         : "$8", "memory");
    return eye;
}

float MCamGetLengthActor(int selector)
{
    float length = 0.0f;

    switch (selector) {
    case 0:
        length = D_00A5B220;
        break;
    case 1:
        length = D_00A5B224;
        break;
    }
    return length;
}

void MCamInit(void)
{
    camFlags = 0;
    memset(&camParams, 0, sizeof(camParams));
}

static void MCamExec_Control(void);
static void MCamExec_SetParams(void);
static void MCamExec_DispParams(void);

void MCamExec(void)
{
    if (camFlags & CAM_FLAG_ACTIVE) {
        MCamExec_Control();
        MCamExec_SetParams();
        MCamExec_DispParams();
    }
}

void MCamDumpDisabled(short disable)
{
    if (disable != 0) {
        camFlags |= CAM_FLAG_DUMP_DISABLED;
    } else {
        camFlags &= ~CAM_FLAG_DUMP_DISABLED;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamTakeControl);

int MCamReleaseControl(void)
{
    if (!(camFlags & CAM_FLAG_ACTIVE)) {
        MOutputDebugStringWarn(D_00A50EF0);
        return 0;
    }

    bcopy(&sysCamBuff, sysCam, sizeof(sysCamBuff));
    camFlags &= ~CAM_FLAG_ACTIVE;
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamStoreParams);

int MCamIsActive(void)
{
    return camFlags & CAM_FLAG_ACTIVE;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamSet);

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamMove);

void MCamStopMove(unsigned int point)
{
    int moveFlag;

    switch (point) {
    case 0:
        moveFlag = CAM_FLAG_MOVE_COORD;
        break;
    case 1:
        moveFlag = CAM_FLAG_MOVE_TARGET;
        break;
    case 2:
        moveFlag = CAM_FLAG_MOVE_BANK;
        break;
    case 3:
        moveFlag = CAM_FLAG_MOVE_PERS;
        break;
    default:
        MOutputDebugStringWarn(D_00A50F98);
        return;
    }
    camFlags &= ~moveFlag;
}

int MCamIsMoving(unsigned int point)
{
    int moveFlag;

    switch (point) {
    case 0:
        moveFlag = CAM_FLAG_MOVE_COORD;
        break;
    case 1:
        moveFlag = CAM_FLAG_MOVE_TARGET;
        break;
    case 2:
        moveFlag = CAM_FLAG_MOVE_BANK;
        break;
    case 3:
        moveFlag = CAM_FLAG_MOVE_PERS;
        break;
    default:
        MOutputDebugStringWarn(D_00A50FB8);
        return 0;
    }
    return (camFlags & moveFlag) != 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamShake);

int MCamIsShaking(void)
{
    return (camFlags >> CAM_SHAKE_BIT) & 1;
}

/* Local in the original: splat emits `glabel MCamGetActorCoord, local`; no reference outside m_cam.c. */
static Vector4 *MCamGetActorCoord(Vector4 *coord, ActRecord *actor, int part)
{
    __asm__ __volatile__("sq $0,0(%0)" : : "r"(coord) : "memory");
    if (actor == 0) {
        MOutputDebugStringWarn(D_00A50FD8);
        return coord;
    }
    if (part == 0) {
        __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                             : : "r"(coord), "r"(&actor->position)
                             : "$8", "memory");
    } else {
        sefGetNullPosition(coord, actor, part - 1, 0);
    }
    coord->w = fmodf(actor->rotation.y + 3.1415927f, 6.283185f);
    return coord;
}

static float MCamCalcBaseCoord(Vector4 *coord, MCamPointParams *point);
INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamCalcBaseCoord);

static float MCamCalcCoord(Vector4 *coord, MCamPointParams *point)
{
    MCamCoordOffset *coordOffset = &point->coordOffset;
    Vector4 polar;
    float length;

    length = MCamCalcBaseCoord(coord, point);
    switch (coordOffset->mode) {
    case 0:
        break;
    case 1:
        MMathCalcOffsetXYZ(coord, coord, &coordOffset->offset);
        break;
    case 2:
        polar.x = coordOffset->pitch;
        polar.y = coordOffset->yaw;
        polar.w = coordOffset->distance;
        MMathCalcOffset(coord, coord, &polar);
        break;
    }
    return length;
}

static float MCamGetAtkRange(int id)
{
    UnitInitData *unit = dataUnitInitGet(id);

    if (unit == 0) {
        MOutputDebugStringWarn(D_00A51000, id);
        return 0.0f;
    }
    return (float)unit->atkRange / 100.0f;
}

static float MCamGetDefRange(int id)
{
    UnitInitData *unit = dataUnitInitGet(id);

    if (unit == 0) {
        MOutputDebugStringWarn(D_00A51028, id);
        return 0.0f;
    }
    return (float)unit->defRange / 100.0f;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamExec_Control);

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamExec_SetParams);

static void MCamExec_SetParams_Base(void)
{
    camParams.actorLength[0] = MCamCalcBaseCoord(&camParams.baseCoord[0], &camParams.point[0]);
    camParams.actorLength[1] = MCamCalcBaseCoord(&camParams.baseCoord[1], &camParams.point[1]);
    MCamCalcCoord(&camParams.coord[0], &camParams.point[0]);
    MCamCalcCoord(&camParams.coord[1], &camParams.point[1]);
    MMathCalcAngleCam(&camParams.angle[0], &camParams.coord[0], &camParams.coord[1]);
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&camParams.rotation), "r"(&camParams.angle[0])
                         : "$8", "memory");
    camParams.rotation.z = camParams.bank;
    MMathCalcAngleCam(&camParams.angle[1], &camParams.coord[1], &camParams.coord[0]);
}

static void MCamExec_DispParams(void)
{
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamMove_Coord);

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamMove_Bank);

/*
 * The caller's own move-request object (its definer is outside this
 * allocation, called through MCamMove, also outside this allocation): only
 * the three fields MCamMove_Pers reads are evidenced.
 */
typedef struct MCamMoveRequest {
    unsigned char unmodeled_00[0xA4];
    float angle;
    unsigned char unmodeled_a8[0xB4 - 0xA8];
    int mode;
    int duration;
} MCamMoveRequest;

static void MCamMove_Pers(MCamMoveRequest *request)
{
    MCamMoveState *state = &D_00A5B5F8;
    int mode;
    int duration;
    float angle;
    float angleRad;
    float savedStart;

    mode = request->mode;
    duration = request->duration;
    /* MCamMove_Pers's start value is the float at state - 0x384. */
    state->start = camParams.perspectiveStart;
    state->mode = mode;
    angle = request->angle;
    state->duration = duration;
    state->elapsed = 0;
    if (angle != 0.0f) {
        angleRad = angle * 0.017453292f;
    } else {
        angleRad = 0.6981317f;
    }
    savedStart = state->start;
    state->end = angleRad;
    state->current = savedStart;
    camFlags |= 0x800;
}


static float MCamMoveProc_CalcRatio(unsigned int mode, unsigned int elapsed, unsigned int duration)
{
    float ratio;

    if (duration == 0) {
        MOutputDebugStringWarn(zero_duration_message, mode);
        return 0.0f;
    }

    ratio = (float)elapsed / (float)duration;

    switch (mode) {
    case 1: {
        float sine1;
        ratio = ratio * 3.14159265f + 4.712389f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine1)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = (sine1 + 1.0f) * 0.5f;
        break;
    }
    case 0:
        break;
    case 2: {
        float sine2;
        ratio = ratio * 1.57079633f + 4.712389f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine2)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = sine2 + 1.0f;
        break;
    }
    case 3: {
        float sine3;
        ratio = ratio * 1.57079633f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine3)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = sine3;
        break;
    }
    default:
        MOutputDebugStringWarn(invalid_interpolation_message);
        break;
    }

    return ratio;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamMoveProc_Coord);

static int MCamMoveProc_Bank(MCamMoveState *state)
{
    float start;
    float ratio;

    ratio = MCamMoveProc_CalcRatio(state->mode, ++state->elapsed, state->duration);
    start = state->start;
    state->current = start + (state->end - start) * ratio;
    return state->elapsed < state->duration;
}

static int MCamMoveProc_Pers(MCamMoveState *state)
{
    float start;
    float ratio;

    ratio = MCamMoveProc_CalcRatio(state->mode, ++state->elapsed, state->duration);
    start = state->start;
    state->current = start + (state->end - start) * ratio;
    return state->elapsed < state->duration;
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamShakeProc);

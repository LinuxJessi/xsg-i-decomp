/*
 * TU-local declarations of main/tu263 (src/main/look.c).
 */

#ifndef SRC_MAIN_LOOK_H
#define SRC_MAIN_LOOK_H

#include "shared.h"

/* Articulation elements have a 64-byte stride and three Euler rotation slots. */
typedef struct LookJointElement {
    float scale[3];
    unsigned char unmodeled_0c[4];
    float rotation[3];
    unsigned char unmodeled_1c[4];
    float translation[3];
    unsigned char unmodeled_2c[0x40 - 0x2c];
} LookJointElement;

typedef struct LookJointWork {
    unsigned char unmodeled_000[0x2c];
    int jointIndex;
    unsigned char unmodeled_030[0x720 - 0x30];
    Matrix4 rootMatrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    Matrix4 *matrixBuffer;
    unsigned char unmodeled_7a4[0x818 - 0x7a4];
    LookJointElement *elements;
} LookJointWork;

typedef struct LookEyeState {
    int frame;
    int timer;
    float yaw;
    float pitch;
} LookEyeState;

typedef struct LookCharacter {
    unsigned char unmodeled_000[0x70];
    Vector4 nearPosition;
    unsigned char number;
    unsigned char unmodeled_081[2];
    unsigned char headPositionValid;
    unsigned char unmodeled_084[0x90 - 0x84];
    Vector4 eyePosition;
    unsigned char unmodeled_0a0[0x600 - 0xa0];
    Vector4 lookAngles[2];
    Vector4 worldPosition;
    Vector4 headPosition;
    LookEyeState eyes[2];
    float eyeYaw;
    float eyePitch;
    unsigned char unmodeled_668[4];
    float lookWeight;
    unsigned char unmodeled_670[4];
    unsigned short lookMode;
    unsigned char unmodeled_676[2];
    unsigned int eyeMode;
    struct LookCharacter *lookTarget;
    unsigned char unmodeled_680[0x8d0 - 0x680];
    struct LookModelRecord *model;
    unsigned char unmodeled_8d4[0x8fc - 0x8d4];
    struct LookCharacter *eyeOwner;
    unsigned char unmodeled_900[0x9a0 - 0x900];
    unsigned int headFlags;
} LookCharacter;

/* The character's look rows and eye states viewed as float rows (0x600 and 0x640). */
typedef struct LookAngleView {
    unsigned char unmodeled_000[0x600];
    float lookAngles[2][4];
    unsigned char unmodeled_620[0x20];
    float eyeAngles[2][4];
} LookAngleView;

typedef struct LookModelRecord {
    unsigned char unmodeled_00[0x11];
    char name[0x48 - 0x11];
    int detail;
} LookModelRecord;

/* Four-float vector with 8-byte alignment: the double member gives the
   alignment that the SDK quadword copies assume, without a GNU attribute. */
typedef union LookAlignedVector4 {
    double alignment;
    Vector4 v;
} LookAlignedVector4;


void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right);
void xglMatrixInverse(Matrix4 destination, Matrix4 source);
void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *source);
float xglAtan2(float x, float y);
/* Head bounds, position adjustment and smoothing steps. */
#define D_004D8578 0.0001f
#define D_004D857C 0.15f
#define D_004D8580 1.0471975803375244f
#define D_004D8584 (-1.0471975803375244f)
#define D_004D8588 1.483529806137085f
#define D_004D858C (-1.483529806137085f)
#define D_004D8590 (-0.7f)
#define D_004D8594 0.7f
#define D_004D8598 0.1745329350233078f
#define D_004D859C 0.0872664675116539f
#define D_004D85A0 0.1745329350233078f
#define D_004D85A4 0.0872664675116539f
#define D_004D8568 0.0001f
#define D_004D856C 0.13962633907794952f
#define D_004D8570 0.13962633907794952f
#define D_004D8574 0.2f

void CUR_MATRIX_Set(Matrix4 matrix);
void CUR_MATRIX_Txyz4s(const float *displacement);
void CUR_MATRIX_Rzyx4s(const float *angles);
void CUR_MATRIX_Get(Matrix4 matrix);
void MATRIX_convert4(Matrix4 destination, Matrix4 source);
void xglMatrixRotX(Matrix4 destination, Matrix4 source, float angle);
void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle);


float xglAsin(float value);

/* MARK provisional: original .lit4 external witnesses, no original symbols. */
#define xglAsin_cubic (-0.01872929931f)

void look_limit(float *angle, float limit);

/* MARK provisional: .lit4 0x004d8554 */
#define xglAsin_quadratic 0.07426100224f

void look_limit_add(float *angle, float target, float step);

/* MARK provisional: .lit4 0x004d8558 */
#define xglAsin_linear 0.2121143937f

/* MARK provisional: .lit4 0x004d855c */
#define xglAsin_pi_over_two_approx 1.570728779f

/* MARK provisional: .lit4 0x004d8560 */
#define xglAsin_pi_over_two 1.570796371f

void LOOK_eyeL_cont(int model, int target, int step);

/* MARK provisional: .lit4 0x004d85a8 */
#define D_004D85A8 0.03999999911f

void LOOK_eyeR_cont(int model, int target, int step);

/* MARK provisional: .lit4 0x004d85ac */
#define D_004D85AC (-0.03999999911f)

/* Eye scale, step and rotation constants. */
#define D_004D85B0 0.4f
#define D_004D85B4 0.005235988f
#define D_004D85B8 0.13962634f
#define D_004D85BC 0.13962634f
#define D_004D85C0 0.3f
#define D_004D85C4 0.6f

#endif /* SRC_MAIN_LOOK_H */

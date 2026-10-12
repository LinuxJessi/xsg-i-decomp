#ifndef INCLUDE_MAIN_MATRIX_PRINT_H
#define INCLUDE_MAIN_MATRIX_PRINT_H

#include "shared.h"

void CUR_MATRIX_Unit4s(void);

void CUR_MATRIX_Get(Matrix4 matrix);

void CUR_MATRIX_SetT(const Vector4 *translation);

void CUR_MATRIX_Rx4s(const int *angle);

void CUR_MATRIX_Ry4s(const int *angle);

void CUR_MATRIX_Rz4s(const int *angle);

void CUR_MATRIX_Sxyz4s(const Vector4 *scale);

#endif /* INCLUDE_MAIN_MATRIX_PRINT_H */

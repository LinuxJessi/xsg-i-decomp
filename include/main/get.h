#ifndef INCLUDE_MAIN_GET_H
#define INCLUDE_MAIN_GET_H

#include "shared.h"

int Get_Rnd(int min, int max);

typedef struct Vector3 { float x, y, z; } Vector3;

typedef Vector4 Point4;

int Check_Angle(float angle, float rangeStart, float rangeEnd);

float Get_Angle_Relative(const Point4 *first, const Point4 *second,
                         float reference);

#endif /* INCLUDE_MAIN_GET_H */

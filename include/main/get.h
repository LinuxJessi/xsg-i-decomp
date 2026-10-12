#ifndef INCLUDE_MAIN_GET_H
#define INCLUDE_MAIN_GET_H

int Get_Rnd(int min, int max);

typedef struct Vector3 { float x, y, z; } Vector3;

typedef struct Point4 { float x, y, z, w; } Point4;

extern int Check_Angle(float angle, float rangeStart, float rangeEnd);

float Get_Angle_Relative(const Point4 *first, const Point4 *second,
                         float reference);

#endif /* INCLUDE_MAIN_GET_H */

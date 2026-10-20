#ifndef INCLUDE_MAIN_INIT_UWAMONO_SYS_H
#define INCLUDE_MAIN_INIT_UWAMONO_SYS_H

#include "shared.h"

int CrossPointUwamono(const Vector4 *position, const Vector4 *previous,
                      float radius, Vector4 *result);

int HitCheckMapUnitPos(const Vector4 *position);

#endif /* INCLUDE_MAIN_INIT_UWAMONO_SYS_H */

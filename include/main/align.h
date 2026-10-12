#ifndef INCLUDE_MAIN_ALIGN_H
#define INCLUDE_MAIN_ALIGN_H

#include "shared.h"

/* The VU0 block builds the component ratios used for the two rotations that
 * place the current matrix's X axis along direction.  The scalar atan2 calls
 * and the persistent matrix operations remain ordinary C. */
void alignXAxis(const Vector4 *direction, Matrix4 matrix);

/* The VU0 block derives the two angles for the X-then-Z rotations that align
 * the current matrix's Y axis with direction. */
void alignYAxis(const Vector4 *direction, Matrix4 matrix);

/* The VU0 block derives the two angles for the X-then-Y rotations that align
 * the current matrix's Z axis with direction. */
void alignZAxis(const Vector4 *direction, Matrix4 matrix);

#endif /* INCLUDE_MAIN_ALIGN_H */

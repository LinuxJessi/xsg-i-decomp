/*
 * Shared declarations of ov01/tu022 (src/ov01/m_ef.c): the actor coordinate
 * providers the effect constructors call.
 */
#ifndef INCLUDE_OV01_M_EF_H
#define INCLUDE_OV01_M_EF_H

#include "shared.h"
#include "main/act_1.h"

/*
 * Coordinate selectors of MEfGetActorMatrix/MEfGetActorCoord: the actor
 * itself, the actor raised 1.5 units, or an accessory selector below
 * MEF_POINT_ACCESSORY_COUNT that ACT_jointGetAccessories resolves to the
 * joint whose matrix is returned.
 */
#define MEF_POINT_ACTOR ((u32)-1)
#define MEF_POINT_ACTOR_RAISED ((u32)-2)
#define MEF_POINT_ACCESSORY_COUNT 0x28

/*
 * actor is the ACT_create actor record the effect follows; coordinate is one
 * of the selectors above. An effect creation request carries two separate
 * actor/selector pairs: the matrix source at +0x08/+0x0c and the coordinate
 * source at +0x24/+0x28.
 */
extern Matrix4 *MEfGetActorMatrix(Matrix4 *destination, ActRecord *actor, u32 coordinate);
extern Vector4 *MEfGetActorCoord(Vector4 *destination, ActRecord *actor, u32 coordinate);

#endif /* INCLUDE_OV01_M_EF_H */

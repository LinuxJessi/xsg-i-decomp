#ifndef INCLUDE_MAIN_NML_MODEL_SET_H
#define INCLUDE_MAIN_NML_MODEL_SET_H

#include "shared.h"

void nmlModelSetSortOffset(float offset);

void nmlModelSetGlobalPointLightCol(int index, const Vector4 *color);

void nmlModelSetSpecularOff(int disableSpecular);

void nmlModelSetRenderStatus(int status);

void nmlModelSetClip(int clip);

int nmlModelSetGlobalFogReset(void);

void nmlModelSetFogCancel(void);

void nmlModelSetMapLastEntry(void *map, int blockId);

#endif /* INCLUDE_MAIN_NML_MODEL_SET_H */

#ifndef PAUSE_MENU_PAGE_ENEMY_H
#define PAUSE_MENU_PAGE_ENEMY_H

#include "main/xgl_2.h"
#include "main/xgl_studio.h"

extern short Times;
extern float D_004D8160;

void xglLightCalcMatrix(StudioLight *light);
void nmlModelSetLight(Matrix4 colorMatrix, Matrix4 normalMatrix);
void nmlModelSetPlace(Matrix4 matrix);
void nmlModelEntry(int entry);

#endif

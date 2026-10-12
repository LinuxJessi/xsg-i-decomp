#include "common.h"

#include "pause_menu_page_enemy.h"

short EventID = 0;
signed char FlagExEvent = 0;

INCLUDE_ASM("asm/main/nonmatchings/pause_menu_page_enemy", PauseMenuPageEnemy);

void Debug_Mark(const Vector4 *position, int entry)
{
    Matrix4 place;
    Vector4 translation;
    Vector4 rotation;
    Matrix4 colorMatrix;
    Matrix4 normalMatrix;
    StudioLight *light;

    rotation.y = (float)Times * D_004D8160 * 0.015625f;
    translation.z = position->z;
    translation.x = position->x;
    translation.y = position->y + 1.0f;
    translation.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&translation.x);
    xglMatrixStackRotY(rotation.y);
    xglMatrixStackSave(place);

    xglStudioGetLight(&light);
    xglLightCalcMatrix(light);
    /* The SDK copies the two matrix blocks through its fixed scratch GPR. */
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        : : "r" (colorMatrix), "r" (&light->unmodeled_70[64])
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        : : "r" (normalMatrix), "r" (&light->unmodeled_70[0])
        : "$2", "memory");
    nmlModelSetLight(colorMatrix, normalMatrix);
    nmlModelSetPlace(place);
    nmlModelEntry(entry);
}

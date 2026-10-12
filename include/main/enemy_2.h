#ifndef INCLUDE_MAIN_ENEMY_2_H
#define INCLUDE_MAIN_ENEMY_2_H

/* Sibling of this TU, still original bytes (INCLUDE_ASM above): converts a
 * command's target selector into the enepc/Actor index Enemy_Command_Target
 * below stores, the same numbering ACT_create hands out into Actor.number
 * (+0x80). */
extern int Get_ActorNumber(int target);

#endif /* INCLUDE_MAIN_ENEMY_2_H */

#ifndef INCLUDE_OV01_CALC_H
#define INCLUDE_OV01_CALC_H

#include "shared.h"

enum CalcTechniqueCount { CALC_NORMAL_TECHNIQUE_COUNT = 6 };

typedef struct CalcUnitParam CalcUnitParam;

struct CalcUnitParam {
    short maxHp;                /* +0x00: calcIdol2Chk's threshold base */
    short maxEp;                /* +0x02: calcEp's clamp ceiling */
    short baseAtk;                /* +0x04: calcWpnAtkGet override term */
    unsigned char unmodeled_06[0x0C - 0x06];
    unsigned char hitStat;      /* +0x0C: calcHitParaGet's own signed view */
    /*
     * +0x0D: calcHitRate (this TU, 0x00a13010) loads the target's byte with
     * `lbu $3,0xD($2)` and reinterprets it as signed (`sll $18,$3,24`/
     * `sra $16,$18,24`) as the evasion term it subtracts from the hit rate.
     */
    unsigned char evadeStat;
    signed char agility;       /* +0x0E: calcWct's wait-count formula */
    signed char moveKindId;    /* +0x0F: unitCmdMove compares it with 4 */
    /*
    * +0x10/+0x12: the two halfwords unitColiGet (ov01/tu002, 0x00a07488)
    * adds into the gap unitCmdDstSet keeps between a moving unit and the
    * unit it walks up to. It reads `lh $3,16($2)` (0x00a074a0) from the
    * moving unit's own block and `lh $3,18($2)` (0x00a074b8) from the
    * approached unit's block, adds them and divides the sum by 100.0f;
    * unitCmdDstSet subtracts that distance from the target position.
    */
    short coliMove;             /* +0x10: term used when this unit moves */
    short coliTarget;           /* +0x12: term used when it is approached */
    unsigned char unmodeled_14[0x15 - 0x14];
    signed char sizeClass;          /* +0x15: cmdPartySizeGet size byte */
    signed char counterRate;    /* +0x16: calcCounter's base counter chance */
    unsigned char unmodeled_17[0x18 - 0x17];
    u16 conditions;                /* +0x18: status conditions word */
    unsigned char unmodeled_1a[0x1C - 0x1A];
    u16 flags;               /* +0x1C */
    unsigned char unmodeled_1e[0x24 - 0x1E];
    u16 statProtect[1];      /* +0x24: calcStatProtectGet/Set's group word */
    unsigned char unmodeled_26[0x34 - 0x26];
    u16 currentHp;           /* +0x34: calcPara2OrgSub's copy source */
    u16 currentEp;           /* +0x36: calcPara2OrgSub's copy source */
    short charaId;           /* +0x38 */
    signed char dblActKind;     /* +0x3A: calcDblActChk */
    signed char bcLimit;        /* +0x3B: calcBc's own clamp ceiling */
    signed char bcValue;        /* +0x3C: calcBc's own running counter */
    unsigned char unmodeled_3d[0x3E - 0x3D];
    short turnBias;             /* +0x3E: calcWct/calcTurnEnd */
    short waitCount;            /* +0x40: calcTurnEnd/calcWctDec */
    short waitCountBase;        /* +0x42: calcTurnEnd */
    short posId;                /* +0x44: calcLineChk/calcMoveChk/calcA2Chk's
                                 * `lh $x,0x44($2)`, fed to dataPosLineGet
                                 * (src/ov01/data_unit_org_get.c). */
    unsigned char unmodeled_46[0x50 - 0x46];
    CalcUnitParam *linkedUnit;   /* +0x50: menuStatChrIdGet reads its charaId */
    short baseCharaId;             /* +0x54: unit character id when flags&0x40 clear */
    unsigned char unmodeled_56[0x5E - 0x56];
    short weaponId[3];       /* +0x5E: per-slot weapon ids */
    short accessoryId[3];    /* +0x64: calcMagDefGet's per-slot item ids */
    short attachmentId[3];   /* +0x6A: calcWpnEnableChk's `lh $3,0x6A($3)`,
                              * mirroring CalcCharParaData's attachmentId */
    short wpnSlotValue[3];   /* +0x70: calcDblActChk, per weapon slot */
    short normalTechniqueId[CALC_NORMAL_TECHNIQUE_COUNT]; /* +0x76 */
    short slotTechniqueId[2];   /* +0x82: calcSlotAChk's own pair, right
                                 * after normalTechniqueId. */
    unsigned char unmodeled_86[0xA6 - 0x86];
    short skillId[3];            /* +0xA6: calcSkillEquChk's `lh $3,0x6($2)`,
                                  * mirroring CalcCharParaData's skillId at
                                  * the same offset. */
    u16 statActiveMask[8];   /* +0xAC: calcStatTurn's per-category bit set */
    /*
     * +0xBC: calcStatReset (this TU, 0x00a17918) clears this per-category
     * word (AND ~mask) instead of statActiveMask when its fourth argument
     * is nonzero; every known caller passes 0 there, so only the clear
     * itself is evidenced.
     */
    u16 statAltMask[8];      /* +0xBC */
    unsigned char statTurnCount[8][16]; /* +0xCC: calcStatTurn's countdown grid */
    u16 etherDisableFlags;   /* +0x14C: calcEtherDisableGet/Set's bit set */
    unsigned char unmodeled_14e[0x158 - 0x14E];
    struct UnitWork *effectUnit;   /* +0x158: effect object, same kind as statEffUnits */
    struct UnitWork *statEffUnits[8]; /* +0x15C: statEffOn/statEffOff */
};

/* ACT_create's actor pool supplies both ObjectTask.work and the unit's
 * separate motion actor. calcUPGet reaches up through work at +0x84;
 * dataUnitFileLoadMdl reaches the model pointers through motionActor.
 * Only fields used by these consumers are named. */
typedef struct BattleActor {
    int flags;
    void (*update)(struct BattleActor *self);
    void (*draw)(struct BattleActor *self);
    unsigned char unmodeled_0c[0x84 - 0x0C];
    struct CalcUnitParam *up; /* +0x84 */
} BattleActor;

/*
 * Writes every unit's HP/EP back to its persistent origin record, and the
 * linked unit's through calcTotalParaMenuSub's fake-unit pattern; a unit
 * carrying CalcUnitParam flag 0x40 hands its own currentEp to the linked
 * unit first.
 */
void calcPara2Org(void);

#endif /* INCLUDE_OV01_CALC_H */

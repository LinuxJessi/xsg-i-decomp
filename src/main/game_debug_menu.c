#include "common.h"

#include "shared.h"

#include "game_debug_menu.h"

char D_004D9EF8[];

static void PauseMenuPage0(void)
{
    int y;
    int i;

    if (PadData.half_2a & PAD_CROSS) {
        GameResourceDump(0);
    }

    xglFontDebugPrintf(136, 52, "WrkEnd:%8x", WorkEnd);

    y = 64;
    for (i = 0; i < 16; i++) {
        xglFontDebugPrintf(8, y, D_004D9EF8, actor[i].inUseId);
        y += 8;
    }

    GameResourceDump(1);
}

INCLUDE_ASM("asm/main/nonmatchings/game_debug_menu", PauseMenuPage2);

char D_004D9EF8[8] = "%4x";

static void PauseMenuPage3(void)
{
    int i;
    int y;
    int dir;
    int number;
    int last;
    int used;
    int remain;
    unsigned int repeat;
    DebugPlayerUnit *player;
    DebugLayoutHeader *header;
    char *text;

    if (GameLoopState.debugPageInitialized == 0) {
        GameLoopState.debugPageInitialized = 1;
        GameLoopState.target.mapChangeId = GameLoopState.currentMapId;
        GameLoopState.leaderBattleId = ((DebugSavedParty *)SaveData)->leaderBattleId;
        GameLoopState.scratch.scriptJump.cfId = 0;
        GameLoopState.scratch.scriptJump.eventId = 0;
    }

    repeat = PadData.half_2c;
    if (repeat & PAD_UP) {
        GameLoopState.debugCursor--;
    }
    if (repeat & PAD_DOWN) {
        GameLoopState.debugCursor++;
    }
    if (GameLoopState.debugCursor < 0) {
        GameLoopState.debugCursor = DEBUG_ROW_COUNT - 1;
    }
    if (GameLoopState.debugCursor >= DEBUG_ROW_COUNT) {
        GameLoopState.debugCursor = 0;
    }

    dir = 0;
    if (repeat & PAD_LEFT) {
        dir = -1;
    }
    if (repeat & PAD_RIGHT) {
        dir = 1;
    }
    if (PadData.half_28 & PAD_CROSS) {
        dir *= 100;
    }
    if (dir != 0) {
        switch (GameLoopState.debugCursor) {
        case 0:
            for (;;) {
                GameLoopState.target.mapChangeId += dir;
                if (GameLoopState.target.mapChangeId < 0) {
                    GameLoopState.target.mapChangeId = MAP_COUNT - 1;
                }
                if (GameLoopState.target.mapChangeId >= MAP_COUNT) {
                    GameLoopState.target.mapChangeId = 0;
                }
                if (MapGetName(GameLoopState.target.mapChangeId) == 0) {
                    dir = dir > 0 ? 1 : -1;
                    continue;
                }
                break;
            }
            break;
        case 1:
            GameLoopState.leaderBattleId += dir;
            break;
        case 3:
            number = GameSnapShotNumber(-1) + dir;
            if (number < 0) {
                number = 0;
            }
            GameSnapShotNumber(number < 10 ? number : 9);
            break;
        case 4:
            ModelPath = (ModelPath + dir) & 1;
            break;
        case 5:
            UseTestPath = (UseTestPath + dir + 3) % 3;
            break;
        case 6:
            ScenePath = (ScenePath + dir + 20) % 20;
            break;
        case 7:
            GameLoopState.scratch.scriptJump.cfId =
                (GameLoopState.scratch.scriptJump.cfId + dir + 10000) % 10000;
            break;
        case 8:
            GameLoopState.scratch.scriptJump.eventId =
                (GameLoopState.scratch.scriptJump.eventId + dir + 10000) % 10000;
            break;
        case 9:
            ScanLineInterpolate = (ScanLineInterpolate + dir + 3) % 3;
            break;
        }
    }

    if (PadData.half_2a & PAD_CIRCLE) {
        switch (GameLoopState.debugCursor) {
        case 0: {
            DebugPlayerUnit *leader;

            MapChange(GameLoopState.target.mapChangeId);
            leader = GameLoopState.player;
            leader->x = 0;
            leader->y = 0;
            leader->z = 0;
            leader->angle = 0;
            header = UnduDataGetHeader(513, 0x8000);
            if (header != 0) {
                leader->x = header->components[0];
                leader->y = header->components[1];
                leader->z = header->components[2];
                leader->layoutAngle = leader->angle = header->components[3];
            }
            break;
        }
        case 1:
            ((DebugSavedParty *)SaveData)->leaderBattleId = GameLoopState.leaderBattleId;
            GameCfPlayerLoadResource(0);
            break;
        case 2:
            GameStateSave();
            GameLoopState.mode = 5;
            func_A22668();
            GameStateRestore();
            GameLoopState.debugPageInitialized = 0;
            break;
        case 7:
            Java_xeno_util_Runtime_jump_sub(GameLoopState.scratch.scriptJump.cfId, -1);
            break;
        case 8:
            Java_xeno_util_Runtime_jump_sub(
                SCRIPT_ID_EVENT_BASE + GameLoopState.scratch.scriptJump.eventId, -1);
            GameLoopState.runtimeFlags &= ~1;
            break;
        }
        PadData.half_2a = 0;
    }

    xglFontDebugPrintf(8, 52, D_004C04B0);
    last = GAME_RESOURCE_COUNT - 1;
    while (GameResource[last].address == 0 && last != 0) {
        last--;
    }
    remain = GameResource[last].size;
    used = 0;
    for (last--; last >= 0; last--) {
        used += GameResource[last].size;
    }
    xglFontDebugPrintf(136, 48, "USED  :%9d", used + DebugMenuResourceSizeAdd);
    if (DebugMenuResourceSizeAdd != 0) {
        xglFontDebugPrintf(246, 48, D_004D9F18);
    }
    xglFontDebugPrintf(136, 56, "REMAIN:%9d", remain);

    y = 64;
    for (i = 0; i < DEBUG_ROW_COUNT; i++) {
        if (i == GameLoopState.debugCursor) {
            xglFontDebugPrintf(8, y + 3, D_004D9F20);
        }
        xglFontDebugPrintf(16, y, tbl_1_00367A70[i]);
        switch (i) {
        case 0:
            xglFontDebugPrintf(80, y + 3, D_004D9F28, GameLoopState.target.mapChangeId,
                               MapGetName(GameLoopState.target.mapChangeId));
            break;
        case 1:
            xglFontDebugPrintf(80, y + 3, D_004D9EF8, GameLoopState.leaderBattleId);
            break;
        case 3:
            xglFontDebugPrintf(72, y + 3, "game%c000", GameSnapShotNumber(-1) + '0');
            break;
        case 4:
            if (ModelPath == 0) {
                xglFontDebugPrintf(92, y + 3, "data\\[path]");
            } else {
                xglFontDebugPrintf(92, y + 3, "data\\test\\[path]");
            }
            break;
        case 5:
            xglFontDebugPrintf(92, y + 3, D_004D9F30, MAP_getPath());
            break;
        case 6:
            xglFontDebugPrintf(92, y + 3, D_004D9F30, RES_getPath());
            break;
        case 7:
            xglFontDebugPrintf(88, y + 3, D_004D9F38, GameLoopState.scratch.scriptJump.cfId);
            break;
        case 8:
            xglFontDebugPrintf(88, y + 3, D_004D9F38, GameLoopState.scratch.scriptJump.eventId);
            {
                char fileName[7] = "cf0000";
                int scriptId = GameLoopState.scriptId;

                if (scriptId <= 0x0fffffff) {
                    fileName[0] = 'c';
                    fileName[1] = 'f';
                } else {
                    fileName[0] = 'e';
                    fileName[1] = 'v';
                    scriptId &= 0xffff;
                }
                fileName[5] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[4] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[3] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[2] = scriptId % 10 + '0';
                xglFontDebugPrintf(120, y - 3, fileName);
            }
            break;
        case 9:
            switch (ScanLineInterpolate) {
            case 0:
            default:
                text = D_004D9F48;
                break;
            case 1:
                text = D_004C0528;
                break;
            case 2:
                text = D_004C0538;
                break;
            }
            xglFontDebugPrintf(80, y, text);
            break;
        }
        y += 12;
    }

    player = GameLoopState.player;
    xglFontDebugPrintf(176, 160, D_004C0548);
    xglFontDebugPrintf(176, 172, "%f\n%f\n%f", player->x, player->y, player->z);
}

void GameDebugMenu(void)
{
    int pageStep;

    pageStep = 0;
    if (PadData.half_2a & PAD_L1) {
        pageStep--;
    }
    if (PadData.half_2a & PAD_R1) {
        pageStep = 1;
    }
    if (pageStep) {
        unsigned int nextPage =
            (GameLoopState.debugPage + pageStep + 6) % 6;
        GameLoopState.debugPageInitialized = 0;
        GameLoopState.debugPage = nextPage;
    }
    switch (GameLoopState.debugPage) {
    case 0:
        PauseMenuPage3();
        return;
    case 1:
        PauseMenuPage0();
        return;
    case 2:
        PauseMenuPage2();
        return;
    case 3:
        PauseMenuPageUwamono();
        return;
    case 4:
        PauseMenuPageEnemy();
        return;
    case 5:
        PauseMenuPagePartyDebug();
        break;
    }
}

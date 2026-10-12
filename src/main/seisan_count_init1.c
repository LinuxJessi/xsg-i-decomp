#include "common.h"

#include "shared.h"

#include "main/xgl_task.h"
#include "main/xgl_font.h"
#include "main/xgl_studio.h"
#include "main/xgl_packet.h"

#include "seisan_count_init1.h"

typedef struct SeisanLabelBytes {
    unsigned char bytes[5];
} SeisanLabelBytes;

const SeisanRibbonData D_004C7350 = {
    { 0x00000000, 0x0000FFFF, 0x80000000, 0x01C00000,
      0x0000FFFF, 0x80000000, 0x00000200, 0x0000FFFF,
      0x80000000, 0x01C00200, 0x0000FFFF, 0x80000000,
      0x00000000, 0x00000000, 0x00000000 }
};

unsigned char *SeisanWork = 0;

XglTaskScheduler *SeisanBgTask = 0;

XglTaskScheduler *SeisanTask = 0;

SeisanResultData *SeisanResult = 0;

static SeisanCountData SeisanCN;

/* The original eight-byte prompt-pointer table remains in data scaffolding. */

extern const char *msg00_1_0036DAC8[];

void SeisanCountInit1(void)
{
    int chrNo;

    memset(&SeisanCN, 0, sizeof(SeisanCN));
    for (chrNo = 1; chrNo < 13; chrNo++)
    {
        SeisanCN.nextExp[chrNo - 1] = func_A19210(chrNo)->nextExp;
    }
}

static inline void SeisanRollbackStats(SeisanUnitRecord *unit,
                                      SeisanCharRecord *character,
                                      const SeisanHissatuEntry *row)
{
    unit->hp -= row->hp;
    unit->ep -= row->ep;
    character->techPoints -= row->techPoints;
    character->skillPoints -= row->skillPoints;
    character->specialPoints -= row->specialPoints;
    unit->level -= row->level;
}

void SeisanCountInit2(void)
{
    int i;
    SeisanHissatuEntry *result = SeisanResult->entries;

    SeisanCN.state = 0;
    for (i = 0; i < 3; i++) {
        int characterNo = result[i].characterNo;
        if ((unsigned int)(characterNo - 1) < 12) {
            SeisanUnitRecord *unit = func_A191C0(characterNo);
            SeisanCharRecord *character = func_A19210(characterNo);

            SeisanCN.targets[i].level = unit->level;
            SeisanCN.targets[i].hp = unit->hp;
            SeisanCN.targets[i].ep = unit->ep;
            {
                SeisanCN.targets[i].experience = character->experience;
                SeisanCN.targets[i].nextExp = character->nextExp;
                SeisanCN.targets[i].techPoints = character->techPoints;
                SeisanCN.targets[i].skillPoints = character->skillPoints;
                SeisanCN.targets[i].specialPoints = character->specialPoints;
                SeisanRollbackStats(unit, character, &SeisanResult->entries[i]);
                {
                    SeisanResultData *freshResults = SeisanResult;
                    result = freshResults->entries;
                    character->experience -= (freshResults->entries + i)->experience;
                }
                character->nextExp = SeisanCN.nextExp[characterNo - 1];
            }
        }
    }
    SeisanCN.countedMoney = 0;
}

int SeisanNumberCount(int current, int target, int step, int *moving)
{
  int overshot;
  int next;
  next = current;
  if (next != target)
  {
    if (next < target)
    {
      next += step;
      overshot = target < next;
    }
    else
    {
      next -= step;
      overshot = next < target;
    }
    if (overshot)
    {
      next = target;
    }
    else
    {
      *moving = 1;
    }
  }
  return next;
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanCountMain);

void TskObjectSet2(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

void tskTskMain2(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (SeisanWork[1] == 0xff) {
        xglTaskWaitRemove(&task->base);
        return;
    }

    if (task->state != 0) {
        if (task->state != 2)
            return;
    } else {
        worker(task, task->data);
        task->state = 2;
    }

    worker(task, task->data);
}

static void SeisanTimeEx(TskObject *task, SeisanTimePanel *panel)
{
    XglClock elapsed;
    short targetY;
    int i;

    switch (task->state) {
    case 0:
        panel->depth = 0xfffff0;
        WindowDXSet(&panel->window);
        panel->window.x = 16;
        panel->window.y = -64;
        panel->window.depth = panel->depth;
        panel->window.width = 480;
        panel->window.height = 32;
        panel->window.title = msg00_0_0036DAB8[0];
        panel->window.state = 1;
        WindowDXMain(&panel->window);
        panel->window.state = 3;
        for (i = 0; i < 2; i++) {
            eTagFontSet(&panel->tags[i], msg00_0_0036DAB8[i + 1]);
            panel->tags[i].depth = panel->depth + 2;
        }
        for (i = 0; i < 3; i++) {
            eNumberSet(&panel->numbers[i], 0);
            panel->numbers[i].depth = panel->depth + 2;
        }
        xglClockUInt2DayTime(&elapsed, func_A22908() / 30);
        PartyTimeLimitCheck(&elapsed);
        PartyTimeDispChange(&elapsed);
        panel->numbers[0].option = 2;
        /* The number receives the first four display-clock bytes as one word. */
        __builtin_memcpy(&panel->numbers[0].value, &elapsed, sizeof(int));
        {
            int money = dataMoneyBoxChk();
            panel->numbers[1].digits = 9;
            panel->numbers[1].option = 0;
            panel->numbers[1].value = money;
        }
        panel->numbers[2].value = SeisanMg->money;
        panel->numbers[2].digits = panel->numbers[1].digits;
        panel->numbers[2].option = 7;
        for (i = 0; i < 2; i++) {
            eSpriteSet(&panel->sprites[i], i + 0x707);
            panel->sprites[i].depth = panel->depth + 2;
        }
        break;
    case 2: {
        SeisanControlState *control = (SeisanControlState *)SeisanWork;
        targetY = 32;
        if (control->state == SEISAN_STATE_CLOSING)
            targetY = -64;
        MoveSlide(&panel->window.y, &targetY, 3.0f);
        WindowDXMain(&panel->window);
        panel->numbers[0].x = panel->window.x + 91;
        panel->numbers[0].y = panel->window.y + 8;
        eNumberMain(&panel->numbers[0]);
        panel->tags[0].x = panel->numbers[0].x - 8;
        panel->tags[0].y = panel->window.y + 8;
        eTagFontMain(&panel->tags[0]);
        panel->numbers[1].x = panel->window.x + 238;
        panel->numbers[1].y = panel->window.y + 8;
        panel->numbers[1].value = dataMoneyBoxChk() - SeisanMg->money + SeisanCN.countedMoney;
        eNumberMain(&panel->numbers[1]);
        panel->tags[1].x = panel->numbers[1].x - 8;
        panel->tags[1].y = panel->window.y + 8;
        eTagFontMain(&panel->tags[1]);
        panel->sprites[0].x = panel->window.x + 340;
        panel->sprites[0].y = panel->numbers[2].y = panel->window.y + 8;
        eSpriteMain(&panel->sprites[0]);
        panel->numbers[2].x = panel->window.x + 350;
        panel->numbers[2].value = SeisanCN.countedMoney;
        panel->numbers[2].y = panel->window.y + 8;
        eNumberMain(&panel->numbers[2]);
        panel->sprites[1].x = panel->window.x + 461;
        panel->sprites[1].y = panel->numbers[2].y = panel->window.y + 8;
        eSpriteMain(&panel->sprites[1]);
        break;
    }
    }
}

static void tskSeisanButton(TskObject *task, SeisanButtonWork *button)
{
    short targetY;

    switch (task->state) {
    case 0: {
        button->color = 0xfffff0;
        eSpriteSet(&button->sprite, 0x200);
        button->sprite.x = -0x70;
        button->sprite.y = 0x1a4;
        button->sprite.color = button->color;
        eMessageSet(&button->message, msg00_1_0036DAC8[0]);
        button->message.mode = 0x20;
        button->message.x = button->sprite.x + 0x14;
        button->message.y = button->sprite.y;
        button->message.color = button->color;
        break;
    }
    case 2: {
        unsigned char *work = SeisanWork;
        unsigned char flags = work[SEISAN_WORK_FLAGS];
        targetY = 0x180;
        if (flags & 4)
            button->message.text = msg00_1_0036DAC8[1];
        if (work[SEISAN_WORK_STATE] == SEISAN_STATE_CLOSING)
            targetY = 0x220;
        MoveSlide(&button->sprite.x, &targetY, 3.0f);
        eSpriteMain(&button->sprite);
        button->message.x = button->sprite.x + 0x14;
        button->message.y = button->sprite.y;
        eMessageMain(&button->message);
        break;
    }
    }
}

static void tskSeisanItem(TskObject *task, void *data)
{
    SeisanItemPanel *panel = data;
    unsigned short ordinalText[3];
    unsigned short quantityText[3];
    short targetX;
    int i;

    switch (task->state) {
    case 0:
        panel->active = 0;
        panel->itemCount = 0;
        panel->depth = 0xfffff0;
        for (i = 0; i < SeisanMg->itemCount; i++) {
            if (SeisanItemBox[i].count != 0)
                panel->itemCount++;
        }
        panel->windows[0].y = 128;
        panel->windows[0].height = 32;
        panel->windows[1].y = 160;
        panel->windows[1].height = panel->itemCount * 24 + 6;
        for (i = 0; i < 2; i++) {
            WindowDXSet(&panel->windows[i]);
            panel->windows[i].x = 832;
            panel->windows[i].width = 288;
            panel->windows[i].depth = panel->depth;
            panel->windows[i].state = 1;
            WindowDXMain(&panel->windows[i]);
            panel->windows[i].state = 3;
        }
        if (panel->itemCount)
            eMessageSet(&panel->title, msg00_2_0036DAD0[0]);
        else
            eMessageSet(&panel->title, msg00_2_0036DAD0[1]);
        panel->title.mode = 32;
        panel->title.color = panel->depth + 2;
        for (i = 0; i < panel->itemCount; i++) {
            eMessageSet(&panel->items[i], MenuTextGet(SeisanItemBox[i].itemId)->name);
            panel->items[i].mode = 32;
            panel->items[i].color = panel->depth + 2;
        }
        break;
    case 2:
        targetX = 112;
        switch (SeisanWork[SEISAN_WORK_STATE]) {
        case 0x20:
        case 0x30:
        case 0x50:
        case 0x58:
        case 0x60:
            targetX = 832;
            break;
        case 0x80:
            panel->active = 1;
            break;
        case SEISAN_STATE_CLOSING:
            targetX = -320;
            break;
        }
        if (panel->active) {
            MoveSlide(&panel->windows[0].x, &targetX, 3.0f);
            WindowDXMain(&panel->windows[0]);
            panel->title.x = panel->windows[0].x + 64;
            panel->title.y = panel->windows[0].y + 3;
            if (panel->itemCount == 0)
                panel->title.x += 20;
            eMessageMain(&panel->title);
            if (panel->itemCount) {
                panel->windows[1].x = panel->windows[0].x;
                WindowDXMain(&panel->windows[1]);
                for (i = 0; i < panel->itemCount; i++) {
                    const unsigned short *ordinalSource;
                    const unsigned short *quantitySource;
                    unsigned short ordinalGlyph;
                    unsigned short quantityGlyph;
                    int printX;
                    int printY;
                    panel->items[i].x = panel->windows[1].x + 43;
                    panel->items[i].y = panel->windows[1].y + i * 24 + 3;
                    eMessageMain(&panel->items[i]);
                    /* Two-byte glyph pairs followed by the original terminating byte. */
                    ordinalSource = msg_number[i + 1];
                    printY = panel->windows[1].y + i * 24 + 2;
                    printX = panel->windows[1].x + 3;
                    ordinalGlyph = ordinalSource[0];
                    __builtin_memcpy(ordinalText, D_004DB090, 5);
                    ordinalText[0] = ordinalGlyph;
                    xglFontPrint(printX, printY, 0xffffff, (const char *)ordinalText);
                    quantitySource = msg_number[SeisanItemBox[i].count];
                    printY = panel->windows[1].y + i * 24 + 2;
                    printX = panel->windows[1].x + 221;
                    *(SeisanLabelBytes *)quantityText = *(const SeisanLabelBytes *)D_004DB098;
                    quantityGlyph = quantitySource[0];
                    quantityText[1] = quantityGlyph;
                    xglFontPrint(printX, printY, 0xffffff, (const char *)quantityText);
                }
            }
        }
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", tskSeisanStatus);

static inline void SeisanDrawFade(SeisanFadeData *fade)
{
    endPrintExtFunc(0x02ffffff, 3, fade->vertices);
}

void SeisanFadeMain(void)
{
    SeisanFadeData *fade = SeisanFade;
    int i;

    switch (fade->state) {
    case 0:
        fade->mode = 0;
        for (i = 0; i < 4; i++) {
            fade->vertices[i].alpha = 0;
            fade->vertices[i].blue = 0;
            fade->vertices[i].green = 0;
            fade->vertices[i].red = 0;
            fade->vertices[i].depth = 0xffffff;
        }
        fade->vertices[0].x = 0;
        fade->vertices[0].y = 0;
        fade->vertices[1].x = 0;
        fade->vertices[1].y = 448;
        fade->vertices[2].x = 512;
        fade->vertices[2].y = 0;
        fade->vertices[3].x = 512;
        fade->vertices[3].y = 448;
        fade->state = 2;
        fade->terminatorDepth = 0;
    case 2:
        if (SeisanWork[SEISAN_WORK_STATE] == SEISAN_STATE_CLOSING) {
            int alpha = (16 - SeisanWork[5]) * 8;
            fade->vertices[3].alpha = alpha;
            fade->vertices[2].alpha = alpha;
            fade->vertices[1].alpha = alpha;
            fade->vertices[0].alpha = alpha;
        }
        SeisanDrawFade(fade);
        break;
    }
}

int subSeisanHissatuCheck00(unsigned char *seisan_work)
{
    int i;

    for (i = 0; i < SeisanWork[6]; i++)
    {
        if (SeisanResult->entries[i].count != 0)
        {
            return 0x50;
        }
    }
    return 0;
}

int subSeisanHissatuCheck01(void)
{
    int i;

    for (i = 0; i < SeisanWork[6]; i++)
    {
        if (SeisanResult->entries[i].count >= 5)
        {
            return 0x58;
        }
    }
    return 0;
}

int subSeisanEtherCheck(void)
{
    return 0;
}

int subSeisanItemCheck(void)
{
    return 0x80;
}

void subSeisanCheck(void)
{
    switch (SeisanWork[SEISAN_WORK_STATE]) {
    case 0x20:
        SeisanWork[SEISAN_WORK_NEXT_STATE] =
            subSeisanHissatuCheck00(SeisanWork);
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x50:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanHissatuCheck01();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x58:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanEtherCheck();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x60:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanItemCheck();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x80:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = SEISAN_STATE_CLOSING;
        SeisanWork[SEISAN_WORK_FLAGS] |= 4;
        break;
    }
}

void subSeisanMain(void)
{
    if (SeisanWork[1] != SeisanWork[3]) {
        SeisanWork[1] = SeisanWork[3];
        SeisanWork[2] |= 1;
    } else {
        SeisanWork[2] &= 0xfe;
    }
    if (SeisanWork[5] != 0)
        SeisanWork[5]--;

    switch (SeisanWork[1]) {
    case 0x20:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 0x20;
            subSeisanCheck();
        }
        if (SeisanWork[5] == 0 && SeisanCN.state == 0)
            SeisanCN.state = 1;
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0) {
            if (SeisanCN.state == 3)
                SeisanWork[3] = SeisanWork[4];
            else
                SeisanCN.state = 2;
        }
        SeisanCountMain(0);
        break;
    case 0x30:
        break;
    case 0x50:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x58:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x60:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x80:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0xf0:
        if ((unsigned char)(SeisanWork[2] & 1))
            SeisanWork[5] = 16;
        if (SeisanWork[5] == 0) {
            unsigned char *work;
            MenuBgTaskBreak();
            work = SeisanWork;
            work[3] = 0xff;
            work[1] = 0xff;
        } else {
            SeisanWork[5]--;
        }
        break;
    }
    SeisanWork[2] &= 0xfd;
}

int SeisanMain(void)
{
    subSeisanMain();
    return SeisanWork[SEISAN_WORK_STATE] != SEISAN_STATE_FINISHED;
}

void SeisanDisp(void)
{
    SeisanRibbonData ribbon;

    ribbon = D_004C7350;
    endPrintDirectRibbon(&ribbon);
    endPrintExtFunc(0, 100, 0);
    if (MenuLoadSync())
    {
        xglTaskExecute(SeisanBgTask);
    }
    else
    {
        MenuBgTaskMain();
        xglTaskExecute(SeisanBgTask);
        xglTaskExecute(SeisanTask);
        SeisanFadeMain();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanInit);

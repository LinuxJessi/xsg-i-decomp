#include "common.h"
#include "main/e_battle_win_open.h"

void WindowSPSet(WindowSPCursor *cursor);
void WindowSPMain(WindowSPCursor *cursor);
void WindowSPSelectJump(WindowSPCursor *cursor, int selection);

#include "main/open_close_main.h"
#include "main/end_print.h"
#include "main/e_number_main.h"
#include "main/menu_1.h"

#include "shared.h"

#include "file.h"

#include "main/xgl_jpeg.h"

#include "main/menu_top_command_check.h"

typedef struct FileThumbnailRequest {
    u32 source;
    u32 destination;
    u8 unmodeled_08[8];
    u16 destination_x;
    u16 destination_y;
    u16 width;
    u16 height;
    u8 unmodeled_18[8];
} FileThumbnailRequest;

/* FileWork+0x09 is the signed current-slot byte used by the JPEG routines. */

typedef struct FileWorkCurrentSlotView {
    unsigned char unmodeled_00[0x09];
    signed char current_slot;
} FileWorkCurrentSlotView;

/* Five decoder buffers are 0xe100 bytes apart; their signed object number is +1. */

typedef struct FileJpegSlotView {
    unsigned char unmodeled_00;
    signed char number;
    unsigned char unmodeled_02[0xe100 - 0x02];
} FileJpegSlotView;

typedef struct FileJpegDecodeSlotView {
    unsigned char unmodeled_00;
    signed char number;
    unsigned char unmodeled_02[0x80 - 0x02];
    unsigned int dma_qwc;
    unsigned int dma_address;
    unsigned int vif_nop;
    unsigned int vif_directhl;
    unsigned int gif_tag_low;
    unsigned int gif_tag_high;
    unsigned int gif_regs_low;
    unsigned int gif_regs_high;
    unsigned char image[0xe100 - 0xa0];
} FileJpegDecodeSlotView;

typedef struct FileObjectEntryView {
    struct FileJpegDecodeSlotView *jpeg;
    unsigned char thumbnail[0x1000];
    unsigned char state;
    signed char number;
    unsigned char unmodeled_1006;
    unsigned char chapter;
    unsigned char leader;
    unsigned char unmodeled_1009[3];
    int map_number;
    int party_ids[16];
    u64 play_time;
    XglClock elapsed;
} FileObjectEntryView;

void FileJpegDecode(int number);

extern char D_004DAA00[];

extern char D_004DA9F8[];

extern char D_004C3A90[];

static char *name_0_0036D628[3] = {
    D_004DAA00,
    D_004DA9F8,
    D_004C3A90,
};

FileWorkBlock *FileWork = 0;

byte *FileJpegDec = 0;

byte *FileObjectData = 0;

#include "file.private.h"

struct FilePassWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    int (*draw)(MenuStatusPanelWindow *window, MenuTopMenuPanel *data);
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};

struct FileInfoWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    int (*draw)(MenuStatusPanelWindow *window, MenuTopInfoPanel *data);
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};

struct FilePassClip {
    short x;
    short y;
    int color;
    short width;
    short height;
};

struct FileTagFont {
    signed char state;
    signed char sub_state;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    signed char channels[4];
    unsigned char unmodeled_10[0x1c - 0x10];
    const char *text;
};

struct FileNumber {
    short x;
    short y;
    int color;
    signed char channels[4];
    signed char mode;
    unsigned char unmodeled_0d;
    unsigned char format;
    unsigned char digits;
    unsigned char flags[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
};

struct FileFaceSprite {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int color;
    signed char channels[4];
    unsigned char unmodeled_10[0x24 - 0x10];
    short sprite_id;
    unsigned char unmodeled_26[2];
};

struct FileRibbon {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[0xe0 - 0x0c];
};

struct FileThumbnailPacket {
    short x;
    short y;
    int color;
    signed char channels[4];
    void *packet;
};

struct FileThumbnailContext {
    unsigned char state;
    unsigned char visible;
    unsigned char slot;
    unsigned char unmodeled_03;
    short x;
    short y;
    int color;
    struct FilePassWindow window;
    struct FileTagFont labels[2];
    unsigned char unmodeled_1e0[0x200 - 0x1e0];
    FileMessage messages[2];
    struct FileNumber numbers[5];
    unsigned char unmodeled_558[0x638 - 0x558];
    struct FileFaceSprite faces[6];
    unsigned char unmodeled_728[0x778 - 0x728];
    struct FileTagFont slot_label;
    struct FileNumber slot_number;
    struct FileRibbon ribbon;
    struct FileThumbnailPacket thumbnail;
};

struct FileCursor {
    signed char state;
    signed char active;
    signed char timer;
    signed char width;
    short x;
    short y;
    int work;
    signed char color[4];
    unsigned char unmodeled_10[0x24 - 0x10];
};

struct FileDataContext {
    unsigned char state;
    unsigned char visible;
    unsigned char extra_panel;
    unsigned char initial_flags;
    int initial_value;
    int work;
    struct FileThumbnailContext panels[4];
    struct FileCursor cursor;
    struct FilePassClip clip;
};

typedef struct FilePasWork {
    unsigned char unmodeled_00[0x04];
    short x;
    short y;
    int color;
    struct FilePassWindow window;
    FileMessage messages[6];
    unsigned char unmodeled_338[0x3c0 - 0x338];
    struct FilePassClip rect;
} FilePasWork;

extern char *msg00_1_0036D638[6];

extern int MenuPasLengthGet(const char *text);

struct FileInfoWindowData {
    short x;
    short y;
    unsigned char unmodeled_04[4];
    int mode;
    char *text;
};

struct FileInfoContext {
    unsigned char unmodeled_00[4];
    int work;
    struct FileInfoWindow window;
    unsigned char unmodeled_19c[0x1b0 - 0x19c];
    struct FileInfoWindowData contents;
    unsigned char unmodeled_1c0[0x360 - 0x1c0];
    char text[256];
};

struct FileDeviceTexts {
    char *text[3];
};

struct FileOperationTexts {
    char *text[3];
};

struct FileSelectionTexts {
    char *text[4];
};

struct FileListTexts {
    char *text[2];
};

struct FileConfirmationTexts {
    char *text[12];
};

struct FileSlotTexts {
    char *text[3];
};

struct FileSlotStateTexts {
    char *text[2][3];
};

struct FileCardTexts {
    char *text[7];
};

struct FileRequestTexts {
    char *text[9];
};

struct FileFormatTexts {
    char *text[3];
};

struct FileFinishTexts {
    char *text[2];
};

struct FileProgressTexts {
    char *text[4];
};

extern char *D_004C3D48[3];

extern char *D_004C3DF0[3];

extern char *D_004C3EA0[4];

extern char *D_004DAA28[];

extern char *D_004C4018[12];

extern char *D_004C40E0[3];

extern char *D_004C41B0[6];

extern char *D_004C4290[7];

extern char *D_004C42E8[9];

extern char *D_004C4418[3];

extern char *D_004DAA38[];

extern char *D_004C45E0[4];

extern char D_004DAA40[], D_004DAA48[], D_004C4310[];

extern void eMessageCpy(char *destination, char *source);

extern void eMessageCat(char *source);

struct FileLoadGauge {
    short x;
    short y;
    unsigned int vertex_attribute;
    short width;
    short height;
    unsigned short current;
    unsigned short maximum;
    struct FileGaugeColor {
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char alpha;
    } filled_color;
    struct FileGaugeColor unfilled_color;
};

extern struct FileLoadGauge gauge;

void tyaDrawGauge(struct FileLoadGauge *progress);

typedef struct FileObjectImages {
    FileObjectEntryView slots[99];
    FileObjectEntryView templates[3];
    FileObjectEntryView current;
} FileObjectImages;

extern XglTaskScheduler *FileTask;

extern unsigned char *FileCardDataBuf;

extern void *FileExistCheckWork;

extern unsigned char D_004C4D80[];

extern unsigned char D_004C4D98[];

extern unsigned char D_004C4DB0[];

extern unsigned char D_004C4DC8[];

void endPrintDirectFrameCopy(XglPacket *packet, int source, int destination);

void endPrintInit(void);

void MenuCfTaikiPush(void);

void MenuCfTaikiPop(void);

XglTaskScheduler *MenuBgTaskInit(void *work, int mode);

void *xglTaskInitial(void *work, int entries, int mode);

void MenuBgTaskMain(void);

void MenuBgTaskBreak(void);

void MenuLoadInit(void);

void MenuFontLoad(int mode, int wait);

void WindowTexLoad(int mode, int resource);

void *MenuSaveMapTextLoad(void *work, int mode);

void MenuLoadFile(const unsigned char *name, void *destination);

void MenuWorkEndCheck(void *end);

int MenuLoadSync(void);

int GameSnapShotSaveThumbnail(int quality, void *destination);

void GameSnapShotCheck(void);

void GameCfPlayerLoadResource(int mode);

void MapChangeFadeSet(void);

void exit(int status);

void *MenuWorkEndGet(void);

void MenuFile(void);

static void VersionUpDate(SaveDataHeader *save_data)
{
    save_data->version += 1;
    FileCheckSumGet((FileChecksumData *)save_data, (long long *)&save_data->checksum);
}

static void VersionChange00To01(SaveDataVersions *save_data)
{
    SaveData01 new_data;

    new_data.added_40 = 0;
    new_data.head = save_data->version_00.head;
    new_data.head.added_3c = 0;
    new_data.added_44 = 0;
    new_data.block = save_data->version_00.block;
    save_data->version_01 = new_data;
    VersionUpDate(&save_data->header);
}

void FileVersionCheck(void)
{
}

int FileObjectDataNoGet(int number, int offset)
{
    number += offset;
    if (number < 0)
        return number + 99;
    if (number < 99)
        return number;
    return number - 99;
}

void FileObjectDataClear(void)
{
    int i;

    for (i = 0; i <= 100; ++i) {
        FileObjectData[i * 0x1060 + 0x1004] = 0;
        FileObjectData[i * 0x1060 + 0x1005] = -1;
    }
    for (i = 0; i <= 4; ++i)
        FileJpegDec[i * 0xe100 + 1] = -1;
}

void FileThumbnailDecode(u32 source, u32 destination)
{
    FileThumbnailRequest request;

    memset(&request, 0, sizeof(request));
    request.source = source;
    request.destination = destination;
    request.destination_x = 0;
    request.destination_y = 0;
    request.width = 0;
    request.height = 0;
    xglJpegDecode(&request);
}

void FileCheckSumGet(FileChecksumData *source, long long *checksum)
{
    FileChecksumData local = *source;
    byte *data = local.data;
    unsigned int count = 0;
    int i = 0;

    local.fields.stored_checksum = 0;
    *checksum = 0;
    do {
        *checksum += (long long)*data * i + 0x7ca;
        ++data;
        ++i;
        ++count;
    } while (count <= 0x163af);
}

int FileCheckSumCheck(FileChecksumData *data)
{
    long long checksum;

    FileCheckSumGet(data, &checksum);
    return data->fields.stored_checksum == checksum;
}

void FileObjectDataInput(int number, int state)
{
    FileObjectEntryView *entry = (FileObjectEntryView *)FileObjectData + number;
    int i;
    int *party_ids;
    PartyStateImage *party;
    short map_number;
    u64 play_time;
    unsigned char *thumbnail;

    memset(entry, 0, sizeof(*entry));
    switch (state) {
    case 2:
        *entry = ((FileObjectEntryView *)FileObjectData)[99];
        break;
    case 3:
        *entry = ((FileObjectEntryView *)FileObjectData)[100];
        break;
    case 4:
        *entry = ((FileObjectEntryView *)FileObjectData)[101];
        break;
    case 1:
        if (FileSaveData->version != 3) {
            *entry = ((FileObjectEntryView *)FileObjectData)[101];
            FileObjectDataSystem->state[number] = 4;
        } else if (!FileCheckSumCheck((FileChecksumData *)FileSaveData)) {
            *entry = ((FileObjectEntryView *)FileObjectData)[101];
            FileObjectDataSystem->state[number] = 4;
        } else {
            party_ids = entry->party_ids;
            entry->chapter = FileSaveData->chapter;
            entry->leader = FileSaveData->leader;
            map_number = FileSaveData->map_number;
            play_time = FileSaveData->play_time;
            entry->elapsed = FileSaveData->elapsed;
            entry->map_number = map_number;
            entry->play_time = play_time;
            PartyTimeDispChange(&entry->elapsed);
            party = (PartyStateImage *)FileSaveData->party;
            memset(party_ids, 0, sizeof(entry->party_ids));
            PartyAllPartyGet2(party_ids, party);
            thumbnail = FileSaveData->thumbnail;
            for (i = 0; i < 0x1000; ++i)
                entry->thumbnail[i] = thumbnail[i];
        }
        break;
    }
    entry->number = number;
}

void FileJpegCheck(void)
{
    signed char valid_numbers[5];
    FileJpegSlotView *slot;
    int i;
    int j;

    i = -1;
    do {
        valid_numbers[i + 1] = FileObjectDataNoGet(((FileWorkCurrentSlotView *)FileWork)->current_slot, i);
        i = i + 1;
    } while (i < 4);

    for (i = 0; i < 5; ++i) {
        slot = (FileJpegSlotView *)FileJpegDec + i;
        for (j = 0; j < 5; ++j) {
            if (slot->number == valid_numbers[j])
                break;
        }
        if (j == 5)
            slot->number = -1;
    }
}

void FileJpegDecode(int number)
{
    FileObjectEntryView *entry = (FileObjectEntryView *)FileObjectData + number;
    FileJpegDecodeSlotView *jpeg = (FileJpegDecodeSlotView *)FileJpegDec;
    FileJpegDecodeSlotView *queued;
    FileJpegDecodeSlotView *slot;
    int i;

    queued = jpeg;
    for (i = 0; i < 5; ++i) {
        if (queued->number == number)
            return;
        ++queued;
    }

    slot = jpeg;
    for (i = 0; i < 5; ++i) {
        if (slot->number < 0) {
            FileThumbnailDecode((u32)entry->thumbnail, (u32)slot->image);
            slot->dma_qwc = 0;
            slot->dma_address = 0;
            slot->vif_nop = 0;
            slot->vif_directhl = 0x51000e01;
            slot->gif_tag_low = 0x8e00;
            slot->gif_tag_high = 0x08000000;
            slot->gif_regs_low = 0;
            slot->gif_regs_high = 0;
            entry->jpeg = slot;
            slot->number = number;
            return;
        }
        ++slot;
    }
}

void FileObjectJpegSet(void)
{
    int offset;
    int slot;

    FileJpegCheck();
    offset = -1;
    do {
        slot = FileObjectDataNoGet(((FileWorkCurrentSlotView *)FileWork)->current_slot, offset);
        offset += 1;
        FileJpegDecode(slot);
    } while (offset < 4);
}

void FileObjectJpegDecChange(int number)
{
    int i;

    for (i = 0; i < 5; ++i) {
        /* The original adds the slot offset to the buffer address as integers
         * (addu offset, base); pointer arithmetic swaps the addu operands. */
        FileJpegSlotView *slot = (FileJpegSlotView *)(i * sizeof(FileJpegSlotView) + (u32)FileJpegDec);

        if (slot->number == number) {
            slot->number = -1;
            FileJpegDecode(number);
            break;
        }
    }
}

int FileObjectAllLoad(void)
{
    struct FileHddLoadRequest {
        int slot;
        FileSaveRecord *data;
        int size;
    } hdd_request;
    McRequestArguments card_request;
    int xglHddMcLoadMount(void);
    int xglHddMcLoadCore(void *request);
    int xglHddMcUmount(void);
    int index;
    int loaded_count;
    int state;
    unsigned char *load_state = &FileObjectDataSystem->load_state;
    unsigned char *load_index = &FileObjectDataSystem->load_index;

    switch (*load_state) {
    case 0: {
        FileWorkBlock *initial_work = FileWork;

        gauge.x = 123;
        gauge.y = 296;
        gauge.vertex_attribute = 0xffffffffU;
        loaded_count = 0;
        gauge.width = 256;
        index = 0;
        gauge.height = 8;
        gauge.current = 0;
        gauge.maximum = 0;
        gauge.filled_color.red = 161;
        gauge.filled_color.green = 232;
        gauge.filled_color.blue = 205;
        gauge.filled_color.alpha = 128;
        gauge.unfilled_color.red = 64;
        gauge.unfilled_color.green = 64;
        gauge.unfilled_color.blue = 64;
        gauge.unfilled_color.alpha = 128;
        __builtin_memset(&initial_work->hdd_result, 0,
                         sizeof(initial_work->hdd_result));
        for (; index < 99; index++) {
            state = FileObjectDataSystem->state[index];

            if (state != 1) {
                int empty = state <= 0;

                if (empty) {
                    continue;
                }
                if (state < 5) {
                    FileObjectDataInput(index, state);
                    loaded_count++;
                }
            } else {
                gauge.maximum++;
            }
        }
        *load_state = 2;
        *load_index = 0;
        if (loaded_count == 99) {
            FileWorkBlock *work = FileWork;

            work->hdd_result = 0;
            if (!work->use_hdd) {
                unsigned short *card_status = work->card_status;

                if (card_status[work->card_number] != 1) {
                    work->hdd_result = -1;
                }
            }
            *load_state = 0;
            return 0;
        }
        if (FileWork->use_hdd) {
            FileWork->hdd_result = xglHddMcLoadMount();
        }
    }
    case 2:
        gauge.current++;
        while (*load_index < 99) {
            if (FileObjectDataSystem->state[*load_index] == 1) {
                *load_state = 4;
                break;
            }
            (*load_index)++;
        }
        if (*load_index == 99) {
            *load_state = 0;
            if (FileWork->use_hdd) {
                FileWork->hdd_result = xglHddMcUmount();
            }
            return 0;
        }
    case 4:
        if (FileWork->use_hdd) {
            hdd_request.slot = *load_index;
            hdd_request.data = FileSaveData;
            hdd_request.size = sizeof(*FileSaveData);
            memset(FileSaveData, 0, sizeof(*FileSaveData));
            FileWork->hdd_result = xglHddMcLoadCore(&hdd_request);
            *load_state = 6;
        } else {
            card_request.slot = *load_index;
            card_request.buffer = FileSaveData;
            card_request.size = sizeof(*FileSaveData);
            memset(FileSaveData, 0, sizeof(*FileSaveData));
            xglMcRequest(FileWork->card_number, 4, &card_request,
                         &FileWork->request_result);
            *load_state = 6;
            tyaDrawGauge(&gauge);
            return 1;
        }
    case 6:
        if (FileWork->use_hdd) {
            if (FileWork->hdd_result != 0) {
                FileObjectDataSystem->state[*load_index] = 4;
                FileObjectDataInput(*load_index, 4);
                *load_state = 9;
            } else {
                FileObjectDataInput(*load_index, 1);
                *load_state = 8;
            }
        } else {
            FileWorkBlock *work = FileWork;
            unsigned short *card_status = work->card_status;
            int status = card_status[work->card_number];

            if (status != 1) {
                if (status != 255) {
                    FileObjectDataSystem->state[*load_index] = 4;
                    FileObjectDataInput(*load_index, 4);
                    FileWork->hdd_result = -1;
                    *load_state = 9;
                }
            } else {
                FileObjectDataInput(*load_index, 1);
                *load_state = 8;
            }
        }
        if (*load_state == 8) {
            *load_state = 2;
            (*load_index)++;
        }
        if (*load_state == 9) {
            *load_state = 0;
            if (FileWork->use_hdd) {
                FileWork->hdd_result = xglHddMcUmount();
            }
            return 0;
        }
        break;
    }
    tyaDrawGauge(&gauge);
    return 1;
}

void FileSinkiSaveDataPush(void)
{
}

void FileSaveDataPush(void)
{
    long long checksum;
    const unsigned char *name;
    int scenario;
    int rank;
    int i;
    short map_no;

    GamePushSaveDataUser();
    for (i = 0; i < 0x1000; ++i)
        ((FileSaveOutput *)SaveData)->thumbnail[i] = ((FileObjectEntryView *)FileObjectData)[102].thumbnail[i];
    map_no = GameLoopState.map_no;
    ((FileSaveOutput *)SaveData)->map_no = map_no;
    name = MenuSaveMapNameGet(map_no);
    xglMcSetMapName(name, name + 33);
    if (map_no == 10)
        ((FileSaveOutput *)SaveData)->unit_map_count++;
    scenario = MenuScenarioNoGet();
    if (scenario > 300)
        rank = 3;
    else if (scenario > 100)
        rank = 2;
    else
        rank = 1;
    ((FileSaveOutput *)SaveData)->version = 3;
    ((FileSaveOutput *)SaveData)->save_count++;
    ((FileSaveOutput *)SaveData)->scenario_rank = rank;
    PartyTimeUpDate();
    xglClockRead(&((FileSaveOutput *)SaveData)->clock);
    FileCheckSumGet((FileChecksumData *)SaveData, &checksum);
    ((FileSaveOutput *)SaveData)->checksum = checksum;
}

char *FileSlotNameGet(int slot)
{
    return name_0_0036D628[slot];
}

void TskObjectSet(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

void tskTskMain(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (FileWork->state == 0xff) {
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

static void MenuFilePas(TskObject *task, void *data)
{
    FilePasWork *work = data;
    int i;
    int lengths[6];
    unsigned short targets[6];
    short slide_target;
    int card_target;

    for (i = 0; i < 6; ++i)
        lengths[i] = MenuPasLengthGet(msg00_1_0036D638[i]);

    switch (task->state) {
    case 0:
        work->x = -272;
        work->y = 8;
        work->color = 0xfffff0;
        WindowDXSet((WindowDX *)(void *)(&work->window));
        work->window.x = work->x;
        work->window.color = work->color;
        work->window.width = 352;
        work->window.y = work->y;
        work->window.height = 32;
        work->window.state = 1;
        WindowDXMain((WindowDX *)(void *)(&work->window));
        work->window.state = 3;
        for (i = 0; i < 6; ++i) {
            eMessageSet((struct EMessageSetParam *)(void *)(&work->messages[i]), msg00_1_0036D638[i]);
            work->messages[i].mode = 32;
            work->messages[i].x = 288;
            work->messages[i].color = work->color + 2;
            work->messages[i].y = work->y + 3;
        }
        break;
    case 2:
        slide_target = -16;
        for (i = 0; i < 6; ++i)
            targets[i] = 288;
        switch (FileWork->state) {
        case 36:
        case 128:
        case 130:
            targets[FileWork->text_index] = 16;
            break;
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:
        case 42:
        case 44:
        case 45:
        case 46:
        case 80:
        case 82:
        case 136:
        case 144:
        case 145:
        case 146:
        case 147:
        case 148:
        case 149:
        case 150:
        case 152:
        case 160:
        case 161:
        case 176:
        case 184:
            targets[FileWork->text_index] = 16;
            if (FileWork->use_hdd)
                targets[4] = targets[FileWork->text_index] + (unsigned short)lengths[FileWork->text_index];
            else
                targets[2 + FileWork->card_number] = targets[FileWork->text_index] + (unsigned short)lengths[FileWork->text_index];
            break;
        case 132:
        case 134:
        case 135:
            targets[FileWork->text_index] = -(unsigned short)lengths[FileWork->text_index];
            targets[5] = 16;
            card_target = (unsigned short)lengths[5] + 16;
            targets[2 + ((union { int i; char c; } *)&FileWork->card_number)->i] = card_target;
            break;
        default:
            slide_target = -352;
            break;
        }
        MoveSlide(&work->window.x, &slide_target, 3.0f);
        WindowDXMain((WindowDX *)(void *)(&work->window));
        work->rect.x = work->window.x + 3;
        work->rect.y = work->window.y + 3;
        work->rect.color = work->color;
        work->rect.width = work->window.width - 6;
        work->rect.height = work->window.height - 6;
        endPrintExtFunc(work->color, 101, &work->rect);
        for (i = 0; i < 6; ++i) {
            MoveSlide(&work->messages[i].x, (short *)&targets[i], 3.0f);
            if (work->messages[i].x < 256)
                eMessageMain((EMessageParam *)(void *)(&work->messages[i]));
        }
        endPrintExtFunc(work->color, 102, 0);
        break;
    }
}

static void MenuFileInfo(TskObject *task, void *data)
{
    char *device_text[3];
    char *operation_text[3];
    char *selection_text[4];
    char *list_text[2];
    char *confirmation_text[12];
    char *slot_text[3];
    char *slot_state_text[2][3];
    char *card_text[7];
    char *request_text[9];
    char *status_text;
    char *format_text[3];
    char *finish_text[2];
    char *progress_text[4];
    short target_y;
    short target_height;
    struct FileInfoContext *view;

    *(struct FileDeviceTexts *)device_text = *(struct FileDeviceTexts *)D_004C3D48;
    *(struct FileOperationTexts *)operation_text = *(struct FileOperationTexts *)D_004C3DF0;
    *(struct FileSelectionTexts *)selection_text = *(struct FileSelectionTexts *)D_004C3EA0;
    view = data;
    *(struct FileListTexts *)list_text = *(struct FileListTexts *)D_004DAA28;
    *(struct FileConfirmationTexts *)confirmation_text = *(struct FileConfirmationTexts *)D_004C4018;
    *(struct FileSlotTexts *)slot_text = *(struct FileSlotTexts *)D_004C40E0;
    *(struct FileSlotStateTexts *)slot_state_text = *(struct FileSlotStateTexts *)D_004C41B0;
    *(struct FileCardTexts *)card_text = *(struct FileCardTexts *)D_004C4290;
    *(struct FileRequestTexts *)request_text = *(struct FileRequestTexts *)D_004C42E8;
    status_text = D_004C4310;
    *(struct FileFormatTexts *)format_text = *(struct FileFormatTexts *)D_004C4418;
    *(struct FileFinishTexts *)finish_text = *(struct FileFinishTexts *)D_004DAA38;
    *(struct FileProgressTexts *)progress_text = *(struct FileProgressTexts *)D_004C45E0;

    switch (task->state) {
    case 0: {
        struct FileInfoWindowData *contents;

        view->work = 0xfffff0;
        WindowDXSet((WindowDX *)(void *)(&view->window));
        view->window.x = -16;
        view->window.y = 480;
        view->window.width = 544;
        view->window.height = 54;
        view->window.state = 0;
        contents = &view->contents;
        view->window.draw = MenuInfoWindow;
        view->window.color = view->work;
        view->window.data = contents;
        contents->x = 0;
        view->window.state = 1;
        view->contents.y = 0;
        view->contents.text = 0;
        WindowDXMain((WindowDX *)(void *)(&view->window));
        view->window.state = 3;
        view->contents.mode = 1;
        memset(view->text, 0, sizeof(view->text));
        break;
    }
    case 2:
        target_y = 378;
        target_height = 54;
        switch (FileWork->state) {
        case 36:
            eMessageCpy(view->text, selection_text[FileWork->text_index]);
            switch (FileWork->device_selection) {
            case 0:
            case 1: {
                unsigned short *card_statuses = FileWork->card_status;

                switch (card_statuses[FileWork->device_selection]) {
                case 0:
                case 129:
                case 130:
                    eMessageCpy(view->text, selection_text[2]);
                    eMessageCat(FileSlotNameGet(FileWork->device_selection));
                    eMessageCat(D_004DAA40);
                    break;
                }
                break;
            }
            case 2:
                eMessageCpy(view->text, device_text[0]);
                break;
            }
            break;
        case 37:
        case 38:
        case 39: {
            unsigned char update_text = FileWork->input_flags & 1;

            if (update_text) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, operation_text[2]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(operation_text[1]);
                } else {
                    eMessageCpy(view->text, operation_text[2]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(operation_text[0]);
                }
            }
            target_y = 352;
            target_height = 78;
            break;
        }
        case 40:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, list_text[FileWork->text_index]);
            target_y = 404;
            target_height = 30;
            break;
        case 41:
            switch (FileObjectDataSystem->state[FileWork->selected_slot]) {
            case 2:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][2]);
                break;
            case 3:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][0]);
                break;
            case 4:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][1]);
                break;
            }
            break;
        case 42:
            if (FileWork->input_flags & 1) {
                if (FileWork->text_index == 0) {
                    if (FileWork->use_hdd) {
                        eMessageCpy(view->text, FileSlotNameGet(2));
                        if (FileObjectDataSystem->state[FileWork->selected_slot] == 2)
                            eMessageCat(confirmation_text[10]);
                        else
                            eMessageCpy(view->text, confirmation_text[1]);
                    } else {
                        int card_slot = FileWork->selected_slot;

                        if (FileObjectDataSystem->state[card_slot] == 2) {
                            eMessageCpy(view->text, confirmation_text[0]);
                            eMessageCat(FileSlotNameGet(FileWork->card_number));
                            eMessageCat(D_004DAA40);
                        } else {
                            eMessageCpy(view->text, confirmation_text[1]);
                        }
                    }
                } else {
                    if (FileWork->use_hdd) {
                        eMessageCpy(view->text, slot_text[2]);
                        eMessageCat(FileSlotNameGet(2));
                        eMessageCat(D_004DAA40);
                    } else {
                        eMessageCpy(view->text, slot_text[0]);
                        eMessageCat(FileSlotNameGet(FileWork->card_number));
                        eMessageCat(D_004DAA40);
                    }
                }
            }
            break;
        case 44:
        case 45:
        case 80:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    if (FileWork->text_index == 0)
                        eMessageCpy(view->text, confirmation_text[4]);
                    else
                        eMessageCpy(view->text, confirmation_text[5]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(confirmation_text[6]);
                } else {
                    if (FileWork->text_index == 0)
                        eMessageCpy(view->text, confirmation_text[7]);
                    else
                        eMessageCpy(view->text, confirmation_text[8]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(confirmation_text[9]);
                }
            }
            target_y = 336;
            target_height = 78;
            break;
        case 132:
            if ((FileWork->input_flags & 1) && !FileWork->use_hdd) {
                eMessageCpy(view->text, confirmation_text[2]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(confirmation_text[11]);
            }
            target_y = 336;
            target_height = 78;
            break;
        case 46:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, confirmation_text[3]);
            break;
        case 82:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, slot_text[1]);
            break;
        case 128:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, card_text[4]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(card_text[6]);
                } else {
                    eMessageCpy(view->text, card_text[0]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(card_text[5]);
                }
            }
            break;
        case 130:
            if (FileWork->input_flags & 1) {
                eMessageCpy(view->text, card_text[1]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(D_004DAA40);
            }
            break;
        case 134:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, card_text[2]);
            break;
        case 135:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, card_text[3]);
            break;
        case 144:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, request_text[FileWork->text_index]);
            break;
        case 145:
            if (FileWork->input_flags & 1) {
                eMessageCpy(view->text, request_text[4]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(D_004DAA48);
            }
            break;
        case 146:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, request_text[6]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(request_text[8]);
                } else {
                    eMessageCpy(view->text, request_text[5]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(request_text[7]);
                }
            }
            break;
        case 148:
        case 149:
        case 150:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, progress_text[FileWork->state - 148]);
            target_y = 336;
            target_height = 78;
            break;
        case 151:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, FileSlotNameGet(2));
                    eMessageCat(request_text[6]);
                } else {
                    char *card_message = view->text;

                    eMessageCpy(card_message, FileSlotNameGet(FileWork->card_number));
                    eMessageCat(request_text[5]);
                }
            }
            break;
        case 160:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, request_text[FileWork->text_index + 2]);
            break;
        case 161:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, status_text);
            target_y = 336;
            target_height = 78;
            break;
        case 176:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, FileSlotNameGet(2));
                    eMessageCat(format_text[1]);
                } else {
                    eMessageCpy(view->text, format_text[0]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(format_text[2]);
                }
            }
            if (FileWork->use_hdd) {
                target_y = 352;
                target_height = 78;
            } else {
                target_y = 328;
                target_height = 102;
            }
            break;
        case 184:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, finish_text[1]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(D_004DAA48);
                } else {
                    eMessageCpy(view->text, finish_text[0]);
                }
            }
            break;
        default:
            target_y = 480;
            break;
        }
        MoveSlide(&view->window.y, &target_y, 3.0f);
        MoveSlide(&view->window.height, &target_height, 3.0f);
        view->contents.text = view->text;
        WindowDXMain((WindowDX *)(void *)(&view->window));
        break;
    }
}




struct FileSelectItem {
    char *text;
    unsigned char unmodeled_04[4];
    signed char color_state;
    unsigned char unmodeled_09[3];
};

struct FileSelectList {
    unsigned char unmodeled_00[2];
    unsigned short layout_mode;
    int selected;
    char *title;
    void *confirmation_data;
    void *items;
    unsigned char unmodeled_14[0x5f8 - 0x14];
};

struct FileSelectContext {
    unsigned char unmodeled_00[2];
    unsigned char initial_flags;
    unsigned char initial_state;
    int work;
    unsigned char animation_flags[4];
    short targets[4];
    struct FilePassWindow windows[4];
    struct FileSelectList lists[4];
    struct FileSelectItem entries[6];
};

struct FileSelectSource {
    char *message;
    char *title;
};

extern char D_004C4858[];
extern char D_004C4870[];
extern char D_004DAA58[];
extern char D_004DAA60[];
extern struct FileSelectSource msg01_2_0036D650[];

static void tskFileSelect(TskObject *task, void *data)
{
    struct FileSelectContext *const view = data;
    int i;

    void ListColorChage(void)
    {
        for (i = 0; i < 2; ++i) {
            unsigned short *card_statuses = FileWork->card_status;
            int card_status = card_statuses[i];
            switch (card_status) {
            case 0:
            case 129:
            case 130:
                view->entries[i].color_state = 1;
                break;
            default:
                view->entries[i].color_state = 0;
                break;
            }
        }
    }

    switch (task->state) {
    case 0: {
        struct FileSelectSource *confirmation;

        view->work = 0xfffff0;
        view->entries[0].text = D_004C4858;
        view->entries[1].text = D_004C4870;
        for (i = 0; i < 2; ++i)
            view->entries[i].color_state = 0;
        view->entries[i].text = D_004DAA58;
        view->entries[i].color_state = 0;
        view->entries[i + 1].text = 0;
        ListColorChage();
        for (i = 0; i < 2; ++i) {
            WindowDXSet((WindowDX *)(void *)(&view->windows[i]));
            view->windows[i].color = view->work;
            view->windows[i].data = &view->lists[i];
            view->windows[i].draw = MenuSelectWindow;
            view->windows[i].width = 245;
            view->windows[i].height = 78;
            view->lists[i].selected = 0;
            view->lists[i].items = view->entries;
            view->windows[i].parameters = D_004DAA60;
            view->lists[i].layout_mode = 2;
        }
        view->windows[i].height = 102;
        confirmation = msg01_2_0036D650;
        for (i = 2; i < 4; ++i) {
            WindowDXSet((WindowDX *)(void *)(&view->windows[i]));
            view->windows[i].color = view->work;
            view->windows[i].data = &view->lists[i];
            view->windows[i].draw = MenuSelectWindow;
            view->windows[i].width = 207;
            view->windows[i].parameters = D_004DAA60;
            view->lists[i].title = confirmation->title;
            view->windows[i].height = 102;
            view->lists[i].selected = 0;
            view->lists[i].confirmation_data = confirmation->message;
            view->lists[i].layout_mode = 1;
        }
        for (i = 0; i < 4; ++i) {
            view->windows[i].x = -258;
            view->windows[i].y = 0;
            view->windows[i].state = 1;
            WindowDXMain((WindowDX *)(void *)(&view->windows[i]));
            view->windows[i].state = 3;
            view->targets[i] = -258;
            view->animation_flags[i] = 0;
        }
        view->initial_flags = 0;
        view->initial_state = 0;
        return;
    }
    case 2: {
        short targets[4];

        for (i = 0; i < 4; ++i)
            targets[i] = -258;
        switch (FileWork->state) {
        case 36:
            if (FileWork->input_flags & 1) {
                view->windows[0].x = 544;
                view->windows[0].y = 176;
            }
            view->lists[0].selected = FileWork->device_selection;
            ListColorChage();
        case 37:
        case 38:
        case 39:
            if (!FileWork->hide_selection)
                targets[0] = 128;
            break;
        case 42:
            if (FileWork->input_flags & 1) {
                view->windows[2].x = 544;
                view->windows[2].y = 224;
            }
            view->lists[2].selected = FileWork->action_selection;
            targets[2] = 160;
            break;
        case 130:
            if (FileWork->input_flags & 1) {
                view->windows[2].x = 544;
                view->windows[2].y = 160;
            }
            view->lists[2].selected = FileWork->action_selection;
            targets[2] = 160;
            break;
        case 160:
            if (FileWork->input_flags & 1) {
                view->windows[3].x = 544;
                view->windows[3].y = 160;
            }
            view->lists[3].selected = FileWork->action_selection;
            targets[3] = 160;
            break;
        }
        for (i = 0; i < 4; ++i) {
            MoveSlide(&view->windows[i].x, &targets[i], 3.0f);
            WindowDXMain((WindowDX *)(void *)(&view->windows[i]));
        }
        break;
    }
    }
}

static void tskFileEx(TskObject *task, FileExplanation *text)
{
    switch (task->state) {
    case 0:
        text->color = 0xffffff;
        eMessageSet((struct EMessageSetParam *)(void *)(&text->message), 0);
        text->message.color = text->color;
        text->message.mode = 32;
        text->open = 0;
        break;
    case 2:
        switch (FileWork->state) {
        case 44:
        case 45:
        case 80:
        case 132:
            if (FileWork->input_flags & 1) {
                if (!text->open) {
                    text->open = 1;
                    text->message.x = 544;
                    if (FileWork->state == 132) {
                        text->target_x = 192;
                        text->message.y = 192;
                        text->message.text = msg00_4_0036D658[2];
                    } else {
                        text->target_x = 224;
                        text->message.y = 256;
                        text->message.text = msg00_4_0036D658[FileWork->text_index];
                    }
                }
            }
            break;
        default:
            text->target_x = -140;
            if (text->message.x == -140)
                text->open = 0;
            break;
        }
        if (text->open) {
            MoveSlide(&text->message.x, &text->target_x, 3.0f);
            do {
                eMessageMain((EMessageParam *)(void *)(&text->message));
            } while (0);
        }
        break;
    }
}

/* Thumbnail display reads the packed time word and signed scenario bytes. */
typedef struct FileThumbnailSaveEntry {
    struct FileJpegDecodeSlotView *jpeg;
    unsigned char unmodeled_004[0x1007 - 0x004];
    signed char chapter;
    signed char leader;
    unsigned char unmodeled_1009[3];
    int map_number;
    int party_ids[16];
    unsigned int play_time;
    unsigned char unmodeled_1054[4];
    unsigned int packed_time;
    unsigned char unmodeled_105c[4];
} FileThumbnailSaveEntry;

extern char D_004C48B8[];
extern char D_004C48C8[];
extern char D_004C48D8[];
extern char D_004C48E8[];
extern char D_004C48F8[];
extern char D_004DAA78[];
extern char *msg00_5_0036D668[];

void FileThumbnailDisp(struct FileThumbnailContext *panel)
{
    int i;
    int party_index;
    int party_count;
    FileThumbnailSaveEntry *entry;
    const unsigned char *map_name;
    unsigned char *window_origin;
    unsigned char *layer_anchor;
    int *slot_layer;
    int *slot_number_layer;
    unsigned char *ribbon_fields;
    unsigned char *leader_fields;
    int ribbon_width;
    int ribbon_height;

    switch (panel->state) {
        do {
    case 0:
        window_origin = (unsigned char *)panel + ((unsigned char *)&panel->window - (unsigned char *)panel);
        WindowDXSet((WindowDX *)(void *)((struct FilePassWindow *)window_origin));
        panel->window.x = 576;
        panel->window.color = panel->color;
        panel->window.width = 336;
        panel->window.parameters = D_004C48B8;
        layer_anchor = (unsigned char *)panel + ((unsigned char *)&panel->window.parameters - (unsigned char *)panel);
        panel->window.height = 96;
        panel->window.state = 1;
        panel->window.y = 0;
        WindowDXMain((WindowDX *)(void *)(&panel->window));
        panel->window.state = 3;
        {
            FileMessage *message = panel->messages;
            for (i = 0; i < 2; ++i) {
                unsigned char *fields;
                int *layer;
                int field_offset;
                eMessageSet((struct EMessageSetParam *)(void *)(&panel->messages[i]), 0);
                field_offset = i * sizeof(FileMessage) +
                    ((unsigned char *)&panel->messages - (unsigned char *)panel) - 16;
                layer = (int *)(layer_anchor + field_offset);
                fields = (unsigned char *)panel + field_offset;
                *layer = panel->color + 2;
                *(unsigned char *)(fields + 16 + ((unsigned char *)&panel->messages[0].mode - (unsigned char *)&panel->messages[0])) = 32;
            }
        }
        {
            struct FileTagFont *label = panel->labels;
            char **text = msg00_5_0036D668;
            unsigned char *layer = layer_anchor +
                ((unsigned char *)&panel->labels[0].color - (unsigned char *)panel) - ((unsigned char *)&panel->window.parameters - (unsigned char *)panel);
            for (i = 0; i < 2; ++i) {
                eTagFontSet((struct ETagFont *)label++, *text++);
                *(int *)layer = panel->color + 2;
                layer += sizeof(struct FileTagFont);
            }
        }
        {
            unsigned char *layer = (unsigned char *)panel +
                ((unsigned char *)&panel->numbers[0].color - (unsigned char *)panel);
            struct FileNumber *number = panel->numbers;
            for (i = 0; i < 5; ++i) {
                eNumberSet(number++, 0);
                *(int *)layer = panel->color + 2;
                layer += sizeof(struct FileNumber);
            }
        }
        slot_layer = &panel->slot_label.color;
        eTagFontSet((struct ETagFont *)&panel->slot_label, D_004DAA78);
        *slot_layer = panel->color + 2;
        slot_number_layer = (int *)(window_origin + ((unsigned char *)&panel->slot_number.color - (unsigned char *)panel) - ((unsigned char *)&panel->window - (unsigned char *)panel));
        ribbon_width = 144;
        ribbon_height = 110;
        eNumberSet(&panel->slot_number, 0);
        *slot_number_layer = panel->color + 2;
        eRibbonSet((struct ERibbonSetData *)&panel->ribbon, 1);
        {
            int field_offset = ((unsigned char *)&panel->ribbon.width - (unsigned char *)panel) - 16;
            int *layer;
            ribbon_fields = (unsigned char *)panel + field_offset;
            layer = (int *)(window_origin + field_offset);
            *(short *)(ribbon_fields + 16) = ribbon_width;
            *layer = panel->color + 1;
            *(short *)(ribbon_fields + 18) = ribbon_height;
        }
        panel->state = 2;
        /* Fall through to the first display. */
    case 2:
        if (!panel->visible)
            break;
        entry = (FileThumbnailSaveEntry *)FileObjectData + panel->slot;
        if (!entry)
            break;
        panel->window.x = panel->x + 144;
        panel->window.y = panel->y;
        WindowDXMain((WindowDX *)(void *)(&panel->window));
        {
            struct FileTagFont *widget = panel->labels;
            int row_offset = 0;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->labels - (unsigned char *)panel) - 16;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 16 + ((unsigned char *)&panel->labels[0].x - (unsigned char *)&panel->labels[0])) = panel->window.x + 67;
                *(short *)(fields + 16 + ((unsigned char *)&panel->labels[0].y - (unsigned char *)&panel->labels[0])) = panel->window.y + row_offset + 54;
                eTagFontMain((struct ETagFontMainData *)widget++);
                fields += sizeof(struct FileTagFont);
                row_offset += 18;
            }
        }
        switch (FileObjectDataSystem->state[panel->slot]) {
        case 2:
            panel->messages[1].text = 0;
            panel->messages[0].text = D_004C48C8;
            break;
        case 4:
            panel->messages[1].text = 0;
            panel->messages[0].text = D_004C48D8;
            break;
        case 3:
            if (FileWork->text_index == 0)
                panel->messages[0].text = D_004C48E8;
            else
                panel->messages[0].text = D_004C48F8;
            panel->messages[1].text = 0;
            break;
        default:
            map_name = MenuSaveMapNameGet(entry->map_number);
            panel->messages[0].text = (char *)map_name;
            panel->messages[1].text = (char *)map_name + 33;
            break;
        }
        {
            FileMessage *widget = panel->messages;
            int row_offset = 0;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->messages - (unsigned char *)panel) - 16;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 16 + ((unsigned char *)&panel->messages[0].x - (unsigned char *)&panel->messages[0])) = panel->window.x + 3;
                *(short *)(fields + 16 + ((unsigned char *)&panel->messages[0].y - (unsigned char *)&panel->messages[0])) = panel->window.y + row_offset + 3;
                eMessageMain((EMessageParam *)(void *)(widget++));
                fields += sizeof(FileMessage);
                row_offset += 24;
            }
        }
        panel->numbers[0].value = entry->packed_time;
        if (FileObjectDataSystem->state[panel->slot] == 1)
            panel->numbers[0].format = 2;
        else
            panel->numbers[0].format = 11;
        panel->numbers[1].value = entry->play_time;
        panel->numbers[1].digits = 9;
        panel->numbers[1].format = 14;
        {
            struct FileNumber *widget = panel->numbers;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->numbers - (unsigned char *)panel) - 24;
            int row_offset = 0;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 24 + ((unsigned char *)&panel->numbers[0].x - (unsigned char *)&panel->numbers[0])) = panel->window.x + 72;
                *(short *)(fields + 24 + ((unsigned char *)&panel->numbers[0].y - (unsigned char *)&panel->numbers[0])) = panel->window.y + row_offset * 2 + 54;
                eNumberMain((struct ENumberMainData *)widget++);
                fields += sizeof(struct FileNumber);
                row_offset += 9;
            }
        }
        panel->numbers[2].digits = 1;
        panel->numbers[2].value = entry->chapter;
        panel->numbers[2].format = 1;
        panel->numbers[3].digits = 2;
        panel->numbers[3].value = entry->leader;
        panel->numbers[3].format = 1;
        leader_fields = (unsigned char *)panel + ((unsigned char *)&panel->numbers[3].x - (unsigned char *)panel) - 24;
        *(short *)(leader_fields + 24) = panel->window.x + 82;
        *(short *)(leader_fields + 26) = panel->window.y - 13;
        party_index = 0;
        eNumberMain((struct ENumberMainData *)&panel->numbers[3]);
        party_count = 0;
        for (; party_index < 16 && entry->party_ids[party_index] != 0; ++party_index) {
            if (++party_count == 6)
                break;
        }
if (party_count > 0) {
            struct FileFaceSprite *face = panel->faces;
            int column_offset = 0;
            int *member = entry->party_ids;
            unsigned char *layer = (unsigned char *)panel +
                ((unsigned char *)&panel->faces[0].color - (unsigned char *)panel);
            party_index = party_count;
            do {
                eSpriteSet((struct ESprite *)face, (short)MenuFaceEpidGet(*member++, 1));
                face->x = panel->window.x + column_offset * 4 + 196;
                *(int *)layer = panel->window.color + 2;
                face->y = panel->window.y + 58;
                eSpriteMain((struct ESpriteMainData *)face++);
                layer += sizeof(struct FileFaceSprite);
                column_offset += 5;
                --party_index;
            } while (party_index != 0);
        }
        panel->ribbon.x = panel->x;
        panel->ribbon.y = panel->y - 14;
        eRibbonMain((struct ERibbonMainData *)&panel->ribbon);
        panel->slot_label.x = panel->x + 8;
        panel->slot_label.y = panel->y - 14;
        eTagFontMain((struct ETagFontMainData *)&panel->slot_label);
        panel->slot_number.digits = 2;
        panel->slot_number.value = panel->slot + 1;
        panel->slot_number.format = 1;
        panel->slot_number.x = panel->slot_label.x + 30;
        panel->slot_number.y = panel->slot_label.y + 2;
        eNumberMain((struct ENumberMainData *)&panel->slot_number);
        panel->thumbnail.packet = &entry->jpeg->dma_qwc;
        panel->thumbnail.x = panel->x + 16;
        panel->thumbnail.y = panel->y + 1;
        panel->thumbnail.color = panel->color + 2;
        panel->thumbnail.channels[0] = panel->thumbnail.channels[1] =
            panel->thumbnail.channels[2] = panel->thumbnail.channels[3] = -128;
        endPrintExtFunc(panel->thumbnail.color, 13, &panel->thumbnail);
        break;
        } while (0);
    }
}

void FileThumbnailDisp(struct FileThumbnailContext *panel);

static void tskFileData(TskObject *task, void *data)
{
    int i;
    int complete;
    short entrance_x;
    short entrance_y;
    short exit_x;
    short exit_y;
    short expanded_y;
    short closing_x;
    struct FileDataContext *view = data;

    switch (task->state) {
    case 0: {
        int offscreen_x = -512;

        view->work = 0xffff00;
        for (i = 0; i < 4; ++i) {
            view->panels[i].color = view->work;
            view->panels[i].y = 64 + i * 112;
            view->panels[i].x = offscreen_x;
            view->panels[i].slot = 0;
            view->panels[i].state = 0;
            view->panels[i].visible = 0;
        }
        view->panels[3].x = 16;
        eCursolSet((ECursol *)&view->cursor, 0);
        view->cursor.work = view->work + 2;
        view->cursor.state = 32;
        view->visible = 0;
        view->initial_value = 0;
        view->initial_flags = 0;
        view->state = 0;
        view->extra_panel = 0;
        return;
    }
    case 2:
        switch (view->state) {
        case 0:
            view->visible = 0;
            view->state = 1;
        case 1:
            if (FileWork->state == 40) {
                view->visible = 1;
                view->state = 10;
            }
            break;
        case 10:
            for (i = 0; i < 3; ++i) {
                view->panels[i].y = 64 + i * 112;
                view->panels[i].x = 528;
            }
            view->cursor.state = 32;
            view->state = 12;
        case 12: {
            int panel_index;

            complete = 0;
            entrance_x = 16;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                MoveSlide(&view->panels[panel_index].x, &entrance_x, 3.0f);
                if (view->panels[panel_index].x == entrance_x) {
                    entrance_y = 64 + panel_index * 112;
                    MoveSlide(&view->panels[panel_index].y, &entrance_y, 3.0f);
                    if (view->panels[panel_index].y == entrance_y)
                        ++complete;
                }
            }
            if (complete == 3)
                view->state = 50;
            break;
        }
        case 14: {
            int exit_complete;
            int panel_index;

            exit_complete = 0;
            exit_x = -512;
            exit_y = 64;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                if (FileWork->selected_panel != panel_index) {
                    MoveSlide(&view->panels[panel_index].x, &exit_x, 3.0f);
                    if (view->panels[panel_index].x == exit_x)
                        ++exit_complete;
                } else {
                    MoveSlide(&view->panels[panel_index].y, &exit_y, 3.0f);
                    if (view->panels[panel_index].y == exit_y)
                        ++exit_complete;
                }
            }
            if (exit_complete == 3)
                view->state = 52;
            break;
        }
        case 16: {

            expanded_y = 112;
            MoveSlide(&view->panels[FileWork->selected_panel].y, &expanded_y, 3.0f);
            if (view->panels[FileWork->selected_panel].y == expanded_y)
                view->state = 52;
            break;
        }
        case 18: {
            int exit_complete;
            int panel_index;

            exit_complete = 0;
            closing_x = -512;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                MoveSlide(&view->panels[panel_index].x, &closing_x, 3.0f);
                if (view->panels[panel_index].x == closing_x)
                    ++exit_complete;
            }
            if (exit_complete == 3)
                view->state = 0;
            break;
        }
        case 50:
            switch (FileWork->state) {
            case 36:
            case 144:
            case 145:
            case 146:
            case 147:
            case 240:
                view->state = 18;
                break;
            case 41:
            case 42:
                view->state = 14;
                break;
            case 44:
            case 45:
            case 46:
            case 80:
                view->state = 16;
                break;
            }
            if (FileWork->scroll_direction) {
                int scroll_speed;
                int scroll_index;

                for (scroll_index = 0; scroll_index < 3; ++scroll_index) {
                    int direction = FileWork->scroll_direction;

                    view->panels[scroll_index].y += direction * 112;
                }
                {
                    FileWorkBlock *scroll_work = FileWork;

                    view->panels[3].x = 16;
                    if (scroll_work->scroll_direction < 0)
                        view->panels[3].y = 288;
                    else
                        view->panels[3].y = 64;
                    scroll_speed = 14;
                    if (scroll_work->fast_scroll)
                        scroll_speed = 28;
                    for (scroll_index = 0; scroll_index < 4; ++scroll_index)
                        view->panels[scroll_index].y += scroll_speed * -FileWork->scroll_direction;
                }
                view->extra_panel = 1;
                view->state = 60;
            }
            break;
        case 52:
            switch (FileWork->state) {
            case 144:
            case 145:
            case 146:
            case 147:
            case 148:
            case 149:
            case 150:
            case 151:
            case 152:
                view->state = 18;
                break;
            case 240:
                view->state = 18;
                break;
            case 44:
            case 45:
            case 46:
                view->state = 16;
                break;
            case 40:
                view->state = 12;
                break;
            }
            break;
        case 60: {
            int scroll_speed = 14;

            if (FileWork->fast_scroll)
                scroll_speed = 28;

            for (i = 0; i < 4; ++i)
                view->panels[i].y += scroll_speed * -FileWork->scroll_direction;
            if (view->panels[0].y == 64) {
                view->extra_panel = 0;
                view->state = 50;
                FileWork->scroll_direction = 0;
            }
            break;
        }
        }
        if (view->visible) {
            FileWorkBlock *work;

            for (i = 0; i < 3; ++i)
                view->panels[i].slot = FileObjectDataNoGet(FileWork->current_slot, i);
            if (view->extra_panel) {
                if (FileWork->scroll_direction < 0)
                    view->panels[3].slot = FileObjectDataNoGet(FileWork->current_slot, 3);
                else
                    view->panels[3].slot = FileObjectDataNoGet(FileWork->current_slot, -1);
            }
            view->clip.x = 0;
            view->clip.y = 48;
            view->clip.color = view->work;
            view->clip.width = 512;
            view->clip.height = 336;
            endPrintExtFunc(view->work, 5, &view->clip);
            endPrintExtFunc(view->work, 101, &view->clip);
            for (i = 0; i < view->extra_panel + 3; ++i) {
                view->panels[i].visible = 1;
                FileThumbnailDisp(&view->panels[i]);
            }
            work = FileWork;
            view->cursor.x = view->panels[work->selected_panel].x;
            view->cursor.y = view->panels[work->selected_panel].y + 32;
            eCursolMain((ECursolMainData *)&view->cursor);
            endPrintExtFunc(view->work, 6, 0);
            endPrintExtFunc(view->work, 102, 0);
        }
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/file", MenuFile);

void endBackTexDraw(unsigned char *color)
{
    signed char rgba[4];
    XglPacket *packet;
    int i;
    FileBackTexturePacket *draw = (FileBackTexturePacket *)0x70000000;

    packet = xglPacketGetCurrent();
    if (color == (unsigned char *)-1) {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = -128;
    } else {
        for (i = 0; i < 4; ++i)
            rgba[i] = color[i];
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    draw->gif_tag = 0xa08b400000008001ULL;
    draw->registers = 0xe53531eeeeULL;
    draw->texture_flush = 0;
    draw->texture_flush_register = 63;
    draw->depth_buffer = 0x131000000ULL;
    draw->depth_buffer_register = 78;
    draw->texture = 0x2000000640000000ULL | (0x24020000 | (sRender.draw_back_value << 5));
    draw->texture_register = 6;
    draw->pixel_test = 0x31001;
    draw->pixel_test_register = 71;
    draw->red = rgba[0];
    draw->green = rgba[1];
    draw->blue = rgba[2];
    draw->alpha_color = rgba[3];
    draw->u0 = 0;
    draw->v0 = 0;
    draw->uv0_zero = 0;
    draw->x0 = 28664;
    draw->y0 = 29176;
    draw->z0_and_adc = 0;
    draw->u1 = 8192;
    draw->v1 = 7168;
    draw->uv1_zero = 0;
    draw->x1 = 0x8ff8;
    draw->y1 = 0x8df8;
    draw->z1_and_adc = 0;
    draw->restored_depth_buffer = 0x31000000;
    draw->restored_depth_buffer_register = 78;
    sceVif1PkAddDirectDataN(packet, draw, 11);
    sceVif1PkCloseDirectHLCode(packet);
}

int MenuFileMain(int menu_mode)
{
    extern void exit(int status) __attribute__((noreturn));
    unsigned char *task_work;
    int buffer_end;
    XglTaskPrefix *entry;
    FileObjectEntryView *images;

    endPrintDirectFrameCopy(xglPacketGetCurrent(), sRender.copy_width << 5,
                           sRender.copy_height << 5);
    GameLoopState.runtime_flags |= 0x04000000;
    xglSleep();
    endPrintInit();
    GameLoopState.runtime_flags |= 0x04000000;
    MenuCfTaikiPush();
    endPrintDirectFrameCopy(xglPacketGetCurrent(), sRender.copy_height << 5,
                           sRender.draw_back_value << 5);
    endBackTexDraw((unsigned char *)-1);
    xglSleep();

    FileWork = (FileWorkBlock *)(((unsigned int)MenuWorkEndGet() + 0x7f) & ~0x7f);
    task_work = (unsigned char *)(FileWork + 1);
    memset(FileWork, 0, sizeof(*FileWork));
    task_work = (unsigned char *)MenuBgTaskInit(task_work, 1);
    FileTask = (XglTaskScheduler *)task_work;
    task_work = xglTaskInitial(task_work, 0x10, 0);

    entry = xglTaskEntryNext(FileTask, (int (*)(XglTaskPrefix *))tskTskMain,
                             FileTask ? FileTask->active_tail : 0);
    TskObjectSet((TskObject *)entry, MenuFilePas, task_work);
    task_work += 0x3cc;
    entry = xglTaskEntryNext(FileTask, (int (*)(XglTaskPrefix *))tskTskMain,
                             FileTask ? FileTask->active_tail : 0);
    TskObjectSet((TskObject *)entry, MenuFileInfo, task_work);
    task_work += 0x460;
    entry = xglTaskEntryNext(FileTask, (int (*)(XglTaskPrefix *))tskTskMain,
                             FileTask ? FileTask->active_tail : 0);
    TskObjectSet((TskObject *)entry, tskFileSelect, task_work);
    task_work += 0x1e8c;
    entry = xglTaskEntryNext(FileTask, (int (*)(XglTaskPrefix *))tskTskMain,
                             FileTask ? FileTask->active_tail : 0);
    TskObjectSet((TskObject *)entry, (TskObjectWorker)tskFileEx, task_work);
    task_work += 0x50;
    entry = xglTaskEntryNext(FileTask, (int (*)(XglTaskPrefix *))tskTskMain,
                             FileTask ? FileTask->active_tail : 0);
    buffer_end = 0x01000000;
    TskObjectSet((TskObject *)entry, tskFileData, task_work);

    FileCardDataBuf = (unsigned char *)buffer_end;
    buffer_end += 0x12000;
    FileObjectDataSystem = (FileObjectSystem *)buffer_end;
    memset(FileObjectDataSystem, 0, 0x258);
    task_work += 0x249c;
    {
        int card;

        for (card = 0; card < 99; ++card)
            FileObjectDataSystem->card_status[card] = -1;
    }

    FileJpegDec = (unsigned char *)(buffer_end + 0x800);
    memset(FileJpegDec, 0, 0x46500);
    FileObjectData = (unsigned char *)((buffer_end + 0x800 + 0x46500 + 0x7ff) & ~0x7ff);
    memset(FileObjectData, 0, 0x696a0);
    buffer_end = ((buffer_end + 0x800 + 0x46500 + 0x7ff) & ~0x7ff) + 0x696a0;
    images = (FileObjectEntryView *)FileObjectData;
    {
        int i;

        for (i = 0; i <= 2; ++i) {
            FileObjectEntryView *leader_images;
            FileObjectEntryView *next_images;

            images[i + 99].chapter = -1;
            leader_images = (FileObjectEntryView *)FileObjectData;
            leader_images[i + 99].leader = -1;
            next_images = (FileObjectEntryView *)FileObjectData;
            next_images[i + 99].play_time = -1;
            images = next_images;
        }
    }
    FileSaveData = (FileSaveRecord *)((buffer_end + 0x7ff) & ~0x7ff);
    FileExistCheckWork = (void *)(((unsigned int)FileSaveData + 0x16baf) & ~0x7ff);
    memset(FileExistCheckWork, 0, 0x2000);
    if (menu_mode == 0) {
        FileObjectImages *objects = (FileObjectImages *)FileObjectData;
        unsigned char *thumbnail = objects->current.thumbnail;

        memset(thumbnail, 0, sizeof(objects->current.thumbnail));
        if (GameSnapShotSaveThumbnail(30, ((FileObjectImages *)FileObjectData)->current.thumbnail) > 0x1000)
            exit(1);
    }
    MenuLoadInit();
    MenuFontLoad(1, 1);
    WindowTexLoad(0, 0x10001);
    WindowTexLoad(0, 0x10002);
    task_work = MenuSaveMapTextLoad(task_work, 1);
    if (menu_mode == 0)
        MenuLoadFile(D_004C4D80, FileCardDataBuf);
    MenuLoadFile(D_004C4D98, ((FileObjectImages *)FileObjectData)->templates[0].thumbnail);
    MenuLoadFile(D_004C4DB0, ((FileObjectImages *)FileObjectData)->templates[1].thumbnail);
    MenuLoadFile(D_004C4DC8, ((FileObjectImages *)FileObjectData)->templates[2].thumbnail);
    MenuWorkEndCheck(task_work);
    if (menu_mode == 0)
        xglSoundEffectNormalID(1, 0);
    while (MenuLoadSync() != 0) {
        MenuBgTaskMain();
        xglSleep();
    }
    FileWork->text_index = menu_mode;
    FileWork->command = 0x24;
    FileWork->input_flags = 2;
    FileWork->result = 0;
    for (;; endPrintExtFunc(0, 100, 0), MenuBgTaskMain(),
            xglTaskExecute(FileTask), xglSleep()) {
        GameSnapShotCheck();
        if (FileWork->state != 0xff) {
            MenuFile();
        } else {
            break;
        }
    }
    MenuBgTaskBreak();
    xglTaskExecute(FileTask);
    MenuCfTaikiPop();
    MenuFontLoad(0, 0);
    WindowTexLoad(0, 0);
    if (menu_mode == 0)
        GameCfPlayerLoadResource(0);
    MapChangeFadeSet();
    GameLoopState.runtime_flags &= 0xfbffffff;
    return FileWork->result == 0;
}

#include "common.h"
#include "shared.h"
#include "xgl_mc.h"

/* The request routines zero and use this complete memory-card save workspace. */
unsigned char SaveData[0x163B0] = { 0 };

/* The state machine uses both a word and per-slot halfwords at offset 4. */
typedef struct McRequestState {
    unsigned char phase;
    unsigned char unmodeled_01[3];
    union {
        int combined;
        short slots[2];
    } status;
} McRequestState;

/* State and its 0x60-byte full-path buffer share the original workspace. */
typedef struct McRequestStorage {
    McRequestState state;
    unsigned char unmodeled_08[0x20 - 0x08];
    unsigned char fullPath[0x60];
} McRequestStorage;
static McRequestStorage mw;

static unsigned char queue_top;
static unsigned char queue_end;

static unsigned char D_0093E120[17 * 2];
extern unsigned char base_0_00491770[];
extern unsigned char n_1[];
extern unsigned char n_2[];

int sceMcOpen(int port, int slot, const char *name, int mode);
int sceMcClose(int fd);
int sceMcRead(int fd, void *buffer, int size);
int sceMcWrite(int fd, const void *buffer, int size);
int sceMcSync(int mode, int *command, int *result);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);

/* One queued memory-card request; the request routines fill the queue slot. */


/* The caller's request parameters: file slot, name, data and size. */

typedef union McRequestPath {
    int empty;
    unsigned char bytes[108];
} McRequestPath;

typedef struct McRequestArguments {
    int slot;
    const unsigned char *path;
    void *buffer;
    int size;
} McRequestArguments;

typedef struct McRequestEntry {
    short card;
    short operation;
    int *result;
    int slot;
    int buffer;
    int size;
    McRequestPath path;
} McRequestEntry;

extern McRequestEntry queue[8];

int sceMcGetDir(int port, int slot, unsigned char *entries, int mode,
                int max_entries, int reserved);

int sceMcMkdir(int port, int slot, unsigned char *path);

int sceMcFormat(int port, int slot);

int sceMcUnformat(int port, int slot);

static void execute(McRequestEntry *request);

static int create_sub2(int result);

unsigned char *xglMcSetFullPath(int card, int selector);

void xglMcWriteMapName(char *buffer, int slot);

int xglMcRequest(int card, int operation, const McRequestArguments *arguments,
                 int *result)
{
    int entry_index = queue_end;
    McRequestEntry *entry;
    int copied;
    signed char *destination;
    const unsigned char *source;
    const unsigned char *path;

    *result = 0;
    entry = &queue[entry_index];
    entry->card = card;
    entry->result = result;
    entry->operation = operation;
    switch (operation)
    {
    case 1:
    case 9:
    case 10:
        break;
    case 2:
    case 4:
    case 6:
    case 8:
        entry->slot = arguments->slot;
        entry->buffer = (int)arguments->buffer;
        entry->size = arguments->size;
        break;
    case 3:
    case 5:
    case 7:
        entry->slot = arguments->slot;
        entry->buffer = (int)arguments->buffer;
        entry->size = arguments->size;
        path = arguments->path;
        if (path == 0)
        {
            entry->path.empty = 0;
        }
        else
        {
            source = path;
            destination = (signed char *)entry->path.bytes;
            copied = 0;
            if ((*destination = *source) != 0)
            {
                do
                {
                    copied++;
                    source++;
                    destination++;
                    if (copied >= 108)
                        break;
                } while ((*destination = *source) != 0);
            }
        }
        break;
    }
    queue_end = (entry_index + 1) & 7;
    return 0;
}

int xglMcGetState(void)
{
    return mw.state.status.combined;
}

void xglMcReset(void)
{
    queue_top = 0;
    queue_end = 0;
    xglMcSetMapName(0, 0);
    mw.state.phase = 0;
}

/*
 * Converts one EUC-JIS double-byte character to Shift-JIS in place: hi and
 * lo hold the character's lead and trail byte, EUC-encoded on entry and
 * Shift-JIS on return. Called from xglMcSetMapName while building a memory
 * card map name.
 */
static void xglMcEUC2SJIS(u8 *hi, u8 *lo)
{
    u32 hi_byte;
    int lo_byte;
    u8 sjis_hi;
    u8 sjis_lo;

    hi_byte = ((*hi) + 0x80) & 0xFF;
    lo_byte = ((*lo) + 0x80) & 0xFF;
    sjis_hi = hi_byte;
    if (sjis_hi & 1)
    {
        lo_byte = lo_byte + 0x1F;
        sjis_hi = (sjis_hi >> 1) + 0x71;
    }
    else
    {
        lo_byte = lo_byte + 0x7D;
        sjis_hi = (sjis_hi >> 1) + 0x70;
    }
    sjis_lo = lo_byte & 0xFF;
    if (sjis_hi >= 0xA0U)
    {
        sjis_hi = (sjis_hi + 0x40) & 0xFF;
    }
    if (sjis_lo >= 0x7FU)
    {
        sjis_lo = (sjis_lo + 1) & 0xFF;
    }
    if ((sjis_hi == 0x87) && (sjis_lo == 0x54))
    {
        sjis_hi = 0x82;
        sjis_lo = 0x50;
    }
    *hi = sjis_hi;
    *lo = sjis_lo;
}

void xglMcSetMapName(const unsigned char *primary_euc_name,
                     const unsigned char *secondary_euc_name)
{
    const unsigned char *primary;
    unsigned char *destination;
    int character_count;
    unsigned char lead;
    unsigned char trail;

    primary = primary_euc_name;
    destination = D_0093E120;
    character_count = 0;

    if (primary != 0)
    {
        lead = primary[0];
        if (lead != 0)
        {
            do
            {
                trail = primary[1];
                primary += 2;
                xglMcEUC2SJIS(&lead, &trail);
                character_count++;
                destination[0] = lead;
                destination[1] = trail;
                destination += 2;
                if (character_count >= 16)
                {
                    break;
                }
                lead = ((const volatile unsigned char *)primary)[0];
            } while (lead != 0);
        }

        if ((character_count < 16) && (secondary_euc_name != 0) &&
            (((const signed char *)secondary_euc_name)[0] != 0))
        {
            destination[0] = 0x81;
            destination[1] = 0x45;
            destination += 2;
            character_count++;
        }
    }

    if ((secondary_euc_name != 0) && (character_count < 16))
    {
        lead = secondary_euc_name[0];
        if (lead != 0)
        {
            do
            {
                trail = secondary_euc_name[1];
                secondary_euc_name += 2;
                xglMcEUC2SJIS(&lead, &trail);
                character_count++;
                destination[0] = lead;
                destination[1] = trail;
                destination += 2;
                if (character_count >= 16)
                {
                    break;
                }
                lead = ((const unsigned char *)secondary_euc_name)[0];
            } while (lead != 0);
        }
    }

    while (character_count < 17)
    {
        destination[0] = 0;
        destination[1] = 0;
        destination += 2;
        character_count++;
    }
}

void xglMcWriteMapName(char *buffer, int slot)
{
    int value = slot + 1;
    int digit = value % 10;

    buffer[229] = digit + 'O';
    value /= 10;
    buffer[227] = value % 10 + 'O';
}

unsigned char *xglMcSetFullPath(int card, int selector)
{
    signed char *dest;
    const unsigned char *source;
    const signed char *prefix_end;

    dest = (signed char *)mw.fullPath;
    source = base_0_00491770;
    while ((*dest = *source) != 0)
    {
        dest++;
        source++;
    }

    if (card < 0)
    {
        *dest = '*';
        dest++;
    }
    else
    {
        dest[1] = '0' + card % 10;
        dest[0] = '0' + (card / 10) % 10;
        dest += 2;
    }

    switch (selector)
    {
    case 0:
        prefix_end = dest;
        for (source = mw.fullPath; source != (const unsigned char *)prefix_end; source++)
        {
            *dest = *source;
            dest++;
        }
        *dest = 0;
        break;
    case 1:
        {
            source = n_1;
        }
        while ((*dest = *source) != 0)
        {
            dest++;
            source++;
        }
        break;
    case 2:
        {
            source = n_2;
        }
        while ((*dest = *source) != 0)
        {
            dest++;
            source++;
        }
        break;
    case -1:
        *dest = 0;
        break;
    default:
        *dest = '/';
        dest++;
        source = (const unsigned char *)selector;
        while ((*dest = *source) != 0)
        {
            dest++;
            source++;
        }
        break;
    }
    return mw.fullPath;
}

static void execute(McRequestEntry *request)
{
    int command = request->operation;

    switch (command)
    {
    case 1:
        sceMcGetInfo(request->card, 0, 0, request->result, 0);
        return;
    case 2:
        if (request->slot == -1)
        {
            xglMcSetFullPath(-1, -1);
            sceMcGetDir(request->card, 0, mw.fullPath, 0, 100,
                        (request->buffer + 0xA3) & ~0x3F);
            mw.state.phase = 6;
            return;
        }
        xglMcSetFullPath(request->slot, 0);
        sceMcOpen(request->card, 0, (const char *)mw.fullPath, 1);
        mw.state.phase = 2;
        return;
    case 3:
        xglMcSetFullPath(0, (int)&request->path.empty);
        sceMcOpen(request->card, 0, (const char *)mw.fullPath, 1);
        mw.state.phase = 2;
        return;
    case 5:
    case 7:
        xglMcSetFullPath(0, (int)&request->path.empty);
        if (request->operation == 7)
        {
            sceMcOpen(request->card, 0, (const char *)mw.fullPath, 0x202);
            mw.state.phase = 3;
        }
        else
        {
            sceMcOpen(request->card, 0, (const char *)mw.fullPath, 1);
            mw.state.phase = 4;
        }
        mw.state.unmodeled_01[0] = 0;
        return;
    case 8:
        xglMcSetFullPath(request->slot, -1);
        sceMcMkdir(request->card, 0, mw.fullPath);
        mw.state.unmodeled_01[0] = 0;
        mw.state.phase = 5;
        return;
    case 6:
        xglMcSetFullPath(request->slot, 0);
        sceMcOpen(request->card, 0, (const char *)mw.fullPath, 0x202);
        mw.state.unmodeled_01[0] = 0;
        mw.state.phase = 3;
        return;
    case 4:
        xglMcSetFullPath(request->slot, 0);
        sceMcOpen(request->card, 0, (const char *)mw.fullPath, 1);
        mw.state.unmodeled_01[0] = 0;
        mw.state.phase = 4;
        return;
    case 9:
        sceMcFormat(request->card, 0);
        return;
    case 10:
        if (1)
        {
            sceMcUnformat(request->card, 0);
        }
        if (request)
        {
            return;
        }
        else
        {
            return;
        }
    default:
        return;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_mc", create_sub2);

INCLUDE_ASM("asm/main/nonmatchings/xgl_mc", xglMcMain);

int xglMcEasyLoad(const char *path, void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;
    int read_result;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (card_type == 0)
    {
        return -1;
    }
    if (result < -1)
    {
        return result;
    }

    sceMcOpen(0, 0, path, 1);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return descriptor;
    }

    sceMcRead(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    read_result = result;
    if (read_result < 0)
    {
        return read_result;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return result;
    }
    return read_result;
}

int xglMcEasySave(const char *path, const void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (result < -1 || card_type == 0)
    {
        return -1;
    }

    sceMcOpen(0, 0, path, 0x202);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return -1;
    }

    sceMcWrite(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return -1;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    return result >= 0 ? 0 : -1;
}

void xglMcInitial(void)
{
    sceMcInit();
    xglMcReset();
}

#include "common.h"

struct FontImage {
    unsigned char unmodeled_00000[0x78040];
    unsigned short proportional_widths[256];
};

struct FontState {
    struct FontImage *font_image;
    void *window_image;
    unsigned short flags;
    unsigned char font_selection;
    unsigned char debug_mode;
    unsigned short line_height;
    unsigned short character_spacing;
    unsigned char default_width;
    unsigned char queue_reset_flag;
    unsigned char proportional_mode;
    unsigned char horizontal_shift;
    unsigned char unmodeled_14[8];
    unsigned char *queue_cursor;
};

/* The partial state view shares the complete 0x20-byte retail allocation. */

typedef union FontStateStorage {
    struct FontState state;
    unsigned char bytes[0x20];
} FontStateStorage;

static FontStateStorage FS;

#define D_00881188 (FS.state.flags)

static void set_xyz(int x, int y, int color);

static void xglFontPrintSub(void *args);

static void xglFontPrintDirectCore(const char *text);

/* Prints text straight away into ordering-table slot ot (passed to set_ot). */

void xglFontPrintDirectOT(int ot, const char *text);

static void set_ot(int ot);

/*
 * Each 4-byte slot of the font print-queue buffer at D_008811A0 (16 slots,
 * scaffold-owned splat name): link is the byte offset, from this array's
 * base, of the next queued record for this bucket (0 marks the bucket
 * empty -- xglFontFlush follows it as a chain), and selfOffset is the
 * slot's own byte offset within the array.
 */

typedef struct FontQueueSlot {
    unsigned short link;
    unsigned short selfOffset;
} FontQueueSlot;

typedef union FontPrintQueueStorage {
    unsigned long long alignment;
    FontQueueSlot slots[16];
    unsigned char bytes[0x2860];
} FontPrintQueueStorage;

static FontPrintQueueStorage D_008811A0;

#define FONT_PRINT_QUEUE (D_008811A0.slots)

/*
 * buffer_reset also reaches two fields 0x20 bytes before the queue buffer:
 * a reset flag byte and the queue's free-cursor pointer. The header these
 * belong to is not otherwise evidenced within this allocation, so they are
 * reached by byte offset from the queue symbol instead of a named member
 * (matching src/ov11/res.c's RES_IsDebugMode).
 */

#define FONT_QUEUE_HEADER_OFFSET 0x20

#define FONT_QUEUE_HEADER_RESET_FLAG_OFFSET 0x11

#define FONT_QUEUE_HEADER_CURSOR_OFFSET 0x1C

/*
 * Draws one glyph from the font texture: u and v are texel coordinates in
 * 1/16 units, size packs the glyph width (high byte) and height (low byte),
 * flags' low nibble selects the draw attributes and bit 4 can double the
 * drawn size.
 */

static void xglFontFlushSub(int u, int v, int size, int flags);

/* Hex digit glyphs are 6x8 texels, 8 texels apart at v 0x2F00 (row 752). */

extern int xglFontGetStringWidth2(const char *text, int);

typedef union FontImageStorage {
    struct FontImage image;
    unsigned char bytes[0x78800];
} FontImageStorage;

static FontImageStorage FontImage;

typedef struct FontState FontState;

#include "main/xgl_cd.h"

#include "main/xgl_packet.h"

struct FontExtCommand {
    unsigned char opcode;
    unsigned char function_high;
    unsigned char function_middle;
    unsigned char function_low;
    unsigned char argument_high;
    unsigned char argument_mid_high;
    unsigned char argument_mid_low;
    unsigned char argument_low;
    unsigned char terminator;
};

struct FontHexCommand {
    unsigned char opcode;
    unsigned char radix;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
    unsigned char terminator;
};

struct FontEffectScratchPacket {
    unsigned long long vif_tag;
    unsigned long long vif_count;
    unsigned long long effect_state;
    unsigned long long payload_count;
};

struct FontDrawCursor {
    XglPacket *packet;
    unsigned short origin_x;
    unsigned char unmodeled_06[2];
    unsigned short x;
    unsigned short y;
    int depth;
    unsigned short offset_x;
    unsigned short offset_y;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char unmodeled_17;
    unsigned char flags;
    unsigned char effect;
    unsigned char attributes;
    unsigned char draw_state_high;
    unsigned char unmodeled_1c[4];
    signed char effect_x;
    signed char effect_y;
    unsigned char unmodeled_22[2];
    unsigned char effect_red;
    unsigned char effect_green;
    unsigned char effect_blue;
    unsigned char unmodeled_27;
    unsigned char primitive_flags;
    unsigned char primitive_mode;
    unsigned char alpha;
    unsigned char unmodeled_2b;
    unsigned short scale_x;
    unsigned short scale_y;
    struct FontEffectScratchPacket effect_packet;
};

struct FontPositionCommand {
    unsigned char opcode;
    unsigned char flags;
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char color_high;
    unsigned char color_middle;
    unsigned char color_low;
};

struct FontCompactPositionCommand {
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char opcode;
    unsigned char color_high;
    unsigned char color_middle;
    unsigned char color_low;
};

union FontPrintArgument {
    unsigned long long integer;
    double real;
    unsigned char *text;
};

struct FontNumericCommand {
    unsigned char opcode;
    unsigned char radix;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
};

struct FontSpriteCorner {
    unsigned int texture_u_or_x;
    unsigned int texture_v_or_y;
    unsigned int backup_x;
    unsigned int backup_y;
    unsigned int x;
    unsigned int y;
    unsigned int depth;
    unsigned int depth_upper;
};

struct FontSpritePacket {
    unsigned int tag_low;
    unsigned int tag_high;
    unsigned int registers_low;
    unsigned int registers_high;
    unsigned long long texture;
    unsigned char unmodeled_18[8];
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
    struct FontSpriteCorner corners[4];
};

#define FONT_ARGUMENT_START(list, last) \
    ((list) = (unsigned long long *)((char *)__builtin_next_arg(last) - \
              (8 - __builtin_args_info(2)) * 8))

extern unsigned short ascii2euc[96];

struct KanjiClutEnvironment {
    unsigned char unmodeled_00[0xb0];
    unsigned short code_by_character[128];
};

extern struct KanjiClutEnvironment KanjiClutEnv;

static const char D_004D2448[16] = "data\\font0.tex";

static const char D_004D2458[16] = "data\\font1.tex";

void xglFontReloadTexture(struct FontDrawCursor *cursor, int flags);

extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern int xglFontCheckProportional(unsigned char high, int low);

unsigned int xglFontGetProportionalSize(int code);

unsigned int xglFontAscii2Euc(signed char character, signed char **text);

static void xglFontFlushCore(struct FontDrawCursor *cursor, const unsigned char *text);

extern unsigned char WindowImage[];

extern int WindowTexLoad(unsigned char *buffer, unsigned int request);

extern const unsigned char D_004DC2A0[];

extern unsigned char spcode_0[32];

struct FontGsRegisterWrite {
    unsigned long long value;
    unsigned long long register_address;
};

struct FontModePacket {
    unsigned int vif_commands[4];
    unsigned long long gif_tag[2];
    struct FontGsRegisterWrite frame;
    struct FontGsRegisterWrite state_registers[5];
};

extern struct FontModePacket ModeEnv[2];

struct FontWindowTextureHeader {
    char signature[4];
    unsigned int total_size;
    int subimage_count;
    unsigned int table_offset;
};

struct FontWindowTextureEntry {
    unsigned int width_and_buffer_width;
    unsigned int height_word;
    unsigned int gs_offset;
    unsigned int dma_quadwords;
    unsigned int data_offset;
};

struct FontWindowTransferPacket {
    unsigned int vif_commands[4];
    unsigned long long gif_tag[2];
    struct FontGsRegisterWrite bitbltbuf;
    struct FontGsRegisterWrite trxpos;
    struct FontGsRegisterWrite trxreg;
    struct FontGsRegisterWrite trxdir;
};

extern const unsigned char KanjiTex0Env_8[];

extern const unsigned char KanjiTex1Env_9[];

extern const unsigned char TestEnv_11_00490BC0[];

extern const unsigned char TestEnv_12_00490C20[];

extern const unsigned char TestEnv_13[];

extern struct FontWindowTransferPacket WinTexEnv_10;

extern unsigned int LoopCount;

extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int words);

extern void sceVif1PkCnt(XglPacket *packet, int mode);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern const signed char tbl_7[];

extern float dptofp(unsigned long long value);

void xglFontDebugMode(int mode)
{
    if (mode >= 0) {
        struct FontModePacket *packets = ModeEnv;
        int value0;
        unsigned long long value1;

        FS.state.debug_mode = mode;
        value1 = (unsigned char)FS.state.debug_mode | 0x50000ULL;
        value0 = 0x50000;
        value0 |= ((unsigned int)FS.state.debug_mode & 0xff) + 0x23;

        packets[0].frame.value = value0;
        packets[1].frame.value = value1;
    }
}

int xglFontGetKanjiClutUV(int code, int *clut)
{
    unsigned short index = code;
    unsigned char column = index >> 8;
    unsigned char row = index;
    int first_range_or_texture_column;
    int second_range;

    first_range_or_texture_column = row < 0xad;
    second_range = row < 0xb0;
    index = (column + row * 94) - 0xa1;
    if (first_range_or_texture_column) {
        index += 0xc4e2;
    } else if (second_range) {
        index += 0xc30c;
    } else {
        index += 0xc250;
    }
    column = index & 0x1f;
    row = index >> 5;
    first_range_or_texture_column = column;
    if (clut != 0) {
        *clut = ((row >> 5) << 12) + 0x1004;
    }
    return ((row & 0x1f) * 0x01800000) +
        (first_range_or_texture_column * 0x140);
}

static void set_ot(int ot)
{
    FontState *state = (FontState *)(D_008811A0.bytes - sizeof(FontState));
    FontQueueSlot *slot = &D_008811A0.slots[((ot >> 22) & 0x3c) / 4];
    unsigned char *queue_bytes = (unsigned char *)&D_008811A0;
    unsigned short offset = state->queue_cursor - queue_bytes;
    unsigned char *previous = queue_bytes + slot->selfOffset;

    previous[0] = offset;
    previous[1] = offset >> 8;
    slot->selfOffset = offset;
    state->queue_cursor[0] = 0;
    state->queue_cursor[1] = 0;
    state->queue_cursor += 2;
}

static void set_xyz(int x, int y, int color)
{
    set_ot(color);
    if ((unsigned int)x >= 768 || y < 0 || y >= 512) {
        struct FontPositionCommand *command = (struct FontPositionCommand *)FS.state.queue_cursor;

        command->opcode = 8;
        command->flags = 7;
        command->x_high = x >> 8;
        command->x_low = x;
        command->y_high = y >> 8;
        command->y_low = y;
        command->color_high = color >> 16;
        command->color_middle = color >> 8;
        command->color_low = color;
        FS.state.queue_cursor += sizeof(*command);
    } else {
        struct FontCompactPositionCommand *command = (struct FontCompactPositionCommand *)FS.state.queue_cursor;

        command->x_high = (x >> 8) + 1;
        command->x_low = x;
        command->y_high = (y >> 8) + 4;
        command->y_low = y;
        command->opcode = 6;
        command->color_high = color >> 16;
        command->color_middle = color >> 8;
        command->color_low = color;
        FS.state.queue_cursor += sizeof(*command);
    }
}

int xglFontGetSPcodeSize(int code, const unsigned char *text)
{
    unsigned char size;

    code &= 0xff;
    size = spcode_0[code];

    if (size == 255) {
        switch (code) {
        case 8: {
            unsigned char parameter_sizes[8];
            unsigned char flags = text[1];
            int bit;

            __builtin_memcpy(parameter_sizes, D_004DC2A0,
                             sizeof(parameter_sizes));
            size = 1;
            for (bit = 0; bit < 8; bit++) {
                if (flags & 1) {
                    size += parameter_sizes[bit];
                }
                flags >>= 1;
            }
            break;
        }
        case 21:
            size = text[1] + 1;
            break;
        }
    }

    return size;
}

static void xglFontPrintSub(void *args)
{
    unsigned long long *list = (unsigned long long *)args;
    const char *text = (const char *)(int)list[0];
    unsigned char ahead = *text;

    list++;
    for (;;) {
        unsigned int code;

        if (*text == 0)
            break;
        code = *text & 0xff;

        if (code < 32) {
            int remaining = xglFontGetSPcodeSize(code, (const unsigned char *)text);

            for (; remaining >= 0; remaining--)
                *FS.state.queue_cursor++ = *text++;
        } else if (code == '%') {
            int width = 0;
            unsigned int radix;

            text++;
            for (;;) {
                code = (unsigned char)*text++;

                switch (code) {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    width = width * 10 + (code - '0');
                    continue;
                case 'b':
                    radix = 2;
                    goto numeric_tail;
                case 'd':
                    radix = 10;
                    goto numeric_tail;
                case 'x':
                    radix = 16;
                    goto numeric_tail;
                numeric_tail:
                    {
                        unsigned long long value = (unsigned int)list[0];
                        struct FontNumericCommand *command = (struct FontNumericCommand *)FS.state.queue_cursor;

                        list++;
                        if (width == 0)
                            width = 1;
                        command->opcode = 17;
                        command->radix = radix;
                        command->digits_minus_one = (unsigned char)(width - 1);
                        command->value_high = value >> 24;
                        command->value_mid_high = value >> 16;
                        command->value_mid_low = value >> 8;
                        command->value_low = value;
                        FS.state.queue_cursor += sizeof(*command);
                    }
                    break;
                case 'f':
                    goto float_tail;
                float_tail:
                    {
                        float value = dptofp(list[0]);
                        unsigned int bits = *(unsigned int *)&value;
                        struct FontNumericCommand *command = (struct FontNumericCommand *)FS.state.queue_cursor;

                        list++;
                        command->opcode = 17;
                        command->radix = 0xff;
                        command->digits_minus_one = 0xff;
                        command->value_high = bits >> 24;
                        command->value_mid_high = bits >> 16;
                        command->value_mid_low = bits >> 8;
                        command->value_low = bits;
                        FS.state.queue_cursor += sizeof(*command);
                    }
                    break;
                case 's': {
                    const unsigned char *string = (const unsigned char *)(int)list[0];

                    int c;

                    list++;
                    while ((c = *string) != 0) {
                        *FS.state.queue_cursor++ = *string++;
                    }
                    break;
                }
                case 'c':
                    code = (unsigned char)list[0];
                    list++;
                    /* fall through */
                case '%':
                    *FS.state.queue_cursor++ = code;
                    break;
                default:
                    break;
                }
                break;
            }
        } else {
            text++;
            *FS.state.queue_cursor++ = ahead;
        }
        ahead = *text;
    }
    *FS.state.queue_cursor++ = 0;
}

static void xglFontPrintDirectCore(const char *text)
{
    while (*text != 0) {
        unsigned char code = *text;

        if (code < 32) {
            int remaining = xglFontGetSPcodeSize(code, (const unsigned char *)text);

            while (remaining > 0) {
                *FS.state.queue_cursor++ = *text++;
                remaining--;
            }
        }
        *FS.state.queue_cursor++ = *text++;
    }
    *FS.state.queue_cursor++ = 0;
}

void xglFontPrintf(int x, int y, unsigned int color, void *arg)
{
    if (D_00881188 & 1 & 0xFFFF) {
        set_xyz(x, y, color);
        xglFontPrintSub(arg);
    }
}

void xglFontPrint(int x, int y, int color, const char *text)
{
    if (D_00881188 & 1 & 0xFFFF) {
        set_xyz(x, y, color);
        xglFontPrintDirectCore(text);
    }
}

void xglFontPrintDirect(const char *text)
{
    xglFontPrintDirectOT(0, text);
}

void xglFontPrintDirectOT(int ot, const char *text)
{
    if (D_00881188 & 1 & 0xFFFF) {
        set_ot(ot);
        xglFontPrintDirectCore(text);
    }
}

void xglFontPrintExtFunc(int ot, unsigned int function_address, unsigned int argument)
{
    struct FontState *state = &FS.state;
    struct FontExtCommand *command;

    if (state->flags & 1 & 0xffff) {
        set_ot(ot);
        command = (struct FontExtCommand *)state->queue_cursor;
        command->opcode = 23;
        command->function_high = function_address >> 16;
        command->function_middle = function_address >> 8;
        command->function_low = function_address;
        command->argument_high = argument >> 24;
        command->argument_mid_high = argument >> 16;
        command->argument_mid_low = argument >> 8;
        command->argument_low = argument;
        command->terminator = 0;
        state->queue_cursor += sizeof(*command);
    }
}

void xglFontDebugPrintf(int x, int y, const char *format, ...)
{
    unsigned long long arguments[32];
    unsigned long long *output;
    unsigned long long *list;

    if ((D_00881188 & 3) == 3) {
        arguments[0] = (unsigned int)format;
        {
        int count;

        FONT_ARGUMENT_START(list, format);
        output = arguments + 1;
        count = 30;
        do {
            unsigned long long *argument = list++;
            unsigned long long value = *argument;

            count--;
            *output = value;
            output++;
        } while (count >= 0);
        }
        if (FS.state.debug_mode == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        *FS.state.queue_cursor++ = 11;
        xglFontPrintSub(arguments);
    }
}

void xglFontDebugHex(int x, int y, unsigned int value, int digits)
{
    struct FontState *state = &FS.state;
    struct FontHexCommand *command;

    if ((state->flags & 3) == 3 && digits != 0) {
        if (state->debug_mode == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        command = (struct FontHexCommand *)state->queue_cursor;
        command->opcode = 11;
        command->radix = 16;
        command->digits_minus_one = digits - 1;
        command->value_high = value >> 24;
        command->value_mid_high = value >> 16;
        command->value_mid_low = value >> 8;
        command->value_low = value;
        command->terminator = 0;
        state->queue_cursor += sizeof(*command);
    }
}

static void buffer_reset(void)
{
    FontQueueSlot *slot;
    struct FontState *state;
    int remaining;

    remaining = 15;
    slot = FONT_PRINT_QUEUE;
    do {
        slot->selfOffset = (unsigned char *)slot - (unsigned char *)FONT_PRINT_QUEUE;
        remaining--;
        slot->link = 0;
        slot++;
    } while (remaining >= 0);

    state = (struct FontState *)(D_008811A0.bytes - sizeof(struct FontState));
    state->queue_cursor = (unsigned char *)slot;
    state->queue_reset_flag = 0;
}

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int size);

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlushSub);

static void xglFontFlushSubHex(int digit)
{
    xglFontFlushSub(digit << 7, 0x2F00, (6 << 8) | 8, 0x13);
}

static void xglFontFlushSubCRLF(void)
{
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)0x70000000;
    int next_y = cursor->y + FS.state.line_height;
    int normal_y;

    cursor->y = next_y;
    cursor->x = cursor->origin_x;
    normal_y = next_y + FS.state.line_height;
    if (FS.state.debug_mode == 0) {
        cursor->y = normal_y;
    }
}


INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlushCore);

int xglFontCheckProportional(unsigned char row, int column)
{
    column &= 0xff;

    if (row == 0xa7) {
        switch (column) {
        case 0xa3:
            return 0x41;
        case 0xa1:
            return 0x40;
        case 0xa6:
            return 0x42;
        default:
            break;
        }
        return -1;
    }
    if (row == 0xa1) {
        if (column == 0xa1) {
            return 0x25;
        }
        if (column == 0xa5) {
            return 0x0a;
        }
    }
    if (row == 0xa3) {
        return tbl_7[(column - 0xa0) >> 5] + (column & 0x1f);
    }
    if (row >= 0xd0) {
        unsigned short glyph = column + row * 94 - 0x4d21;

        row = ~(glyph >> 5) & 7;
        if (row < 2) {
            row ^= 1;
        }
        return (row << 5) + (glyph & 0x1f);
    }
    return -1;
}

unsigned int xglFontGetProportionalSize(int code)
{
    struct FontState *state = &FS.state;
    struct FontImage *font_image = state->font_image;
    unsigned int width;
    signed char average_width;

    width = (unsigned short)(font_image->proportional_widths[code] + 0xff);
    if ((width & 0xff) == 0xff) {
        width = state->default_width << 8;
    }
    if (state->proportional_mode != 0) {
        average_width = ((int)((width & 0xff) + (width >> 8)) >> 1) - 5;
        if (average_width < 0) {
            average_width = 0;
        }
        if (average_width >= 0x0b) {
            average_width = 0x0a;
        }
        width = (((average_width + 9) << 8) + average_width) & 0xffff;
    }
    return width;
}

static unsigned int hex2val(signed char digit)
{
    unsigned int digit_value;

    digit_value = (digit - '0') & 0xff;
    if (digit_value >= 10U) {
        if ((unsigned int)(digit - 'a') < 6U) {
            return (digit - 'W') & 0xff;
        }
        if ((unsigned int)(digit - 'A') < 6U) {
            return (digit - '7') & 0xff;
        }
        return 0U;
    }
    return digit_value;
}

unsigned int xglFontAscii2Euc(signed char character, signed char **text)
{
    unsigned int code = 0xa1a1;

    if (character >= 32) {
        if (character == '#' && text != 0) {
            signed char *cursor = *text;
            signed char next = cursor[1];

            if (next == character) {
                cursor++;
                code = ascii2euc[3];
            } else {
                unsigned int high = hex2val(next);
                unsigned int low = hex2val(cursor[2]);

                cursor += 2;
                code = (high * 16 + low + 32) & 0xffff;

                code = (((code / 94U & 255) << 8) + code % 94U -
                        0x2f5f) & 0xffff;
            }
            *text = cursor;
        } else {
            code = KanjiClutEnv.code_by_character[character];
        }
    }
    return code;
}

void xglFontReloadTexture(struct FontDrawCursor *cursor, int flags)
{
    XglPacket *packet = cursor->packet;
    unsigned int *saved_direct_state;

    cursor->flags |= 0x40;
    if (packet->directCode && (saved_direct_state = packet->directState)) {
        sceVif1PkCloseDirectHLCode(packet);
        if (*saved_direct_state == 0x51000000)
            cursor->packet->write_cursor -= 4;
    }

    flags &= 3;
    if (flags == 1) {
        sceVif1PkRef(cursor->packet, KanjiTex0Env_8, 7, 0, 0, 0);
        sceVif1PkRef(cursor->packet, FS.state.font_image, 15362, 0, 0, 0);
        sceVif1PkRef(cursor->packet, &KanjiClutEnv, 15, 0, 0, 0);
        sceVif1PkRef(cursor->packet, KanjiTex1Env_9, 7, 0, 0, 0);
        sceVif1PkRef(cursor->packet,
                    (unsigned char *)FS.state.font_image + 0x3c020,
                    15362, 0, 0, 0);
    }
    if (flags == 2) {
        struct FontWindowTextureHeader *window = FS.state.window_image;
        struct FontWindowTextureEntry *entry =
            (struct FontWindowTextureEntry *)((unsigned char *)window + window->table_offset);
        int subimage = 0;

        while (subimage < window->subimage_count) {
            unsigned int buffer_width = entry->width_and_buffer_width >> 16;
            unsigned int transfer_width = entry->width_and_buffer_width & 0xffff;
            unsigned int gs_block = (entry->gs_offset + 0xf0000) >> 6;

            WinTexEnv_10.bitbltbuf.value =
                ((unsigned long long)gs_block << 32) |
                ((unsigned long long)buffer_width << 48);
            WinTexEnv_10.trxreg.value = transfer_width |
                ((unsigned long long)entry->height_word << 32);
            sceVif1PkCnt(cursor->packet, 0);
            sceVif1PkAddDataN(cursor->packet, &WinTexEnv_10, 24);
            sceVif1PkRef(cursor->packet, (unsigned char *)window + entry->data_offset,
                        entry->dma_quadwords, 0, 0, 0);
            entry++;
            subimage++;
        }
    }

    sceVif1PkRef(cursor->packet, TestEnv_11_00490BC0, 6, 0, 0, 0);
    if (FS.state.debug_mode) {
        sceVif1PkRef(cursor->packet, &ModeEnv[LoopCount & 1], 8, 0, 0, 0);
    } else {
        const unsigned char *environment;

        switch (cursor->effect) {
        case 0:
        case 2:
        case 3:
        case 4:
            environment = TestEnv_12_00490C20;
            break;
        case 1:
        default:
            environment = TestEnv_13;
            break;
        }
        sceVif1PkRef(cursor->packet, environment, 6, 0, 0, 0);
    }
    sceVif1PkCnt(cursor->packet, 0);
    sceVif1PkOpenDirectHLCode(cursor->packet, 0);
}

extern unsigned char zbuf_14[];
extern unsigned char test_15[];

struct FontQueueRecord {
    unsigned char next_low;
    unsigned char next_high;
};

void xglFontFlush(void)
{
    /* The font drawing cursor resides at the base of EE scratchpad RAM. */
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)0x70000000;
    FontQueueSlot *slot;
    int remaining;

    *FS.state.queue_cursor = 0;
    cursor->depth = -1;
    cursor->blue = 127;
    cursor->green = 127;
    cursor->red = 127;
    if (FS.state.debug_mode) {
        cursor->offset_x = 0;
        cursor->offset_y = 0;
    } else {
        cursor->offset_x = 0x700;
        cursor->offset_y = 0x720;
    }
    cursor->flags = 0;
    cursor->draw_state_high = 0;
    cursor->attributes = 255;
    cursor->effect_y = cursor->effect_x = cursor->effect = 1;
    cursor->effect_red = 0;
    cursor->scale_x = 16;
    cursor->effect_green = 0;
    cursor->primitive_flags = 0x44;
    cursor->alpha = 0x80;
    cursor->effect_blue = 0;
    cursor->primitive_mode = 0;
    cursor->scale_y = 16;
    remaining = 15;
    cursor->packet = xglPacketGetCurrent();
    xglFontReloadTexture(cursor, 0x80000001);

    slot = D_008811A0.slots;
    do {
        unsigned short offset = slot->link;
        if (offset) {
            do {
                struct FontQueueRecord *record =
                    (struct FontQueueRecord *)&D_008811A0.bytes[offset];
                unsigned char *text = (unsigned char *)(record + 1);
                offset = record->next_low + (record->next_high << 8);
                xglFontFlushCore(cursor, text);
            } while (offset);
        }
        remaining--;
        slot++;
    } while (remaining >= 0);
    if (FS.state.debug_mode)
        sceVif1PkAddDirectDataN(cursor->packet, zbuf_14, 6);
    sceVif1PkAddDirectDataN(cursor->packet, test_15, 2);
    sceVif1PkCloseDirectHLCode(cursor->packet);
    buffer_reset();
}

int xglFontGetStringWidth2(const char *text, int delimiter)
{
    struct FontState *state;
    int width = 0;
    signed char compact = 0;
    int first = 1;
    unsigned char high;

    state = &FS.state;
    while (*text != 0 &&
           (high = *text,
            !(!first && delimiter > 0 && high == ' ') &&
                !(!first && delimiter == 2 && high == '/') &&
                high != '\n')) {
        unsigned char low;
        int proportional_code;

        if (high < 32) {
            int remaining;
            switch (high) {
            case 21:
                switch (text[2]) {
                case 2:
                    state->default_width = text[3];
                    break;
                case 4:
                    state->proportional_mode = text[3];
                    break;
                }
                break;
            case 24:
                compact = text[1];
                break;
            case 25:
                state->queue_reset_flag = text[1];
                break;
            }
            remaining = xglFontGetSPcodeSize(
                high, (const unsigned char *)text);
            while (remaining >= 0) {
                remaining--;
                text++;
            }
        } else {
            if (high < 0xa0) {
                if ((signed char)high >= 0) {
                    if (!(state->queue_reset_flag & 2)) {
                        width += 16;
                        text++;
                        first = 0;
                        continue;
                    }
                }
            }
            if ((signed char)high >= 0) {
                unsigned int encoded = xglFontAscii2Euc(high, (signed char **)&text);
                low = encoded;
                high = (encoded >> 8) & 0xff;
                text++;
            } else {
                low = text[1];
                text += 2;
            }
            if (compact && (unsigned char)(high + 0x5d) < 3) {
                width += 8;
            } else {
                proportional_code = xglFontCheckProportional(high, low);
                if ((state->queue_reset_flag & 1) && proportional_code >= 0) {
                    unsigned int bounds = xglFontGetProportionalSize(proportional_code);
                    width += ((bounds >> 8) & 0xff) - (bounds & 0xff) + 1;
                } else {
                    width += 20;
                }
            }
        }
        first = 0;
    }
    return width;
}

int xglFontGetStringWidth(const char *text)
{
    return xglFontGetStringWidth2(text, 0);
}

void *xglFontGetLoadAddress(void)
{
    return FS.state.font_image;
}

unsigned short xglFontGetFlags(void)
{
    return D_00881188;
}

void xglFontSetFlags(int flags)
{
    D_00881188 = flags & 0xFFFD;
}

unsigned char xglFontLoad(int font_index, int load_flags)
{
    unsigned char previous_font;
    int read_mode;

    previous_font = FS.state.font_selection;
    FS.state.font_selection = (unsigned char)font_index;
    read_mode = 1;
    if (load_flags == 0) {
        /*
         * Both fonts take the same synchronous defaults, but the original
         * tests the font here too: the inner branch ends the extended block,
         * so each read below re-addresses FS (lw %lo(FS)) as it does.
         */
        if (font_index != 0) {
            read_mode = 0;
            load_flags = 1;
        } else {
            read_mode = 0;
            load_flags = 1;
        }
    }
    if (font_index == 0) {
        xglCdReadFile(D_004D2448, FS.state.font_image, read_mode, load_flags);
    } else {
        xglCdReadFile(D_004D2458, FS.state.font_image, read_mode, load_flags);
    }
    return previous_font;
}

void xglFontInitial(void)
{
    FS.state.font_selection = 0xFF;
    FS.state.font_image = &FontImage.image;
    xglFontLoad(1, 0);
    FS.state.window_image = WindowImage;
    WindowTexLoad(WindowImage, 0);
    buffer_reset();
    FS.state.flags = 9;
    FS.state.line_height = 8;
    FS.state.default_width = 4;
    FS.state.debug_mode = 0;
    FS.state.proportional_mode = 0;
    FS.state.horizontal_shift = 0;
    FS.state.character_spacing = 0;
}

#include "common.h"

#include "e_message.h"

/* Number of message sprites currently allocated by the print routines. */

static int msg_spr_count = 0;

/*
 * The message object eMessageModeChange, eMessageDraw and eMessageMain
 * receive.  Callers build it on their stack and it extends well past the
 * mode byte (eMessageMain also reads +0x00 and +0x18, TextTest fills +0x04
 * to +0x1C); only the mode byte eMessageModeChange writes is modelled.
 * eMessageMain dispatches on it with lbu, so it is unsigned.
 */

typedef struct EMessageParam {
    unsigned char flags;
    unsigned char mode;
    unsigned char visible;
    unsigned char wrap_width_half;
    short x;
    short y;
    int draw_parameter;
    short text_x_offset;
    short text_y_offset;
    unsigned char color[4];
    unsigned short text_timer;
    unsigned short frame;
    char *text;
    unsigned char page_available;
    unsigned char page_index;
    unsigned char page_count;
    ECursol cursor;
} EMessageParam;

extern int eMessageMain(EMessageParam *message);

extern char *eMessageNextWaitKeySearch(char *text);

typedef struct EMessageTextParam {
    unsigned char unmodeled_00[0x18];
    char *text;
    unsigned char page_available;
    unsigned char page_index;
    unsigned char page_count;
} EMessageTextParam;

typedef struct EMessagePageState {
    unsigned char flags;
    unsigned char mode;
    unsigned char unmodeled_02[26];
    unsigned char page_available;
    unsigned char page_index;
    unsigned char page_count;
} EMessagePageState;

/* Current write position of the message text being built. */

static char *MessageCpyEnd;

extern void eMessageCat(char *src);

/* Number of message sprites currently allocated by the print routines. */




/* Current write position of the message text being built. */

void eMessageSpriteReset(void)
{
    msg_spr_count = 0;
}

void eMessageDrawType00(void)
{
}

int eMessageHalfSpaseCheck(unsigned char *text)
{
    unsigned char character = *text++;
    int half_spaces = 0;

    while ((signed char)character == ' ') {
        character = *text++;
        half_spaces++;
    }
    return half_spaces;
}

int xglFontGetSPcodeSize(int code, const unsigned char *text);

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageNextGyou);

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageNextGyouMaxGet);

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageNextWaitKeySearch);

/* Partial sprite record: byte 18 is not interpreted by this TU. */


INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageDrawType01);

/* Legacy procedure ABI hypothesis: callers discard the function result. */
int eMessageMain(EMessageParam *message)
{
    switch (message->mode) {
    case 0:
        message->visible = 0;
        message->mode = 1;
        /* fall through */
    case 1:
        message->mode = 240;
        break;
    case 32:
        message->visible = 1;
        message->mode = 33;
        message->flags &= 15;
        /* fall through */
    case 33:
        message->mode = 80;
        break;
    case 34:
        message->visible = 1;
        message->mode = 35;
        message->text_timer = 0;
        message->flags = (message->flags & 15) | 16;
        message->frame = 0;
        /* fall through */
    case 35:
        if (message->flags & 128) {
            message->mode = 80;
        } else {
            message->frame++;
            message->text_timer++;
        }
        break;
    case 36:
        message->visible = 1;
        message->mode = 37;
        message->text_timer = 0;
        message->flags = (message->flags & 15) | 48;
        message->frame = 0;
        /* fall through */
    case 37:
        if (message->flags & 128) {
            message->mode = 80;
        } else {
            message->frame++;
            message->text_timer++;
        }
        break;
    case 80:
        message->mode = 81;
        break;
    case 81:
        break;
    case 112:
        message->visible = 0;
        message->mode = 113;
        /* fall through */
    case 113:
        message->mode = 240;
        break;
    case 240:
        message->visible = 0;
        message->mode = 241;
        break;
    case 241:
        break;
    }
    if (message->text != 0 && message->visible != 0) {
        eMessageDrawType01(message);
    }
}

void eMessageModeChange(EMessageParam *message, unsigned char mode)
{
    message->mode = mode;
}

INCLUDE_ASM("asm/main/nonmatchings/e_message", eMessageSet);

void eMessageTextChange(EMessageTextParam *message, char *text)
{
    message->text = text;
    if (message->page_available != 0) {
        message->page_index = 0;
        message->page_count = 0;
        while (*(text = eMessageNextWaitKeySearch(text)) != '\0') {
            message->page_count++;
        }
    }
}

int eMessageNextPage(EMessagePageState *message, int reset_page)
{
    int next_page;
    unsigned char wrapped_page;
    unsigned char next_mode;

    if (message->page_available != 0) {
        if (reset_page == 0) {
            if ((message->flags & 0x80) != 0) {
                next_page = message->page_index + 1;
                wrapped_page = next_page;
                message->page_index = next_page;
                if (message->page_count < wrapped_page) {
                    message->page_index = message->page_count;
                    return 0;
                }
                next_mode = 34;
            } else {
                next_mode = 32;
            }
            message->mode = next_mode;
            return 1;
        } else {
            message->page_index = 0;
        }
    }
    return 0;
}

void eMessageDraw(EMessageParam *message)
{
    eMessageMain(message);
}

void eMessageCpy(char *dst, char *src)
{
    MessageCpyEnd = dst;
    eMessageCat(src);
}

void eMessageCat(char *src)
{
    char *dst = MessageCpyEnd;
    int count;
    int copied;
    int encoded_byte;

    if (src != 0) {
        encoded_byte = (unsigned char)*src;
        while (*src != 0) {
            if ((unsigned int)(encoded_byte - 1) < 25) {
                char command_byte = *src;
                count = xglFontGetSPcodeSize((unsigned char)command_byte,
                                           (unsigned char *)src) + 1;
                for (copied = 0; copied < count; copied++) {
                    *dst++ = *src++;
                }
            } else {
                int control = *src;

                if (control == 30) {
                    *dst++ = *src++;
                    *dst++ = *src++;
                } else if (control == 31 ||
                           (unsigned char)(*src - 32) < 95) {
                    *dst++ = (unsigned char)*src;
                    src++;
                } else {
                    count = eMessageHalfSpaseCheck((unsigned char *)src);
                    if (count != 0) {
                        for (copied = 0; copied < count; copied++) {
                            *dst++ = *src++;
                        }
                    } else {
                        *dst++ = *src++;
                        *dst++ = *src++;
                    }
                }
            }
            encoded_byte = (unsigned char)*src;
        }
        *dst = 0;
        MessageCpyEnd = dst;
    }
}

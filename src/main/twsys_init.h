#ifndef MAIN_TWSYS_INIT_PRIVATE_H
#define MAIN_TWSYS_INIT_PRIVATE_H

#include "main/xgl_font.h"

/* TWindow belongs to main/tu143. Its width at +0x0C is read as a signed short:
 * TWIN_popScene compares a signed halfword there, and TW_setPos sign-extends
 * that halfword before its centering calculation. */

/* TMenu belongs to main/tu143. TMENU_init initializes the pointer at +0x154
 * to its inline string workspace; TMENU_addItem advances it after each line.
 * The pointer at +0x148 retains the start of the active query text. */

/* Declaration proposal owned by main/tu142, msg_init.c.
 * The queue is embedded in text windows at offset 0xAC. */
#include "main/msg_init.h"

#include "main/ew_sprt_set_cursor_uv.h"

typedef struct MsgQueueInfo {
    short count;
    short lines;
    short width;
} MsgQueueInfo;


int MSG_queuePop(MsgQueue *queue, unsigned char *output, int glyphLimit);
void MSG_queueReset(MsgQueue *queue, unsigned int flags);
unsigned int MSG_convert(unsigned char *output, int capacity, const char *text, int length);
char *strcpy(char *destination, const char *source);

/* MBUF storage and allocation are defined by main/tu142, msg_init.c. */

MsgBuffer *MBUF_create2(void);

/* EW slot types are defined by main/tu155, ew_sprt_set_cursor_uv.c. */





/* TWIN_create2 and TWIN_draw2 establish the label's line count at +0x14
 * and its caller-owned line pointer table at +0x24. */



EwWidget *EW_create(int index, int type);
int EW_addComponent(EwContainerWidget *widget, int index, unsigned short *child);

/* Private proposal for the main/tu089 pad owner: the +0x2A pressed halfword
 * is also established by main/tu236's getEdge and main/tu144's slider input.
 * The rest retains the published XglPadRecord layout and 0x68 stride. */
typedef struct XglPadRecord {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2c[0x32 - 0x2c];
    unsigned short mappedPressed; /* +0x32 */
    unsigned short mappedRepeat; /* +0x34 */
    unsigned char unmodeled_36[0x48 - 0x36];
    signed char repeat_delay;
    signed char repeat_interval;
    unsigned short repeat_mask;
    signed char repeat_wait;
    signed char repeat_count;
    unsigned char horizontal_dead_zone;
    unsigned char vertical_dead_zone;
    unsigned char actuator[6];
    unsigned char actuator_pending;
    unsigned char unmodeled_57;
    unsigned char button_map[8];
    unsigned char axis_dead_zone[4];
    signed char axis[4];
} XglPadRecord;

extern XglPadRecord PadData[2];

#endif

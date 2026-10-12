#ifndef INCLUDE_MAIN_E_MESSAGE_H
#define INCLUDE_MAIN_E_MESSAGE_H

#include "main/open_close_main.h"

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

#endif /* INCLUDE_MAIN_E_MESSAGE_H */

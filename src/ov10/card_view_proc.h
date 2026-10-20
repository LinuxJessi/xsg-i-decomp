/*
 * TU-local declarations of ov10/tu005 (src/ov10/card_view_proc.c).
 */

#ifndef SRC_OV10_CARD_VIEW_PROC_H
#define SRC_OV10_CARD_VIEW_PROC_H

#include "shared.h"
#include "ov10/cgp.h"

/* One line of a card list window: a card id and how many copies it holds. */
typedef struct CardListEntry {
    s16 cardId;
    u16 count;
} CardListEntry;

/*
 * A scrolling card list window (0x270 bytes) handed to the card view screen
 * (CardViewProc reads mode +0x04, cursor +0x05, top +0x0C, rows +0x10,
 * count +0x12 and the entries from +0x14, stride 4).
 */
typedef struct CardListWindow {
    s16 active;
    u8 baseColor;                  /* +0x02 */
    u8 cursorColor;                /* +0x03 */
    s8 mode;                       /* +0x04: scroll request, -1 / 0 / 1; lb at 0x00a1c2ec, 0x00a1c304 */
    s8 cursor;                     /* +0x05: lb at 0x00a1c388, 0x00a1c3c0; the decrements reload it with lbu (0x00a1c394) */
    s16 x;                         /* +0x06 */
    s16 y;                         /* +0x08 */
    s16 lineHeight;                /* +0x0A */
    s16 top;                       /* +0x0C */
    s16 columns;                   /* +0x0E */
    s16 rows;                      /* +0x10 */
    s16 count;                     /* +0x12: entries in use */
    CardListEntry entries[151];    /* +0x14 */
} CardListWindow;

/*
 * PadData is the 0xd0-byte table of two 0x68-byte per-pad records at 0x00490d90
 * (the same view card_play_sound1.h declares). The card screens read the halfwords
 * at +0x28 and +0x2a, the d-pad direction halfword at +0x2c, and +0x28..+0x2f as
 * one doubleword. The shared PadPrefix (include/shared.h) extends to +0x36 and has
 * no member at +0x2c, so a record built on it would put `directions` at +0x36.
 */
typedef struct CardPadEntry {
    u8 unmodeled_00[0x28];
    union {
        u64 all;                   /* +0x28: `ld` at 0x00a1cfa8 */
        struct {
            u16 half_28;
            u16 half_2a;
            u16 directions;        /* +0x2c: `lhu` at 0x00a1c30c */
        };
    };
    u8 unmodeled_30[0x68 - 0x30];
} CardPadEntry;

extern CardPadEntry PadData[2];

#endif /* SRC_OV10_CARD_VIEW_PROC_H */

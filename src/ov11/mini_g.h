/*
 * TU-local declarations of ov11/tu001 (src/ov11/mini_g.c).
 *
 */

#ifndef SRC_OV11_MINI_G_H
#define SRC_OV11_MINI_G_H

/* Gwork is the original 0x250-byte local work block shared by the casino
 * games. The word view preserves fields that have not yet been modeled;
 * poker also uses short deck, hand and replacement arrays. */
extern char *SaveWork;

struct PokerDealWork {
    unsigned char unmodeled_000[0x10C];
    short card_used[53];
    short hand_cards[5];
    unsigned char unmodeled_180[0x18];
    short replace_selected[5];
    short sorted_hand[5];
};

/* Slot per-line stake amounts, printed by pay_print. */
struct SlotPayWork {
    unsigned char unmodeled_000[0x56];
    short line_pay[5];
};

/* Nine-game reel state: one 8-byte record per reel starting at 0xC2. */
struct NineGameReel {
    short number;
    short position;
    short state;
    short remaining;
};

struct NineGameWork {
    unsigned char unmodeled_000[0xA8];
    int amount;
    int unknown_ac;
    int unknown_b0;
    int flag;
    int counter;
    short mode;
    short guide;
    short time_left;
    struct NineGameReel reel[9];
};

/* Per-game "initialised" flags used by MiniG_Main. */
struct MiniGModeWork {
    unsigned char unmodeled_000[0x240];
    short initialized[6];
};

union CasinoWork {
    int words[148];
    struct PokerDealWork poker_deal;
    struct SlotPayWork slot;
    struct NineGameWork nine;
    struct MiniGModeWork modes;
};

static union CasinoWork Gwork;

#endif /* SRC_OV11_MINI_G_H */

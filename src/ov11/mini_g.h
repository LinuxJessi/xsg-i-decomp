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
    unsigned char unmodeled_180[0x10];
    /* 0x190: set by PoCardFlip once all five cards have stayed selected
     * for 32 frames, cleared otherwise. Not sure of the game meaning;
     * PoInitWork's notes call it the bonus mode. */
    int bonus_mode;
    /* 0x194: index of the bonus card PoBonusCardAnime deals next. */
    int bonus_round;
    short replace_selected[5];
    short sorted_hand[5];
};

/* Slot work. Only the fields the recovered slot code uses are named;
 * the rest stays as unmodeled byte ranges. */
struct SlotWork {
    unsigned char unmodeled_000[0x20];
    /* 0x20: one state per reel. check_mode treats 1 as "a reel can be
     * stopped"; SlStopBtnPrint picks the stop-button texture from 1, 2 or
     * anything else. Not sure of the full set of values. */
    int reel_state[3];
    unsigned char unmodeled_02c[0xC];
    /* 0x38: pay-line pointer (0..2) moved by the pad in point_control. */
    int pointer;
    /* 0x3C: guide position drawn by point_control when no reel can be
     * stopped. */
    int guide;
    unsigned char unmodeled_040[0x4];
    /* 0x44: 1 shows the help screen (slot_main). */
    int help;
    unsigned char unmodeled_048[0x8];
    /* 0x50: 2 makes slot_main draw the result table instead of the rate
     * table. Not sure of the other values. */
    int state;
    unsigned char unmodeled_054[0x2];
    /* 0x56: per-line stakes, printed by pay_print. */
    short line_pay[5];
    unsigned char unmodeled_060[0x48];
    /* 0xA8: coins still to be paid out (paid_action). */
    int payout;
    /* 0xAC: 1 makes paid_action switch to the nine-game. */
    int start_nine_game;
    /* 0xB0: current slot event, passed to SlEvent by slot_main. */
    int event;
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
    /* 0xA8: coins to be paid out, as in the slot view. */
    int payout;
    unsigned char unmodeled_0ac[0x8];
    int flag;
    int counter;
    short mode;
    short guide;
    short time_left;
    struct NineGameReel reel[9];
};

/* Casino game selected by Gwork.modes.game (see MiniG_Main). */
enum CasinoGame {
    CASINO_MENU = 0,
    CASINO_SLOT = 1,
    CASINO_POKER = 2,
    CASINO_COIN_EXCHANGE = 3,
    CASINO_SHOP = 4,
    CASINO_VIEWER = 5,
    CASINO_NINE_GAME = 6,
    CASINO_TEST_MODE = 7
};

/* Game selector and per-game "initialised" flags used by MiniG_Main. */
struct MiniGModeWork {
    unsigned char unmodeled_000[0x8];
    /* 0x08: the current game, an enum CasinoGame value. */
    int game;
    unsigned char unmodeled_00c[0x234];
    short initialized[6];
};

union CasinoWork {
    int words[148];
    struct PokerDealWork poker_deal;
    struct SlotWork slot;
    struct NineGameWork nine;
    struct MiniGModeWork modes;
};

static union CasinoWork Gwork;

#endif /* SRC_OV11_MINI_G_H */

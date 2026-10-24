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

/* Screen rectangle (x, y, width, height) passed to MakeSprite. */
struct SpriteRect {
    int x;
    int y;
    int w;
    int h;
};

/* Texture descriptor read by MakeSprite (28 bytes, the casino's sprite
 * tables): the source rectangle in the texture, two GS buffer addresses and
 * the texture format. Both addresses are in GS 64-word blocks, relative to
 * block 0x3800: MakeSprite adds 0x3800 before placing them in TEX0. */
struct SpriteTex {
    int u;
    int v;
    int w;
    int h;
    /* TBP0 (texture base) for format 1. The other formats ignore it and use
     * block 0x3800 itself. */
    int texture_base;
    /* CBP (CLUT base), placed at bit 37 of TEX0 for every format. */
    int clut_base;
    /* Picks the rest of the TEX0 word. Decoded from MakeSprite's constants
     * (if I have the GS register layout right): 1 = 4-bit 512x512 at
     * texture_base, 2 = 8-bit 256x256, 3 = 4-bit 1024x1024, anything else =
     * 8-bit 512x512. Formats 1 and 3 also double v. */
    int format;
};

/* 0x10C in PokerDealWork: the deck (one flag per card) and the hand. */
struct PokerCards {
    short used[53];
    short hand[5];
};

/* Poker sequence states (Gwork.poker_deal.seq), as PokerMain handles them.
 * Nothing in OV11 sets 8 or 10. */
enum PokerSeq {
    POKER_SEQ_READY = 0,         /* stake check, "Play N coins" dialog */
    POKER_SEQ_NO_COINS = 1,      /* "There are no more coins." */
    POKER_SEQ_BET = 2,           /* move the stake into the pot */
    POKER_SEQ_DEAL = 3,          /* deal animation (PoCardDrawAnime) */
    POKER_SEQ_DRAW_HAND = 4,     /* fill the hand (PoShuffleCard) */
    POKER_SEQ_SELECT = 5,        /* choose the cards to replace (PoKey) */
    POKER_SEQ_REPLACE = 6,       /* replace them (PoShuffleCard) */
    POKER_SEQ_SCORE = 7,         /* score the hand (PoCardCheck) */
    POKER_SEQ_CONTINUE = 8,      /* "Continue game?" */
    POKER_SEQ_WIN = 9,           /* goes straight on to 11 */
    POKER_SEQ_DOUBLE_UP = 11,    /* "Double or nothing?" */
    POKER_SEQ_BONUS_START = 12,  /* reset the hand (every card set to 0x36) */
    POKER_SEQ_BONUS_DEAL = 13,   /* slide in the next card (PoBonusCardAnime) */
    POKER_SEQ_BONUS_DRAW = 14,   /* give it a value (PoShuffleCard) */
    POKER_SEQ_BONUS_JUDGE = 15,  /* compare it with the previous card */
    POKER_SEQ_BONUS_LOST = 16,   /* wrong guess: wait for circle */
    POKER_SEQ_BONUS_GUESS = 17,  /* three-choice dialog: guess or collect */
    POKER_SEQ_PAYOUT = 18        /* pay the pot out */
};

struct PokerDealWork {
    unsigned char unmodeled_000[0xC];
    /* 0x0C: the bet level chosen in the menu (see MiniGModeWork). */
    int level;
    unsigned char unmodeled_010[0xFC];
    struct PokerCards cards;
    unsigned char unmodeled_180[0x4];
    /* 0x184: nonzero hides the rate table PokerMain draws every frame. */
    int rates_hidden;
    /* 0x188: PokerMain's sequence state, an enum PokerSeq value (stored by
     * PoSeqChange). */
    int seq;
    unsigned char unmodeled_18c[0x4];
    /* 0x190: set by PoCardFlip once all five cards have stayed selected
     * for 32 frames, cleared otherwise. Not sure of the game meaning;
     * PoInitWork's notes call it the bonus mode. */
    int bonus_mode;
    /* 0x194: index of the bonus card PoBonusCardAnime deals next. */
    int bonus_round;
    short replace_selected[5];
    short sorted_hand[5];
    unsigned char unmodeled_1ac[0x4];
    /* 0x1B0: hand category from PoCardCheck (0 = no win). */
    int hand_rank;
    /* 0x1B4: the bonus game's guess: 0 = the next card is not lower than
     * the previous one, 1 = it is lower (a tie wins either way). */
    int guess;
    /* 0x1B8: coins at stake: the bet, then the winnings. */
    int pot;
    /* 0x1BC: 1 while a round is in progress (PokerMain then shows the pot
     * instead of the coin balance). */
    int in_round;
};

/* One slot pay line: the three pictures on it and the result SlPrizeCheck
 * computes for it (prize, kind, and whether the prize is halved). */
struct SlotLine {
    short pic[3];
    short prize;
    short kind;
    short half;
};

/* Slot work. Only the fields the recovered slot code uses are named;
 * the rest stays as unmodeled byte ranges. */
struct SlotWork {
    unsigned char unmodeled_000[0xC];
    /* 0x0C: the bet level chosen in the menu (level_select), which picks
     * the stake step in SlCoinEntry. */
    int level;
    unsigned char unmodeled_010[0x4];
    /* 0x14: reel scroll positions, wrapped back by 0x540 in SlRealDraw. */
    int reel_pos[3];
    /* 0x20: one state per reel. check_mode treats 1 as "a reel can be
     * stopped"; SlStopBtnPrint picks the stop-button texture from 1, 2 or
     * anything else. Not sure of the full set of values. */
    int reel_state[3];
    /* 0x2C: how far each reel has slid since its stop button was pressed
     * (reset by the stop, +0x10 per frame in slot_key). */
    int stop_travel[3];
    /* 0x38: pay-line pointer (0..2) moved by the pad in point_control. */
    int pointer;
    /* 0x3C: guide position drawn by point_control when no reel can be
     * stopped. */
    int guide;
    /* 0x40: one word GameModeChange clears (through Gwork.words); nothing
     * else in the TU reads or writes it, so its role is not modeled. */
    unsigned char unmodeled_040[0x4];
    /* 0x44: 1 shows the help screen (slot_main). */
    int help;
    /* 0x48: reel row used to read the pay lines (SlGetPic), re-rolled by
     * SlChangeRealEffect. */
    short row;
    /* 0x4A: nonzero doubles every prize (SlPrizeCheck). */
    short double_pay;
    /* 0x4C, 0x4E: bonus flag and remaining bonus time, set by SlEvent and
     * SlChangeRealEffect. Not sure of the in-game name of the bonus. */
    short bonus;
    short bonus_time;
    /* 0x50: 2 makes slot_main draw the result table instead of the rate
     * table. Not sure of the other values. */
    int state;
    /* 0x54: total stake, printed by total_pay_print and taken from the
     * coin balance on a spin (slot_key). */
    short total_bet;
    /* 0x56: per-line stakes, printed by pay_print. */
    short line_pay[5];
    /* 0x60: stake steps on each line (0..3), set with left/right in
     * SlCoinEntry; line_pay is this times the current step. */
    short line_bet[5];
    /* 0x6A: the five pay lines (SlLineCheck, SlPrizeCheck, SlResultDraw). */
    struct SlotLine line[5];
    unsigned char unmodeled_0a6[0x2];
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
    unsigned char unmodeled_000[0x4];
    /* 0x04: the row (0..2) exchange_select's three-choice submenu stores.
     * Not sure what the three choices are. */
    int exchange_choice;
    /* 0x08: the current game, an enum CasinoGame value. */
    int game;
    /* 0x0C: the bet level level_select stores (0..3). init_menu_mode and
     * GameModeChange reset it. */
    int level;
    unsigned char unmodeled_010[0x230];
    short initialized[6];
};

/* Viewer work: the image scroll offsets VW_ViewMode moves and clamps
 * (x to -0x242..0, y to -0x250..0) and VW_ViewModeInit resets, and the
 * sample list VW_SamMode shows. */
struct ViewerWork {
    unsigned char unmodeled_000[0x1C0];
    int scroll_x;
    int scroll_y;
    /* 0x1C8: the enabled sound samples, as VW_SamlistInit lists them. */
    int sample[30];
};

/* One reel-change panel of SlChangeRealEffect. */
struct SlReelPanel {
    int state;
    int delay;
    int size;
};

union CasinoWork {
    int words[148];
    struct PokerDealWork poker_deal;
    struct SlotWork slot;
    struct NineGameWork nine;
    struct MiniGModeWork modes;
    struct ViewerWork viewer;
};

static union CasinoWork Gwork;

#endif /* SRC_OV11_MINI_G_H */

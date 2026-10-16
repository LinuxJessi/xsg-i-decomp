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
};

union CasinoWork {
    int words[148];
    struct PokerDealWork poker_deal;
};

static union CasinoWork Gwork;

#endif /* SRC_OV11_MINI_G_H */

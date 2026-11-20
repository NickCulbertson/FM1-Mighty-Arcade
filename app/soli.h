/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* soli.h: SOLITAIRE (Klondike, one card a draw), for Mighty Arcade's menu. Included once from app.c, after its helpers
 * (the colours, header(), degree_note(), the slots).
 *
 * A cursor: left and right go along the row, up and down between the top row and the columns, and within a column
 * up the face-up cards (to take several). A on the stock draws; on a card takes it (and the cards on it); then A on
 * where it goes puts it there. A on the card you hold sends it home to its foundation if it can go. Every move
 * sounds: the card's rank as a note, a foundation's arpeggio rising as it fills, a soft chord under it all. */

enum { SS_PAD, SS_PICK, SS_PLACE, SS_HOME, SS_FLIP, SS_HAT, SS_NO, SS_WIN, SS_BASS, SS_KICK, SS_SNARE };
#define SS_DRAW SS_FLIP                  /* (a draw: plucked, like a card turning) */
#define SS_KEYS SS_PLACE                 /* the sequencer's keys: a card put down sounds on them too */
#define CARD_UP 0x80u
#define C_RANK(c) ((c) & 15u)
#define C_SUIT(c) (((c) >> 4) & 3u)
#define C_RED(c) (C_SUIT(c) == 1u || C_SUIT(c) == 2u)   /* 0 spades 1 hearts 2 diamonds 3 clubs */

typedef struct {
    uint8_t stock[52], waste[52], found[4][13], tab[7][20];
    uint8_t ns, nw, nf[4], nt[7];
    int8_t area, col, depth;             /* the cursor: area 0 the top row (col 0 stock, 1 waste, 3..6 foundations),
                                          * 1 the columns (depth: the card, from the bottom of the pile) */
    int8_t held_area, held_col, held_depth;   /* -1: nothing held */
    uint16_t moves;
    uint8_t won;
    uint32_t won_at;
    /* the juice: cards fly where they go, a turned card flips, the deal deals */
    uint8_t fly_n, fly_area, fly_col, fly_start;   /* the cards in the air: where they land (area 0 top, 1 a column) */
    uint8_t fly_cards[20];
    int16_t fly_x0, fly_y0;
    uint32_t fly_at, flip_at, deal_at;
    int8_t flip_col;
    /* the sequencer: the playhead's last pass over each column (and the waste, 7), when it sounded */
    uint32_t seq_ms[8];
    uint8_t tier;                        /* the drums brought in so far (shown once each) */
} so_t;
static uint16_t so_best;
#define SO_W 32
#define SO_H 44
#define SO_X(c) (2 + (c) * 34)
#define SO_TOP 20
#define SO_TAB 70                 /* the fewest moves to a win (outside the arena: it lasts) */
#define so (*(so_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(so_t) <= sizeof g_arena, "so: the arena is too small");

static void so_deal(void)
{
    uint8_t d[52];
    uint32_t i, j, k = 0;
    for (i = 0; i < 52u; i++)
        d[i] = (uint8_t)((i / 13u) << 4 | (i % 13u + 1u));
    for (i = 51; i > 0; i--) {           /* shuffled */
        uint8_t t;
        j = kr() % (i + 1u);
        t = d[i], d[i] = d[j], d[j] = t;
    }
    for (i = 0; i < 7u; i++) {
        so.nt[i] = (uint8_t)(i + 1u);
        for (j = 0; j <= i; j++)
            so.tab[i][j] = (uint8_t)(d[k++] | (j == i ? CARD_UP : 0u));
    }
    so.ns = 0;
    while (k < 52u)
        so.stock[so.ns++] = d[k++];
    so.nw = 0;
    for (i = 0; i < 4u; i++)
        so.nf[i] = 0;
    so.area = 1, so.col = 0, so.depth = 0;
    so.held_area = -1;
    so.moves = 0;
    so.won = 0;
    so.fly_n = 0;
    so.flip_col = -1;
    so.tier = 0;
    for (i = 0; i < 8u; i++)
        so.seq_ms[i] = 0;
    so.deal_at = plat_ms() | 1u;
}
#define SO_DEAL_MS 45u
static int so_deal_order(int c, int j)    /* the deal's order: across, a row at a time */
{
    int r, k = 0;
    for (r = 0; r < j; r++)
        k += 7 - r;
    return k + c - j;
}
static void so_src_xy(int32_t *x, int32_t *y);
static void so_fly(int area, int col, int start, const uint8_t *cards, int n, int32_t x0, int32_t y0)
{
    int i;
    so.fly_area = (uint8_t)area;
    so.fly_col = (uint8_t)col;
    so.fly_start = (uint8_t)start;
    so.fly_n = (uint8_t)n;
    for (i = 0; i < n && i < 20; i++)
        so.fly_cards[i] = cards[i];
    so.fly_x0 = (int16_t)x0;
    so.fly_y0 = (int16_t)y0;
    so.fly_at = plat_ms() | 1u;
}

static int so_first_up(int c)            /* the column's first face-up card (nt: none) */
{
    int i;
    for (i = 0; i < so.nt[c] && !(so.tab[c][i] & CARD_UP); i++)
        ;
    return i;
}
/* a card's note: its rank up the chord the pad is on (A the root, K two octaves and more up), so always in tune */
static int so_note(uint8_t card) { return h_tone(((int)C_RANK(card) - 1) * 2 / 3); }
/* (its tempo, chance and sound: seq.h's sq_cfg[SQ_SOLI]) */

/* the card held now (the first of them) */
static uint8_t so_held_card(void)
{
    if (so.held_area == 0)
        return so.held_col == 1 ? so.waste[so.nw - 1] : so.found[so.held_col - 3][so.nf[so.held_col - 3] - 1];
    return so.tab[so.held_col][so.held_depth];
}
static int so_held_n(void) { return so.held_area == 1 ? so.nt[so.held_col] - so.held_depth : 1; }

static void so_take_from_source(void)    /* the held cards leave where they were (a column's new top turns up) */
{
    int n = so_held_n();
    if (so.held_area == 0) {
        if (so.held_col == 1)
            so.nw--;
        else
            so.nf[so.held_col - 3]--;
    } else {
        so.nt[so.held_col] = (uint8_t)(so.nt[so.held_col] - n);
        if (so.nt[so.held_col] && !(so.tab[so.held_col][so.nt[so.held_col] - 1] & CARD_UP)) {
            so.tab[so.held_col][so.nt[so.held_col] - 1] |= CARD_UP;
            k_on(0, so_note(so.tab[so.held_col][so.nt[so.held_col] - 1]) + 12, 70, SS_FLIP, 0.5f);
            so.flip_col = so.held_col;
            so.flip_at = plat_ms() | 1u;
        }
    }
    so.held_area = -1;
    so.moves++;
}

static int so_fits_found(uint8_t card, int f)
{
    return so.nf[f] ? C_SUIT(so.found[f][so.nf[f] - 1]) == C_SUIT(card) && C_RANK(so.found[f][so.nf[f] - 1]) + 1u ==
                                                                              C_RANK(card)
                    : C_RANK(card) == 1u;
}
static int so_fits_tab(uint8_t card, int c)
{
    uint8_t top;
    if (!so.nt[c])
        return C_RANK(card) == 13u;
    top = so.tab[c][so.nt[c] - 1];
    return (top & CARD_UP) && C_RED(top) != C_RED(card) && C_RANK(top) == C_RANK(card) + 1u;
}

static void so_to_found(int f)
{
    uint8_t card = so_held_card();
    int i;
    int32_t x0, y0;
    so_src_xy(&x0, &y0);
    so_take_from_source();
    so.found[f][so.nf[f]++] = (uint8_t)(card | CARD_UP);
    {
        uint8_t c1 = (uint8_t)(card | CARD_UP);
        so_fly(0, 3 + f, 0, &c1, 1, x0, y0);
    }
    juice_ring(2 + (3 + f) * 34 + 16, 42, 24 + (int32_t)so.nf[f], C_RED(card) ? C_PINK : C_CYAN, 300);
    juice_burst(2 + (3 + f) * 34 + 16, 20 + 22, 6 + (int32_t)so.nf[f] / 2, 2, C_RED(card) ? C_PINK : C_CYAN, C_YEL, 1);
    juice_embers(2 + (3 + f) * 34 + 16, 42, 3 + (int32_t)so.nf[f] / 3, C_YEL);
    juice_kick(0, -2 - (int32_t)so.nf[f] / 4);   /* the foundation punches up, harder as it fills */
    juice_heat(5 + (int32_t)so.nf[f]);
    if (so.nf[f] == 13u) {               /* a suit home, all of it */
        juice_shake(6, 400);
        juice_freeze(80);
        juice_flash(C_YEL, 90, 300);
        juice_ring(2 + (3 + f) * 34 + 16, 42, 70, C_YEL, 500);
        juice_confetti(2 + (3 + f) * 34 + 16, 42, 20);
        juice_pop(120, 120, "SUIT HOME!", C_YEL);
        juice_impact(60);
        juice_fountain(2 + (3 + f) * 34 + 16, 236, 700);
    }
    for (i = 0; i < 3; i++)              /* the foundation's arpeggio, higher as it fills */
        k_on(k_at(0.07f * (float)i), degree_note((int)so.nf[f] / 2 + 2 * i + 7), 90, SS_HOME, 0.6f);
    if (so.nf[0] + so.nf[1] + so.nf[2] + so.nf[3] == 52u) {
        so.won = 1;
        so.won_at = plat_ms();
        if (!so_best || so.moves < so_best)
            so_best = so.moves;
        juice_flash(RGB(255, 255, 255), 160, 600);
        juice_shake(5, 500);
        juice_celebrate(120, 42);
        juice_fireworks(6000);
        for (i = 0; i < 4; i++)
            juice_burst(2 + (3 + i) * 34 + 16, 42, 14, 5, C_YEL, i & 1 ? C_PINK : C_CYAN, 1);
        for (i = 0; i < 12; i++)
            k_on(k_at(0.4f + 0.12f * (float)i), degree_note(i + 7), 100, SS_WIN, 1.5f);
    }
}

static void so_to_tab(int c)             /* what you hold, onto column c (it fits) */
{
    int n = so_held_n(), i;
    uint8_t cards[20];
    int32_t x0, y0;
    int start = so.nt[c];
    for (i = 0; i < n; i++)
        cards[i] = so.held_area == 0 ? so_held_card() : so.tab[so.held_col][so.held_depth + i];
    so_src_xy(&x0, &y0);
    so_take_from_source();
    for (i = 0; i < n; i++)
        so.tab[c][so.nt[c]++] = (uint8_t)(cards[i] | CARD_UP);
    for (i = 0; i < n; i++)
        cards[i] |= CARD_UP;
    so_fly(1, c, start, cards, n, x0, y0);
    k_on(0, so_note(cards[0]), 90, SS_PLACE, 0.6f);
    juice_heat(3 + n);
    if (n >= 3) {                        /* a long run moved: it lands with weight */
        juice_shake(2 + n / 2, 200);
        juice_kick(0, 3);
    }
}
static void so_select(void)
{
    int i;
    if (so.won)
        return;
    if (so.held_area < 0) {              /* take */
        if (so.area == 0 && so.col == 0) {   /* the stock: a card to the waste, or the waste back */
            if (so.ns) {
                uint8_t c = so.stock[--so.ns];
                so.waste[so.nw++] = (uint8_t)(c | CARD_UP);
                c |= CARD_UP;
                so_fly(0, 1, 0, &c, 1, SO_X(0), SO_TOP);
                k_on(0, so_note(c), 60, SS_DRAW, 0.2f);
            } else {
                while (so.nw)
                    so.stock[so.ns++] = (uint8_t)(so.waste[--so.nw] & ~CARD_UP);
                k_on(0, degree_note(0), 50, SS_DRAW, 0.3f);
            }
            so.moves++;
            return;
        }
        if (so.area == 0 && ((so.col == 1 && so.nw) || (so.col >= 3 && so.nf[so.col - 3]))) {
            so.held_area = 0, so.held_col = so.col, so.held_depth = 0;
        } else if (so.area == 1 && so.nt[so.col] && so.depth >= so_first_up(so.col)) {
            so.held_area = 1, so.held_col = so.col, so.held_depth = so.depth;
        } else {
            k_on(0, 41, 70, SS_NO, 0.1f);
            return;
        }
        k_on(0, so_note(so_held_card()), 80, SS_PICK, 0.3f);
        return;
    }
    /* put */
    if ((so.area == 0 && so.col == so.held_col && so.held_area == 0) ||
        (so.area == 1 && so.held_area == 1 && so.col == so.held_col)) {   /* A again on what you hold: it goes where
                                                                       * it can (home first, then a column), else
                                                                       * you let it go */
        int from = so.held_area == 1 ? so.held_col : -1, whole = so.held_area == 1 && so.held_depth == 0;
        if (so_held_n() == 1)
            for (i = 0; i < 4; i++)
                if (so_fits_found(so_held_card(), i)) {
                    so_to_found(i);
                    if (so.area == 1 && so.depth >= so.nt[so.col])
                        so.depth = (int8_t)(so.nt[so.col] ? so.nt[so.col] - 1 : 0);
                    return;
                }
        for (i = 0; i < 2; i++) {        /* a column with cards first; an empty one only for a run that isn't
                                          * already a whole column (a king moved to another space is no move) */
            int c;
            for (c = 0; c < 7; c++)
                if (c != from && (i ? !so.nt[c] && !whole : so.nt[c] != 0) && so_fits_tab(so_held_card(), c)) {
                    so_to_tab(c);
                    if (so.area == 1 && so.depth >= so.nt[so.col])
                        so.depth = (int8_t)(so.nt[so.col] ? so.nt[so.col] - 1 : 0);
                    return;
                }
        }
        so.held_area = -1;               /* nowhere: let it go */
        k_on(0, 41, 50, SS_NO, 0.1f);
        return;
    }
    if (so.area == 0 && so.col <= 1) {   /* the stock or the waste while holding: let go, and draw / take that */
        so.held_area = -1;
        so_select();
        return;
    }
    if (so.area == 0 && so.col >= 3) {   /* a foundation */
        if (so_held_n() == 1 && so_fits_found(so_held_card(), so.col - 3)) {
            so_to_found(so.col - 3);
            return;
        }
    } else if (so.area == 1) {           /* a column */
        if (so_fits_tab(so_held_card(), so.col)) {
            so_to_tab(so.col);
            so.depth = (int8_t)(so.nt[so.col] - 1);
            return;
        }
    }
    so.held_area = -1;                   /* it doesn't go there: what's there is taken instead (Nick, 19 Nov 2026:
                                          * not an error, a change of mind) */
    so_select();
}

static void so_nav(int dx, int dy)
{
    if (so.won)
        return;
    if (dx) {
        so.col = (int8_t)clamp(so.col + dx, 0, 6);
        if (so.area == 0 && so.col == 2)
            so.col = (int8_t)(dx > 0 ? 3 : 1);
        if (so.area == 1)
            so.depth = (int8_t)(so.nt[so.col] ? so.nt[so.col] - 1 : 0);
    }
    if (dy > 0) {                        /* up: a card further into the pile, then the top row */
        if (so.area == 1 && so.nt[so.col] && so.depth > so_first_up(so.col))
            so.depth--;
        else if (so.area == 1) {
            so.area = 0;
            if (so.col == 2)
                so.col = 1;
        }
    } else if (dy < 0) {
        if (so.area == 0) {
            so.area = 1;
            so.depth = (int8_t)(so.nt[so.col] ? so.nt[so.col] - 1 : 0);
        } else if (so.depth < so.nt[so.col] - 1)
            so.depth++;
    }
    if (dx || dy)
        k_on(0, degree_note(so.col + 7), 40, SS_DRAW, 0.08f);
}

/* the music under it: a sequencer. A playhead sweeps the seven columns, an eighth note each, then the waste: each
 * column, by chance, plays its top card on long, reverby keys, the rank its note (A low, K high; now and then an
 * octave up), and between them now and then another card's sparkles; an empty column rests. Under it a slow chord every two bars, a bass note each bar, and the drums: the more cards you have home,
 * the more of the kit comes in, until the end runs double time */
static const uint8_t SO_TIER_AT[SQ_TOP + 1] = {0, 5, 12, 20, 30, 40};   /* cards home for each tier */
static int so_home(void) { return so.nf[0] + so.nf[1] + so.nf[2] + so.nf[3]; }
static int so_tier(void)
{
    int k = SQ_TOP, f = so_home();
    while (k > 0 && f < SO_TIER_AT[k])
        k--;
    return k;
}
static uint8_t so_seq_card(int c)        /* the card a column plays (0: it rests) */
{
    if (c == 7)
        return so.nw ? so.waste[so.nw - 1] : 0u;
    return so.nt[c] && (so.tab[c][so.nt[c] - 1] & CARD_UP) ? so.tab[c][so.nt[c] - 1] : 0u;
}
static void so_groove(uint32_t t, uint32_t step)
{
    uint32_t bar = step / 16u, p = step % 16u, ms = plat_ms();
    int32_t ahead = (int32_t)(t - k_now());
    int i, tier = so_tier(), dealing = so.deal_at && ms - so.deal_at < 28u * SO_DEAL_MS + 200u;
    if (p == 0u && bar % 2u == 0u) {     /* every two bars a new chord, at random in the key: its voicing, slowly */
        h_deg_at(bar / 2u);
        for (i = 1; i <= 4; i++)
            k_on(t + (uint32_t)(i - 1) * 900u, h_note(i), 36, SS_PAD, 3.6f);
    }
    if (p == 0u)
        k_on(t, h_bass(), 44, SS_BASS, 1.2f);
    if (dealing || so.won)
        return;
    sq_tier_say(tier, &so.tier, 150);    /* a new part of the kit: named, once */
    if (!(p & 1u)) {                     /* the playhead: by chance (K2), each card's note, long, soft */
        int c = (int)(p / 2u);
        uint8_t card = so_seq_card(c);
        if (card && sq_roll(SQ_SOLI)) {
            k_on(t, so_note(card), 26u + kr() % 16u + (p % 8u == 0u ? 8u : 0u), SS_KEYS, 2.0f);
            so.seq_ms[c] = (ms + (uint32_t)(ahead > 0 ? ahead / 44 : 0)) | 1u;
        }
    } else if (kr() % 100u < (uint32_t)sq_cfg[SQ_SOLI].chance * (tier == SQ_DOUBLE ? 3u : 1u) / 5u) {   /* between: another card's, softer */
        uint8_t card = so_seq_card((int)(kr() % 8u));
        if (card)
            k_on(t, so_note(card) + 12, 16u + kr() % 12u, SS_KEYS, 1.6f);
    }
    sq_drums(t, p, tier, SS_HAT, SS_KICK, SS_SNARE, 63);   /* (soft: almost ambient) */
}

static void so_keys_sound(void)          /* K3: the keys the cards play */
{
    sq_slot = SS_KEYS;
    sq_sound(SQ_SOLI);
}
static void so_room(void)                /* a big room, a dotted-eighth echo at the tempo: the keys hang in it */
{
    k_main.rev_mix = 0.5f, k_main.rev_secs = 9.0f;
    k_main.dly_samples = SR_F * 45.0f / (float)sq_cfg[SQ_SOLI].bpm;
    k_main.dly_mix = 0.3f, k_main.dly_fb = 0.5f;
}
static void so_sounds(void)
{
    k_slot_set(SS_PAD, &KSND[S_GLASSPAD]);
    k_slot_set(SS_PICK, &KSND[S_VIBES]);
    k_slot_set(SS_HOME, &KSND[S_CELESTA]);
    k_slot_set(SS_FLIP, &KSND[S_KOTO]);
    k_slot_set(SS_HAT, &KSND[S_HAT]);
    k_slot_set(SS_KICK, &KSND[S_KICK]);
    k_slot_set(SS_SNARE, &KSND[S_SNARE]);
    k_slot_set(SS_NO, &KSND[S_LOGDRUM]);
    k_slot_set(SS_WIN, &KSND[S_TUBE]);
    k_slot_set(SS_BASS, &KSND[S_TINES]);
    k_slot_rel(SS_PAD, 1.6f);
    k_slot_rel(SS_HOME, 0.8f);
    k_slot_rel(SS_WIN, 1.5f);
    k_slot_rel(SS_BASS, 0.8f);
    k_main_slot = SS_PAD;
    so_keys_sound();
}

/* ---------------------------------------------------------------- drawing --- */
static const char *const SUIT_SPR[4][7] = {
    {"...a...", "..aaa..", ".aaaaa.", "aaaaaaa", "aaaaaaa", "..a.a..", ".aaaaa."},   /* spades */
    {".aa.aa.", "aaaaaaa", "aaaaaaa", "aaaaaaa", ".aaaaa.", "..aaa..", "...a..."},   /* hearts */
    {"...a...", "..aaa..", ".aaaaa.", "aaaaaaa", ".aaaaa.", "..aaa..", "...a..."},   /* diamonds */
    {"..aaa..", "..aaa..", "aa.a.aa", "aaaaaaa", "aa.a.aa", "...a...", ".aaaaa."},   /* clubs */
};
static const char *const RANK_TXT[14] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

static void so_card(int32_t x, int32_t y, uint8_t c)
{
    if (!(c & CARD_UP)) {                /* its back */
        cv_rect(x + 1, y, SO_W - 2, SO_H, RGB(70, 50, 120));
        cv_rect(x, y + 1, SO_W, SO_H - 2, RGB(70, 50, 120));
        frame_rect(x + 3, y + 3, SO_W - 6, SO_H - 6, RGB(120, 92, 190));
        return;
    }
    {
        uint16_t ink = C_RED(c) ? C_PINK : RGB(20, 14, 36);
        uint16_t map[1];
        map[0] = ink;
        cv_rect(x + 1, y, SO_W - 2, SO_H, C_INK);
        cv_rect(x, y + 1, SO_W, SO_H - 2, C_INK);
        cv_rect(x, y + SO_H - 1, SO_W, 1, RGB(150, 140, 180));
        cv_text(x + (C_RANK(c) == 10u ? 0 : 2), y - 1, &FONT_M, RANK_TXT[C_RANK(c)], ink);   /* the rank, big: it
                                                                                                * shows on a pile */
        sprite(x + SO_W - 9, y + 3, SUIT_SPR[C_SUIT(c)], 7, 1, map);
        sprite(x + 9, y + 24, SUIT_SPR[C_SUIT(c)], 7, 2, map);
    }
}
static void so_slot(int32_t x, int32_t y, const char *s)
{
    frame_rect(x, y, SO_W, SO_H, C_GHOST);
    if (s)
        text_c(x + SO_W / 2, y + 14, &FONT_XS, s, C_GHOST);
}

/* where a column's cards sit: the face-down ones close, the face-up ones apart (closer when it is long) */
static void so_offsets(int c, int32_t *dd, int32_t *du)
{
    int nd = so_first_up(c), nu = so.nt[c] - nd;
    int32_t avail = 240 - SO_TAB - SO_H - 2;
    *dd = 5;
    *du = 22;                            /* a face-up card shows its rank, big */
    if (nu > 1 && nd * *dd + (nu - 1) * *du > avail)
        *dd = 3;
    if (nu > 1 && nd * *dd + (nu - 1) * *du > avail)
        *du = clamp((avail - nd * *dd) / (nu - 1), 9, 22);
}
static int32_t so_card_y(int c, int i)
{
    int32_t dd, du, nd = so_first_up(c);
    so_offsets(c, &dd, &du);
    return SO_TAB + (i < nd ? i * dd : nd * dd + (i - nd) * du);
}

static void so_src_xy(int32_t *x, int32_t *y)   /* the held cards' place, before they go */
{
    *x = SO_X(so.held_col);
    *y = so.held_area == 0 ? SO_TOP : so_card_y(so.held_col, so.held_depth);
}
static int so_hidden(int area, int col, int i)   /* a card still in the air (or not yet dealt): not drawn in place */
{
    uint32_t ms = plat_ms();
    if (so.deal_at && area == 1 && ms - so.deal_at < (uint32_t)(so_deal_order(col, i) + 1) * SO_DEAL_MS + 120u)
        return 1;
    if (!so.fly_n || (int32_t)(ms - so.fly_at) >= 170 || so.fly_area != area || so.fly_col != col)   /* (signed: | 1) */
        return 0;
    return area == 0 ? 1 : i >= so.fly_start;
}
static void so_card_flip(int32_t x, int32_t y, uint8_t c, uint32_t q)   /* turning over: narrow, the back, then the face */
{
    int32_t w = q < 90u ? SO_W - (int32_t)q * SO_W / 90 : (int32_t)(q - 90u) * SO_W / 90, cx = x + SO_W / 2;
    if (q < 90u) {
        cv_rect(cx - w / 2, y, w, SO_H, RGB(70, 50, 120));
        frame_rect(cx - w / 2, y, w > 1 ? w : 1, SO_H, RGB(120, 92, 190));
    } else {
        so_card(x, y, c);
        cv_rect(x - 1, y, (SO_W - w) / 2 + 1, SO_H, C_BG);   /* (the face, only its middle so far) */
        cv_rect(cx + w / 2, y, SO_W - (SO_W - w) / 2 - w / 2 + 1, SO_H, C_BG);
    }
}
static int32_t so_seq_lift(int c)       /* the playhead on a column's top card: it hops as it sounds */
{
    uint32_t ms = plat_ms(), q;
    if (!so.seq_ms[c] || (int32_t)(ms - so.seq_ms[c]) < 0 || (q = ms - so.seq_ms[c]) >= 160u)
        return 0;
    if (so.held_area >= 0 && so.held_area == (c == 7 ? 0 : 1) && so.held_col == (c == 7 ? 1 : c))
        return 0;                        /* (what you hold stays put) */
    return 4 - (int32_t)q / 40;
}
static void so_scene(void)
{
    int c, i;
    char t[24], n[8];
    uint32_t ms = plat_ms();
    itos(so.moves, n);
    tcpy(t, n);
    tcat(t, " MOVES");
    header("SOLITAIRE", C_CYAN, "", C_CYAN, t);
    /* the top row */
    if (so.ns)
        so_card(SO_X(0), SO_TOP, so.stock[so.ns - 1] & ~CARD_UP);
    else
        so_slot(SO_X(0), SO_TOP, "@");
    if (so.nw > 1 && so_hidden(0, 1, 0))
        so_card(SO_X(1), SO_TOP, so.waste[so.nw - 2]);
    else if (so.nw && !so_hidden(0, 1, 0)) {
        int32_t l = so_seq_lift(7);
        if (l && so.nw > 1)
            so_card(SO_X(1), SO_TOP, so.waste[so.nw - 2]);
        so_card(SO_X(1), SO_TOP - l, so.waste[so.nw - 1]);
        if (l)
            cv_rect(SO_X(1) + 4, SO_TOP + SO_H + 1, SO_W - 8, 2, C_CYAN);
    }
    else if (so.nw)
        so_slot(SO_X(1), SO_TOP, 0);
    else
        so_slot(SO_X(1), SO_TOP, 0);
    for (i = 0; i < 4; i++) {
        if (so.nf[i] && !(so_hidden(0, 3 + i, 0) && so.nf[i] == 1))
            so_card(SO_X(3 + i), SO_TOP, so.found[i][so.nf[i] - (so_hidden(0, 3 + i, 0) ? 2 : 1)]);
        else {
            uint16_t map[1];
            map[0] = C_GHOST;
            so_slot(SO_X(3 + i), SO_TOP, 0);
            sprite(SO_X(3 + i) + 9, SO_TOP + 15, SUIT_SPR[i], 7, 2, map);
        }
    }
    /* the columns */
    for (c = 0; c < 7; c++) {
        if (!so.nt[c])
            so_slot(SO_X(c), SO_TAB, "K");
        for (i = 0; i < so.nt[c]; i++) {
            if (so_hidden(1, c, i))
                continue;
            if (c == so.flip_col && i == so.nt[c] - 1 && so.flip_at && ms - so.flip_at < 180u)
                so_card_flip(SO_X(c), so_card_y(c, i), so.tab[c][i], ms - so.flip_at);
            else if (i == so.nt[c] - 1 && so_seq_lift(c)) {   /* the playhead's card: up, a light under it */
                int32_t l = so_seq_lift(c), y = so_card_y(c, i);
                cv_rect(SO_X(c), y + SO_H - l, SO_W, l, C_BG);
                so_card(SO_X(c), y - l, so.tab[c][i]);
                cv_rect(SO_X(c) + 4, y + SO_H + 1, SO_W - 8, 2, C_CYAN);
            } else
                so_card(SO_X(c), so_card_y(c, i), so.tab[c][i]);
        }
    }
    if (so.deal_at && ms - so.deal_at < 28u * SO_DEAL_MS + 200u) {   /* the deal: one card at a time, from the stock */
        int j;
        for (c = 0; c < 7; c++)
            for (j = 0; j < so.nt[c] && j <= c; j++) {
                uint32_t t0 = (uint32_t)so_deal_order(c, j) * SO_DEAL_MS, q = ms - so.deal_at;
                if (q >= t0 && q < t0 + 120u) {
                    float e = (float)(q - t0) / 120.0f;
                    int32_t x1 = SO_X(c), y1 = so_card_y(c, j);
                    e = 1.0f - (1.0f - e) * (1.0f - e);
                    so_card(SO_X(0) + (int32_t)((float)(x1 - SO_X(0)) * e), SO_TOP + (int32_t)((float)(y1 - SO_TOP) * e),
                            j == c ? so.tab[c][j] : (uint8_t)(so.tab[c][j] & ~CARD_UP));
                }
            }
    }
    if (so.fly_n && ms - so.fly_at < 170u) {   /* cards in the air: eased in, a little arc */
        float e = (float)(ms - so.fly_at) / 170.0f, arc;
        int32_t x1 = SO_X(so.fly_col), y1 = so.fly_area == 0 ? SO_TOP : so_card_y(so.fly_col, so.fly_start);
        arc = fm_sinf(e * 3.14159f) * 10.0f;
        e = 1.0f - (1.0f - e) * (1.0f - e) * (1.0f - e);
        for (i = 0; i < so.fly_n; i++) {
            int32_t dy = so.fly_area == 1 ? so_card_y(so.fly_col, so.fly_start + i) - y1 : 0;
            int32_t x = so.fly_x0 + (int32_t)((float)(x1 - so.fly_x0) * e);
            int32_t y = so.fly_y0 + (int32_t)((float)(y1 - so.fly_y0) * e) + dy - (int32_t)arc;
            cv_rect(x + 3, y + 4, SO_W, SO_H, RGB(4, 2, 10));   /* its shadow, lifted */
            so_card(x, y, so.fly_cards[i]);
        }
    } else if (so.fly_n && ms - so.fly_at < 260u && so.fly_area == 1) {   /* landed: a puff */
        so.fly_n = 0;
    }
    /* what you hold: outlined in cyan */
    if (so.held_area == 0) {
        int32_t g = (int32_t)(fm_sinf((float)ms * 0.012f) * 1.5f + 1.5f);
        frame_rect(SO_X(so.held_col) - 1 - g, SO_TOP - 1 - g, SO_W + 2 + 2 * g, SO_H + 2 + 2 * g, C_CYAN);
    }
    else if (so.held_area == 1) {
        int32_t y0 = so_card_y(so.held_col, so.held_depth), y1 = so_card_y(so.held_col, so.nt[so.held_col] - 1) + SO_H;
        frame_rect(SO_X(so.held_col) - 1, y0 - 1, SO_W + 2, y1 - y0 + 2, C_CYAN);
        frame_rect(SO_X(so.held_col) - 2, y0 - 2, SO_W + 4, y1 - y0 + 4, C_CYAN);
    }
    /* the cursor */
    {
        int32_t x = SO_X(so.col), y = so.area == 0 ? SO_TOP : so.nt[so.col] ? so_card_y(so.col, so.depth) : SO_TAB;
        int32_t h = so.area == 1 && so.nt[so.col] && so.depth < so.nt[so.col] - 1
                        ? so_card_y(so.col, so.depth + 1) - y + 2 : SO_H;
        frame_rect(x - 2, y - 2, SO_W + 4, h + 4, C_YEL);
        frame_rect(x - 3, y - 3, SO_W + 6, h + 6, C_YEL);
    }
    if (so.won) {
        int32_t b = (int32_t)((ms - so.won_at) / 4u % 40u), k;
        uint32_t q = ms - so.won_at;
        for (k = 0; k < 52; k++) {       /* a card every 110 ms, from each foundation in turn: bouncing away */
            int32_t t0 = 600 + k * 110, f = (int32_t)q - t0, fr, x, y, vx, vy, tr;
            if (f < 0 || f > 4000)
                continue;
            for (tr = 6; tr >= 0; tr--) {   /* its trail, then it */
                int32_t ff = f / 16 - tr * 3, j;
                if (ff < 0)
                    continue;
                x = (SO_X(3 + k % 4)) * 16;
                y = SO_TOP * 16;
                vx = (k & 1 ? -1 : 1) * (28 + (k * 7) % 30);
                vy = -40 - (k * 13) % 40;
                for (fr = 0; fr < ff; fr++) {   /* (stepped: it bounces off the floor, losing a little) */
                    x += vx;
                    y += vy;
                    vy += 6;
                    if (y > (240 - SO_H) * 16) {
                        y = (240 - SO_H) * 16;
                        vy = -vy * 4 / 5;
                    }
                }
                if (x < -SO_W * 16 || x > 240 * 16)
                    break;
                if (tr)
                    frame_rect(x / 16, y / 16, SO_W, SO_H, mix565(C_BG, C_RED(so.found[k % 4][12 - (k / 4) % 13]) ? C_PINK : C_CYAN, 200 - tr * 26));
                else
                    so_card(x / 16, y / 16, so.found[k % 4][12 - (k / 4) % 13]);
                (void)j;
            }
        }
        cv_rect(30, 96, 180, 54, C_BG);
        frame_rect(30, 96, 180, 54, C_YEL);
        frame_rect(31, 97, 178, 52, C_YEL);
        text_c(120, 100 - (b < 20 ? b / 4 : (40 - b) / 4), &FONT_M, "YOU WIN!", (q / 150u) & 1u ? C_CYAN : C_YEL);
        {
            char bs[24], bn[8];
            itos(so_best, bn);
            tcpy(bs, "BEST: ");
            tcat(bs, bn);
            tcat(bs, " MOVES");
            text_c(120, 122, &FONT_T, bs, C_INK);
        }
        text_c(120, 136, &FONT_T, "PLAY: DEAL AGAIN   HOME: MENU", C_GHOST);
    }
}

/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* seq.h: what the sequencer games share (SOLITAIRE, BRICK BREAK, STAR SWARM, SNAKE, MINES, 2048, CUBE DROP, 4 IN A ROW, ROCK DRIFT, POOL;
 * the toys' K2 is their own): the game's state is the sequence, a
 * playhead plays it on the beat, and the drums come in as you get further. Their knobs, each game its own: K1 TEMPO,
 * K2 CHANCE (how many of the playhead's notes sound), K3 SOUND, K4 EFFECTS (app.c), PRESET the scale, ALGORITHM the
 * key. Included once from app.c, after the modes and the harmony, before the games. */

enum { SQ_SOLI, SQ_BRICK, SQ_SWARM, SQ_DROP, SQ_BOUNCE,   /* (DROP, BOUNCE: toys.h, K2 not CHANCE) */
       SQ_SNAKE, SQ_MINES, SQ_2048, SQ_CUBE, SQ_FOUR, SQ_DRIFT, SQ_POOL, SQ_N };   /* (CUBE: its tempo is its level's) */
/* the sounds K3 steps through */
static const uint8_t SQ_INST[] = {S_DREAM, S_TINES, S_VIBES, S_HARP, S_CRYSTAL, S_CELESTA, S_GLASSPAD, S_NYLON,
                                  S_KOTO, S_FLUTE, S_MARIMBA, S_CHIP, S_SAWPLUCK, S_SQLEAD, S_SYNCLEAD, S_SOFTSAW};
#define SQ_NINST (sizeof SQ_INST / sizeof SQ_INST[0])
static struct {
    uint8_t bpm, chance, inst;           /* inst: SQ_INST's index */
} sq_cfg[SQ_N] = {{66, 60, 0}, {104, 75, 7}, {120, 80, 12}, {100, 100, 2}, {92, 100, 4},   /* DREAM EP; NYLON (Nick, 18 Nov 2026; CHIP before);
                                                                                        * SAW PLUCK; VIBES; CRYSTAL */
                  {92, 90, 11}, {96, 80, 10}, {84, 85, 5}, {100, 70, 2}, {84, 85, 8},   /* CHIP; MARIMBA; CELESTA;
                                                                                        * VIBES; KOTO */
                  {96, 85, 3}, {96, 85, 2}};                    /* ROCK DRIFT: HARP; POOL: VIBES */
static uint8_t sq_slot;                  /* the slot the playhead plays on (the game's sounds set it) */

static int sq_game(void)                 /* the sequencer game on screen (-1: another) */
{
    return mode == M_SOLI ? SQ_SOLI : mode == M_BRICK ? SQ_BRICK : mode == M_SWARM ? SQ_SWARM : mode == M_DROP ? SQ_DROP
           : mode == M_BOUNCE ? SQ_BOUNCE : mode == M_SNAKE ? SQ_SNAKE : mode == M_MINES ? SQ_MINES
           : mode == M_2048 ? SQ_2048 : mode == M_CUBE ? SQ_CUBE : mode == M_FOUR ? SQ_FOUR
           : mode == M_DRIFT ? SQ_DRIFT : mode == M_POOL ? SQ_POOL : -1;
}
static void sq_sound(int g) { k_slot_set(sq_slot, &KSND[SQ_INST[sq_cfg[g].inst % SQ_NINST]]); }   /* K3 */
static uint32_t sq_ms(uint32_t t)        /* when a note scheduled for sample t sounds, on plat_ms's clock */
{
    int32_t ahead = (int32_t)(t - k_now());
    return (plat_ms() + (uint32_t)(ahead > 0 ? ahead / 44 : 0)) | 1u;
}
static int sq_roll(int g) { return kr() % 100u < sq_cfg[g].chance; }   /* the playhead's note: does it sound */

/* the drums, by how far you've got (0 .. 5): hi-hats, the kick, the snare, the full kit, then double time. p: the
 * sixteenth in the bar. vel: 100 as loud as the game likes them */
static const char *const SQ_TIER_NAME[6] = {"", "HI-HATS", "KICK DRUM", "SNARE", "FULL KIT", "DOUBLE TIME"};
static void sq_drums(uint32_t t, uint32_t p, int tier, uint32_t hat, uint32_t kick, uint32_t snare, uint32_t vel)
{
    if (tier >= 1 && (p % 4u == 2u || (tier >= 4 && (p & 1u))))   /* hats: the off-beats, then every sixteenth */
        k_on(t, 92, (p % 4u == 2u ? 54u : 30u) * vel / 100u, hat, 0.05f);
    if (tier >= 2 && (p == 0u || p == 8u || (tier >= 4 && p == 10u)))
        k_on(t, 36, (p == 10u ? 80u : 104u) * vel / 100u, kick, 0.3f);
    if (tier >= 3 && (p == 4u || p == 12u))
        k_on(t, 50, 90u * vel / 100u, snare, 0.25f);
    if (tier >= 5 && p == 15u)            /* a ghost into the bar */
        k_on(t, 50, 36u * vel / 100u, snare, 0.1f);
}
static int sq_tier_of(int done, int total)   /* how far: the share done (of total) as a tier */
{
    static const uint8_t AT[6] = {0, 15, 35, 55, 75, 90};   /* percent */
    int k = 5, pct = total > 0 ? done * 100 / total : 0;
    while (k > 0 && pct < AT[k])
        k--;
    return k;
}
static void sq_tier_say(int tier, uint8_t *said, int32_t y)   /* a new part of the kit: named once (y < 0: not) */
{
    if (tier > *said) {
        *said = (uint8_t)tier;
        if (y >= 0)
            juice_text(120, y, SQ_TIER_NAME[tier], C_CYAN);
    }
}

/* the playhead's column, lit as its note sounds: an outline round x, y, w, h fading over 180 ms, a mark above */
static void sq_col_glow(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t at, uint16_t c)
{
    uint32_t q = plat_ms() - at;
    int32_t g;
    if (!at || (int32_t)q < 0 || q >= 180u)
        return;
    g = 256 - (int32_t)(q * 256u / 180u);
    frame_rect(x, y, w, h, mix565(C_BG, c, g));
    cv_rect(x + 2, y - 3, w - 4, 2, mix565(C_BG, c, g));
}

/* where a new note goes in a game's loop (POOL's rack, SWARM's jar): a random empty slot, or, the loop full, any slot
 * at random, so the rhythm comes out random (Nick, 17 Nov 2026) */
static int seq_free_slot(const uint8_t *slot, int n)
{
    int k, free = 0, pick;
    for (k = 0; k < n; k++)
        free += !slot[k];
    pick = (int)((kr() >> 16) % (uint32_t)(free ? free : n));
    for (k = 0; k < n; k++)
        if (!free || !slot[k]) {
            if (!pick--)
                return k;
        }
    return 0;
}

/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* dice.h: DICE SYNTH, from the Dice Synth prototype (FM1-Dice-Synth): roll a die, get a new FM sound with a name, and
 * play it on the keys. A sound is four parts, each from its own seed: TIMBRE (the operators, the algorithm, the
 * filter), SHAPE (the envelope), MOTION (the LFO, vibrato, chorus) and SPACE (delay, reverb). A lock keeps a part
 * through the rolls. Included once from app.c, after its helpers; its state is its own (not the arena), so the rolls
 * and the kept sounds last between visits, and the kept ones are saved (OBJ_SETTINGS, which nothing else uses).
 *
 *   PLAY        roll; a one on the die is a cursed roll (anything goes)
 *   EDIT ENV LFO FX   lock TIMBRE / SHAPE / MOTION / SPACE
 *   PRESET      the die: ANY, KEYS, BELLS, PLUCKS, LEADS, PADS, BASS, PERC, WEIRD
 *   SELECT      back and forth through the last rolls
 *   K1..K4      BRIGHT, LENGTH, SPACE, WOBBLE (over the roll)
 *   ARP         hear it (a few notes)        SAVE   keep it   SEQ    the kept ones in turn
 *   OCT- OCT+   octave; every key plays it   HOME   the menu */

enum { DL_TIMBRE, DL_SHAPE, DL_MOTION, DL_SPACE, DC_NLOCK };
static const uint8_t DC_LOCK_BTN[DC_NLOCK] = {B_EDIT, B_ENV, B_LFO, B_FX};
static const char *const DC_LOCK_NAME[DC_NLOCK] = {"TIMBRE", "SHAPE", "MOTION", "SPACE"};
static const char *const DC_LOCK_KEY[DC_NLOCK] = {"EDIT", "ENV", "LFO", "FX"};
enum { DK_ANY, DK_KEYS, DK_BELLS, DK_PLUCKS, DK_LEADS, DK_PADS, DK_BASS, DK_PERC, DK_WEIRD, DC_NKIND };
static const char *const DC_KIND_NAME[DC_NKIND] = {"ANY", "KEYS", "BELLS", "PLUCKS", "LEADS", "PADS", "BASS", "PERC",
                                                   "WEIRD"};
static const char *const DC_MACRO_NAME[4] = {"BRIGHT", "LENGTH", "SPACE", "WOBBLE"};

typedef struct {
    uint32_t seed[DC_NLOCK];
    uint8_t kind, cursed, rsv[2];
} dc_roll_t;
#define DC_NFAV 16
#define DC_MAGIC 0x44494331u             /* "DIC1" */
typedef struct {
    uint32_t magic;
    uint8_t kind, locks, nfav, rsv;
    int8_t octave, rsv2[3];
    uint8_t macro[4];
    dc_roll_t cur;
    dc_roll_t fav[DC_NFAV];
} dc_set_t;
static dc_set_t dcs;

#define DC_NHIST 32
static dc_roll_t dc_hist[DC_NHIST];
static uint32_t dc_hist_n, dc_hist_at;   /* rolls kept, and the one shown (dc_hist_at back from the newest) */
static uint32_t dc_rolls;                /* since boot */

/* ------------------------------------------------------------- the sound --- */
/* what a roll makes: the operators and every knob */
typedef struct {
    uint8_t kind;                        /* the kind it came out as (ANY picks one) */
    eng_sound_t s;
    uint8_t alg, fb;
    float att, dec, sus, rel;            /* seconds, a level */
    float detune, gain;
    uint8_t filter;
    float cut, res, fenv, fdec;
    uint8_t lfo_shape;
    float lfo_hz, vib, trem, lfo_cut, chorus, chorus_hz, chorus_depth, pwm;
    float dly_beats, dly_fb, dly_mix, dly_width, rev_mix, rev_secs, rev_dark, shimmer;
    uint16_t adj, noun;                  /* the name */
    uint8_t face;                        /* the die's face, 1..6 */
} dc_gen_t;
static dc_gen_t dgen;

static const float DC_OPR[] = {0.5f, 1.0f, 1.41f, 1.5f, 2.0f, 2.5f, 2.76f, 3.0f, 3.5f, 4.0f, 5.0f, 5.4f, 6.0f, 7.0f,
                               8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
#define DC_NOPR (sizeof DC_OPR / sizeof DC_OPR[0])
static const uint8_t DC_R_HARM[] = {1, 1, 1, 4, 4, 7, 9, 10, 12, 13, 14};   /* 1 1 1 2 2 3 4 5 6 7 8 */
static const uint8_t DC_R_INH[] = {2, 5, 6, 8, 11, 15, 16, 17, 19, 21};
static const uint8_t DC_R_CWAVE[] = {8, 8, 9, 10, 11, 12};                  /* saw saw square pulses triangle */
typedef struct {                         /* a kind's ranges (knob values 0..100) */
    int8_t a0, a1, d0, d1, s0, s1, r0, r1;
    int8_t ml0, ml1, md0, md1;           /* a modulator's depth (x100) and decay (knob) */
    uint8_t synth, filt, fb, wild;       /* % table-wave carriers, % filter, feedback at most, % anything goes */
    uint8_t alg[6];
} dc_kind_t;
static const dc_kind_t DC_RK[DC_NKIND] = {
    {0, 10, 60, 85, 10, 60, 30, 80, 10, 60, 30, 90, 30, 40, 5, 30, {0, 2, 4, 6, 7, 4}},     /* ANY (not used) */
    {0, 10, 65, 85, 10, 50, 40, 70, 15, 40, 50, 80, 15, 20, 3, 15, {4, 4, 4, 2, 3, 0}},     /* KEYS */
    {0, 0, 80, 92, 0, 10, 70, 90, 15, 45, 60, 90, 0, 10, 3, 25, {4, 4, 5, 2, 3, 6}},        /* BELLS */
    {0, 0, 50, 72, 0, 10, 40, 60, 15, 45, 35, 65, 35, 40, 3, 15, {4, 4, 2, 3, 0, 6}},       /* PLUCKS */
    {0, 30, 60, 80, 60, 95, 30, 60, 10, 40, 60, 90, 55, 60, 6, 15, {0, 1, 2, 6, 7, 4}},     /* LEADS */
    {55, 80, 80, 92, 70, 95, 75, 92, 4, 22, 75, 95, 50, 50, 3, 15, {4, 5, 6, 7, 4, 7}},     /* PADS */
    {0, 5, 55, 75, 30, 80, 20, 40, 20, 55, 35, 65, 50, 70, 6, 15, {0, 2, 4, 6, 7, 4}},      /* BASS */
    {0, 0, 40, 65, 0, 0, 30, 50, 20, 60, 25, 50, 10, 20, 5, 20, {4, 6, 7, 4, 2, 3}},        /* PERC */
    {0, 80, 50, 95, 0, 100, 30, 95, 10, 80, 30, 95, 40, 50, 7, 55, {0, 1, 2, 3, 4, 5}},     /* WEIRD */
};

/* the names: an adjective (any) and a noun (the kind's, or any) */
static const char *const DC_ADJ[] = {
    "DAMP", "HAUNTED", "GREASY", "WET", "SAD", "TINY", "CURSED", "FANCY", "SLEEPY", "ANGRY", "VELVET", "RUSTY",
    "FROZEN", "SOGGY", "DISCO", "FERAL", "POLITE", "OFFICE", "MIDNIGHT", "GLITTER", "BROKEN", "SPICY", "LONELY",
    "NERVOUS", "VINTAGE", "RADIOACTIVE", "SUSPICIOUS", "EMOTIONAL", "SUNDAY", "BUDGET", "LUXURY", "HAPPY", "MOODY",
    "ROYAL", "PLASTIC", "COSMIC", "SWAMP", "BASEMENT", "CHROME", "MILD", "UNPAID", "OVERDUE", "WOBBLY", "SMOKY",
    "FUZZY", "GENTLE", "SALTY", "ANCIENT", "TURBO", "LAZY", "HOLLOW", "SECOND-HAND", "UNDERWATER", "DRAMATIC"};
#define DC_NADJ (sizeof DC_ADJ / sizeof DC_ADJ[0])
static const char *const DC_NOUN_ANY[] = {"GHOST", "ACCORDION", "DOLPHIN", "CASSETTE", "TOASTER", "SPACESHIP",
                                          "LASAGNA", "ROBOT", "WIZARD", "GOOSE", "MAILBOX", "CATHEDRAL", "MOTH",
                                          "SPREADSHEET", "LIGHTHOUSE", "PIGEON", "VACUUM", "TEAPOT", "AIRPORT",
                                          "WALRUS"};
#define DC_NNA (sizeof DC_NOUN_ANY / sizeof DC_NOUN_ANY[0])
static const char *const DC_NOUN_KIND[DC_NKIND][6] = {
    {"THING", "SOUND", "NOISE", "MACHINE", "BOX", "DEVICE"},
    {"PIANO", "ORGAN", "KEYS", "HARPSICHORD", "RHODES", "CLAVINET"},
    {"BELLS", "CHIMES", "GLOCKENSPIEL", "MUSIC BOX", "GAMELAN", "WIND CHIME"},
    {"HARP", "BANJO", "KOTO", "UKULELE", "PLUCK", "MANDOLIN"},
    {"LEAD", "TRUMPET", "WHISTLE", "KAZOO", "FLUTE", "SAXOPHONE"},
    {"PAD", "CHOIR", "STRINGS", "FOG", "CLOUD", "BLANKET"},
    {"BASS", "TUBA", "SUBWOOFER", "BASSOON", "GROWL", "BOOM"},
    {"DRUM", "CLANG", "KNOCK", "TIN CAN", "BUCKET", "COWBELL"},
    {"PORTAL", "ANOMALY", "SIGNAL", "UFO", "DIMENSION", "GLITCH"},
};

static uint32_t dc_gs;                   /* the generator's state */
static uint32_t dc_r(void)
{
    dc_gs ^= dc_gs << 13;
    dc_gs ^= dc_gs >> 17;
    dc_gs ^= dc_gs << 5;
    return dc_gs;
}
static void dc_seed(uint32_t s, uint32_t salt)
{
    uint32_t i;
    dc_gs = (s ^ (salt * 0x9E3779B9u)) | 1u;
    for (i = 0; i < 4u; i++)
        dc_r();
}
static int32_t dc_in(int32_t lo, int32_t hi) { return lo + (int32_t)(dc_r() % (uint32_t)(hi - lo + 1)); }
static int dc_pc(uint32_t pc) { return dc_r() % 100u < pc; }

static void dc_make(const dc_roll_t *r)
{
    const dc_kind_t *k;
    uint32_t c, i, car, ncar = 0, ci = 0, synth, wild = r->cursed ? 85u : 0u;
    /* TIMBRE */
    dc_seed(r->seed[DL_TIMBRE], 1u);
    c = r->kind ? r->kind : 1u + dc_r() % (DC_NKIND - 1u);   /* (a cursed roll: any kind too, just made wild) */
    dgen.kind = (uint8_t)c;
    k = &DC_RK[c];
    if (!wild)
        wild = k->wild;
    dgen.alg = (uint8_t)(dc_pc(wild) ? dc_r() % 8u : k->alg[dc_r() % 6u]);
    car = ENG_CAR[dgen.alg];
    for (i = 0; i < 4u; i++)
        ncar += car >> i & 1u;
    synth = (uint32_t)dc_pc(k->synth);
    for (i = 0; i < 4u; i++) {
        eng_op_t *o = &dgen.s.op[i];
        int w = dc_pc(wild);
        o->det = 0.0f;
        o->strike = 0;
        if (car >> i & 1u) {             /* a carrier: on the note (a bass's an octave down), others on or off it */
            float lv = 0.95f / (float)ncar * (float)dc_in(70, 130) / 100.0f;
            uint32_t ri;
            if (ci == 0u)
                ri = c == DK_BASS ? 0u : 1u;
            else if ((w || c == DK_BELLS || c == DK_WEIRD) && dc_pc(50))
                ri = DC_R_INH[dc_r() % sizeof DC_R_INH];
            else
                ri = c == DK_BASS ? dc_r() % 2u : (uint32_t)(dc_pc(60) ? 1 : dc_pc(50) ? 4 : 0);
            if (ci && dc_pc(30))
                lv *= 0.4f;
            o->ratio = DC_OPR[ri];
            o->lvl = fm_clampf(lv, 0.05f, 1.0f);
            o->wave = (uint8_t)(synth ? DC_R_CWAVE[dc_r() % sizeof DC_R_CWAVE] : 0u);
            if (ci && ((c == DK_PERC && dc_pc(30)) || (c == DK_WEIRD && dc_pc(20))))
                o->wave = 13u;           /* noise */
            if (ci)
                o->det = (float)dc_in(-9, 9);
            o->att = o->dec = o->sus = 0.0f;
            ci++;
        } else {                         /* a modulator: harmonic, or (a bell's, a weird one's) not */
            uint32_t ri = w ? dc_r() % DC_NOPR
                            : (c == DK_BELLS || c == DK_WEIRD) && dc_pc(50) ? DC_R_INH[dc_r() % sizeof DC_R_INH]
                                                                          : DC_R_HARM[dc_r() % sizeof DC_R_HARM];
            o->ratio = DC_OPR[ri];
            o->lvl = (float)dc_in(k->ml0, k->ml1) * (synth ? 0.4f : 1.0f) / 100.0f;
            o->wave = (uint8_t)(dc_pc(c == DK_WEIRD ? 50u : 20u) ? dc_in(1, 7) : 0);
            o->att = c == DK_PADS ? 0.2f + 1.5f * (float)(dc_r() % 100u) / 100.0f : 0.0f;
            o->dec = knob_secs(dc_in(k->md0, k->md1));
            o->sus = (float)dc_in(0, c == DK_PADS || c == DK_LEADS ? 70 : 30) / 100.0f;
        }
    }
    dgen.s.mono = (uint8_t)(c == DK_BASS ? 1u : c == DK_LEADS ? (dc_pc(60) ? 2u : 1u) : 0u);
    dgen.s.glide = c == DK_LEADS ? 0.03f + 0.08f * (float)(dc_r() % 100u) / 100.0f
                 : c == DK_BASS && dc_pc(40) ? 0.04f : 0.0f;
    dgen.fb = (uint8_t)(dc_pc(50) ? 0 : dc_in(0, k->fb));
    dgen.detune = (float)(c == DK_PADS ? dc_in(20, 55) : dc_in(0, 25)) / 100.0f;
    dgen.gain = c == DK_BASS ? 1.6f : c == DK_PERC ? 1.2f : 1.0f;
    dgen.filter = (uint8_t)dc_pc(synth ? 80u : k->filt);
    dgen.cut = 4.9069f + (float)(c == DK_BASS ? dc_in(30, 55) : dc_in(35, 75)) * 0.0923f;
    dgen.res = (float)dc_in(0, c == DK_WEIRD || r->cursed ? 85 : 55) / 100.0f * 0.95f;
    dgen.fenv = (float)dc_in(0, 60) / 100.0f * 8.0f;
    dgen.fdec = knob_secs(dc_in(40, 85));
    /* SHAPE: the envelope */
    dc_seed(r->seed[DL_SHAPE], 2u);
    dgen.att = knob_secs(dc_in(k->a0, k->a1));
    if (k->a1 == 0 || dgen.att < 0.0015f)
        dgen.att = 0.0f;
    dgen.dec = knob_secs(dc_in(k->d0, k->d1));
    dgen.sus = (float)dc_in(k->s0, k->s1) / 100.0f;
    dgen.rel = knob_secs(dc_in(k->r0, k->r1));
    /* MOTION */
    dc_seed(r->seed[DL_MOTION], 3u);
    dgen.lfo_shape = (uint8_t)(dc_r() % LFO_NSHAPE);
    dgen.lfo_hz = 0.05f * fm_exp2f((float)dc_in(25, 80) * 0.0864386f);
    dgen.vib = c == DK_LEADS ? (float)dc_in(5, 25) / 100.0f
             : c == DK_WEIRD || r->cursed ? (float)dc_in(0, 90) / 100.0f
             : dc_pc(25) ? (float)dc_in(3, 12) / 100.0f : 0.0f;
    dgen.trem = c == DK_WEIRD || r->cursed ? (float)dc_in(0, 60) / 100.0f
              : c == DK_PADS ? (float)dc_in(0, 20) / 100.0f : dc_pc(15) ? 0.15f : 0.0f;
    dgen.lfo_cut = dgen.filter && (c == DK_WEIRD || c == DK_PADS || r->cursed) && dc_pc(40)
                     ? (float)dc_in(10, 60) / 100.0f * 3.0f : 0.0f;
    dgen.chorus = (float)(c == DK_PADS || c == DK_KEYS ? dc_in(20, 70) : dc_in(0, 40)) / 100.0f;
    dgen.chorus_hz = 0.1f * fm_exp2f((float)dc_in(10, 70) * 0.0564386f);
    dgen.chorus_depth = 20.0f + (float)dc_in(20, 80) * 2.3f;
    dgen.pwm = synth && dc_pc(40) ? (float)dc_in(20, 70) / 100.0f : 0.0f;
    /* SPACE */
    dc_seed(r->seed[DL_SPACE], 4u);
    {
        static const float DB[6] = {0.5f, 0.75f, 0.375f, 1.0f, 0.25f, 1.5f};   /* in beats at 100 BPM */
        uint32_t room = dc_r() % 10u;    /* 0-1 dry, 2-6 a room, 7-8 a hall, 9 a cathedral */
        dgen.dly_beats = DB[dc_r() % 6u];
        dgen.dly_fb = (float)dc_in(15, room >= 8u ? 70 : 50) / 100.0f;
        dgen.dly_mix = dc_pc(c == DK_PADS ? 40u : 60u) ? (float)dc_in(8, 35) / 100.0f : 0.0f;
        dgen.dly_width = (float)dc_in(40, 100) / 100.0f;
        dgen.rev_mix = room < 2u ? 0.04f : room < 7u ? (float)dc_in(10, 25) / 100.0f : (float)dc_in(25, 45) / 100.0f;
        dgen.rev_secs = room < 7u ? 0.8f + 2.0f * (float)(dc_r() % 100u) / 100.0f
                                : 3.0f + 9.0f * (float)(dc_r() % 100u) / 100.0f;
        dgen.rev_dark = (float)dc_in(10, 80) / 100.0f;
        dgen.shimmer = room == 9u || (r->cursed && dc_pc(50)) ? (float)dc_in(20, 60) / 100.0f : 0.0f;
    }
    /* the name and the face: from every part, so a lock still changes the name */
    dc_seed(r->seed[0] ^ r->seed[1] * 3u ^ r->seed[2] * 5u ^ r->seed[3] * 7u, 9u + r->kind);
    dgen.adj = (uint16_t)(dc_r() % DC_NADJ);
    dgen.noun = (uint16_t)(dc_pc(55) ? (dc_r() % 6u) : 100u + dc_r() % DC_NNA);
    dgen.face = (uint8_t)(1u + dc_r() % 6u);
}

static const char *dc_noun(void) { return dgen.noun >= 100u ? DC_NOUN_ANY[dgen.noun - 100u] : DC_NOUN_KIND[dgen.kind][dgen.noun]; }

/* the sound as the engine plays it: the roll, under the four macros. Double-buffered over slots 0 and 1: the render
 * reads one while the other is written, then k_main_slot moves */
static uint8_t dc_slot;
static void dc_apply(void)
{
    uint32_t sl = dc_slot ^ 1u, i;
    kslot_t *s = &k_slot[sl];
    eng_params_t p;
    float bright = fm_exp2f(((float)dcs.macro[0] - 50.0f) / 25.0f);     /* x0.25 .. x4 */
    float len = fm_exp2f(((float)dcs.macro[1] - 50.0f) / 22.0f);        /* the envelope's times */
    float space = (float)dcs.macro[2] / 50.0f;                           /* x0 .. x2 */
    float wob = (float)dcs.macro[3] / 100.0f;
    kp_defaults(&p);
    p.alg = dgen.alg;
    p.fb = dgen.fb;
    p.tone = bright;
    p.detune = dgen.detune;
    p.att = dgen.att * len;
    p.dec = dgen.dec * len;
    p.sus = dgen.sus;
    p.rel = dgen.rel * len;
    p.gain = dgen.gain;
    p.filter = dgen.filter;
    p.cut = dgen.filter ? dgen.cut + (bright > 1.0f ? fm_log2f(bright) : fm_log2f(bright) * 0.6f) : 14.0f;
    p.res = dgen.res;
    p.env = dgen.fenv;
    p.fdec = dgen.fdec * len;
    p.fsus = 0.3f;
    p.frel = dgen.rel * len;
    p.keytrack = 0.5f;
    p.lfo_shape = dgen.lfo_shape;
    p.lfo_hz = dgen.lfo_hz * (1.0f + wob * 2.0f);
    p.vib = dgen.vib + wob * wob * 0.6f;
    p.trem = fm_clampf(dgen.trem + wob * 0.4f, 0.0f, 1.0f);
    p.lfo_cut = dgen.lfo_cut + (dgen.filter ? wob * 1.5f : 0.0f);
    p.chorus = dgen.chorus;
    p.chorus_hz = dgen.chorus_hz;
    p.chorus_depth = dgen.chorus_depth;
    p.pwm = dgen.pwm;
    p.dly_samples = dgen.dly_beats * 0.6f * SR_F;   /* 100 BPM */
    p.dly_fb = dgen.dly_fb;
    p.dly_mix = fm_clampf(dgen.dly_mix * space, 0.0f, 0.6f);
    p.dly_width = dgen.dly_width;
    p.rev_mix = fm_clampf(dgen.rev_mix * space, 0.0f, 0.6f);
    p.rev_secs = dgen.rev_secs * (0.5f + space * 0.5f);
    p.rev_dark = dgen.rev_dark;
    p.shimmer = dgen.shimmer;
    {
        eng_sound_t ns = dgen.s;         /* built whole first, then handed to the audio in one locked step: the render
                                          * (an interrupt) never sees half of it (GPT's review, 18 Nov 2026) */
        uint32_t f;
        for (i = 0; i < 4u; i++)         /* the modulators' own envelopes follow LENGTH too */
            if (!(ENG_CAR[dgen.alg] >> i & 1u)) {
                ns.op[i].att *= len;
                ns.op[i].dec *= len;
            }
        f = plat_lock();
        s->s = ns;
        s->p = p;
        k_main = p;
        k_main_slot = (uint8_t)sl;
        dc_slot = (uint8_t)sl;
        plat_unlock(f);
    }
}

/* ------------------------------------------------------------------- play --- */
static uint32_t dc_roll_at;              /* the die's tumble started (ms) */
static uint8_t dc_tumble_face;
static int32_t dc_held[NKEYS];

/* the audition: a few notes in the new sound, the kind's way */
static void dc_audition(void)
{
    uint32_t i, s = dc_slot;
    float t = 0.06f;
    int32_t base = 60 + 12 * dcs.octave;
    static const int8_t ARP[] = {0, 4, 7, 12, 16}, LICK[] = {0, 2, 4, 7, 9, 7, 12}, BASSL[] = {0, 0, 12, 0, 7, 10, 12};
    if (ui.keys)
        return;                          /* the player is playing: no */
    switch (dgen.kind) {
    case DK_BASS:
        for (i = 0; i < 7u; i++)
            k_on(k_at(t + 0.15f * (float)i), base - 24 + BASSL[i], i % 2u ? 90 : 118, s, 0.12f);
        break;
    case DK_LEADS:
        for (i = 0; i < 7u; i++)
            k_on(k_at(t + 0.14f * (float)i), base + LICK[i], 100, s, i == 6u ? 0.6f : 0.12f);
        break;
    case DK_PERC:
        for (i = 0; i < 8u; i++)
            k_on(k_at(t + 0.11f * (float)i), base - 12 + (i % 3u ? 7 : 0) + (i == 7u ? 12 : 0), i % 2u ? 70 : 120, s,
                 0.08f);
        break;
    case DK_PADS:
        for (i = 0; i < 4u; i++)
            k_on(k_at(t + 0.02f * (float)i), base + ARP[i], 96, s, 1.6f);
        break;
    default:                             /* keys, bells, plucks, weird: up an arpeggio, then the chord */
        for (i = 0; i < 4u; i++)
            k_on(k_at(t + 0.12f * (float)i), base + ARP[i], 100, s, 0.3f);
        for (i = 0; i < 4u; i++)
            k_on(k_at(t + 0.55f + 0.015f * (float)i), base + ARP[i] - 12 * (i == 0u), 104, s, 0.9f);
        break;
    }
}

static void dc_show(const dc_roll_t *r)
{
    dcs.cur = *r;
    dc_make(r);
    dc_apply();
}

static void dc_roll(void)                /* a new sound; the die's face is in it: a one curses it */
{
    dc_roll_t r = dcs.cur;
    uint32_t i, ms = plat_ms();
    static uint32_t mixer = 0xA5A5F00Du;
    mixer ^= ms * 2654435761u + dc_rolls * 40503u + 0x1234567u;
    for (i = 0; i < DC_NLOCK; i++) {
        mixer ^= mixer << 13;
        mixer ^= mixer >> 17;
        mixer ^= mixer << 5;
        if (!(dcs.locks >> i & 1u))
            r.seed[i] = mixer;
    }
    r.kind = dcs.kind;
    r.cursed = 0;
    dc_make(&r);                         /* (the face is from the seeds and the die, not the curse: it stays) */
    if (dgen.face == 1u) {
        r.cursed = 1;
        app_say("A ONE!", "CURSED");
    }
    dc_hist[(dc_hist_n++) % DC_NHIST] = r;
    dc_hist_at = 0;
    dc_rolls++;
    dc_show(&r);
    dc_roll_at = ms ? ms : 1u;
    k_push(0, KE_ALLOFF, 0, 0, 0, 0);   /* the old sound's tails go: the new one is yours to play */
}

static void dc_history(int32_t d)
{
    uint32_t avail = dc_hist_n < DC_NHIST ? dc_hist_n : DC_NHIST;
    int32_t at = (int32_t)dc_hist_at - d;   /* turning right goes forward, to newer */
    char t[24], n[12];
    if (!avail)
        return;
    at = clamp(at, 0, (int32_t)avail - 1);
    if ((uint32_t)at == dc_hist_at)
        return;
    dc_hist_at = (uint32_t)at;
    dc_show(&dc_hist[(dc_hist_n - 1u - dc_hist_at) % DC_NHIST]);
    tcpy(t, dc_hist_at ? "BACK " : "LATEST");
    if (dc_hist_at) {
        itos((int32_t)dc_hist_at, n);
        tcat(t, n);
    }
    app_say("HISTORY", t);
}

static int dc_fav_has(const dc_roll_t *r)
{
    uint32_t i, k;
    for (i = 0; i < dcs.nfav; i++) {
        for (k = 0; k < DC_NLOCK && dcs.fav[i].seed[k] == r->seed[k]; k++)
            ;
        if (k == DC_NLOCK && dcs.fav[i].kind == r->kind && dcs.fav[i].cursed == r->cursed)
            return 1;
    }
    return 0;
}

static int dc_store(void) { return plat_store_save(OBJ_SETTINGS, &dcs, sizeof dcs); }

static void dc_keep(void)
{
    if (!dc_fav_has(&dcs.cur)) {
        if (dcs.nfav < DC_NFAV)
            dcs.fav[dcs.nfav++] = dcs.cur;
        else {                           /* full: the oldest goes */
            uint32_t i;
            for (i = 1; i < DC_NFAV; i++)
                dcs.fav[i - 1] = dcs.fav[i];
            dcs.fav[DC_NFAV - 1] = dcs.cur;
        }
    }
    app_say(dc_store() == 0 ? "KEPT" : "SAVE FAILED", DC_ADJ[dgen.adj]);
}

static void dc_fav_next(void)
{
    static uint32_t at;
    char t[16], n[8];
    if (!dcs.nfav) {
        app_say("NOTHING KEPT", "SAVE KEEPS ONE");
        return;
    }
    at = (at + 1u) % dcs.nfav;
    dc_show(&dcs.fav[at]);
    itos((int32_t)at + 1, n);
    tcpy(t, n);
    tcat(t, " OF ");
    itos(dcs.nfav, n);
    tcat(t, n);
    app_say("KEPT", t);
}

/* the keys: every one plays the sound (key 0 is F3) */
static void dc_key(uint32_t k, int down)
{
    if (down) {
        dc_held[k] = 53 + (int32_t)k + 12 * dcs.octave;
        k_on(0, dc_held[k], touch_vel(k), dc_slot, 0.0f);
    } else {
        k_off(0, dc_held[k]);
    }
}

/* its buttons and knobs (app.c's input() hands them over in DICE SYNTH; HOME stays the menu's) */
static void dc_input(uint32_t pressed)
{
    uint32_t k;
    int32_t e;
    if (pressed >> B_PLAY & 1u)
        dc_roll();
    if (pressed >> B_ARP & 1u)
        dc_audition();
    if (pressed >> B_SAVE & 1u)
        dc_keep();
    if (pressed >> B_SEQ & 1u)
        dc_fav_next();
    for (k = 0; k < DC_NLOCK; k++)
        if (pressed >> DC_LOCK_BTN[k] & 1u) {
            dcs.locks ^= (uint8_t)(1u << k);
            app_say(DC_LOCK_NAME[k], dcs.locks >> k & 1u ? "LOCKED" : "FREE");
        }
    if (pressed >> B_OCTDN & 1u && dcs.octave > -2)
        dcs.octave--;
    if (pressed >> B_OCTUP & 1u && dcs.octave < 2)
        dcs.octave++;
    if ((e = plat_enc(EN_PRESET)) != 0) {
        dcs.kind = (uint8_t)clamp((int32_t)dcs.kind + e, 0, DC_NKIND - 1);
        app_say("DIE", DC_KIND_NAME[dcs.kind]);
    }
    if ((e = plat_enc(EN_SELECT)) != 0)
        dc_history(e);
    (void)plat_enc(EN_ALGO);
    for (k = 0; k < 4u; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0) {
            dcs.macro[k] = (uint8_t)clamp((int32_t)dcs.macro[k] + e * 2, 0, 100);
            dc_apply();
        }
}
static uint32_t dc_lit(void)             /* its buttons lit */
{
    uint32_t lit = 1u << B_PLAY | 1u << B_ARP | 1u << B_HOME | (dcs.nfav ? 1u << B_SEQ : 0u), k;
    for (k = 0; k < DC_NLOCK; k++)
        if (dcs.locks >> k & 1u)
            lit |= 1u << DC_LOCK_BTN[k];
    if (dc_fav_has(&dcs.cur))
        lit |= 1u << B_SAVE;
    return lit;
}

static void dc_sounds(void)              /* in: the sound as it was left */
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        dc_held[k] = -1;
    dc_show(&dcs.cur);
}

static void dc_init(void)                /* at boot: the kept sounds, and where it was */
{
    uint32_t i;
    if (plat_store_load(OBJ_SETTINGS, &dcs, sizeof dcs) != (int)sizeof dcs || dcs.magic != DC_MAGIC ||
        dcs.kind >= DC_NKIND || dcs.nfav > DC_NFAV || dcs.octave < -2 || dcs.octave > 2) {
        dcs.magic = DC_MAGIC;
        dcs.kind = DK_ANY;
        dcs.locks = 0;
        dcs.nfav = 0;
        dcs.octave = 0;
        for (i = 0; i < 4u; i++)
            dcs.macro[i] = 50;
        dcs.cur.seed[0] = 0x6A09E667u, dcs.cur.seed[1] = 0xBB67AE85u;
        dcs.cur.seed[2] = 0x3C6EF372u, dcs.cur.seed[3] = 0xA54FF53Au;
        dcs.cur.kind = DK_KEYS;
        dcs.cur.cursed = 0;
    }
    for (i = 0; i < 4u; i++)
        if (dcs.macro[i] > 100u)
            dcs.macro[i] = 50;
    dc_make(&dcs.cur);
    dc_hist[dc_hist_n++ % DC_NHIST] = dcs.cur;
}

/* ---------------------------------------------------------------- drawing --- */
#define C_DIE RGB(250, 246, 236)
#define C_DIESH RGB(196, 186, 214)
static const uint8_t DC_PIPS[7][6][2] = {   /* a face's pips, on a 3 x 3 grid (0..2) */
    {{0, 0}}, {{1, 1}}, {{0, 0}, {2, 2}}, {{0, 0}, {1, 1}, {2, 2}}, {{0, 0}, {2, 0}, {0, 2}, {2, 2}},
    {{0, 0}, {2, 0}, {1, 1}, {0, 2}, {2, 2}}, {{0, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {2, 2}}};

static void dc_die_in(int32_t x, int32_t y, int32_t sz, uint32_t face, uint16_t pip, uint16_t top, uint16_t side);
static void dc_die(int32_t x, int32_t y, int32_t sz, uint32_t face, uint16_t pip)
{
    dc_die_in(x, y, sz, face, pip, C_DIE, C_DIESH);
}
static void dc_die_in(int32_t x, int32_t y, int32_t sz, uint32_t face, uint16_t pip, uint16_t top, uint16_t side)
{
    uint32_t i;
    int32_t r = sz / 6, step = sz / 4, pr = sz / 11;
    int32_t b, d = sz >= 40 ? 5 : 2;     /* its depth: the side below the face */
    if (pr < 1)
        pr = 1;
    for (b = d; b >= 0; b -= d) {        /* the side (offset down), then the face over it */
        uint16_t c = b ? side : top;
        cv_rect(x + r, y + b, sz - 2 * r, sz - d, c);
        cv_rect(x, y + r + b, sz, sz - d - 2 * r, c);
        disc(x + r, y + r + b, r, c);
        disc(x + sz - 1 - r, y + r + b, r, c);
        disc(x + r, y + sz - d - 1 - r + b, r, c);
        disc(x + sz - 1 - r, y + sz - d - 1 - r + b, r, c);
    }
    sz -= d;
    face %= 7u;
    for (i = 0; i < face; i++)
        disc(x + step + DC_PIPS[face][i][0] * step, y + step + DC_PIPS[face][i][1] * (sz / 4) - sz / 20, pr, pip);
}

static const char *const DC_LOCK_ROWS[] = {".aaa.", "a...a", "a...a", "aaaaa", "aabaa", "aabaa", "aaaaa"};
static const char *const DC_OPEN_ROWS[] = {".aaa.", "a...a", "....a", "aaaaa", "aabaa", "aabaa", "aaaaa"};

static void dc_scene(void)
{
    uint32_t now = plat_ms(), i, tumbling = dc_roll_at && now - dc_roll_at < 520u;
    char t[40], n[12];
    int32_t dx = 0, dy = 0;
    const uint16_t KIND_COL[DC_NKIND] = {C_INK, C_PINK, C_CYAN, C_PURP, C_YEL, C_PURP, C_PINK, C_YEL, C_CYAN};
    uint16_t kc = KIND_COL[dgen.kind];
    tcpy(t, "DIE: ");
    tcat(t, DC_KIND_NAME[dcs.kind]);
    header("DICE", C_CYAN, "SYNTH", C_PINK, t);
    cv_rect(8, 24, 224, 1, C_LINE2);
    /* the die, tumbling for a moment after a roll */
    if (tumbling) {
        float p = (float)(now - dc_roll_at) / 520.0f;
        dy = -(int32_t)(22.0f * fm_fabsf(fm_sinf(p * 9.0f)) * (1.0f - p));
        dx = (int32_t)(6.0f * fm_sinf(p * 23.0f) * (1.0f - p));
        if ((now / 55u) % 7u != dc_tumble_face)
            dc_tumble_face = (uint8_t)(1u + (now / 55u * 2654435761u >> 29) % 6u);
    }
    dc_die(12 + dx, 36 + dy, 62, tumbling ? dc_tumble_face : dgen.face, dcs.cur.cursed ? C_PURP : C_PINK);
    /* the name */
    if (!tumbling) {
        const char *a = DC_ADJ[dgen.adj], *no = dc_noun();
        const felucca_font_t *fa = text_w(&FONT_M, a) <= 146 ? &FONT_M : &FONT_B;
        const felucca_font_t *fn = text_w(&FONT_M, no) <= 146 ? &FONT_M : &FONT_B;
        cv_text(86, 36, fa, a, C_INK);
        cv_text(86, 62, fn, no, kc);
        tcpy(t, "#");
        {
            static const char HEX[] = "0123456789ABCDEF";
            uint32_t h = dcs.cur.seed[0] ^ dcs.cur.seed[1] * 3u ^ dcs.cur.seed[2] * 5u ^ dcs.cur.seed[3] * 7u;
            for (i = 0; i < 4u; i++)
                t[1 + i] = HEX[(h >> (28 - 4 * i)) & 15u];
            t[5] = 0;
        }
        tcat(t, "  ");
        tcat(t, DC_KIND_NAME[dgen.kind]);
        if (dcs.cur.cursed)
            tcat(t, " (CURSED)");
        if (dcs.octave) {
            tcat(t, "  OCT ");
            if (dcs.octave > 0)
                tcat(t, "+");
            itos(dcs.octave, n);
            tcat(t, n);
        }
        cv_text(86, 90, &FONT_XS, t, C_GHOST);
    } else {
        cv_text(86, 48, &FONT_M, "ROLLING", C_GHOST);
    }
    /* the sound itself */
    frame_rect(8, 108, 224, 44, C_LINE2);
    scope_draw(9, 109, 222, 42, k_level > 0.02f ? kc : C_GHOST);
    /* the locks */
    for (i = 0; i < DC_NLOCK; i++) {
        int32_t lx = 8 + (int32_t)i * 57, on = dcs.locks >> i & 1u;
        uint16_t map[2] = {on ? C_YEL : C_GHOST, C_BG};
        cv_rect(lx, 158, 53, 34, on ? RGB(48, 38, 12) : RGB(20, 15, 34));
        frame_rect(lx, 158, 53, 34, on ? C_YEL : C_LINE2);
        sprite(lx + 4, 162, on ? DC_LOCK_ROWS : DC_OPEN_ROWS, 7, 2, map);
        cv_text(lx + 17, 161, &FONT_T, DC_LOCK_KEY[i], on ? C_YEL : C_GHOST);
        text_c(lx + 26, 176, &FONT_XS, DC_LOCK_NAME[i], on ? C_INK : C_PURP);
    }
    /* the macros */
    for (i = 0; i < 4u; i++) {
        int32_t mx = 8 + (int32_t)i * 57, v = dcs.macro[i];
        cv_text(mx, 200, &FONT_T, DC_MACRO_NAME[i], C_CYAN);
        cv_rect(mx, 216, 53, 6, C_LINE2);
        cv_rect(mx, 216, 53 * v / 100, 6, i == 0 ? C_PINK : i == 1 ? C_PURP : i == 2 ? C_CYAN : C_YEL);
        cv_rect(mx + 26, 214, 1, 10, C_GHOST);                       /* the roll's own (50) */
        itos(v, n);
        cv_text(mx, 226, &FONT_T, n, C_GHOST);
    }
}

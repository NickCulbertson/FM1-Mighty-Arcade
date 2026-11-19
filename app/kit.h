/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* kit.h: what the three FM-1 prototypes share (Dice Synth, Mighty Arcade, Key Panic). Part of the unity build, after
 * the platform (gfx.c in scope), included once from app.c.
 *
 *  - text helpers, and the whole 240 x 240 screen drawn in two bands of 120 rows (the canvas holds 240 x 144)
 *  - notes from the UI to the audio with sample times: the UI schedules a little ahead, the render plays each at its
 *    sample (k_at), so timing never depends on the frame rate
 *  - sounds as data (Mighty Arp's, its engine's terms), each note in a sound of its own (eng_note_on_with)
 *  - a scope of the output */
#include "fastmath.h"
#include "dsp/engine.h"

/* while the screen draws, the clock stands still: the screen is drawn in two bands of 120 rows, one after the other,
 * and anything timed (a ring growing, a flash fading, a star spinning) read a little later for the lower band tore at
 * row 120 (Nick, 19 Nov 2026). The games' own logic, outside the drawing, keeps the real clock */
static uint32_t k_draw_ms;               /* the frame's time while it's drawn; 0 not drawing */
static uint32_t k_ms(void) { return k_draw_ms ? k_draw_ms : (plat_ms)(); }
#define plat_ms() k_ms()
#define SR_F 44100.0f
#define SR_U 44100u

/* ------------------------------------------------------------------- text --- */
static void tcpy(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}
static void tcat(char *d, const char *s)
{
    while (*d)
        d++;
    tcpy(d, s);
}
static void itos(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void itos0(uint32_t v, int digits, char *b)   /* zero-padded */
{
    int i;
    for (i = digits - 1; i >= 0; i--, v /= 10u)
        b[i] = (char)('0' + v % 10u);
    b[digits] = 0;
}
static int32_t text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* centred */
{
    return cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static int32_t text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* right-aligned */
{
    return cv_text(rx - text_w(f, s), y, f, s, c);
}

/* ---------------------------------------------------------------- drawing --- */
static void frame_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* an outline, 1 px */
{
    cv_rect(x, y, w, 1, c);
    cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c);
    cv_rect(x + w - 1, y, 1, h, c);
}
static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y, x;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r * r + r)
                cv_pset(cx + x, cy + y, c);
}
static uint16_t mix565(uint16_t a, uint16_t b, int32_t t256)   /* a .. b, t of 256 */
{
    int32_t ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31, rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
    t256 = clamp(t256, 0, 256);
    return (uint16_t)(((ra + (rb - ra) * t256 / 256) << 11) | ((ga + (gb - ga) * t256 / 256) << 5)
                      | (ba + (bb - ba) * t256 / 256));
}
/* a sprite: rows of characters, '.' clear, else a colour from the map (index by char - 'a') */
static void sprite(int32_t x, int32_t y, const char *const *rows, int32_t nrows, int32_t scale, const uint16_t *map)
{
    int32_t r, c;
    for (r = 0; r < nrows; r++)
        for (c = 0; rows[r][c]; c++)
            if (rows[r][c] != '.')
                cv_rect(x + c * scale, y + r * scale, scale, scale, map[rows[r][c] - 'a']);
}

/* ------------------------------------------------------------------ juice --- *
 * What every game shares to feel alive: a screen shake (the drawn band shifted) and a kick (the screen knocked one way,
 * springing back), a flash (the band tinted, fading), an impact frame (the screen inverted a moment), a glitch (rows
 * torn sideways), particles (sparks that fly, white-hot, and fall), shockwave rings, floating and popping text,
 * confetti, a confetti fountain and fireworks. The games call juice_*() when something happens; screen_draw moves and
 * draws it all, after the game's scene, so no game draws any of it.
 * JUICE_N / JUICE_D scales it all at once (shake, sparks, hit-stops, rings): 1/1 is the second pass, 3/2 this one. */
#define JUICE_N 3
#define JUICE_D 2
#define JX(v) ((v) * JUICE_N / JUICE_D)
#define JP_N 192
#define JT_N 8
typedef struct {
    int16_t x, y, vx, vy;                /* in 1/16 px, and 1/16 px a frame */
    uint8_t life, max, size, grav;       /* grav: 0 none, 1 falls, 2 flutters (confetti), 3 rises (embers) */
    uint16_t c;
} jpart_t;
typedef struct {
    int16_t x, y;
    uint8_t life, big;
    uint16_t c;
    char s[28];                          /* (was 16: longer words were cut at 15 letters) */
} jtext_t;
#define JR_N 16
typedef struct {                         /* a shockwave: a ring that grows and fades */
    int16_t x, y, r;
    uint16_t ms, c;
    uint32_t at;
} jring_t;
static struct {
    uint32_t seed, last, frame;
    int32_t shake, shake_max;            /* px, (fading over shake_ms) */
    uint32_t shake_at, shake_ms;
    int32_t kick_x, kick_y;              /* a kick: px at its start, springing back over 360 ms */
    uint32_t kick_at;
    uint16_t flash_c;
    uint32_t flash_at, flash_ms;
    int32_t flash_a;                     /* 0..256 at its start */
    uint32_t impact_until, glitch_until; /* an inverted frame; torn rows */
    int16_t fx, fy;                      /* the confetti fountain: from, until */
    uint32_t fount_until;
    uint32_t fire_until, fire_next;      /* fireworks: bursts all over, until */
    int32_t jx, jy;                      /* this frame's shake */
    jpart_t p[JP_N];
    jtext_t t[JT_N];
    jring_t r[JR_N];
    uint32_t dms;                        /* the frame's time, for drawing: both bands of the screen draw the same
                                          * moment (rings, the flash, the beat: their halves tore apart at row 120,
                                          * Nick 19 Nov 2026) */
    uint32_t freeze_until;               /* a hit-stop: the game holds still a moment, the hit lands */
    /* the music: the heat (0..100) your play builds and idling cools (the groove grows a sparkle with it), a stinger
     * asked for (played on the next sixteenth), and the beats coming (when, in ms), the screen pulsing on each */
    int32_t heat;
    uint32_t heat_at;
    uint8_t sting, bar;
    uint32_t beat_ms[4];
    uint8_t beat_acc[4];
    uint32_t pulse_at;
    uint8_t pulse_acc;
} J;
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static const uint16_t J_RAINBOW[6] = {0xF9EF, 0x07FF, 0xFFE0, 0x87F0, 0xFC1F, 0xFD20};
static int32_t j_rand(int32_t n)         /* 0 .. n-1 */
{
    J.seed = J.seed * 1664525u + 1013904223u;
    return n > 0 ? (int32_t)((J.seed >> 8) % (uint32_t)n) : 0;
}
static void juice_reset(void)
{
    uint32_t seed = J.seed, i;
    uint8_t *b = (uint8_t *)&J;
    for (i = 0; i < sizeof J; i++)
        b[i] = 0;
    J.seed = seed ? seed : 0x1234567u;
}
static void juice_heat(int32_t n)       /* your play heats the music up (and the screen's pulse with it) */
{
    J.heat = J.heat + n > 100 ? 100 : J.heat + n;
}
static void juice_cool(void) { J.heat = 0; J.sting = 0; }   /* the music drops back: a death, a loss */
static void juice_sting(int kind) { if (kind > J.sting) J.sting = (uint8_t)kind; }   /* 1 a phrase, 2 a fanfare */
static void juice_beat_at(uint32_t ms, int accent)   /* the groove says: a beat at ms (accent: the bar's first) */
{
    int i;
    for (i = 0; i < 4; i++)
        if (!J.beat_ms[i]) {
            J.beat_ms[i] = ms | 1u;
            J.beat_acc[i] = (uint8_t)accent;
            return;
        }
}
static int32_t juice_pulse(void)        /* 256 on the beat, gone 160 ms later */
{
    uint32_t q = (J.dms ? J.dms : plat_ms()) - J.pulse_at;
    return J.pulse_at && q < 160u ? 256 - (int32_t)(q * 256u / 160u) : 0;
}
static void juice_kick(int32_t dx, int32_t dy)   /* the screen knocked dx, dy px, springing back (a bigger one wins) */
{
    uint32_t now = plat_ms();
    int32_t left = J.kick_at && now - J.kick_at < 360u ? (iabs(J.kick_x) + iabs(J.kick_y)) * (int32_t)(360u - (now - J.kick_at)) / 360 : 0;
    dx = JX(dx);
    dy = JX(dy);
    if (iabs(dx) + iabs(dy) < left)
        return;
    J.kick_x = dx;
    J.kick_y = dy;
    J.kick_at = now | 1u;
}
static void juice_impact(uint32_t ms) { J.impact_until = plat_ms() + ms; }   /* the screen inverted: the hit's frame */
static void juice_glitch(uint32_t ms) { J.glitch_until = plat_ms() + ms; }   /* rows torn sideways */
static void juice_shake(int32_t px, uint32_t ms)   /* a shake: px at its start, fading over ms (a bigger one wins) */
{
    uint32_t now = plat_ms();
    int32_t left = J.shake_ms && now - J.shake_at < J.shake_ms
                       ? J.shake_max * (int32_t)(J.shake_ms - (now - J.shake_at)) / (int32_t)J.shake_ms : 0;
    px = JX(px);
    ms = JX(ms);
    if (px < left)
        return;
    J.shake_max = px;
    J.shake_at = now;
    J.shake_ms = ms;
    juice_heat(px);
    if (px >= 6)                         /* a big one knocks the screen too */
        juice_kick(j_rand(2) ? px / 2 : -px / 2, j_rand(2) ? px / 3 : -px / 3);
    if (px >= 11)
        juice_glitch(ms / 3);
}
static void juice_flash(uint16_t c, int32_t a256, uint32_t ms)   /* the screen tinted c, a of 256, fading over ms */
{
    J.flash_c = c;
    J.flash_a = a256 + a256 / 3 > 256 ? 256 : a256 + a256 / 3;
    J.flash_at = plat_ms();
    J.flash_ms = ms;
}
static jpart_t *j_part(void)             /* a free spark (full: the oldest goes) */
{
    int32_t i;
    jpart_t *q;
    for (i = 0; i < JP_N; i++)
        if (!J.p[i].life)
            return &J.p[i];
    q = &J.p[0];
    for (i = 1; i < JP_N; i++)
        if (J.p[i].life < q->life)
            q = &J.p[i];
    return q;
}
/* n sparks from x, y: speed in px a frame (at most), colour c (and a second, c2, mixed in), gravity (see jpart_t) */
static void juice_burst(int32_t x, int32_t y, int32_t n, int32_t speed, uint16_t c, uint16_t c2, int grav)
{
    int32_t k;
    n = JX(n);
    speed = speed + speed / 3;
    for (k = 0; k < n; k++) {
        jpart_t *q = j_part();
        int32_t a = j_rand(64), sp = 6 + j_rand(speed * 16 > 6 ? speed * 16 - 6 : 1);
        int32_t cx = fm_cosf((float)a * 0.0981748f) * (float)sp, cy = fm_sinf((float)a * 0.0981748f) * (float)sp;
        q->x = (int16_t)(x * 16);
        q->y = (int16_t)(y * 16);
        q->vx = (int16_t)cx;
        q->vy = (int16_t)(cy - (grav == 1 || grav == 2 ? 24 : 0));
        q->max = q->life = (uint8_t)(grav == 2 ? 40 + j_rand(30) : 12 + j_rand(16));
        q->size = (uint8_t)(1 + j_rand(grav == 2 ? 2 : 4));
        q->grav = (uint8_t)grav;
        q->c = j_rand(3) ? c : c2;
    }
}
static void juice_embers(int32_t x, int32_t y, int32_t n, uint16_t c)   /* sparks that drift up and wink out */
{
    juice_burst(x, y, n, 2, c, 0xFFE0, 3);
}
static void juice_text_(int32_t x, int32_t y, const char *s, uint16_t c, int big)
{
    int32_t i, k = 0, tries;
    for (i = 0; i < JT_N; i++)
        if (J.t[i].life < J.t[k].life)
            k = i;
    /* text never lands on text (Nick, 19 Nov 2026: overlapping words didn't read): a big pop sends the one before it
     * off at once (the newest moment wins); a small one steps clear of a big one, then up a line past the others */
    if (big) {
        for (i = 0; i < JT_N; i++)
            if (J.t[i].life && J.t[i].big && J.t[i].life > 6)
                J.t[i].life = 6;
    } else {
        int32_t w = text_w(&FONT_M, s), moved = 1;
        for (tries = 0; tries < 6 && moved; tries++) {
            moved = 0;
            for (i = 0; i < JT_N; i++) {
                int32_t ow, oy;
                if (!J.t[i].life || J.t[i].life <= 6)
                    continue;
                oy = J.t[i].y;
                ow = text_w(J.t[i].big ? &FONT_L : &FONT_M, J.t[i].s);
                if ((x - w / 2 < J.t[i].x + ow / 2 + 4) && (J.t[i].x - ow / 2 < x + w / 2 + 4) &&
                    y > oy - (J.t[i].big ? 18 : 16) && y < oy + (J.t[i].big ? 34 : 16)) {
                    y = J.t[i].big ? oy + 36 : oy - 17;   /* under a big one; over a small one */
                    if (y > 220)
                        y = oy - 18;
                    if (y < 22)
                        y = 22;
                    moved = 1;
                }
            }
        }
    }
    J.t[k].x = (int16_t)x;
    J.t[k].y = (int16_t)y;
    J.t[k].life = (uint8_t)(big ? 48 : 28);
    J.t[k].big = (uint8_t)big;
    J.t[k].c = c;
    for (i = 0; i < 27 && s[i]; i++)
        J.t[k].s[i] = s[i];
    J.t[k].s[i] = 0;
}
static void juice_ring(int32_t x, int32_t y, int32_t r, uint16_t c, uint32_t ms);
static void juice_text(int32_t x, int32_t y, const char *s, uint16_t c) { juice_text_(x, y, s, c, 0); }   /* rises, fades */
static void juice_pop(int32_t x, int32_t y, const char *s, uint16_t c)   /* big: it slams in, letters waving */
{
    juice_text_(x, y, s, c, 1);
    juice_burst(x, y + 8, 10, 4, c, 0xFFFF, 0);
    juice_heat(14);
    juice_sting(1);
}
static void juice_ring(int32_t x, int32_t y, int32_t r, uint16_t c, uint32_t ms)   /* a shockwave out to r over ms */
{
    int32_t i, k = 0;
    for (i = 0; i < JR_N; i++)
        if (J.r[i].at < J.r[k].at)
            k = i;
    J.r[k].x = (int16_t)x;
    J.r[k].y = (int16_t)y;
    J.r[k].r = (int16_t)JX(r);
    J.r[k].c = c;
    J.r[k].ms = (uint16_t)JX(ms);
    J.r[k].at = plat_ms() | 1u;
    if (r >= 12)                         /* a big ring throws sparks of its colour */
        juice_burst(x, y, r / 4, 3 + r / 16, c, 0xFFFF, 0);
}
static void juice_freeze(uint32_t ms)   /* a hit-stop; a long one gets an impact frame */
{
    ms = JX(ms);
    J.freeze_until = plat_ms() + ms;
    if (ms >= 70u)
        juice_impact(ms >= 140u ? 70u : 40u);
}
static int juice_frozen(void) { return (int32_t)(J.freeze_until - plat_ms()) > 0; }
static void juice_confetti(int32_t x, int32_t y, int32_t n)   /* every colour, up and fluttering down */
{
    int32_t i;
    for (i = 0; i < n; i++)
        juice_burst(x, y, 1, 6, J_RAINBOW[i % 6], J_RAINBOW[(i + 3) % 6], 2);
}
static void juice_fountain(int32_t x, int32_t y, uint32_t ms)   /* confetti pouring up from x, y for ms */
{
    J.fx = (int16_t)x;
    J.fy = (int16_t)y;
    J.fount_until = plat_ms() + ms;
}
static void juice_calm(void)            /* a card coming up: the fireworks, the fountain and the words stop */
{
    int i;
    J.fire_until = J.fount_until = 0;
    for (i = 0; i < JT_N; i++)
        J.t[i].life = 0;
}
static void juice_fireworks(uint32_t ms)   /* bursts all over the screen for ms */
{
    J.fire_until = plat_ms() + ms;
    J.fire_next = plat_ms();
}
/* the big one: every effect at once, for a win, a checkmate, a cleared dungeon */
static void juice_celebrate(int32_t x, int32_t y)
{
    juice_freeze(90);
    juice_flash(0xFFFF, 200, 260);
    juice_shake(8, 420);
    juice_ring(x, y, 90, 0xFFE0, 500);
    juice_ring(x, y, 60, 0xF9EF, 380);
    juice_confetti(x, y, 40);
    juice_fountain(x, 236, 1800);
    juice_fireworks(3000);
    J.heat = 100;
    juice_sting(2);
}
static void juice_step(void)             /* once a frame: the sparks move, the shake picks this frame's offset */
{
    int32_t i;
    uint32_t now = plat_ms();
    J.frame++;
    J.dms = now;
    if (now - J.heat_at >= 140u) {       /* the heat cools, slowly */
        J.heat_at = now;
        if (J.heat > 0)
            J.heat--;
    }
    for (i = 0; i < 4; i++)              /* a beat arriving: the pulse */
        if (J.beat_ms[i] && (int32_t)(now - J.beat_ms[i]) >= 0) {
            J.pulse_at = now | 1u;
            J.pulse_acc = J.beat_acc[i];
            if (J.beat_acc[i])
                J.bar++;
            J.beat_ms[i] = 0;
        }
    if ((int32_t)(J.fount_until - now) > 0)   /* the fountain */
        for (i = 0; i < 3; i++) {
            jpart_t *q = j_part();
            q->x = (int16_t)((J.fx + j_rand(41) - 20) * 16);
            q->y = (int16_t)(J.fy * 16);
            q->vx = (int16_t)(j_rand(61) - 30);
            q->vy = (int16_t)(-90 - j_rand(50));
            q->max = q->life = (uint8_t)(50 + j_rand(20));
            q->size = (uint8_t)(2 + j_rand(2));
            q->grav = 2;
            q->c = J_RAINBOW[j_rand(6)];
        }
    if ((int32_t)(J.fire_until - now) > 0 && (int32_t)(now - J.fire_next) >= 0) {   /* fireworks */
        int32_t fx = 30 + j_rand(180), fy = 40 + j_rand(110);
        uint16_t c = J_RAINBOW[j_rand(6)];
        juice_burst(fx, fy, 16, 4, c, 0xFFFF, 1);
        juice_ring(fx, fy, 22, c, 300);
        J.fire_next = now + 180u + (uint32_t)j_rand(260);
    }
    for (i = 0; i < JP_N; i++) {
        jpart_t *q = &J.p[i];
        if (!q->life)
            continue;
        q->life--;
        q->x = (int16_t)(q->x + q->vx);
        q->y = (int16_t)(q->y + q->vy);
        switch (q->grav) {
        case 1:                          /* falls; bounces once off the floor */
            q->vx = (int16_t)(q->vx * 15 / 16);
            q->vy = (int16_t)(q->vy + 5);
            if (q->y > 236 * 16 && q->vy > 0)
                q->vy = (int16_t)(-q->vy / 2);
            break;
        case 2:                          /* flutters: slow fall, swaying */
            q->vx = (int16_t)(q->vx * 15 / 16 + (((J.frame + i * 7) & 15) < 8 ? 2 : -2));
            q->vy = (int16_t)(q->vy < 18 ? q->vy + 3 : 18);
            break;
        case 3:                          /* rises */
            q->vx = (int16_t)(q->vx * 7 / 8 + j_rand(5) - 2);
            q->vy = (int16_t)(q->vy * 7 / 8 - 3);
            break;
        default:
            q->vx = (int16_t)(q->vx * 7 / 8);
            q->vy = (int16_t)(q->vy * 7 / 8);
        }
    }
    for (i = 0; i < JT_N; i++)
        if (J.t[i].life) {
            J.t[i].life--;
            if (!J.t[i].big || J.t[i].life < 16)
                J.t[i].y--;
        }
    J.jx = J.jy = 0;
    if (J.shake_ms && now - J.shake_at < J.shake_ms) {
        int32_t a = J.shake_max * (int32_t)(J.shake_ms - (now - J.shake_at)) / (int32_t)J.shake_ms;
        if (a > 0) {
            J.jx = j_rand(2 * a + 1) - a;
            J.jy = j_rand(2 * a + 1) - a;
        }
    }
    if (J.kick_at && now - J.kick_at < 360u) {   /* the kick: a damped spring */
        float t = (float)(now - J.kick_at) / 360.0f, s = (1.0f - t) * (1.0f - t) * fm_cosf(t * 15.7f);
        J.jx += (int32_t)((float)J.kick_x * s);
        J.jy += (int32_t)((float)J.kick_y * s);
    }
}
static void juice_draw(void)             /* over the scene, in screen coordinates (each band) */
{
    int32_t i, pu = juice_pulse();
    if (pu && J.heat >= 8) {             /* the beat: the screen's edge glows with it, hotter as the music heats */
        int32_t a = pu * (J.heat + 20) / 120 * (J.pulse_acc ? 4 : 3) / 4, w = J.heat >= 60 ? 3 : 2;
        uint16_t c = mix565(0, J_RAINBOW[J.bar % 6u], a);
        cv_rect(0, 0, 240, w, c);
        cv_rect(0, 240 - w, 240, w, c);
        cv_rect(0, 0, w, 240, c);
        cv_rect(240 - w, 0, w, 240, c);
    }
    for (i = 0; i < JP_N; i++) {
        jpart_t *q = &J.p[i];
        int32_t sz, px, py;
        uint16_t c;
        if (!q->life)
            continue;
        sz = q->life * 3 > q->max ? q->size : 1;
        px = q->x / 16;
        py = q->y / 16;
        c = q->max - q->life < 2 && q->grav != 2 ? 0xFFFF   /* white-hot as it leaves */
            : q->life < 5 ? mix565(0, q->c, q->life * 50) : q->c;
        if (q->grav == 0 || q->grav == 1) {   /* a fast one streaks */
            int32_t tx = q->vx / 24, ty = q->vy / 24;
            if (tx || ty)
                cv_rect(px - tx - (tx < 0 ? 0 : 0), py - ty, 1, 1, mix565(0, c, 140));
            if (iabs(tx) + iabs(ty) > 2)
                cv_rect(px - tx / 2, py - ty / 2, sz > 1 ? sz - 1 : 1, sz > 1 ? sz - 1 : 1, mix565(0, c, 200));
        }
        if (q->grav == 2 && ((J.frame + i) & 4))   /* confetti turns: thin, then full */
            cv_rect(px - sz / 2, py, sz + 1, 1, c);
        else
            cv_rect(px - sz / 2, py - sz / 2, sz, sz, c);
    }
    for (i = 0; i < JR_N; i++) {         /* the shockwaves: a white flash, then a thick ring and a white one inside */
        jring_t *g = &J.r[i];
        uint32_t q = J.dms - g->at;      /* (the frame's time, not the band's) */
        if (g->at && q < g->ms) {
            int32_t rr = g->r * (int32_t)q / g->ms + 2, a, fade = 256 - (int32_t)(q * 256u / g->ms);
            uint16_t c = mix565(0x0000, g->c, fade), w = mix565(0x0000, 0xFFFF, fade * 3 / 4);
            int32_t steps = rr * 6 + 8, ri = rr * 3 / 4;
            if (q < 50u && g->r >= 12)
                disc(g->x, g->y, 3 + g->r / 6, 0xFFFF);
            for (a = 0; a < steps; a++) {
                float th = (float)a * 6.2831853f / (float)steps, cs = fm_cosf(th), sn = fm_sinf(th);
                cv_rect(g->x + (int32_t)(cs * (float)rr) - 1, g->y + (int32_t)(sn * (float)rr) - 1, 3, 3, c);
                if (!(a & 1))
                    cv_rect(g->x + (int32_t)(cs * (float)ri), g->y + (int32_t)(sn * (float)ri), 1, 1, w);
            }
        }
    }
    for (i = 0; i < JT_N; i++)
        if (J.t[i].life) {
            const felucca_font_t *f = J.t[i].big ? &FONT_L : &FONT_M;
            int32_t y = J.t[i].y, l = J.t[i].life;
            uint16_t c = l > 6 ? J.t[i].c : mix565(0, J.t[i].c, l * 40);
            if (!J.t[i].big) {
                text_c(J.t[i].x + 1, y + 1, f, J.t[i].s, 0);
                text_c(J.t[i].x - 1, y, f, J.t[i].s, 0);
                text_c(J.t[i].x, y, f, J.t[i].s, c);
            } else {                     /* slams down, overshoots, settles; each letter waves, outlined */
                int32_t x0 = J.t[i].x - text_w(f, J.t[i].s) / 2, k;
                char ch[2] = {0, 0};
                if (l > 40)
                    y -= (l - 40) * 6;
                else if (l > 34)
                    y += (l - 34);
                if (l > 38 && (l & 1))
                    c = 0xFFFF;          /* a white blink as it lands */
                for (k = 0; J.t[i].s[k]; k++) {
                    int32_t wy = y + (l <= 40 ? (int32_t)(fm_sinf((float)(J.frame + k * 3) * 0.45f) * (2.5f + (float)pu / 64.0f)) : 0);
                    ch[0] = J.t[i].s[k];
                    cv_text(x0 - 1, wy, f, ch, 0);
                    cv_text(x0 + 1, wy, f, ch, 0);
                    cv_text(x0, wy - 1, f, ch, 0);
                    cv_text(x0 + 2, wy + 2, f, ch, 0);
                    cv_text(x0, wy, f, ch, c);
                    x0 += text_w(f, ch);
                }
            }
        }
}
static void j_shift_row(uint16_t *row, int32_t dx, uint16_t sbg)
{
    int32_t x;
    if (dx > 0) {
        for (x = (int32_t)cv_w - 1; x >= dx; x--)
            row[x] = row[x - dx];
        for (x = 0; x < dx && x < (int32_t)cv_w; x++)
            row[x] = sbg;
    } else if (dx < 0) {
        for (x = 0; x < (int32_t)cv_w + dx; x++)
            row[x] = row[x - dx];
        for (x = (int32_t)cv_w + dx; x < (int32_t)cv_w; x++)
            if (x >= 0)
                row[x] = sbg;
    }
}
static void juice_post(uint16_t bg)      /* the drawn band: shaken sideways, torn, tinted by the flash, inverted */
{
    uint32_t now = J.dms ? J.dms : plat_ms(), r;   /* (the frame's time: both bands alike) */
    uint16_t sbg = swap16(bg);
    int glitch = (int32_t)(J.glitch_until - now) > 0;
    if (J.jx || glitch)
        for (r = 0; r < cv_h; r++) {
            int32_t dx = J.jx;
            if (glitch && ((r + J.frame * 13u) % 23u) < 3u)   /* a torn band of rows */
                dx += (int32_t)((J.frame * 7u + r / 8u) % 25u) - 12;
            if (dx)
                j_shift_row(cv_px + r * cv_w, dx > (int32_t)cv_w ? (int32_t)cv_w : dx, sbg);
        }
    if (J.flash_ms && now - J.flash_at < J.flash_ms) {
        int32_t a = J.flash_a * (int32_t)(J.flash_ms - (now - J.flash_at)) / (int32_t)J.flash_ms;
        uint32_t i, n = cv_w * cv_h;
        for (i = 0; i < n; i++)
            cv_px[i] = swap16(mix565(swap16(cv_px[i]), J.flash_c, a));
    }
    if ((int32_t)(J.impact_until - now) > 0) {   /* the impact frame: everything inverted */
        uint32_t i, n = cv_w * cv_h;
        for (i = 0; i < n; i++)
            cv_px[i] = (uint16_t)~cv_px[i];
    }
}

/* the whole screen, in two bands of 120 rows: scene() draws in screen coordinates, twice (shaken by the juice) */
static uint32_t k_frame;                 /* frames drawn (scope_draw: one snapshot a frame, both bands) */
static void screen_draw(void (*scene)(void), uint16_t bg)
{
    int32_t b;
    k_frame++;
    k_draw_ms = 0;
    juice_step();
    k_draw_ms = (plat_ms)() | 1u;        /* (one moment for both bands) */
    for (b = 0; b < 2; b++) {
        cv_begin(240, 120, bg);
        cv_oy = -120 * b + J.jy;         /* (gfx.c adds cv_oy to every y and clips to the band's rows) */
        cv_y0 = 0;
        cv_y1 = 120;
        scene();
        juice_draw();
        juice_post(bg);
        cv_blit(0, (uint32_t)(120 * b));
    }
    k_draw_ms = 0;
}

/* ----------------------------------------------------------------- sounds --- */
#define OP(r, c, l, w, st, a, d, s) {(r), (c), (l), (w), (st), (a), (d), (s)}
#define CARRIER(r, c, l) OP(r, c, l, 0, 0, 0.0f, 0.0f, 0.0f)
#define SAWC(r, c, l) OP(r, c, l, 8, 0, 0.0f, 0.0f, 0.0f)
#define WAVC(r, c, l, w) OP(r, c, l, w, 0, 0.0f, 0.0f, 0.0f)
#define NOISEC(l) OP(1, 0, l, 13, 0, 0.0f, 0.0f, 0.0f)
#define Z CARRIER(1, 0, 0)
/* a sound as Mighty Arp writes its own: algorithm, feedback, the operators (and mono, glide), the envelope's knobs
 * (0..100: knob_secs), a gain */
typedef struct {
    const char *name;
    uint8_t alg, fb;
    eng_sound_t s;
    int8_t a, d, su, r;
    float gain;
} ksnd_t;
static float knob_secs(int32_t v) { return 0.001f * fm_exp2f((float)v * 0.1328771f); }   /* 10000 ^ (v / 100) */

/* Mighty Arp's sounds the prototypes play */
enum { S_TINES, S_DREAM, S_VIBES, S_MARIMBA, S_CELESTA, S_CRYSTAL, S_TUBE, S_HARP, S_KOTO, S_NYLON, S_FMBASS,
       S_16BASS, S_SQBASS, S_KICK, S_SNARE, S_HAT, S_LOGDRUM, S_BRASSPAD, S_GLASSPAD, S_DRONE, S_DEEPSEA, S_SYNCLEAD,
       S_METAL, S_CHIP, S_FLUTE, S_SONAR, S_SQLEAD, S_SAWPLUCK, S_SOFTSAW, S_TDRONE, NKSND };
static const ksnd_t KSND[NKSND] = {
    {"TINES EP", 4, 0, {{CARRIER(1, 0, 1.0f), OP(1, 0, 0.25f, 0, 0, 0, 1.2f, 0.15f),
                         OP(1, 3, 0.35f, 0, 1, 0, 2.0f, 0.08f), OP(14, 0, 0.21f, 0, 0, 0, 0.07f, 0)}, 0, 0},
     0, 74, 25, 82, 1.0f},
    {"DREAM EP", 4, 0, {{CARRIER(1, 0, 1.0f), OP(1, 0, 0.18f, 0, 0, 0, 2.5f, 0.2f),
                         OP(2, 5, 0.25f, 0, 1, 0, 3.5f, 0.15f), OP(14, 0, 0.08f, 0, 0, 0, 0.1f, 0)}, 0, 0},
     8, 86, 35, 92, 1.0f},
    {"VIBES", 4, 0, {{CARRIER(1, 0, 1.0f), OP(4, 0, 0.12f, 0, 0, 0, 0.4f, 0),
                      OP(4, 0, 0.14f, 0, 1, 0, 0.5f, 0), OP(1, 0, 0.05f, 0, 0, 0, 0.3f, 0)}, 0, 0},
     0, 84, 0, 82, 1.0f},
    {"MARIMBA", 4, 0, {{CARRIER(1, 0, 1.0f), OP(4, 0, 0.32f, 0, 0, 0, 0.03f, 0),
                        OP(1, 0, 0.6f, 0, 1, 0, 0.5f, 0), OP(1, 0, 0.08f, 0, 0, 0, 0.12f, 0)}, 0, 0},
     0, 58, 0, 66, 1.0f},
    {"CELESTA", 4, 0, {{CARRIER(1, 0, 1.0f), OP(6, 0, 0.27f, 0, 0, 0, 0.35f, 0.02f),
                        OP(4, 0, 0.42f, 0, 1, 0, 0.55f, 0), OP(3, 0, 0.09f, 0, 0, 0, 0.4f, 0)}, 0, 0},
     0, 76, 0, 82, 1.0f},
    {"CRYSTAL", 4, 0, {{CARRIER(1, 0, 1.0f), OP(7, 0, 0.2f, 0, 0, 0, 0.08f, 0.02f),
                        OP(3, 0, 0.3f, 0, 1, 0, 1.2f, 0), OP(9, 0, 0.15f, 0, 0, 0, 0.3f, 0)}, 0, 0},
     0, 76, 0, 90, 1.0f},
    {"TUBE BELLS", 4, 0, {{CARRIER(1, 0, 0.9f), OP(3.5f, 0, 0.3f, 0, 0, 0, 2.5f, 0.1f),
                           CARRIER(1, 7, 0.45f), OP(7, 0, 0.12f, 0, 0, 0, 1.0f, 0)}, 0, 0},
     0, 88, 0, 82, 1.0f},
    {"HARP", 4, 0, {{CARRIER(1, 0, 1.0f), OP(2, 0, 0.24f, 0, 0, 0, 0.15f, 0.1f),
                     OP(1, 0, 0.5f, 0, 1, 0, 1.2f, 0), OP(1, 0, 0.04f, 0, 0, 0, 0.5f, 0)}, 0, 0},
     0, 64, 0, 80, 1.0f},
    {"KOTO", 4, 0, {{CARRIER(1, 0, 1.0f), OP(3, 0, 0.45f, 0, 0, 0, 0.12f, 0.05f),
                     OP(1, 0, 0.35f, 0, 1, 0, 0.5f, 0), OP(5, 0, 0.2f, 0, 0, 0, 0.05f, 0)}, 0, 0},
     0, 66, 0, 72, 1.0f},
    {"NYLON", 4, 0, {{CARRIER(1, 0, 1.0f), OP(1, 0, 0.25f, 0, 0, 0, 0.3f, 0.05f),
                      OP(3, 0, 0.15f, 0, 1, 0, 0.15f, 0), OP(1, 0, 0.1f, 0, 0, 0, 0.1f, 0)}, 0, 0},
     0, 70, 0, 70, 1.0f},
    {"FM BASS", 2, 6, {{CARRIER(0.5f, 0, 0.8f), OP(0.5f, 0, 0.55f, 0, 0, 0, 0.18f, 0.25f),
                        OP(1, 0, 0.25f, 0, 0, 0, 0.12f, 0), OP(0.5f, 0, 0.3f, 0, 0, 0, 0.08f, 0.1f)}, 0, 0},
     0, 62, 70, 35, 1.5f},
    {"16BIT BASS", 0, 6, {{CARRIER(0.5f, 0, 0.85f), OP(1, 0, 0.5f, 0, 0, 0, 0.2f, 0.35f),
                           OP(1, 0, 0.35f, 0, 0, 0, 0.15f, 0.2f), OP(2, 0, 0.3f, 0, 0, 0, 0.1f, 0.1f)}, 0, 0},
     0, 65, 60, 32, 1.35f},
    {"SQUARE BASS", 7, 0, {{WAVC(0.5f, 0, 0.6f, 9), CARRIER(0.5f, 0, 0.4f), Z, Z}, 0, 0},
     0, 60, 60, 25, 2.2f},
    {"FM KICK", 4, 0, {{CARRIER(0.5f, 0, 1.0f), OP(0.5f, 0, 0.6f, 0, 0, 0, 0.03f, 0),
                        OP(1, 0, 0.2f, 13, 1, 0, 0.008f, 0), OP(1, 0, 0, 0, 0, 0, 0.1f, 0)}, 0, 0},
     0, 56, 0, 44, 1.6f},
    {"FM SNARE", 4, 0, {{OP(1, 0, 0.75f, 0, 1, 0, 0.06f, 0), OP(1.41f, 0, 0.35f, 0, 0, 0, 0.03f, 0),
                         NOISEC(0.75f), OP(1, 0, 0, 0, 0, 0, 0.1f, 0)}, 0, 0},
     0, 58, 0, 46, 1.0f},
    {"NOISE HAT", 4, 0, {{NOISEC(0.7f), OP(1, 0, 0, 0, 0, 0, 0.1f, 0),
                          CARRIER(9, 0, 0.25f), OP(14, 0, 0.6f, 0, 0, 0, 0.05f, 0)}, 0, 0},
     0, 36, 0, 30, 0.7f},
    {"LOG DRUM", 4, 0, {{CARRIER(1, 0, 1.0f), OP(1, 0, 0.5f, 0, 0, 0, 0.05f, 0), Z,
                         OP(1, 0, 0, 0, 0, 0, 0.1f, 0)}, 0, 0},
     0, 64, 0, 52, 1.0f},
    {"BRASS PAD", 0, 3, {{CARRIER(1, 0, 0.9f), OP(1, 0, 0.25f, 0, 0, 0.5f, 2.0f, 0.75f),
                          OP(1, 0, 0.1f, 0, 0, 0.6f, 2.0f, 0.6f), OP(1, 0, 0.06f, 0, 0, 0.4f, 2.0f, 0.5f)}, 0, 0},
     66, 85, 95, 82, 0.9f},
    {"GLASS PAD", 4, 0, {{CARRIER(1, 0, 0.7f), OP(1, 0, 0.19f, 0, 0, 0, 4.0f, 0.6f),
                          CARRIER(1, 5, 0.55f), OP(2, 0, 0.13f, 0, 0, 0, 3.0f, 0.5f)}, 0, 0},
     72, 82, 80, 88, 0.9f},
    {"DARK DRONE", 4, 4, {{CARRIER(0.5f, 0, 0.7f), OP(1.41f, 0, 0.3f, 0, 0, 1.5f, 6.0f, 0.8f),
                           CARRIER(1, 7, 0.4f), OP(2.76f, 0, 0.25f, 0, 0, 2.5f, 6.0f, 0.7f)}, 0, 0},
     70, 90, 90, 85, 0.9f},
    {"DEEP SEA", 5, 3, {{CARRIER(0.5f, 0, 0.45f), CARRIER(1, -6, 0.4f), CARRIER(1, 6, 0.4f),
                         OP(1.41f, 0, 0.2f, 0, 0, 1.0f, 4.0f, 0.7f)}, 0, 0},
     70, 85, 85, 90, 0.9f},
    {"SYNC LEAD", 0, 6, {{CARRIER(1, 0, 1.0f), OP(2, 0, 0.35f, 0, 0, 0, 0.6f, 0.6f),
                          OP(1, 0, 0.2f, 0, 0, 0, 0.5f, 0.5f), OP(1, 0, 0.2f, 0, 0, 0, 0.5f, 0.5f)}, 0, 0},
     0, 60, 70, 40, 0.9f},
    {"METAL LEAD", 0, 6, {{CARRIER(1, 0, 0.9f), OP(2, 0, 0.4f, 0, 0, 0, 0.6f, 0.7f),
                           OP(3, 0, 0.25f, 0, 0, 0, 0.5f, 0.6f), OP(1, 0, 0.2f, 0, 0, 0, 0.4f, 0.5f)}, 0, 0},
     2, 60, 85, 50, 0.9f},
    {"CHIP ARP", 7, 0, {{WAVC(1, 0, 0.9f, 11), WAVC(2, 0, 0.2f, 9), Z, Z}, 0, 0},
     0, 55, 30, 30, 0.9f},
    {"PAN FLUTE", 7, 0, {{CARRIER(1, 0, 0.8f), OP(1, 0, 0.05f, 13, 1, 0.01f, 0.12f, 0.06f),
                          CARRIER(3, 0, 0.05f), CARRIER(2, 0, 0.08f)}, 0, 0},
     40, 80, 90, 55, 1.0f},
    {"SONAR", 4, 0, {{CARRIER(1, 0, 1.0f), OP(2.76f, 0, 0.35f, 0, 0, 0, 2.0f, 0.1f),
                      OP(5.4f, 0, 0.3f, 0, 1, 0, 0.6f, 0), OP(1.41f, 0, 0.15f, 0, 0, 0, 1.0f, 0)}, 0, 0},
     0, 88, 0, 92, 1.0f},
    {"SQUARE LEAD", 7, 0, {{WAVC(1, -4, 0.6f, 9), WAVC(1, 4, 0.5f, 9), Z, Z}, 0, 0},
     0, 60, 80, 40, 0.9f},
    {"SAW PLUCK", 0, 6, {{CARRIER(1, 0, 1.0f), OP(1, 0, 0.4f, 0, 0, 0, 0.25f, 0.05f),
                          OP(1, 0, 0.3f, 0, 0, 0, 0.2f, 0), OP(1, 0, 0.3f, 0, 0, 0, 0.2f, 0)}, 0, 0},
     0, 60, 0, 70, 1.0f},
    /* Mighty Arp's SOFT SAW: a reed-like buzz (operator 4's feedback through the chain), a breathy attack. (Its
     * vibrato and legato glide stay in Mighty Arp: here it plays chords and loops, a voice a note) */
    {"SOFT SAW", 2, 5, {{CARRIER(1, 0, 0.9f), OP(1, 0, 0.2f, 0, 0, 0.03f, 0.4f, 0.7f),
                         OP(3, 0, 0.1f, 0, 0, 0.03f, 0.3f, 0.6f), OP(1, 0, 0.12f, 0, 0, 0.05f, 0.4f, 0.6f)}, 0, 0},
     30, 70, 85, 40, 1.0f},
    /* TUNED DRONE: DARK DRONE with its modulators on whole ratios (1.5, 3), so it sits in the key (CHESS's kings: Nick,
     * 19 Nov 2026, the drones sounded out of tune) */
    {"TUNED DRONE", 4, 4, {{CARRIER(0.5f, 0, 0.7f), OP(1.5f, 0, 0.22f, 0, 0, 1.5f, 6.0f, 0.8f),
                           CARRIER(1, 4, 0.45f), OP(3.0f, 0, 0.18f, 0, 0, 2.5f, 6.0f, 0.7f)}, 0, 0},
     70, 90, 90, 85, 0.9f},
};

/* the main sound's knobs: everything a voice does not keep (the LFO, the filter, the effects) */
static void kp_defaults(eng_params_t *p)
{
    p->tone = 1.0f;
    p->strike = 1.0f;
    p->detune = 0.1f;
    p->velo = 0.55f;
    p->att = 0.001f, p->dec = 0.8f, p->sus = 0.3f, p->rel = 0.6f;
    p->alg = 4, p->fb = 0;
    p->wave = 0xFFu;
    p->ratio = 1.0f;
    p->lfo_hz = 4.0f;
    p->lfo_shape = LFO_SINE;
    p->vib = 0.0f, p->trem = 0.0f, p->bend = 0.0f;
    p->chorus = 0.25f, p->chorus_hz = 0.4f, p->chorus_depth = 60.0f;
    p->dly_samples = 0.375f * SR_F, p->dly_fb = 0.35f, p->dly_mix = 0.18f, p->dly_width = 0.45f;   /* (width
                                          * was 0.85: narrower, Nick, 19 Nov 2026) */
    p->rev_mix = 0.22f, p->rev_secs = 2.5f, p->rev_dark = 0.45f, p->shimmer = 0.0f;
    p->filter = 0, p->cut = 14.0f, p->res = 0.0f, p->env = 0.0f, p->lfo_cut = 0.0f;
    p->fatt = 0.0f, p->fdec = 0.5f, p->fsus = 1.0f, p->frel = 0.5f;
    p->keytrack = 0.0f;
    p->pwm = 0.0f;
    p->gain = 1.0f;
}
/* a sound's per-voice knobs (its envelope, algorithm, gain) over the main ones */
static void kp_sound(eng_params_t *p, const ksnd_t *k)
{
    p->alg = k->alg;
    p->fb = k->fb;
    p->att = knob_secs(k->a) * (k->a > 0);
    p->dec = knob_secs(k->d);
    p->sus = (float)k->su / 100.0f;
    p->rel = knob_secs(k->r);
    p->gain = k->gain;
    p->wave = 0xFFu;
    p->ratio = 1.0f;
    p->tone = 1.0f;
    p->strike = 1.0f;
}

/* ------------------------------------------------------- timed notes --- */
/* the UI's notes to the audio: each with the sample it plays at (k_now() + a little, for in time; 0: at once). A
 * sound (slot) is the app's: k_slot[n] holds its operators and its voice knobs */
#define K_NSLOT 12
typedef struct {
    eng_sound_t s;
    eng_params_t p;
} kslot_t;
static kslot_t k_slot[K_NSLOT];
static eng_params_t k_main;              /* the LFO, the filter, the effects: the UI sets them, the render reads them */
static uint8_t k_main_slot;              /* the slot whose sound eng_set gets (a note with no slot of its own) */
static void k_slot_set(uint32_t slot, const ksnd_t *k)   /* built aside, then handed to the audio in one locked step:
                                                         * the render (an interrupt) never plays a note on half of it
                                                         * (GPT's review, 18 Nov 2026) */
{
    kslot_t n, *s = &k_slot[slot % K_NSLOT];
    uint32_t f;
    n.s = k->s;
    n.p = k_main;
    kp_sound(&n.p, k);
    f = plat_lock();
    *s = n;
    plat_unlock(f);
}

/* a slot's release, at most secs: generated music piles up voices, and a voice taken while it still rings is a blip */
static void k_slot_rel(uint32_t slot, float secs)
{
    kslot_t *s = &k_slot[slot % K_NSLOT];
    if (s->p.rel > secs)
        s->p.rel = secs;
}

enum { KE_ON = 1, KE_OFF, KE_ALLOFF, KE_KILL };
typedef struct {
    uint32_t t;                          /* the sample it plays at (0: at once) */
    int32_t gate;                        /* samples before it releases; -1 held (a KE_OFF lets it go) */
    uint8_t kind, note, vel, slot;
} kev_t;
#define KQ 256u
static kev_t kq[KQ];
static volatile uint32_t kq_w, kq_r;     /* the UI writes, the render reads */
static volatile uint32_t k_clock;        /* samples rendered */
static uint32_t k_now(void) { return k_clock; }
static void k_push(uint32_t t, uint32_t kind, int32_t note, uint32_t vel, uint32_t slot, int32_t gate)
{
    uint32_t w = kq_w;
    if (w - kq_r >= KQ || note < 0 || note > 127)
        return;                          /* full: dropped */
    kq[w % KQ].t = t;
    kq[w % KQ].gate = gate;
    kq[w % KQ].kind = (uint8_t)kind;
    kq[w % KQ].note = (uint8_t)note;
    kq[w % KQ].vel = (uint8_t)clamp((int32_t)vel, 1, 127);
    kq[w % KQ].slot = (uint8_t)slot;
    kq_w = w + 1u;
}
/* the EFFECTS knob: every sound a game plays outside its music (a hit, a pick-up: at once, so off the beat) turned
 * down by it; the music (the groove, and whatever it plays) never. 0: the games play only their music */
static uint8_t k_fx = 100;               /* percent, for sounds outside the music */
static uint8_t k_in_music;               /* 1 while the groove schedules */
static void k_on(uint32_t t, int32_t note, uint32_t vel, uint32_t slot, float secs)   /* secs <= 0: held */
{
    if (!k_in_music && k_fx < 100u) {
        if (!k_fx)
            return;
        vel = vel * k_fx / 100u;
        if (!vel)
            vel = 1;
    }
    k_push(t, KE_ON, note, vel, slot, secs > 0.0f ? (int32_t)(secs * SR_F) : -1);
}
static void k_off(uint32_t t, int32_t note) { k_push(t, KE_OFF, note, 0, 0, 0); }
static uint32_t k_at(float secs_ahead) { return k_now() + (uint32_t)(secs_ahead * SR_F); }

/* the render's side: what came in, in time order, then each at its sample */
#define KP 96u
static kev_t kp[KP];
static uint32_t kpn;
static float k_l[ENG_BLOCK], k_r[ENG_BLOCK];
#define SCOPE_N 256u
static int16_t scope[SCOPE_N];
static volatile uint32_t scope_w;
static volatile float k_level;            /* the output's level lately (0..1), for the screen */
static void k_apply(const kev_t *e)
{
    if (e->kind == KE_ON) {
        kslot_t *s = &k_slot[e->slot % K_NSLOT];
        eng_note_on_with(&s->s, &s->p, e->note, e->vel, e->gate);
    } else if (e->kind == KE_OFF) {
        eng_note_off(e->note);
    } else if (e->kind == KE_ALLOFF) {
        eng_release_all();
    } else if (e->kind == KE_KILL) {
        eng_silence();
    }
}
static void k_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, j, seg = 0, now = k_clock;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f, pk = 0.0f;
    if (n > ENG_BLOCK)
        n = ENG_BLOCK;
    while (kq_r != kq_w && kpn < KP) {   /* in, kept in time order (a 0 time: now) */
        kev_t e = kq[kq_r % KQ];
        kq_r = kq_r + 1u;
        if (e.kind == KE_KILL)           /* a kill: the notes still waiting go too (a game left: none of its notes
                                          * after it, on the next game's sounds) */
            kpn = 0;
        if (e.t == 0u || (int32_t)(e.t - now) < 0)
            e.t = now;
        for (i = kpn; i > 0 && (int32_t)(kp[i - 1].t - e.t) > 0; i--)
            kp[i] = kp[i - 1];
        kp[i] = e;
        kpn++;
    }
    eng_set(&k_slot[k_main_slot].s, &k_main, n);
    for (i = 0; i < n; i++)
        k_l[i] = k_r[i] = 0.0f;
    for (i = 0; i < kpn && (int32_t)(kp[i].t - (now + n)) < 0; i++) {
        uint32_t off = (uint32_t)(kp[i].t - now);
        if (off > seg) {
            eng_voices(k_l, k_r, seg, off);
            seg = off;
        }
        k_apply(&kp[i]);
    }
    for (j = 0; i < kpn; i++, j++)       /* the rest wait */
        kp[j] = kp[i];
    kpn = j;
    eng_voices(k_l, k_r, seg, n);
    eng_out(k_l, k_r, lr, n, gain);
    for (i = 0, j = scope_w; i < n; i++, j++) {
        int32_t v = (lr[2 * i] + lr[2 * i + 1]) >> 9;
        float a = (float)(v < 0 ? -v : v) * (1.0f / 32768.0f);
        scope[j & (SCOPE_N - 1u)] = (int16_t)clamp(v, -32767, 32767);
        if (a > pk)
            pk = a;
    }
    scope_w = j;
    k_level = pk > k_level ? pk : k_level * 0.93f;
    k_clock = now + n;
}
static void k_panic(void)
{
    eng_silence();
    kpn = 0;
    kq_r = kq_w;
}

/* the scope: the last samples as a line across w at x, y, h tall */
static void scope_draw(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    /* one snapshot a frame: the screen is drawn in two bands, and the scope straddles them; each band reading the
     * buffer afresh drew two moments of the sound, torn across the middle (Nick: "super choppy", 18 Nov 2026) */
    static int16_t snap[SCOPE_N];
    static uint32_t snap_frame = 0xFFFFFFFFu;
    static uint32_t z;
    uint32_t i;
    int32_t last = 0, k;
    if (snap_frame != k_frame) {
        uint32_t at, f;
        snap_frame = k_frame;
        f = plat_lock();
        at = scope_w - SCOPE_N;
        for (i = 0; i < SCOPE_N; i++)
            snap[i] = scope[(at + i) & (SCOPE_N - 1u)];
        plat_unlock(f);
        for (z = 0, i = 1; i < SCOPE_N / 2u; i++)   /* start at an upward zero crossing: the wave stands still */
            if (snap[i - 1u] < 0 && snap[i] >= 0) {
                z = i;
                break;
            }
    }
    for (k = 0; k < w; k++) {
        int32_t v = snap[(z + (uint32_t)k * (SCOPE_N / 2u) / (uint32_t)w) & (SCOPE_N - 1u)];
        int32_t yy = y + h / 2 - v * (h / 2) / 2500;
        yy = clamp(yy, y, y + h - 1);
        if (k)
            cv_line(x + k - 1, last, x + k, yy, c);
        last = yy;
    }
}

/* a small random generator (xorshift32), seeded */
static uint32_t kr_s = 0x9E3779B9u;
static uint32_t kr(void)
{
    kr_s ^= kr_s << 13;
    kr_s ^= kr_s >> 17;
    kr_s ^= kr_s << 5;
    return kr_s;
}
static int32_t kr_in(int32_t lo, int32_t hi) { return lo + (int32_t)(kr() % (uint32_t)(hi - lo + 1)); }
static int kr_pc(uint32_t pc) { return kr() % 100u < pc; }
static float kr_f(void) { return (float)(kr() >> 8) * (1.0f / 16777216.0f); }
/* what a hit says (a life lost, a hit taken): one at random, never the same twice running (Nick, 19 Nov 2026: more
 * than OUCH, and funny) */
static const char *oops_word(void)
{
    static const char *const W[] = {"OUCH!", "HECK!", "SHOOT!", "POOP!", "DANG!",  "BRUH!",  "NOPE!", "SHEESH!",
                                    "RAGE!", "WHY!",  "OOF!",   "YIKES!", "WHOOPS!", "BONK!", "RATS!", "UGH!",
                                    "NOOO!", "GAH!",  "WELP!",  "DARN!",  "CRUD!",  "ARGH!",  "BLAST!", "YOWCH!"};
    static uint32_t last = 99u;
    uint32_t k;
    do
        k = kr() % (uint32_t)(sizeof W / sizeof W[0]);
    while (k == last);
    last = k;
    return W[k];
}

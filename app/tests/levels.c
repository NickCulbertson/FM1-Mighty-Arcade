/* Progression and collision regression checks. Run app/tests/levels.sh; outputs stay in /tmp. */
#define main scripted_host_main
#include "../../base/host/plat_host.c"
#undef main
#include <assert.h>

static void step_dodge(void)
{
    now_ms += 16;
    k_clock += 706;
    bx_tick();
}
static void picture(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    juice_calm();
    ui.msg_until = 0;
    screen_draw(scene, C_BG);
    png_write(path);
}
/* All four possible exits must be reachable by the full-size player, not just by a point. */
static void check_routes(void)
{
    uint8_t seen[101][101] = {{0}};
    int qx[10201], qy[10201], read = 0, write = 1;
    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    qx[0] = 50; qy[0] = 44; seen[50][44] = 1; /* (120,128) */
    while (read < write) {
        int x = qx[read], y = qy[read++];
        for (int d = 0; d < 4; d++) {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            float px = 20 + nx * 2, py = 40 + ny * 2;
            if (nx < 3 || nx > 97 || ny < 1 || ny > 87 || seen[nx][ny] || !bx_clear_spot(px,py,BX_ME + 1))
                continue;
            seen[nx][ny] = 1; qx[write] = nx; qy[write++] = ny;
        }
    }
    assert(seen[10][10] && seen[90][10] && seen[10][78] && seen[90][78]);
}
int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "/tmp";
    boot();
    enter(M_BOUNCE);
    assert(bbox.level == 0 && bbox.nballs == 1 && !bbox.exit_on && !bx_obstacles());
    bbox.safe_until = now_ms + 60000;
    run_ms(11500);
    assert(bbox.nballs == 2 && !bbox.exit_on);
    run_ms(10500);
    assert(bbox.nballs == 3 && bbox.exit_on);  /* real scheduler, not a forced spawn */
    picture(out, "dodge-exit");
    bx_hurt(0);
    assert(bbox.exit_on && bbox.level == 0 && bbox.nballs == 1);
    uint32_t score = bbox.score;
    bbox.px = bbox.ex + 16; bbox.py = bbox.ey; bbox.pvx = -0.18f; bbox.pvy = 0;
    ui.keys = 0;
    for (int i = 0; i < 8 && bbox.level == 0; i++) step_dodge();
    assert(bbox.level == 1 && bbox.score >= score + 500 && bbox.nballs == 1 && !bbox.exit_on);
    assert(bbox.bars == 0 && !bbox.spawn_at && !bbox.pvx && !bbox.pvy && bx_obstacles() == 2);
    picture(out, "dodge-level2");
    /* Real physics against the first bumper: enemies reflect, the rolling player cannot pass through. */
    bbox.safe_until = now_ms + 60000;
    bbox.b[0].x = 59; bbox.b[0].y = 100; bbox.b[0].vx = bx_speed(); bbox.b[0].vy = 0;
    step_dodge();
    assert(bbox.b[0].vx < 0 && bx_clear_spot(bbox.b[0].x,bbox.b[0].y,BX_R - 0.01f));
    bbox.px = 52; bbox.py = 100; bbox.pvx = 0.2f; bbox.pvy = 0;
    ui.keys = 1u << PAD_R;
    for (int i = 0; i < 60; i++) step_dodge();
    assert(bbox.px <= 54.1f && bx_clear_spot(bbox.px,bbox.py,BX_ME - 0.01f));
    ui.keys = 0;
    for (int level = 0; level < 12; level++) {
        bbox.level = (uint16_t)level; bx_start_level();
        assert(bx_clear_spot(120,128,BX_ME + 1)); check_routes();
        bbox.safe_until = now_ms + 100000;
        for (int i = 0; i < 200; i++) {
            bbox.star_on = 0; bbox.star_at = 0; step_dodge();
            assert(bx_clear_spot(bbox.sx,bbox.sy,BX_ME + 4));
        }
    }
    bbox.level = 4; bx_start_level(); picture(out, "dodge-level5");
    enter(M_MENU); enter(M_BOUNCE);
    assert(bbox.level == 0 && bbox.score == 0 && !bbox.exit_on);

    enter(M_DROP); dp_set_board(-100);
    int last_np = 0;
    for (int board = 0; board < DP_NBOARD; board++) {
        dp_set_board(board - ndrop.board);
        assert(ndrop.board == board && ndrop.np >= last_np && ndrop.np <= DP_NP);
        last_np = ndrop.np;
        if (board == 0) {assert(ndrop.np == 2); picture(out, "drop-level1");}
        if (board == DP_NBOARD - 1) picture(out, "drop-level12");
        /* Sweep the playable keys and adjust the actual platform controls until every floor block is lit. */
        uint32_t aim_seed = 12345u + (uint32_t)board;
        for (int frame = 0; frame < 10000 && !ndrop.cleared; frame++) {
            if (frame % 16 == 0) dp_drop((uint32_t)(frame / 16) % NKEYS);
            if (frame % 432 == 0 && frame) {
                for (int knob = 0; knob < 4; knob++) {
                    aim_seed = aim_seed * 1664525u + 1013904223u;
                    int target = (int)((aim_seed >> 16) % 21u) - 10;
                    dp_tilt(knob, target - ndrop.tilt[knob]);
                }
            }
            now_ms += 16; k_clock += 706; dp_tick();
        }
        if (!ndrop.cleared) fprintf(stderr,"board %d: only %d floor blocks reached\n",board + 1,dp_floor_n());
        assert(ndrop.cleared && dp_floor_n() == 8);
        uint32_t saved_score = ndrop.score;
        now_ms += 2500; dp_tick();
        assert(ndrop.board == (board + 1) % DP_NBOARD && !ndrop.cleared && dp_floor_n() == 0);
        assert(ndrop.score == saved_score);
    }
    puts("PASS: exit timing, rolling into exit, hit recovery, obstacle collisions/routes, stars, all 12 Drop clears and wraparound");
    return 0;
}

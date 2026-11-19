/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Nick Culbertson */
/* MIGHTY ARCADE: a prototype on fm1-base, from Mighty Arp's stack. This header is the app's half of the contract with the
 * platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MIGHTY ARCADE"
#define APP_SPLASH_RGB RGB(95, 227, 238)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_OVERLOAD                     /* the render ran late: app_overload sheds voices */
#define APP_SPLASH                       /* the boot screen is app_splash's: black, the menu's title (Nick, 19 Nov 2026) */

/* the store: the app's settings (DICE SYNTH's kept sounds) and the panel calibration (the platform's), and their flash
 * offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_overload(void);
void app_splash(void);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);

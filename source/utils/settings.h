/*
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  settings.h
 * @brief Optional loader settings read from ux0:data/earthandlegend/config.txt.
 */

#ifndef SOLOADER_SETTINGS_H
#define SOLOADER_SETTINGS_H

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Enable the native OpenAL/sceAudioOut audio backend (1) or keep the game
 * silent (0). */
extern int setting_openal;

/* Allow mipmap sampling (1) or force the base mip level (0). */
extern int setting_mipmaps;

/* Decode DXT textures to RGBA on the CPU (1) instead of uploading them as
 * native GXM UBC textures (0). */
extern int setting_dxtDecode;

/* Decoded texture format when setting_dxtDecode is 1:
 * 0 = RGBA8888 (best quality, 4 bytes/texel)
 * 1 = RGB565 / RGBA4444 (half the memory) */
extern int setting_dxtFormat;

/* Force the synchronous engine path (1). 0 uses the engine's own async
 * renderer, which is faster. */
extern int setting_v3Sync;

/* Number of CPU cores reported to the engine. 0 = auto (3 user cores),
 * 1 disables the engine's worker threads. */
extern int setting_cpuCount;

/* Cache blocks of S3DMain.stk in RAM to avoid slow small reads. */
extern int setting_packCache;

/* 1 = wait for vblank when presenting, 0 = present immediately. */
extern int setting_vsync;

/* Right-stick camera drag: anchor point and pixels per 1/60 s at full tilt. */
extern int setting_cameraAnchorX;
extern int setting_cameraAnchorY;
extern int setting_cameraSpeed;

/* START / SELECT tap coordinates on the game's 960x544 HUD:
 * top-left portrait = inventory, bottom-left globe = map. */
extern int setting_startTapX;
extern int setting_startTapY;
extern int setting_selectTapX;
extern int setting_selectTapY;

void settings_load(void);
void settings_save(void);
void settings_reset(void);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_SETTINGS_H

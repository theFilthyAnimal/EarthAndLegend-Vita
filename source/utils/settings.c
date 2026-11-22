/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "utils/trace.h"
#include "settings.h"

#define CONFIG_FILE_PATH DATA_PATH "config.txt"

int setting_openal = 1;
int setting_mipmaps = 0;
int setting_dxtDecode = 1;
int setting_dxtFormat = 1;
int setting_v3Sync = 0;
int setting_cpuCount = 0;
int setting_packCache = 1;
int setting_vsync = 1;
int setting_cameraAnchorX = 720;
int setting_cameraAnchorY = 272;
int setting_cameraSpeed = 6;
int setting_startTapX = 50;
int setting_startTapY = 28;
int setting_selectTapX = 50;
int setting_selectTapY = 485;

void settings_reset(void) {
    setting_openal = 1;
    setting_mipmaps = 0;
    setting_dxtDecode = 1;
    setting_dxtFormat = 1;
    setting_v3Sync = 0;
    setting_cpuCount = 0;
    setting_packCache = 1;
    setting_vsync = 1;
    setting_cameraAnchorX = 720;
    setting_cameraAnchorY = 272;
    setting_cameraSpeed = 6;
    setting_startTapX = 50;
    setting_startTapY = 28;
    setting_selectTapX = 50;
    setting_selectTapY = 485;
}

void settings_load(void) {
    settings_reset();

    char line[128];
    char key[64];
    int value;

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (config) {
        while (fgets(line, sizeof(line), config)) {
            if (sscanf(line, "%63s %d", key, &value) != 2)
                continue;

            if (strcmp("setting_openal", key) == 0)
                setting_openal = value;
            else if (strcmp("setting_mipmaps", key) == 0)
                setting_mipmaps = value;
            else if (strcmp("setting_dxtDecode", key) == 0)
                setting_dxtDecode = value;
            else if (strcmp("setting_dxtFormat", key) == 0)
                setting_dxtFormat = value;
            else if (strcmp("setting_v3Sync", key) == 0)
                setting_v3Sync = value;
            else if (strcmp("setting_cpuCount", key) == 0)
                setting_cpuCount = value;
            else if (strcmp("setting_packCache", key) == 0)
                setting_packCache = value;
            else if (strcmp("setting_vsync", key) == 0)
                setting_vsync = value;
            else if (strcmp("setting_cameraAnchorX", key) == 0)
                setting_cameraAnchorX = value;
            else if (strcmp("setting_cameraAnchorY", key) == 0)
                setting_cameraAnchorY = value;
            else if (strcmp("setting_cameraSpeed", key) == 0)
                setting_cameraSpeed = value;
            else if (strcmp("setting_startTapX", key) == 0)
                setting_startTapX = value;
            else if (strcmp("setting_startTapY", key) == 0)
                setting_startTapY = value;
            else if (strcmp("setting_selectTapX", key) == 0)
                setting_selectTapX = value;
            else if (strcmp("setting_selectTapY", key) == 0)
                setting_selectTapY = value;
        }
        fclose(config);
    }

    /* Sanitize values read from a hand-edited file. */
    if (setting_openal < 0 || setting_openal > 1)
        setting_openal = 1;
    if (setting_mipmaps < 0 || setting_mipmaps > 1)
        setting_mipmaps = 0;
    if (setting_dxtDecode < 0 || setting_dxtDecode > 1)
        setting_dxtDecode = 1;
    if (setting_dxtFormat < 0 || setting_dxtFormat > 1)
        setting_dxtFormat = 1;
    if (setting_v3Sync < 0 || setting_v3Sync > 1)
        setting_v3Sync = 0;
    if (setting_cpuCount < 0)
        setting_cpuCount = 0;
    if (setting_cpuCount > 3)
        setting_cpuCount = 3;
    if (setting_packCache < 0 || setting_packCache > 1)
        setting_packCache = 1;
    if (setting_vsync < 0 || setting_vsync > 1)
        setting_vsync = 1;
    if (setting_cameraAnchorX < 0)   setting_cameraAnchorX = 0;
    if (setting_cameraAnchorX > 960) setting_cameraAnchorX = 960;
    if (setting_cameraAnchorY < 0)   setting_cameraAnchorY = 0;
    if (setting_cameraAnchorY > 544) setting_cameraAnchorY = 544;
    if (setting_cameraSpeed < 1)  setting_cameraSpeed = 1;
    if (setting_cameraSpeed > 64) setting_cameraSpeed = 64;
    if (setting_startTapX < 0)   setting_startTapX = 0;
    if (setting_startTapX > 960) setting_startTapX = 960;
    if (setting_startTapY < 0)   setting_startTapY = 0;
    if (setting_startTapY > 544) setting_startTapY = 544;
    if (setting_selectTapX < 0)   setting_selectTapX = 0;
    if (setting_selectTapX > 960) setting_selectTapX = 960;
    if (setting_selectTapY < 0)   setting_selectTapY = 0;
    if (setting_selectTapY > 544) setting_selectTapY = 544;

    eal_trace("settings: openal=%d mip=%d dxt=%d dxt_fmt=%d v3sync=%d "
              "cpu=%d pack_cache=%d vsync=%d camera=(%d,%d,%d) "
              "start_tap=(%d,%d) select_tap=(%d,%d)",
              setting_openal, setting_mipmaps, setting_dxtDecode,
              setting_dxtFormat, setting_v3Sync, setting_cpuCount,
              setting_packCache, setting_vsync,
              setting_cameraAnchorX, setting_cameraAnchorY,
              setting_cameraSpeed, setting_startTapX, setting_startTapY,
              setting_selectTapX, setting_selectTapY);
}

void settings_save(void) {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "%s %d\n", "setting_openal", setting_openal);
        fprintf(config, "%s %d\n", "setting_mipmaps", setting_mipmaps);
        fprintf(config, "%s %d\n", "setting_dxtDecode", setting_dxtDecode);
        fprintf(config, "%s %d\n", "setting_dxtFormat", setting_dxtFormat);
        fprintf(config, "%s %d\n", "setting_v3Sync", setting_v3Sync);
        fprintf(config, "%s %d\n", "setting_cpuCount", setting_cpuCount);
        fprintf(config, "%s %d\n", "setting_packCache", setting_packCache);
        fprintf(config, "%s %d\n", "setting_vsync", setting_vsync);
        fprintf(config, "%s %d\n", "setting_cameraAnchorX", setting_cameraAnchorX);
        fprintf(config, "%s %d\n", "setting_cameraAnchorY", setting_cameraAnchorY);
        fprintf(config, "%s %d\n", "setting_cameraSpeed", setting_cameraSpeed);
        fprintf(config, "%s %d\n", "setting_startTapX", setting_startTapX);
        fprintf(config, "%s %d\n", "setting_startTapY", setting_startTapY);
        fprintf(config, "%s %d\n", "setting_selectTapX", setting_selectTapX);
        fprintf(config, "%s %d\n", "setting_selectTapY", setting_selectTapY);
        fclose(config);
    }
}

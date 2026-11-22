#!/usr/bin/env python3
"""Prepare the game data directory for the Earth And Legend PS Vita port.

Usage:
    python tools/prepare_data.py "Earth+And+Legend+[3.0.0].apk" [output-dir]

The script extracts the ShiVa3D engine library and the main data pack from the
3.0.0 Android APK and lays them out the way the port expects:

    <output-dir>/
    |-- libS3DClient.so
    |-- S3DMain.stk
    |-- config.txt
    |-- cache/
    |-- home/
    `-- fonts/

Copy the contents of <output-dir> to ux0:data/earthandlegend/ on the Vita.
"""

import argparse
import os
import shutil
import sys
import zipfile

ENGINE_CANDIDATES = (
    "lib/armeabi-v7a/libS3DClient.so",
    "lib/armeabi/libS3DClient.so",
)

PACK_SOURCE = "assets/S3DMain.smf"
PACK_TARGET = "S3DMain.stk"

DEFAULT_CONFIG = """\
# Earth And Legend (PS Vita) configuration
# Optional: copy this file to ux0:data/earthandlegend/config.txt.
# The values below are the defaults; the game works without this file.

# Native audio backend (OpenAL -> sceAudioOut):
#   1 - music and sound effects (default)
#   0 - completely silent
setting_openal 1

# Allow mipmap sampling (1) or force the base mip level (0).
# Keep 0 if UI textures show pixel artifacts.
setting_mipmaps 0

# Decode DXT textures to RGBA on the CPU (1) or upload them as native GXM
# UBC textures (0). 1 fixes block artifacts on this engine build.
setting_dxtDecode 1

# Decoded texture format when setting_dxtDecode is 1:
#   0 - RGBA8888 (best quality, 4 bytes per texel)
#   1 - RGB565 / RGBA4444 (half the memory, default)
setting_dxtFormat 1

# 3.0.0 uses the engine's asynchronous renderer by default.
#   0 - asynchronous renderer (faster, default)
#   1 - force the synchronous path (slower, for debugging)
setting_v3Sync 0

# Number of CPU cores reported to the engine:
#   0 - auto (3 user cores, default)
#   1 - single-threaded (use if multi-threading is unstable)
#   2..3 - exact number
setting_cpuCount 0

# Keep a read cache for S3DMain.stk in RAM (1, default) or read the file
# directly (0). The pack is read through very small calls, which is slow on
# the Vita memory card.
setting_packCache 1

# 1 - wait for vblank (no tearing, default)
# 0 - present immediately (smoother frame times, may tear)
setting_vsync 1

# Right-stick camera drag: anchor point on the 960x544 screen and speed in
# pixels per 1/60 s at full tilt. The speed is scaled by the real frame time,
# so the camera turns at the same rate at any frame rate.
setting_cameraAnchorX 720
setting_cameraAnchorY 272
setting_cameraSpeed 6

# START / SELECT tap coordinates on the 960x544 HUD:
#   START  - top-left portrait (inventory)
#   SELECT - bottom-left globe (map)
# Both are plain taps: nothing else is changed while a screen is open.
setting_startTapX 50
setting_startTapY 28
setting_selectTapX 50
setting_selectTapY 485
"""


def find_engine(apk):
    names = set(apk.namelist())
    for candidate in ENGINE_CANDIDATES:
        if candidate in names:
            return candidate
    return None


def extract(apk, source, target):
    with apk.open(source) as src, open(target, "wb") as dst:
        shutil.copyfileobj(src, dst, length=1024 * 1024)


def main():
    parser = argparse.ArgumentParser(
        description="Extract Earth And Legend 3.0.0 data for the PS Vita port.")
    parser.add_argument("apk", help="path to the 3.0.0 game APK")
    parser.add_argument("output", nargs="?", default="data",
                        help="output directory (default: data)")
    args = parser.parse_args()

    if not os.path.isfile(args.apk):
        print("error: APK not found: %s" % args.apk, file=sys.stderr)
        return 1

    try:
        apk = zipfile.ZipFile(args.apk)
    except zipfile.BadZipFile:
        print("error: not a valid APK/ZIP file: %s" % args.apk,
              file=sys.stderr)
        return 1

    with apk:
        engine = find_engine(apk)
        if engine is None:
            print("error: libS3DClient.so not found in the APK",
                  file=sys.stderr)
            return 1

        if PACK_SOURCE not in apk.namelist():
            print("error: %s not found in the APK" % PACK_SOURCE,
                  file=sys.stderr)
            return 1

        os.makedirs(args.output, exist_ok=True)

        engine_target = os.path.join(args.output, "libS3DClient.so")
        pack_target = os.path.join(args.output, PACK_TARGET)
        config_target = os.path.join(args.output, "config.txt")

        print("extracting %s -> %s" % (engine, engine_target))
        extract(apk, engine, engine_target)

        print("extracting %s -> %s" % (PACK_SOURCE, pack_target))
        extract(apk, PACK_SOURCE, pack_target)

        for directory in ("cache", "home", "fonts"):
            os.makedirs(os.path.join(args.output, directory), exist_ok=True)

        if not os.path.exists(config_target):
            with open(config_target, "w", encoding="utf-8", newline="\n") as f:
                f.write(DEFAULT_CONFIG)
            print("created %s" % config_target)

    print()
    print("Done. Copy the contents of '%s' to ux0:data/earthandlegend/."
          % args.output)
    print("Optionally place .ttf fonts into '%s'."
          % os.path.join(args.output, "fonts"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

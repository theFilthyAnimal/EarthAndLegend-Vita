"""Convert LiveArea images to the format the Vita package installer requires.

The Vita only accepts 8-bit palette PNGs (color type 3) in a VPK. 24-bit RGB
or 32-bit RGBA images make the installation fail with 0x8010113D.

Usage: python tools/prepare_livearea.py
"""

import os
import struct
import sys

from PIL import Image

EXPECTED = {
    'icon0.png': (128, 128),
    'pic0.png': (960, 544),
    'startup.png': (280, 158),
    'bg0.png': (840, 500),
}


def convert(path):
    im = Image.open(path)
    if im.mode in ('RGBA', 'LA') or 'transparency' in im.info:
        # FASTOCTREE keeps per-pixel alpha through a tRNS chunk.
        out = im.convert('RGBA').quantize(colors=256, method=Image.FASTOCTREE)
    else:
        out = im.convert('RGB').quantize(
            colors=256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
    out.save(path, optimize=True)


def check(path):
    with open(path, 'rb') as f:
        head = f.read(33)
    if head[:8] != b'\x89PNG\r\n\x1a\n':
        return 'not a PNG'
    width, height = struct.unpack('>II', head[16:24])
    bit_depth, color_type = head[24], head[25]
    if color_type != 3 or bit_depth != 8:
        return 'color type %d / %d bit, need 8-bit palette' % (
            color_type, bit_depth)
    if (width, height) != EXPECTED[os.path.basename(path)]:
        return '%dx%d, need %dx%d' % ((width, height) +
                                      EXPECTED[os.path.basename(path)])
    return 'ok'


def main():
    base = os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), 'extras', 'livearea')
    failed = False
    for name in EXPECTED:
        path = os.path.join(base, name)
        if not os.path.exists(path):
            print('missing:', name)
            failed = True
            continue
        convert(path)
        status = check(path)
        print('%-12s %s' % (name, status))
        failed = failed or status != 'ok'
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())

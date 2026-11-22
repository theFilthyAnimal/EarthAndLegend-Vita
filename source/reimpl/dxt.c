#include "reimpl/dxt.h"

#include <string.h>

#define DXT1_RGB   0x83F0u
#define DXT1_RGBA  0x83F1u
#define DXT3_RGBA  0x83F2u
#define DXT5_RGBA  0x83F3u

typedef struct {
    uint8_t r, g, b, a;
} RGBA;

static void rgb565_to_rgba(uint16_t c, RGBA *out) {
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;

    out->r = (uint8_t) ((r << 3) | (r >> 2));
    out->g = (uint8_t) ((g << 2) | (g >> 4));
    out->b = (uint8_t) ((b << 3) | (b >> 2));
    out->a = 255;
}

static void decode_color_block(const uint8_t *in, int width, int height,
                               int bx, int by, int four_color, int alpha_mode,
                               uint8_t *out) {
    uint16_t c0 = (uint16_t) (in[0] | (in[1] << 8));
    uint16_t c1 = (uint16_t) (in[2] | (in[3] << 8));
    uint32_t bits = (uint32_t) in[4] | ((uint32_t) in[5] << 8) |
                    ((uint32_t) in[6] << 16) | ((uint32_t) in[7] << 24);

    RGBA colors[4];
    rgb565_to_rgba(c0, &colors[0]);
    rgb565_to_rgba(c1, &colors[1]);

    if (four_color || c0 > c1) {
        colors[2].r = (uint8_t) ((2 * colors[0].r + colors[1].r) / 3);
        colors[2].g = (uint8_t) ((2 * colors[0].g + colors[1].g) / 3);
        colors[2].b = (uint8_t) ((2 * colors[0].b + colors[1].b) / 3);
        colors[2].a = 255;

        colors[3].r = (uint8_t) ((colors[0].r + 2 * colors[1].r) / 3);
        colors[3].g = (uint8_t) ((colors[0].g + 2 * colors[1].g) / 3);
        colors[3].b = (uint8_t) ((colors[0].b + 2 * colors[1].b) / 3);
        colors[3].a = 255;
    } else {
        colors[2].r = (uint8_t) ((colors[0].r + colors[1].r) / 2);
        colors[2].g = (uint8_t) ((colors[0].g + colors[1].g) / 2);
        colors[2].b = (uint8_t) ((colors[0].b + colors[1].b) / 2);
        colors[2].a = 255;

        colors[3].r = 0;
        colors[3].g = 0;
        colors[3].b = 0;
        colors[3].a = alpha_mode ? 0 : 255;
    }

    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int idx = (int) ((bits >> (2 * (y * 4 + x))) & 3);
            RGBA c = colors[idx];
            uint8_t *dst = out + ((size_t) py * width + px) * 4;
            dst[0] = c.r;
            dst[1] = c.g;
            dst[2] = c.b;
            dst[3] = c.a;
        }
    }
}

static void decode_alpha_dxt3(const uint8_t *in, int width, int height,
                              int bx, int by, uint8_t *out) {
    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        uint16_t row = (uint16_t) (in[y * 2] | (in[y * 2 + 1] << 8));
        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int a = (row >> (4 * x)) & 0xF;
            out[((size_t) py * width + px) * 4 + 3] = (uint8_t) (a * 17);
        }
    }
}

static void decode_alpha_dxt5(const uint8_t *in, int width, int height,
                              int bx, int by, uint8_t *out) {
    int a0 = in[0];
    int a1 = in[1];

    uint8_t alphas[8];
    alphas[0] = (uint8_t) a0;
    alphas[1] = (uint8_t) a1;

    if (a0 > a1) {
        for (int i = 1; i <= 6; i++)
            alphas[i + 1] = (uint8_t) (((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; i++)
            alphas[i + 1] = (uint8_t) (((5 - i) * a0 + i * a1) / 5);
        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t bits = 0;
    for (int i = 0; i < 6; i++)
        bits |= (uint64_t) in[2 + i] << (8 * i);

    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int idx = (int) ((bits >> (3 * (y * 4 + x))) & 7);
            out[((size_t) py * width + px) * 4 + 3] = alphas[idx];
        }
    }
}

void eal_dxt_decode(uint32_t format, int width, int height,
                    const void *data, uint8_t *out) {
    if (!data || !out || width <= 0 || height <= 0)
        return;

    memset(out, 0, (size_t) width * height * 4);

    const uint8_t *src = (const uint8_t *) data;
    int is_dxt1 = (format == DXT1_RGB || format == DXT1_RGBA);
    int is_dxt3 = (format == DXT3_RGBA);
    int is_dxt5 = (format == DXT5_RGBA);

    if (!is_dxt1 && !is_dxt3 && !is_dxt5)
        return;

    int block_size = is_dxt1 ? 8 : 16;
    int four_color = (is_dxt3 || is_dxt5);
    int alpha_mode = (format == DXT1_RGBA);

    for (int by = 0; by < height; by += 4) {
        for (int bx = 0; bx < width; bx += 4) {
            const uint8_t *block = src;

            // Color first: DXT3/DXT5 alpha is decoded afterwards so that the
            // color pass does not overwrite the alpha channel.
            decode_color_block(block + (is_dxt1 ? 0 : 8),
                               width, height, bx, by,
                               four_color, alpha_mode, out);

            if (is_dxt3)
                decode_alpha_dxt3(block, width, height, bx, by, out);
            else if (is_dxt5)
                decode_alpha_dxt5(block, width, height, bx, by, out);

            src += block_size;
        }
    }
}

/*
 * 16-bit variants: DXT1 RGB -> RGB565, everything else -> RGBA4444.
 * Halves the texture memory compared to RGBA8888.
 */

static void decode_color_block16(const uint8_t *in, int width, int height,
                                 int bx, int by, int four_color, int alpha_mode,
                                 uint16_t *out, int rgba4444) {
    uint16_t c0 = (uint16_t) (in[0] | (in[1] << 8));
    uint16_t c1 = (uint16_t) (in[2] | (in[3] << 8));
    uint32_t bits = (uint32_t) in[4] | ((uint32_t) in[5] << 8) |
                    ((uint32_t) in[6] << 16) | ((uint32_t) in[7] << 24);

    RGBA colors[4];
    rgb565_to_rgba(c0, &colors[0]);
    rgb565_to_rgba(c1, &colors[1]);

    if (four_color || c0 > c1) {
        colors[2].r = (uint8_t) ((2 * colors[0].r + colors[1].r) / 3);
        colors[2].g = (uint8_t) ((2 * colors[0].g + colors[1].g) / 3);
        colors[2].b = (uint8_t) ((2 * colors[0].b + colors[1].b) / 3);
        colors[2].a = 255;

        colors[3].r = (uint8_t) ((colors[0].r + 2 * colors[1].r) / 3);
        colors[3].g = (uint8_t) ((colors[0].g + 2 * colors[1].g) / 3);
        colors[3].b = (uint8_t) ((colors[0].b + 2 * colors[1].b) / 3);
        colors[3].a = 255;
    } else {
        colors[2].r = (uint8_t) ((colors[0].r + colors[1].r) / 2);
        colors[2].g = (uint8_t) ((colors[0].g + colors[1].g) / 2);
        colors[2].b = (uint8_t) ((colors[0].b + colors[1].b) / 2);
        colors[2].a = 255;

        colors[3].r = 0;
        colors[3].g = 0;
        colors[3].b = 0;
        colors[3].a = alpha_mode ? 0 : 255;
    }

    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int idx = (int) ((bits >> (2 * (y * 4 + x))) & 3);
            RGBA c = colors[idx];
            uint16_t v;

            if (rgba4444) {
                v = (uint16_t) (((c.r >> 4) << 12) |
                                ((c.g >> 4) << 8) |
                                ((c.b >> 4) << 4) |
                                (c.a >> 4));
            } else {
                v = (uint16_t) (((c.r >> 3) << 11) |
                                ((c.g >> 2) << 5) |
                                (c.b >> 3));
            }

            out[(size_t) py * width + px] = v;
        }
    }
}

static void decode_alpha_dxt3_16(const uint8_t *in, int width, int height,
                                 int bx, int by, uint16_t *out) {
    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        uint16_t row = (uint16_t) (in[y * 2] | (in[y * 2 + 1] << 8));
        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int a = (row >> (4 * x)) & 0xF;
            uint16_t *p = &out[(size_t) py * width + px];
            *p = (uint16_t) ((*p & 0xFFF0) | a);
        }
    }
}

static void decode_alpha_dxt5_16(const uint8_t *in, int width, int height,
                                 int bx, int by, uint16_t *out) {
    int a0 = in[0];
    int a1 = in[1];

    uint8_t alphas[8];
    alphas[0] = (uint8_t) a0;
    alphas[1] = (uint8_t) a1;

    if (a0 > a1) {
        for (int i = 1; i <= 6; i++)
            alphas[i + 1] = (uint8_t) (((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; i++)
            alphas[i + 1] = (uint8_t) (((5 - i) * a0 + i * a1) / 5);
        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t bits = 0;
    for (int i = 0; i < 6; i++)
        bits |= (uint64_t) in[2 + i] << (8 * i);

    for (int y = 0; y < 4; y++) {
        int py = by + y;
        if (py >= height)
            break;

        for (int x = 0; x < 4; x++) {
            int px = bx + x;
            if (px >= width)
                break;

            int idx = (int) ((bits >> (3 * (y * 4 + x))) & 7);
            int a = alphas[idx] >> 4;
            uint16_t *p = &out[(size_t) py * width + px];
            *p = (uint16_t) ((*p & 0xFFF0) | a);
        }
    }
}

void eal_dxt_decode16(uint32_t format, int width, int height,
                      const void *data, uint16_t *out, int rgba4444) {
    if (!data || !out || width <= 0 || height <= 0)
        return;

    memset(out, 0, (size_t) width * height * sizeof(uint16_t));

    const uint8_t *src = (const uint8_t *) data;
    int is_dxt1 = (format == DXT1_RGB || format == DXT1_RGBA);
    int is_dxt3 = (format == DXT3_RGBA);
    int is_dxt5 = (format == DXT5_RGBA);

    if (!is_dxt1 && !is_dxt3 && !is_dxt5)
        return;

    int block_size = is_dxt1 ? 8 : 16;
    int four_color = (is_dxt3 || is_dxt5);
    int alpha_mode = (format == DXT1_RGBA);

    for (int by = 0; by < height; by += 4) {
        for (int bx = 0; bx < width; bx += 4) {
            const uint8_t *block = src;

            decode_color_block16(block + (is_dxt1 ? 0 : 8),
                                 width, height, bx, by,
                                 four_color, alpha_mode, out, rgba4444);

            if (rgba4444) {
                if (is_dxt3)
                    decode_alpha_dxt3_16(block, width, height, bx, by, out);
                else if (is_dxt5)
                    decode_alpha_dxt5_16(block, width, height, bx, by, out);
            }

            src += block_size;
        }
    }
}

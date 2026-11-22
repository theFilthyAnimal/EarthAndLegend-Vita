/*
 * Earth And Legend (PS Vita) - small DXT1/DXT3/DXT5 decoder.
 *
 * The game stores its 3D textures as S3TC DXT data. On this vitaGL build the
 * native GXM UBC upload path produces severe block artifacts, so the port
 * can decode DXT to RGBA8888 and upload an uncompressed texture instead.
 */

#ifndef EAL_DXT_H
#define EAL_DXT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decode a DXT1/DXT3/DXT5 image to tightly packed RGBA8888.
 *
 * @param format  GL internal format (0x83F0..0x83F3)
 * @param width   image width in pixels
 * @param height  image height in pixels
 * @param data    compressed source data
 * @param out     destination buffer, width * height * 4 bytes
 */
void eal_dxt_decode(uint32_t format, int width, int height,
                    const void *data, uint8_t *out);

/**
 * Decode a DXT1/DXT3/DXT5 image to 16-bit texels.
 * DXT1 RGB is packed as RGB565, everything else as RGBA4444.
 *
 * @param format    GL internal format (0x83F0..0x83F3)
 * @param width     image width in pixels
 * @param height    image height in pixels
 * @param data      compressed source data
 * @param out       destination buffer, width * height * 2 bytes
 * @param rgba4444  1 to pack RGBA4444, 0 to pack RGB565
 */
void eal_dxt_decode16(uint32_t format, int width, int height,
                      const void *data, uint16_t *out, int rgba4444);

#ifdef __cplusplus
}
#endif

#endif // EAL_DXT_H

#ifndef ICO_VITA_BLEND_REFERENCE_H
#define ICO_VITA_BLEND_REFERENCE_H

#include <stdint.h>

/*
 * CPU reference for the PS2 GS blend channel:
 *   out = ((Cs - Cd) * As >> 7) + Cd
 * with either saturation or modulo-256 wrapping. For negative products,
 * the shift is floor division, not C's truncation-toward-zero division.
 *
 * This is a test oracle for the vitaGL RGBA8_UNORM fallback, not the
 * production GPU renderer.
 */
static inline uint8_t vita_blend_reference_channel(uint8_t source,
                                                   uint8_t destination,
                                                   uint8_t alpha,
                                                   int clamp_mode)
{
    const int product = ((int)source - (int)destination) * (int)alpha;
    int scaled = product / 128;

    if (product < 0 && product % 128 != 0) {
        --scaled;
    }

    int value = scaled + (int)destination;
    if (clamp_mode) {
        if (value < 0) {
            value = 0;
        } else if (value > 255) {
            value = 255;
        }
    } else {
        value %= 256;
        if (value < 0) {
            value += 256;
        }
    }

    return (uint8_t)value;
}

#endif

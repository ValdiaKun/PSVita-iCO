#include "../../../shaders/test/hlsl_shim.h"
#include "../../../shaders/gs_math.hlsli"
#include "../vita_blend_reference.h"

#include <stdio.h>

typedef struct BlendVector {
    uint8_t source;
    uint8_t destination;
    uint8_t alpha;
    int clamp_mode;
    uint8_t expected;
    const char *name;
} BlendVector;

int main(void)
{
    static const BlendVector vectors[] = {
        {200, 100, 64, 1, 150, "clamped blend"},
        {100, 80, 64, 1, 90, "clamped green"},
        {50, 20, 64, 1, 35, "clamped blue"},
        {0, 255, 255, 0, 2, "wrapped negative blend"},
        {20, 80, 255, 0, 216, "wrapped negative green"},
        {40, 200, 255, 0, 137, "wrapped negative blue"},
        {255, 0, 255, 1, 255, "clamped overflow"},
        {255, 0, 255, 0, 252, "wrapped positive overflow"},
        {0, 0, 0, 0, 0, "zero source"},
        {255, 255, 255, 0, 255, "identical source and destination"},
    };

    int failures = 0;
    for (unsigned int i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        const BlendVector *v = &vectors[i];
        const uint8_t actual = vita_blend_reference_channel(
            v->source, v->destination, v->alpha, v->clamp_mode);
        if (actual != v->expected) {
            fprintf(stderr, "FAIL %s: expected %u, got %u\n",
                    v->name, (unsigned int)v->expected, (unsigned int)actual);
            ++failures;
        }
    }

    if (failures != 0) {
        fprintf(stderr, "%d vitaGL blend reference vector(s) failed\n", failures);
        return 1;
    }

    /*
     * Compare the vitaGL CPU oracle against ICO's canonical GS arithmetic for
     * every 8-bit source, destination and alpha value in both COLCLAMP modes.
     * This prevents a test-vector correction from drifting away from the
     * renderer's established integer semantics.
     */
    unsigned long checks = 0;
    for (unsigned int clamp_mode = 0; clamp_mode < 2; ++clamp_mode) {
        for (int source = 0; source < 256; ++source) {
            for (int destination = 0; destination < 256; ++destination) {
                for (int alpha = 0; alpha < 256; ++alpha) {
                    const int expected = gs_blend_ch(source, destination, alpha,
                                                     destination, clamp_mode);
                    const uint8_t actual = vita_blend_reference_channel(
                        (uint8_t)source, (uint8_t)destination, (uint8_t)alpha,
                        (int)clamp_mode);
                    if ((int)actual != expected) {
                        fprintf(stderr,
                                "FAIL GS parity: src=%d dst=%d alpha=%d clamp=%u "
                                "expected=%d got=%u\n",
                                source, destination, alpha, clamp_mode, expected,
                                (unsigned int)actual);
                        return 1;
                    }
                    ++checks;
                }
            }
        }
    }

    printf("%zu fixed vectors and %lu canonical GS blend comparisons passed\n",
           sizeof(vectors) / sizeof(vectors[0]), checks);
    return 0;
}

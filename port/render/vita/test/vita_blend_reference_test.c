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
        {0, 255, 255, 0, 3, "wrapped negative blend"},
        {20, 80, 255, 0, 216, "wrapped negative green"},
        {40, 200, 255, 0, 137, "wrapped negative blue"},
        {255, 0, 255, 1, 255, "clamped overflow"},
        {255, 0, 255, 0, 251, "wrapped positive overflow"},
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
    printf("%zu vitaGL blend reference vectors passed\n",
           sizeof(vectors) / sizeof(vectors[0]));
    return 0;
}

/*
 * port/audio/test/out_vita_test.c
 *
 * Host-side regression for the native Vita sink's fixed 1024-frame queue.
 * The mock audioout API captures submitted blocks so this test needs no
 * Vita hardware and does not claim to test the Vita driver itself.
 */
#include <psp2/audioout.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../audio_host.h"

enum { BLOCK_FRAMES = 1024, STEREO_SAMPLES = BLOCK_FRAMES * 2 };

static int failures;
static int open_type = -1;
static int open_frames = -1;
static int open_rate = -1;
static int open_mode = -1;
static int output_calls;
static int release_calls;
static int output_result;
static int16_t captured[STEREO_SAMPLES];
static int captured_valid;

static void check(int ok, const char *what)
{
    if (!ok) {
        failures++;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

int sceAudioOutOpenPort(int type, int len, int freq, int mode)
{
    open_type = type;
    open_frames = len;
    open_rate = freq;
    open_mode = mode;
    return 7;
}

int sceAudioOutOutput(int port, const void *buf)
{
    check(port == 7, "output uses opened port");
    output_calls++;
    if (output_result < 0) {
        return output_result;
    }
    memcpy(captured, buf, sizeof(captured));
    captured_valid = 1;
    return 0;
}

int sceAudioOutReleasePort(int port)
{
    check(port == 7, "release uses opened port");
    release_calls++;
    return 0;
}

static void check_block(const int16_t *expected, const char *what)
{
    int i;

    check(captured_valid, "successful output captured");
    for (i = 0; i < STEREO_SAMPLES; i++) {
        if (captured[i] != expected[i]) {
            char msg[128];
            (void)snprintf(msg, sizeof(msg), "%s sample %d: got %d expected %d",
                           what, i, captured[i], expected[i]);
            check(0, msg);
            return;
        }
    }
}

int main(void)
{
    int16_t first[1200 * 2];
    int16_t second[848 * 2];
    int16_t expected[STEREO_SAMPLES];
    int16_t loud[BLOCK_FRAMES * 2];
    int16_t failure_block[BLOCK_FRAMES * 2];
    int i;

    for (i = 0; i < (int)(sizeof(first) / sizeof(first[0])); i++) {
        first[i] = (int16_t)(i + 1);
    }
    for (i = 0; i < (int)(sizeof(second) / sizeof(second[0])); i++) {
        second[i] = (int16_t)(i + 2401);
    }
    for (i = 0; i < (int)(sizeof(loud) / sizeof(loud[0])); i += 2) {
        loud[i] = 1000;
        loud[i + 1] = -1000;
        failure_block[i] = 200;
        failure_block[i + 1] = -200;
    }

    check(ico_audio_vita_open() == 0, "open succeeds");
    check(open_type == SCE_AUDIO_OUT_PORT_TYPE_MAIN, "main audio port selected");
    check(open_frames == BLOCK_FRAMES, "fixed block length is 1024 frames");
    check(open_rate == SCE_AUDIO_OUT_SAMPLE_RATE_48000, "sample rate is 48 kHz");
    check(open_mode == SCE_AUDIO_OUT_MODE_STEREO, "stereo mode selected");

    ico_audio_vita_push(first, 600);
    check(output_calls == 0, "partial block is buffered");
    ico_audio_vita_push(first + 1200, 600);
    check(output_calls == 1, "1200 frames submit one block");
    check_block(first, "first block preserves order");

    memcpy(expected, first + STEREO_SAMPLES, sizeof(first) - sizeof(expected));
    memcpy(expected + (sizeof(first) - sizeof(expected)) / sizeof(first[0]), second,
           sizeof(second));
    ico_audio_vita_push(second, 848);
    check(output_calls == 2, "176 pending plus 848 frames submit next block");
    check_block(expected, "second block preserves order");

    ico_audio_set_volume(0.5);
    ico_audio_vita_push(loud, BLOCK_FRAMES);
    check(output_calls == 3, "full block submits immediately");
    check(captured_valid, "volume block captured");
    for (i = 0; i < STEREO_SAMPLES; i += 2) {
        if (captured[i] != 500 || captured[i + 1] != -500) {
            check(0, "software volume scales stereo samples");
            break;
        }
    }

    output_result = -5;
    ico_audio_vita_push(failure_block, BLOCK_FRAMES);
    check(output_calls == 4, "failed output attempted once");
    check(release_calls == 1, "failed output releases port");
    ico_audio_vita_push(failure_block, BLOCK_FRAMES);
    check(output_calls == 4, "failed port is not reused");
    ico_audio_vita_close();
    check(release_calls == 1, "close does not release failed port twice");

    if (failures == 0) {
        puts("vita_audio_buffer_test: ok");
    }
    return failures != 0;
}

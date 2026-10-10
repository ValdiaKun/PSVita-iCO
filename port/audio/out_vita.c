/*
 * port/audio/out_vita.c
 *
 * Native PlayStation Vita audio output for the software SPU2 mixer.
 *
 * sceAudioOut uses a fixed frame count per submission. Accumulate the
 * host loop's variable-sized 48 kHz blocks (800/801 frames at 59.94 Hz,
 * 960 at 50 Hz) and submit complete 1024-frame chunks. This keeps the
 * hardware cadence at 48 kHz instead of padding every simulated vsync.
 */
#include "audio_host.h"

#include <psp2/audioout.h>
#include <stdio.h>
#include <string.h>

enum {
    ICO_VITA_AUDIO_FRAMES = 1024,
    ICO_VITA_AUDIO_QUEUE_FRAMES = 2048,
    ICO_VITA_AUDIO_CHANNELS = 2
};

_Static_assert(ICO_VITA_AUDIO_FRAMES % SCE_AUDIO_MIN_LEN == 0,
               "Vita audio frame count must satisfy sceAudioOut granularity");
_Static_assert(ICO_VITA_AUDIO_FRAMES <= SCE_AUDIO_MAX_LEN,
               "Vita audio frame count exceeds sceAudioOut maximum");

static int port = -1;
static int16_t pending[ICO_VITA_AUDIO_QUEUE_FRAMES * ICO_VITA_AUDIO_CHANNELS];
static int pending_frames;
static int16_t scaled[ICO_VITA_AUDIO_FRAMES * ICO_VITA_AUDIO_CHANNELS];

int ico_audio_vita_open(void)
{
    if (port >= 0) {
        return 0;
    }
    pending_frames = 0;
    port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, ICO_VITA_AUDIO_FRAMES, 48000,
                               SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) {
        fprintf(stderr, "audio: sceAudioOutOpenPort failed: 0x%08X\n", (unsigned int)port);
        return -1;
    }
    return 0;
}

void ico_audio_vita_push(const int16_t *frames, int count)
{
    int offset = 0;

    if (port < 0 || frames == NULL || count <= 0) {
        return;
    }
    while (offset < count) {
        int room = ICO_VITA_AUDIO_QUEUE_FRAMES - pending_frames;
        int take = count - offset;

        if (take > room) {
            take = room;
        }
        if (take <= 0) {
            /* A successful output normally drains below one block. If the
               queue invariant is ever broken, discard no audio silently. */
            fprintf(stderr, "audio: Vita output queue invariant failed\n");
            return;
        }
        memcpy(pending + pending_frames * ICO_VITA_AUDIO_CHANNELS,
               frames + offset * ICO_VITA_AUDIO_CHANNELS,
               (size_t)take * ICO_VITA_AUDIO_CHANNELS * sizeof(*frames));
        pending_frames += take;
        offset += take;

        while (pending_frames >= ICO_VITA_AUDIO_FRAMES) {
            int q8 = ico_audio_volume_q8();
            int rc;

            ico_audio_scale(scaled, pending, ICO_VITA_AUDIO_FRAMES * ICO_VITA_AUDIO_CHANNELS, q8);
            rc = sceAudioOutOutput(port, scaled);
            if (rc < 0) {
                fprintf(stderr, "audio: sceAudioOutOutput failed: 0x%08X\n", (unsigned int)rc);
                sceAudioOutReleasePort(port);
                port = -1;
                pending_frames = 0;
                return;
            }
            pending_frames -= ICO_VITA_AUDIO_FRAMES;
            memmove(pending, pending + ICO_VITA_AUDIO_FRAMES * ICO_VITA_AUDIO_CHANNELS,
                    (size_t)pending_frames * ICO_VITA_AUDIO_CHANNELS * sizeof(*pending));
        }
    }
}

void ico_audio_vita_close(void)
{
    if (port >= 0) {
        sceAudioOutReleasePort(port);
        port = -1;
    }
    pending_frames = 0;
}

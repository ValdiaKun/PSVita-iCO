#ifndef ICO_TEST_MOCK_PSP2_AUDIOOUT_H
#define ICO_TEST_MOCK_PSP2_AUDIOOUT_H

#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
#define SCE_AUDIO_OUT_MODE_STEREO 1
#define SCE_AUDIO_MIN_LEN 64
#define SCE_AUDIO_MAX_LEN 65472

int sceAudioOutOpenPort(int type, int len, int freq, int mode);
int sceAudioOutOutput(int port, const void *buf);
int sceAudioOutReleasePort(int port);

#endif

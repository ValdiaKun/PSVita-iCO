#ifndef ICO_TEST_MOCK_PSP2_CTRL_H
#define ICO_TEST_MOCK_PSP2_CTRL_H

#include <stdint.h>

#define SCE_CTRL_MODE_ANALOG 1
#define SCE_CTRL_SELECT   0x00000001u
#define SCE_CTRL_START    0x00000008u
#define SCE_CTRL_UP       0x00000010u
#define SCE_CTRL_RIGHT    0x00000020u
#define SCE_CTRL_DOWN     0x00000040u
#define SCE_CTRL_LEFT     0x00000080u
#define SCE_CTRL_LTRIGGER 0x00000100u
#define SCE_CTRL_RTRIGGER 0x00000200u
#define SCE_CTRL_TRIANGLE 0x00001000u
#define SCE_CTRL_CIRCLE   0x00002000u
#define SCE_CTRL_CROSS    0x00004000u
#define SCE_CTRL_SQUARE   0x00008000u
#define SCE_CTRL_L3   0x00010000u
#define SCE_CTRL_R3   0x00020000u

typedef struct SceCtrlData {
    uint32_t timeStamp;
    uint32_t buttons;
    unsigned char lx, ly, rx, ry;
} SceCtrlData;

int sceCtrlSetSamplingMode(int mode);
int sceCtrlPeekBufferPositive(int port, SceCtrlData *data, int count);

#endif

/*
 * port/input/input_vita.c
 *
 * Native PlayStation Vita controller adapter. Samples the physical controls
 * with sceCtrl and feeds the project's existing virtual DualShock pad seam.
 * This deliberately uses a fixed default mapping; desktop binding/config
 * support is a separate integration task.
 */
#include <psp2/ctrl.h>
#include <string.h>

#include "input.h"
#include "input_vita.h"

static int s_ready;

static float axis_value(unsigned char value)
{
    /* Make both endpoints exact while keeping the centre exactly zero. */
    return value < 128 ? ((float)value - 128.0f) / 128.0f
                       : ((float)value - 128.0f) / 127.0f;
}

static void publish_empty(void)
{
    const IcoVirtualPad empty = {0};
    ico_input_set_vpad(&empty);
}

int ico_input_vita_init(void)
{
    if (s_ready) {
        return 0;
    }
    if (sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) < 0) {
        return -1;
    }
    s_ready = 1;
    ico_input_set_live(1);
    publish_empty();
    return 0;
}

int ico_input_vita_update(void)
{
    SceCtrlData sample;
    IcoVirtualPad pad;
    int rc;

    if (!s_ready) {
        return -1;
    }
    rc = sceCtrlPeekBufferPositive(0, &sample, 1);
    if (rc <= 0) {
        /* A transient read failure must release prior buttons and sticks. */
        publish_empty();
        return -1;
    }

    memset(&pad, 0, sizeof(pad));
#define MAP_BUTTON(vita_button, ico_button) \
    do { if ((sample.buttons & (vita_button)) != 0) pad.buttons |= (ico_button); } while (0)
    MAP_BUTTON(SCE_CTRL_LTRIGGER, ICO_PAD_L2);
    MAP_BUTTON(SCE_CTRL_RTRIGGER, ICO_PAD_R2);
    MAP_BUTTON(SCE_CTRL_TRIANGLE, ICO_PAD_TRIANGLE);
    MAP_BUTTON(SCE_CTRL_CIRCLE, ICO_PAD_CIRCLE);
    MAP_BUTTON(SCE_CTRL_CROSS, ICO_PAD_CROSS);
    MAP_BUTTON(SCE_CTRL_SQUARE, ICO_PAD_SQUARE);
    MAP_BUTTON(SCE_CTRL_SELECT, ICO_PAD_SELECT);
    MAP_BUTTON(SCE_CTRL_START, ICO_PAD_START);
    MAP_BUTTON(SCE_CTRL_UP, ICO_PAD_UP);
    MAP_BUTTON(SCE_CTRL_RIGHT, ICO_PAD_RIGHT);
    MAP_BUTTON(SCE_CTRL_DOWN, ICO_PAD_DOWN);
    MAP_BUTTON(SCE_CTRL_LEFT, ICO_PAD_LEFT);
    MAP_BUTTON(SCE_CTRL_L3, ICO_PAD_L3);
    MAP_BUTTON(SCE_CTRL_R3, ICO_PAD_R3);
#undef MAP_BUTTON
    pad.menu_buttons = pad.buttons;
    pad.lx = axis_value(sample.lx);
    pad.ly = axis_value(sample.ly);
    pad.rx = axis_value(sample.rx);
    pad.ry = axis_value(sample.ry);
    ico_input_set_vpad(&pad);
    return 0;
}

void ico_input_vita_shutdown(void)
{
    if (!s_ready) {
        return;
    }
    publish_empty();
    ico_input_set_live(0);
    s_ready = 0;
}

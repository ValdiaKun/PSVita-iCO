#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "input_vita.h"
#include <psp2/ctrl.h>

static int sampling_result;
static int read_result = 1;
static SceCtrlData next_sample;
static int live_state;
static IcoVirtualPad published;

int sceCtrlSetSamplingMode(int mode)
{
    assert(mode == SCE_CTRL_MODE_ANALOG);
    return sampling_result;
}

int sceCtrlPeekBufferPositive(int port, SceCtrlData *data, int count)
{
    assert(port == 0 && count == 1);
    if (read_result > 0) {
        *data = next_sample;
    }
    return read_result;
}

void ico_input_set_live(int on)
{
    live_state = on != 0;
}

void ico_input_set_vpad(const IcoVirtualPad *v)
{
    published = *v;
}

int main(void)
{
    assert(ico_input_vita_init() == 0);
    assert(live_state == 1);
    memset(&next_sample, 0, sizeof(next_sample));
    next_sample.lx = 255;
    next_sample.ly = 0;
    next_sample.rx = 128;
    next_sample.ry = 128;
    next_sample.buttons = SCE_CTRL_CROSS | SCE_CTRL_START | SCE_CTRL_LTRIGGER;
    assert(ico_input_vita_update() == 0);
    assert(published.buttons == (ICO_PAD_CROSS | ICO_PAD_START | ICO_PAD_L1));
    assert(published.menu_buttons == published.buttons);
    assert(published.lx == 1.0f && published.ly == -1.0f);
    assert(published.rx == 0.0f && published.ry == 0.0f);

    read_result = 0;
    assert(ico_input_vita_update() == -1);
    assert(published.buttons == 0 && published.menu_buttons == 0);
    assert(published.lx == 0.0f && published.ly == 0.0f);

    ico_input_vita_shutdown();
    assert(live_state == 0 && published.buttons == 0);

    sampling_result = -1;
    assert(ico_input_vita_init() == -1);
    assert(live_state == 0);
    puts("vita_input_test: ok");
    return 0;
}

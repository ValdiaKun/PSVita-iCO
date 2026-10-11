/*
 * port/render/vita/vita_gl_probe.c
 *
 * Minimal in-repository Vita graphics integration probe. This is deliberately
 * independent of ICO's RHI: it verifies that project-owned code can initialize
 * vitaGL, submit colored geometry, present frames, and exit on the Vita pad.
 * It is not the ICO renderer and does not load game data.
 */
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    vglInit(0x800000);

    glViewport(0, 0, 960, 544);
    glClearColor(0.035f, 0.055f, 0.10f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 960.0, 544.0, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (;;) {
        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0 &&
            (pad.buttons & SCE_CTRL_START) != 0) {
            break;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_TRIANGLES);
        glColor3f(0.95f, 0.20f, 0.16f);
        glVertex2f(480.0f, 92.0f);
        glColor3f(0.20f, 0.85f, 0.45f);
        glVertex2f(690.0f, 440.0f);
        glColor3f(0.20f, 0.45f, 1.0f);
        glVertex2f(270.0f, 440.0f);
        glEnd();
        vglSwapBuffers(GL_FALSE);
    }

    sceKernelExitProcess(0);
    return 0;
}

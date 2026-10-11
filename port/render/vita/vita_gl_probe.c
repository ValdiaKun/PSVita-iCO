/*
 * port/render/vita/vita_gl_probe.c
 *
 * Minimal in-repository Vita graphics integration probe. This is deliberately
 * independent of ICO's RHI: it verifies that project-owned code can initialize
 * vitaGL, submit colored and textured geometry, exercise depth/blend state,
 * present frames, and exit on the Vita pad. It is not the ICO renderer and does
 * not load game data.
 */
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>

static unsigned char s_checker[4 * 4 * 4];

static void init_checker_texture(void)
{
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            unsigned char *pixel = &s_checker[(y * 4 + x) * 4];
            const int alternate = (x + y) & 1;
            pixel[0] = alternate ? 245 : 25;
            pixel[1] = alternate ? 70 : 210;
            pixel[2] = alternate ? 35 : 245;
            /* Half-alpha cells make the background visible through the texture. */
            pixel[3] = alternate ? 255 : 128;
        }
    }
}

static void draw_probe_scene(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* A distant colored triangle verifies interpolated vertex colors. */
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor4f(0.95f, 0.20f, 0.16f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(480.0f, 76.0f, -0.7f);
    glColor4f(0.20f, 0.85f, 0.45f, 1.0f);
    glVertex3f(710.0f, 420.0f, -0.7f);
    glColor4f(0.20f, 0.45f, 1.0f, 1.0f);
    glVertex3f(250.0f, 420.0f, -0.7f);
    glEnd();

    /* Draw a solid background quad, then a nearer alpha-textured quad. */
    glColor4f(0.10f, 0.35f, 0.85f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(80.0f, 170.0f, -0.3f);
    glVertex3f(400.0f, 170.0f, -0.3f);
    glVertex3f(80.0f, 490.0f, -0.3f);
    glVertex3f(400.0f, 490.0f, -0.3f);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(110.0f, 200.0f, 0.2f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(370.0f, 200.0f, 0.2f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(110.0f, 460.0f, 0.2f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(370.0f, 460.0f, 0.2f);
    glEnd();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

int main(void)
{
    SceCtrlData pad;
    GLuint texture = 0;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (!vglInit(0x800000)) {
        sceKernelExitProcess(1);
        return 1;
    }

    glViewport(0, 0, 960, 544);
    glClearColor(0.035f, 0.055f, 0.10f, 1.0f);
    glClearDepthf(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 960.0, 544.0, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    init_checker_texture();
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, s_checker);
    if (texture == 0 || glGetError() != GL_NO_ERROR) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
        sceKernelExitProcess(2);
        return 2;
    }

    for (;;) {
        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0 &&
            (pad.buttons & SCE_CTRL_START) != 0) {
            break;
        }

        draw_probe_scene();
        vglSwapBuffers(GL_FALSE);
    }

    glDeleteTextures(1, &texture);
    sceKernelExitProcess(0);
    return 0;
}

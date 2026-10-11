/*
 * port/render/vita/vita_gl_probe.c
 *
 * Minimal in-repository Vita graphics integration probe. This is deliberately
 * independent of ICO's RHI: it verifies that project-owned code can initialize
 * vitaGL, compile a native Cg shader, submit colored and textured geometry,
 * exercise depth/blend state, present frames, and exit on the Vita pad. It is
 * not the ICO renderer and does not load game data.
 */
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>
#include <stdio.h>

typedef struct ProbeVertex {
    float position[3];
    float color[4];
} ProbeVertex;

static unsigned char s_checker[4 * 4 * 4];
static FILE *s_log;

static void probe_log(const char *message)
{
    FILE *out = s_log != NULL ? s_log : stderr;
    fprintf(out, "%s\n", message);
    fflush(out);
}

static void probe_shader_log(const char *label, GLuint object, int is_program)
{
    GLchar info[768] = {0};
    GLsizei length = 0;
    if (is_program) {
        glGetProgramInfoLog(object, (GLsizei)sizeof(info), &length, info);
    } else {
        glGetShaderInfoLog(object, (GLsizei)sizeof(info), &length, info);
    }
    if (length > 0) {
        FILE *out = s_log != NULL ? s_log : stderr;
        fprintf(out, "%s: %s\n", label, info);
        fflush(out);
    } else {
        probe_log(label);
    }
}

static void probe_log_gl_error(const char *stage)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        FILE *out = s_log != NULL ? s_log : stderr;
        fprintf(out, "%s: GL error 0x%04x\n", stage, (unsigned int)error);
        fflush(out);
    }
}

static const char s_vertex_shader[] =
    "void main(\n"
    "    float3 aPosition,\n"
    "    float4 aColor,\n"
    "    float4 out vPosition : POSITION,\n"
    "    float4 out vColor : COLOR)\n"
    "{\n"
    "    vPosition = float4(aPosition, 1.0f);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char s_fragment_shader[] =
    "float4 main(float4 vColor : COLOR)\n"
    "{\n"
    "    return vColor;\n"
    "}\n";

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

static GLuint compile_cg_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint compiled = GL_FALSE;

    if (shader == 0) {
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        probe_shader_log(type == GL_CG_VERTEX_SHADER_EXT ? "Cg vertex compile failed" :
                         "Cg fragment compile failed", shader, 0);
        probe_log_gl_error("Cg shader compile");
        glDeleteShader(shader);
        return 0;
    }
    probe_log(type == GL_CG_VERTEX_SHADER_EXT ? "Cg vertex compile passed" :
              "Cg fragment compile passed");
    return shader;
}

static GLuint create_cg_program(void)
{
    GLuint vertex = compile_cg_shader(GL_CG_VERTEX_SHADER_EXT, s_vertex_shader);
    GLuint fragment = compile_cg_shader(GL_CG_FRAGMENT_SHADER_EXT, s_fragment_shader);
    GLuint program;
    GLint linked = GL_FALSE;

    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return 0;
    }

    program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aColor");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    if (linked != GL_TRUE) {
        probe_shader_log("Cg program link failed", program, 1);
        probe_log_gl_error("Cg program link");
        glDeleteProgram(program);
        return 0;
    }
    probe_log("Cg program link passed");
    return program;
}

static void draw_colored_triangle(GLuint program)
{
    static const ProbeVertex vertices[] = {
        {{-0.78f, -0.70f, 0.0f}, {0.95f, 0.20f, 0.16f, 1.0f}},
        {{ 0.00f,  0.78f, 0.0f}, {0.20f, 0.85f, 0.45f, 1.0f}},
        {{ 0.78f, -0.70f, 0.0f}, {0.20f, 0.45f, 1.00f, 1.0f}},
    };

    if (program != 0) {
        glUseProgram(program);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(ProbeVertex), vertices[0].position);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                              sizeof(ProbeVertex), vertices[0].color);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        glUseProgram(0);
        return;
    }

    /* Visible fallback: a red background means the native Cg shader failed. */
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
}

static void draw_textured_depth_probe(void)
{
    /* The solid quad is farther away than the alpha-textured quad. */
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
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
    GLuint program;

    s_log = fopen("ux0:data/ico-vita-gl-probe.log", "wb");
    probe_log("ICO VitaGL probe started");
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (!vglInit(0x800000)) {
        probe_log("vglInit failed");
        if (s_log != NULL) fclose(s_log);
        sceKernelExitProcess(1);
        return 1;
    }
    probe_log("vglInit passed");

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

    program = create_cg_program();
    if (program == 0) {
        /* The color is a visible status indicator; fixed-function tests continue. */
        glClearColor(0.28f, 0.015f, 0.025f, 1.0f);
    }

    /* Shader failure is shown by the red background, not misreported as a
     * texture-upload failure through a stale GL error flag. */
    while (glGetError() != GL_NO_ERROR) {
    }

    init_checker_texture();
    glGenTextures(1, &texture);
    if (texture == 0) {
        if (program != 0) {
            glDeleteProgram(program);
        }
        probe_log("glGenTextures failed");
        if (s_log != NULL) fclose(s_log);
        sceKernelExitProcess(2);
        return 2;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, s_checker);
    {
        GLenum texture_error = glGetError();
        if (texture_error != GL_NO_ERROR) {
            {
                FILE *out = s_log != NULL ? s_log : stderr;
                fprintf(out, "texture upload failed: GL error 0x%04x\n", (unsigned int)texture_error);
                fflush(out);
            }
            glDeleteTextures(1, &texture);
            if (program != 0) {
                glDeleteProgram(program);
            }
            if (s_log != NULL) fclose(s_log);
            sceKernelExitProcess(3);
            return 3;
        }
    }

    for (;;) {
        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0 &&
            (pad.buttons & SCE_CTRL_START) != 0) {
            break;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw_colored_triangle(program);
        draw_textured_depth_probe();
        vglSwapBuffers(GL_FALSE);
    }

    glDeleteTextures(1, &texture);
    if (program != 0) {
        glDeleteProgram(program);
    }
    probe_log(program != 0 ? "Probe ended: Cg shader path passed" :
              "Probe ended: Cg shader path failed; fixed-function fallback was used");
    if (s_log != NULL) fclose(s_log);
    sceKernelExitProcess(program != 0 ? 0 : 4);
    return program != 0 ? 0 : 4;
}

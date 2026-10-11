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

typedef struct BlendTestVertex {
    float position[2];
    float texcoord[2];
} BlendTestVertex;

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

/* This tests the proposed RGBA8_UNORM fallback for the RHI's RGBA8_UINT
 * GS blend target. A=Cs, B=Cd, C=As, D=Cd in ((A-B)*C >> 7)+D. */
static const char s_blend_vertex_shader[] =
    "void main(\n"
    "    float2 aPosition,\n"
    "    float2 aTexcoord,\n"
    "    float4 out vPosition : POSITION,\n"
    "    float2 out vTexcoord : TEXCOORD0)\n"
    "{\n"
    "    vPosition = float4(aPosition, 0.0f, 1.0f);\n"
    "    vTexcoord = aTexcoord;\n"
    "}\n";

static const char s_blend_fragment_shader[] =
    "float4 main(\n"
    "    float2 vTexcoord : TEXCOORD0,\n"
    "    uniform sampler2D uSrc,\n"
    "    uniform sampler2D uDst,\n"
    "    uniform float uClampMode) : COLOR\n"
    "{\n"
    "    float4 cs = floor(tex2D(uSrc, vTexcoord) * 255.0f + 0.5f);\n"
    "    float4 cd = floor(tex2D(uDst, vTexcoord) * 255.0f + 0.5f);\n"
    "    float3 v = floor(((cs.rgb - cd.rgb) * cs.a) * (1.0f / 128.0f)) + cd.rgb;\n"
    "    if (uClampMode > 0.5f) v = clamp(v, 0.0f, 255.0f);\n"
    "    else v = v - floor(v * (1.0f / 256.0f)) * 256.0f;\n"
    "    return float4(v, cs.a) * (1.0f / 255.0f);\n"
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

static GLuint create_unorm_blend_program(void)
{
    GLuint vertex = compile_cg_shader(GL_CG_VERTEX_SHADER_EXT, s_blend_vertex_shader);
    GLuint fragment = compile_cg_shader(GL_CG_FRAGMENT_SHADER_EXT, s_blend_fragment_shader);
    GLuint program;
    GLint linked = GL_FALSE;

    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return 0;
    }

    program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        probe_log("UNORM blend test could not allocate a program");
        return 0;
    }
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexcoord");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (linked != GL_TRUE) {
        probe_shader_log("UNORM blend test program link failed", program, 1);
        probe_log_gl_error("UNORM blend test link");
        glDeleteProgram(program);
        return 0;
    }
    probe_log("UNORM blend test shader link passed");
    return program;
}

static int test_unorm_integer_blend_emulation(void)
{
    static const BlendTestVertex vertices[] = {
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f}, {0.0f, 1.0f}},
        {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
    };
    static const struct {
        unsigned char src[4];
        unsigned char dst[4];
        float clamp_mode;
        unsigned char expected[4];
        const char *name;
    } cases[] = {
        {{200, 100,  50,  64}, {100, 80,  20, 255}, 1.0f, {150,  90,  35,  64}, "clamped blend"},
        {{  0,  20,  40, 255}, {255, 80, 200, 255}, 0.0f, {  3, 216, 137, 255}, "wrapped negative blend"},
        {{255, 255, 255, 255}, {  0,  0,   0, 255}, 1.0f, {255, 255, 255, 255}, "clamped overflow"},
    };
    GLuint textures[3] = {0, 0, 0};
    GLuint framebuffer = 0;
    GLuint program = 0;
    GLint src_location, dst_location, clamp_location;
    int ok = 0;

    program = create_unorm_blend_program();
    if (program == 0) {
        probe_log("UNORM integer blend emulation test: shader unavailable");
        goto cleanup;
    }

    glGenTextures(3, textures);
    if (textures[0] == 0 || textures[1] == 0 || textures[2] == 0) {
        probe_log("UNORM integer blend emulation test: texture allocation failed");
        goto cleanup;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 cases[0].src);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 cases[0].dst);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &framebuffer);
    if (framebuffer == 0) {
        probe_log("UNORM integer blend emulation test: framebuffer allocation failed");
        goto cleanup;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           textures[2], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        probe_log("UNORM integer blend emulation test: framebuffer incomplete");
        goto cleanup;
    }

    src_location = glGetUniformLocation(program, "uSrc");
    dst_location = glGetUniformLocation(program, "uDst");
    clamp_location = glGetUniformLocation(program, "uClampMode");
    if (src_location < 0 || dst_location < 0 || clamp_location < 0) {
        probe_log("UNORM integer blend emulation test: required shader uniform missing");
        goto cleanup;
    }

    glViewport(0, 0, 4, 4);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glUniform1i(src_location, 0);
    glUniform1i(dst_location, 1);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BlendTestVertex),
                          vertices[0].position);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BlendTestVertex),
                          vertices[0].texcoord);

    ok = 1;
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        unsigned char actual[4] = {0, 0, 0, 0};
        GLenum error;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        cases[i].src);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        cases[i].dst);
        glUniform1f(clamp_location, cases[i].clamp_mode);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            FILE *out = s_log != NULL ? s_log : stderr;
            fprintf(out, "%s: GL error 0x%04x\n", cases[i].name, (unsigned int)error);
            fflush(out);
            ok = 0;
            break;
        }
        for (unsigned int channel = 0; channel < 4; ++channel) {
            if (actual[channel] != cases[i].expected[channel]) {
                FILE *out = s_log != NULL ? s_log : stderr;
                fprintf(out, "%s mismatch channel %u: expected %u, got %u\n",
                        cases[i].name, channel, (unsigned int)cases[i].expected[channel],
                        (unsigned int)actual[channel]);
                fflush(out);
                ok = 0;
                break;
            }
        }
        if (!ok) break;
        if (s_log != NULL) {
            fprintf(s_log, "%s passed: RGBA=(%u,%u,%u,%u)\n", cases[i].name,
                    (unsigned int)actual[0], (unsigned int)actual[1],
                    (unsigned int)actual[2], (unsigned int)actual[3]);
            fflush(s_log);
        }
    }

    if (ok) {
        probe_log("UNORM integer blend emulation: all byte-exact cases passed");
    } else {
        probe_log("UNORM integer blend emulation: failed; RGBA8_UINT fallback remains unproven");
    }

cleanup:
    glUseProgram(0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (framebuffer != 0) glDeleteFramebuffers(1, &framebuffer);
    if (textures[0] != 0 || textures[1] != 0 || textures[2] != 0) glDeleteTextures(3, textures);
    if (program != 0) glDeleteProgram(program);
    glViewport(0, 0, 960, 544);
    glEnable(GL_DEPTH_TEST);
    return ok;
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

    const int unorm_blend_ok = test_unorm_integer_blend_emulation();
    if (program != 0 && !unorm_blend_ok) {
        glClearColor(0.28f, 0.12f, 0.015f, 1.0f);
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
            FILE *out = s_log != NULL ? s_log : stderr;
            fprintf(out, "texture upload failed: GL error 0x%04x\n", (unsigned int)texture_error);
            fflush(out);
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
    const int exit_code = program == 0 ? 4 : !unorm_blend_ok ? 5 : 0;
    probe_log(exit_code == 0 ? "Probe ended: Cg shader and byte-exact UNORM blend tests passed" :
              exit_code == 4 ? "Probe ended: Cg shader path failed; fixed-function fallback was used" :
              "Probe ended: UNORM integer blend emulation failed");
    if (s_log != NULL) fclose(s_log);
    sceKernelExitProcess(exit_code);
    return exit_code;
}

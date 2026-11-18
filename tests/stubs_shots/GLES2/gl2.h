// Sinov uchun OpenGL ES 2.0 ning DASTURIY (CPU) amalga oshirilishi: o'yin chizgan uchburchaklarni xotiradagi rasmga to'ldiradi.
// Faqat main.c ishlatadigan chaqiruvlar bor (tri/fan/strip, rang, shaffoflik). Do'kon uchun skrinshotlar olishda ishlatiladi.
#ifndef STUB_GL2_SOFT_H
#define STUB_GL2_SOFT_H
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned GLuint, GLenum; typedef int GLint, GLsizei; typedef float GLfloat; typedef unsigned char GLboolean; typedef unsigned GLbitfield;
#define GL_VERTEX_SHADER 1
#define GL_FRAGMENT_SHADER 2
#define GL_FLOAT 3
#define GL_FALSE 0
#define GL_TRIANGLE_FAN 6
#define GL_TRIANGLE_STRIP 5
#define GL_TRIANGLES 4
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_BLEND 0xBE2
#define GL_SRC_ALPHA 0x302
#define GL_ONE_MINUS_SRC_ALPHA 0x303
static unsigned char *gl_fb; static int gl_w, gl_h;                  // RGB, 3 bayt/piksel
static float gl_col[4] = {1, 1, 1, 1}, gl_clr[4] = {0, 0, 0, 1}, gl_res[2] = {1, 1};
static const float *gl_vp;

static void gl_tri(float x0, float y0, float x1, float y1, float x2, float y2) {
    float sx = gl_w / gl_res[0], sy = gl_h / gl_res[1];
    x0 *= sx; x1 *= sx; x2 *= sx; y0 *= sy; y1 *= sy; y2 *= sy;
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 1e-6f) return;
    int minx = (int)floorf(fminf(x0, fminf(x1, x2))), maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
    int miny = (int)floorf(fminf(y0, fminf(y1, y2))), maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    if (minx < 0) minx = 0; if (miny < 0) miny = 0; if (maxx > gl_w - 1) maxx = gl_w - 1; if (maxy > gl_h - 1) maxy = gl_h - 1;
    float a = gl_col[3], ia = 1.f - a; float cr = gl_col[0] * 255.f * a, cg = gl_col[1] * 255.f * a, cb = gl_col[2] * 255.f * a;
    float inv = 1.f / area;
    for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
        float px = x + 0.5f + 1e-4f, py = y + 0.5f + 2e-4f;
        float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * inv;
        float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * inv;
        float w2 = 1.f - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        unsigned char *d = gl_fb + ((size_t)y * gl_w + x) * 3;
        d[0] = (unsigned char)(cr + d[0] * ia); d[1] = (unsigned char)(cg + d[1] * ia); d[2] = (unsigned char)(cb + d[2] * ia);
    }
}
static inline void glUniform4f(GLint l, float a, float b, float c, float d) { (void)l; gl_col[0] = a; gl_col[1] = b; gl_col[2] = c; gl_col[3] = d; }
static inline void glUniform2f(GLint l, float a, float b) { (void)l; gl_res[0] = a; gl_res[1] = b; }
static inline void glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p) { (void)i; (void)s; (void)t; (void)n; (void)st; gl_vp = (const float *)p; }
static inline void glDrawArrays(GLenum m, GLint f, GLsizei c) {
    const float *v = gl_vp + 2 * f;
    if (m == GL_TRIANGLES) for (int i = 0; i + 2 < c; i += 3) gl_tri(v[2*i], v[2*i+1], v[2*i+2], v[2*i+3], v[2*i+4], v[2*i+5]);
    else if (m == GL_TRIANGLE_FAN) for (int i = 1; i + 1 < c; i++) gl_tri(v[0], v[1], v[2*i], v[2*i+1], v[2*i+2], v[2*i+3]);
    else if (m == GL_TRIANGLE_STRIP) for (int i = 0; i + 2 < c; i++) gl_tri(v[2*i], v[2*i+1], v[2*i+2], v[2*i+3], v[2*i+4], v[2*i+5]);
}
static inline void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) { (void)a; (void)b; (void)c; (void)d; }
static inline void glClearColor(float a, float b, float c, float d) { gl_clr[0] = a; gl_clr[1] = b; gl_clr[2] = c; gl_clr[3] = d; }
static inline void glClear(GLbitfield m) { (void)m; for (long i = 0; i < (long)gl_w * gl_h; i++) { gl_fb[3*i] = (unsigned char)(gl_clr[0]*255); gl_fb[3*i+1] = (unsigned char)(gl_clr[1]*255); gl_fb[3*i+2] = (unsigned char)(gl_clr[2]*255); } }
static inline void glUseProgram(GLuint p) { (void)p; }
static inline GLuint glCreateShader(GLenum t) { (void)t; return 1; }
static inline void glShaderSource(GLuint s, GLsizei n, const char *const *src, const GLint *l) { (void)s; (void)n; (void)src; (void)l; }
static inline void glCompileShader(GLuint s) { (void)s; }
static inline GLuint glCreateProgram(void) { return 1; }
static inline void glAttachShader(GLuint p, GLuint s) { (void)p; (void)s; }
static inline void glLinkProgram(GLuint p) { (void)p; }
static inline GLint glGetUniformLocation(GLuint p, const char *n) { (void)p; (void)n; return 0; }
static inline GLint glGetAttribLocation(GLuint p, const char *n) { (void)p; (void)n; return 0; }
static inline void glEnableVertexAttribArray(GLuint i) { (void)i; }
static inline void glEnable(GLenum c) { (void)c; }
static inline void glBlendFunc(GLenum a, GLenum b) { (void)a; (void)b; }
#endif

#ifndef STUB_GL2_H
#define STUB_GL2_H
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
static long gl_draw_calls;
static inline void glUniform4f(GLint l, float a, float b, float c, float d) { (void)l; (void)a; (void)b; (void)c; (void)d; }
static inline void glUniform2f(GLint l, float a, float b) { (void)l; (void)a; (void)b; }
static inline void glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p) { (void)i; (void)s; (void)t; (void)n; (void)st; (void)p; }
static inline void glDrawArrays(GLenum m, GLint f, GLsizei c) { (void)m; (void)f; (void)c; gl_draw_calls++; }
static inline void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) { (void)a; (void)b; (void)c; (void)d; }
static inline void glClearColor(float a, float b, float c, float d) { (void)a; (void)b; (void)c; (void)d; }
static inline void glClear(GLbitfield m) { (void)m; }
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

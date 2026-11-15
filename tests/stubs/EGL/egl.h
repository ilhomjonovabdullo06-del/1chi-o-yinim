#ifndef STUB_EGL_H
#define STUB_EGL_H
#include <stdint.h>
typedef void *EGLDisplay, *EGLSurface, *EGLContext, *EGLConfig; typedef int32_t EGLint; typedef unsigned EGLBoolean;
#define EGL_DEFAULT_DISPLAY ((void *)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
enum { EGL_NONE = 0x3038, EGL_RENDERABLE_TYPE = 0x3040, EGL_OPENGL_ES2_BIT = 4, EGL_SURFACE_TYPE = 0x3033, EGL_WINDOW_BIT = 4,
       EGL_RED_SIZE = 0x3024, EGL_GREEN_SIZE = 0x3023, EGL_BLUE_SIZE = 0x3022, EGL_NATIVE_VISUAL_ID = 0x302E,
       EGL_CONTEXT_CLIENT_VERSION = 0x3098, EGL_WIDTH = 0x3057, EGL_HEIGHT = 0x3056 };
static inline EGLDisplay eglGetDisplay(void *d) { (void)d; return (EGLDisplay)1; }
static inline EGLBoolean eglInitialize(EGLDisplay d, EGLint *a, EGLint *b) { (void)d; (void)a; (void)b; return 1; }
static inline EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint n, EGLint *o) { (void)d; (void)a; (void)c; (void)n; (void)o; return 1; }
static inline EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v) { (void)d; (void)c; (void)a; (void)v; return 1; }
static inline EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, void *w, const EGLint *a) { (void)d; (void)c; (void)w; (void)a; return (EGLSurface)1; }
static inline EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext s, const EGLint *a) { (void)d; (void)c; (void)s; (void)a; return (EGLContext)1; }
static inline EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface a, EGLSurface b, EGLContext c) { (void)d; (void)a; (void)b; (void)c; return 1; }
static inline EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v) { (void)d; (void)s; (void)a; *v = 0; return 1; }
static inline EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { (void)d; (void)c; return 1; }
static inline EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return 1; }
static inline EGLBoolean eglTerminate(EGLDisplay d) { (void)d; return 1; }
static inline EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return 1; }
#endif

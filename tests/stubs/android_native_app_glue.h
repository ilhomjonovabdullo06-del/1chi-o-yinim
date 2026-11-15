// Linux'da main.c'ni kompilyatsiya qilish uchun Android NDK sarlavhalarining sodda "stub" nusxasi (faqat sinov uchun).
#ifndef STUB_GLUE_H
#define STUB_GLUE_H
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
typedef struct AInputEvent { int type, action, key; float x, y; } AInputEvent;
struct android_app;
struct android_poll_source { void (*process)(struct android_app *, struct android_poll_source *); };
struct android_app {
    void (*onAppCmd)(struct android_app *, int32_t);
    int32_t (*onInputEvent)(struct android_app *, AInputEvent *);
    ANativeWindow *window; int destroyRequested;
};
enum { APP_CMD_INIT_WINDOW = 1, APP_CMD_TERM_WINDOW = 2 };
#define AINPUT_EVENT_TYPE_KEY 1
#define AINPUT_EVENT_TYPE_MOTION 2
#define AKEYCODE_BACK 4
#define AKEY_EVENT_ACTION_UP 1
#define AMOTION_EVENT_ACTION_MASK 0xff
#define AMOTION_EVENT_ACTION_POINTER_INDEX_MASK 0xff00
#define AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT 8
#define AMOTION_EVENT_ACTION_DOWN 0
#define AMOTION_EVENT_ACTION_UP 1
#define AMOTION_EVENT_ACTION_MOVE 2
#define AMOTION_EVENT_ACTION_CANCEL 3
#define AMOTION_EVENT_ACTION_POINTER_DOWN 5
#define AMOTION_EVENT_ACTION_POINTER_UP 6
static inline int32_t AInputEvent_getType(const AInputEvent *e) { return e->type; }
static inline int32_t AKeyEvent_getKeyCode(const AInputEvent *e) { return e->key; }
static inline int32_t AKeyEvent_getAction(const AInputEvent *e) { return e->action; }
static inline int32_t AMotionEvent_getAction(const AInputEvent *e) { return e->action; }
static inline float AMotionEvent_getX(const AInputEvent *e, int i) { (void)i; return e->x; }
static inline float AMotionEvent_getY(const AInputEvent *e, int i) { (void)i; return e->y; }
static inline int32_t AMotionEvent_getPointerId(const AInputEvent *e, int i) { (void)e; (void)i; return 0; }
static inline int32_t AMotionEvent_getPointerCount(const AInputEvent *e) { (void)e; return 1; }
static inline int ALooper_pollAll(int t, void *a, int *b, void **c) { (void)t; (void)a; (void)b; (void)c; return -1; }
static inline int ANativeWindow_setBuffersGeometry(ANativeWindow *w, int a, int b, int c) { (void)w; (void)a; (void)b; (void)c; return 0; }
#endif

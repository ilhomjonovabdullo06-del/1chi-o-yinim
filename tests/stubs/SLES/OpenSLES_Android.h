#ifndef STUB_SLESA_H
#define STUB_SLESA_H
#include "OpenSLES.h"
typedef struct SLAndroidSimpleBufferQueueItf_ { SLresult (*Enqueue)(const void *, ...); SLresult (*Clear)(const void *, ...); } const *const *SLAndroidSimpleBufferQueueItf;
static const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = 0;
#endif

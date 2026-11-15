#ifndef STUB_SLES_H
#define STUB_SLES_H
#include <stdint.h>
typedef uint32_t SLresult, SLuint32; typedef int16_t SLint16; typedef uint8_t SLboolean;
#define SL_RESULT_SUCCESS 0
#define SL_BOOLEAN_FALSE 0
#define SL_BOOLEAN_TRUE 1
typedef struct SLObjectItf_ { SLresult (*Realize)(const void *, ...); SLresult (*GetInterface)(const void *, ...); } const *const *SLObjectItf;
typedef struct SLEngineItf_ { SLresult (*CreateOutputMix)(const void *, ...); SLresult (*CreateAudioPlayer)(const void *, ...); } const *const *SLEngineItf;
typedef struct SLPlayItf_ { SLresult (*SetPlayState)(const void *, ...); } const *const *SLPlayItf;
typedef const void *SLInterfaceID;
static const SLInterfaceID SL_IID_ENGINE = 0, SL_IID_PLAY = 0;
#define SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE 1
#define SL_DATALOCATOR_OUTPUTMIX 2
#define SL_DATAFORMAT_PCM 3
#define SL_SAMPLINGRATE_22_05 22050000
#define SL_PCMSAMPLEFORMAT_FIXED_16 16
#define SL_SPEAKER_FRONT_CENTER 4
#define SL_BYTEORDER_LITTLEENDIAN 2
#define SL_PLAYSTATE_PLAYING 3
typedef struct { SLuint32 locatorType, numBuffers; } SLDataLocator_AndroidSimpleBufferQueue;
typedef struct { SLuint32 formatType, numChannels, samplesPerSec, bitsPerSample, containerSize, channelMask, endianness; } SLDataFormat_PCM;
typedef struct { void *pLocator, *pFormat; } SLDataSource;
typedef struct { SLuint32 locatorType; SLObjectItf outputMix; } SLDataLocator_OutputMix;
typedef struct { void *pLocator, *pFormat; } SLDataSink;
static inline SLresult slCreateEngine(SLObjectItf *e, SLuint32 a, const void *b, SLuint32 c, const void *d, const void *f) { (void)e; (void)a; (void)b; (void)c; (void)d; (void)f; return 1; }
#endif

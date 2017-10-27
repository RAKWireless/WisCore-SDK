#ifndef __AUDIO_CAPTURE___
#define __AUDIO_CAPTURE___
/* libao relative library*/
#include <alsa/asoundlib.h>
#include "aplay/aplay.h"

int steroConverMono(char *src, size_t srcLen, char *dst);
int monoConverStero(char *src, size_t srcLen, char *dst);
int RK_InitCaptureParam(S_AUDIO_PARAM *ps_AudioParam);
int RK_CaptureStreamSavefile(snd_pcm_t *handle, S_AUDIO_PARAM *ps_AudioParam, size_t captureBytes);
#endif	//__AUDIO_CAPTURE___
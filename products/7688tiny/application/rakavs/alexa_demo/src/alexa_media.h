//dec_raw.c
//by sevencheng 2017-01-09
//dec_raw.h
//by sevencheng
#ifndef __ALEXA_MEDIA_H__
#define __ALEXA_MEDIA_H__
#include <semaphore.h>

//#include "utils.h"

#include "alexa_common.h"
//#include "alarm_timer.h"
#include "avs_private.h"

void *RK_MediaStreamThread(void *arg);
int RK_AudioPlayerRequestMedia(S_ALEXA_RES *psAlexaRes, S_PlayDirective *psPlay);


#endif

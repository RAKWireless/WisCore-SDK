//audio_ao.c
//by sevencheng
#ifndef __AUDIO_AO_H__
#define __AUDIO_AO_H__
#include <ao/ao.h>

int RK_AOInit(void);
void RK_AODeinit(void);
ao_device *RK_AODev_open(int default_driver, int smplerate);
void RK_AOAdjustVolume(char* frame_buffer, int frame_size, float vol);
int RK_AO_MonoConverStero(unsigned char *src, size_t srcLen, unsigned char *dst);
#endif	//__DEC_MP3_H__

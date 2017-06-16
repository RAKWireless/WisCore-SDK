//audio_ao.c
//by sevencheng
//LIBAO
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* private header*/
#include "audio_debug.h"
#include "audio_ao.h"

//@ software mothed for adjust pcm volume
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;	
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)(WORD)(w))
#define HIBYTE(w) ((BYTE)((WORD)(w) >> 8))
//end

#define SKIP_STEP 4

int RK_AO_MonoConverStero(unsigned char *src, size_t srcLen, unsigned char *dst)
{	
	unsigned int change;	
	int i, interval = srcLen/SKIP_STEP;	
	int dst_size=0;	
	for(i=0; i<interval; i++){		
		change = *(unsigned int *)(src+(i*SKIP_STEP));		
		*dst++ = (unsigned char)change;		
		*dst++ = (unsigned char)(change>>8);		
		*dst++ = (unsigned char)change;		
		*dst++ = (unsigned char)(change>>8);		
		*dst++ = (unsigned char)(change>>16);		
		*dst++ = (unsigned char)(change>>24);		
		*dst++ = (unsigned char)(change>>16);		
		*dst++ = (unsigned char)(change>>24);		
		dst_size += 8;	
	}
	
	return dst_size;
}

void RK_AOAdjustVolume(char* frame_buffer, int frame_size, float vol)
{
	
	int i;

	if(!frame_size )
	{
	    return;
	}

	if(vol <= 0 || vol >= 1)
	{
	    return;
	}
	
	for (i = 0; i < frame_size;)
	{
		/* for 16bit*/
//		signed long minData = -0x8000; 	/* if 8bit have to set -0x80 */
//		signed long maxData = 0x7FFF;	/* if 8bit have to set -0x7f */

		signed short wData = frame_buffer[i+1];
		wData = MAKEWORD(frame_buffer[i],frame_buffer[i+1]);
		signed long dwData = wData;

		dwData = dwData * vol;//0.1 -1
		if (dwData < -0x8000)
		{
			dwData = -0x8000;
		}
		else if (dwData > 0x7FFF)
		{
			dwData = 0x7FFF;
		}

		wData = LOWORD(dwData);
		frame_buffer[i] = LOBYTE(wData);
		frame_buffer[i+1] = HIBYTE(wData);
		i += 2;
	}
	
}

int RK_AOInit(void)
{
	int default_driver;
	
	/* -- Initialize -- */
	fprintf(stderr, "libao start to initalize...\n");
	ao_initialize();
	
	/* -- Setup for default driver -- */
	default_driver = ao_default_driver_id();
	if(default_driver < 0){
		AUDIO_ERROR("ao_init is failed\n");
	}
	
	return default_driver;
}

void RK_AODeinit(void)
{
	ao_shutdown();
}

ao_device *RK_AODev_open(int default_driver, int smplerate)
{
	ao_device 			*device;
	ao_option			ao_options = {"dev", "plug:dmix"};
	ao_sample_format 	format;
	
    memset(&format, 0, sizeof(ao_sample_format));
	format.bits = 16;
	format.channels = 2;
	format.rate = smplerate;		//24000
	format.byte_format = AO_FMT_LITTLE;

	/* -- Open driver -- */
	device = ao_open_live(default_driver, &format, &ao_options);

	return device;
}



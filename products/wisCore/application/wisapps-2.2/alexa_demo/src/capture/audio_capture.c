//
/*  
 * audio_capture.c , 2016/07/30 , rak University , China 
 * 	Author: Sevencheng
 */  
#include <stdio.h>  
#include <stdlib.h>  
#include <alsa/asoundlib.h>  
#include "audio_capture.h"

#define CHUNK_SIZE  4000
//#define SAVE_RAW_DATA

#define SKIP_STEP 4

int steroConverMono(char *src, size_t srcLen, char *dst)
{	
	unsigned int change;
	int i, interval = srcLen/SKIP_STEP;	
	int dst_size=0;
	
	for(i=0; i<interval; i++){		
		change = *(unsigned int *)(src+(i*SKIP_STEP));
		*dst++ = (char)change;		
		*dst++ = (char)(change>>8);		
		dst_size += 2;	
	}
	
	return dst_size;
}

int monoConverStero(char *src, size_t srcLen, char *dst)
{	
	unsigned int change;
	int i, interval = srcLen/SKIP_STEP;	
	int dst_size=0;	
	for(i=0; i<interval; i++){		
		change = *(unsigned int *)(src+(i*SKIP_STEP));
		*dst++ = (char)change;		
		*dst++ = (char)(change>>8);		
		*dst++ = (char)change;		
		*dst++ = (char)(change>>8);		
		*dst++ = (char)(change>>16);
		*dst++ = (char)(change>>24);
		*dst++ = (char)(change>>16);
		*dst++ = (char)(change>>24);
		dst_size += 8;	
	}
	
	return dst_size;
}


int RK_InitCaptureParam(S_AUDIO_PARAM *ps_AudioParam)
{
	char 				*filename	= "/tmp/16k.raw";
	char 				*devname	= "default";
	S_AUDIO_SPEC		*hwparams 	= &ps_AudioParam->hwparams, 
						*rhwparams 	= &ps_AudioParam->rhwparams;
	S_CTRL_PARAM		*psCtrlParams = &ps_AudioParam->asCtrlParams;
	S_AU_STREAM_PRIV	*psAuStream = &ps_AudioParam->hwparams.sAuStreamPriv;

	memset(hwparams, 0, sizeof(S_AUDIO_SPEC));
	memset(psCtrlParams, 0, sizeof(S_CTRL_PARAM));
	psCtrlParams->avail_min = -1;

	ps_AudioParam->eStream = SND_PCM_STREAM_CAPTURE;
	ps_AudioParam->iAuType = FORMAT_RAW;
	ps_AudioParam->filename = filename;
	ps_AudioParam->mmap_flag = 1;	/* must be enable for mt7628 board*/
	ps_AudioParam->strDrvName = devname;
	rhwparams->format = SND_PCM_FORMAT_S16_LE;
	rhwparams->rate = 16000;	//DEFAULT_SPEED;
	rhwparams->channels = 2;	/* if 8k must be set stereo for mt7628 board */

	RH_AudioGetParams(ps_AudioParam, psAuStream);

	return 0;
}

int RK_CaptureStreamSavefile(snd_pcm_t *handle, S_AUDIO_PARAM *ps_AudioParam, size_t captureBytes)
{
	S_AU_STREAM_PRIV *psAuStream = &ps_AudioParam->hwparams.sAuStreamPriv;
	int fd;
	off64_t count, rest;		/* number of bytes to capture */
	size_t chunk_bytes = psAuStream->uiBufbytes;
	size_t bits_per_frame = psAuStream->uiBits_per_frame;
	u_char *audiobuf = NULL;
	u_char *monoBuf	= NULL;
	int monoSize;

	/* open a new file */
	remove(ps_AudioParam->filename);
	fd = RH_AudioSafeOpenFile(ps_AudioParam->filename);
	if (fd < 0) {
		perror(ps_AudioParam->filename);
		prg_exit(handle, EXIT_FAILURE);
	}
	count = (off64_t)captureBytes;
	rest = count;
	audiobuf = (u_char *)malloc(chunk_bytes);
	monoBuf = (u_char *)malloc(chunk_bytes/2);
	
	/* repeat the loop when format is raw without timelimit or
	* requested counts of data are recorded
	*/
	/* capture */
	
	while (rest > 0) {
		size_t c = (rest <= (off64_t)chunk_bytes) ?
			(size_t)rest : chunk_bytes;
		size_t f = c * 8 / bits_per_frame;
		if (RH_AudioPcmRead(handle, audiobuf, f) != f)
			break;
		/* start stero conver mono */
		monoSize = steroConverMono((char *)audiobuf, c, (char *)monoBuf);
		
		if (write(fd, monoBuf, monoSize) != monoSize) {
			perror(ps_AudioParam->filename);
			prg_exit(handle, EXIT_FAILURE);
		}
		count -= c;
		rest -= c;
	}

	free(audiobuf);
	free(monoBuf);
	close(fd);

	return 0;
}



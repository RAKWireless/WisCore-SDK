//dec_raw.c
//by sevencheng 2017-01-09
//dec_raw.h
//by sevencheng
#ifndef __DEC_RAW_H__
#define __DEC_RAW_H__
#include <semaphore.h>
#include <time.h>

#include "utils.h"
#include "audio_queue.h"
#include "audio_ao.h"

#define BUFSIZE 9216	/*1152*8 frames*/
#define PKSIZE 16384

typedef struct _rawattr {  
	FILE 			*fp;
	FILE 			*save_pcm_fp;
	int				default_driver;
	int				header_size;//the header of size at audio file.
	int				replaytimes;
	ao_device 		*device;
	bool 			isExceptAbort;		/* take initiative to request playback abort */
	bool			bPause;
	float           f16Volume;			/*  value rang: 0.1~1 */
	int 			isLastpkt;
    unsigned int 	flen; 				/* file or audiodata length */
    unsigned int 	fpos; 				/* current position */
    unsigned char 	fbuf[BUFSIZE]; 		/* buffer*/
    unsigned int 	fbsize; 			/* indeed size of buffer */
	S_AVPacket 		*apkt;				/* current use audio packet */
	PacketQueue 	*ptrRawQueue;			/* current queue */
	sem_t			m_SemRawDone;
} S_RAWATTR;

S_RAWATTR *RK_RAW_PcmAttr_alloc(void);
int RK_RAWInitParams(S_RAWATTR *s, int default_driver);
int RK_RAWRun_pcms16le(S_RAWATTR *pRawattr, int smplerate, const char *filename, time_t stoptime);
int RK_RAWClose_stream(S_RAWATTR *pRawattr);
int RK_RAWPause_stream(S_RAWATTR *pRawattr, bool pause);
int RK_RAWAdjust_volume(S_RAWATTR *pRawattr, float f16Vol);
int RK_RAWWrite(S_RAWATTR *pRawattr, S_AVPacket *pkt);

#endif

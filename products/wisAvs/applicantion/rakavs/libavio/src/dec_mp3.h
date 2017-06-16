//dec_mp3.h
//by sevencheng
#ifndef __DEC_MP3_H__
#define __DEC_MP3_H__
#include <semaphore.h>
#include <time.h>

#include "utils.h"
#include "audio_queue.h"
#include "audio_ao.h"

#define BUFSIZE 9216	/*1152*8 frames*/
#define PKSIZE 16384

typedef struct decode_packet {
	int 			chunck_size;	/* the size of a request pcm(dev) data */
	int				padding_size;	/* remaing request the size of the valid data */
	int				unproc_size;	/* legacy data size */
	int				current_pos;	/* outbuf current position */
	unsigned char	*outbuf;		/* pcm data */
} S_DecPacket;

typedef struct _mp3attr {  
	FILE 			*fp;
	FILE 			*save_pcm_fp;
	time_t			stoptime;
	int				replaytimes;
	char			*fileName;
	int				default_driver;
	ao_device 		*device;
	bool 			isExceptAbort;		/* take initiative to request playback abort */
	bool			bPause;
	float           f16Volume;			/*  value rang: 0.1~1 */
	int 			isLastpkt;
    unsigned int 	flen; 				/* file or audiodata length */
    unsigned int 	fpos; 				/* current position */
    unsigned char 	fbuf[BUFSIZE]; 		/* buffer*/
    unsigned int 	fbsize; 			/* indeed size of buffer */
	S_DecPacket		sDecPkt;
	S_AVPacket 		*apkt;				/* current use audio packet */
	PacketQueue 	*ptrQueue;			/* current queue */
	sem_t			m_Mp3Done;
//	void *privatedata;
} S_MP3ATTR;

int RK_MADRun_decoder(S_MP3ATTR *pMp3attr);
int RK_MADPackageDecodec_init(S_DecPacket *decPkt);
int RK_MADInitParams(S_MP3ATTR *pMp3attr, int default_driver);
int RK_MADPause_stream(S_MP3ATTR *pMp3attr, bool pause);
S_MP3ATTR *RK_MAD_Mp3Attr_alloc(void);
int RK_MADInit(void);
int RK_MADClose_stream(S_MP3ATTR *pMp3attr);
int RK_MADAdjust_volume(S_MP3ATTR *pMp3attr, float f16Vol);
int RK_MADWrite(S_MP3ATTR *pMp3attr, S_AVPacket *pkt);

#endif	//__DEC_MP3_H__

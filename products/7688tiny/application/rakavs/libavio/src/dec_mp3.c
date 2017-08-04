//dec_mp3.c
//by seven
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <math.h>
#include "mad.h"

#include "audio_debug.h"
#include "dec_mp3.h"


//pthread_mutex_t 	g_sMutexMp3Done = PTHREAD_MUTEX_INITIALIZER;


//write data to audio_buf from audio_queue(data)
int player_getAudioPkt4(void *data, unsigned char *audio_buf, int max_len)
{
	S_MP3ATTR *pMp3attr = (S_MP3ATTR *)data;
	unsigned char *pbuf;
	S_AVPacket 	*packet = pMp3attr->apkt;				//For encodec data
	int pkt_size, pkt_pos;	/* pkt_pos: current pbuf of position, pkt_size: request frame size */
	int ret;
	
	pbuf = (unsigned char *)audio_buf;
	pkt_size = max_len;
	pkt_pos = 0;

	/* laster packet */
	if(packet->lastpkt == 1){
		if(pMp3attr->flen < pkt_size){
			memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pMp3attr->flen);
			max_len = pMp3attr->flen;
			pMp3attr->fpos = 0;
			pMp3attr->flen = 0;
			RH_AVFree_Packet(packet);
		}else{
			memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pkt_size);
			pMp3attr->fpos += pkt_size;
			pMp3attr->flen -= pkt_size;
			pkt_size = 0;
		}
		
		return max_len;
	}
	
	do{
		if(pMp3attr->flen < pkt_size){
			if(packet->data != NULL){	
				memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pMp3attr->flen);
				pkt_size -= pMp3attr->flen;
				pkt_pos += pMp3attr->flen;	/* fix me: avoid multiple read a queue alway can not fully feed casue stream be dropped*/
				pMp3attr->fpos = 0;
				RH_AVFree_Packet(packet);
			}
	
			ret = RK_AVQueue_Packet_Get(pMp3attr->ptrQueue, packet, 1);
			if(ret <= 0){
				max_len = pMp3attr->flen;	/* return remain framer size */
				pMp3attr->flen = 0;
				pMp3attr->fpos = 0;
				//pMp3attr->isLastpkt = 1;
				packet->data = NULL;
				packet->size = 0;
				//RH_AVFree_Packet(packet);
				AUDIO_DBG("No framer %d \n", max_len);
				break;
			}else if(packet->lastpkt == 1){
				pMp3attr->flen = packet->size;
				if(pMp3attr->flen >= pkt_size){
					memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pkt_size);
					pMp3attr->fpos += pkt_size;
					pMp3attr->flen -= pkt_size;
					pkt_size = 0;
				}else{
					memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pMp3attr->flen);
					pMp3attr->fpos += pMp3attr->flen;
					pMp3attr->flen -= pMp3attr->flen;
					pkt_size -= pMp3attr->flen;
					max_len -= pkt_size;		/* real vaild data */
#ifdef LASTER_PACK_NO_QUIT
					RH_AVFree_Packet(packet);
					if(max_len == 0)	/*temp*/
						max_len = 1;
#endif
				}
				break;
			}
			pMp3attr->isLastpkt = packet->lastpkt;
			pMp3attr->flen = packet->size;
		}else{
			memcpy(pbuf+pkt_pos, packet->data+pMp3attr->fpos, pkt_size);
			pMp3attr->fpos += pkt_size;
			pMp3attr->flen -= pkt_size;
			pkt_size = 0;
		}

	}while(pkt_size);

	return max_len;	
}

static enum mad_flow mad_InputStream (void *data, struct mad_stream *stream)  
{  
	S_MP3ATTR *pMp3attr = (S_MP3ATTR *)data;
	int ret_code;
	int copy_size;
	int unproc_size;
	int readSize = 0;

	if(pMp3attr->stoptime?(pMp3attr->stoptime<time(NULL)):0){
		usleep(1000);
		ret_code = MAD_FLOW_STOP;
		printf("input sem_post\n");
		return ret_code;
	}
	
	unproc_size = stream->bufend - stream->next_frame;
	memcpy(pMp3attr->fbuf, pMp3attr->fbuf + pMp3attr->fbsize - unproc_size, unproc_size);
	
	copy_size = BUFSIZE - unproc_size;
//	printf("+++++++readSize:%d-%d\n", readSize, pMp3attr->isExceptAbort);
	if(!pMp3attr->isExceptAbort){
		if(pMp3attr->fileName && pMp3attr->fp){
			readSize = fread(pMp3attr->fbuf+unproc_size, 1, copy_size, pMp3attr->fp);
			if(readSize <= 0){
				if(pMp3attr->replaytimes==1){
					ret_code = MAD_FLOW_STOP;
					return ret_code;
				}
				if(pMp3attr->replaytimes>1)
					pMp3attr->replaytimes--;
				(void)fseek(pMp3attr->fp, 0L, SEEK_SET);
				readSize = fread(pMp3attr->fbuf+unproc_size, 1, copy_size, pMp3attr->fp);
			}
		}else{
			readSize = player_getAudioPkt4((void *)pMp3attr, pMp3attr->fbuf+unproc_size, copy_size);
		}
	}
	//printf("readSize:%d\n", readSize);
//	printf("-------readSize:%d-%d\n", readSize, pMp3attr->isExceptAbort);
	if(readSize > 0){	
		pMp3attr->fbsize = unproc_size + readSize;				
		mad_stream_buffer(stream, pMp3attr->fbuf, pMp3attr->fbsize);		
		ret_code = MAD_FLOW_CONTINUE;	
	}
	else{			
		usleep(1000);	
		ret_code = MAD_FLOW_STOP;
		AUDIO_INFO("Input Mp3 stream is empty!\n");
	}
		
    return ret_code;
}

static inline signed int scale (mad_fixed_t sample)  
{  
    sample += (1L << (MAD_F_FRACBITS - 16));  
    if (sample >= MAD_F_ONE)  
        sample = MAD_F_ONE - 1;  
    else if (sample < -MAD_F_ONE)  
        sample = -MAD_F_ONE;  
    return sample >> (MAD_F_FRACBITS + 1 - 16);  
} 

static enum mad_flow mad_OutputStream (void *data, struct mad_header const *header, struct mad_pcm *pcm)  
{ 
	S_MP3ATTR 		*pMp3attr = (struct _mp3attr *)data;
	S_DecPacket		*pDecPkt = &pMp3attr->sDecPkt;
	//unsigned char	*OutputPtr = pDecPkt->outbuf;
    mad_fixed_t const 	*left_ch, *right_ch;
	unsigned int 	nchannels, 
					nsamples,
					smplerate = 24000,
					n;
	int chk_size = pDecPkt->chunck_size;
	//int cur_pos	= pDecPkt->current_pos;
	int pad_size = pDecPkt->padding_size;
	int unproc_size;
	int op_pos;
	int sambytes;
    unsigned char Output[4608], *OutputPtr;  
	unsigned char sterobuf[4608], *steroBufPtr;
		 
    nchannels = pcm->channels;  	/*1-mono, 2-stero*/
    n = nsamples = pcm->length;  	/* chunck_size 1152 for stero, 576 for mono*/
	smplerate = pcm->samplerate;
    left_ch = pcm->samples[0];  	//
    right_ch = pcm->samples[1];
	sambytes = n*nchannels*2;
	
	OutputPtr = Output;
	while (nsamples--)  
    {  
        signed int sample;  
        sample = scale (*left_ch++);  
        *(OutputPtr++) = sample >> 0;  
        *(OutputPtr++) = sample >> 8;  
        if (nchannels == 2)  /* stero support */
        { 
            sample = scale (*right_ch++);  
            *(OutputPtr++) = sample >> 0;  
            *(OutputPtr++) = sample >> 8;  
        }  
    }

	if(pMp3attr->device == NULL){
		pMp3attr->device = RK_AODev_open(pMp3attr->default_driver, smplerate);
		if(pMp3attr->device == NULL){	/* fix me: we will not playback handle if open dev failure*/
			AUDIO_ERROR("ao_open_live is failure!\n");
			return MAD_FLOW_CONTINUE;
		}
	}

	if(nchannels == 1){/*mono*/
		steroBufPtr = sterobuf;
		sambytes = RK_AO_MonoConverStero((unsigned char *)Output, n*2, sterobuf);	
	}else{/*stero*/
		steroBufPtr = Output;
	}
	unproc_size = sambytes;
	if(unproc_size >= pad_size){
		
		memcpy(pDecPkt->outbuf+pDecPkt->current_pos, steroBufPtr, pad_size);
		RK_AOAdjustVolume((char *)pDecPkt->outbuf, chk_size, pMp3attr->f16Volume);
		ao_play(pMp3attr->device, (char *)pDecPkt->outbuf, chk_size);
		unproc_size -= pad_size;
		op_pos = pad_size;
		if(unproc_size >= chk_size){
			memcpy(pDecPkt->outbuf, steroBufPtr+op_pos, chk_size);
			RK_AOAdjustVolume((char *)pDecPkt->outbuf, chk_size, pMp3attr->f16Volume);
			ao_play(pMp3attr->device, (char *)pDecPkt->outbuf, chk_size);
			unproc_size -= chk_size;
			op_pos += chk_size;
		}
		
		memcpy(pDecPkt->outbuf, steroBufPtr+op_pos, unproc_size);
		pad_size = chk_size - unproc_size;
		pDecPkt->current_pos = unproc_size;
	}else{
		memcpy(pDecPkt->outbuf+pDecPkt->current_pos, steroBufPtr, unproc_size);
		pad_size -= unproc_size;
		pDecPkt->current_pos += unproc_size;
	}

	pDecPkt->padding_size = pad_size;

	do{
		if(pMp3attr->isExceptAbort){
			AUDIO_INFO("Output request except abort!\n");
		  return MAD_FLOW_STOP;	
	    }
		
		if(!pMp3attr->bPause)
			break;
		/* we will block if pause stay in*/
		usleep(10*1000);
	}while(1);

	//fwrite(Output, 1, n*2*nchannels, pMp3attr->save_pcm_fp);	
	//fwrite(steroBufPtr, 1, sambytes, pMp3attr->save_pcm_fp);	
	return MAD_FLOW_CONTINUE;	
}

static enum mad_flow error (void *data, struct mad_stream *stream, struct mad_frame *frame)  
{  
    return MAD_FLOW_CONTINUE;  
}  

int RK_MADRun_decoder(S_MP3ATTR *pMp3attr)
{
	struct mad_decoder 	decoder;
	pMp3attr->fp = NULL;
	
	AUDIO_DBG("MP3 Start dec\n");
	
	if(pMp3attr->fileName != NULL){
		pMp3attr->fp = fopen(pMp3attr->fileName, "rb");
		if(pMp3attr->fp == NULL){
			AUDIO_ERROR("open file stream <%s> is failed!\n", pMp3attr->fileName);
			return -1;
		}
	}
	
	mad_decoder_init (&decoder, pMp3attr, mad_InputStream, 0, 0, mad_OutputStream, error, 0);  
    mad_decoder_options (&decoder, 0);
    mad_decoder_run (&decoder, MAD_DECODER_MODE_SYNC);
	
	AUDIO_DBG("MP3 End dec\n");
    mad_decoder_finish (&decoder); 
	sem_post(&pMp3attr->m_Mp3Done);

	return 0;
}

S_MP3ATTR *RK_MAD_Mp3Attr_alloc(void)
{
	S_MP3ATTR *s = (S_MP3ATTR *)malloc(sizeof(S_MP3ATTR));;
	if(s)
		memset(s, 0, sizeof(S_MP3ATTR));
	return s;
}

int RK_MADPackageDecodec_init(S_DecPacket *decPkt)
{
	decPkt->chunck_size = 4000;
	decPkt->padding_size = 4000;
	decPkt->current_pos = 0;
	decPkt->unproc_size = 0;

	if(!decPkt->outbuf){
		decPkt->outbuf = (unsigned char *)malloc(decPkt->chunck_size);
		if(!decPkt->outbuf)
			return -1;	/* failed to alloc mem */
	}
	
	return 0;
}
/*mark seven*/
int RK_MADInitParams(S_MP3ATTR *pMp3attr, int default_driver)
{
	pMp3attr->apkt = (S_AVPacket *)malloc(sizeof(S_AVPacket));
	if(pMp3attr->apkt == NULL){
		fprintf(stderr, "mp3 memcalloc failure\n");
		return -1;
	}
	memset(pMp3attr->apkt, 0, sizeof(S_AVPacket));

	pMp3attr->ptrQueue = (PacketQueue *)malloc(sizeof(PacketQueue));
	if(pMp3attr->ptrQueue == NULL){
		fprintf(stderr, "mp3 queue memcalloc failure\n");
		return -1;
	}

	pMp3attr->default_driver = default_driver;

	sem_init(&pMp3attr->m_Mp3Done, 0, 0);
	RK_AVQueue_Init(pMp3attr->ptrQueue);
	RK_MADPackageDecodec_init(&pMp3attr->sDecPkt);

	return 0;
}

int RK_MADInit(void)
{
#if 0
	if(pthread_mutex_init(&g_sMutexMp3Done, NULL) != 0)	{
		AVS_ERROR("ERROR: Could not initialize mutex variable\n");
		return -1;
	}

#endif
	return 0;
}

/* maybe lock*/
int RK_MADClose_stream(S_MP3ATTR *pMp3attr)
{

	//pthread_mutex_lock(&g_sMutexMp3Done);

	AUDIO_TRACE("< Entry! >\n"); 

	if(pMp3attr->apkt == NULL){
		//pthread_mutex_unlock(&g_sMutexMp3Done);
		AUDIO_WARNING("pMp3attr->apkt is NULL,  playhandle is already closed\n");
		return 0;
	}
	
	pMp3attr->isExceptAbort = true;
	RK_AVQueue_Set_Quit(pMp3attr->ptrQueue);
	sem_wait(&pMp3attr->m_Mp3Done);
	RK_AVQueue_Packet_Flush(pMp3attr->ptrQueue);

	if(pMp3attr->apkt){
		free(pMp3attr->apkt);
		pMp3attr->apkt = NULL;
	}
	
	if(pMp3attr->ptrQueue){
		free(pMp3attr->ptrQueue);
		pMp3attr->ptrQueue = NULL;
	}
	if(pMp3attr->fp){
		fclose(pMp3attr->fp);
		pMp3attr->fp = NULL;
	}

	if(pMp3attr->save_pcm_fp){
		fclose(pMp3attr->save_pcm_fp);
		pMp3attr->save_pcm_fp = NULL;
	}

	if(pMp3attr->sDecPkt.outbuf){
		free(pMp3attr->sDecPkt.outbuf);
		pMp3attr->sDecPkt.outbuf = NULL;
	}
	
	AUDIO_DBG("Close MP3 handle!\n");	
	/* -- Close and shutdown -- */
	if(pMp3attr->device)
	ao_close(pMp3attr->device);
	pMp3attr->device = NULL;
	
	sem_destroy(&pMp3attr->m_Mp3Done);
#if 0
	// self free
	if(pMp3attr){
		AUDIO_INFO("release decodec...............\n");
		free(pMp3attr);
		pMp3attr = NULL;
	}
#endif	
	//pthread_mutex_unlock(&g_sMutexMp3Done);
	
	AUDIO_TRACE("< Exit! >\n");	
	return 0;
}

int RK_MADPause_stream(S_MP3ATTR *pMp3attr, bool pause)
{
	pMp3attr->bPause = pause;
	return 0;
}

int RK_MADAdjust_volume(S_MP3ATTR *pMp3attr, float f16Vol)
{
	pMp3attr->f16Volume = f16Vol;

	return 0;
}

int RK_MADWrite(S_MP3ATTR *pMp3attr, S_AVPacket *pkt)
{
	return RK_AVQueue_Packet_Put(pMp3attr->ptrQueue, pkt, 1);	/* write queue */
}



//dec_raw.c
//by seven 2017-01-10
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
#include "dec_raw.h"

//pthread_mutex_t 	g_sMutexMp3Done = PTHREAD_MUTEX_INITIALIZER;


//从音频队列取一个包写入到audio_buf
int player_getRawAudioPkt4(void *data, unsigned char *audio_buf, int max_len)
{
	S_RAWATTR *pRawAttr = (S_RAWATTR *)data;
	unsigned char *pbuf;
	S_AVPacket 	*packet = pRawAttr->apkt;				//For encodec data
	int pkt_size, pkt_pos;	/* pkt_pos: current pbuf of position, pkt_size: request frame size */
	int ret;
	
	pbuf = (unsigned char *)audio_buf;
	pkt_size = max_len;
	pkt_pos = 0;

	/* laster packet */
	if(packet->lastpkt == 1){
		if(pRawAttr->flen < pkt_size){
			memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pRawAttr->flen);
			max_len = pRawAttr->flen;
			pRawAttr->fpos = 0;
			pRawAttr->flen = 0;
			RH_AVFree_Packet(packet);
		}else{
			memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pkt_size);
			pRawAttr->fpos += pkt_size;
			pRawAttr->flen -= pkt_size;
			pkt_size = 0;
		}
		
		return max_len;
	}
	
	do{
		if(pRawAttr->flen < pkt_size){
			if(packet->data != NULL){	
				memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pRawAttr->flen);
				pkt_size -= pRawAttr->flen;
				pkt_pos += pRawAttr->flen;	/* fix me: avoid multiple read a queue alway can not fully feed casue stream be dropped*/
				pRawAttr->fpos = 0;
				RH_AVFree_Packet(packet);
			}
	
			ret = RK_AVQueue_Packet_Get(pRawAttr->ptrRawQueue, packet, 1);
			if(ret <= 0){
				max_len = pRawAttr->flen;	/* return remain framer size */
				pRawAttr->flen = 0;
				pRawAttr->fpos = 0;
				//pRawAttr->isLastpkt = 1;
				packet->data = NULL;
				packet->size = 0;
				//RH_AVFree_Packet(packet);
				AUDIO_DBG("No framer %d \n", max_len);
				break;
			}else if(packet->lastpkt == 1){
				pRawAttr->flen = packet->size;
				if(pRawAttr->flen >= pkt_size){
					memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pkt_size);
					pRawAttr->fpos += pkt_size;
					pRawAttr->flen -= pkt_size;
					pkt_size = 0;
				}else{
					memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pRawAttr->flen);
					pRawAttr->fpos += pRawAttr->flen;
					pRawAttr->flen -= pRawAttr->flen;
					pkt_size -= pRawAttr->flen;
					max_len -= pkt_size;		/* real vaild data */
#ifdef LASTER_PACK_NO_QUIT
					RH_AVFree_Packet(packet);
					if(max_len == 0)	/*temp*/
						max_len = 1;
#endif					
				}
				break;
			}
			pRawAttr->isLastpkt = packet->lastpkt;
			pRawAttr->flen = packet->size;
		}else{
			memcpy(pbuf+pkt_pos, packet->data+pRawAttr->fpos, pkt_size);
			pRawAttr->fpos += pkt_size;
			pRawAttr->flen -= pkt_size;
			pkt_size = 0;
		}

	}while(pkt_size);

	return max_len;	
}

int RK_RAWRun_pcms16le(S_RAWATTR *pRawattr, int smplerate, const char *filename, time_t stoptime)
{
	int ret = 0;
	unsigned char iSfilestream = 0;
	FILE *stream_fp = NULL;
//	int replaytimes = 0;
	if(filename != NULL){
		iSfilestream = 1;
		stream_fp = fopen(filename, "rb");
		if(stream_fp == NULL){
			AUDIO_ERROR("open file stream <%s> is failed!\n", filename);
			return -1;
		}
	}

	if(pRawattr->device == NULL){
		pRawattr->device = RK_AODev_open(pRawattr->default_driver, smplerate);
		if(pRawattr->device == NULL){	/* fix me: we will not playback handle if open dev failure*/
			AUDIO_ERROR("ao_open_live is failure!\n");
			return -1;
		}		
	}
	
	do{

	size_t	readsize = 0;
	
	if(stoptime?stoptime < time(NULL):0){
		ret = 0;
		break;
	}
	
	if(pRawattr->bPause){
		if(pRawattr->isExceptAbort)
			break;
		usleep(20*1000);
		continue;
	}
	
	if(!pRawattr->isExceptAbort){
		if(iSfilestream){
			readsize = fread(pRawattr->fbuf, 1, 4000, stream_fp);
			if(readsize <= 0){
	//			replaytimes++;
				if(pRawattr->replaytimes==1)
					break;
				if(pRawattr->replaytimes>1)
					pRawattr->replaytimes--;
				(void)fseek(stream_fp, pRawattr->header_size, SEEK_SET);
				continue;
			}
		}else
			readsize = player_getRawAudioPkt4((void *)pRawattr, pRawattr->fbuf, 4000);
	}
	
	if(readsize > 0){
		
		RK_AOAdjustVolume((char *)pRawattr->fbuf, readsize, pRawattr->f16Volume);
		
		ret = ao_play(pRawattr->device, (char *)pRawattr->fbuf, readsize);
		if(ret <0){
			AUDIO_ERROR("****************************************************\n"
						 "*******raw_audio <ao_play> encounter accident*******\n"
						 "****************************************************\n");
			break;
		 }

	}else{
		AUDIO_INFO("raw_audio playback is end!\n");
		break;
	}
	
	}while(1);

	sem_post(&pRawattr->m_SemRawDone);
	
	return ret;
}

S_RAWATTR *RK_RAW_PcmAttr_alloc(void)
{
	S_RAWATTR *s = (S_RAWATTR *)malloc(sizeof(S_RAWATTR));
	if(s)
		memset(s, 0, sizeof(S_RAWATTR));
	
	return s;
}

/*mark seven*/
int RK_RAWInitParams(S_RAWATTR *s, int default_driver)
{
	s->apkt = (S_AVPacket *)malloc(sizeof(S_AVPacket));
	if(s->apkt == NULL){
		fprintf(stderr, "raw attr memcalloc failure\n");
		return -1;
	}
	memset(s->apkt, 0, sizeof(S_AVPacket));

	s->ptrRawQueue = (PacketQueue *)malloc(sizeof(PacketQueue));
	if(s->ptrRawQueue == NULL){
		fprintf(stderr, "raw queue memcalloc failure\n");
		return -1;
	}

	s->default_driver = default_driver;
	
	sem_init(&s->m_SemRawDone, 0, 0);
	RK_AVQueue_Init(s->ptrRawQueue);

	return 0;
}

/* maybe lock*/
int RK_RAWClose_stream(S_RAWATTR *pRawattr)
{

	//pthread_mutex_lock(&g_sMutexMp3Done);
	AUDIO_TRACE("< Entry! >\n");	

	if(pRawattr->apkt == NULL){
		//pthread_mutex_unlock(&g_sMutexMp3Done);
		AUDIO_WARNING("pRawattr->apkt is NULL, playhandle is already closed\n");
		return 0;
	}
	
	pRawattr->isExceptAbort = true;
	RK_AVQueue_Set_Quit(pRawattr->ptrRawQueue);
	sem_wait(&pRawattr->m_SemRawDone);
	RK_AVQueue_Packet_Flush(pRawattr->ptrRawQueue);

	if(pRawattr->apkt){
		free(pRawattr->apkt);
		pRawattr->apkt = NULL;
	}
	
	if(pRawattr->ptrRawQueue){
		free(pRawattr->ptrRawQueue);
		pRawattr->ptrRawQueue = NULL;
	}
	if(pRawattr->fp)
		fclose(pRawattr->fp);

	if(pRawattr->save_pcm_fp){
		fclose(pRawattr->save_pcm_fp);
		pRawattr->save_pcm_fp = NULL;
	}

	/* -- Close and shutdown -- */
	if(pRawattr->device)
	ao_close(pRawattr->device);
	pRawattr->device = NULL;
	
	sem_destroy(&pRawattr->m_SemRawDone);

	//pthread_mutex_unlock(&g_sMutexMp3Done);
	
	AUDIO_TRACE("< Exit! >\n");	
	return 0;
}

int RK_RAWPause_stream(S_RAWATTR *pRawattr, bool pause)
{
	pRawattr->bPause = pause;
	return 0;
}

int RK_RAWAdjust_volume(S_RAWATTR *pRawattr, float f16Vol)
{
//	printf("+++*********************volume:%f, %f\n", pRawattr->f16Volume, f16Vol);
	pRawattr->f16Volume = f16Vol;
//	printf("---*********************volume:%f, %f\n", pRawattr->f16Volume, f16Vol);

	return 0;
}

int RK_RAWWrite(S_RAWATTR *pRawattr, S_AVPacket *pkt)
{
	return RK_AVQueue_Packet_Put(pRawattr->ptrRawQueue, pkt, 1);	/* write queue */
}



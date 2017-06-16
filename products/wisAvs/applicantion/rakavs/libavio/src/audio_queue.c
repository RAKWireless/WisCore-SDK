//audio_queue.h
//by seven
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include "audio_queue.h"

#define DEF_INQUEUE_THRESHOLD_BYTES		8*1024*1024

int queue_quit = 0;  
//队列初始化
int RK_AVQueue_Init(PacketQueue *q) {
	memset(q, 0, sizeof(PacketQueue));
	q->thresholdSize = DEF_INQUEUE_THRESHOLD_BYTES;	
	// this mutex and conditional variable are used to synchronize access to the s_sGlobal resource data
	if(pthread_mutex_init(&q->mutex, NULL) != 0)
	{
		printf("ERROR: Could not initialize mutex variable\n");
		return -1;
	}

	if(pthread_cond_init(&q->cond, NULL) != 0)
	{
		printf("ERROR: Could not initialize condition variable\n");
		return -1;
	}

	return 0;
}

void RH_AVFree_Packet(S_AVPacket *pkt)
{
    if (pkt) {
        if (pkt->data)
            free(pkt->data);

        pkt->data            = NULL;
        pkt->size            = 0;
		pkt->lastpkt		 = 0;
    }
}

void RH_AVFreep(void *arg)
{
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){ NULL }, sizeof(val));
    free(val);
}

//向队列放一个包
int RK_AVQueue_Packet_Put(PacketQueue *q, S_AVPacket *pkt, int block) {
	S_AVPacketList *pkt1;

	if(!pkt)
		return -1;
//add queue buffer threshold @2017-0210
	/*make sure enqueue data not overflow*/
	if((q->size+pkt->size) > DEF_INQUEUE_THRESHOLD_BYTES)
		return -7;
	
	pkt1 = malloc(sizeof(S_AVPacketList));
	if (!pkt1)
		return -1;
	
	pkt1->pkt = *pkt;
	pkt1->next = NULL;
	pthread_mutex_lock(&q->mutex);
	
	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	/* signal fresh_inform */
	if(block){
		//pthread_cond_broadcast(&psAlexaHnd->aCondUpdate);
		pthread_cond_signal(&q->cond);
	}
	pthread_mutex_unlock(&q->mutex);
	return 0;
}
//从队列取一个包
int RK_AVQueue_Packet_Get(PacketQueue *q, S_AVPacket *pkt, int block) {
	S_AVPacketList *pkt1;

	struct timeval now;  
    struct timespec abstime;  
	
	int ret;

	pthread_mutex_lock(&q->mutex);
	for (;;) 
	{
		if (q->queue_quit) 
		{
			printf("Queue packet get exit!\n");
			ret = -1;
			break;
		}
		
		pkt1 = q->first_pkt;
		if (pkt1) 
		{
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			gettimeofday(&now, NULL);  
    		abstime.tv_sec = now.tv_sec + 5;	//timeout = 30 seconds  
    		abstime.tv_nsec = now.tv_usec * 1000;
			//printf("dialog mp3 queque cont_timewait timeout\n");
			ret = pthread_cond_timedwait(&q->cond, &q->mutex, &abstime);
			if(ret == ETIMEDOUT){
				printf("audio voice queque cont_timewait_timeout - %d\n", q->queue_quit);
			
			}
		}
	}
	pthread_mutex_unlock(&q->mutex);

	return ret;
}

void RK_AVQueue_Packet_Flush(PacketQueue *q) {
	S_AVPacketList *pkt, *pkt1;

	if(q == NULL)
		return;
	
	pthread_mutex_lock(&q->mutex);
	
	for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		RH_AVFree_Packet(&pkt->pkt);
	//	RH_AVFreep(&pkt);
		free(pkt);
	}
	
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	pthread_mutex_unlock(&q->mutex);
}

void RK_AVQueue_Set_Quit(PacketQueue *q){
	pthread_mutex_lock(&q->mutex);
	    //printf("RK_Queue_Set_Quit Wake q=%d...\n", q->queue_quit);
	if(q != NULL){
		q->queue_quit = 1;
	    //printf("RK_Queue_Set_Quit Wake q=%d...\n", q->queue_quit);
		pthread_cond_signal(&q->cond);
	}
	pthread_mutex_unlock(&q->mutex);
}


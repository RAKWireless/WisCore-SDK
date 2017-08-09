//audio_queue.h
//by seven
#ifndef __AUDIO_QUEUE_H__
#define __AUDIO_QUEUE_H__

#include <pthread.h>

typedef struct av_packet {
	unsigned char 	*data;
	int 			size;
	int 			lastpkt;
} S_AVPacket;

typedef struct av_packetList {
    S_AVPacket pkt;
    struct av_packetList *next;
} S_AVPacketList;

typedef struct PacketQueue {
  S_AVPacketList 	*first_pkt, *last_pkt;
  int 				nb_packets;
  int 				size;
  int 				queue_quit;
  pthread_mutex_t 	mutex;
  pthread_cond_t  	cond;
} PacketQueue;

int RK_Queue_Init(PacketQueue *q);

void RH_Free_Packet(S_AVPacket *pkt);

int RK_Queue_Packet_Put(PacketQueue *q, S_AVPacket *pkt, int block);

int RK_Queue_Packet_Get(PacketQueue *q, S_AVPacket *pkt, int block);

void RK_Queue_Packet_Flush(PacketQueue *q);
#endif
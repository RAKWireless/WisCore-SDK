#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "RKLedio.h"

#define SWAP(A,B) ({if((A)!=(B)){ typeof(A) C; C=(A); A=(B); B=(C);}})

typedef struct timeval timeval_t;
typedef struct S_LED_CTX_t S_LED_CTX_t;
typedef struct single_thread{
	pthread_mutex_t pthread_mutex;
	int pthread_working;
	int pthread_quited;
	S_LED_CTX_t *led_ctx;
}single_thread_t;

struct S_LED_CTX_t{
	S_LED_INFO_t ledinfo;
	timeval_t tv;
	pthread_mutex_t ledoptmutex;
	S_LED_CTX_t * next;
	S_LED_CTX_t * prev;
	single_thread_t* parent;
	void *ledid;
	void *ledinit;
	int (*led_hardware_set_lumen)(void *ledid, int lumen);
	int (*led_hardware_destory)(void*ledid);
	int curlumen;
	int init_nums;
};
static void *ledThread(void *arg);

LED_t led_init(
	void* ledinit,
	void* (*led_hardware_init)(void* ledinit),
	int   (*led_hardware_set_lumen)(void* ledid, int lumen),
	int   (*led_hardware_destory)(void*ledid)
	)
{

	static single_thread_t single_thread = {PTHREAD_MUTEX_INITIALIZER,0,1,0};
	S_LED_CTX_t * led_ctx = NULL;
	void *ledid;

	if(pthread_mutex_lock(&single_thread.pthread_mutex) <0){
		goto led_mutex_error;
	}
	if(single_thread.led_ctx){
		led_ctx = single_thread.led_ctx;
		if(single_thread.led_ctx->ledinit == ledinit){
			led_ctx->init_nums++;
			goto led_fine;
		}
		led_ctx = led_ctx->next;
		while(led_ctx != single_thread.led_ctx){
			if(led_ctx->ledinit == ledinit){
				led_ctx->init_nums++;
				goto led_fine;
			}else{
				led_ctx = led_ctx->next;
			}
		}
	}
	if(!led_hardware_init || !led_hardware_set_lumen){
		goto led_hardware_init_error;
	}
	ledid = led_hardware_init(ledinit);
	if(ledid == NULL){
		goto led_hardware_init_error;
	}
	if(led_hardware_set_lumen(ledid, 0)<0){
		goto led_ctx_calloc_err;
	}
	led_ctx = calloc(1,sizeof(S_LED_CTX_t));
	if(!led_ctx){
		goto led_ctx_calloc_err;
	}
	if(pthread_mutex_init(&led_ctx->ledoptmutex, NULL)<0){
		goto led_ctx_calloc_err;
	}

	led_ctx->ledinit = ledinit;
	led_ctx->ledid = ledid;
	led_ctx->led_hardware_set_lumen = led_hardware_set_lumen;
	led_ctx->led_hardware_destory = led_hardware_destory;
	led_ctx->prev = led_ctx->next = led_ctx;
	int needcalldestroy = 0;
	if(!single_thread.led_ctx){
		needcalldestroy = 1;
		single_thread.led_ctx = led_ctx;
		led_ctx->init_nums++;
		led_ctx->parent = &single_thread;
	}else{
		S_LED_CTX_t *led_tail = single_thread.led_ctx->prev;
		pthread_mutex_lock(&led_tail->ledoptmutex);
		SWAP(led_tail->next, led_ctx->prev);
		SWAP(single_thread.led_ctx->prev, led_ctx->next);
		pthread_mutex_unlock(&led_tail->ledoptmutex);
	}
led_fine:
	if(!single_thread.pthread_working && single_thread.pthread_quited){
		pthread_t threadID;
		if(pthread_create(&threadID, NULL, ledThread, &single_thread) < 0){
			goto led_successed;
		}
		single_thread.pthread_quited=0;
		pthread_detach(threadID);
	}
led_successed:
	pthread_mutex_unlock(&single_thread.pthread_mutex);
	if(needcalldestroy)
		led_destory(led_ctx);
	return led_ctx;

led_ctx_calloc_err:
	if(led_ctx){
		free(led_ctx);
		led_ctx = NULL;
	}
	if(ledid)
		led_hardware_destory(ledid);
led_hardware_init_error:
	pthread_mutex_unlock(&single_thread.pthread_mutex);
led_mutex_error:
	return (void*)led_ctx;
}

int led_set(LED_t led, const S_LED_INFO_t* ledinfo)//0 on success, -1 on failed.
{
	int ret = 0;
	S_LED_CTX_t * led_ctx = (S_LED_CTX_t*)led;

	if(pthread_mutex_lock(&led_ctx->ledoptmutex) <0){
		ret = -1;
		goto led_set_quit;
	}

	int needswap = 0, sngstep = 1;
	switch(ledinfo->mode){
		case LED_BREATH:{
			if((led_ctx->ledinfo.breathstep ^ ledinfo->breathstep) < 0)
				sngstep = -1;
		}//no break
		case LED_FLASH:{
			if(ledinfo->sub_lumen > ledinfo->lumen)
				needswap = 1;

			if(ledinfo->mode == LED_FLASH){
				gettimeofday(&led_ctx->tv,NULL);
			}
			if(led_ctx->tv.tv_sec<0)
				led_ctx->tv.tv_sec=0;
		}//no break
		case LED_SIMPLE:{
			led_ctx->ledinfo = *ledinfo;
			if(ledinfo->mode == LED_FLASH || ledinfo->mode == LED_SIMPLE){
				led_ctx->led_hardware_set_lumen(led_ctx->ledid, ledinfo->lumen);
				led_ctx->curlumen = ledinfo->lumen;
			}
			if(needswap)
				SWAP(led_ctx->ledinfo.lumen, led_ctx->ledinfo.sub_lumen);
			if(sngstep<0)
				led_ctx->ledinfo.breathstep = -led_ctx->ledinfo.breathstep;
		}break;
		default:
			ret = -1;
			break;
	}
	pthread_mutex_unlock(&led_ctx->ledoptmutex);

led_set_quit:
	return ret;
}

S_LED_INFO_t led_get(LED_t led)
{
	S_LED_CTX_t * led_ctx = (S_LED_CTX_t*)led;
	S_LED_INFO_t ledinfo = {0,-1,-1,-1,-1,0};
	if(pthread_mutex_lock(&led_ctx->ledoptmutex) <0){
		return ledinfo;
	}
	ledinfo = led_ctx->ledinfo;
	pthread_mutex_unlock(&led_ctx->ledoptmutex);
	return ledinfo;
}


int led_destory(LED_t led)
{
	int ret = 0;
	S_LED_CTX_t * led_ctx = (S_LED_CTX_t*)led;
	static single_thread_t *psingle_thread=NULL;
	if(!psingle_thread){
		if(led_ctx->parent)
			psingle_thread = led_ctx->parent;
		else
			return -1;
	}
	if(pthread_mutex_lock(&psingle_thread->pthread_mutex) <0){
		return -1;
	}
	if(led_ctx->init_nums>0){
		led_ctx->init_nums--;
		ret = led_ctx->init_nums;
		goto led_destory_quit;
	}
	if(led_ctx->parent){
		if(led_ctx->next==led_ctx){
			psingle_thread->pthread_working = 0;
			while(!psingle_thread->pthread_quited){usleep(1*1000);}
			psingle_thread->led_ctx = NULL;
			free(led_ctx);led_ctx=NULL;
			goto led_destory_quit;
		}
		psingle_thread->led_ctx = led_ctx->next;
		led_ctx->parent = psingle_thread;
	}

	pthread_mutex_lock(&led_ctx->ledoptmutex);
	SWAP(led_ctx->next, led_ctx->prev->next);
	SWAP(led_ctx->prev, led_ctx->next->prev);
	led_ctx->led_hardware_set_lumen(led_ctx->ledid, 0);
	led_ctx->led_hardware_destory(led_ctx->ledid);
	pthread_mutex_unlock(&led_ctx->ledoptmutex);
	free((void*)led_ctx);led_ctx=NULL;

led_destory_quit:
	pthread_mutex_unlock(&psingle_thread->pthread_mutex);
	return ret;
}

static inline double diftimems(timeval_t tv1, timeval_t tv2)
{
	return (tv1.tv_sec-tv2.tv_sec) + (tv1.tv_usec-tv2.tv_usec)/1000000.0;//单位是秒
}

static void *ledThread(void *arg)
{
	single_thread_t *psingle_thread = (single_thread_t *)arg;
	psingle_thread->pthread_working = 1;
	S_LED_CTX_t * led_ctx;// = psingle_thread->led_ctx;
	while(psingle_thread->pthread_working){
		while(psingle_thread->led_ctx && pthread_mutex_trylock(&psingle_thread->led_ctx->ledoptmutex)){usleep(1*1000);}
		if(!psingle_thread->led_ctx)break;
		for(led_ctx = psingle_thread->led_ctx; led_ctx!=NULL; led_ctx = led_ctx->next){
			if(!psingle_thread->pthread_working)
				break;
			switch(led_ctx->ledinfo.mode){
				case LED_SIMPLE:{
					if(led_ctx->ledinfo.lumen != led_ctx->curlumen){
						led_ctx->curlumen = led_ctx->ledinfo.lumen;
						led_ctx->led_hardware_set_lumen(led_ctx->ledid, led_ctx->curlumen);
					}
				}break;
				case LED_FLASH:{
					timeval_t tv;
					gettimeofday(&tv, NULL);
					if(diftimems(tv, led_ctx->tv) >= 0.5 / led_ctx->ledinfo.flashfreq){
						led_ctx->tv = tv;
						int lumen = led_ctx->ledinfo.lumen, sub_lumen = led_ctx->ledinfo.sub_lumen;
						led_ctx->curlumen = (led_ctx->curlumen >= (lumen + sub_lumen) / 2.0)?sub_lumen : lumen;
						led_ctx->led_hardware_set_lumen(led_ctx->ledid, led_ctx->curlumen);
					}
				}break;
				case LED_BREATH:{//to do for future; not support now.
					timeval_t tv;
					gettimeofday(&tv, NULL);
					if(diftimems(tv, led_ctx->tv) >= 0.5 * led_ctx->ledinfo.breathstep / led_ctx->ledinfo.flashfreq / (led_ctx->ledinfo.lumen - led_ctx->ledinfo.sub_lumen)){
						led_ctx->tv = tv;
						led_ctx->curlumen += led_ctx->ledinfo.breathstep;
						if(led_ctx->curlumen >= led_ctx->ledinfo.lumen || led_ctx->curlumen <= led_ctx->ledinfo.sub_lumen)
							led_ctx->ledinfo.breathstep = -led_ctx->ledinfo.breathstep;
						led_ctx->led_hardware_set_lumen(led_ctx->ledid, led_ctx->curlumen);
					}
				}break;
			}
			while(led_ctx->next != led_ctx && pthread_mutex_trylock(&led_ctx->next->ledoptmutex)){usleep(1*1000);}
			if(led_ctx->next != led_ctx){
				pthread_mutex_unlock(&led_ctx->ledoptmutex);
			}else{
				pthread_mutex_unlock(&led_ctx->ledoptmutex);
				break;
			}
			
		}
		if(!psingle_thread->pthread_working)
			break;
		usleep(1000);
	}

	psingle_thread->pthread_quited = 1;
	return NULL;
}




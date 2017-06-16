#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "RKGpioApi.h"
#include "led.h"

#define LED_SOFT_OFF 0
#define LED_SOFT_ON 100
#define LED_HW_OFF 100
#define LED_HW_ON 0

#define LED_MAX_LUMIN 100
#define SWAP(A,B) ({A = (A)^(B);B = (A)^(B);A = (A)^(B);})
#define SH_LUMIN_TRANSFORM(__lumin) (LED_MAX_LUMIN - (__lumin))

static pthread_mutex_t ledoptmutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct timeval timeval_t;
typedef struct{
	S_LED_t led;
	timeval_t tv;
	int ledlumin;
}S_LED_TIMER_t;
static S_LED_TIMER_t *ledtimer = NULL;
static int ledtnum = 0;
S_LED_t led_init(int ledport)
{
	S_LED_t ret={.ledport = -1};
	int lednum = ledtnum;
	S_LED_TIMER_t* ledtp = ledtimer;
	if(pthread_mutex_lock(&ledoptmutex) <0){
		return ret;
	}
	if(RK_GpioOpen(ledport, eGPIO_OPEN_OUTPUT) < 0){
		goto led_init_error;
	}
	if(RK_GpioWrite(ledport, LED_HW_OFF) <0){
		goto led_init_error;
	}
	ledtp = realloc(ledtp, sizeof(S_LED_TIMER_t)*(lednum+1));
	ledtp[lednum].led.ledport = ledport;
	ledtp[lednum].led.ledlumin = LED_SOFT_OFF;
	ledtp[lednum].led.ledluminsub = LED_SOFT_OFF;
	ledtp[lednum].led.ledmode = LED_SIMPLE;
	ret = ledtp[lednum].led;
	ledtimer = ledtp;
	ledtnum++;
led_init_error:
	pthread_mutex_unlock(&ledoptmutex);
	return ret;
}

int led_set(S_LED_t led)//0 on success, -1 on failed.
{
	if(pthread_mutex_lock(&ledoptmutex) <0){
		return -1;
	}
	S_LED_TIMER_t* ledtp = ledtimer;
	int ledindex;
	int lednum = ledtnum;
	for(ledindex=0; ledindex<lednum; ledindex++){
		if(led.ledport == ledtp[ledindex].led.ledport){
			switch(led.ledmode){
				case LED_SIMPLE:{
					ledtp[ledindex].led=led;
				}break;
				case LED_FLASH:{
					if(led.ledlumin<led.ledluminsub){
						SWAP(led.ledlumin, led.ledluminsub);
					}
					ledtp[ledindex].led=led;
				}break;
				case LED_BREATH:{
					if(led.ledlumin<led.ledluminsub){
						SWAP(led.ledlumin, led.ledluminsub);
					}
					int step = ledtp[ledindex].led.ledbreathstep;
					int sgn = ((step > 0)*2 -1) * (step != 0);
					led.ledbreathstep *= sgn;
					ledtp[ledindex].led=led;
				}break;
				default:
					break;
			}
			break;
		}
	}
	pthread_mutex_unlock(&ledoptmutex);
	return (ledindex >= lednum)? -1 : 0;
}

S_LED_t led_get(int ledport)
{
	S_LED_t ret={.ledport = -1};
	if(pthread_mutex_lock(&ledoptmutex) <0){
		return ret;
	}
	int ledindex;
	int lednum = ledtnum;
	S_LED_TIMER_t* ledtp = ledtimer;
	for(ledindex=0; ledindex<lednum; ledindex++){
		if(ledport == ledtp[ledindex].led.ledport){
			ret = ledtp[ledindex].led;
			break;
		}
	}
	pthread_mutex_unlock(&ledoptmutex);
	return ret;
}


int led_destory(int ledport)
{
	int ret = -1;
	S_LED_TIMER_t* ledtp = ledtimer;
	int lednum = ledtnum;
	if(pthread_mutex_lock(&ledoptmutex) <0){
		return -1;
	}
	if(RK_GpioWrite(ledport, LED_HW_OFF) <0){
		goto led_destory_error;
	}
	if(RK_GpioClose(ledport) <0){
		goto led_destory_error;
	}
	int ledindex;
	for(ledindex=0; ledindex<lednum; ledindex++){
		if(ledport == ledtp[ledindex].led.ledport){
			break;
		}
	}
	if(ledindex>=lednum){
		goto led_destory_error;
	}
	while(ledindex<lednum-1){
		ledtp[ledindex] = ledtp[ledindex+1];
		ledindex++;
	}
	ledtnum--;
	ledtp[ledindex] = (S_LED_TIMER_t){0};
	ledtp = realloc(ledtp, sizeof(S_LED_TIMER_t)*(lednum-1));
	ledtimer = ledtp?ledtp : ledtimer;
	ret=0;
led_destory_error:
	pthread_mutex_unlock(&ledoptmutex);
	return ret;
}

static inline double diftimems(timeval_t tv1, timeval_t tv2)
{
	return (tv1.tv_sec-tv2.tv_sec) + (tv1.tv_usec-tv2.tv_usec)/1000000.0;//单位是秒
}

static void *ledThread(void *arg)
{
	
	while(1){
		int ledidx;
		for(ledidx=0;ledidx<ledtnum;ledidx++){
			pthread_mutex_lock(&ledoptmutex);
			switch(ledtimer[ledidx].led.ledmode){
				case LED_SIMPLE:{
					if(ledtimer[ledidx].led.ledlumin != ledtimer[ledidx].ledlumin){
						ledtimer[ledidx].ledlumin = ledtimer[ledidx].led.ledlumin;
						RK_GpioWrite(ledtimer[ledidx].led.ledport, SH_LUMIN_TRANSFORM(ledtimer[ledidx].ledlumin));
						gettimeofday(&ledtimer[ledidx].tv, NULL);
					}
				}break;
				case LED_FLASH:{
					timeval_t tv;
					gettimeofday(&tv, NULL);
					if(diftimems(tv, ledtimer[ledidx].tv) >= 0.5 / ledtimer[ledidx].led.ledflashfreq){
						ledtimer[ledidx].tv = tv;
						int ledlumin = ledtimer[ledidx].led.ledlumin, ledluminsub = ledtimer[ledidx].led.ledluminsub;
						ledtimer[ledidx].ledlumin = (ledtimer[ledidx].ledlumin >= (ledlumin+ledluminsub) / 2.0)?ledluminsub : ledlumin;
						RK_GpioWrite(ledtimer[ledidx].led.ledport, SH_LUMIN_TRANSFORM(ledtimer[ledidx].ledlumin));
					}
				}break;
				case LED_BREATH:{//to do for future; not support now.
					timeval_t tv;
					gettimeofday(&tv, NULL);
					if(diftimems(tv, ledtimer[ledidx].tv) >= 0.5 / ledtimer[ledidx].led.ledflashfreq){
						ledtimer[ledidx].tv = tv;
						ledtimer[ledidx].ledlumin += ledtimer[ledidx].led.ledbreathstep;
						if(ledtimer[ledidx].ledlumin >= ledtimer[ledidx].led.ledlumin || ledtimer[ledidx].ledlumin <= ledtimer[ledidx].led.ledluminsub )
							ledtimer[ledidx].led.ledbreathstep = -ledtimer[ledidx].led.ledbreathstep;
						RK_GpioWrite(ledtimer[ledidx].led.ledport, SH_LUMIN_TRANSFORM(ledtimer[ledidx].ledlumin));
						
					}
				}break;
			}
			pthread_mutex_unlock(&ledoptmutex);
		}
		usleep(1000);
	}
	return NULL;
}

int led_workstart(void*arg)
{
	pthread_t threadID;
	if(pthread_create(&threadID, NULL, ledThread, arg) < 0){
		return -1;
	}
	pthread_detach(threadID);
	return 0;
}

#if 0
int main()
{
	S_LED_t led = led_init(37);
	led_workstart(NULL);
	while(1){
		led.ledlumin = 100;
		printf("start\n");
		led_set(led);
		sleep(5);
		printf("flash\n");
		led.ledmode = LED_FLASH;
		led.ledflashfreq = 5;
		led_set(led);
		sleep(5);
		printf("flash\n");
		led.ledflashfreq = 10;
		led_set(led);
		pause();
	}
}
#endif


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <sys/time.h>

#include <RKIoctrlDef.h>
#include "RKGpioApi.h"
#include "led.h"

#define LED_SOFT_OFF 0
#define LED_SOFT_ON 100
#define LED_HW_OFF 100
#define LED_HW_ON 0

#define LED_MAX_LUMIN 100
#define SWAP(A,B) {A = (A)^(B);B = (A)^(B);A = (A)^(B);}
#define SH_LUMIN_TRANSFORM(__lumin) (LED_MAX_LUMIN - (__lumin))

#define RK_WLEDPORT 44 
#define RK_BUTTONRESET 	38
#define RK_BUTTONMUTE 	22
#define RK_BUTTONCNT 	2

typedef struct{
	int port;
	int fd;
	int old_key;
	int new_key;
	int key_press;
	int key_flag;
	struct timeval t_press;
	struct timeval t_release;
}RK_BUTTON_MAP;

typedef struct{
	
	int port[5];
	int fd[5];
	SMsgIoctrlData *msgData;

}RK_GPIODATA; 

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

void button_cleanup(RK_BUTTON_MAP rButtonMap[RK_BUTTONCNT])
{
	int i;
	for(i = 0; i < RK_BUTTONCNT; i++)
	{
		close(rButtonMap[i].fd);
		RK_GpioClose(rButtonMap[i].port);
	}
}

void *button_pthread(void *arg)
{
	int ret;
	int i;
	int msg_id = *(int *)arg;
	char buff[10];
	struct pollfd fds[RK_BUTTONCNT];

	int value;
	struct timeval Curtime;

	RK_BUTTON_MAP rButtonMap[RK_BUTTONCNT];// = (RK_BUTTON_MAP *)malloc(sizeof(RK_BUTTON_MAP));
	rButtonMap[0].port = RK_BUTTONRESET;
	rButtonMap[0].key_press = 0;
	rButtonMap[0].key_flag = 0;
	rButtonMap[1].port = RK_BUTTONMUTE;
	rButtonMap[1].key_press = 0;
	rButtonMap[1].key_flag = 0;

	for(i = 0; i< RK_BUTTONCNT; i++){
		RK_GpioOpen(rButtonMap[i].port,eGPIO_OPEN_INPUT);

		RK_GpioEdge(rButtonMap[i].port,eGPIO_EDGE_BOTH);
		rButtonMap[i].fd = RK_GpioValueOpen(rButtonMap[i].port);

		fds[i].fd = rButtonMap[i].fd;
		fds[i].events = POLLPRI;

		ret = read(rButtonMap[i].fd,buff,sizeof(buff));
		if(ret < 0)
		{
			printf("read value fail\n");
		}

		rButtonMap[i].old_key = atoi(buff);
		rButtonMap[i].new_key = atoi(buff);
	}

	pthread_cleanup_push(button_cleanup,&rButtonMap[RK_BUTTONCNT]);
		while(1)
		{
		
			if((ret = poll(fds,RK_BUTTONCNT,30)) < 0)
				printf("poll error\n");

			for(i = 0; i < RK_BUTTONCNT; i++)
			{
				if(fds[i].revents & POLLPRI){
					if((ret = lseek(rButtonMap[i].fd,0,SEEK_SET)) < 0)
						printf("lseek error fd[%d]",i);
					if((ret = read(rButtonMap[i].fd,buff,sizeof(buff))) < 0)
						printf("read error fd[%d]\n",i);
					rButtonMap[i].new_key = atoi(buff);
					if(rButtonMap[i].new_key != rButtonMap[i].old_key)
					{
						rButtonMap[i].key_press = 1;
						rButtonMap[i].key_flag = 1;
						gettimeofday(&Curtime,NULL);
						rButtonMap[i].t_press = Curtime;
					}else{
						rButtonMap[i].key_press = 0;
						rButtonMap[i].key_flag = 0;
					}
				}

				switch(rButtonMap[i].port)
				{
					case RK_BUTTONRESET:
						{

							gettimeofday(&Curtime,NULL);
							rButtonMap[i].t_release = Curtime;
							if((rButtonMap[i].key_press == 1) && (rButtonMap[i].t_press.tv_sec + 5 == rButtonMap[i].t_release.tv_sec) && (rButtonMap[i].key_flag == 1))
							{
								rButtonMap[i].key_flag = 0;

								RK_SndIOCtrl(msg_id, NULL, 0, eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVSLOGOUT);

								RK_SndIOCtrl(msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_OFF);
								system("recoverboard.sh");
							}

						}
						break;

					case RK_BUTTONMUTE:
						{
							if((rButtonMap[i].key_press == 1) && (rButtonMap[i].key_flag == 1)){
								RK_SndIOCtrl(msg_id,NULL,0,eIOTYPE_USER_MSG_AVS,eIOTYPE_MSG_AVS_BUTTON_MUTE);
								rButtonMap[i].key_flag = 0;
							}						
						}
						break;

					default:
						{
							if((rButtonMap[i].key_press == 1) && (rButtonMap[i].key_flag == 1)){
								printf("Not has this port\n");
								rButtonMap[i].key_flag = 0;
							}	
						}
						break;
				}

			}

		}
		pthread_cleanup_pop(1);
		return NULL;
}

int main()
{

	int msg_id;
	pthread_t button;

	SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData));

	S_LED_t led = led_init(RK_WLEDPORT);
	led_workstart(NULL);

	msg_id = RK_MsgInit();

	pthread_create(&button,NULL,&button_pthread,&msg_id);
	
	while(1){
		RK_RecvIOCtrl(msg_id, msgData, sizeof(SMsgIoctrlData), eIOTYPE_USER_MSG_GPIO_WLED);
		switch(msgData->u32iCommand){
		case eIOTYPE_MSG_GPIO_WLED_ON:
			{
				led.ledmode = LED_SIMPLE;
				led.ledlumin = 100;
				led_set(led);
			}
			break;

		case eIOTYPE_MSG_GPIO_WLED_SLOWFLASHING:
			{
				led.ledmode = LED_FLASH;
				led.ledlumin = 100;
				led.ledflashfreq = 1;
				led_set(led);
			}
			break;

		case eIOTYPE_MSG_GPIO_WLED_QUICKFLASHING:
			{
				led.ledmode = LED_FLASH;
				led.ledlumin = 100;
				led.ledflashfreq = 4;
				led_set(led);
			}
			break;

		case eIOTYPE_MSG_GPIO_WLED_OFF:
			{
				led.ledmode = LED_SIMPLE;
				led.ledlumin = 0;
				led_set(led);
			}
			break;
		case eIOTYPE_MSG_GPIO_WLED_UPDATE_FLASHING:
			{
				led.ledmode = LED_FLASH;
				led.ledlumin = 100;
				led.ledflashfreq = 20;
				led_set(led);
			}
			break;

		default:
			printf("WLEN STATUS ERROR\n");
			break;
		}
	}

	led_destory(RK_WLEDPORT);

	return 0;
}


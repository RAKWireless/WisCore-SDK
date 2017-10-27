#include <stdio.h>
#include "RKLedio.h"
#include "RKLed.h"
#include "RKGpioApi.h"

static void* rak_led_init_callback(void *ledinit){
	int ledport = (int)(long)ledinit;
	int ret = 0;
	if(RK_GpioOpen(ledport, eGPIO_OPEN_OUTPUT) < 0)ret = -1;
	if((ret == 0) && (RK_GpioWrite(ledport, 1) <0))ret = -1;
	return (ret==0)?ledinit: NULL;
}

static int rak_led_set_lumen_callback(void* ledid, int lumen){
	int ledport = (int)(long)ledid;
	lumen = (lumen==0)?1:0;
	return RK_GpioWrite(ledport, lumen);
}

static int rak_led_destroy_callback(void* ledid){
	int ledport = (int)(long)ledid;
	int ret = 0;
	if(RK_GpioWrite(ledport, 1)<0)ret = -1;
	if((ret==0)&&(RK_GpioClose(ledport)<0))ret = -1;
	return ret;
}


S_RK_LED_t RK_LED_Init(int ledport){
	void* ledinit = (void*)(long)ledport;
	return (S_RK_LED_t)led_init(ledinit, rak_led_init_callback, rak_led_set_lumen_callback, rak_led_destroy_callback);
}
int RK_LED_TurnOn(S_RK_LED_t rak_led){
	S_LED_INFO_t ledinfo = {LED_SIMPLE,1};
	return led_set((LED_t)rak_led, &ledinfo);
}
int RK_LED_TurnOff(S_RK_LED_t rak_led){
	S_LED_INFO_t ledinfo = {LED_SIMPLE,0};
	return led_set((LED_t)rak_led, &ledinfo);
}
int RK_LED_Flash(S_RK_LED_t rak_led, double flashfreq){
	S_LED_INFO_t ledinfo = {LED_FLASH,1,0,flashfreq};
	return led_set((LED_t)rak_led, &ledinfo);
}
int RK_LED_GetMode(S_RK_LED_t rak_led){
	S_LED_INFO_t ledinfo = led_get((LED_t)rak_led);
	int mode = -1;
	if(ledinfo.mode == LED_SIMPLE){
		mode = eLED_MODE_SIMPLE;
	}else if(ledinfo.mode == LED_FLASH){
		mode = eLED_MODE_FLASH;
	}else if(ledinfo.mode == LED_BREATH){
		mode = eLED_MODE_BREATH;
	}
	return mode;
}
int RK_LED_Destroy(S_RK_LED_t rak_led){
	return led_destory((LED_t)rak_led);
}


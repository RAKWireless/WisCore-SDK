#ifndef __RK_LED_IO_H__
#define __RK_LED_IO_H__

typedef enum{
	LED_SIMPLE,
	LED_FLASH,
	LED_BREATH,
}E_LED_MODE;

typedef void* LED_t;

typedef struct{
	E_LED_MODE mode;
	int lumen;
	int sub_lumen; //for LED_FLASH and LED_BREATH
	double flashfreq; //for LED_FLASH and LED_BREATH
	int breathstep; //only for LED_BREATH
	const char *description;// just for description
}S_LED_INFO_t;

LED_t led_init(
	void* ledinit,
	void*(*led_hardware_init)(void* ledinit),
	int(*led_hardware_set_lumen)(void* ledid, int lumen),
	int (*led_hardware_destory)(void*ledid)
	);

int led_set(LED_t led, const S_LED_INFO_t* ledinfo);
S_LED_INFO_t led_get(LED_t led);
int led_destory(LED_t led);
#endif

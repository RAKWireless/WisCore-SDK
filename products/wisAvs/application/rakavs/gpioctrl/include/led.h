#ifndef __LED_H__
#define __LED_H__

typedef enum{
	LED_SIMPLE,
	LED_FLASH,
	LED_BREATH,
}E_LED_MODE;


typedef struct{
	int ledport;
	E_LED_MODE ledmode;
	int ledlumin;
	int ledluminsub;
	double ledflashfreq;
	int ledbreathstep;
}S_LED_t;

S_LED_t led_init(int ledport);
int led_workstart(void*arg);
int led_set(S_LED_t led);
S_LED_t led_get(int ledport);
int led_destory(int ledport);

#endif

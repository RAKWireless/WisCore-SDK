#ifndef __RK_LED_H__
#define __RK_LED_H__

typedef void * S_RK_LED_t;
typedef enum{
	eLED_MODE_ERROR = -1,
	eLED_MODE_SIMPLE = 0,
	eLED_MODE_FLASH,
	eLED_MODE_BREATH,
}E_RK_LED_MODE;

S_RK_LED_t RK_LED_Init(int ledport);
int RK_LED_TurnOn(S_RK_LED_t rak_led);
int RK_LED_TurnOff(S_RK_LED_t rak_led);
int RK_LED_Flash(S_RK_LED_t rak_led, double flashfreq);
int RK_LED_Destroy(S_RK_LED_t rak_led);
int RK_LED_GetMode(S_RK_LED_t rak_led);


#endif


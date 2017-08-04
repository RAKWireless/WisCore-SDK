#ifndef __RKGPIO_API_H__
#define __RKGPIO_API_H__

typedef enum 
{
	eGPIO_OPEN_INPUT 					= 0,
	eGPIO_OPEN_OUTPUT 					= 1,

}GPIO_OPEN_TYPE;

typedef enum 
{
	eGPIO_EDGE_NONE 					= 0,
	eGPIO_EDGE_RISE 					= 1,
	eGPIO_EDGE_FALL 					= 2,
	eGPIO_EDGE_BOTH 					= 3,
}GPIO_EDGE_TYPE;

int gpio_open(int port , int dir);
int gpio_write(int port, int value);
int gpio_read(int port);
int gpio_edge(int port, int edge);
int gpio_close( int port );
int RK_GpioValueOpen(int port);
int RK_GpioValueClose(int fd);

#endif

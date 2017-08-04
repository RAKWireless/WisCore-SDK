#ifndef __RKUART_DEF_H__
#define __RKUART_DEF_H__

#define UART1_DEV "/dev/ttyS1"
int RK_SetUartOpt(int fd,int nSpeed, int nBits, char nEvent, int nStop);
void RK_CloseUart(int fd);
int RK_InitUart();

#endif

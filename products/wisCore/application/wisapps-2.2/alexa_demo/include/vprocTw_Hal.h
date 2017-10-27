#include <sys/ioctl.h>
#include <linux/types.h>
//#include "zl380tw.h"
//#include <linux/spi/zl380tw.h>
/* ************************************************************************
 * changed by chonggao.yan: add fd to distinguish zl38062 and zl38067 and so on.
 * ************************************************************************/


int VprocHALInit(char *device_name);
int VprocHALGetFd(char* device_name);
void VprocHALcleanup(int fd);
int VprocHALread(int fd, unsigned char *pData, unsigned short numBytes);
int VprocHALwrite(int fd, unsigned char *pData, unsigned short numBytes);
int ioctlHALfunctions (int fd, unsigned int cmd, void* arg); 

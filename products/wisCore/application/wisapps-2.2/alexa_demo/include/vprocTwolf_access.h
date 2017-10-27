/****************************************************************************
* Microsemi Semiconductor, Kanata, ON
****************************************************************************
*
* Description: Voice Processor devices high level access module function
*                definitions
*
* NOTE: The registers of the device are 16-bit wide. A 32-bit access
*       is not required. However, the 32-bit access functions are provided
*       only if the host wants to access two consecutives 16-bit registers
*       in one single access.
*  Author: Jean Bony
****************************************************************************
* Copyright Microsemi Semiconductor Ltd., 2013. All rights reserved. This
* copyrighted work constitutes an unpublished work created in 2013. The use
* of the copyright notice is intended to provide notice that Microsemi
* Semiconductor Ltd. owns a copyright in this unpublished work; the main
* copyright notice is not an admission that publication has occurred. This
* work contains confidential, proprietary information and trade secrets of
* Microsemi Semiconductor Ltd.; it may not be used, reproduced or transmitted,
* in whole or in part, in any form or by any means without the prior
* written permission of Microsemi Semiconductor Ltd. This work is provided on
* a right to use basis subject to additional restrictions set out in the
* applicable license or other agreement.
*
***************************************************************************/
/* ************************************************************************
 * changed by chonggao.yan: add fd to distinguish zl38062 and zl38067 and so on.
 * ************************************************************************/


#ifndef VPROCTWOLFACCESS_H
#define VPROCTWOLFACCESS_H

#include "vproc_common.h"
#include "vprocTw_Hal.h"


#define TWOLF_MAILBOX_SPINWAIT  1000  /*at least a 1000 to avoid mailbox busy */
//#define DEBUG
#ifdef DEBUG
#define DBG(...) 	fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#define INFO(...)	{ fprintf(stderr, " INFO(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define DBG(...)
#define INFO(...)
#endif
typedef struct tw_setup {
    unsigned char direction;        /* 0 - write, 1 - read */
    unsigned char numwords;
    unsigned short reg;
    unsigned short value;
} TW_SETUP;

/*device HBI command structure*/
typedef struct hbiCmdInfo {
   unsigned char page;
   unsigned char offset;
   unsigned char numwords;
} hbiCmdInfo;

/* external function prototypes */

VprocStatusType VprocTwolfHbiInit(char *devicename, int *pfd); /*Use this function to initialize the HBI bus*/
int VprocTwolfGetFd(char *devicename);//add by yan
VprocStatusType VprocTwolfHbiRead(
	int fd,
    unsigned short cmd,       /*the 16-bit register to read from*/
    unsigned char numwords,   /* The number of 16-bit words to read*/
    unsigned short *pData);   /* Pointer to the read data buffer*/

VprocStatusType VprocTwolfHbiWrite(
    int fd,
	unsigned short cmd,     /*the 16-bit register to write to*/
    unsigned char numwords, /* The number of 16-bit words to write*/
    unsigned short *pData); /*the words (0-255) to write*/

VprocStatusType TwolfHbiNoOp( /*send no-op command to the device*/
    unsigned char numWords);  /* The number of no-op (0-255) to write*/

/*An alternative method to loading the firmware into the device
* USe this method if you have used the provided tool to convert the *.s3 into
* c code that can be compiled with the application
*/
VprocStatusType VprocTwolfHbiBoot_alt( /*use this function to boot load the firmware (*.c) from the host to the device RAM*/
    int fd, twFirmware *st_firmware); /*Pointer to the firmware image in host RAM*/

/*An alternative method to loading the firmware into the device
* USe this method if you have not used the provided tool to convert the *.s3 but
* instead prefer to load the *.s3 file directly
*/
VprocStatusType VprocTwolfHbiBoot(     /*use this function to boot load the firmware (*.s3) from the host to the device RAM*/
    int fd, FILE *BOOT_FD);     /*Pointer to the firmware image in host RAM*/

VprocStatusType VprocTwolfLoadConfig(
	int fd,
    dataArr *pCr2Buf,
    unsigned short numElements);
    
VprocStatusType VprocTwolfHbiCleanup(int fd);
VprocStatusType VprocTwolfHbiDeviceCheck(int fd);
VprocStatusType VprocTwolfHbiBootPrepare(int fd);
VprocStatusType VprocTwolfHbiBootMoreData(int fd, char *dataBlock);
VprocStatusType VprocTwolfHbiBootConclude(int fd);
VprocStatusType VprocTwolfFirmwareStop(int fd);   /*Use this function to halt the currently running firmware*/
VprocStatusType VprocTwolfFirmwareStart(int fd);  /*Use this function to start/restart the firmware currently in RAM*/
VprocStatusType VprocTwolfSaveImgToFlash(int fd);  /*Save current loaded firmware from device RAM to FLASH*/
VprocStatusType VprocTwolfSaveCfgToFlash(int fd, uint16 imgnum); /*Save current device config from device RAM to FLASH*/
VprocStatusType VprocTwolfReset(int fd, VprocResetMode mode);
VprocStatusType VprocTwolfUpstreamConfigure(int fd, uint16 clockrate,/*in kHz*/ 
                                uint16 fsrate,   /*in Hz*/
                                uint8 aecOn);     /*0 for OFF, 1 for ON*/
VprocStatusType VprocTwolfDownstreamConfigure(int fd, uint16 clockrate, /*in kHz*/
                                uint16 fsrate,    /*in Hz*/
                              uint8 aecOn);      /*0 for OFF, 1 for ON*/
VprocStatusType VprocTwolfEraseFlash(int fd);
VprocStatusType VprocTwolfLoadFwrCfgFromFlash(int fd, uint16 image_number);
VprocStatusType VprocTwolfLoadFwrFromFlash(int fd, uint16 image_number);//add by yan
VprocStatusType VprocTwolfMute(int fd, VprocAudioPortsSel port, uint8 on);
int RK_TW_Setup(TW_SETUP* tw_params, char* dev_name);
int RK_SwitchFw(int numimage, int debug, char *dev_name);
#endif /* VPROCTWOLFACCESS_H */

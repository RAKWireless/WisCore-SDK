//player_io.h
//by seven @2016-12-27
#ifndef __AUDIO_IO_H__
#define __AUDIO_IO_H__

#include "avcodec.h"

typedef  void AUDIO_IO;

enum _err_code {
	eAUDIO_ERRCODE_SUCCESS				=  0,
	eAUDIO_ERRCODE_FAILED				= -1,
	eAUDIO_ERRCODE_WRITE_FILE_FAILED	= -2,
	eAUDIO_ERRCODE_READ_FILE_FAILED		= -3,
	eAUDIO_ERRCODE_FAILED_OPEN_DEV		= -4,
	eAUDIO_ERRCODE_NO_VALID_RES			= -5,
	eAUDIO_ERRCODE_INVALID_CMD			= -6,
	eAUDIO_ERRCODE_INVALID_CMD_VALUE	= -7,
	eAUDIO_ERRCODE_ABORT				= -10,
	eAUDIO_ERRCODE_FAILED_ALLOCATE		= -20,
	eAUDIO_ERRCODE_FAILED_INIT_MUTEX	= -21,
	eAUDIO_ERRCODE_FAILED_INIT_COND_VAR	= -22,
};

typedef enum {
	ePBH_STATE_IDLE,
	ePBH_STATE_START,
	ePBH_STATE_PAUSED,
	ePBH_STATE_BUSY,
}E_PBHSTATE;	/*Playback handle state*/

typedef enum {
	eOUTPUT_FORMAT_UNKNOW	= -1,
	eOUTPUT_FORMAT_DIALOG	= 0,
	eOUTPUT_FORMAT_ALERT,
	eOUTPUT_FORMAT_MEDIA1,
	eOUTPUT_FORMAT_MEDIA2,
	eOUTPUT_FORMAT_SYSTIP,
	eOUTPUT_FORMAT_CNT
}E_OUTPUTFORMAT;

typedef struct{
	int		pbId;
	uint32_t u32lastpkt;
	uint32_t u32frameSize;
	uint8_t *frameBuf;
}S_PLAYER_CMD_AUDIO_DATA;

typedef struct AVFormat{
	char            fileName[64];
	long			playseconds;
	int 			replaytimes;
	int				header_size;
	unsigned int    ui32sample_rate;
	E_OUTPUTFORMAT	eOutputFormat;
	enum AVCodecID 	audio_codec;
	void (*play_done_cbfunc)(int, void*);
	void            *usrpriv;
}S_AVFormat;

typedef struct player_cmd_volume {
	E_OUTPUTFORMAT	eOutputFormat;
	int			f16Volume;
}S_PLAYER_CMD_VOL;
//Define Player commands

int RK_Audio_WriteDataHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, const void *pAVData);
int RK_Audio_VolumeHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, uint8_t f16Volume);
int RK_Audio_PauseHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, bool bPause);
int RK_Audio_CloseHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOpFmt);
int RK_Audio_OpenHandle(AUDIO_IO *c, S_AVFormat	*psAVFmt);
void RK_AudioIO_GlobalInit(void);
AUDIO_IO *RK_AudioIO_Init(void);



#endif	//__AUDIO_IO_H__

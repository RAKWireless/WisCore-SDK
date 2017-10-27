#ifndef __AUDIO_PLAYER_ITF_H___	//audio player interface
#define __AUDIO_PLAYER_ITF_H___

enum {
	ePB_ERR_SUCCESS 		= 0,
	ePB_ERR_FAIL			= -1,
	ePB_ERR_OPEN_DEV 		= -2,
	ePB_ERR_CLOSE_DEV		= -3,
	ePB_ERR_NO_RES			= -4,
	ePB_ERR_INVALID_USR_RES	= -5,
	ePB_ERR_INIT_FAIL 		= -6,
	ePB_ERR_WRITE_OVERFLOW	= -7,	/* receive buffer overflow */
	ePB_ERR_ALLOCATE_FAIL 	= -8,
};

typedef enum {
	ePB_TYPE_UNKNOW	= -1,
	ePB_TYPE_DIALOG	= 0,
	ePB_TYPE_ALERT,
	ePB_TYPE_PLAY_STREAM,		/* play a remote http/https media stream.*/
	ePB_TYPE_PLAY_CID_TTS,		/* play a binary audio attachment when url is formatted as follows:'CID' */
	ePB_TYPE_CNT
}E_PB_TYPE;

typedef enum {
	eAUDIO_MASK_NONE,
	eAUDIO_MASK_MP3,
	eAUDIO_MASK_AAC,
	eAUDIO_MASK_PCM,
	eAUDIO_MASK_NUM
}E_AUDIO_MASK;	

typedef enum {
	/*first byte*/
	ePB_STATE_CLOSED,
	ePB_STATE_OPENED,
	ePB_STATE_PLAYING,
	ePB_STATE_FINISHED,
	ePB_STATE_PAUSED,
	/*second byte*/
	ePB_STATE_UNDERRUN =(1<<8),
	ePB_STATE_OVERRUN  =(2<<8),
}E_PB_STATE;

typedef struct AudioConfig {
	E_AUDIO_MASK	m_eAudioMask;
	int				m_i32smplrate;			/* sample rate: 8000 ~ 48000 */
	int				m_i32channel;			/* 0 - none, 1 - mono, 2-stero*/
	char 			*strFileName;			/* add @20161107 merge*/
	long			playseconds;            /* 0: until to  the end. On the otherwise, playback playseconds seconds and finished*/
	int 			replaytimes;			/* 0: replay until it is closed. On the otherwise, playback audiofile replaytimes times and finished, but maximum duration do not beyound playseconds seconds*/
}S_AudioConf;

typedef struct AudioPlayback {
	const char *name;
	int     (*pb_open)(S_AudioConf *pconf, void *userPtr);
	int     (*pb_close)(void *userPtr);
	int		(*pb_state)(void *userPtr);
    int     (*pb_write)(unsigned char *buf, int size, void *userPtr);
	int		(*pb_set_dmix_volume)(unsigned char f16Volume, void *userPtr);
	int     (*pb_hardwarevolume)(int i32Volume, int isSetVol);
	int 	(*pb_pause)(int pause, void *userPtr);
	int		(*pb_query_seeks)(int expect_seeks, void *userPtr);
}S_AudioPlayback;


#endif	//__AUDIO_PLAYER_ITF_H___

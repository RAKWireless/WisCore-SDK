#ifndef __AUDIO_PLAYER__H___
#define __AUDIO_PLAYER__H___
#include <stdint.h>

typedef enum {
	ePB_TYPE_UNKNOW		= -1,
	ePB_TYPE_DIALOG		= 0,
	ePB_TYPE_ALERT,
	ePB_TYPE_MEDIA1,
	ePB_TYPE_MEDIA2,		/* ping-pong buf*/
	ePB_TYPE_SYSTIP,
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
	ePB_STATE_IDLE,
	ePB_STATE_START,
	ePB_STATE_RUNNING,
	ePB_STATE_PAUSE,
	ePB_STATE_STOP_LATER,		/*EXPECT STOP*/
}E_PB_STATE;

typedef struct AudioHandler {
	E_PB_STATE 		ePlaybackState;
	E_AUDIO_MASK	eAudioMask;
	int8_t			i8PbIdx;
	unsigned char	*ui8PBDone;				/* Point to a variable address that represents the pb completion of the flag, the value of flag is 0: is busy, 1: is finished*/
	unsigned int	ui32smplrate;			/* sample rate: 8000 ~ 48000 */
	int				max_packet_size;
	int				blast_packet;
	char 			*strFileName;			/* add @20161107 merge*/
	long			playseconds;
	int 			replaytimes;
}S_AudioHnd;

typedef struct PlaybackContext {
	struct AudioPlayback	*asAPlayback;
	struct AudioHandler		asAhandler[ePB_TYPE_CNT];
	E_PB_TYPE				ePlaybackType;
	E_PB_TYPE				ePBMeidaIdx;	/* current valid ID for ePB_TYPE_MEDIA1 or ePB_TYPE_MEDIA2*/
	void 					*priv_data;
}S_PBContext;

typedef struct AudioPlayback {
	const char *name;
	int     (*pb_init)( S_PBContext *h);
	int     (*pb_open)( S_PBContext *h, E_PB_TYPE ePlaybackType);
	int     (*pb_close)(S_PBContext *h, E_PB_TYPE ePlaybackType);
	int     (*pb_read)( S_PBContext *h, unsigned char *buf, int size);
    int     (*pb_write)(S_PBContext *h, E_PB_TYPE ePlaybackType, unsigned char *buf, int size);
	int		(*pb_set_dmix_volume)(S_PBContext *h, E_PB_TYPE ePlaybackType, unsigned char f16Volume);
	int     (*pb_hardwarevolume)(int i32Volume, int isSetVol);
	int 	(*pb_pause)(S_PBContext *h, E_PB_TYPE ePlaybackType, int pause);
	int 	(*pb_wait_finished)(S_PBContext *h, E_PB_TYPE ePlaybackType, int timeout);
	int		(*pb_check_handle)(S_PBContext *h);
	void	(*pb_change_handle)(S_PBContext *h, int state);
	int		(*pb_get_handle)(S_PBContext *h, E_PB_TYPE ePBType);
	int		(*pb_get_runing_handles)(void *arg);
	struct AudioPlayback *next;
}S_AudioPlayback;
typedef void (*dialog_view_func_t)(void); //add @20170519

//return 0 on success, negatives on fail,
int RK_Query_Playing(void *arg); 

#endif	//__AUDIO_PLAYER__H___

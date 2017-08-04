//
//ffplayer_dec.h:
#ifndef __FF_PLAYER_DEC_H__
#define __FF_PLAYER_DEC_H__

#define eFF_ERROR_WRITEN_OVERFLOW 	-7

#undef bool
#undef false
#undef true
#define bool	unsigned char
#define false	0
#define true	(!false)

typedef struct FFPlayback {
	const char *name;
	int     (*ffplayer_init)( void *ff);
	int     (*ffplayer_open)( void *ff, int ePlaybackType, int smplrate);
	int     (*ffplayer_close)(void *ff, int ePlaybackType);
	int     (*ffplayer_read)( void *ff, unsigned char *buf, int size);
    int     (*ffplayer_write)(void *ff, int ePlaybackType, const unsigned char *buf, int size);
	int 	(*ffplayer_pause)(void *ff, int ePlaybackType, int pause);
	int		ffplay_runing;
}S_FFPlayback;

int RK_FFLiveStreamHandle
(
	void *p, 
	int mtype, 
	char *pfilename,
	int seek_pos,	//sec
	int (* deliver_callback)(void *h, int mtype, const unsigned char *data, int i32Length)
);

void RK_LibnmediaVersion(void);
int RK_FFPlayer_paramInit(S_FFPlayback *psFFPlayer);
void RK_FFCloseStream(void);
void ffplayer_packet_clear_queue(void);
void RK_FFPlayer_OpenStreamBlock(bool bBlock);
int RK_FFPlayer_PlaybackState(void);

#endif		//__FF_PLAYER_DEC_H__

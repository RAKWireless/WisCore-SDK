//
//audio_decodeItf.h:
#ifndef __AUDIO_DECODE_ITF_H__
#define __AUDIO_DECODE_ITF_H__

enum dec_resp{
 	eDEC_RESP_CODE_OK		= 200,	/* Successful decode frame complete*/
 	eDEC_RESP_CODE_BREAK 	= 204,	/* Decoding be abruptly break relative to remote stream component */
 	eDEC_RESP_CODE_EXCEPTE  = 500	/* Decoding exception */
};

enum {
	eDEC_ERR_SUCCESS 		= 0,
	eDEC_ERR_FAIL			= -1,
	eDEC_ERR_OPEN_DEV 		= -2,
	eDEC_ERR_CLOSE_DEV		= -3,
	eDEC_ERR_NO_RES			= -4,
	eDEC_ERR_INVALID_USR_RES	= -5,
	eDEC_ERR_INIT_FAIL 		= -6,
	eDEC_ERR_WRITE_OVERFLOW	= -7,	/* receive buffer overflow */
	eDEC_ERR_ALLOCATE_FAIL 	= -8,
};

enum dec_state {
	eDEC_STATE_ERR = -1,	/* decode err*/
	eDEC_STATE_IDLE = 0,	/* nothing */
	eDEC_STATE_STARTED,		/* decode begin*/
	eDEC_STATE_DECODING,	/* decoding */
	eDEC_STATE_FINISHED		/* decode  finished*/
};
/*
 * Describle: This is the PCM processing interface from the decoder, We need to pass 
 * raw pcm data to AudioPlayback of libavs and playback audio when decode remote MP3/AAC/OGG etc.
 * @param: opaque - A private pointer, passed to the init/open/read/write/pause/...
 * return eDEC_ERR_...
 */
typedef struct FFPlayback {
	/*
	 * A private pointer, passed to the init/open/read/write/pause/...
	 */
	void 	*opaque;
	/*
	 * We need to open a pcm handle when a remote uri be successful opened
	 * @param: smplrate - current pcm data sample rate
	 * @param: channel - current pcm data channel and be accepted value is 0,1,2(None, mono, stero)
	 */
	int     (*ffplayer_open)( void *opaque, int smplrate, int channel);
	/*
	 * We need to close a pcm handle when the func be called
	 */
	int     (*ffplayer_close)(void *opaque);
	/*
	 * reserved
	 */
	int     (*ffplayer_read)( void *opaque, unsigned char *buf, int size);
	/*
	 * We start to delived pcm data to handle when the func be called
	 * @param: buf - pcm data from decode frame
	 * @param: size - length of pcm data
	 */
    int     (*ffplayer_write)(void *opaque, const unsigned char *buf, int size);
	/*
	 * to pause/resume current pcm handle
	 * @param: pause - true as pause, otherelse resume
	 */
	int 	(*ffplayer_pause)(void *opaque, int pause);
}S_FFPlayback;

typedef struct _decodec_itf {
	const char *name;
	/*
	 * init a decoder resource and setup playback interface
	 * @param: void **s - a private usr decoder pointer, passed to init/open/frame/close...
	 * @param: psPBItf - a pcm data how deliver interface when successful decode a frame. 
	 * the func be implemented by libavs
	 */
	int (*dec_streamInit)(void **s, S_FFPlayback *psPBItf);
	/* 
	 * deinit a decoder resource
	 */
	int (*dec_streamDeInit)(void *s);
	/*
	 * to open/request remote uri
	 * @param: filename - a vaild remote url
	 */
	int (*dec_streamOpen)(void *s, char *filename);
	/*
	 * to read/decode frame packet from remote uri and the decoded data needs to delivered
	 * to libavs by ffplayer_write function 
	 * @param: seek_pos - seek to a given timestamp relative to the frames in current stream component
	 * @return enum dec_resp
	 */
	int	(*dec_streamFrame)(void *s, int seek_pos);
	/*
	 * to close decoder when be called
	 */
	int (*dec_streamClose)(void *s);
	/*
	 * to pause/resume decoder for network streaming protocols
	 * @param: pause - true as pause, otherelse resume
	 */
	int (*dec_streamPause)(void *s, int pause);
	/*
	 * return current decoder state
	 */
	int (*dec_streamState)(void *s);		/// enum dec_state
	/*
	 * a private usr decoder pointer from dec_streamInit function
	 */
	void	*priv_data;
}S_DecodecItf;	

#endif		//__AUDIO_DECODE_ITF_H__
#ifndef __ALSA_PLAYER_H___
#define __ALSA_PLAYER_H___

#ifdef SND_CHMAP_API_VERSION
#define CONFIG_SUPPORT_CHMAP	1
#endif

#ifndef LLONG_MAX
#define LLONG_MAX    9223372036854775807LL
#endif

#define DEFAULT_FORMAT		SND_PCM_FORMAT_U8
#define DEFAULT_SPEED 		8000

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

enum {
	VUMETER_NONE,
	VUMETER_MONO,
	VUMETER_STEREO
};

enum {
	OPT_VERSION = 1,
	OPT_PERIOD_SIZE,
	OPT_BUFFER_SIZE,
	OPT_DISABLE_RESAMPLE,
	OPT_DISABLE_CHANNELS,
	OPT_DISABLE_FORMAT,
	OPT_DISABLE_SOFTVOL,
	OPT_TEST_POSITION,
	OPT_TEST_COEF,
	OPT_TEST_NOWAIT,
	OPT_MAX_FILE_TIME,
	OPT_PROCESS_ID_FILE,
	OPT_USE_STRFTIME,
	OPT_DUMP_HWPARAMS,
	OPT_FATAL_ERRORS,
};

typedef struct au_stream_private {
	snd_pcm_uframes_t 	uiBufSize;				/* buffer duration is # frames */
	snd_pcm_uframes_t 	uiPeriodSize;		/* distance between interrupts is # frames */
	unsigned 			uiPeriodTime;		/* distance between interrupts is # microseconds */
	unsigned 			uiBufTime;			/* buffer duration is # microseconds */
	size_t				uiBufbytes;
	size_t 				uiBits_per_sample; 		/* bit16 */
	size_t				uiBits_per_frame;		/* bit32 */
}S_AU_STREAM_PRIV;

typedef struct ctrl_param {
	snd_pcm_chmap_t 	*channel_map; 
	int 				start_delay;		/* to set 1 if enable arecord, otherwise to set 1 for aplay */
	int 				stop_delay;			//mark
	int 				nonblock;			/* 1-nonblocking mode */
	int 				open_mode;	
	int 				avail_min;	
	int 				verbose;
	int 				vumeter;
	int 				test_nowait;
	int 				max_file_time;
	int 				use_strftime;
	int 				dump_hw_params;
	int 				fatal_errors;

}S_CTRL_PARAM;

typedef struct audio_spec {
	snd_pcm_format_t 	format;
	unsigned int 		channels;
	unsigned int 		rate;		/*sample rate*/
	S_AU_STREAM_PRIV	sAuStreamPriv;
} S_AUDIO_SPEC;

typedef struct audio_param {
	char				*filename;
	char				*strDrvName;		/* alsa driver dev */
	snd_pcm_stream_t 	eStream;			/* audio direction */
	int 				iAuType;			/* audio data type, raw - play, wave - arecord */
	int 				quiet_mode;
	int 				timelimit;			/* to capture arecord time e.g: seconds*/
	int 				mmap_flag;
	unsigned 			period_time;
	unsigned 			buffer_time;
	snd_pcm_uframes_t 	period_frames;
	snd_pcm_uframes_t	buffer_frames;
	S_AUDIO_SPEC		hwparams;
	S_AUDIO_SPEC		rhwparams;
	S_CTRL_PARAM		asCtrlParams;
}S_AUDIO_PARAM;

void version(void);
void prg_exit(snd_pcm_t *handle_ext, int code);
ssize_t safe_read(int fd, void *buf, size_t count);

void signal_handler_recycle (int sig);
void capture(char *filename, size_t size);

ssize_t RH_AudioPcmRead(snd_pcm_t *handle_pcm, u_char *data, size_t rcount);
int RH_AudioSafeOpenFile(char * filename);
int RH_AudioCaptureBytes(size_t *count_bytes, int timelen);
int RH_AudioGetParams(S_AUDIO_PARAM *ps_AudioParam, S_AU_STREAM_PRIV *psAuStreamPriv);
int RH_AudioSetParams(snd_pcm_t *handle_pcm, S_AUDIO_PARAM *ps_AudioParam, S_AU_STREAM_PRIV *psAuStreamPriv);
int RH_AudioPlayback(char *filename, u_char *data, size_t data_size, int isFile);
int RH_AudioOpenDev(snd_pcm_t **handle_pcm, S_AUDIO_PARAM *ps_AudioParam);
void RH_AudioCloseDev(snd_pcm_t *handle_pcm, S_AUDIO_PARAM *ps_AudioParam);
#endif	//__ALSA_PLAYER_H___

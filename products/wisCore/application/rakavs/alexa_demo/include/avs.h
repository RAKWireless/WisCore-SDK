#ifndef __AVS__H___
#define __AVS__H___
#include <cJSON/cJSON.h>
#include <curl/curl.h>
#include <semaphore.h>

#undef bool
#undef false
#undef true
#define bool	uint8_t
#define false	0
#define true	(!false)

#ifndef CURLPIPE_MULTIPLEX
/* This little trick will just make sure that we don't enable pipelining for
   libcurls old enough to not have this symbol. It is _not_ defined to zero in
   a recent libcurl header. */
#define CURLPIPE_MULTIPLEX 0
#endif

#ifndef CODE_VERIFIER_MAX_LEN
#define CODE_VERIFIER_MAX_LEN 129
#define CODE_CHALLENGE_LEN    45
#endif

typedef enum {
	eCURL_HND_REQTOKEN,								//request accesstoken
	eCURL_HND_DOWNCHANNEL_DRT,						//downchannel directive stream
	eCURL_HND_DIRECTIVE_EVT,						//directives event
	eCURL_HND_MEDIA,							//Media stream
	eCURL_HND_PING,								//ping/timeout TEST AVS
	eCURL_HND_RES_NUM,							/* easy handle numbers of resource */
	eCURL_HND_EVENT		= eCURL_HND_RES_NUM,	//recognzie event
	eMAX_NUM_HANDLES							/* max process handle event stream and downchannel */
}E_CURL_TYPE; 

/* AVS all Event set */
typedef enum {
	eAVS_DIRECTIVE_EVENT_UNKNOW			= -1,
	eAVS_DIRECTIVE_EVENT_SPEECHSTARTED  = 0,				//SpeechStarted
	eAVS_DIRECTIVE_EVENT_SPEECHFINISHED,					//SpeechFinished
	eAVS_DIRECTIVE_EVENT_EXPECTSPEECH,						//ExpectSpeech
	eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED,				//SetAlertSucceeded
	eAVS_DIRECTIVE_EVENT_SETALERT_FAILED,					//SetAlertFailed
	eAVS_DIRECTIVE_EVENT_ALERT_STARTED,						//AlertStarted
	eAVS_DIRECTIVE_EVENT_ALERT_STOP,						//AlertStop
	eAVS_DIRECTIVE_EVENT_DELETE_ALERT_SUCCEEDED,			//DeleteAlertSucceeded
	eAVS_DIRECTIVE_EVENT_DELETE_ALERT_FAILED,				//DeleteAlertFailed
	eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_FOREGROUND,			//AlertEnteredForeground 
	eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_BACKGROUND,			//AlertEnteredBackground
	/*AudioPlayer interface*/
	eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED,					//PlaybackStarted
	eAVS_DIRECTIVE_EVENT_PLAYBACK_REPLACE,					//replace_equeue for repeat
	eAVS_DIRECTIVE_EVENT_PLAYBACK_FINISHED,					//PlaybackFinished
	eAVS_DIRECTIVE_EVENT_PLAYBACK_NEARLY_FINISHED,			//PlaybackNearlyFinished
	eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED,					//PlaybackStopped Event
	eAVS_DIRECTIVE_EVENT_PLAYBACK_QUEUE_CLEARED,			//PlaybackQueueCleared Event
	eAVS_DIRECTIVE_EVENT_ProgressReportDelayElapsed,		//ProgressReportDelayElapsed Event
	eAVS_DIRECTIVE_EVENT_ProgressReportIntervalElapsed,
	/*Speak interface*/
	eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED,					//VolumeChanged Event
	eAVS_DIRECTIVE_EVENT_ADJUST_VOLUME_CHANGED,				//VolumeChanged Event For adjustVolume directive
	eAVS_DIRECTIVE_EVENT_MUTE_CHANGED,						//MuteChanged Event
	eAVS_DIRECTIVE_EVENT_STOP_CAPTURE,						// StopCapture Event
	/*System interface*/
	eAVS_DIRECTIVE_EVENT_SYNCHRONIZESTATE,					//SynchronizeState
	eAVS_DIRECTIVE_EVENT_USER_INACTIVITY_REPORT,			//UserInactivityReport Event
	eAVS_DIRECTIVE_EVENT_EXCEPTIONENCOUNTERED,				//ExceptionEncountered
	eAVS_DIRECTIVE_EVENT_SWITCH_ENDPOINT,					//Switch endpoint
	eAVS_ALIVE_PING,										/* To do ping request every 5 minutes @20161028*/
	eAVS_DIRECTIVE_EVENT_CNT
}E_AVS_EVENT_ID;

typedef struct curl_slist 		RK_CURL_SLIST;
typedef struct curl_httppost	RK_CURL_HTTPPOST;

#define 	TANK_MAX	8

typedef struct directives_event_handle {
E_AVS_EVENT_ID		eTankEvtID[TANK_MAX];
uint8_t				tank_position_push;
uint8_t				tank_position_top;
}S_DEVT_HND;

typedef struct KeepAlive {
	char *boundary;				/* point to the end boundary - eg.'--boundary--'*/
	int  boundary_lengths;		/* length of boundary*/
}S_KEEPALIVE;

typedef struct curl_set {
	CURL 		*curl;
	uint8_t		ui8Running;
	uint8_t		ui8Quite;
	uint8_t		ui8Quite_done;
	long int	i32HttpCode;
	int			i32EnableTime;
	uint64_t	ui64StartSec;
	uint8_t		ui8ExpectTimeo;
	sem_t		m_CurlSem;
} S_CURL_SET;

typedef struct avs_curl_set {
	S_CURL_SET 	asCurlSet[eMAX_NUM_HANDLES];
	CURLM 		*multiCurlHnd;					/* muilt handler */
} S_AVS_CURL_SET;

void RK_AVSVersion(void);
void RK_AVSTriggerPerform(void);	//remove - token
int RK_AVSCurlCheckRequestState(S_AVS_CURL_SET 	*pAvsCurlSet);
uint64_t RK_AVSUpdateAliveConnect(void);

/**
 * Return void or NULL
 * set callback - read callback for data uploads or 
 * write callback for recives data or 
 * callback that receives header data.
 *
 * @param curlhnd - set options for a curl easy handle
 * @param read_callback - this callback function gets called by libcurl as soon as it needs to read data in order to send it to the peer - like if you ask it to upload or post data to the server
 * @param read_cb_priv - Data pointer to pass to the file read function
 * @param write_callback - this callback function gets called by libcurl as soon as there is data received that needs to be saved
 * @param write_cb_priv - A data pointer to pass to the write callback
 * @param header_callback - this function gets called by libcurl as soon as it has received header data
 * @param header_cb_priv - pointer to pass to header callback
 * @return void or NULL
 */
void RK_AVSSetCurlReadAndWrite	//remove - token
(
	CURL 	*curlhnd,
	size_t (*read_callback)(char *pAvsData, size_t size, size_t nitems, void *instream),
	void 	*read_cb_priv,
	size_t (*write_callback)(char *pAvsData, size_t size, size_t nitems, void *instream),
	void 	*write_cb_priv,
	size_t (*header_callback)(char *pAvsData, size_t size, size_t nitems, void *instream),
	void 	*header_cb_priv
);
void RK_AVSGetDirectivesEventID
(
	E_AVS_EVENT_ID *peAvsEvtID
);
void RK_AVSSetDirectivesEventID
(
	E_AVS_EVENT_ID	eAvsEvtID
);
#endif	//__AVS__H___

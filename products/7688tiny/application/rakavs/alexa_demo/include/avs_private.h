#ifndef __AVS_PRIVATE__H___
#define __AVS_PRIVATE__H___

#include "avs.h"

/* AVS all directive set */
typedef enum {
	eAVS_DIRECTIVE_UNKNOW 		= -1,
	eAVS_DIRECTIVE_SPEAK		= 0,
	eAVS_DIRECTIVE_EXCEPTSPEECH,	
	eAVS_DIRECTIVE_SETALERT,
	eAVS_DIRECTIVE_DELETE_ALERT,
	/* AudioPlayer interface */
	eAVS_DIRECTIVE_PLAY,
	eAVS_DIRECTIVE_STOP,
	eAVS_DIRECTIVE_CLEAR_QUEUE,				//ClearQueue Directive
	/* Speak interface*/
	eAVS_DIRECTIVE_SETVOLUME,
	eAVS_DIRECTIVE_ADJUSTVOLUME,
	eAVS_DIRECTIVE_SETMUTE,
	eAVS_DIRECTIVE_RESTUSERINACTIVITY,		//ResetUserInactivity
	eAVS_DIRECTIVE_SETENDPOINT,				//SetEndpoint
	eAVS_DIRECTIVE_CNT
}E_AVS_DIRECTIVE;

/* directive event timer/alarm state machine */
typedef enum {
	eSCH_STATE_IDLE,			/* current timer is idle or not exist*/
	eSCH_STATE_STOP,			/* stop status add @20161107 merge*///			getbit=0x00000001
	eSCH_STATE_PREPARE,			/* recive a setAlert directive *///				setbit=0x00000002
	eSCH_STATE_WAIT_SUBMIT,		/* prepare notify avs setAlert result*///		submitset=0x00000004
	eSCH_STATE_SUBMIT,			/* already notify avs alert is set success *///	
	eSCH_STATE_ALERT_REACH,			/* specific a schedule timer have be reached */
	eSCH_STATE_ALERT_REACHED,		/* add @20161020*/
	eSCH_STATE_ALERTING,		/* in the alarm */
}E_SCHEDULE_STATE;

/* directive event play state machine */
typedef enum {
	eMPLAY_STATE_IDLE,
	eMPLAY_STATE_PREPARE,
	eMPLAY_STATE_WAIT_SUBMIT,
	eMPLAY_STATE_PLAYING,			/* device have to avoid recognize speak when play sample music speak*/
	eMPLAY_STATE_STOPPED,
	eMPLAY_STATE_PAUSE,			/* device have to drop the speak when device is playing music brief suddenly a Recognize speak come in.*/
	eMPLAY_STATE_FINISHED,
}E_MPLAY_STATE;

/***********************SpeechSynthesizer Interface********************************/
typedef struct SpeakDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	char strFormat[16];						/* AUDIO_MPEG */
	char strToken[128];						/*payload*/
}S_SpeakDirective;

typedef struct ExpectSpeechDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	char strDialogRequestId[64];	//dialogRequestId
//	uint32_t strTimeoutInMilliseconds;
}S_ExpectSpeechDirective;

/***********************Alerts Interface********************************/
typedef struct SetAlertDirective {
	union{
	E_SCHEDULE_STATE eScheduledState;		/* specific a timer event */
	int bSetAlertSucceed;//0 failed, 1succeeded , 2 succeed and have been in scheduled ------->>>>by yan /* add @20161020*/
	};
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	char strToken[128];						/*payload*/
	char strType[8];						/*TIMER || ALARM*/
	char strScheduledTime[32];
}S_SetAlertDirective;
//DeleteAlert
typedef struct DeleteAlertDirective {
	char cDeleteSucceeded;		/*0 failed, >0 succeeded and == iIndex+1, >TIMER_MAX, first the same with second alert add @20161107*/
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	char strToken[128];						/*payload*/
}S_DeleteAlertDirective;

/***********************AudiaPlayer Interface********************************/
//Play
typedef struct PlayDirective{
	E_MPLAY_STATE eMplayState;
	unsigned char ui8PlayType;				/*1 - sample stream, 2 - forever stream*/ //maybe remove @20161021
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	/*payload*/
	char strPlayBehavior[16];				/* REPLACE_ALL, ENQUEUE, REPLACE_ENQUEUED */
	/*audioItem*/
	char strAudioItemId[128];
	/*stream*/
	long offsetInMilliseconds;
	long ui64Startoffset;					/* add start time*/
	long ui64CurrentOffsetInMilliseconds;	/* current frame position */
	long ui64ReportDelayMS;	 				/* progressReportDelayInMilliseconds */
	long ui64ReportIntervalMS;				/* progressReportIntervalInMilliseconds */
	char strStreamFormat[16];
	char strExpiryTime[32];
	char strUrl[512];
	char strToken[256];
}S_PlayDirective;
//Stop
typedef struct StopDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	/*payload null*/
}S_StopDirective;
//ClearQueue
typedef struct ClearQueueDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	/*payload*/
	char strClearBehavior[16];
	bool bClearAll;				/* true - clear all, false - clear queue*/	
}S_ClearQueueDirective;

/***********************Speaker Interface************************************/
//SetVolume
typedef struct SetVolumeDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	int	 i32Volume;							/*payload*/
}S_SetVolumeDirective;

//AdjustVolume
typedef struct AdjustVolumeDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	int	 i32Volume;							/*payload*/
}S_AdjustVolumeDirective;

//SetMute
typedef struct SetMuteDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	bool bMute;								/*payload*/
}S_SetMuteDirective;

//add System interface
typedef struct ResetUserInactivityDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];							/*payload*/
	long i32InactiveSec;
}S_ResetUserInactivityDirective;

typedef struct SetEndpointDirective {
	char strNamespace[32];
	char strName[32];
	char strMessageId[64];
	char strEndpoint[64];								/*payload*/
}S_SetEndpointDirective;

int RK_AVSJsonParseDirective
(
	cJSON 				*cJsonHeader,
	cJSON 				*cJsonPayload,
	void				*pAvsDirective,
	E_AVS_DIRECTIVE		eAvsDirectiveID
);

int RK_AVSCurlRequestSubmit(S_CURL_SET 	*psCurlSet, int isTimer);
void RK_AVSCurlRequestBreak(S_CURL_SET 	*psCurlSet);
int RK_AVSWaitRequestCompleteCode(S_CURL_SET *psCurlSet);
int RK_AVSCurlMutexInit(void);

int RK_AVSKeepAlive(S_KEEPALIVE *pKA, char *pData, size_t data_size);
char *RK_Random_uuid( char *uuid);

void RK_AVSSetUrl(CURL *curlHnd, char *url);
void RK_AVSConnectUrl(CURLM *mcurlHnd, CURL *curlHnd);
void RK_AVSDisconnectUrl(CURLM *mcurlHnd, CURL *curlHnd);
void RK_AVSSetupCurlHandle(CURL *curlHnd, int num);
int RK_AVSWaitHttpCode		//remove - waitresponse
(
	CURLM				*multiHandler,
	CURL 				*curlHandler,
	uint8_t				ui8timeout,
	S_AVS_CURL_SET 		*pAvsCurlSet
);

RK_CURL_SLIST *RK_AVSCurlHttpHeaderPackage(char *pstrHeaderData);
int RK_AVSPingHeader(char *pReqHeader, char *accessToken);
int RK_AVSDownChannelHeader(char *pReqHeader, char *accessToken);
void RK_AVSTokenEasyParamsSet
(
	CURL 			*curlHandler, 
	RK_CURL_SLIST	*httpHeaderSlist, 
	char 			*pData, 
	int 			data_length
);
void RK_AVSDownchannelEasyParamsSet
(
	CURL 				*curlhnd,
	RK_CURL_SLIST 	    *httpHeaderList,
	size_t 				(*write_callback)(char *pAvsData, size_t size, size_t nitems, void *stream_priv),
	void 				*psAlexaRes
);

int RK_AVSDirectivesEventParamInit(void);

void RK_AVSSpeechSynthesizerEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId,
	char	*pToken
);
/*set Alert Multipart*/
void RK_SetAlertEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId,
	char	*pToken
);

void RK_AVSPlaybackStartedEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId,
	char	*pToken,
	long	i64InMS
);
/*set PlaybackQueueCleared Event*/
void RK_AVSPlaybackQueueClearedEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId
);

/*set VolumeChanged Event*/
void RK_AVSVolumeChangedEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId,
	int		i32Vol,
	bool	bMute
);

/*set UserInactivityReport Event*/
void RK_AVSUserInactivityReportEvent
(
	char 	**strJSONout,
	char	*pNamespace,
	char	*pName,
	char	*pMsgId,
	long	i32InactiveTimeSec
);

struct curl_httppost *RK_AVSPackagePostData
(
	char *strJSONout
);

void RK_AVSDownchannelHandle
(
	S_AVS_CURL_SET 	*pAvsCurlSet, 
	char 			*pAccessToken, 
	size_t 			(*write_callback)(char *pAvsData, size_t size, size_t nitems, void *stream_priv),
	void 			*pPrivRes
);

int RK_AVSHttpReqInteraction(
	CURLM 			*pMultiCurlHnd, 
	S_AVS_CURL_SET 	*pAvsCurlSet,
	char			*pPostdata,
	int				iPostDataLen,
	int 			isHttp2
);

#endif	//__AVS_PRIVATE__H___

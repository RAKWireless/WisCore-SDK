//------------------------------------------------------------------
//
// Copyright (c) Rakwireless Technology Corp. All rights reserved.
//
//------------------------------------------------------------------

#ifndef __ALEXA_COMMON___
#define __ALEXA_COMMON___
/* curl stuff */
#include <curl/curl.h>
#include <cJSON/cJSON.h>
#include <semaphore.h>
#include "avs.h"
#include "hls.h"
#include "audio_player.h"
//#include "utils.h"

#define DEF_ALEXA_INFO
#ifdef DEF_ALEXA_INFO
#define ALEXA_INFO(...) 		fprintf(stderr, "ALEXA_INFO(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define ALEXA_INFO(...)		{;}
#endif

//#define DEF_ALEXA_TRACE
#ifdef DEF_ALEXA_TRACE
#define ALEXA_TRACE(...) 	{ fprintf(stderr, "ALEXA_TRACE(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define ALEXA_TRACE(...)	{;}
#endif

#if 1
#define ALEXA_DBG(...)		{ fprintf(stderr, "ALEXA_DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#define ALEXA_ERROR(...)	{ fprintf(stderr, " ALEXA_ERROR(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#define ALEXA_WARNING(...)	{ fprintf(stderr, " ALEXA_WARNING(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define ALEXA_ERROR(...)	{;}
#define ALEXA_DBG(...)		{;}
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define RK_NULL     0L
#define RK_SUCCESS  0
#define RK_FAILURE  (-1)

typedef void (*AvsFunCB)(void *pvPriv, char *psResData, int *datasize);

typedef enum {
	eALEXA_ERRCODE_SUCCESS				=  0,
	eALEXA_ERRCODE_FAILED				= -1,
	eALEXA_ERRCODE_WRITE_FILE_FAILED	= -2,
	eALEXA_ERRCODE_READ_FILE_FAILED		= -3,
	eALEXA_ERRCODE_FAILED_OPEN_DEV		= -4,
	eALEXA_ERRCODE_NO_VALID_RES			= -5,
	eALEXA_ERRCODE_INVALID_CMD			= -6,
	eALEXA_ERRCODE_INVALID_CMD_VALUE	= -7,
	eALEXA_ERRCODE_NETWORK_EXCEPTION	= -8,
	eALEXA_ERRCODE_TIMEOUT				= -9,
	eALEXA_ERRCODE_ABORT				= -10,
	eALEXA_ERRCODE_FAILED_ALLOCATE		= -20,
	eALEXA_ERRCODE_FAILED_INIT_MUTEX	= -21,
	eALEXA_ERRCODE_FAILED_INIT_COND_VAR	= -22,
} E_ALEXA_ERRCODE;

typedef enum {
	eALEXA_HTTPCODE_OK_200				= 200,
	eALEXA_HTTPCODE_NO_CONTENT_204		= 204,
	eALEXA_HTTPCODE_BAD_REQUEST_400		= 400,	/* The request was malformed. */
	eALEXA_HTTPCODE_UNAUTHORIZED_403	= 403,	/* The request was not authorized */
	eALEXA_HTTPCODE_MANY_REQUEST_429	= 429,	/* Too many requests to the Alexa Voice Service */
	eALEXA_HTTPCODE_AVS_EXCEPTE_500		= 500,	/* Internal service exception */
	eALEXA_HTTPCODE_AVS_UNAVAILE_503	= 503,	/* The Alexa Voice Service is unavailable. */
//	eALEXA_HTTPCODE_ABORT_997			= 997,
//	eALEXA_HTTPCODE_ABORT_998			= 998,
//	eALEXA_HTTPCODE_TIMEOUT_999			= 999,	/* curl/alexa timeout, check network problem*/
} E_ALEXA_HTTPCODE;

typedef enum alexa_state {
	eALEXA_STATE_IDLE			= 0x0000,
	eALEXA_STATE_START			= 0x0001,
	eALEXA_STATE_SPEECHSTARTED	= 0x0002,
	eALEXA_STATE_SPEECHFINISHED = 0x0004,
	eALEXA_STATE_RUNNING		= 0x0008,
	eALEXA_STSTE_CAPTURING		= 0x0010,
	eALEXA_STSTE_MIC			= 0x0020,		// mute
	eALEXA_EXPECTSPEECH			= 0x0040,
	eALEXA_MEDIA_PROMPT			= 0x0080,
	eALEXA_STSTE_NETWORK		= 0x0100,		// 1:linked; 0:breaked.
	eALEXA_STSTE_ETHERNET		= 0x0200,		// ok
	eALEXA_STSTE_USRACCOUNT		= 0x0400,		// 
	eALEXA_STSTE_TOKEN 			= 0x0800,
	eALEXA_EXPECTSPEECH_1		= 0x1000,
} E_ALEXA_STATE;

typedef enum _sysaudiotipscode{
	eSYSTIPSCODE_NONE = -1,
	eSYSTIPSCODE_ALERT_FAIL = 0,
	eSYSTIPSCODE_ALEXA_NOETH,		/* no ethertnet */
	eSYSTIPSCODE_ALEXA_NOITN,		/* no internet */
	eSYSTIPSCODE_ALEXA_NOUSR,
	eSYSTIPSCODE_ALEXA_TROBL,
	eSYSTIPSCODE_ALEXA_LOGINING,
	eSYSTIPSCODE_ALEXA_CAPTURE_START,
	eSYSTIPSCODE_ALEXA_CAPTURE_STOP,
	eSYSTIPSCODE_ALEXA_MUTE_MIC,
	eSYSTIPSCODE_ALEXA_UNMUTE_MIC,
	eSYSTIPSCODE_NUM,
} E_SYSTIPSCODE;

typedef enum _sysstate{
	eSYSSTATE_ALL,
	eSYSSTATE_ALERTTIMEUP_NUMADD,
	eSYSSTATE_ALERTALL_NUMADD,
	eSYSSTATE_ALERTTIME,
	eSYSSTATE_ETHERNET,
	eSYSSTATE_NETWORK,
	eSYSSTATE_ALEXA,
} E_SYSSTATECODE;

/* Receive AVS response information and saved to a file server */
typedef struct alexa_info {	//
	FILE 				*headerfs;
	FILE 				*bodyfs;
	char 				*header_name;
	char 				*body_name;
}S_ALEXA_DEB_FS;

typedef struct{
	char *pAlarmAudioFile;//name
	char *pTimerAudioFile;//name
	char *pAlertConfigFile;//name
	char *pAlertConfigFileTmp;//name
	int  iStopNextTime;//when one alert stop failed, after iStopNextTime seconds, the alert will stop again.
	int  iAlertTimeOut;//when device just poweron,if alert have passed iAlertTimeOut seconds, alert will stop and send ALERT_STOP_EVENT to avs. 
}S_ALERT_BASE_RES;

typedef struct avs_token {
	char				*pdsn;
	char 				*pproductId;
	FILE 				*tokenfp;
	char				*psRefreshJsonData;				/*332 maybe max size is 512*/
	char				*psClientID;
	char 				*psAccessToken;
	char 				*psRefreshToken;
	int					i32AccessTokenLength;
	int					i32RefreshTokenLength;
	uint64_t			ui64ExpireSec;
	char				change_login;
	//	char 				strTokenType[16];			/*authorize_code or Implicit , force default: authorize_code*/
} S_ALEXA_TOKEN;

//typedef struct avs_token S_ALEXA_TOKEN;

typedef struct alexa_res {
	bool				bEnableCapture;						/* trigger capture audiostream */
	bool				bWaitCapture;						/* To ensure that there is a capture of data exists in the queue */
	int 				iReadWriteStamp;					/* readable/writable be done */
	uint8_t				ui8isTokenValid;					/* a access token must be valid When you want to make a voice interaction, so first you request a access token from AVS */
	char				*pAccessToken;						/* access token */
	uint8_t 			isPromptSpeak;						//prompt speech flags
	RK_CURL_SLIST 		*headerSlist;						/* all event headerlist we need to change - mark*/
	S_KEEPALIVE			asKeepAlive;						/* only for recongzine event*/
	S_AVS_CURL_SET		asAvsCurlSet; 						/**/
	AvsFunCB			m_pfnAvsReadFunCB;					/*callback - deliver speech request to avs, User can defined implementation */
	S_MediaFormat		asMediaFormat;
	S_ALEXA_DEB_FS		*psAlexaDebFs;						/* debug file */
	E_ALEXA_STATE 		eAlexaResState;
	S_PBContext			*psPlaybackCxt;
	int					replace_behavior;					/*have a repeat*/
	void				*sAvsDirective;						/* point a current active playdirective */
} S_ALEXA_RES;

typedef struct alexa_handle {
	uint8_t				ui8Done;				/* set 1 if the request period is forced to close*/
	uint8_t				ui8Index;				/* user identify request*/
	uint8_t				isStream;				/* 0 - no stream, 1 - is mp3 stream */
	bool				bisLastPacket;			/* */
	S_ALEXA_DEB_FS		*psAlexaDebFs;		/* delect */
	S_ALEXA_RES 		*psResData;
} S_ALEXA_HND;

typedef void *(*SysStRdFunc_t)  (E_SYSSTATECODE stmember);
typedef int   (*SysStWrFunc_t)  (E_SYSSTATECODE stmember,...);
typedef void (*SysStTipsFunc_t) (E_SYSTIPSCODE stmember);

void RK_AlexaVersion(void);

int RK_APBFinished(S_PBContext *pPBCxt, E_PB_TYPE ePBType, int i32TimeoMs);
int RK_APBPause(S_PBContext *pPBCxt, E_PB_TYPE ePBType, int pause);
int RK_APBSetDmixVolume(S_PBContext *pPBCxt, E_PB_TYPE ePBType, float f16Volume);
int RK_APBWrite(S_PBContext *pPBCxt, E_PB_TYPE ePBType, unsigned char *pSrcData, int i32Length);
int RK_APBClose(S_PBContext *pPBCxt, E_PB_TYPE ePBType);
int RK_APBOpen(S_PBContext *pPBCxt, E_PB_TYPE ePBType);
void RK_APBChange_handleState(S_PBContext *pPBCxt, int state);
void RK_APBStream_open_block(bool bBlock);

int RK_BeginAudioPlayerMediaStream(void);

/********************************************************************************
Description: appends a specified string to a linked list of strings for httpheader; 
		note - Each header must be separated by '&'
pstrHeaderData: specified string
return value: RK_CURL_SLIST - success, NULL - failed, RK_CURL_SLIST use complete 
			  must be destroyed by curl_slist_free_all function.
********************************************************************************/
RK_CURL_SLIST *RK_CurlHttpHeaderPackage(char *pstrHeaderData);

void RK_SysFuncSet(void * sysrd, void* syswr, void * systips);

/*token callback , User can customize*/
size_t RK_TokenDataFromAvsCB(char *pAvsData, size_t size, size_t nmemb, void *stream_priv);

RK_CURL_HTTPPOST *RK_SetRecognizePostData
(
	char *strJSONout, 
	void *pPrivData, 
	unsigned int i32Length
);

void RK_SetEasyOptionParams
(
	CURL 				*curlhnd,
	RK_CURL_SLIST 		*httpHeader, 
	RK_CURL_HTTPPOST 	*postData, 
	void 				*pPrivData,
	bool				bEnable
);

void RK_AlexaPriorityForeground(void);
void RK_AlexaPriorityBackground(void);

void RK_StartRecognizeSendRequest
(
	S_ALEXA_HND *psAlexaHnd
);

/* set Recognize Multipart*/
void RK_SetRecognizeEvent(char **strJSONout, int enable_stop);
/**
 * Description: use this function to create a multi handle and returns a CURLM handle 
 * to be used as input to all the other multi-functions, sometimes referred to as a 
 * multi handle in some places in the documentation. This init call MUST have a 
 * corresponding call to curl_multi_cleanup when the operation is complete.
 * @params: NULL
 * @return : a CURLM handle
*/
CURLM *RK_CurlMulitInit(void);
/**
 * Description: Cleans up and removes a whole multi stack. It does not free or 
 * touch any individual easy handles in any way - they still need to be closed 
 * individually, using the usual curl_easy_cleanup way
 * @params mcurlHnd: a vaild CURLM handle
 * @return : null
*/
void RK_CurlMulitCleanup(CURLM *mcurlHnd);
/**
 * Description: use this function to create an easy handle and must be the first function to call,
 * and it returns a CURL easy handle that you must use as input to other functions in the easy 
 * interface
 * @params: NULL
 * @return : a CURL handle
*/
void RK_CurlEasyInit(S_AVS_CURL_SET *pAlexaCurlSet);
/**
 * Description: this function must be the last function to call for an easy session. 
 * It is the opposite of the curl_easy_init function and must be called with the same
 * handle as input that a curl_easy_init call returned.
 * @params curlHnd - a vaild CURL handle
 * @return : null
*/
void RK_CurlEasyCleanup(S_AVS_CURL_SET  *pAlexaCurlSet);
/**
 * Description: use this function to return a httpcode from http interaction
 * @param: multiHandler - a Alexa multi handle
 * @param: pAvsCurlSet - a sets of CURL easy handle
 * @param: uiTimeoSec - allow to wait response code time
 * @return : a vaild http code
*/
int RK_WaitResponseCode1
(
	S_CURL_SET *psCurlSet
);

int RK_GetEventHeader(char *pReqHeader, char *accessToken);
void RK_GetEventHeaderList(RK_CURL_SLIST **pHeaderSlist, char *pAToken);


int RK_WaitResponseCode1(S_CURL_SET *psCurlSet);

int RK_AlexaRequestCheckState(S_AVS_CURL_SET 	*pAvsCurlSet);

void RK_AlexaReqestBegin(S_CURL_SET 	*psCurlSet, int isTimer);

void RK_AlexaRequestDone(S_CURL_SET	*psCurlSet);

void RK_AlexaMasterWorkHandler(void *psResData);

int RH_AlexaReqToken
(
	S_CURL_SET  *pAvsCurlSet,
	S_ALEXA_TOKEN   *pAlexaToken,
	int (*avstoken_callback)(S_ALEXA_TOKEN *psAlexaToken)
);


/**
 * Description: use this function to send all components directives event to avs, 
 * the function is block and still listen directives event come in and handle it 
 * until the alexa be terminated.
 * @param: psAlexaRes - Alexa resource data
 * @return : NULL
*/
void RK_AlexaDirectivesHandle
(
	S_ALEXA_RES *psAlexaRes
);
void RK_Alert_Initial
(
	void * arg
);
void RK_Alert_Reset(S_ALEXA_RES *pAlexaResData);
int RK_AlertRes_Set(void * arg);
#endif	//__ALEXA_COMMON___

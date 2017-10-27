//------------------------------------------------------------------
//
// Copyright (c) Rakwireless Technology Corp. All rights reserved.
//
//------------------------------------------------------------------

#ifndef __ALEXA_COMMON___
#define __ALEXA_COMMON___
#include <stdint.h>

#include "audio_playerItf.h"	/* audio player interface*/

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define RK_NULL     0L
#define RK_SUCCESS  0
#define RK_FAILURE  (-1)

#undef bool
#undef false
#undef true
#define bool	uint8_t
#define false	0
#define true	(!false)

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
	eALEXA_HTTPCODE_OK_200				= 200,	/* HTTP Success with reponse payload. */
	eALEXA_HTTPCODE_NO_CONTENT_204		= 204,	/* HTTP Succcess with no response payload. */
	eALEXA_HTTPCODE_BAD_REQUEST_400		= 400,	/* The request was malformed. */
	eALEXA_HTTPCODE_UNAUTHORIZED_403	= 403,	/* The request was not authorized */
	eALEXA_HTTPCODE_MANY_REQUEST_429	= 429,	/* Too many requests to the Alexa Voice Service */
	eALEXA_HTTPCODE_AVS_EXCEPTE_500		= 500,	/* Internal service exception */
	eALEXA_HTTPCODE_AVS_UNAVAILE_503	= 503,	/* The Alexa Voice Service is unavailable. */
} E_ALEXA_HTTPCODE;

/*Alexa status*/
typedef enum alexa_state {
	eALEXA_STATE_IDLE				= 0x0000,
	eALEXA_STATE_RECOGNIZE_START	= 0x0001,		/* Indicates Recognize Evt that Alexa enters to send user speech to Amazon AVS */
	eALEXA_STATE_RECOGNIZE_BREAK	= 0x0002,		/* REMOVE If a user barges in and interrupts Alexa, we won't need to sent SpeechFinished Event*/
	eALEXA_STATE_EXPECTSPEECH       = 0x0004,       /* recved expect speech from avs*/
	eALEXA_STATE_NETWORK            = 0x0008,       /* network linked*/
	eALEXA_STATE_USRACCOUNT         = 0x0010,       /* usr have logined*/
	eALEXA_STATE_TOKEN              = 0x0020,       /* token is valid*/
	eALEXA_STATE_REGISTER_ONLINE    = 0x0040,       /* Register server to get access permission */
} E_ALEXA_STATE;

typedef struct{
	char *pAlarmAudioFile;//name
	char *pTimerAudioFile;//name
	char *pAlertConfigFile;//name
	char *pAlertConfigFileTmp;//name
	int  iStopNextTime;//when one alert stop failed, after iStopNextTime seconds, the alert will stop again.
	int  iAlertTimeOut;//when device just poweron,if alert have passed iAlertTimeOut seconds, alert will stop and send ALERT_STOP_EVENT to avs. 
}S_ALERT_BASE_RES;

enum token_method {
	eTOKEN_METHOD_APP,
	eTOKEN_METHOD_WEB
};

typedef struct auth_conf {
    uint16_t u16TlsPort;//端口号：9017
	char *pTlsServerIP;//服务器IP地址
	char *pTlsDomainName;//服务器域名
	char *pTlsCertName;//服务端证书
	char *pTlsClientCertName;//客户端证书
	char *pTlsClientKeyName;//客户端密钥
	char *pUsrerID;//RAK颁发的客户唯一识别码
	char *pLicenseSN;//RAK颁发的设备序列号
} S_AUTH_CONF;

typedef struct avs_token {
	char				*pClientID;					/* client encode, readonly for usrer*/
	char                *pClientSecret;				/* client secret, readonly for usrer*/
	char 				*pRefreshToken;             /* refresh token, readonly for usrer*/
} S_ALEXA_TOKEN;

typedef struct alexa_res {
	E_ALEXA_STATE 		eAlexaResState;
	void 				*m_AvsCtx;
} S_ALEXA_RES;

void RK_AlexaVersion(void);
int RK_AlexaGlobalInit(S_ALEXA_RES *psAlexaRes);
void RK_AlexaGlobalDestory(S_ALEXA_RES *psAlexaRes);
void RK_AlexaLogout(S_ALEXA_RES *psAlexaRes);

/* Description:
 * user below params to change their language settings
 * @params pLocale - acceptable parameters are one of them "en-US", "en-GB", "de-DE"
 * return value - error return negative, otherwise return positive.
 * note: you need call RK_AlexaGetLocation to get the language to judge the setting is successed or failed.
 */
int RK_AlexaSetLocation(const char *pLocale);

/*
 * Description: get the current language
 * @param NULL 
 * return value:  NULL, the language is setting, otherwise, return "en-US", "en-GB" or "de-DE";
 */
char * RK_AlexaGetLocation(void);

void RK_AlexaPrepareRecognizeBegin(S_ALEXA_RES *psAlexaRes);
void RK_AlexaPrepareRecognizeEnd(S_ALEXA_RES *psAlexaRes);
void RK_AlexaPlaybackForeground(S_ALEXA_RES *psAlexaRes);
void RK_AlexaPlaybackBackground(S_ALEXA_RES *psAlexaRes);

int RK_AlexaSetupPlaybackInterface(S_ALEXA_RES *psAlexaRes, S_AudioPlayback *psPlaybackItf);
int RK_AlexaSetupPlaybackAttr(S_ALEXA_RES *psAlexaRes, E_PB_TYPE ePbType, void *userPtr);
int RK_AlexaPlaybackStartedNotify(S_ALEXA_RES *psAlexaRes, E_PB_TYPE ePbType);
int RK_AlexaPlaybackFinishedNotify(S_ALEXA_RES *psAlexaRes, E_PB_TYPE ePbType);

void RK_StartRecognizeSendRequest
(
	S_ALEXA_RES *psAlexaRes
);

int RK_generateCodeVerifier(unsigned char *ucCodeVerifier);
int RK_generateCodeChallenge
(
	const unsigned char* ucCodeVerifier,
	const int iCodeVerifierLen,
	unsigned char* ucCodeChallenge
);

int RK_WaitResponseCode1(S_ALEXA_RES *psAlexaRes, int * pi32ResponseCode);

int RK_AlexaServiceRegister(S_AUTH_CONF *pTlsConf);

/* @psAlexaRes - alexa context resource
 * deliver_func - this a delivers usr utterance function by avs call
 * 	@param private data - this a usr private data
 * 	@param avbuffer - this is a cache that delivers usr utterance
 * 	@bstopcapture - indicates whether usr utterance is allowed to be delivered
 *					when the param is true, avs will not allow to deliver data
 * 	return - avbuffer size or zero when avs not allow to deliver data
 * @param priv_usrdata - usr private data pointer to be used by the deliver_func callback
 * return - success 0 or failure -1
 */
int RK_AlexaSetSpeechDeliverItf
(
	S_ALEXA_RES *psAlexaRes,
	size_t (*deliver_func)(void *priv_usrdata, char *avbuffer, bool bstopcapture),	
	void	*priv_usrdata
);

int RK_AlexaMasterWorkHandler(S_ALEXA_RES *psResData);
int RH_AlexaTokenResume(S_ALEXA_RES *pAlexaRes, S_ALEXA_TOKEN *psAlexaToken, int i32Method);

int RH_AlexaAuthorize
(
	S_ALEXA_RES		*pAlexaRes,
	char            *pLoginInfoString,
	int				i32Method,
	int (*avstoken_callback)(S_ALEXA_TOKEN *psAlexaToken)
);

/*******************************************************************************
 * Description: Query whether speech is expected again.
 * @pAlexaRes:
 * @return: SUCCESS return RK_SUCCESS, FAILED return RK_FAILED
*/
int RK_AlexaEnquireExpectspeech(S_ALEXA_RES *pAlexaRes);

int RK_AlertRes_Set(S_ALEXA_RES *pAlexaResData, S_ALERT_BASE_RES *psAlertConf);
#endif	//__ALEXA_COMMON___

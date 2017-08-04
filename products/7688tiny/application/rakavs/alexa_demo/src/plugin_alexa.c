#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
/* somewhat unix-specific */
#include <sys/time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <syslog.h>

//#include "alarm_timer.h"
//#include "alexa_time.h"
#include "alexa_alert.h"

#include "alexa_common.h"
#include "audio_capture.h"
#include "aplay/aplay.h"
#include "audio_player.h"
#include "audio_queue.h"
#include "plugin_alexa.h"
#include "vprocTwolf_access.h"

#include "RKIoctrlDef.h"
#include "RKGpioApi.h"
#include "led.h"


#define DEF_ENABLE_PB	

#define ALEXA_CMD_NONE 				0
#define ALEXA_CMD_START				0x01
#define ALEXA_CMD_STOP				0x02
#define ALEXA_CMD_RESTART			0x03

#define SYS_AUDIO_TIPS				0x01
#define SYS_VISUAL_TIPS				0x02
volatile uint8_t g_uisys_tips[eSYSTIPSCODE_NUM]   = {0};
volatile time_t start_tips_time[eSYSTIPSCODE_NUM] = {0};
volatile time_t alexa_capture_time = 0;
volatile int break_to_start = 0;
S_LED_t gs_led;

typedef enum{
	eTRIG_WAY_IDLE,
	eTRIG_WAY_BUTTON,
	eTRIG_WAY_VOICE,
}E_TRIG_WAY_t;

//#define ALEXA_TRIGGER_PIN			39		/* gpio39 alexa wakeup pin */
#define PLUGIN_REQUEST

#ifndef ASR_SND_DETECT_PIN
#define ASR_SND_DETECT_PIN 			19		/* gpio19 wisalexa wakeup pin */
#endif
#ifndef LED_PIN
#define LED_PIN 			37		/* gpio37 wisalexa status led pin */
#endif

//
typedef enum _triggermode{
	eGPIO_TRIGGER_MODE_NONE  = 0,
	eGPIO_TRIGGER_MODE_RISING,
	eGPIO_TRIGGER_MODE_FALLING,
	eGPIO_TRIGGER_MODE_BOTH,
	eGPIO_TRIGGER_MODE_QUERY,
}E_TRIGGER_MODE;

//// asr detect   ////
enum{
	eSOUND_REG_EVENT 		= 0x000,
	eSOUND_REG_EVENT_ID 	= 0x002,
};

#define eSOUND_EVENT_ID_ASR 0x6000
#define eSOUND_EVENT_ID_VAD 0x0009
	
/* end */
//

extern S_AudioPlayback rk_audio_playback; 

typedef struct alexa_context {
	S_AUDIO_PARAM 	psAudioParam;	/* try to use point*/
	S_ALEXA_HND		sAlexaHnd; 
}S_ALEXA_CONTEXT;
typedef struct alexa_auth{
	unsigned char codeVerifier[CODE_VERIFIER_MAX_LEN];
	unsigned char codeChallenge[CODE_CHALLENGE_LEN];
	
	char encodeMode[10];
	char productID[64];
	char dsn[64];
}S_ALEXA_AUTH;
// -------------------------------------------------------------------
// Globe data
// -------------------------------------------------------------------
S_ALEXA_CONTEXT	g_sAlexaContext;
S_ALEXA_TOKEN 	g_sAlexaToken;
S_ALEXA_RES		*g_sAlexaResData;
PacketQueue		QCapture;

volatile uint32_t	g_uiQuit		= 0;			// Flag to quit
/* debug remove*/
int speechfile;
// --------------------------------------------------------------------
// Private data
// --------------------------------------------------------------------
static uint32_t 		g_u32CmdCode;				//ALEXA command code
static uint32_t 		g_ui32Mute = 0;				/* enable alexa function when mute button isn't press otherwise we have to disable alexa function*/
static	sem_t			s_sCmdCaptureSem;			//streamer command capture semaphore
static volatile int 	g_ui32NetwortIsNomal=-1;    //Just record the network state when it just boot.Ö»¼ÇÂ¼¸Õ¿ª»úÊ±µÄÍøÂç×´Ì¬
typedef enum _spicode{
	eSPICODE_SND_START_PB,
	eSPICODE_SND_SMPLRATE,
	eSPICODE_SND_ALEXA_STATUS,
	eSPICODE_SND_NETWORK_DISCONNECT,
	eSPICODE_SND_RFTOKEN_SUCCESS,
} E_SPICODE;

#ifdef ALEXA_FILE_DEBUG
S_ALEXA_DEB_FS g_sAlexa_DebFs = {
	.headerfs 	= NULL,
	.bodyfs		= NULL,
	.header_name= "/tmp/HeadFile",
	.body_name	= "/tmp/BodyFile",
};
#endif
static void dialog_view(void)
{
	S_LED_t ledt = gs_led;
	if(ledt.ledmode == LED_FLASH){
		ledt.ledflashfreq = 2;
		led_set(ledt);
		gs_led = ledt;
	}
}

void RK_sysStatusCues(E_SYSTIPSCODE status)
{
	S_LED_t ledt = gs_led;
	S_ALEXA_RES		* pAlexaResData = g_sAlexaResData;
	switch(status){
		case eSYSTIPSCODE_ALEXA_CAPTURE_START:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 100;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/triggertip.wav";
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_CAPTURE_STOP:{
			if(ledt.ledmode == LED_SIMPLE && ledt.ledlumin == 100){
				ledt.ledmode = LED_FLASH;
				ledt.ledlumin = 100;
				ledt.ledflashfreq = 8;
				led_set(ledt);
				gs_led = ledt;
			}
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/triggertip.wav";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_NOETH:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/cont_connect_to_your_network.mp3";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_NOITN:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/lost_the_intnet_try_again.mp3";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_NOUSR:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/no_alexa_user.wav";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_LOGINING:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/welcome_to_wisalexa.wav";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_TROBL:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/i_have_a_trouble_to_understand_your_quest.mp3";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALERT_FAIL:{
			ledt.ledmode = LED_SIMPLE;
			ledt.ledlumin = 0;
			led_set(ledt);
			gs_led = ledt;
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/the_alert_was_set_unsuccessfully.wav";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_MUTE_MIC:{
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/mic_off";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		case eSYSTIPSCODE_ALEXA_UNMUTE_MIC:{
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].strFileName = "/usr/lib/sound/mic_on";
			pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
			RK_APBOpen(pAlexaResData->psPlaybackCxt, ePB_TYPE_SYSTIP);
		}break;
		default:
			break;
	}
}

#if 0
S_ALEXA_AUTH RK_initAlexaAuthForApp(void)
{
	S_ALEXA_AUTH auth = {0};
	FILE * fp = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	char str[64];
	while(fgets(str, 63, fp)>0){
		if(!strncmp(str,"avs_device", 10)){
			strcpy(auth.productID,str+10+3);
		}
		if(!strncmp(str,"avs_dsn", 7)){
			strcpy(auth.dsn,str+7+3);
		}
	}
	return auth;
}

int RK_setAlexaAuthForApp(S_ALEXA_AUTH*auth)
{
	if(!auth)
		return -1;
	int vlen = generateCodeVerifier(auth->codeVerifier);
	if(vlen < 43)
		return -1;
	int clen = generateCodeChallenge(auth->codeVerifier, vlen, auth->codeChallenge);
	if(clen < 0)
		return -1;
	return 0;
}


char * generateCjsonStringForApp(S_ALEXA_AUTH*arg)
{
	S_ALEXA_AUTH * authforapp = (S_ALEXA_AUTH*)arg;
	cJSON* root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "codeChallenge", authforapp->codeChallenge);
	cJSON_AddStringToObject(root, "codeVerifier", authforapp->codeVerifier);
	cJSON_AddStringToObject(root, "productID", authforapp->productID);
	cJSON_AddStringToObject(root, "encodeMode", authforapp->encodeMode);
	char* ret = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	return ret;
}
#endif

static int HI_ChgDspDataLine(int beginend)
{
	//U_MSF_PLUGIN_CMD	uiCmd = {.m_uiCmd = eMSP_PLAYER_CMD_ALEXA_CAPTURE};
	//return pglobal->m_asPluginPriv[eMSF_PLUGIN_ID_PLAYER].m_pPluginIF->m_pfnCommand(uiCmd, &beginend, NULL);
	return 0;
}


static E_ALEXA_STATE HI_AlexaSimpleJudgeResState(E_ALEXA_STATE eAlexaResState)//
{
	E_ALEXA_STATE state = eALEXA_STATE_IDLE;

	if((eAlexaResState & eALEXA_STSTE_MIC)){
		state = eALEXA_STSTE_MIC;
	}else if(!(eAlexaResState & eALEXA_STSTE_ETHERNET)){
		state = eALEXA_STSTE_ETHERNET;
	}else if(!(eAlexaResState & eALEXA_STSTE_NETWORK)){
		state = eALEXA_STSTE_NETWORK;
	}else if(!(eAlexaResState & eALEXA_STSTE_USRACCOUNT)){
		state = eALEXA_STSTE_USRACCOUNT;
	}else if(!(eAlexaResState & eALEXA_STSTE_TOKEN)){
		state = eALEXA_STSTE_TOKEN;
	}
	return state;
}

static uint32_t HI_AlexaTriggerJudgeCmd(E_ALEXA_STATE simpleState, int capturing, E_TRIG_WAY_t triggerType)//
{
	if(simpleState)return ALEXA_CMD_STOP;
	int cmd;
	if(capturing){
		cmd = ((triggerType==eTRIG_WAY_BUTTON)? ALEXA_CMD_STOP:((triggerType==eTRIG_WAY_VOICE)?ALEXA_CMD_NONE : ALEXA_CMD_START));
		break_to_start = 1;
	}else{
		cmd = (time(NULL) - start_tips_time[eSYSTIPSCODE_ALEXA_CAPTURE_STOP] >1)? ALEXA_CMD_START : ALEXA_CMD_NONE;
	}
	return cmd;
}

static void SpeechDeliverToAVS(void *pvPriv, char *psResData, int *datalen)
{
	S_AVPacket 	CapturePkt;
	int ret;
	do{
		
	ret = RK_Queue_Packet_Get(&QCapture, &CapturePkt, 1);
	if(ret <= 0){
		*datalen = 0;
		break;
	}

	if(CapturePkt.lastpkt == 1){
		RH_Free_Packet(&CapturePkt);
		*datalen = 0;
		break;
	}

	if(g_sAlexaResData->bEnableCapture == false && (QCapture.nb_packets < 12)){//after stopping capture, the last 12 packets data will be dropped, usr can drop the code accordding to themselves
		RH_Free_Packet(&CapturePkt);
		*datalen = 0;
		printf(" fffffff- %d\n", QCapture.nb_packets);
		break;
	}

	memcpy(psResData, CapturePkt.data, CapturePkt.size);
	*datalen = (int)CapturePkt.size;
	RH_Free_Packet(&CapturePkt);
	}while(0);
}

static void RH_InitAlexaConfig(S_ALEXA_TOKEN *psAlexaToken)
{
	if(psAlexaToken == NULL)
		return;

	if(psAlexaToken->psClientID != NULL){
		*psAlexaToken->psClientID = 0;
		free(psAlexaToken->psClientID);
		psAlexaToken->psClientID = NULL;
	}
	if(psAlexaToken->psRefreshToken != NULL){
		*psAlexaToken->psRefreshToken = 0;
		free(psAlexaToken->psRefreshToken);
		psAlexaToken->psRefreshToken = NULL;
	}
	if(psAlexaToken->psAccessToken != NULL){
		*psAlexaToken->psAccessToken = 0;
		free(psAlexaToken->psAccessToken);
		psAlexaToken->psAccessToken = NULL;
	}
	psAlexaToken->i32RefreshTokenLength = 0;
	psAlexaToken->i32AccessTokenLength = 0;

}


static int RH_ReadAlexaConfig(S_ALEXA_TOKEN *psAlexaToken)
{
	FILE 	*fconf = NULL;
	char 	fbuf[1024] = {0};
	char 	*pTmpPtr = NULL;
	int 	ret = 0;
	
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	if(fconf == NULL){
		printf("%s %s %d:""fopen "DEF_USR_BIND_DEVICE_CONF_FILE_NAME" - %m\n", __FILE__, __func__, __LINE__);
		ret = eALEXA_ERRCODE_FAILED_OPEN_DEV;
		return ret;
	}

	while((fgets(fbuf, sizeof(fbuf)-1, fconf)) != NULL){
		//printf("fbuf:%s, len:%d\n", fbuf, strlen(fbuf));
		if((pTmpPtr = strstr(fbuf, "avs_clientid")) != NULL){
			pTmpPtr += strlen("avs_clientid");
			psAlexaToken->psClientID = (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->psClientID != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->psClientID);	
				//printf("avs_clientid:%s\n", psAlexaToken->psClientID);
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_refresh")) != NULL){
			pTmpPtr += strlen("avs_refresh");
			psAlexaToken->psRefreshToken = (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->psRefreshToken != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->psRefreshToken);
				psAlexaToken->i32RefreshTokenLength = strlen(psAlexaToken->psRefreshToken);
				printf("avs_refresh:*%s*\n", psAlexaToken->psRefreshToken);
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_dsn")) != NULL){
			pTmpPtr += strlen("avs_dsn");
			psAlexaToken->pdsn= (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->pdsn != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->pdsn);
				//printf("avs_refresh:*%s*\n", psAlexaToken->psRefreshToken);
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_device")) != NULL){
			pTmpPtr += strlen("avs_device");
			psAlexaToken->pproductId= (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->pproductId != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->pproductId);
				//printf("avs_refresh:*%s*\n", psAlexaToken->psRefreshToken);
			}
		}
	}
	
	fclose(fconf);
	if(psAlexaToken->pdsn && psAlexaToken->pproductId && psAlexaToken->pdsn[0] && psAlexaToken->pproductId[0])
		return ret;
	return eALEXA_ERRCODE_FAILED;

}

int creatpath(char *path, mode_t dirmode, mode_t filemode)
{

	char pathbuf[256] = {0}, *bufpointer = pathbuf;
	char *pdirnext = NULL;
	char *pdir = path;
	if(pdir[0]=='/'){
		pathbuf[0]='/';
		bufpointer++;
	}
	while((pdirnext = strstr(pdir, "/"))){
		if(pdirnext==pdir){
			pdir = pdirnext+1;
			continue;
		}
		strncat(bufpointer, pdir,  (pdirnext - pdir));
		bufpointer += (pdirnext - pdir);
		if(access(pathbuf, F_OK)<0){
			if (mkdir(pathbuf, dirmode) < 0){
				printf("%s %s line:%d mkdir %s error£º%m\n", __FILE__, __func__, __LINE__, pathbuf);
				return -1;
			}
		}
		*bufpointer='/';
		bufpointer++;
		pdir = pdirnext+1;
	}
	int ret = 0;
	if(access(path, F_OK)<0){
		if(filemode){
			if(creat(path, filemode)<0){
				ret = -1;
				printf("%s %s line:%d mkdir %s error£º%m\n", __FILE__, __func__, __LINE__, path);
			}
		}else{
			if (mkdir(path, dirmode) < 0){
				ret = -1;
				printf("%s %s line:%d mkdir %s error£º%m\n", __FILE__, __func__, __LINE__, path);
			}
		}
	}else{
		printf("target is exist\n");
		ret = 1;
	}
	return ret;
}

static int RH_WriteAlexaConfig(S_ALEXA_TOKEN *psAlexaToken)
{
	FILE	*fconf = NULL;
	char	fbuf[1024] = {0};
	char	*pTmpPtr = NULL;
	int 	ret = 0;
	
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
	if(fconf == NULL){
		if(creatpath(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", 0755, 0666)>=0){
			fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
			if(fconf == NULL){
				ret = eALEXA_ERRCODE_FAILED_OPEN_DEV;
				return ret;
			}
		}
	}
	if(psAlexaToken->pdsn && \
		psAlexaToken->pdsn[0] && \
		psAlexaToken->pproductId && \
		psAlexaToken->pproductId[0])
	{
		fprintf(fconf, "avs_dsn #=%s\n", 		(psAlexaToken->pdsn));
		fprintf(fconf, "avs_device #=%s\n", 	(psAlexaToken->pproductId));
		if(psAlexaToken->psClientID && psAlexaToken->psClientID[0])
			fprintf(fconf, "avs_clientid #=%s\n", 	(psAlexaToken->psClientID));
		if(psAlexaToken->psRefreshToken && psAlexaToken->psRefreshToken[0])
			fprintf(fconf, "avs_refresh #=%s\n", 	(psAlexaToken->psRefreshToken));
	}else{
		ret = eALEXA_ERRCODE_NO_VALID_RES;
	}


	fclose(fconf);
	remove(DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	rename(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
		
	sync();

	return ret;
}

static void RH_CleanAlexaConfig(S_ALEXA_TOKEN *psAlexaToken)
{
	if(!psAlexaToken)return;
	FILE * fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
	if(!fconf){
		printf("%s %s %d ""fopen "DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new error:%m\n", __FILE__, __func__, __LINE__);
		return;
	}
	if(!(psAlexaToken->pdsn && psAlexaToken->pproductId)){fclose(fconf);return;}
	fprintf(fconf, "avs_dsn #=%s\n", psAlexaToken->pdsn);
	fprintf(fconf, "avs_device #=%s\n", psAlexaToken->pproductId);
	
	fclose(fconf);
	remove(DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	rename(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	sync();
}

static int RH_CreateAlexaConfig(void)
{
	FILE	*wisalexainfo;
	char 	fbuf[1024] = {0};
	
	wisalexainfo = fopen(DEF_DEVICE_INFO_PATH, "rb");
	if(!wisalexainfo){
		return -1;
	}
	while((fgets(fbuf, sizeof(fbuf)-1, wisalexainfo)) != NULL){
		if(!strncmp(fbuf, "serial_num", 10)){
			break;
		}
		fbuf[0] = 0;
	}
	int len = strlen(fbuf);
	if(fbuf[len-1] == '\n') fbuf[len-1]=0;
	if(fbuf[len-2] == '\n') fbuf[len-1]=0;
	if(fbuf[len-2] == '\r') fbuf[len-1]=0;
	
	S_ALEXA_TOKEN AlexaToken;
	AlexaToken.pproductId = DEF_PRODUCT_ID;
	
	if(strlen(fbuf)>13)
		AlexaToken.pdsn = fbuf+13;
	RH_WriteAlexaConfig(&AlexaToken);
	
	fclose(wisalexainfo);
	sync();
	return 0;
}

int HI_CaptureStream(snd_pcm_t *handle, S_AUDIO_PARAM *ps_AudioParam, size_t captureBytes, S_ALEXA_RES *pAlexaResData)
{
	S_AU_STREAM_PRIV *psAuStream = &ps_AudioParam->hwparams.sAuStreamPriv;
	int fd;
	off64_t count, rest;		/* number of bytes to capture */
	size_t chunk_bytes = psAuStream->uiBufbytes;
	size_t bits_per_frame = psAuStream->uiBits_per_frame;
	u_char *audiobuf = NULL;
	u_char *monoBuf	= NULL;
	int monoSize;
	int timeo;
	/*set capture audio queue params*/
	S_AVPacket AudioPkt;

	/* init queue params */
	//RK_Queue_Packet_Flush(&QCapture);
	
	count = (off64_t)captureBytes;
	rest = count;
	audiobuf = (u_char *)malloc(chunk_bytes);
	monoBuf = (u_char *)malloc(chunk_bytes/2);
	
	/* repeat the loop when format is raw without timelimit or
	* requested counts of data are recorded
	*/
	/* capture */
	timeo = 0;
	while ((rest > 0) && (pAlexaResData->bEnableCapture)) {
		size_t c = (rest <= (off64_t)chunk_bytes) ?
			(size_t)rest : chunk_bytes;
		size_t f = c * 8 / bits_per_frame;	/* monoSize:666 c:1332, f:333*/
		if (RH_AudioPcmRead(handle, audiobuf, f) != f)
			break;
		/* start stero conver mono */
		monoSize = steroConverMono((char *)audiobuf, c, (char *)monoBuf);
		/* calloc mem for queue */
		AudioPkt.data = (unsigned char *)calloc(monoSize, sizeof(unsigned char));
		if(AudioPkt.data == NULL){
			printf("Capture mem alloc failed\n");
			break;
		}
		memcpy(AudioPkt.data, monoBuf, monoSize);
		AudioPkt.size = monoSize;
		AudioPkt.lastpkt = 0;
		RK_Queue_Packet_Put(&QCapture, &AudioPkt, 1);	/* write queue non block */

		if(pAlexaResData->bWaitCapture){
			timeo++;
			if(timeo > 30)	/* 200*666*/
				pAlexaResData->bWaitCapture = false;
		}
		
		count -= c;
		rest -= c;
	}
		AudioPkt.data = (unsigned char *)calloc(1, sizeof(unsigned char));
		AudioPkt.size = 1;
		AudioPkt.lastpkt = 1;
		RK_Queue_Packet_Put(&QCapture, &AudioPkt, 1);	/* write queue non block */

	/*sure waitcapture variables is false when capture finished */
	if(pAlexaResData->bEnableCapture == true)
		pAlexaResData->bEnableCapture = false;

	free(audiobuf);
	free(monoBuf);

	return 0;
}

int HI_CaptureStreamFromFile(void)
{
	FILE		*fp;
	S_AVPacket 	AudioPkt;
	int 		size, monoSize=666;
	
//	RK_Queue_Packet_Flush(&QCapture);

	printf("%s %d +++\n", __func__, __LINE__);
	fp = fopen("/usr/bin/16kk.raw", "rb");
	if(fp == NULL){
		printf("open capture file is failed!\n");
		return -1;
	}
	do{
	AudioPkt.data = (unsigned char *)calloc(monoSize, sizeof(unsigned char));
	if(AudioPkt.data == NULL){
		printf("Capture mem alloc failed\n");
		break;
	}
	size = fread(AudioPkt.data, 1, monoSize, fp);
	if(size <= 0)
		break;
			AudioPkt.size = size;
		AudioPkt.lastpkt = 0;
		RK_Queue_Packet_Put(&QCapture, &AudioPkt, 0);	/* write queue */
	}while(1);
	
	fclose(fp);

	return 0;
}

int HI_AlexaCaptureAudioV4(S_AUDIO_PARAM *psAudioParam, S_ALEXA_RES 	*pAlexaResData)
{
	//S_AUDIO_PARAM		*psAudioParam = pAudioParam;
	S_AU_STREAM_PRIV	*psAuStream = &psAudioParam->hwparams.sAuStreamPriv;
	snd_pcm_t 			*handle;
	size_t 				captureBytes;
	int 				ret = 0;
	
	if(speechfile == 1){
		HI_CaptureStreamFromFile();
		pAlexaResData->bWaitCapture = false;
    }else{
		ret = RH_AudioOpenDev(&handle, psAudioParam);
		if(ret != 0)
			return eALEXA_ERRCODE_FAILED_OPEN_DEV;
		
		/* set capture audio time and calculate audio size */
		ret = RH_AudioCaptureBytes(&captureBytes, 12);
		RH_AudioSetParams(handle, psAudioParam, psAuStream);
		HI_CaptureStream(handle, psAudioParam, captureBytes, pAlexaResData);
		RH_AudioCloseDev(handle, psAudioParam);
	}
	return ret;
}

void *HI_AlexaCaptureThread(void *arg)
{
	S_ALEXA_RES 	*pAlexaResData		= (S_ALEXA_RES *)arg;

	int 	ret;

	/* initialization capture related param */
	sem_init(&s_sCmdCaptureSem, 0, 0);
	//RK_InitCaptureParam(&g_sAlexaContext.psAudioParam);
	RK_Queue_Init(&QCapture);
	
	do{

	ALEXA_DBG(" Wait Capture...\n");
	pAlexaResData->eAlexaResState &= ~eALEXA_STSTE_CAPTURING;
	sem_wait(&s_sCmdCaptureSem);
	ALEXA_DBG(" Start Capture ...\n");
	RK_sysStatusCues(eSYSTIPSCODE_ALEXA_CAPTURE_START);
	pAlexaResData->eAlexaResState |= eALEXA_STSTE_CAPTURING;
	
	alexa_capture_time = time(NULL);
	RK_Queue_Packet_Flush(&QCapture);
	
	/*start to switch i2s clk for hive prj */
#ifdef DEF_ENABLE_PB
	/* set softvolume to low for alexa background voice */
	RK_APBChange_handleState(pAlexaResData->psPlaybackCxt, 1);
	RK_APBClose(pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG);
#endif

	if(speechfile == 0)
		RK_InitCaptureParam(&g_sAlexaContext.psAudioParam);
	
	/*start to capture user request sound */
	ret = HI_AlexaCaptureAudioV4(&g_sAlexaContext.psAudioParam, pAlexaResData);
	if(!break_to_start){
		start_tips_time[eSYSTIPSCODE_ALEXA_CAPTURE_STOP] = time(NULL);
		RK_sysStatusCues(eSYSTIPSCODE_ALEXA_CAPTURE_STOP);
	}else{
		break_to_start = 0;
	}
	}while(1);
}

void *HI_AlexaTcpTriggerThread(void *arg)
{
	struct sockaddr_in addr, client_addr;
	int socket_sd, u32Port = 5000;
	int on;

	socklen_t addr_len = sizeof(struct sockaddr_in);

	/* open socket for server */    
	socket_sd = socket(PF_INET, SOCK_STREAM, 0);
	if ( socket_sd < 0 ) {
		fprintf(stderr, "socket failed\n");    	
		exit(EXIT_FAILURE);  	
	}	
	
	/* ignore "socket already in use" errors */	
	on = 1;	
	if (setsockopt(socket_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {		
		perror("setsockopt(SO_REUSEADDR) failed");		
		exit(EXIT_FAILURE);	
	}

	/* perhaps we will use this keep-alive feature oneday */
	/* setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)); */		
	/* configure server address to listen to all local IPs */	
	memset(&addr, 0, sizeof(addr));	
	addr.sin_family = AF_INET;	
	addr.sin_port = htons(u32Port); 
	/* is already in right byteorder */	
	addr.sin_addr.s_addr = htonl(INADDR_ANY);	
	if ( bind(socket_sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {		
		perror("bind");		
		ALEXA_DBG("%s(): bind(%d) failed", __FUNCTION__, htons(u32Port));		
		closelog();		
		exit(EXIT_FAILURE);	
	}
	
	/* start listening on socket */	
	if ( listen(socket_sd, 10) != 0 ) {    
		fprintf(stderr, "listen failed\n");    	
		exit(EXIT_FAILURE);	
	}

	while(1){		
		int 	iConnFd, read_len = 0;		
		char iobuf[16];	
		
		ALEXA_DBG("accept alexa\n");

		iConnFd = accept(socket_sd, (struct sockaddr *)&client_addr, &addr_len);	   
		/* start new thread that will handle this TCP connected client */    	
		ALEXA_DBG("create thread to handle client that just established a connection:%s\n", inet_ntoa(client_addr.sin_addr));

		while(1){
			if((read_len = read(iConnFd, iobuf, 15)) <= 0) {
				printf("-iiiiobuf:%s, len:%d\n", iobuf, read_len);
				close(iConnFd);
				/* an error occured */			
				break;
			}
			
			printf("iobuf:%s, len:%d\n", iobuf, read_len);
			if(read_len == 4){				
				iobuf[4] = '\0';
				if(strncmp(iobuf, "AAAA", 4) == 0){
					ALEXA_DBG("tirgger alexa\n");
					g_u32CmdCode = ALEXA_CMD_START;
				}
			}			
			memset(&iobuf, 0, 16);
		}

		printf("-iobuf:%s, len:%d\n", iobuf, read_len);
	}

	return NULL;
}

//add asr gpio controls
static int RH_GpioExport(int pin_num)  
{  
    char buffer[64];  
    int len;  
    int fd;
	
    snprintf(buffer, sizeof(buffer), "/sys/class/gpio/gpio%d", pin_num);  
    if(access(buffer, F_OK) == 0)
		return 0;
	
    fd = open("/sys/class/gpio/export", O_WRONLY);  
    if (fd < 0) {  
        ALEXA_ERROR("Failed to open export for writing!\n");  
        return(-1);  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin_num);  
    if (write(fd, buffer, len) < 0) {  
        ALEXA_ERROR("Failed to export gpio!");  
        return -1;  
    }  
     
    close(fd);
	
    return 0;  
}  

static int gpio_unexport(int pin_num)  
{  
    char buffer[64];  
    int len;  
    int fd;  
  
    fd = open("/sys/class/gpio/unexport", O_WRONLY);  
    if (fd < 0) {  
        ALEXA_ERROR("Failed to open unexport for writing!\n");  
        return -1;  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin_num);  
    if (write(fd, buffer, len) < 0) {  
        ALEXA_ERROR("Failed to unexport gpio!");  
        return -1;  
    }  
     
    close(fd);
	
    return 0;  
} 

static int RH_GpioDirection(int pin_num, int dir)  
{  
    static const char dir_str[] = "in\0out";  
    char path[64];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin_num);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) {  
        ALEXA_ERROR("Failed to open gpio direction for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0) {  
        ALEXA_ERROR("Failed to set direction!\n");  
        return -1;  
    }  
  
    close(fd);
	
    return 0;  
}  

static int RH_GpioEdge(int pin_num, int edge)
{
	const char dir_str[] = "none\0rising\0falling\0both"; 
	char ptr;
	char path[64];  
	int fd;
	
	switch(edge){
		case 0:
			ptr = 0;
			break;
		case 1:
			ptr = 5;
			break;
		case 2:
			ptr = 12;
			break;
		case 3:
			ptr = 20;
			break;
		default:
			ptr = 0;
	} 
	  
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", pin_num);  
	fd = open(path, O_WRONLY);  
	if (fd < 0) {  
		ALEXA_ERROR("Failed to open gpio edge for writing!\n");  
		return -1;  
	}  
  
	if (write(fd, &dir_str[(unsigned char)ptr], strlen(&dir_str[(unsigned char)ptr])) < 0) {  
		ALEXA_ERROR("Failed to set edge!\n");  
		return -1;  
	}  
  
    close(fd);
	
    return 0;  
}

int RH_GpioRead(int fd)
{
	int ret;
	char buff[10]="";
	ret = lseek(fd,0,SEEK_SET);
	if( ret == -1 ){
		ALEXA_ERROR("lseek\n");
		return -1;
	}
	ret = read(fd,buff,10);
	if( ret == -1 ){
		ALEXA_ERROR("read\n");
		return -1;
	}
	return atoi(buff);
}

int RH_GpioOpen(int pin_num)
{
	char path[64];  
	int fd;  

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin_num);  
	fd = open(path, O_RDONLY);  
	if (fd < 0) {  
		ALEXA_ERROR("Failed to open gpio value for reading!\n");  
		return -1;  
	}
	
	return fd;
}
int RH_GpioClose(int fd)
{
	int ret;
	ret = close(fd);
	if (ret < 0) {  
		ALEXA_ERROR("Failed to open gpio value for reading!\n");  
	}
	
	return ret;
}
//gpio pin input/output
void RH_GpioInit(int pin_num, int trigger_mode)
{
	RH_GpioExport(pin_num);
	RH_GpioDirection(pin_num, 0);
	RH_GpioEdge(pin_num, trigger_mode);//0, none; 1,rising; 2,falling; 3,both
}



void *HI_AlexaAsrTriggerThread(void *arg)
{
	S_ALEXA_RES* pAlexaResData = (S_ALEXA_RES*)arg;
	S_PBContext * pbcontex= pAlexaResData->psPlaybackCxt;
	int ret = 0, readval = 0;

	TW_SETUP setup_params;
	struct pollfd fds;

	int cnt = 0;
	RK_APBSetHardwareVolume(pAlexaResData->psPlaybackCxt, 50, 1);

	RH_GpioInit(ASR_SND_DETECT_PIN, eGPIO_TRIGGER_MODE_BOTH);
	
	fds.fd = RH_GpioOpen(ASR_SND_DETECT_PIN);
	
	fds.events  = POLLPRI;

	setup_params.direction = 1;		/* 0-write, 1-read*/
	setup_params.reg = eSOUND_REG_EVENT;			/* cmdreg */
	setup_params.value = 0;		/* reg nums*/

	pbcontex->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
	while(1){
		RK_TW_Setup(&setup_params, "/dev/zl38067tw");
		if(!setup_params.value){
			break;
		}
	}

	while(1){
		ret = poll(&fds, 1, -1);
		readval = RH_GpioRead(fds.fd);
#if 1
		if(readval == 0){
			setup_params.value = 0;
			ret = RK_TW_Setup(&setup_params, "/dev/zl38067tw");
//			printf("--------->>>>>>>> line:%d, func:%s() pin : setup_params.value : %d......\n", __LINE__, __func__,setup_params.value);
			if(setup_params.value & eSOUND_EVENT_ID_ASR){
				E_ALEXA_STATE eAlexaResState = pAlexaResData->eAlexaResState;
				E_ALEXA_STATE simpleStatebit = HI_AlexaSimpleJudgeResState(eAlexaResState);//
				ALEXA_DBG("eSOUND_EVENT_ID_ASR - %04x , %04x\n", eAlexaResState, simpleStatebit);
				g_u32CmdCode = HI_AlexaTriggerJudgeCmd(simpleStatebit, (eAlexaResState & eALEXA_STSTE_CAPTURING), eTRIG_WAY_VOICE);
				switch(simpleStatebit){
					case eALEXA_STSTE_MIC:{
					}break;
					case eALEXA_STSTE_ETHERNET:{
						RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOETH);
					}break;
					case eALEXA_STSTE_NETWORK:{
						RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOITN);
					}break;
					case eALEXA_STSTE_USRACCOUNT:{
						RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOUSR);
					}break;
					case eALEXA_STSTE_TOKEN:{
						RK_sysStatusCues(eSYSTIPSCODE_ALEXA_TROBL);
					}break;
					default:
					break;
				}				
				
			}else if(setup_params.value & eSOUND_EVENT_ID_VAD){
				if(time(NULL) - alexa_capture_time > 2)
					g_sAlexaResData->bEnableCapture = false;
			}
		}
//		usleep(20*1000);
#else
		
		time_t triggertime=time(NULL);
		if(readval == 0){
		}else{
			time_t thesecondstime = time(NULL);
			if(thesecondstime - triggertime > 5){
				triggertime = thesecondstime;
				printf("--------->>>>>>>> func:void*%s(void*arg) line:%d alexa triggered\n", __func__, __LINE__);
				RK_APBOpen(pbcontex, ePB_TYPE_SYSTIP);
				g_u32CmdCode = ALEXA_CMD_START;
			}
		}
//		printf("---->>>> void* %s(void*arg) line:%d, readval:%d\n",__func__, __LINE__, readval);
	//	sleep(2);
		usleep(500*1000);
#endif
	}
	
	RH_GpioClose(ASR_SND_DETECT_PIN);
}



void *HI_AlexaRecognizeEventHandleThread
(
	void *arg 
)
{
	S_ALEXA_HND 	*psAlexaHnd = (S_ALEXA_HND *)arg;
	S_ALEXA_RES 	*pAlexaResData = psAlexaHnd->psResData;
	S_PBContext		*pPBCxt = pAlexaResData->psPlaybackCxt;
	S_AudioHnd		*psAudioHandle = &pPBCxt->asAhandler[ePB_TYPE_DIALOG];
	RK_CURL_HTTPPOST *postData = NULL;
	char *cJsonData = NULL;
	
	uint32_t iReqSize;
	int ret;
	
	int i32ParamValue;
	pAlexaResData->eAlexaResState |= eALEXA_STATE_START;
	do{
	 pAlexaResData->iReadWriteStamp = 0;
#if 1	
	if(pAlexaResData->eAlexaResState & eALEXA_EXPECTSPEECH){
		pAlexaResData->eAlexaResState &= ~eALEXA_EXPECTSPEECH;
		if(!HI_ChgDspDataLine(1)){
			pAlexaResData->bEnableCapture = true;
			pAlexaResData->bWaitCapture = true;
			sem_post(&s_sCmdCaptureSem);
		}else{
			ALEXA_INFO("\033[1;32mHI_ChgDspDataLine failed. \033[0m\n");
			pAlexaResData->iReadWriteStamp = 1;
			pAlexaResData->eAlexaResState &= ~eALEXA_STATE_START;
			return NULL;
		}
	}
#endif
	/*set flags start*/
	psAlexaHnd->isStream = 0;
	/*set flags start*/

	//seven mark
	RK_AlexaPriorityBackground();

	/*1- enable to use stop capture directive, 0 - disable to use stop capture directive*/
	RK_SetRecognizeEvent(&cJsonData, 1);
	//384000	/*12*/
//	iReqSize = 256000;	/*8s*/
	iReqSize = 320000;	/*10s*/
//	iReqSize = 128000;	/*4s*/
//	iReqSize = 96000;	/*2s*/
	printf("uploadFileSize = %ld\n", iReqSize);

	/* prepare recognize Event post data */
	postData = RK_SetRecognizePostData(cJsonData, pAlexaResData, iReqSize);
	
	RK_SetEasyOptionParams(pAlexaResData->asAvsCurlSet.asCurlSet[eCURL_HND_EVENT].curl, pAlexaResData->headerSlist, postData, (void *)psAlexaHnd, true);
	
	RK_StartRecognizeSendRequest(psAlexaHnd);	

	ret = RK_WaitResponseCode1(&pAlexaResData->asAvsCurlSet.asCurlSet[eCURL_HND_EVENT]);
	
	ALEXA_INFO("Dialog Code: %d-%d\n", psAlexaHnd->ui8Index, ret);

	/*set laster packet to specify interaction finish*/
	iReqSize = 1;

	if(psAudioHandle->ePlaybackState != ePB_STATE_IDLE){
#ifdef DEF_ENABLE_PB
		psAudioHandle->blast_packet = 1;
		pPBCxt->ePlaybackType = ePB_TYPE_DIALOG;
		if(pPBCxt->asAPlayback->pb_write){
			//ALEXA_TRACE("Insert the last packet of dialog!\n");
			RK_APBWrite((void *)pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, iReqSize);
		}
#endif
		psAudioHandle->blast_packet = 0;
		/* send SpeechFinished event to avs server*/
		RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SPEECHFINISHED);

	}
	
	int forcebreak = psAlexaHnd->ui8Done;
#ifdef DEF_ENABLE_PB
	/*we can ignore wait finished pb if user force to quit the request*/
	if(!forcebreak){
		pAlexaResData->psPlaybackCxt->ePlaybackType = ePB_TYPE_DIALOG;
		if(pPBCxt->asAPlayback->pb_wait_finished)
			RK_APBFinished((void *)pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG, 0);
	}
#endif

	if(!ret && !forcebreak){
		i32ParamValue = 3;
		pAlexaResData->eAlexaResState &= ~eALEXA_STSTE_NETWORK;
		/*We must send the event to avs so that we can query the network connectivity*/
		RK_AVSSetDirectivesEventID(eAVS_ALIVE_PING);
		RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOITN);
	}else{
		i32ParamValue = 0;
	}
	pAlexaResData->bEnableCapture = false;
	RK_APBStream_open_block(true);
	/*change alexa light status to specify alexa finished. blue blink - turnoff */
	{
		S_LED_t ledt=gs_led;
		ledt.ledmode = LED_SIMPLE;
		ledt.ledlumin = 0;
		led_set(ledt);
		gs_led = ledt;
	}
	}while(pAlexaResData->eAlexaResState & eALEXA_EXPECTSPEECH);//pAlexaResData->eAlexaResState & eALEXA_EXPECTSPEECH
	//seven mark
	RK_AlexaPriorityForeground();
#ifdef DEF_ENABLE_PB	
	/* resume softvolume to normal */
	RK_APBChange_handleState(pAlexaResData->psPlaybackCxt, 0);
#endif


#ifdef ALEXA_FILE_DEBUG	
	RK_CloseAlexaRecordData(pAlexaResData->psAlexaDebFs);
#endif

	if(cJsonData)
		free(cJsonData);
	cJsonData = NULL;
	if(postData)
		curl_formfree(postData);
		postData = NULL;

	psAlexaHnd->ui8Done = 0;
	pAlexaResData->eAlexaResState &= ~eALEXA_STATE_START;
//	printf("Dialog finished:%d, index:%d\n", ret, psAlexaHnd->ui8Index);

	return NULL;
}

int HI_AlexaRecognizeEventHandle
(
	S_ALEXA_CONTEXT *pAlexaCxt, 
	unsigned int 	theCmd
)
{
	S_ALEXA_HND	*pAlexaHnd = (S_ALEXA_HND *)&pAlexaCxt->sAlexaHnd;
	S_ALEXA_RES *pAlexaResData = pAlexaCxt->sAlexaHnd.psResData;
	pthread_t	threadRecognizeID;
	uint8_t		iIdx;
	int expect_speech = 0;
	
	if(theCmd != ALEXA_CMD_START && theCmd != ALEXA_CMD_STOP){
		expect_speech = pAlexaResData->eAlexaResState & eALEXA_EXPECTSPEECH_1;
		if(!expect_speech)
			return RK_FAILURE;
			
		pAlexaResData->eAlexaResState &= ~eALEXA_EXPECTSPEECH_1;
	}
#if 1
	if(expect_speech){
		//RK_APBWrite(pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, 1);
		pAlexaResData->isPromptSpeak = 0;
		/* send SpeechFinished event to avs server*/
		RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SPEECHFINISHED);
		RK_APBFinished(pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG, 0);
		pAlexaResData->eAlexaResState &= ~eALEXA_MEDIA_PROMPT;
	}
#else	
	g_u32CmdCode = ALEXA_CMD_NONE;
/*find idle handle*/
	if(!expect_speech){
		if(pAlexaResData->eAlexaResState & eALEXA_STATE_START){
			if((pAlexaResData->iReadWriteStamp)){
				pAlexaHnd->ui8Done = 1;
				RK_AlexaRequestDone(&pAlexaResData->asAvsCurlSet.asCurlSet[eCURL_HND_EVENT]);
				RK_APBClose(pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG);
			}
		}
	}
#endif
		g_u32CmdCode = ALEXA_CMD_NONE;

	/*find idle handle*/
	if(pAlexaResData->eAlexaResState & eALEXA_STATE_START){
		if((pAlexaResData->iReadWriteStamp)){
			pAlexaHnd->ui8Done = 1;
			RK_AlexaRequestDone(&pAlexaResData->asAvsCurlSet.asCurlSet[eCURL_HND_EVENT]);
			RK_APBClose(pAlexaResData->psPlaybackCxt, ePB_TYPE_DIALOG);
		}
	}

	while(pAlexaResData->eAlexaResState & eALEXA_STATE_START) usleep(1000*100);
	if(theCmd == ALEXA_CMD_STOP){
		pAlexaResData->eAlexaResState &= ~eALEXA_STATE_START;
		return 0;
	}
	
	//Initialization alexa params
	/*set request alive peirod time */
	pAlexaHnd->ui8Done = 0;
	pAlexaResData->bEnableCapture = true;
	pAlexaResData->bWaitCapture = true;
	pAlexaResData->eAlexaResState |= eALEXA_STATE_START;
	
	//Trigger Capture voice
	sem_post(&s_sCmdCaptureSem);

	pthread_create(&threadRecognizeID, 0, HI_AlexaRecognizeEventHandleThread, (void *)pAlexaHnd);
	pthread_detach(threadRecognizeID);

	return RK_SUCCESS;
}

int PluginInit(void *strCmdArg)
{
	uint8_t iIdx;
	int ret = 0;

	memset(&g_sAlexaContext.psAudioParam, 0, sizeof(S_AUDIO_PARAM));
#if 1
	g_sAlexaResData = malloc(sizeof(S_ALEXA_RES));
	if(g_sAlexaResData == NULL){
		ALEXA_ERROR("avsResData: %s init failed.\n", strerror(errno));
	}
	memset(g_sAlexaResData, 0, sizeof(S_ALEXA_RES));
	
	/* we need free - mark*/
	g_sAlexaResData->psPlaybackCxt = malloc(sizeof(S_PBContext));
	if(!g_sAlexaResData->psPlaybackCxt){
		ret = eALEXA_ERRCODE_FAILED_ALLOCATE;
		ALEXA_ERROR("Playback context malloc error:%m\n");
		goto CANCEL_MALLOC;
	}
	
	S_PBContext *pb = g_sAlexaResData->psPlaybackCxt;
	memset(pb, 0, sizeof(S_PBContext));
	pb->priv_data =	dialog_view;
	pb->asAPlayback = &rk_audio_playback;
	if(pb->asAPlayback){
		if(pb->asAPlayback->pb_init)
			pb->asAPlayback->pb_init(pb);
	}
#endif

	memset(&g_sAlexaContext.sAlexaHnd, 0, sizeof(S_ALEXA_HND));
	g_sAlexaContext.sAlexaHnd.psResData = g_sAlexaResData;

	g_sAlexaToken = (S_ALEXA_TOKEN){0};
	int firstboot = 0;
	/* get alexa config from /etc/hive/alexa.conf */
	while(RH_ReadAlexaConfig(&g_sAlexaToken)<0){
		RH_CreateAlexaConfig();
		if(firstboot++ != 0){
			
			printf("please factory reset or delete /etc/wiskey/alexa.conf, and reboot\n");
			if(firstboot>5){
				firstboot = 6;
				break;
			}
			sleep(10);
		}
		
	}
	
//add alert releative file @201601107 merge
	static S_ALERT_BASE_RES alertBaseRes={
		.iAlertTimeOut = ALERT_TIME_OUT,
		.iStopNextTime = STOP_NEXT_TIME,
		.pAlarmAudioFile = DEFAULT_ALERT_FILE_ALARM,
		.pTimerAudioFile = DEFAULT_ALERT_FILE_TIMER,
		.pAlertConfigFile = DEF_ALERT_CONF_FILE_NAME,
		.pAlertConfigFileTmp = DEF_ALERT_CONF_FILE_NAME_TMP
	};

	if(RK_AlertRes_Set(&alertBaseRes) != 0){
		fprintf(stdout, "alert resoure set fail!\n");
		return -1;
	}
	
//end	
	
	//g_sPluginIf.m_bInitialized = TRUE;
//	RK_SysFuncSet((void*)HI_SysRdState, (void*)HI_SysWrState, (void*)HI_SysAudTips);

CANCEL_MALLOC:
	
	/* TODO */
	return ret;
}

static void *HI_AlexaWaitforAuthThread(void*arg)
{
	S_ALEXA_RES		*pAlexaResData = (S_ALEXA_RES *)g_sAlexaResData;
	int msqid = *(int*)arg;
	char secondstime = 0;

	ALEXA_DBG("2Start the request access_token...\n");
	while(g_sAlexaToken.change_login){//wait for longin result.
		if(secondstime>40){
			secondstime = 0;
			break;
		}
		secondstime++;
		sleep(1);
	}
	char response = 0 - !pAlexaResData->ui8isTokenValid;
	printf("response==%d\n",response);
	RK_SndIOCtrl(msqid, &response, 1, eIOTYPE_USER_MSG_HTTPD, eIOTYPE_MSG_AVSLOGIN_RESPONSE);
}

void *HI_AlexaCommunicationThread(void*arg)
{
	S_ALEXA_RES		*pAlexaResData = (S_ALEXA_RES *)arg;
	int msqid;

	while(eIOCTRL_ERR == (msqid = RK_MsgInit())){
		printf("eIOCTRL_ERR:%d :%m\n", eIOCTRL_ERR);
		sleep(1);
	}
	SMsgIoctrlAvsGetAuth avs_get_auth={
		.code_method="S256",
		.product_dsn="",
		.product_id="",
	};
	S_ALEXA_AUTH alexa_auth={
		.encodeMode="S256",
		.dsn="",
		.productID="",

	};
	{//limit the scope of variables (fd, strline)
		FILE* fd = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
		char strline[77];
		while(fgets(strline, 32, fd)){
			if(!strncmp(strline, "avs_dsn #=", 10)){

				sscanf(strline+10, "%[^\r\n]", avs_get_auth.product_dsn);
				strcpy(alexa_auth.dsn, avs_get_auth.product_dsn);
				if(avs_get_auth.product_id[0])
					break;
				continue;
			}
			if(!strncmp(strline, "avs_device #=", 13)){
				sscanf(strline+13, "%[^\r\n]", avs_get_auth.product_id);
				strcpy(alexa_auth.productID, avs_get_auth.product_id);
				if(avs_get_auth.product_dsn[0])
					break;
			}
		}
		fclose(fd);
	}

	int msgHeaderSize = (size_t)(((SMsgIoctrlData*)0)->next) - sizeof(long);
	SMsgIoctrlData * msqp = calloc(msgHeaderSize,1);
	while(1){
		int nblock = 0;
		int rcvsz;
		while((rcvsz = RK_RecvIOCtrl(msqid, msqp, msgHeaderSize + 100*nblock, eIOTYPE_USER_MSG_AVS))<0){
			if(nblock>15){
				free(msqp);
				msqp=NULL;
				ALEXA_DBG("RK_RecvIOCtrl:%m, and exit!!!!\n");
				exit(-1);
			}
			nblock++;
			msqp = realloc(msqp, msgHeaderSize + nblock*100);
//			ALEXA_DBG("RK_RecvIOCtrl:%m, and try again!\n");
		}
		switch(msqp->u32iCommand){
			case eIOTYPE_MSG_NATSTATE://ÍøÂç×´Ì¬//
			{
				
				SMsgIoctrlNatState *avs_net_state = (SMsgIoctrlNatState *)msqp->next;
				if(!avs_net_state->restult){
					pAlexaResData->eAlexaResState |= eALEXA_STSTE_NETWORK;
					pAlexaResData->eAlexaResState |= eALEXA_STSTE_ETHERNET;
					printf("**************************************************************************\n");
					printf("**************************************************************************\n\n\n");
					printf("**** %s %s line:%d ****\n\n\n", __FILE__, __func__, __LINE__);
					printf("**************************************************************************\n");
					printf("**************************************************************************\n");
				}else{
					pAlexaResData->eAlexaResState &= ~eALEXA_STSTE_NETWORK;
				}
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_PHYSTATE://ÎïÀíÍø¿¨×´Ì¬
			{
				SMsgIoctrlNatState *avs_net_state = (SMsgIoctrlNatState *)msqp->next;
				if(!avs_net_state->restult){
					pAlexaResData->eAlexaResState |= eALEXA_STSTE_ETHERNET;
				}else{
					pAlexaResData->eAlexaResState &= ~eALEXA_STSTE_ETHERNET;
				}
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_TUNNEL_START:
			case eIOTYPE_MSG_TUNNEL_STOP:
			break;
			case eIOTYPE_MSG_AVSLOGIN_GETAUTH:{
				memset(alexa_auth.codeVerifier,0,CODE_VERIFIER_MAX_LEN);
				memset(alexa_auth.codeChallenge,0,CODE_CHALLENGE_LEN);
				int iCodeVerifierLen = generateCodeVerifier(alexa_auth.codeVerifier);
				if(iCodeVerifierLen<=0){
					ALEXA_DBG("generateCodeVerifier failed, and exit!!!!\n");
					exit(-1);
				}
				ALEXA_INFO("iCodeVerifierLen==%d\n", iCodeVerifierLen);
				if(generateCodeChallenge(alexa_auth.codeVerifier, iCodeVerifierLen , alexa_auth.codeChallenge)<=0){
					ALEXA_DBG("generateCodeChallenge failed, and exit!!!!\n");
					exit(-1);
				}
				strcpy(avs_get_auth.code_challenge, alexa_auth.codeChallenge);
				int msgBodySize = sizeof(SMsgIoctrlAvsGetAuth);
				ALEXA_INFO("Respond Data:\ndsn:%s,\nproduct:%s,\ncodeMode:%s,\ncode_challenge:%s;\n\nlocaldata:\ncode_verifier:%s\n", \
										avs_get_auth.product_dsn, avs_get_auth.product_id, avs_get_auth.code_method, avs_get_auth.code_challenge, alexa_auth.codeVerifier);
				RK_SndIOCtrl(msqid, (void*)&avs_get_auth, (size_t)msgBodySize, eIOTYPE_USER_MSG_HTTPD, eIOTYPE_MSG_AVSLOGIN_GETAUTH);
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_AVSLOGIN_SETTOKEN:{
				SMsgIoctrlAvsSetAuth *avs_set_auth = (SMsgIoctrlAvsSetAuth *)msqp->next;
				strcpy(avs_set_auth->code_verify, alexa_auth.codeVerifier);
				cJSON * root = cJSON_CreateObject();
				cJSON_AddStringToObject(root,"client_id",avs_set_auth->client_id);
				cJSON_AddStringToObject(root,"redirect_uri",avs_set_auth->redirect_uri);
				cJSON_AddStringToObject(root,"grant_type","authorization_code");
				cJSON_AddStringToObject(root,"code",avs_set_auth->authorize_code);
				cJSON_AddStringToObject(root,"code_verifier",avs_set_auth->code_verify);
				g_sAlexaToken.psRefreshJsonData = cJSON_PrintUnformatted(root);cJSON_Delete(root);//cJSON_PrintUnformatted(root)//cJSON_Print(root)
				ALEXA_DBG("---->>>>json:\n%s\n", g_sAlexaToken.psRefreshJsonData);
				g_sAlexaToken.change_login = 1;
				pAlexaResData->ui8isTokenValid = 0;
				pthread_t threadAlexaWaitforAuthID;
				pthread_create(&threadAlexaWaitforAuthID, 0, HI_AlexaWaitforAuthThread, &msqid);
				pthread_detach(threadAlexaWaitforAuthID);
				memset(msqp, 0, msgHeaderSize+nblock*100);

			}
			break;
			case eIOTYPE_MSG_AVSLOGOUT:{
				printf("---->>>>Bye Bye Alexa!<<<<----\n");
				printf("---->>>>Bye Bye Alexa!<<<<----\n");
				RH_CleanAlexaConfig(&g_sAlexaToken);
				RH_InitAlexaConfig(&g_sAlexaToken);
				RK_Alert_Reset(pAlexaResData);
				pAlexaResData->pAccessToken = NULL;
				pAlexaResData->ui8isTokenValid = 0;
				pAlexaResData->eAlexaResState &= ~eALEXA_STSTE_USRACCOUNT;
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_SETTUNNELCTRL_REQ:
			case eIOTYPE_MSG_SETTUNNELCTRL_RESP:
				memset(msqp, 0, msgHeaderSize+nblock*100);
				break;
			case eIOTYPE_MSG_AVS_BUTTON_MUTE:{
				printf("trigger mute MIC button\n");
				pAlexaResData->eAlexaResState = (pAlexaResData->eAlexaResState & eALEXA_STSTE_MIC)? (pAlexaResData->eAlexaResState & ~eALEXA_STSTE_MIC) : (pAlexaResData->eAlexaResState | eALEXA_STSTE_MIC);
				memset(msqp, 0, msgHeaderSize+nblock*100);
				E_SYSTIPSCODE mic_state = (pAlexaResData->eAlexaResState & eALEXA_STSTE_MIC)? eSYSTIPSCODE_ALEXA_MUTE_MIC : eSYSTIPSCODE_ALEXA_UNMUTE_MIC;
				RK_sysStatusCues(mic_state);
			}
			break;
			default:
				ALEXA_DBG("---->>>>don't support CMD:%#x\n", msqp->u32iCommand);
				break;
		}
		
	}
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int main(int argc, char **argv)
{
	int 			ret = eALEXA_ERRCODE_SUCCESS;
	S_ALEXA_RES		*pAlexaRES;
	S_ALEXA_TOKEN 	*pAlexaToken = &g_sAlexaToken;
	S_AVS_CURL_SET 	*pAlexaCurlSet;
	pthread_t 		threadCaptureID;	/* capture voice task*/
	pthread_t 		threadTcpTriggerID;
	pthread_t 		threadAsrTriggerID;
	pthread_t 		threadCommunicationID;
	int i;
	static struct timespec	s_tsReq	= {0, 1000};
	uint64_t		ui64CurSec = 0,
					ui64IntervalSec = 0;

	if(argc == 2)
		speechfile = atoi(argv[1]);
	
	if(PluginInit(NULL) != 0)
		return eALEXA_ERRCODE_NO_VALID_RES;
	pAlexaRES = g_sAlexaResData;

	pAlexaRES->m_pfnAvsReadFunCB = SpeechDeliverToAVS;

	pAlexaCurlSet = &pAlexaRES->asAvsCurlSet;

	/* 1, Initialization a multi stack */
	pAlexaCurlSet->multiCurlHnd = RK_CurlMulitInit();
	/* 2, Initialization a easy stack for each resource request */
	RK_CurlEasyInit(pAlexaCurlSet);
	gs_led = led_init(LED_PIN);
	led_workstart(NULL);

	/* 3, need to creat a capture speech task to provide avs*/
	pthread_create(&threadCaptureID, 0, HI_AlexaCaptureThread, (void *)pAlexaRES);
    pthread_detach(threadCaptureID);

	pthread_create(&threadTcpTriggerID, 0, HI_AlexaTcpTriggerThread, NULL);
    pthread_detach(threadTcpTriggerID);

	pthread_create(&threadAsrTriggerID, 0, HI_AlexaAsrTriggerThread, pAlexaRES);
    pthread_detach(threadAsrTriggerID);

	pthread_create(&threadCommunicationID, 0, HI_AlexaCommunicationThread, pAlexaRES);
    pthread_detach(threadCommunicationID);

	/* 4, start alexa master task */
	RK_AlexaMasterWorkHandler((void *)pAlexaRES);
	

	RK_AlexaVersion();
	int isfirstboot = 1;

	while(!(pAlexaRES->eAlexaResState & eALEXA_STSTE_NETWORK)){
		sleep(1);
	}
	if(pAlexaToken->psClientID && pAlexaToken->psClientID[0]){
		pAlexaRES->eAlexaResState |= eALEXA_STSTE_USRACCOUNT;
	}

	ALEXA_DBG("1Start the request access_token...\n");
	
	while(!g_uiQuit){
		if(pAlexaRES->ui8isTokenValid){
			/* 5, initiated a event request */
			HI_AlexaRecognizeEventHandle(&g_sAlexaContext, g_u32CmdCode);
		}else{
			pAlexaRES->eAlexaResState &= ~eALEXA_STSTE_TOKEN;
			
			S_ALEXA_TOKEN AlexaToken = *pAlexaToken;//to avoid twice login
			if(RK_AlexaRequestCheckState(pAlexaCurlSet) == 0){
				/* 6, have to request alexa token before submming speech data */
				if(RH_AlexaReqToken(&pAlexaCurlSet->asCurlSet[eCURL_HND_REQTOKEN], &AlexaToken, RH_WriteAlexaConfig) == 0){//0:successed; others:failed
					if(pAlexaToken->change_login){//new login
						if(AlexaToken.change_login){//new login and find the new login
							*pAlexaToken = AlexaToken;
							
							pAlexaRES->pAccessToken = AlexaToken.psAccessToken;
							pAlexaRES->ui8isTokenValid = 1;
							pAlexaRES->eAlexaResState |= eALEXA_STSTE_TOKEN;
							pAlexaRES->eAlexaResState |= eALEXA_STSTE_USRACCOUNT;
							RK_sysStatusCues(eSYSTIPSCODE_ALEXA_LOGINING);
							pAlexaToken->change_login =0;
						}else{//new login but not find new login
							AlexaToken.psRefreshJsonData = pAlexaToken->psRefreshJsonData;
							AlexaToken.change_login = pAlexaToken->change_login;
							*pAlexaToken = AlexaToken;
							continue;
						}
					}else{//no new longin
						*pAlexaToken = AlexaToken;
						
						pAlexaRES->pAccessToken = AlexaToken.psAccessToken;
						pAlexaRES->ui8isTokenValid = 1;
						pAlexaRES->eAlexaResState |= eALEXA_STSTE_TOKEN;
						if(isfirstboot){
							RK_sysStatusCues(eSYSTIPSCODE_ALEXA_LOGINING);
							isfirstboot =0;
						}
					}	
					/* 7, have to prepare headerlist before submming speech data*/
					RK_GetEventHeaderList(&pAlexaRES->headerSlist, AlexaToken.psAccessToken);
					ALEXA_DBG("Access Token Successed!\n");
				}else{
					ALEXA_TRACE("Access Token request is failed\n");
					if(pAlexaToken->change_login){//new login
						if(AlexaToken.change_login){//new login and find it but failed.
							ALEXA_TRACE("Login is failed\n");
							AlexaToken.change_login =0;
						}else{//new login but not found it
							ALEXA_TRACE("Next loop will DO login\n");
							AlexaToken.psRefreshJsonData = pAlexaToken->psRefreshJsonData;
							AlexaToken.change_login = pAlexaToken->change_login;
						}
					}
						
					*pAlexaToken = AlexaToken;
					
				}
				
			}
		}
		
		ui64CurSec = utils_get_sec_time();
		ui64IntervalSec = ui64CurSec - pAlexaToken->ui64ExpireSec;
		if(ui64IntervalSec >= 60*59){	/* Max 50 minute again to request access token */
			pAlexaRES->ui8isTokenValid = 0;		/*the better to lock the atomic variables*/
		}

		nanosleep(&s_tsReq, NULL);
	}
	
	/* 8, end and clean up all alexa resource easy session*/
	RK_CurlEasyCleanup(pAlexaCurlSet);
	/* 9, clean multi curl handle*/
	RK_CurlMulitCleanup(pAlexaCurlSet->multiCurlHnd);
	
CANCEL_MALLOC:
	
	ALEXA_INFO("Alexa Demo Exit...\n");
	return ret;
}


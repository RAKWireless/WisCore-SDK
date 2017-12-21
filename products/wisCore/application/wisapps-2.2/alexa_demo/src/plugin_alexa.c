#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
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
/* cJSON */
#include <cJSON/cJSON.h>

#include "alexa_common.h"
#include "capture/audio_capture.h"
#include "aplay/aplay.h"
#include "audio_playerItf.h"
#include "audio_decodeItf.h"
#include "capture/audio_queue.h"
#include "plugin_alexa.h"
#include "audio_player.h"
#include "vprocTwolf_access.h"

#include "RKIoctrlDef.h"
#include "RKGpioApi.h"
#include "RKLed.h"
#include "RKLog.h"

rak_log_t demobugfs;

#define DEF_ENABLE_PB	

#ifndef CODE_VERIFIER_MAX_LEN
#define CODE_VERIFIER_MAX_LEN 129
#define CODE_CHALLENGE_LEN    45
#endif

#define ALEXA_CMD_NONE 				0
#define ALEXA_CMD_START				0x01
#define ALEXA_CMD_STOP				0x02
#define ALEXA_CMD_RESTART			0x03

#define SYS_AUDIO_TIPS				0x01
#define SYS_VISUAL_TIPS				0x02

typedef enum{
	eTRIG_WAY_IDLE,
	eTRIG_WAY_BUTTON,
	eTRIG_WAY_VOICE,
}E_TRIG_WAY_t;
#if 0
typedef struct alexa_handle {
	uint8_t				ui8Done;				/* set 1 if the request period is forced to close*/
	uint8_t				ui8Index;				/* user identify request*/
	uint8_t				isStream;				/* 0 - no stream, 1 - is mp3 stream  remove 20170620*/
	bool				bisLastPacket;			/* */
	S_ALEXA_DEB_FS		*psAlexaDebFs;		/* delect */
	S_ALEXA_RES 		*psResData;
} S_ALEXA_HND;
#endif

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
extern S_DecodecItf decodecStreamItf;

typedef struct alexa_context {
	S_AUDIO_PARAM 		psAudioParam;	/* try to use point*/
	uint8_t				ui8Done;				/* set 1 if the request period is forced to close*/
	uint8_t				ui8Index;				/* user identify request*/
	uint8_t				isStream;				/* 0 - no stream, 1 - is mp3 stream  remove 20170620*/
	bool				bisLastPacket;			/* */
	S_ALEXA_RES 		*psResData;
}S_ALEXA_CONTEXT;

typedef struct alexa_auth{
	unsigned char codeVerifier[CODE_VERIFIER_MAX_LEN];
	unsigned char codeChallenge[CODE_CHALLENGE_LEN];
	
	char encodeMode[10];
	char productID[64];
	char dsn[64];
}S_ALEXA_AUTH;

typedef struct alexa_config{
	const char    *pLocaleSet;
	int            micmute;
}S_ALEXA_CONFIG;



typedef enum{
	eDIALOG_STATUS_IDLE,
	eDIALOG_STATUS_LISTENING,
	eDIALOG_STATUS_THINKING,
	eDIALOG_STATUS_SPEAKING,
}dialog_status_t;

typedef struct {
    unsigned char 	Mask;
    unsigned char   *Description;
} DEBUG_MASK_DESCRIPTION;


// -------------------------------------------------------------------
// Globe data
// -------------------------------------------------------------------
S_ALEXA_CONTEXT	g_sAlexaContext;
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
typedef struct S_PlayerCtx{
	void *hnd;
	int tips;
}S_PlayerCtx_t;
S_PlayerCtx_t g_pbhnd[ePB_TYPE_CNT+1];

//static void             *g_pbhnd[ePB_TYPE_CNT+1];		//mark
#define ePB_TYPE_SYSTIP ePB_TYPE_CNT

dialog_status_t dialog_status;
volatile uint8_t g_uisys_tips[eSYSTIPSCODE_NUM]   = {0};
volatile time_t start_tips_time[eSYSTIPSCODE_NUM] = {0};
volatile time_t alexa_capture_time = 0;
volatile int break_to_start = 0;
S_RK_LED_t gs_led;

S_ALEXA_CONFIG g_sAlexaConfig={"en-US",0};

void RK_sysStatusCues(E_SYSTIPSCODE status)
{
	S_ALEXA_RES		*pAlexaRes = g_sAlexaContext.psResData;
	S_AudioConf conf={0};
	switch(status){
		case eSYSTIPSCODE_ALEXA_CAPTURE_START:{
			dialog_status = eDIALOG_STATUS_LISTENING;
			conf.strFileName = "/usr/lib/sound/listen_start";
			RK_LED_TurnOn(gs_led);
		}break;
		case eSYSTIPSCODE_ALEXA_CAPTURE_STOP:{
			if(dialog_status = eDIALOG_STATUS_LISTENING){
				dialog_status = eDIALOG_STATUS_THINKING;
				RK_LED_Flash(gs_led, 8);
			}
			conf.strFileName = "/usr/lib/sound/listen_stop";
		}break;
		case eSYSTIPSCODE_ALEXA_NOETH:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/cont_connect_to_your_network";
		}break;
		case eSYSTIPSCODE_ALEXA_NOITN:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/lost_the_intnet_try_again";
		}break;
		case eSYSTIPSCODE_ALEXA_NOUSR:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/no_alexa_user";
		}break;
		case eSYSTIPSCODE_ALEXA_LOGINING:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/welcome_to_wisalexa";
			pAlexaRes->eAlexaResState |= (eALEXA_STATE_TOKEN | eALEXA_STATE_USRACCOUNT | eALEXA_STATE_NETWORK);
		}break;
		case eSYSTIPSCODE_ALEXA_TROBL:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/i_have_a_trouble_to_understand_your_quest";
		}break;
		case eSYSTIPSCODE_ALERT_FAIL:{
			RK_LED_TurnOff(gs_led);
			conf.strFileName = "/usr/lib/sound/the_alert_was_set_unsuccessfully";
		}break;
		case eSYSTIPSCODE_ALEXA_MUTE_MIC:{
			conf.strFileName = "/usr/lib/sound/mic_off";
		}break;
		case eSYSTIPSCODE_ALEXA_UNMUTE_MIC:{
			conf.strFileName = "/usr/lib/sound/mic_on";
		}break;
		default:
			conf.strFileName = NULL;
			break;
	}
	if(conf.strFileName){
		conf.replaytimes= 1;
		LOG_P(demobugfs, RAK_LOG_FINE, "system tips\n");

		rk_audio_playback.pb_open(&conf, g_pbhnd[ePB_TYPE_SYSTIP].hnd);
	}
}

static E_ALEXA_STATE HI_AlexaSimpleJudgeResState(E_ALEXA_STATE eAlexaResState)//
{
	E_ALEXA_STATE state = eALEXA_STATE_IDLE;

//	if((eAlexaResState & eALEXA_STATE_MIC)){
//		state = eALEXA_STATE_MIC;
//	}else 
	if(!(eAlexaResState & eALEXA_STATE_NETWORK)){
		state = eALEXA_STATE_NETWORK;
	}else if(!(eAlexaResState & eALEXA_STATE_USRACCOUNT)){
		state = eALEXA_STATE_USRACCOUNT;
	}else if(!(eAlexaResState & eALEXA_STATE_TOKEN)){
		state = eALEXA_STATE_TOKEN;
	}
	return state;
}

static uint32_t HI_AlexaTriggerJudgeCmd(E_ALEXA_STATE simpleState, int capturing, E_TRIG_WAY_t triggerType)//
{
	if(simpleState)return ALEXA_CMD_NONE;
	int cmd;
	if(capturing){
		cmd = ((triggerType==eTRIG_WAY_BUTTON)? ALEXA_CMD_NONE:((triggerType==eTRIG_WAY_VOICE)?ALEXA_CMD_NONE : ALEXA_CMD_START));
		break_to_start = 1;
	}else{
		cmd = (time(NULL) - start_tips_time[eSYSTIPSCODE_ALEXA_CAPTURE_STOP] >1)? ALEXA_CMD_START : ALEXA_CMD_NONE;
	}
	return cmd;
}

static bool stopcapture = false;
static size_t SpeechDeliverToAVS(void *priv_usrdata, char *avbuffer, bool bstopcapture)
{
	S_AVPacket 	CapturePkt;
	size_t lengths = 0;
	int ret;
	
	do{
	/// no data will be blocked	
	ret = RK_Queue_Packet_Get(&QCapture, &CapturePkt, 1);
	if(ret <= 0){
		break;
	}

	if(CapturePkt.lastpkt == 1){
		RH_Free_Packet(&CapturePkt);
		break;
	}
	if(bstopcapture == true){//after stopping capture, the last 12 packets data will be dropped, usr can drop the code accordding to themselves
		RH_Free_Packet(&CapturePkt);
		lengths = 0;
		LOG_P(demobugfs, RAK_LOG_FINE, " Speech deliver over - %d\n", QCapture.nb_packets);
		break;
	}

	memcpy(avbuffer, CapturePkt.data, CapturePkt.size);
	lengths = (size_t)CapturePkt.size;
	
	RH_Free_Packet(&CapturePkt);
	}while(0);

	if(lengths == 0)
		stopcapture = true;
	
	return lengths;
}


static int RH_ReadAlexaConfig(S_ALEXA_TOKEN *psAlexaToken)
{
#if 01
	FILE 	*fconf = NULL;
	char 	fbuf[1024] = {0};
	char 	*pTmpPtr = NULL;
	int 	ret = 0;
	
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	if(fconf == NULL){
		LOG_P(demobugfs, RAK_LOG_WARN, "fopen "DEF_USR_BIND_DEVICE_CONF_FILE_NAME"ERROR - %m\n");
		ret = eALEXA_ERRCODE_FAILED_OPEN_DEV;
		return ret;
	}

	while((fgets(fbuf, sizeof(fbuf)-1, fconf)) != NULL){
		//printf("fbuf:%s, len:%d\n", fbuf, strlen(fbuf));
		if((pTmpPtr = strstr(fbuf, "avs_clientid")) != NULL){
			pTmpPtr += strlen("avs_clientid");
			psAlexaToken->pClientID = (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->pClientID != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->pClientID);	
				LOG_P(demobugfs, RAK_LOG_FINE, "avs_clientid:*%s*\n", psAlexaToken->pClientID);
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_refresh")) != NULL){
			pTmpPtr += strlen("avs_refresh");
			psAlexaToken->pRefreshToken = (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->pRefreshToken != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->pRefreshToken);
				LOG_P(demobugfs, RAK_LOG_FINE, "avs_refresh:*%s*\n", psAlexaToken->pRefreshToken);
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_clientsecret")) != NULL){
			pTmpPtr += strlen("avs_clientsecret");
			psAlexaToken->pClientSecret= (char *)calloc(strlen(pTmpPtr), sizeof(char));
			if(psAlexaToken->pClientSecret != NULL){
				sscanf(pTmpPtr, " #=%s", psAlexaToken->pClientSecret);
				LOG_P(demobugfs, RAK_LOG_FINE, "avs_clientsecret:*%s*\n", psAlexaToken->pClientSecret);
				//psAlexaToken->i8Method = 1;
			}
		}else if((pTmpPtr = strstr(fbuf, "avs_mute")) != NULL){//avs_mute
			pTmpPtr += strlen("avs_mute");
			sscanf(pTmpPtr, " #=%u", &g_sAlexaConfig.micmute);
			LOG_P(demobugfs, RAK_LOG_FINE, "avs_mute:*%u*\n", g_sAlexaConfig.micmute);
		}else if((pTmpPtr = strstr(fbuf, "avs_language")) != NULL){//avs_language
			pTmpPtr += strlen("avs_language");
			if(strstr(pTmpPtr, "en-US")){
				g_sAlexaConfig.pLocaleSet = "en-US";
			}else if(strstr(pTmpPtr, "en-GB")){
				g_sAlexaConfig.pLocaleSet = "en-GB";
			}else if(strstr(pTmpPtr, "de-DE")){
				g_sAlexaConfig.pLocaleSet = "de-DE";
			}
			LOG_P(demobugfs, RAK_LOG_FINE, "avs_language:*%s*\n", g_sAlexaConfig.pLocaleSet);
		}
	}
	
	fclose(fconf);
	if(psAlexaToken->pRefreshToken && psAlexaToken->pClientID && \
		psAlexaToken->pRefreshToken[0] && psAlexaToken->pRefreshToken[0]){
		LOG_P(demobugfs, RAK_LOG_FINE, "tokens:**successed\n");
		return ret;
	}
#endif
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


static int RH_WriteAlexaToken(S_ALEXA_TOKEN *pAlexaToken)
{
	FILE	*fconf = NULL, *fconfold = NULL;
	char	fbuf[1024] = {0};
	char	*pTmpPtr = NULL;
	int 	ret = 0;
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
	
	fconfold = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");

	if(fconf == NULL){
		if(creatpath(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", 0755, 0666)>=0){
			fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
			if(fconf == NULL){
				ret = eALEXA_ERRCODE_FAILED_OPEN_DEV;
				return ret;
			}
		}
	}
	if(fconfold){
		while(fgets(fbuf, 1024, fconfold)){
			if(!strncmp(fbuf, "avs_clientid #=", strlen("avs_clientid #="))){;
			}else if(!strncmp(fbuf, "avs_refresh #=", strlen("avs_refresh #="))){;
			}else if(!strncmp(fbuf, "avs_clientsecret #=", strlen("avs_clientsecret #="))){;
			}else{
				int len = strlen(fbuf);
				if(fbuf[len-2] == '\r' || fbuf[len-2] == '\n')fbuf[len-2] = 0;
				if(fbuf[len-1] == '\r' || fbuf[len-1] == '\n')fbuf[len-1] = 0;
				if(fbuf[0]=='\r' || fbuf[0]=='\n' || fbuf[0]=='\0')
					continue;
				fprintf(fconf, "%s\n",fbuf);
			}
		}
		fclose(fconfold);
		fconfold = NULL;
	}
	if(pAlexaToken){
		if(pAlexaToken->pClientID && pAlexaToken->pClientID[0])
			fprintf(fconf, "avs_clientid #=%s\n", 	pAlexaToken->pClientID);
		if(pAlexaToken->pRefreshToken && pAlexaToken->pRefreshToken[0])
			fprintf(fconf, "avs_refresh #=%s\n", 	pAlexaToken->pRefreshToken);
		if(pAlexaToken->pClientSecret && pAlexaToken->pClientSecret[0]){
			fprintf(fconf, "avs_clientsecret #=%s\n", 	pAlexaToken->pClientSecret);
		}
	}
	fclose(fconf);
	remove(DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	rename(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
		
	sync();
	return ret;
}

static int RH_WriteAlexaConfig(S_ALEXA_CONFIG *pAlexaConfig){
	FILE	*fconf = NULL, *fconfold = NULL;
	char	fbuf[1024] = {0};
	char	*pTmpPtr = NULL;
	int 	ret;
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
	fconfold = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	if(fconfold){
		if(fconf){
			while(fgets(fbuf, 1024, fconfold)){
				if(strncmp(fbuf, "avs_mute #=", strlen("avs_mute #=")) && strncmp(fbuf, "avs_language #=", strlen("avs_language #="))){
					int len = strlen(fbuf);
					if(fbuf[len-2] == '\r' || fbuf[len-2] == '\n')fbuf[len-2] = 0;
					if(fbuf[len-1] == '\r' || fbuf[len-1] == '\n')fbuf[len-1] = 0;
					if(fbuf[0]=='\r' || fbuf[0]=='\n' || fbuf[0]=='\0')
						continue;
					
					fprintf(fconf, "%s\n",fbuf);
				}
			}
		}
		fclose(fconfold);
		fconfold = NULL;
	}
	if(fconf){
		fprintf(fconf, "avs_mute #=%u\n", pAlexaConfig->micmute);
		fprintf(fconf, "avs_language #=%s\n", pAlexaConfig->pLocaleSet);
		fclose(fconf);
		ret = 0;
	}else{
		ret =-1;
	}
	fconf = NULL;
	remove(DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	rename(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	return ret;
}


int HI_CaptureStream(snd_pcm_t *handle, S_AUDIO_PARAM *ps_AudioParam, size_t captureBytes, S_ALEXA_CONTEXT *psAlexaCxt)
{
	S_ALEXA_RES 		*pAlexaResData = psAlexaCxt->psResData;
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

	count = (off64_t)captureBytes;
	rest = count;
	audiobuf = (u_char *)malloc(chunk_bytes);
	monoBuf = (u_char *)malloc(chunk_bytes/2);
	
	/* repeat the loop when format is raw without timelimit or
	* requested counts of data are recorded
	*/
	/* capture */
	timeo = 0;
	while ((rest > 0) && (!stopcapture)) {
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
			LOG_P(demobugfs, RAK_LOG_WARN, "Capture mem alloc failed\n");
			break;
		}
		memcpy(AudioPkt.data, monoBuf, monoSize);
		AudioPkt.size = monoSize;
		AudioPkt.lastpkt = 0;
		RK_Queue_Packet_Put(&QCapture, &AudioPkt, 1);	/* write queue block */
		
		count -= c;
		rest -= c;
	}
		AudioPkt.data = (unsigned char *)calloc(1, sizeof(unsigned char));
		AudioPkt.size = 1;
		AudioPkt.lastpkt = 1;
		RK_Queue_Packet_Put(&QCapture, &AudioPkt, 1);	/* write queue block */

	free(audiobuf);
	free(monoBuf);

	return 0;
}

int HI_CaptureStreamFromFile(void)
{
	FILE		*fp;
	S_AVPacket 	AudioPkt;
	int 		size, monoSize=666;
	

	fp = fopen("/usr/bin/16kk.raw", "rb");
	if(fp == NULL){
		LOG_P(demobugfs, RAK_LOG_WARN, "open capture file is failed!\n");
		return -1;
	}
	do{
	AudioPkt.data = (unsigned char *)calloc(monoSize, sizeof(unsigned char));
	if(AudioPkt.data == NULL){
		LOG_P(demobugfs, RAK_LOG_WARN, "Capture mem alloc failed\n");
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

int HI_AlexaCaptureAudioV4(S_AUDIO_PARAM *psAudioParam, S_ALEXA_CONTEXT *psAlexaCxt)
{
	S_ALEXA_RES 		*pAlexaResData = psAlexaCxt->psResData;
	S_AU_STREAM_PRIV	*psAuStream = &psAudioParam->hwparams.sAuStreamPriv;
	snd_pcm_t 			*handle;
	size_t 				captureBytes;
	int 				ret = 0;
	
	if(speechfile == 1){
		HI_CaptureStreamFromFile();
    }else{
		ret = RH_AudioOpenDev(&handle, psAudioParam);
		if(ret != 0){
			LOG_P(demobugfs, RAK_LOG_ERROR, "Failed RH_AudioOpenDev!");
			return eALEXA_ERRCODE_FAILED_OPEN_DEV;
		}
		/* set capture audio time and calculate audio size */
		ret = RH_AudioCaptureBytes(&captureBytes, 12);
		RH_AudioSetParams(handle, psAudioParam, psAuStream);
		HI_CaptureStream(handle, psAudioParam, captureBytes, psAlexaCxt);
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

	LOG_P(demobugfs, RAK_LOG_TRACE, " Wait Capture...\n");
	sem_wait(&s_sCmdCaptureSem);
	LOG_P(demobugfs, RAK_LOG_TRACE, " Start Capture ...\n");
	RK_sysStatusCues(eSYSTIPSCODE_ALEXA_CAPTURE_START);

	alexa_capture_time = time(NULL);
	RK_Queue_Packet_Flush(&QCapture);

	stopcapture = false;
	
	if(speechfile == 0)
		RK_InitCaptureParam(&g_sAlexaContext.psAudioParam);
	
	/*start to capture user request sound */
	ret = HI_AlexaCaptureAudioV4(&g_sAlexaContext.psAudioParam, &g_sAlexaContext);
	if(!break_to_start){
		start_tips_time[eSYSTIPSCODE_ALEXA_CAPTURE_STOP] = time(NULL);
		RK_sysStatusCues(eSYSTIPSCODE_ALEXA_CAPTURE_STOP);
	}else{
		break_to_start = 0;
	}
	}while(1);
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
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open export for writing!\n");  
        return(-1);  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin_num);  
    if (write(fd, buffer, len) < 0) {  
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to export gpio!");  
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
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open unexport for writing!\n");  
        return -1;  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin_num);  
    if (write(fd, buffer, len) < 0) {  
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to unexport gpio!");  
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
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open gpio direction for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0) {  
        LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to set direction!\n");  
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
		LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open gpio edge for writing!\n");  
		return -1;  
	}  
  
	if (write(fd, &dir_str[(unsigned char)ptr], strlen(&dir_str[(unsigned char)ptr])) < 0) {  
		LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to set edge!\n");  
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
		LOG_P(demobugfs, RAK_LOG_ERROR, "lseek\n");
		return -1;
	}
	ret = read(fd,buff,10);
	if( ret == -1 ){
		LOG_P(demobugfs, RAK_LOG_ERROR, "read\n");
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
		LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open gpio value for reading!\n");  
		return -1;  
	}
	
	return fd;
}
int RH_GpioClose(int fd)
{
	int ret;
	ret = close(fd);
	if (ret < 0) {  
		LOG_P(demobugfs, RAK_LOG_ERROR, "Failed to open gpio value for reading!\n");  
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
	int ret = 0, readval = 0;

	TW_SETUP setup_params;
	struct pollfd fds;

	int cnt = 0;

//	RK_APBSetHardwareVolume(pAlexaResData->psPlaybackCxt, 50, 1);
//	rk_audio_playback.pb_hardwarevolume(50, 1);

	RH_GpioInit(ASR_SND_DETECT_PIN, eGPIO_TRIGGER_MODE_BOTH);
	
	fds.fd = RH_GpioOpen(ASR_SND_DETECT_PIN);
	
	fds.events  = POLLPRI;

	setup_params.direction = 1;		/* 0-write, 1-read*/
	setup_params.reg = eSOUND_REG_EVENT;			/* cmdreg */
	setup_params.value = 0;		/* reg nums*/

	//pbcontex->asAhandler[ePB_TYPE_SYSTIP].replaytimes= 1;
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
//			LOG_P(demobugfs, RAK_LOG_INFO, "setup_params.value : %d......\n", setup_params.value);
			if(setup_params.value & eSOUND_EVENT_ID_ASR){
				if(!g_sAlexaConfig.micmute && g_pbhnd[ePB_TYPE_SYSTIP].tips != 1){
					E_ALEXA_STATE eAlexaResState = pAlexaResData->eAlexaResState;
					E_ALEXA_STATE simpleStatebit = HI_AlexaSimpleJudgeResState(eAlexaResState);//
					LOG_P(demobugfs, RAK_LOG_INFO, "eSOUND_EVENT_ID_ASR - %04x , %04x\n", eAlexaResState, simpleStatebit);
					g_u32CmdCode = HI_AlexaTriggerJudgeCmd(simpleStatebit, (dialog_status == eDIALOG_STATUS_LISTENING), eTRIG_WAY_VOICE);
					switch(simpleStatebit){
//						case eALEXA_STATE_MIC:{
//						}break;
						case eALEXA_STATE_NETWORK:
						{
							g_pbhnd[ePB_TYPE_SYSTIP].tips=1;
							RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOITN);
						}
						break;
						case eALEXA_STATE_USRACCOUNT:{
							RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOUSR);
						}break;
						case eALEXA_STATE_TOKEN:{
							g_pbhnd[ePB_TYPE_SYSTIP].tips=1;
							RK_sysStatusCues(eSYSTIPSCODE_ALEXA_TROBL);
						}break;
						default:
						break;
					}
				}
				
			}else if(setup_params.value & eSOUND_EVENT_ID_VAD){
				if(time(NULL) - alexa_capture_time > 1)
					stopcapture = true;
			}
		}
#endif
	}
	
	RH_GpioClose(ASR_SND_DETECT_PIN);
}



void *HI_AlexaRecognizeEventHandleThread
(
	void *arg 
)
{
	S_ALEXA_CONTEXT *psAlexaCxt = (S_ALEXA_CONTEXT *)arg;
	S_ALEXA_RES 	*pAlexaResData = psAlexaCxt->psResData;

	int ret;

	//LOG_P(demobugfs, RAK_LOG_DEBUG, "start submit recognize http interaction!\n");

	// prepare to submit a recognize interaction
	RK_StartRecognizeSendRequest(pAlexaResData);
	int i32HttpResponseCode;
	ret = RK_WaitResponseCode1(pAlexaResData, &i32HttpResponseCode);
	
	LOG_P(demobugfs, RAK_LOG_INFO, "Dialog Code: %d-%d\n", psAlexaCxt->ui8Index, i32HttpResponseCode);

	if(!ret && !i32HttpResponseCode){
		
		/*We must send the event to avs so that we can query the network connectivity*/
		RK_sysStatusCues(eSYSTIPSCODE_ALEXA_NOITN);
		LOG_P(demobugfs, RAK_LOG_INFO, ">>>>>>>>>>>>>>2eSOUND_EVENT_ID_ASR-%d<<<<<<<<<<<<\n", rk_audio_playback.pb_state(g_pbhnd[ePB_TYPE_SYSTIP].hnd));
		while(rk_audio_playback.pb_state(g_pbhnd[ePB_TYPE_SYSTIP].hnd) != ePB_STATE_FINISHED) usleep(100*1000);
	}
	
	RK_AlexaPrepareRecognizeEnd(pAlexaResData);

	/* change alexa light status to specify alexa finished. blue blink - turnoff */
	
	RK_LED_TurnOff(gs_led);
	dialog_status = eDIALOG_STATUS_IDLE;

	psAlexaCxt->ui8Done = 0;
	//LOG_P(demobugfs, RAK_LOG_TRACE, "psAlexaHnd->ui8Done: %d\n", psAlexaCxt->ui8Done);
	
	return NULL;
}

int HI_AlexaRecognizeEventHandle
(
	S_ALEXA_CONTEXT *psAlexaCxt, 
	unsigned int 	theCmd
)
{
	S_ALEXA_RES *pAlexaResData = psAlexaCxt->psResData;
	pthread_t	threadRecognizeID;
	uint8_t		iIdx;
	int expect_speech = RK_FAILURE;
	
	if(theCmd != ALEXA_CMD_START ){
		/* check passive expect speech */
		expect_speech = RK_AlexaEnquireExpectspeech(pAlexaResData);
		if(expect_speech == RK_FAILURE)
			return RK_FAILURE;
	}


	/* Preparation for a speech*/
	RK_AlexaPrepareRecognizeBegin(pAlexaResData);
	LOG_P(demobugfs, RAK_LOG_DEBUG, "RK_AlexaPrepareBegin Function Exit...\n");
	
	/*set request alive peirod time */
	while(psAlexaCxt->ui8Done)usleep(100*1000);
	psAlexaCxt->ui8Done = 1;
	//Trigger Capture voice
	sem_post(&s_sCmdCaptureSem);

	pthread_create(&threadRecognizeID, 0, HI_AlexaRecognizeEventHandleThread, (void *)psAlexaCxt);
	pthread_detach(threadRecognizeID);
	g_u32CmdCode = ALEXA_CMD_NONE;

	return RK_SUCCESS;
}

int RK_PBStartedCallback(void* hnd, void* usrpriv){
	int iIdx = (int)(long)usrpriv;
	{
		if(g_pbhnd[iIdx].hnd == hnd){
			if(iIdx<ePB_TYPE_CNT)
				RK_AlexaPlaybackStartedNotify(g_sAlexaContext.psResData, iIdx);
			if(iIdx == ePB_TYPE_DIALOG || iIdx == ePB_TYPE_PLAY_CID_TTS){
				if(iIdx == ePB_TYPE_DIALOG)
					dialog_status = eDIALOG_STATUS_SPEAKING;
				RK_LED_Flash(gs_led, 2);
			}
			if(iIdx == ePB_TYPE_SYSTIP && g_pbhnd[ePB_TYPE_SYSTIP].tips){
				RK_AlexaPlaybackBackground(g_sAlexaContext.psResData);
			}
		}
	}
}

int RK_PBFinishedCallback(void* hnd, void* usrpriv){
	int iIdx = (int)(long)usrpriv;
	{
		if(g_pbhnd[iIdx].hnd== hnd){
			if(iIdx<ePB_TYPE_CNT)
				RK_AlexaPlaybackFinishedNotify(g_sAlexaContext.psResData, iIdx);
			if(iIdx == ePB_TYPE_DIALOG || iIdx == ePB_TYPE_PLAY_CID_TTS){
				if(iIdx == ePB_TYPE_DIALOG && dialog_status == eDIALOG_STATUS_SPEAKING)
					dialog_status = eDIALOG_STATUS_IDLE;
				RK_LED_TurnOff(gs_led);
			}
			if(iIdx == ePB_TYPE_SYSTIP && g_pbhnd[ePB_TYPE_SYSTIP].tips){
				RK_AlexaPlaybackForeground(g_sAlexaContext.psResData);
				g_pbhnd[ePB_TYPE_SYSTIP].tips=0;
			}
		}
	}
}

static void *HI_AlexaWaitforAuthThread(void*arg)
{
	S_ALEXA_RES		*pAlexaRes = g_sAlexaContext.psResData;
	char**vc = (char**)arg;
	char*usrlogin = *vc;
	int msqid = *(int*)*(vc+1);
	char end =  **(vc+2);
	char secondstime = 0;
	int response;
	LOG_P(demobugfs, RAK_LOG_INFO, "msqid:%d, usrlogin pointer:%p\n", msqid, usrlogin);
	response = RH_AlexaAuthorize(pAlexaRes, usrlogin, eTOKEN_METHOD_APP, RH_WriteAlexaToken);
	if(response == 200)
		response = 0;	//ok
	else
		response = -1;
	
//	LOG_P(demobugfs, RAK_LOG_INFO, "========response:%d, msqid:%d\n", response, msqid);
	int ret = RK_SndIOCtrl(msqid, &response, sizeof(int), eIOTYPE_USER_MSG_HTTPD, eIOTYPE_MSG_AVSLOGIN_RESPONSE);
	LOG_P(demobugfs, RAK_LOG_INFO, "response Snd Data:%d, ret=%d\n", response, ret);
	free(usrlogin);usrlogin=NULL;
}

void *HI_AlexaCommunicationThread(void*arg)
{
	S_ALEXA_RES		*pAlexaResData = (S_ALEXA_RES *)arg;
	int msqid;

	while(eIOCTRL_ERR == (msqid = RK_MsgInit())){
		LOG_P(demobugfs, RAK_LOG_ERROR, "eIOCTRL_ERR:%d :%m\n", eIOCTRL_ERR);
		sleep(1);
	}
	SMsgIoctrlAvsGetAuth avs_get_auth={
		.code_method="S256",
		.product_dsn="",
		.product_id="my_device",
	};
	S_ALEXA_AUTH alexa_auth={
		.encodeMode="S256",
		.dsn="",
		.productID="my_device",

	};
	{//limit the scope of variables (fd, strline)
		FILE* fd = fopen(DEF_DEVICE_INFO_PATH, "rb");
		if(fd){
			char strline[77];
			while(fgets(strline, 64, fd)){
				if(!strncmp(strline, "serial_num #=", 13)){
					sscanf(strline+13, "%[^\r\n]", avs_get_auth.product_dsn);
					strcpy(alexa_auth.dsn, avs_get_auth.product_dsn);
					break;
				}
			}
			fclose(fd);
		}
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
				LOG_P(demobugfs, RAK_LOG_DEBUG, "RK_RecvIOCtrl:%m, and exit!!!!\n");
				exit(-1);
			}
			nblock++;
			msqp = realloc(msqp, msgHeaderSize + nblock*100);
//			LOG_P(demobugfs, RAK_LOG_DEBUG, "RK_RecvIOCtrl:%m, and try again!\n");
		}
		switch(msqp->u32iCommand){
			case eIOTYPE_MSG_NATSTATE://ÍøÂç×´Ì¬//
			{
				
				SMsgIoctrlNatState *avs_net_state = (SMsgIoctrlNatState *)msqp->next;
				if(!avs_net_state->restult){
					pAlexaResData->eAlexaResState |= eALEXA_STATE_NETWORK;
//					pAlexaResData->eAlexaResState |= eALEXA_STATE_ETHERNET;
					LOG_P(demobugfs, RAK_LOG_INFO, "NETWORK IS LINKED!\n");

				}else{
					pAlexaResData->eAlexaResState &= ~eALEXA_STATE_NETWORK;
				}
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_PHYSTATE://ÎïÀíÍø¿¨×´Ì¬
			{
				SMsgIoctrlNatState *avs_net_state = (SMsgIoctrlNatState *)msqp->next;
				if(!avs_net_state->restult){
//					pAlexaResData->eAlexaResState |= eALEXA_STATE_ETHERNET;
				}else{
//					pAlexaResData->eAlexaResState &= ~eALEXA_STATE_ETHERNET;
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
				int iCodeVerifierLen = RK_generateCodeVerifier(alexa_auth.codeVerifier);
				if(iCodeVerifierLen<=0){
					LOG_P(demobugfs, RAK_LOG_DEBUG, "generateCodeVerifier failed, and exit!!!!\n");
					exit(-1);
				}
				LOG_P(demobugfs, RAK_LOG_INFO, "iCodeVerifierLen==%d\n", iCodeVerifierLen);
				if(RK_generateCodeChallenge(alexa_auth.codeVerifier, iCodeVerifierLen , alexa_auth.codeChallenge)<=0){
					LOG_P(demobugfs, RAK_LOG_DEBUG, "generateCodeChallenge failed, and exit!!!!\n");
					exit(-1);
				}
				strcpy(avs_get_auth.code_challenge, alexa_auth.codeChallenge);
				int msgBodySize = sizeof(SMsgIoctrlAvsGetAuth);
				LOG_P(demobugfs, RAK_LOG_INFO, "Respond Data:\ndsn:%s,\nproduct:%s,\ncodeMode:%s,\ncode_challenge:%s;\n\nlocaldata:\ncode_verifier:%s\n", \
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
				char*UsrLogin = cJSON_PrintUnformatted(root);cJSON_Delete(root);//cJSON_PrintUnformatted(root)//cJSON_Print(root)
				LOG_P(demobugfs, RAK_LOG_DEBUG, "msqid=%d, ---->>>>json:\n%s\n", msqid, UsrLogin);

				pthread_t threadAlexaWaitforAuthID;
				pthread_create(&threadAlexaWaitforAuthID, 0, HI_AlexaWaitforAuthThread, (void*)&(void*[]){UsrLogin,(void*)&msqid,(void*)0});
				pthread_detach(threadAlexaWaitforAuthID);
				memset(msqp, 0, msgHeaderSize+nblock*100);

			}
			break;
			case eIOTYPE_MSG_AVSLOGOUT:{
				LOG_P(demobugfs, RAK_LOG_INFO, "---->>>>Bye Bye Alexa!<<<<----\n");
				RK_AlexaLogout(pAlexaResData);
				RH_WriteAlexaToken(NULL);
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}
			break;
			case eIOTYPE_MSG_SETTUNNELCTRL_REQ:
			case eIOTYPE_MSG_SETTUNNELCTRL_RESP:
				memset(msqp, 0, msgHeaderSize+nblock*100);
				break;
			case eIOTYPE_MSG_AVS_BUTTON_MUTE:{
				g_sAlexaConfig.micmute = !g_sAlexaConfig.micmute;
				RH_WriteAlexaConfig(&g_sAlexaConfig);
				memset(msqp, 0, msgHeaderSize+nblock*100);
				E_SYSTIPSCODE mic_state = (g_sAlexaConfig.micmute)? eSYSTIPSCODE_ALEXA_MUTE_MIC : eSYSTIPSCODE_ALEXA_UNMUTE_MIC;
				RK_sysStatusCues(mic_state);
				LOG_P(demobugfs, RAK_LOG_INFO, "trigger %s MIC button\n", (mic_state == eSYSTIPSCODE_ALEXA_MUTE_MIC)?"mute":"unmute");
			}break;
			case eIOTYPE_MSG_AVS_SETLANGUAGE:{
				LOG_P(demobugfs, RAK_LOG_INFO, "eIOTYPE_MSG_AVS_SETLANGUAGE=%s\n", msqp->next);
				if(RK_AlexaSetLocation(msqp->next)>=0){
					char * language;
					while(!(language = RK_AlexaGetLocation()))sleep(1);
					if(*(int*)language == *(int*)msqp->next){
						g_sAlexaConfig.pLocaleSet = language;
						RH_WriteAlexaConfig(&g_sAlexaConfig);
					}
				}else{
					LOG_P(demobugfs, RAK_LOG_INFO, "RK_AlexaSetLocation error\n");
				}
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}break;
			case eIOTYPE_MSG_AVS_GETLANGUAGE:{
				char * language;
				while(!(language = RK_AlexaGetLocation()))sleep(1);
				RK_SndIOCtrl(msqid, (void*)language, (size_t)strlen(language)+1, eIOTYPE_USER_MSG_HTTPD, eIOTYPE_MSG_AVS_GETLANGUAGE);
				if(*(int*)language != *(int*)g_sAlexaConfig.pLocaleSet){
					g_sAlexaConfig.pLocaleSet = language;
					RH_WriteAlexaConfig(&g_sAlexaConfig);
				}
				memset(msqp, 0, msgHeaderSize+nblock*100);
			}break;
			default:
				LOG_P(demobugfs, RAK_LOG_INFO, "---->>>>don't support CMD:%#x\n", msqp->u32iCommand);
				break;
		}
		
	}
}


#define webreqtoken "grant_type=authorization_code&" \
					"code=ANTgIQALKaRByBNcBjlR&" \
					"client_id=amzn1.application-oa2-client.52fcbe8bc8b24e53864b55e8a72ef4b8&" \
					"client_secret=4282650ce9f34773bd88bf563ca306b0ad739371a1fd39040c5549babf976abc&" \
					"redirect_uri=https://localhost:3000/authresponse"

#define ALEXA_TOKEN_WEB_VALUE 	"{" \
								"\"grant_type\":\"authorization_code\"," \
								"\"code\":\"ANPyitATpDiXYQYDCFYv\"," \
								"\"client_id\":\"amzn1.application-oa2-client.c1076f5a1a0948d59cfa19cb7ee46f07\"," \
								"\"client_secret\":\"5e5859ede687998d1271477e4f4da760af0fd1fe3fabcec77b99953c34e7da9d\"," \
								"\"redirect_uri\":\"https://localhost:3000/authresponse\"" \
								"}"
							

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int main(int argc, char **argv)
{
	S_ALEXA_RES		*pAlexaRES;
	S_ALEXA_TOKEN 	AlexaToken = (S_ALEXA_TOKEN){0};
	pthread_t 		threadCaptureID;	/* capture voice task*/
	pthread_t 		threadAsrTriggerID;
	pthread_t 		threadCommunicationID;
	
	static struct timespec	s_tsReq	= {0, 1000};
	uint64_t		ui64CurSec = 0,
					ui64IntervalSec = 0;
	
	int 			ret = eALEXA_ERRCODE_SUCCESS;
	
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE); 
	if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) == -1) 
		perror("MAIN PTHREAD SIG_PIPE");

#if 0	
	if(argc == 2)
		speechfile = atoi(argv[1]);
#endif
	
	/* description a audio handle information type for customer debugs logs */
	DEBUG_MASK_DESCRIPTION player_desc[] = {
		{ ePB_TYPE_DIALOG,			"dialog"},
		{ ePB_TYPE_ALERT,			"alert"},
		{ ePB_TYPE_PLAY_STREAM, 	"media stream"},
		{ ePB_TYPE_PLAY_CID_TTS,	"media tts"},
		{ ePB_TYPE_SYSTIP,			"system tips sound"},
	};

	/// 1, define and init alexa resource data
	/* define tls configure data to request liscense auth */
	S_AUTH_CONF sTlsConf = {
		.u16TlsPort = 9017,
		.pTlsServerIP = NULL,
		.pTlsDomainName = "license.cn-test-2.hyiot.io",
		.pTlsCertName = "/etc/ssl/certs/ca.crt",
		.pTlsClientCertName = "/etc/ssl/certs/dlsclient.crt",
		.pTlsClientKeyName = "/etc/ssl/certs/dlsclient.key",
		.pUsrerID = "8b3f557e",
		.pLicenseSN = "0cb26e15-7c31-4110-a53a-057592da9a4c",
	//	.pLicenseSN = "8a6eb7aa-ac10-4aee-9f4d-600ba92b1c01",
	} ;

	/* define alert relative config params */
	static S_ALERT_BASE_RES alertBaseRes={
		.iAlertTimeOut = ALERT_TIME_OUT,
		.iStopNextTime = STOP_NEXT_TIME,
		.pAlarmAudioFile = DEFAULT_ALERT_FILE_ALARM,
		.pTimerAudioFile = DEFAULT_ALERT_FILE_TIMER,
		.pAlertConfigFile = DEF_ALERT_CONF_FILE_NAME,
		.pAlertConfigFileTmp = DEF_ALERT_CONF_FILE_NAME_TMP
	};

	/* default dialog status is idle */
	dialog_status = eDIALOG_STATUS_IDLE;

	/* init debug logs interface */
	demobugfs = rak_log_init("DEMO", RAK_LOG_INFO, 8, NULL, NULL);
	LOG_P(demobugfs, RAK_LOG_INFO, "alexa_run_demo start.\n");
	
	/* init alexa context resource data and alloc mem speace */
	memset(&g_sAlexaContext, 0, sizeof(S_ALEXA_CONTEXT));
	g_sAlexaContext.psResData = malloc(sizeof(S_ALEXA_RES));
	if(g_sAlexaContext.psResData == NULL){
		LOG_P(demobugfs, RAK_LOG_ERROR, "avsResData: %s init failed.\n", strerror(errno));
	}
	/* Allocate mem resource */
	memset(g_sAlexaContext.psResData, 0, sizeof(S_ALEXA_RES));

	/* init device audio playback interface */
	RK_Playback_global_init();
	
	/* init led light to indicate alexa status */
	gs_led = RK_LED_Init(LED_PIN);

	/// 2, gain and print current libavs library version
	RK_AlexaVersion();

	/// 3, init libalexa library resorce params
	RK_AlexaGlobalInit(g_sAlexaContext.psResData);

	/// 4, setup playback handle interface //implement
	RK_AlexaSetupPlaybackInterface(g_sAlexaContext.psResData, &rk_audio_playback);

	/* create and implement audio playback interface*/
	int iIdx;
	for(iIdx=ePB_TYPE_DIALOG; iIdx<=ePB_TYPE_CNT; iIdx++){
		ret = RK_Playback_create(&g_pbhnd[iIdx].hnd, player_desc[iIdx].Description, RK_PBStartedCallback, (void*)(long)iIdx, RK_PBFinishedCallback, (void*)(long)iIdx);
		if(!ret){
			/// 5, setup playback private attribute resource data 
			if((iIdx < ePB_TYPE_CNT) && RK_AlexaSetupPlaybackAttr(g_sAlexaContext.psResData, iIdx, g_pbhnd[iIdx].hnd)){
				LOG_P(demobugfs, RAK_LOG_ERROR, "SetupPlaybackAttr hnd %uhh :%m\n", iIdx);
				exit(1);
			}
		}
	}

	RK_AlexaSetupDecodecInterface(g_sAlexaContext.psResData, &decodecStreamItf);
	/// 6, Check for a presence of user account attribute and notic to libavs
	/* Get alexa config params if already restore alexa config params 
	 * in a files e.g: /etc/wiskey/alexa.conf, otherwise you can skip
	 * the step6 and you must to login a alexa account.
	 */
	RH_ReadAlexaConfig(&AlexaToken);
	/* notic avs have a presence user account attribute */
	RH_AlexaTokenResume(g_sAlexaContext.psResData, &AlexaToken, (AlexaToken.pClientSecret == NULL)?eTOKEN_METHOD_APP:eTOKEN_METHOD_WEB);
	RK_AlexaSetLocation(g_sAlexaConfig.pLocaleSet);

	/// 7, setup alert relative config params 
	if(RK_AlertRes_Set(g_sAlexaContext.psResData, &alertBaseRes) != 0){
		fprintf(stdout, "alert resoure set fail!\n");
		return -1;
	}
	
	pAlexaRES = g_sAlexaContext.psResData;

	/// 8, setup user speech deliver callback interface, it will send user speech from 
	///    device capture audio data to avs.
	RK_AlexaSetSpeechDeliverItf(pAlexaRES, SpeechDeliverToAVS, NULL);

	/* user must to creat a capture audio data task to provide speech deliver callback*/
	pthread_create(&threadCaptureID, 0, HI_AlexaCaptureThread, (void *)pAlexaRES);
    pthread_detach(threadCaptureID);
	
	/* impletement a trigger way: ASR/TAP/BUTTON */
	pthread_create(&threadAsrTriggerID, 0, HI_AlexaAsrTriggerThread, pAlexaRES);
    pthread_detach(threadAsrTriggerID);

	/* alexa demo communicate with external interface*/
	pthread_create(&threadCommunicationID, 0, HI_AlexaCommunicationThread, pAlexaRES);
    pthread_detach(threadCommunicationID);

	/// 9, alexa liscense auth
	//RK_AlexaServiceRegister(&sTlsConf);
	RK_AlexaUnAuthLicense();
	LOG_P(demobugfs, RAK_LOG_INFO, "Liscense Auth success.\n");
	
	/// 10, start alexa master task
	RK_AlexaMasterWorkHandler((S_ALEXA_RES *)pAlexaRES);
	LOG_P(demobugfs, RAK_LOG_INFO, "Wait for access token ...\n");
	if (argc >=2) {
		int response;
		sleep(1);
		while(0 == (response = RH_AlexaAuthorize(pAlexaRES, ALEXA_TOKEN_WEB_VALUE, eTOKEN_METHOD_WEB, RH_WriteAlexaToken))){
			LOG_P(demobugfs, RAK_LOG_INFO, "Loop ...\n");
			usleep(1000);
		}
		if(response != 200)
			return -1;
	}
	
	while(!(pAlexaRES->eAlexaResState & eALEXA_STATE_TOKEN)){
		sleep(1);
	}
	
	RK_sysStatusCues(eSYSTIPSCODE_ALEXA_LOGINING);
	LOG_P(demobugfs, RAK_LOG_INFO, "Wellcome to wiscore alexa ...\n");
	while(!g_uiQuit){
		if(pAlexaRES->eAlexaResState & eALEXA_STATE_TOKEN){
			/* 11, initiated a event request */
			HI_AlexaRecognizeEventHandle(&g_sAlexaContext, g_u32CmdCode);
		}
		nanosleep(&s_tsReq, NULL);
	}
	
	/// 12 destory alexa resource
	RK_AlexaGlobalDestory(pAlexaRES);

CANCEL_MALLOC:
	
	LOG_P(demobugfs, RAK_LOG_INFO, "Alexa Demo Exit...\n");
	return ret;
}


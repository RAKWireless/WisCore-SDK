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
/*rak private header files*/
#include "rk_string.h"
#include "alexa_common.h"
//#include "alarm_timer.h"
#include "avs_private.h"
//#include "hls_private.h"
#include "ffplayer_dec.h"
//#include "alexa_time.h"
#include "alexa_alert.h"
#include "alexa_media.h"

#define DEF_PING_INTERVAL_TIME		300
#define DEF_ENABLE_PB

#define DEF_AVS_VERSION		"release-0.2.0-full"
#define DEF_AVS_DATE		"2017-05-18"
#define DEF_AVS_AUTHER		"Seven"
#define DEF_TIME_OUT	30

static S_ALERT_BASE_RES alertBaseRes={
	.iAlertTimeOut = 30*60,
	.iStopNextTime = 20,
	.pAlarmAudioFile = "/usr/lib/sysaudio/alarm.pcm",
	.pTimerAudioFile = "/usr/lib/sysaudio/timer.pcm",
	.pAlertConfigFile = "/etc/wisalexa/alert_struct.conf",
	.pAlertConfigFileTmp = "/tmp/alert_struct.conf.new"
};

static	bool			gs_bStopCapture = false;
unsigned char speech_finish_state = 0;

//seven temp
//FILE *directfp = NULL;
FILE *directfp1 = NULL;
/* set a max number of timer */
#define		TIMER_MAX	6

typedef struct avs_request_timer {
	uint64_t	ui64StartSec;
	uint64_t	ui64CurrentSec;
}S_AVS_REQ_TIMER;
S_AVS_REQ_TIMER gs_ReqTimeo[eMAX_NUM_HANDLES];

typedef enum {
	eDELALERT_FAILED = 0,
	eDELALERT_SUCCEEDED = 1,
}E_DELETE_ALERT_STATE;
typedef enum {
	eSETALERT_FAILED = 0,
	eSETALERT_SUCCEEDED = 1,
}E_SET_ALERT_STATE;

typedef enum {
	eTYPE_RECONGINZE,
	eTYPE_SYNCHRONIZESTATE,
	eTYPE_EXCEPTIONENCOUNTERED,
}E_TYPE_EVENT;

/* scheduledTime set */
typedef struct AlertTimerSet {
	bool				bBackGround;			/* add 20161107 merge*/
	uint32_t			ui32TimerMax;			/* max number of timer: TIMER_MAX */
	uint32_t			i32CurrentTimer;		/* current no active timer will be set TIMER_MAX+1 */
	S_AVS_scheduledTime	asAvsSchTm[TIMER_MAX];
	S_SetAlertDirective asAlertDirective[TIMER_MAX];
	S_SetAlertDirective sDirectiveSetAlertInput;	//eScheduledState == eSetAlertSucceeded
} S_SetAlertDirectiveSet;

typedef struct playDirectiveSet{
	uint8_t			ui8DFlag;					/*directive play: 0 - play directive from nearly finished, 1 - from speech voice */
	E_MPLAY_STATE	eMplayPriorityState;
	S_PlayDirective asReplacePlayStream;		/* for replace_equeue @2017-0210*/
	S_PlayDirective asCurrentPlayStream;
	S_PlayDirective asDirectivePlay[2];			/*ping-pong*/
	pthread_mutex_t asPlayMutexLock;
	sem_t	asPlaySem;
}S_PlayDirectiveSet;

typedef struct AvsDirectives {
	S_SpeakDirective 		sDirectiveSpeak;
	S_ExpectSpeechDirective	sDirectiveExpectSpeech;
	S_SetAlertDirectiveSet	sDirectiveSetAlert;
	S_DeleteAlertDirective  sDirectiveDeleteAlert;
	S_PlayDirectiveSet		sDirectivePlay;
	S_StopDirective			sDirectiveStop;
	S_ClearQueueDirective	sDirectiveClearQueue;
	S_SetVolumeDirective	sDirectiveSetVolume;
	S_AdjustVolumeDirective	sDirectiveAdjustVolume;
	S_SetMuteDirective		sDirectiveSetMute;
	S_ResetUserInactivityDirective sDirectiveRestUserInactivity;
	S_SetEndpointDirective	sDirectiveSetEndpoint;
	E_AVS_EVENT_ID			eEventID;
}S_AvsDirectives;

////20170406 cg add mutelock for hardware volume
pthread_mutex_t hwVolumeMutexLock = PTHREAD_MUTEX_INITIALIZER;

S_AvsDirectives	g_astrAvsDirectives;

SysStRdFunc_t 		RK_SysRdState = NULL;
SysStWrFunc_t 		RK_SysWrState = NULL;
SysStTipsFunc_t 	RK_SysAudTips = NULL;

void RK_SysFuncSet(void * sysrd, void* syswr, void * systips)
{
	RK_SysRdState = sysrd;
	RK_SysWrState = syswr;//RK_SysWrState(eSYSSTATE_ALERTALL_NUMADD, 1);RK_SysWrState(eSYSSTATE_ALERTTIMEUP_NUMADD, 1);eSYSSTATE_ETHERNET;eSYSSTATE_NETWORK
	RK_SysAudTips = systips;//RK_SysAudTips(eSYSTIPSCODE_ALERT_FAIL);
}

uint64_t utils_get_sec_time(void)
{
	return time((time_t*)NULL);
}

int utils_auto_set_time(void)
{
	return system("/etc/init.d/sysntpd restart");
}

/*****************************audio_player api***************************************/
int RK_APBFinished(S_PBContext *pPBCxt, E_PB_TYPE ePBType, int i32TimeoMs)
{
	return pPBCxt->asAPlayback->pb_wait_finished(pPBCxt, ePBType, i32TimeoMs);
}

int RK_APBPause(S_PBContext *pPBCxt, E_PB_TYPE ePBType, int pause)
{
	return pPBCxt->asAPlayback->pb_pause(pPBCxt, ePBType, pause);
}

int RK_APBSetDmixVolume(S_PBContext *pPBCxt, E_PB_TYPE ePBType, float f16Volume)
{
	return pPBCxt->asAPlayback->pb_set_dmix_volume(pPBCxt, ePBType, f16Volume);
}

int RK_APBSetHardwareVolume(S_PBContext *pPBCxt, int i32Vol, int isSetVol)
{
//cg add mutexlock
	pthread_mutex_lock(&hwVolumeMutexLock);
	int ret = pPBCxt->asAPlayback->pb_hardwarevolume(i32Vol, isSetVol);
	pthread_mutex_unlock(&hwVolumeMutexLock);
	return ret;
}

int RK_APBWrite(S_PBContext *pPBCxt, E_PB_TYPE ePBType, unsigned char *pSrcData, int i32Length)
{
	if(pSrcData == NULL){
		pPBCxt->asAhandler[ePBType].blast_packet = 1;
	}
	
	if(pPBCxt->asAhandler[ePBType].i8PbIdx<0){
		ALEXA_DBG("asAhandler->i8PbIdx is - %d\n", pPBCxt->asAhandler[ePBType].i8PbIdx);
		return -5;
	}
	
	return pPBCxt->asAPlayback->pb_write(pPBCxt, ePBType, pSrcData, i32Length);
}

int RK_APBClose(S_PBContext *pPBCxt, E_PB_TYPE ePBType)
{
	return pPBCxt->asAPlayback->pb_close(pPBCxt, ePBType);
}

int RK_APBOpen(S_PBContext *pPBCxt, E_PB_TYPE ePBType)
{
	return pPBCxt->asAPlayback->pb_open(pPBCxt, ePBType);
}

void RK_APBChange_handleState(S_PBContext *pPBCxt, int state)
{
	pPBCxt->asAPlayback->pb_change_handle(pPBCxt, state);

//	RK_PBChange_handleState(pPBCxt, state);
}

int RK_APBStream_open_callback(void *h, int mtype, int smplrate)
{
	S_PBContext *pPBCxt = (S_PBContext *)h;
	E_PB_TYPE	ePBType = (E_PB_TYPE)mtype;

	S_AudioHnd	*pAudioHnd = &pPBCxt->asAhandler[ePBType];

	pAudioHnd->ui32smplrate = smplrate;
	pAudioHnd->eAudioMask = eAUDIO_MASK_PCM;

	return pPBCxt->asAPlayback->pb_open(pPBCxt, ePBType);
}

int RK_APBStream_close_callback(void *h, int mtype)
{
	return RK_APBClose((S_PBContext *)h, (E_PB_TYPE)mtype);
}

int RK_APBStream_write_callback(void *h, int mtype, const unsigned char *pSrcData, int i32Length)
{
	S_PBContext *pPBCxt = (S_PBContext *)h;
	E_PB_TYPE 	ePBType = (E_PB_TYPE)mtype;
	
	return RK_APBWrite(pPBCxt, ePBType, (unsigned char *)pSrcData, i32Length);
}

/*we need to block until dialog speech playback is complete */
void RK_APBStream_open_block(bool bBlock)
{
	RK_FFPlayer_OpenStreamBlock(bBlock);
}

/**
 * Description: use this function to create a multi handle and returns a CURLM handle 
 * to be used as input to all the other multi-functions, sometimes referred to as a 
 * multi handle in some places in the documentation. This init call MUST have a 
 * corresponding call to curl_multi_cleanup when the operation is complete.
 * @params: NULL
 * @return : a CURLM handle
*/
CURLM *RK_CurlMulitInit(void)
{
	return curl_multi_init();
}
/**
 * Description: Cleans up and removes a whole multi stack. It does not free or 
 * touch any individual easy handles in any way - they still need to be closed 
 * individually, using the usual curl_easy_cleanup way
 * @params mcurlHnd: a vaild CURLM handle
 * @return : null
*/
void RK_CurlMulitCleanup(CURLM *mcurlHnd)
{
	curl_multi_cleanup(mcurlHnd);
}

void RK_SetupCurlHandle(CURL *hnd, int num)
{
	RK_AVSSetupCurlHandle(hnd, num);
}

/**
 * Description: use this function to create an easy handle and must be the first function to call,
 * and it returns a CURL easy handle that you must use as input to other functions in the easy 
 * interface
 * @params: NULL
 * @return : a CURL handle
*/
void RK_CurlEasyInit(S_AVS_CURL_SET *pAlexaCurlSet)
{
	int i;
	/* Initialization a easy stack for each resource request */
	for(i=0; i<eMAX_NUM_HANDLES; i++){
		pAlexaCurlSet->asCurlSet[i].curl = curl_easy_init();
		RK_SetupCurlHandle(pAlexaCurlSet->asCurlSet[i].curl, i);
		pAlexaCurlSet->asCurlSet[i].ui8Quite = 2;	//0, 1 ,2
	}
}
/**
 * Description: this function must be the last function to call for an easy session. 
 * It is the opposite of the curl_easy_init function and must be called with the same
 * handle as input that a curl_easy_init call returned.
 * @params curlHnd - a vaild CURL handle
 * @return : null
*/
void RK_CurlEasyCleanup(S_AVS_CURL_SET 	*pAlexaCurlSet)
{
	int i;
	/*end and clean up all alexa resource easy session*/
	for(i=0; i<eMAX_NUM_HANDLES; i++){
		curl_easy_cleanup(pAlexaCurlSet->asCurlSet[i].curl);
		pAlexaCurlSet->asCurlSet[i].curl = NULL;
		sem_destroy(&pAlexaCurlSet->asCurlSet[i].m_CurlSem);
	}
}

/*************************avs api***************************/

/* close record filedescriptor*/
void RK_CloseAlexaRecordData(S_ALEXA_DEB_FS *psAVSDebFs)
{
	if(psAVSDebFs != NULL){
		if(psAVSDebFs->headerfs)
			fclose(psAVSDebFs->headerfs);
		if(psAVSDebFs->bodyfs)
			fclose(psAVSDebFs->bodyfs);
#if 0
		if(psAVSDebFs->header_name)
			free(psAVSDebFs->header_name);
		if(psAVSDebFs->body_name)
			free(psAVSDebFs->body_name);
		psAVSDebFs->header_name = NULL;
		psAVSDebFs->body_name = NULL;
#endif
		psAVSDebFs->headerfs = NULL;
		psAVSDebFs->bodyfs = NULL;
	}
}

/* record dialog data from avs server */
int RK_OpenAlexaRecordData(S_ALEXA_DEB_FS	 *psAVSDebFs)
{
	int ret = eALEXA_ERRCODE_SUCCESS;
	
	if(psAVSDebFs != NULL){
		psAVSDebFs->headerfs = fopen(psAVSDebFs->header_name, "w+" );
		if( psAVSDebFs->headerfs == NULL ){
			perror("fopen");
			ret = eALEXA_ERRCODE_WRITE_FILE_FAILED;
		}
		
		psAVSDebFs->bodyfs = fopen( psAVSDebFs->body_name,	"w+" );
		if( psAVSDebFs->bodyfs == NULL ){
			perror("fopen");
			ret = eALEXA_ERRCODE_WRITE_FILE_FAILED;
		}
	}

	if(ret != eALEXA_ERRCODE_SUCCESS){
		RK_CloseAlexaRecordData(psAVSDebFs);
	}

	return ret;
}

void RK_AlexaVersion(void)
{
	fprintf(stderr, "\033[1;41mTURTLE-VER: %s-%s <%s>\033[0m\n", DEF_AVS_VERSION, DEF_AVS_DATE, DEF_AVS_AUTHER);
	RK_AVSVersion();
	RK_LibnmediaVersion();
}

void RK_AlexaReqestBegin(S_CURL_SET *psCurlSet, int isTimer)
{
	RK_AVSCurlRequestSubmit(psCurlSet, isTimer);
}

void RK_AlexaRequestDone(S_CURL_SET	*psCurlSet)
{
	RK_AVSCurlRequestBreak(psCurlSet);
}

int RK_WaitResponseCode1(S_CURL_SET *psCurlSet)
{
	return RK_AVSWaitRequestCompleteCode(psCurlSet);
}
/*空闲返回0，有连接返回1*/
int RK_AlexaRequestCheckState(S_AVS_CURL_SET 	*pAvsCurlSet)
{
	int found1 = 0, found2 = 0;
	int iIdx;

	/*每次查询两次确保句柄处于空闲状态*/
	for(iIdx=0; iIdx<2; iIdx++){
		found1 = RK_AVSCurlCheckRequestState(pAvsCurlSet);
		/* 让出资源 等待1s， 再一次查询*/
		sleep(1);
		found2 = RK_AVSCurlCheckRequestState(pAvsCurlSet);
	}

	if(found2)
		return found2;

//	if(found1 == found2)
	
	return found1;
}

//add 20161107 merge
static int RK_AlertStruct_to_File(S_SetAlertDirectiveSet* p_struct, char*filename)
{
	if(p_struct == NULL || filename == NULL){
		printf("---->>>>Alert---S_SetAlertDirectiveSet *p_struct || configfilename == NULL----by yan\n");
		return -1;
	}
	int ret = 0;
	FILE* fd = fopen(filename, "wb");
	switch((int)(fd && 1)){
		case 0: ret = -1;break;
		default :
//			size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
			ret = (fwrite((const void *)p_struct, 1, sizeof(S_SetAlertDirectiveSet), fd) > 0) ? fflush(fd): -1;
	}
	fclose(fd);
	return ret;
}

void RK_Alert_Update_ConfigFile(void)
{
	RK_AlertStruct_to_File(&g_astrAvsDirectives.sDirectiveSetAlert, alertBaseRes.pAlertConfigFileTmp);
	char *cJsonData = RK_AlertStruct_to_Json(g_astrAvsDirectives.sDirectiveSetAlert.asAlertDirective, TIMER_MAX);
	RK_AlertJson_Append_to_StructFile(cJsonData, alertBaseRes.pAlertConfigFileTmp);
	char cmd[128] = {0};
	sprintf(cmd, "mv %s %s", alertBaseRes.pAlertConfigFileTmp, alertBaseRes.pAlertConfigFile);
	ALEXA_INFO("%s Started\n", cmd);
	system(cmd);
	ALEXA_INFO("%s Over\n", cmd);
	if(cJsonData)
		free(cJsonData);
}

static void RK_Alert_Initial_In(S_SetAlertDirectiveSet* p_struct, char*filename)
{
	if(p_struct == NULL || filename == NULL){
		printf("---->>>> 1(or 2) parameter(s) is(are) NULL---by yan\n");
		return;
	}

	FILE* fd = fopen(filename, "rb");
	memset(p_struct, 0, sizeof(S_SetAlertDirectiveSet));
	if(fd == NULL){
		return ;
	}
	size_t structsize = sizeof(S_SetAlertDirectiveSet);
	size_t readsize = fread(p_struct, 1, structsize, fd);
	if(readsize == structsize){
		printf("---->>>>__RK_Alert_Initial succeeded ----by yan\n");
	}else{
		printf("---->>>>__RK_Alert_Initial failed ----by yan\n");
	}
	fclose(fd);
}

void RK_Alert_Initial(void *arg)
{
	RK_Alert_Initial_In( &g_astrAvsDirectives.sDirectiveSetAlert, alertBaseRes.pAlertConfigFile);	
//	g_astrAvsDirectives.sDirectiveSetAlert.asAvsSchTm[0].m_itmeno = 0;
	g_astrAvsDirectives.sDirectiveSetAlert.ui32TimerMax = TIMER_MAX;
	g_astrAvsDirectives.sDirectiveSetAlert.i32CurrentTimer = TIMER_MAX + 1;
}

void RK_Alert_Reset(S_ALEXA_RES *pAlexaResData)
{
	memset(&g_astrAvsDirectives.sDirectiveSetAlert, 0, sizeof(S_SetAlertDirectiveSet));
	memset(&g_astrAvsDirectives.sDirectiveDeleteAlert, 0, sizeof(S_DeleteAlertDirective));
	RK_Alert_ConfigFile_Initial(alertBaseRes.pAlertConfigFile);
	if(pAlexaResData && pAlexaResData->psPlaybackCxt && pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE){
		if(RK_APBFinished(pAlexaResData->psPlaybackCxt, ePB_TYPE_ALERT, 1)){
			if(pAlexaResData->psPlaybackCxt->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE)
				RK_APBClose(pAlexaResData->psPlaybackCxt, ePB_TYPE_ALERT);//S_PBContext *pPBCxt
		}
	}
}
/*0 -success, negative - fail*/
int RK_AlertRes_Set(void * arg)
{
	alertBaseRes = *(S_ALERT_BASE_RES*)arg;
	
	if(access(alertBaseRes.pAlarmAudioFile, R_OK | F_OK) != 0){
		ALEXA_ERROR("No find the alarm file: <%s>\n", alertBaseRes.pAlarmAudioFile);
		return -1;
	}

	if(access(alertBaseRes.pTimerAudioFile, R_OK | F_OK) != 0){
		ALEXA_ERROR("No find the Timer file: <%s>\n", alertBaseRes.pTimerAudioFile);
		return -1;
	}

	return 0;
}

int RK_Alert_Delete_Index(S_SetAlertDirectiveSet* p_Alerts, int iIndex, int iNumDirectives)
{
	if(p_Alerts == NULL || iIndex < 0 || iIndex >= iNumDirectives){
		ALEXA_INFO("---->>>>Alert--RK_Alert_Delete_Index--S_SetAlertDirectiveSet* p_Alerts == NULL----by yan\n");
		return -1;
	}
	if(-1 == RK_Schedule_Delete_Index(p_Alerts->asAvsSchTm, iIndex, iNumDirectives)){
		printf("---->>>>RK_Alert_Delete_Index do RK_Schedule_Delete_Index failed ----by yan\n");
		return -1;
	}
	if(-1 == RK_AlertDirective_Delete_Index(p_Alerts->asAlertDirective, iIndex, iNumDirectives)){
		printf("---->>>>RK_Alert_Delete_Index do RK_AlertDirective_Delete_Index failed ----by yan\n");
		return -1;
	}
	return 0;
}

void RK_Alert_Delete_Alerting(S_PBContext *psPlaybackCxt, S_SetAlertDirectiveSet *pSetAlerts, S_DeleteAlertDirective *pDeleteAlert)
{
	struct AudioHandler *pAlertAudioHandler= psPlaybackCxt ? &psPlaybackCxt->asAhandler[ePB_TYPE_ALERT] : NULL;
	pDeleteAlert->cDeleteSucceeded = 0 + eDELALERT_SUCCEEDED;
	if((RK_Schedule_Get(pSetAlerts->asAvsSchTm + 1)>=0) && (!strcmp(pSetAlerts->asAlertDirective[1].strType, pSetAlerts->asAlertDirective[0].strType))){
		pDeleteAlert->cDeleteSucceeded = 0 + eDELALERT_SUCCEEDED + TIMER_MAX;
	}else if(pAlertAudioHandler && pAlertAudioHandler->ePlaybackState != ePB_STATE_IDLE){//is alerting//time is not up or alert is not the same with next alert.
		if(RK_APBFinished(psPlaybackCxt, ePB_TYPE_ALERT, 1)){
			if(pAlertAudioHandler->ePlaybackState != ePB_STATE_IDLE){
				if(RK_APBClose(psPlaybackCxt, ePB_TYPE_ALERT))//close succeeded.
					pDeleteAlert->cDeleteSucceeded = 0 + eDELALERT_FAILED;
			}
		}
	}
}

S_SetAlertDirective* RK_GetAlertDirectives(void)
{
	return g_astrAvsDirectives.sDirectiveSetAlert.asAlertDirective;
}

int RK_GetAlertMaxNum(void)
{
	return TIMER_MAX;
}

int RK_Alert_Can_Started(S_ALEXA_RES *psAlexaRes)// can started return 1;  0 error or cannt started;
{

	//S_AudioPlayback *pPBHnd = psAlexaRes->psPlaybackCxt->asAPlayback;
	if((g_astrAvsDirectives.sDirectiveSetAlert.bBackGround) || (g_astrAvsDirectives.sDirectiveDeleteAlert.cDeleteSucceeded > TIMER_MAX))
		return 0;
	if(g_astrAvsDirectives.sDirectiveSetAlert.asAlertDirective[0].eScheduledState >= eSCH_STATE_ALERTING)
		return 0;
	
	return 1;
}

//end
static int RK_AlexaResourcesParamInit(S_ALEXA_RES *psAlexaRes)
{
	/* initialization all avs related directives */
	memset(&g_astrAvsDirectives, 0, sizeof(g_astrAvsDirectives));
	if(pthread_mutex_init(&g_astrAvsDirectives.sDirectivePlay.asPlayMutexLock, NULL) != 0)
	{
		ALEXA_ERROR("ERROR: Could not initialize play mutex variable\n");
		return -1;
	}
	
	sem_init(&g_astrAvsDirectives.sDirectivePlay.asPlaySem, 0, 0);
	
	//init default time
	g_astrAvsDirectives.sDirectiveRestUserInactivity.i32InactiveSec = (long)utils_get_sec_time();
	//get hwvolume
	g_astrAvsDirectives.sDirectiveSetVolume.i32Volume = RK_APBSetHardwareVolume(psAlexaRes->psPlaybackCxt,0,0);
	
	return 0;
}

static E_AVS_EVENT_ID RK_AlexaDirectiveParseJson
(
	char			*cJsonData,
	S_AvsDirectives *pAvsDirectives,
	uint8_t			ui8Channel			/* 0 - others directive path, 1 - speech path */	
)
{
	E_AVS_EVENT_ID 		eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;
	E_AVS_DIRECTIVE		eAvsDirectiveID = eAVS_DIRECTIVE_UNKNOW;
	void				*pData = NULL;
	cJSON 	*pJson = NULL;
	int 	value_len;
	
	if(cJsonData == NULL){
		ALEXA_ERROR("cJsonData is NULL!\n");
		return eAVS_DIRECTIVE_EVENT_UNKNOW;
	}

	pAvsDirectives = &g_astrAvsDirectives;
	
	do{
		
	pJson = cJSON_Parse(cJsonData);
	if(pJson == NULL){
		ALEXA_ERROR("No found valid cjson format!\n");
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;
		break;
	}

	cJSON * pRoot = cJSON_GetObjectItem(pJson, "directive");
	if(pRoot == NULL){
		ALEXA_ERROR("No found directive!\n");
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;
		break;
	}

	cJSON *pSubHeader = cJSON_GetObjectItem(pRoot, "header");
	if(pSubHeader == NULL){
		ALEXA_ERROR("No found header!\n");
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;
		break;
	}

	cJSON *pSubPayload = cJSON_GetObjectItem(pRoot, "payload");
	if(pSubPayload == NULL){
		ALEXA_ERROR("No found payload!\n");
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;
		break;
	}

	cJSON *pSubSubName = cJSON_GetObjectItem(pSubHeader, "name");
	if(pSubSubName != NULL){
		value_len = strlen(pSubSubName->valuestring);
		if(strcmp(pSubSubName->valuestring, "StopCapture") == 0){
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_STOP_CAPTURE;
			break;
			
		}else if(strcmp(pSubSubName->valuestring, "Speak") == 0){
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_SPEECHSTARTED;
			eAvsDirectiveID = eAVS_DIRECTIVE_SPEAK;
			pData = (void *)&pAvsDirectives->sDirectiveSpeak;
			memcpy(pAvsDirectives->sDirectiveSpeak.strName, pSubSubName->valuestring, value_len);
			pAvsDirectives->sDirectiveSpeak.strName[value_len] = '\0';
			
		}else if(strcmp(pSubSubName->valuestring, "ExpectSpeech") == 0){
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_EXPECTSPEECH;
			eAvsDirectiveID = eAVS_DIRECTIVE_EXCEPTSPEECH;
			pData = (void *)&pAvsDirectives->sDirectiveExpectSpeech;
			memcpy(pAvsDirectives->sDirectiveSpeak.strName, pSubSubName->valuestring, value_len);
			pAvsDirectives->sDirectiveExpectSpeech.strName[value_len] = '\0';
			
		}else if(strcmp(pSubSubName->valuestring, "SetAlert") == 0){
			S_SetAlertDirectiveSet	*psDirectiveSetAlertSet = &pAvsDirectives->sDirectiveSetAlert;
			//S_SetAlertDirective		*psDirectiveSetAlert;
			//uint32_t	iIdx;
			
			//cJSON * p_scheduledTime = cJSON_GetObjectItem(pSubPayload, "scheduledTime");
			memset(&psDirectiveSetAlertSet->sDirectiveSetAlertInput, 0, sizeof(S_SetAlertDirective));
			pData = (void *)&psDirectiveSetAlertSet->sDirectiveSetAlertInput;
			strcpy(psDirectiveSetAlertSet->sDirectiveSetAlertInput.strNamespace, "Alerts");
			strcpy(psDirectiveSetAlertSet->sDirectiveSetAlertInput.strName, pSubSubName->valuestring);
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED;
			eAvsDirectiveID = eAVS_DIRECTIVE_SETALERT;
#if 0
//		int	RK_AlertDirective_Vacate_TimeSpace(S_SetAlertDirective* p_AlertDirectives, char* strScheduledTime, int iNumDirectives)
			psDirectiveSetAlert = RK_AlertDirective_Vacate_TimeSpace(psDirectiveSetAlertSet->asAlertDirective, p_scheduledTime->valuestring, TIMER_MAX);
			/* Find a valid instruction container */

			eAvsEvtID = eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED;
			eAvsDirectiveID = eAVS_DIRECTIVE_SETALERT;
			if(psDirectiveSetAlert != NULL){
				pData = (void *)psDirectiveSetAlert;
				memset(&psDirectiveSetAlertSet->sDirectiveSetAlertInput, 0, sizeof(S_SetAlertDirective));
			}else{
				pData = (void *)&psDirectiveSetAlertSet->sDirectiveSetAlertInput;
				psDirectiveSetAlertSet->sDirectiveSetAlertInput.eScheduledState = -1;
			}
#endif

		}else if(strcmp(pSubSubName->valuestring, "DeleteAlert") == 0){
			S_DeleteAlertDirective  *psDirectiveDeleteAlert = &pAvsDirectives->sDirectiveDeleteAlert;
			
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_DELETE_ALERT_SUCCEEDED;
			eAvsDirectiveID = eAVS_DIRECTIVE_DELETE_ALERT;
			
			pData = (void *)psDirectiveDeleteAlert;
			memset(psDirectiveDeleteAlert, 0, sizeof(S_DeleteAlertDirective));
			strcpy(psDirectiveDeleteAlert->strNamespace, "Alerts");
			strcpy(psDirectiveDeleteAlert->strName, pSubSubName->valuestring);

		}else if(strcmp(pSubSubName->valuestring, "Play") == 0){
			S_PlayDirectiveSet	*psDirectivePlaySet = &pAvsDirectives->sDirectivePlay;
			S_PlayDirective		*psDirectivePlay;

			uint32_t	iIdx, found = 0;
			
			pthread_mutex_lock(&psDirectivePlaySet->asPlayMutexLock);
			
			if(ui8Channel == 1 && psDirectivePlaySet->ui8DFlag == 0){	/* from speech request */
				memset(&psDirectivePlaySet->asDirectivePlay, 0, sizeof(S_PlayDirective)*2);
				psDirectivePlaySet->ui8DFlag = 1;
			}else if(ui8Channel == 0 && psDirectivePlaySet->ui8DFlag == 1){	/* from nearly finished  */
				pthread_mutex_unlock(&psDirectivePlaySet->asPlayMutexLock);
				return eAvsEvtID;	/* eAVS_DIRECTIVE_EVENT_UNKNOW */
			}
			
			/* Find a valid instruction container */
			for(iIdx=0; iIdx<2; iIdx++){
				ALEXA_TRACE("Playdirective index:%d, eMplayState:%d~%d\n", iIdx, psDirectivePlaySet->asDirectivePlay[0].eMplayState, psDirectivePlaySet->asDirectivePlay[1].eMplayState);
				if(psDirectivePlaySet->asDirectivePlay[iIdx].eMplayState == eMPLAY_STATE_IDLE){
					psDirectivePlay = &psDirectivePlaySet->asDirectivePlay[iIdx];
					found = 1;
					break;
				}
			}

			if(found == 0){
				ALEXA_WARNING("The play directive is fully!\n");
				pthread_mutex_unlock(&psDirectivePlaySet->asPlayMutexLock);
				break;
			}

			pData = (void *)psDirectivePlay;
			memset(psDirectivePlay, 0, sizeof(S_PlayDirective));
			memcpy(psDirectivePlay->strName, pSubSubName->valuestring, value_len);
			psDirectivePlay->strName[value_len] = '\0';
			
			/* bzero play buffer @20170210 fix - REPLACE_ENQUEUED ISSUE*/
			int	sub_size = 0;
			cJSON *pBehavior = cJSON_GetObjectItem(pSubPayload, "playBehavior");
			if(pBehavior == NULL){
				ALEXA_WARNING("No found playBehavior!\n");
			}else{
				sub_size = strlen(pBehavior->valuestring);
				memcpy(psDirectivePlay->strPlayBehavior, pBehavior->valuestring, sub_size);
				psDirectivePlay->strPlayBehavior[sub_size] = '\0';
				psDirectivePlay->eMplayState = eMPLAY_STATE_PREPARE;
				ALEXA_TRACE("behavior:%s\n", psDirectivePlay->strPlayBehavior);
			}
			/*must be stop current songs if playBehavior is REPLACE_ALL*/
			if(psDirectivePlaySet->asCurrentPlayStream.eMplayState == eMPLAY_STATE_PLAYING){
				if(strcmp(psDirectivePlay->strPlayBehavior, "REPLACE_ALL") == 0){
					RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED);
				}
			}
				
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED;
			eAvsDirectiveID = eAVS_DIRECTIVE_PLAY;
			
			pthread_mutex_unlock(&psDirectivePlaySet->asPlayMutexLock);
		}else if(strcmp(pSubSubName->valuestring, "Stop") == 0){
			S_StopDirective *pStopDirective = &pAvsDirectives->sDirectiveStop;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED;
			eAvsDirectiveID = eAVS_DIRECTIVE_STOP;

			pData = (void *)pStopDirective;
			memcpy(pStopDirective->strName, pSubSubName->valuestring, value_len);
			pStopDirective->strName[value_len] = '\0';
		}else if(strcmp(pSubSubName->valuestring, "ClearQueue") == 0){
			S_ClearQueueDirective *pClearQueueDirective = &pAvsDirectives->sDirectiveClearQueue;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_PLAYBACK_QUEUE_CLEARED;
			eAvsDirectiveID = eAVS_DIRECTIVE_CLEAR_QUEUE;

			pData = (void *)pClearQueueDirective;
			memcpy(pClearQueueDirective->strName, pSubSubName->valuestring, value_len);
			pClearQueueDirective->strName[value_len] = '\0';
		}else if(strcmp(pSubSubName->valuestring, "SetVolume") == 0){
			S_SetVolumeDirective *pSetVolumeDirective = &pAvsDirectives->sDirectiveSetVolume;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED;
			eAvsDirectiveID = eAVS_DIRECTIVE_SETVOLUME;

			pData = (void *)pSetVolumeDirective;
			memcpy(pSetVolumeDirective->strName, pSubSubName->valuestring, value_len);
			pSetVolumeDirective->strName[value_len] = '\0';
		}else if(strcmp(pSubSubName->valuestring, "AdjustVolume") == 0){
			S_AdjustVolumeDirective *pAdjustVolumeDirective = &pAvsDirectives->sDirectiveAdjustVolume;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_ADJUST_VOLUME_CHANGED;
			eAvsDirectiveID = eAVS_DIRECTIVE_ADJUSTVOLUME;

			pData = (void *)pAdjustVolumeDirective;
			memcpy(pAdjustVolumeDirective->strName, pSubSubName->valuestring, value_len);
			pAdjustVolumeDirective->strName[value_len] = '\0';
		}else if(strcmp(pSubSubName->valuestring, "SetMute") == 0){
			S_SetMuteDirective *pSetMuteDirective = &pAvsDirectives->sDirectiveSetMute;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_MUTE_CHANGED;
			eAvsDirectiveID = eAVS_DIRECTIVE_SETMUTE;

			pData = (void *)pSetMuteDirective;
			memcpy(pSetMuteDirective->strName, pSubSubName->valuestring, value_len);
			pSetMuteDirective->strName[value_len] = '\0';
		}else if(strcmp(pSubSubName->valuestring, "ResetUserInactivity") == 0){/*System interface*/
			S_ResetUserInactivityDirective *pRestUserDirective = &pAvsDirectives->sDirectiveRestUserInactivity;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_USER_INACTIVITY_REPORT;
			eAvsDirectiveID = eAVS_DIRECTIVE_RESTUSERINACTIVITY;

			pData = (void *)pRestUserDirective;
			memcpy(pRestUserDirective->strName, pSubSubName->valuestring, value_len);
			pRestUserDirective->strName[value_len] = '\0';
			/*long - longlong*/
			pRestUserDirective->i32InactiveSec = (long)utils_get_sec_time();
		}
		else if(strcmp(pSubSubName->valuestring, "SetEndpoint") == 0){/*System interface*/
			S_SetEndpointDirective *pSetEndpointDirective = &pAvsDirectives->sDirectiveSetEndpoint;
		
			eAvsEvtID = eAVS_DIRECTIVE_EVENT_SWITCH_ENDPOINT;
			eAvsDirectiveID = eAVS_DIRECTIVE_SETENDPOINT;

			pData = (void *)pSetEndpointDirective;
			memcpy(pSetEndpointDirective->strName, pSubSubName->valuestring, value_len);
			pSetEndpointDirective->strName[value_len] = '\0';
		}
		
		RK_AVSJsonParseDirective(pSubHeader, pSubPayload, pData, eAvsDirectiveID);

	}else{
		ALEXA_ERROR("No found name!\n");
	}
	
	}while(0);
	
	if(pJson != NULL)
		cJSON_Delete(pJson);

	return eAvsEvtID;
}
/**
 * Return a positive value or zero
 * handle SpeechSynthesizer data from AVS and judge whether the data from AVS is completed
 * 
 * @param psAlexaHndcb - a S_ALEXA_HND context
 * @param pData - from avs raw data
 * @param dataSize - lengths of raw data
 * return 0 - AVS data have residual, a positive value - Avs data already is completed(be sent finished)
 */
static int RK_AlexaRecognizeDataFromAvs(S_ALEXA_HND *psAlexaHndcb, char *pData, size_t dataSize)
{
	S_ALEXA_RES *pAlexaRes = psAlexaHndcb->psResData;
	char 		*pSrcData 		= (char *)pData;
	char		*pAudioHead		= NULL;		/*	MP3 head 'ID3' */
	int			links = 0;
	size_t 		unproc_data_size = 0;

	if(!psAlexaHndcb->isStream){
		int 	eAvsID;
		int 	found = 0;
		char 	*pToken = NULL,
				*pSavePtr	= NULL;

		links = RK_AVSKeepAlive(&pAlexaRes->asKeepAlive, pData, dataSize);
			
		if((pAudioHead = strstr(pSrcData, "ID3")) != NULL){		//"octet-stream"
			unproc_data_size =	dataSize - (pAudioHead-pSrcData);
		}

		pToken = strtok_r(pSrcData, "\r\n", &pSavePtr);
		while(pToken != NULL){
			if(pToken[0] == '{'){
				ALEXA_TRACE("pToken=\n*%s*\n", pToken);
				eAvsID = RK_AlexaDirectiveParseJson(pToken, NULL, 1);
				switch(eAvsID){
					/*we will send SpeechStarted event to avs before playback speek speech. */
					case eAVS_DIRECTIVE_EVENT_SPEECHSTARTED:
					case eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED:
					case eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED:
					case eAVS_DIRECTIVE_EVENT_ADJUST_VOLUME_CHANGED:
					case eAVS_DIRECTIVE_EVENT_MUTE_CHANGED:	
						RK_AVSSetDirectivesEventID(eAvsID);
						break;
					case eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED:
						/*we need to block until dialog speech playback is complete */
						RK_APBStream_open_block(false);	
						RK_BeginAudioPlayerMediaStream();
						break;
					case eAVS_DIRECTIVE_EVENT_EXPECTSPEECH:
						pAlexaRes->eAlexaResState |= eALEXA_EXPECTSPEECH;
						break;
				}
				
			}else if(strncmp(pToken, "ID3", 3) == 0){
				found = 1;
				break;
			}
			pToken = strtok_r(NULL, "\r\n", &pSavePtr);
		}

		if(found){
			S_PBContext *pPBCxt = pAlexaRes->psPlaybackCxt;
			S_AudioHnd  *pb_hnd = &pPBCxt->asAhandler[ePB_TYPE_DIALOG];

			psAlexaHndcb->isStream = 1;
			
#ifdef DEF_ENABLE_PB
			if(pPBCxt->asAPlayback){
				if(pb_hnd->ePlaybackState == ePB_STATE_IDLE){
					/* set encode format */
					pb_hnd->eAudioMask = eAUDIO_MASK_MP3;
					RK_APBOpen(pPBCxt, ePB_TYPE_DIALOG);	
				}
				/* set Playback handle type */
				pPBCxt->ePlaybackType = ePB_TYPE_DIALOG;
				RK_APBWrite(pPBCxt, ePB_TYPE_DIALOG, (unsigned char *)pToken, unproc_data_size);
			}
#endif
		}
	}else{
		S_PBContext *pPBCxt = pAlexaRes->psPlaybackCxt;
		
		links = RK_AVSKeepAlive(&pAlexaRes->asKeepAlive, pData, dataSize);

		unproc_data_size = dataSize;
#ifdef DEF_ENABLE_PB
		if(pPBCxt->asAPlayback){
			/* set Playback handle type */
			pPBCxt->ePlaybackType = ePB_TYPE_DIALOG;
			RK_APBWrite(pPBCxt, ePB_TYPE_DIALOG, (unsigned char *)pSrcData, unproc_data_size);
		}
#endif
	}
	
	return links;
}

/*
for CURLOPT_WRITEFUNCTION callback 
*/
static size_t RK_EventDataFromAvsCB
(
	char 	*pAvsData, 
	size_t 	size, 
	size_t 	nmemb, 
	void 	*stream_priv
)
{
	int written = 0, ret = 0;
	S_ALEXA_HND 	*psAlexaHndcb = (S_ALEXA_HND *)stream_priv;
	S_ALEXA_RES		*pAlexaRes = psAlexaHndcb->psResData;
	S_AVS_CURL_SET *pAvsCurlSet = &pAlexaRes->asAvsCurlSet;

	if(pAlexaRes->iReadWriteStamp == 0)
		pAlexaRes->iReadWriteStamp = 1;

    written = size*nmemb;
     
	if(written < 128){
		if(strstr(pAvsData, "x-amzn-requestid:") != NULL)
			printf("write speech data begin...Len:%d, written:%d, \n*%s*\n", size*nmemb, written, pAvsData);
	}else	
		printf("write speech data begin...Len:%d, written:%d\n", size*nmemb, written);
	ret = RK_AlexaRecognizeDataFromAvs(psAlexaHndcb, pAvsData, size*nmemb);
	
	//printf("BOUNDARY:%d\n", ret);
	
	/* update timestamp or disconnect */
	if(ret > 0)
		RK_AVSCurlRequestBreak(&pAvsCurlSet->asCurlSet[eCURL_HND_EVENT]);

	return written;
}

/* Debug statistics push data*/
int total_size = 0, interval = 0;
static size_t RK_EventDataToAvsCB
(
	char 	*pAvsData,
	size_t 	size,
	size_t 	nitems,
	void 	*stream_priv
)
{
	S_ALEXA_RES		*pAlexaRes = (S_ALEXA_RES *)stream_priv;
	int 			readSize = 0;

	if(pAlexaRes->iReadWriteStamp == 0)
		pAlexaRes->iReadWriteStamp = 1;
#if 1
	if(gs_bStopCapture){
		pAlexaRes->bEnableCapture = false;
		gs_bStopCapture = false;
		printf("begin last freadfunc ... reqLen:%d, cnt:%d, total:%d\n", size*nitems, readSize, total_size);

		return readSize;
	}
#endif

	if((pAlexaRes->m_pfnAvsReadFunCB)){
		pAlexaRes->m_pfnAvsReadFunCB(NULL, pAvsData, &readSize);
	}
	//for total data statistics	
	total_size +=readSize;
	if(interval%20 == 0)
	printf("begin freadfunc ... reqLen:%d, count:%d, total:%d\n", size*nitems, readSize, total_size);
	interval++;
	
	if(readSize == 0){
		printf("begin freadfunc ... reqLen:%d, count:%d, total:%d\n", size*nitems, readSize, total_size);
		pAlexaRes->bEnableCapture = false;
	}

	/* 15ms of capture audio chunk(320bytes) */
	usleep(5*1000);
	
	return readSize;
	
///EXIT_ABORT:
	
//	return CURL_READFUNC_ABORT;
}
#if 0
/*Temp function we need a better test for speech start--- remove*/
static size_t RK_DirectiveDataFromAvsCB(char *pAvsData, size_t size, size_t nmemb, void *stream_priv)
{
	int written = 0;
//	FILE *fp = (FILE *)stream_priv;
	
//	written = fwrite(pAvsData, size, nmemb, fp);
	written = size*nmemb;

	ALEXA_INFO("write directive data begin...Len:%d, written:%d\n", size*nmemb, written);

 //   fflush(fp);

    return written;
}
#endif
/*
 * Return a valid size of data from avs server.
 * revices token request data of response from avs server.
 * 
 * @param pAvsData - from avs token data that maybe is cjson format
 * @param size - size of each pAvsData
 * @param nmemb - lengths of each pAvsData
 * @param stream_priv - specified descriptor which file to restore
 * return written - pAvsData actual size
 *
 */
size_t RK_TokenDataFromAvsCB(char *pAvsData, size_t size, size_t nmemb, void *stream_priv)
{
	int written = 0;
	FILE *fp = (FILE *)stream_priv;
	
	written = fwrite(pAvsData, size, nmemb, fp);
	printf("write token data begin...Len:%d, written:%d\n", size*nmemb, written);

    fflush(fp);

    return written;
}
//add 20161107 merge
//end
static void RK_AlexaDownchannelDirectiveHandler(E_AVS_EVENT_ID esAvsEvtID, S_ALEXA_RES *pAlexaRes)
{
	switch(esAvsEvtID){
		case eAVS_DIRECTIVE_EVENT_STOP_CAPTURE:
		{
			gs_bStopCapture = true;
		}
		break;
		case eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED:
		case eAVS_DIRECTIVE_EVENT_SETALERT_FAILED:
		{
			S_SetAlertDirectiveSet *pSetAlerts = (S_SetAlertDirectiveSet *)&g_astrAvsDirectives.sDirectiveSetAlert;
			S_SetAlertDirective *pAlertInput = (S_SetAlertDirective *)&pSetAlerts->sDirectiveSetAlertInput;
			
			//int	RK_AlertDirective_Vacate_TimeSpace(S_SetAlertDirective* p_AlertDirectives, char* strScheduledTime, int iNumDirectives)
			S_SetAlertDirective *psDirectiveSetAlert = RK_AlertDirective_Vacate_TimeSpace(pSetAlerts->asAlertDirective, pAlertInput, TIMER_MAX);
			/* Find a valid instruction container */
			int iIndex=0;
		//	E_DELETE_ALERT_STATE;
			if(pAlertInput->bSetAlertSucceed == 2){//0 failed, 1succeeded, 2 succeed and have been in scheduled ------->>>>by yan
				if(psDirectiveSetAlert->eScheduledState == eSCH_STATE_ALERTING){//not be Alerting may be time comming
					if(pAlexaRes->psPlaybackCxt && (pAlexaRes->psPlaybackCxt->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE))
						RK_APBClose(pAlexaRes->psPlaybackCxt, ePB_TYPE_ALERT);
				}else{
					if(RK_Schedule_Get(pSetAlerts->asAvsSchTm + (psDirectiveSetAlert - pSetAlerts->asAlertDirective)) >= 0){//if timer/alarm is comming
						if(pAlexaRes->psPlaybackCxt){
							int tmp = 1000*5;
							while((pAlexaRes->psPlaybackCxt->asAhandler[ePB_TYPE_ALERT].ePlaybackState < ePB_STATE_START) && (tmp--)){
								usleep(1);
							}
							if(pAlexaRes->psPlaybackCxt && (pAlexaRes->psPlaybackCxt->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE))
								RK_APBClose(pAlexaRes->psPlaybackCxt, ePB_TYPE_ALERT);
						}
					}
				}
				iIndex = psDirectiveSetAlert - pSetAlerts->asAlertDirective;
				RK_Alert_Delete_Index(pSetAlerts, (iIndex), TIMER_MAX);
				psDirectiveSetAlert = RK_AlertDirective_Vacate_TimeSpace(pSetAlerts->asAlertDirective, pAlertInput, TIMER_MAX);
				*psDirectiveSetAlert = *pAlertInput;
				iIndex = psDirectiveSetAlert - pSetAlerts->asAlertDirective;
				memmove(pSetAlerts->asAvsSchTm + iIndex + 1, pSetAlerts->asAvsSchTm + iIndex, ( TIMER_MAX - iIndex -1 ) * sizeof(S_AVS_scheduledTime));//2016-12-14
				RK_Schedule_Set(&pSetAlerts->asAvsSchTm[iIndex], pSetAlerts->asAlertDirective[iIndex].strScheduledTime);
				psDirectiveSetAlert->eScheduledState = eSCH_STATE_WAIT_SUBMIT;
				RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED);
				break;
			}else if(pAlertInput->bSetAlertSucceed == 1){
				*psDirectiveSetAlert = *pAlertInput;
				psDirectiveSetAlert->eScheduledState = eSCH_STATE_WAIT_SUBMIT;
				iIndex = psDirectiveSetAlert - pSetAlerts->asAlertDirective;
				memmove(pSetAlerts->asAvsSchTm + iIndex + 1, pSetAlerts->asAvsSchTm + iIndex, ( TIMER_MAX - iIndex -1 ) * sizeof(S_AVS_scheduledTime));
				if(RK_Schedule_Set(&pSetAlerts->asAvsSchTm[iIndex], pSetAlerts->asAlertDirective[iIndex].strScheduledTime) < 0){
					pAlertInput->bSetAlertSucceed = 0;//set fail
					RK_Alert_Delete_Index(pSetAlerts, iIndex, TIMER_MAX);
					RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SETALERT_FAILED);
				}else{
//					pSetAlerts->asAlertDirective[iIndex].eScheduledState = eSCH_STATE_WAIT_SUBMIT;
					RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED);
				}
			}else{
				RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_SETALERT_FAILED);
			}
			if(!iIndex && RK_SysWrState)
				RK_SysWrState(eSYSSTATE_ALERTTIME, pSetAlerts->asAlertDirective[0].strScheduledTime);
		}
		break;
		case eAVS_DIRECTIVE_EVENT_DELETE_ALERT_SUCCEEDED:
		case eAVS_DIRECTIVE_EVENT_DELETE_ALERT_FAILED:
		{
			S_DeleteAlertDirective *pDeleteAlert = (S_DeleteAlertDirective *)&g_astrAvsDirectives.sDirectiveDeleteAlert;
			S_SetAlertDirectiveSet *pSetAlerts = (S_SetAlertDirectiveSet *)&g_astrAvsDirectives.sDirectiveSetAlert;
			int iIndex;
			//E_AVS_EVENT_ID EventID;
			for(iIndex=0; iIndex<TIMER_MAX; iIndex++){
				if(!strcmp(pSetAlerts->asAlertDirective[iIndex].strToken, pDeleteAlert->strToken)){//search for alerter
					break;
				}
			}
			pDeleteAlert->cDeleteSucceeded = eDELALERT_FAILED;
			switch(iIndex){
				case -1:
				case TIMER_MAX://in this case, don't find any Alert to delete, so delete failed.
				{
					pDeleteAlert->cDeleteSucceeded = eDELALERT_FAILED;
					printf("---->>>>Not find the Alert :\n%s---->>>>by yan\n", pDeleteAlert->strToken);
				}
				break;
				case 0:
				{
					if(RK_Schedule_Get(pSetAlerts->asAvsSchTm)>=0){
						if(pSetAlerts->asAlertDirective[0].eScheduledState == eSCH_STATE_ALERTING){
							RK_Alert_Delete_Alerting(pAlexaRes->psPlaybackCxt, pSetAlerts, pDeleteAlert);//will 
						}else{
							int timeout = 1000*5;
							while((pSetAlerts->asAlertDirective[0].eScheduledState != eSCH_STATE_ALERTING)&& timeout--){
								usleep(1);
							}
							if(pSetAlerts->asAlertDirective[0].eScheduledState == eSCH_STATE_ALERTING){
								RK_Alert_Delete_Alerting(pAlexaRes->psPlaybackCxt, pSetAlerts, pDeleteAlert);
							}else{
								pDeleteAlert->cDeleteSucceeded = 0 + eDELALERT_SUCCEEDED;
							}
						}
						RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STOP);
					}else{// time not reach
						RK_Alert_Delete_Index(pSetAlerts, 0, TIMER_MAX);
						pDeleteAlert->cDeleteSucceeded = 0 + eDELALERT_SUCCEEDED;
						if(RK_SysWrState){
							RK_SysWrState(eSYSSTATE_ALERTALL_NUMADD, -1);
							RK_SysWrState(eSYSSTATE_ALERTTIME, pSetAlerts->asAlertDirective[0].strScheduledTime);
						}
						RK_Alert_Update_ConfigFile();
					}
				}
				break;
				default://in this case, found the Alert to delete
					switch((int)(RK_Schedule_Get(pSetAlerts->asAvsSchTm) >= 0)){
						case 1:
						{
							pDeleteAlert->cDeleteSucceeded = iIndex + eDELALERT_SUCCEEDED;
							RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STOP);
						}
						break;
						default://o????1???|?
							RK_Alert_Delete_Index(pSetAlerts, iIndex, TIMER_MAX);
							pDeleteAlert->cDeleteSucceeded = iIndex + eDELALERT_SUCCEEDED;
							if(RK_SysWrState)
								RK_SysWrState(eSYSSTATE_ALERTALL_NUMADD, -1);
							RK_Alert_Update_ConfigFile();
					}
			}
			RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_DELETE_ALERT_SUCCEEDED);
		}
		break;
		//add System and app control interface @2017-0215
		case eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED:
			RK_BeginAudioPlayerMediaStream();
		break;
		case eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED:
		case eAVS_DIRECTIVE_EVENT_PLAYBACK_QUEUE_CLEARED:
		case eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED:
//		case eAVS_DIRECTIVE_EVENT_USER_INACTIVITY_REPORT:
		case eAVS_DIRECTIVE_EVENT_EXCEPTIONENCOUNTERED:
		case eAVS_DIRECTIVE_EVENT_SWITCH_ENDPOINT:	
		{
			RK_AVSSetDirectivesEventID(esAvsEvtID);
		}
		break;
		
		default:
			ALEXA_WARNING("no found DownchannelEvent!\n");
		
	}

}

size_t RK_DownchannelDataFromAvsCB(char *pAvsData, size_t size, size_t nitems, void *instream)
{
	S_ALEXA_RES *psAlexaRes = (S_ALEXA_RES *)instream;
	E_AVS_EVENT_ID eAvsEvtID;
	int written = 0;
	
	char 	*pToken 	= NULL,
			*pSavePtr	= NULL;
	
	written = size*nitems;
	ALEXA_DBG("Downchannel data come in: size - %d \n*%s*\n", written, pAvsData);

	pToken = strtok_r(pAvsData, "\r\n", &pSavePtr);
	while(pToken != NULL){
		if(pToken[0] == '{'){
			//printf("downdchannel pToken=\n*%s*\n", pToken);
			eAvsEvtID = RK_AlexaDirectiveParseJson(pToken, NULL, 0);
			RK_AlexaDownchannelDirectiveHandler(eAvsEvtID, psAlexaRes);
		}
		pToken = strtok_r(NULL, "\r\n", &pSavePtr);
	}
	
	return written;
}

#if 0
/* PLAYBACK_STARTED - temp --- remove*/
size_t RK_MediaStreamStartedCB(char *pAvsData, size_t size, size_t nmemb, void *stream_priv)
{
	int written = size*nmemb;
//	S_ALEXA_RES *pAlexaRes = (S_ALEXA_RES *)stream_priv;
	
//	RK_MediaStreamNearlyParse(pAlexaRes, pAvsData, size*nmemb);
    fwrite(pAvsData, 1, written, directfp1);
	printf("revice len:%d\n", written);

    return written;
}
#endif
static void RK_MediaStreamNearlyParse(S_ALEXA_RES *pAlexaRes, void *pData, size_t dataSize)
{
	char 	*pSrcData 		= (char *)pData;
	char 	*pAudioHead 	= NULL;	/*	MP3 head 'ID3' */
		
	E_AVS_EVENT_ID	eAvsID = eAVS_DIRECTIVE_EVENT_UNKNOW;
	int 	found = 0;
	char	*pToken = NULL,
			*pSavePtr	= NULL;
	
	size_t 	unproc_data_size = 0;

	if(!pAlexaRes->isPromptSpeak){

		if((pAudioHead = strstr(pSrcData, "ID3")) != NULL){		//"octet-stream"
			/* set state of prompt */
			pAlexaRes->eAlexaResState |= eALEXA_MEDIA_PROMPT;
			unproc_data_size =	dataSize - (pAudioHead-pSrcData);
		}

		pToken = strtok_r(pSrcData, "\r\n", &pSavePtr);
		while(pToken != NULL){
			if(pToken[0] == '{'){
				ALEXA_TRACE("Nearly-pToken=\n*%s*\n", pToken);
				eAvsID = RK_AlexaDirectiveParseJson(pToken, NULL, 0);
				switch(eAvsID){
				case eAVS_DIRECTIVE_EVENT_SPEECHSTARTED:
				case eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED:
			//	case eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED:
			//	case eAVS_DIRECTIVE_EVENT_ADJUST_VOLUME_CHANGED:	
				case eAVS_DIRECTIVE_EVENT_MUTE_CHANGED:
					RK_AVSSetDirectivesEventID(eAvsID);
					break;
				case eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED:
					/*we need to block until dialog speech playback is complete */
					//RK_APBStream_open_block(false);	
					ALEXA_DBG("the next song will begin to play.\n");
					RK_BeginAudioPlayerMediaStream();
					break;
				case eAVS_DIRECTIVE_EVENT_EXPECTSPEECH:
					ALEXA_DBG("Expect speech capture...\n");
					speech_finish_state = 1;
					break;
				default:
					ALEXA_DBG("No support directive...\n");
					break;
				}
			}else if(strncmp(pToken, "ID3", 3) == 0){
				found = 1;
				break;
			}
			pToken = strtok_r(NULL, "\r\n", &pSavePtr);
		}

		if(found){
			S_PBContext *pPBCxt = pAlexaRes->psPlaybackCxt;
			S_AudioHnd  *pPBHnd = &pPBCxt->asAhandler[ePB_TYPE_DIALOG];

			pAlexaRes->isPromptSpeak = 1;
			
			if(pPBCxt->asAPlayback){
				if(pPBHnd->ePlaybackState == ePB_STATE_IDLE){
					//ALEXA_INFO("====>media promt\n");
					pPBHnd->eAudioMask = eAUDIO_MASK_MP3;
					RK_APBOpen(pPBCxt, ePB_TYPE_DIALOG);	
				}
				
				pPBCxt->ePlaybackType = ePB_TYPE_DIALOG;
				//printf("%d, actual:%d\n", __LINE__, unproc_data_size);
				RK_APBWrite(pPBCxt, ePB_TYPE_DIALOG, (unsigned char *)pToken, unproc_data_size);
			}
		}
	}else{
		S_PBContext *pPBCxt = pAlexaRes->psPlaybackCxt;
	#if 0
		if((pAudioHead = strstr(pSrcData, "ID3")) != NULL){	
			ALEXA_INFO("->>>>sencond promt\n");
		}
	#endif
		unproc_data_size = dataSize;
		printf("%d, actual:%d\n", __LINE__, unproc_data_size);
		if(pPBCxt->asAPlayback){
			pPBCxt->ePlaybackType = ePB_TYPE_DIALOG;
			RK_APBWrite(pPBCxt, ePB_TYPE_DIALOG, (unsigned char *)pSrcData, unproc_data_size);
		}
	}

}

/* PLAYBACK_NEARLY_FINISHED */
size_t RK_MediaStreamNearlyFinishedCB(char *pAvsData, size_t size, size_t nmemb, void *stream_priv)
{
	int written = size*nmemb;
	S_ALEXA_RES *pAlexaRes = (S_ALEXA_RES *)stream_priv;
	
    //written = fwrite(pAvsData, size, nmemb, directfp1);  
	RK_MediaStreamNearlyParse(pAlexaRes, pAvsData, size*nmemb);

	ALEXA_DBG("=>AudioPlayer callback received data - length:%d, written:%d\n", size*nmemb, written);
	
    return written;
}

/********************************************************************************
Description: appends a specified string to a linked list of strings for httpheader; 
		note - Each header must be separated by '&'
pstrHeaderData: specified string
return value: RK_CURL_SLIST - success, NULL - failed, RK_CURL_SLIST use complete 
			  must be destroyed by curl_slist_free_all function.
********************************************************************************/
RK_CURL_SLIST *RK_CurlHttpHeaderPackage(char *pstrHeaderData)
{
	return RK_AVSCurlHttpHeaderPackage(pstrHeaderData);
}

/*
* all event header
*/
int RK_GetEventHeader(char *pReqHeader, char *accessToken)
{
	int 	iReqHeaderLen;
	char 	strEventHearder[] 	= "Path: /v20160207/events&Host: access-alexa-na.amazon.com&Authorization:Bearer %s&Content-Type: multipart/form-data&Transfer-Encoding: chunked";
	//char 	pRequest[1024] 	= {0};

	if(accessToken == NULL){
		ALEXA_ERROR("AccessToken is failure!\n");
		return -1;
	}
	iReqHeaderLen = sprintf(pReqHeader, strEventHearder, accessToken);
	ALEXA_INFO("AVS event header length:%d\n", iReqHeaderLen);

	return iReqHeaderLen;
}
void RK_GetEventHeaderList(RK_CURL_SLIST **pHeaderSlist, char *pAToken)
{
	char strEventHeader[1024] = {0};
	int iEventHeaderLen = RK_GetEventHeader(strEventHeader, pAToken);
	
	if(iEventHeaderLen < 0){
		ALEXA_ERROR("Get header of request event part is failure!\n");
		return ;
	}
	RK_CURL_SLIST * freeheaderlist = *pHeaderSlist;
	*pHeaderSlist = RK_CurlHttpHeaderPackage(strEventHeader);
	if(freeheaderlist){
		curl_slist_free_all(freeheaderlist);
	}
}

/* set Recognize Multipart*/
static void RK_SetSynchronizeEvent(char **strJSONout, E_TYPE_EVENT evtType, int enable_stop)
{
	S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
	S_PlayDirective 	*pCurrentPlayDirective = &pPlayDirectives->asCurrentPlayStream;
	S_SetAlertDirective * asAlertDirective = g_astrAvsDirectives.sDirectiveSetAlert.asAlertDirective;
	long currentOffset_ms = 0;
	
	const char *pPlayState[] = {
			"IDLE",
			"PLAYING",
			"STOPPED",
			"PAUSED",
			"BUFFER_UNDERRUN",
			"FINISHED",
			NULL
		};
	
	if(*strJSONout != NULL)
		free(*strJSONout);
	
	uint8_t iIdx = 0;
	switch(pCurrentPlayDirective->eMplayState){
		case eMPLAY_STATE_IDLE:
			iIdx = 0;
			break;
		case eMPLAY_STATE_PLAYING:
			iIdx = 1;
			pCurrentPlayDirective->ui64CurrentOffsetInMilliseconds = \
						(time(NULL) - pCurrentPlayDirective->ui64Startoffset) * 1000;
			pCurrentPlayDirective->ui64CurrentOffsetInMilliseconds += \
						pCurrentPlayDirective->offsetInMilliseconds;
			currentOffset_ms = pCurrentPlayDirective->ui64CurrentOffsetInMilliseconds;
			break;
		case eMPLAY_STATE_STOPPED:
			iIdx = 2;
			currentOffset_ms = pCurrentPlayDirective->ui64CurrentOffsetInMilliseconds;
			break;
		case eMPLAY_STATE_PAUSE:
			iIdx = 3;
			currentOffset_ms = pCurrentPlayDirective->ui64CurrentOffsetInMilliseconds;
			break;
		case eMPLAY_STATE_FINISHED:
			iIdx = 5;
		break;
		default:
			ALEXA_WARNING("PlaybackState no found - %d\n", pCurrentPlayDirective->eMplayState);
	
	}

	ALEXA_INFO("AudioPlayer state - %s - %ld\n", pPlayState[iIdx], currentOffset_ms);
	
	cJSON 	*root, *context; 
	cJSON	*headerCxt, *payloadCxt, *nodeCxt;
	
	root = cJSON_CreateObject();
	/**************************** Context ****************************/
	cJSON_AddItemToObject(root, "context", context = cJSON_CreateArray() );
///////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------------PlaybackState---------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////////
	cJSON_AddItemToArray(context, nodeCxt = cJSON_CreateObject());
	cJSON_AddItemToObject(nodeCxt, "header", headerCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(headerCxt, "namespace", "AudioPlayer");
	cJSON_AddStringToObject(headerCxt, "name", "PlaybackState");
	
	cJSON_AddItemToObject(nodeCxt, "payload", payloadCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(payloadCxt, "token", pCurrentPlayDirective->strToken);
	cJSON_AddNumberToObject(payloadCxt, "offsetInMilliseconds", currentOffset_ms);
	cJSON_AddStringToObject(payloadCxt, "playerActivity", pPlayState[iIdx]);
///////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------------AlertState------------------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////////
	cJSON *allAlerts = NULL, *activeAlerts = NULL, *AlertsNodeCxt = NULL,
			*AlertsHeaderCxt = NULL, *AlertsPayloadCxt = NULL, *AlertNode = NULL;
	cJSON_AddItemToArray(context, AlertsNodeCxt = cJSON_CreateObject());
	cJSON_AddItemToObject(AlertsNodeCxt, "header", AlertsHeaderCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(AlertsHeaderCxt, "namespace", "Alerts");
	cJSON_AddStringToObject(AlertsHeaderCxt, "name", "AlertsState");
	cJSON_AddItemToObject(AlertsNodeCxt, "payload", AlertsPayloadCxt = cJSON_CreateObject());
	cJSON_AddItemToObject(AlertsPayloadCxt, "allAlerts", allAlerts = cJSON_CreateArray());
	cJSON_AddItemToObject(AlertsPayloadCxt, "activeAlerts", activeAlerts = cJSON_CreateArray());
	
	for(iIdx = 0; iIdx < TIMER_MAX; iIdx++){
		int schedule_state = asAlertDirective[iIdx].eScheduledState;
		
		switch(schedule_state){
			case eSCH_STATE_IDLE:
			case eSCH_STATE_STOP:
				iIdx = TIMER_MAX;
				break;
			default: 
			{
				cJSON_AddItemToArray(allAlerts, AlertNode = cJSON_CreateObject());
				cJSON_AddStringToObject(AlertNode, "token", asAlertDirective[iIdx].strToken);
				cJSON_AddStringToObject(AlertNode, "type", asAlertDirective[iIdx].strType);
				cJSON_AddStringToObject(AlertNode, "scheduledTime", asAlertDirective[iIdx].strScheduledTime);
				switch(schedule_state){
					case eSCH_STATE_ALERT_REACH:
					case eSCH_STATE_ALERT_REACHED:
					case eSCH_STATE_ALERTING:
					{
						cJSON_AddItemToArray(activeAlerts, AlertNode = cJSON_CreateObject());
						cJSON_AddStringToObject(AlertNode, "token", asAlertDirective[iIdx].strToken);
						cJSON_AddStringToObject(AlertNode, "type", asAlertDirective[iIdx].strType);
						cJSON_AddStringToObject(AlertNode, "scheduledTime", asAlertDirective[iIdx].strScheduledTime);
					}
				}
			}
		}
	}
	
	/************************ SpeechState ****************************/
	cJSON_AddItemToArray(context, nodeCxt = cJSON_CreateObject());
	cJSON_AddItemToObject(nodeCxt, "header", headerCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(headerCxt, "namespace", "SpeechSynthesizer");
	cJSON_AddStringToObject(headerCxt, "name", "SpeechState");
	
	cJSON_AddItemToObject(nodeCxt, "payload", payloadCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(payloadCxt, "token", g_astrAvsDirectives.sDirectiveSpeak.strToken);	//pDirective->strToken
	cJSON_AddNumberToObject(payloadCxt, "offsetInMilliseconds", 0);
	cJSON_AddStringToObject(payloadCxt, "playerActivity", "FINISHED");

	/************************* VolumeState **************************/
	cJSON_AddItemToArray(context, nodeCxt = cJSON_CreateObject());
	cJSON_AddItemToObject(nodeCxt, "header", headerCxt = cJSON_CreateObject());
	cJSON_AddStringToObject(headerCxt, "namespace", "Speaker");
	cJSON_AddStringToObject(headerCxt, "name", "VolumeState");

	cJSON_AddItemToObject(nodeCxt, "payload", payloadCxt = cJSON_CreateObject());
	
	cJSON_AddNumberToObject(payloadCxt, "volume", g_astrAvsDirectives.sDirectiveSetVolume.i32Volume);	//?????g_astrAvsDirectives.sDirectiveSetVolume.i32Volume
	cJSON_AddBoolToObject(payloadCxt, "muted", g_astrAvsDirectives.sDirectiveSetMute.bMute);

	/**************************** Event ****************************/
	cJSON  *event, *header, *payload;
	
	cJSON_AddItemToObject(root, "event", event = cJSON_CreateObject());
	/*************************** Recognize **************************/
	if(evtType == eTYPE_RECONGINZE){
	#if 0	
         char sDialogReqID[16]="dialogId-";//9+1
         int a=65, b=90, i;	/*A-Z*/
         srand((unsigned)time(NULL));
         for(i=0; i<5; i++){
             sDialogReqID[9+i] = rand()%(b-a+1)+a;
        }
        sDialogReqID[14]='\0';
	#endif	
	char sMssageReqID[37]; 
	char sDialogReqID[37];
	
	RK_Random_uuid(sMssageReqID);
	RK_Random_uuid(sDialogReqID);

	//printf("messageID:%s\n", sMssageReqID);
	cJSON_AddItemToObject(event, "header", header = cJSON_CreateObject());
	cJSON_AddStringToObject(header, "namespace", "SpeechRecognizer");
	cJSON_AddStringToObject(header, "name", "Recognize");
	cJSON_AddStringToObject(header, "messageId", sMssageReqID );//"messageId-123"
	cJSON_AddStringToObject(header, "dialogRequestId", sDialogReqID );//"dialogRequestId-123"
	cJSON_AddItemToObject(event, "payload", payload = cJSON_CreateObject());
	if(enable_stop)
		cJSON_AddStringToObject(payload, "profile", "NEAR_FIELD");	//CLOSE_TALK FAR_FIELD NEAR_FIELD
	else
		cJSON_AddStringToObject(payload, "profile", "CLOSE_TALK");
	cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");
	
	}else if(evtType == eTYPE_SYNCHRONIZESTATE){
	/**************************** SynchronizeState Events ****************************/
		char sMssageReqID[37]; 
					
		RK_Random_uuid(sMssageReqID);
		cJSON_AddItemToObject(event, "header", header = cJSON_CreateObject());
		cJSON_AddStringToObject(header, "namespace", "System");
		cJSON_AddStringToObject(header, "name", "SynchronizeState");
		cJSON_AddStringToObject(header, "messageId", sMssageReqID );//"messageId-123"
		cJSON_AddItemToObject(event, "payload", payload = cJSON_CreateObject());

	}else if(evtType == eTYPE_EXCEPTIONENCOUNTERED){
	/**************************** ExceptionEncountered Events ****************************/
		cJSON *errornode = NULL;
		char sMssageReqID[37]; 
					
		RK_Random_uuid(sMssageReqID);
		
		cJSON_AddItemToObject(event, "header", header = cJSON_CreateObject());
		cJSON_AddStringToObject(header, "namespace", "System");
		cJSON_AddStringToObject(header, "name", "ExceptionEncountered");
		cJSON_AddStringToObject(header, "messageId", sMssageReqID );		//"messageId-123"
		cJSON_AddItemToObject(event, "payload", payload = cJSON_CreateObject());
		cJSON_AddStringToObject(payload, "unparsedDirective", "directives_temp");	/*mark seven*/
		cJSON_AddItemToObject(payload, "error", errornode = cJSON_CreateObject());
		cJSON_AddStringToObject(errornode, "type", "UNSUPPORTED_OPERATION");
		cJSON_AddStringToObject(errornode, "message", "the device isnot supported directives");
	}
	
	//===========Tail===============//
	*strJSONout = cJSON_Print(root);	
	cJSON_Delete(root);
	
	if(g_astrAvsDirectives.sDirectiveExpectSpeech.strMessageId[0] != 0){
		memset(g_astrAvsDirectives.sDirectiveExpectSpeech.strMessageId, 0, 64);
	}

#if 1
	ALEXA_TRACE("%s\n %d\n", *strJSONout, strlen(*strJSONout));
#endif	
}

/* set SynchronizeState Multipart*/
void RK_SetSynchronizeStateEvent(char **strJSONout, int enable_stop)
{
	RK_SetSynchronizeEvent(strJSONout, eTYPE_SYNCHRONIZESTATE, enable_stop);
}

/* set Recognize Multipart*/
void RK_SetRecognizeEvent(char **strJSONout, int enable_stop)
{
	while(speech_finish_state != 0) usleep(1000*100);

	RK_SetSynchronizeEvent(strJSONout, eTYPE_RECONGINZE, enable_stop);
}

/* set Recognize Multipart*/
void RK_SetExceptionEncounteredEvent(char **strJSONout, int enable_stop)
{
	RK_SetSynchronizeEvent(strJSONout, eTYPE_EXCEPTIONENCOUNTERED, enable_stop);
}

/* prepare recognize Event post data */
RK_CURL_HTTPPOST *RK_SetRecognizePostData(char *strJSONout, void *pPrivData, unsigned int i32Length)
{
	struct curl_httppost 	*postFirst 	= NULL,	*postLast	= NULL;

	if(strJSONout == NULL)
		return NULL;
				//=============JSON==================//
	curl_formadd(&postFirst, &postLast, \
				CURLFORM_COPYNAME, "metadata", /* CURLFORM_PTRCONTENTS, pAlexaJSON,  */ \
				CURLFORM_COPYCONTENTS, strJSONout, \
				CURLFORM_CONTENTTYPE, "application/json; charset=UTF-8", \
				CURLFORM_END);
							
				//=============Audio=================//
	curl_formadd(&postFirst, &postLast, \
				CURLFORM_COPYNAME, "audio", \
				CURLFORM_STREAM, pPrivData, \
				CURLFORM_CONTENTSLENGTH, i32Length, \
				CURLFORM_CONTENTTYPE, "application/octet-stream", \
				CURLFORM_END);

	return postFirst;
}

/* remove avs*/
void RK_SetEasyOptionParams
(
	CURL 				*curlhnd,
	RK_CURL_SLIST 	   *httpHeader, 
	RK_CURL_HTTPPOST   *postData, 
	void 				*pPrivData,
	bool				bEnable
)
{
	curl_easy_setopt(curlhnd, CURLOPT_HTTPHEADER, httpHeader);
	curl_easy_setopt(curlhnd, CURLOPT_HTTPPOST, postData);
	curl_easy_setopt(curlhnd, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curlhnd, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(curlhnd, CURLOPT_NOSIGNAL, 1L);
	//only recongzied event to use it
	if(bEnable){
		curl_easy_setopt(curlhnd, CURLOPT_TIMEOUT, 40L);
		RK_AVSSetCurlReadAndWrite(curlhnd, RK_EventDataToAvsCB, NULL, \
								RK_EventDataFromAvsCB, pPrivData, \
								RK_EventDataFromAvsCB, pPrivData);

	}
}

void RK_StartRecognizeSendRequest
(
	S_ALEXA_HND *psAlexaHnd
)
{
	S_AVS_CURL_SET *pAvsCurlSet = &psAlexaHnd->psResData->asAvsCurlSet;
	S_KEEPALIVE 	*pKeepAlive = &psAlexaHnd->psResData->asKeepAlive;
	
	//Debug params
	total_size = 0;
	interval = 0;
	/*init capture termination*/
	gs_bStopCapture = false;
		
	if(pKeepAlive->boundary != NULL){
		free(pKeepAlive->boundary);
		pKeepAlive->boundary = NULL;
	}

	
	RK_AVSCurlRequestSubmit(&pAvsCurlSet->asCurlSet[eCURL_HND_EVENT], 1);

}

void RK_AlexaPriorityChange(bool bForeground)
{
	E_AVS_EVENT_ID	eAvsEvtID = eAVS_DIRECTIVE_EVENT_UNKNOW;

	if(bForeground)
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_FOREGROUND;
	else
		eAvsEvtID = eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_BACKGROUND;

	RK_AVSSetDirectivesEventID(eAvsEvtID);
}

void RK_AlexaPriorityForeground(void)
{
	RK_AlexaPriorityChange(true);
}

void RK_AlexaPriorityBackground(void)
{
	RK_AlexaPriorityChange(false);
}

/* begin trigger media stream*/
int RK_BeginAudioPlayerMediaStream(void)
{
	S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;

	pthread_mutex_lock(&pPlayDirectives->asPlayMutexLock);

	sem_post(&pPlayDirectives->asPlaySem);
	pthread_mutex_unlock(&pPlayDirectives->asPlayMutexLock);

	return 0;
}
unsigned short RK_AuidoPlayerReportIntervalMS(S_PlayDirective *pDirective, unsigned short IntervalSec)
{
	if(pDirective->ui64ReportIntervalMS){
		IntervalSec++;
		if(IntervalSec >= pDirective->ui64ReportIntervalMS/1000){
			IntervalSec = 0;
			RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ProgressReportIntervalElapsed);
		}
	}else
		IntervalSec = 0;

	return IntervalSec;
}

void *RK_AlexaAudioPlayerHandle(void *arg)
{
	S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
	S_ALEXA_RES *psAlexaRes = (S_ALEXA_RES *)arg;
	uint8_t iIdx;

	do{
		int found = -1;
		pthread_t	threadMediaID;
		S_PlayDirective		*pDirective = NULL;
		
		sem_wait(&pPlayDirectives->asPlaySem);
		ALEXA_DBG("AudioPlayer Start...\n");

		pthread_mutex_lock(&pPlayDirectives->asPlayMutexLock);

		for(iIdx=0; iIdx<2; iIdx++){
			if(pPlayDirectives->asDirectivePlay[iIdx].eMplayState == eMPLAY_STATE_PREPARE){
				pPlayDirectives->asDirectivePlay[iIdx].eMplayState = eMPLAY_STATE_IDLE;
				pDirective = &pPlayDirectives->asDirectivePlay[iIdx];
				found = RK_AudioPlayerRequestMedia(psAlexaRes, pDirective);
				break;
			}
		}
		/*Don't move the location and must be clear zero*/
		pPlayDirectives->ui8DFlag = 0;	
		
		if(found < 0)
			goto LOOP;
		
		if(found == 1){ /* replace_enque*/
			memcpy(&pPlayDirectives->asReplacePlayStream, &pPlayDirectives->asDirectivePlay[iIdx], sizeof(S_PlayDirective));
			psAlexaRes->replace_behavior = 1;
			goto LOOP;
		}

		pthread_mutex_unlock(&pPlayDirectives->asPlayMutexLock);
		/* we have to wait until before the end of the media stream*/
		while(pPlayDirectives->asCurrentPlayStream.eMplayState == eMPLAY_STATE_PLAYING){
			usleep(10*1000);
		}
		
		/* restore current play directives */
		memcpy(&pPlayDirectives->asCurrentPlayStream, &pPlayDirectives->asDirectivePlay[iIdx], sizeof(S_PlayDirective));
		psAlexaRes->sAvsDirective = &pPlayDirectives->asCurrentPlayStream;
		pPlayDirectives->asCurrentPlayStream.eMplayState = eMPLAY_STATE_PLAYING;				

		/*create a Mediastream pthread*/
		pthread_create(&threadMediaID, 0, RK_MediaStreamThread, (void *)psAlexaRes);
		pthread_detach(threadMediaID);
		/* make sure current strem already begin to playing, so we start to send 'started event' */
		if(RK_FFPlayer_PlaybackState() == 1){
			RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED);
		}
		
LOOP:	
		if(found < 0 || found == 1)
			pthread_mutex_unlock(&pPlayDirectives->asPlayMutexLock);
		
		ALEXA_DBG("AudioPlayer End...\n");
	}while(1);
	
}

void *RK_AlexaTimerThread(void *arg)
{
	S_ALEXA_RES *psAlexaRes = (S_ALEXA_RES *)arg;
	S_PBContext *pPBCxt = psAlexaRes->psPlaybackCxt;
	S_AudioHnd 	*pPBHnd = &pPBCxt->asAhandler[ePB_TYPE_ALERT];	//pPBHnd
	uint64_t	ui64AliveSec = 0;	//temp state param - test debug

	struct timeval tv;
	time_t t_3hours = utils_get_sec_time();
	time_t t_NowTime;
	
	int isFirst = 1;
	int iIDELTime[TIMER_MAX] = {0};
	//int iAlertLen = -1;
	uint16_t ReportInterval_tm = 0;
	
	if(pPBCxt == NULL){
		ALEXA_ERROR("Playback context is NULL\n");
		return NULL;
	}

	do{
		tv.tv_sec = 1;  // set timer 1s
		tv.tv_usec = 0;	 

		switch(select(0, NULL, NULL, NULL, &tv))	 
		{  
		case -1:	// error occurred
			printf("Error!\n");  
		break;
		
		case 0: //timeout
		{
			S_SetAlertDirectiveSet	*pDirectiveSetAlert = &g_astrAvsDirectives.sDirectiveSetAlert;		
			int iIdx, i32ScheduledState = eSCH_STATE_IDLE;
			for(iIdx=0; iIdx<TIMER_MAX; iIdx++){
				//INFO("---->>>>Clock FOR ---->>>>by yan");
				if(pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState == eSCH_STATE_IDLE){
					break;
				}
				if(pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState == eSCH_STATE_WAIT_SUBMIT){
					ALEXA_INFO("Alert numbers - %d continue\n", (iIdx));
					continue;
				}
				
				if(pPBHnd->ePlaybackState !=  ePB_STATE_IDLE){
					if(!RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_ALERT, 10)){
						pDirectiveSetAlert->asAlertDirective[0].eScheduledState = eSCH_STATE_STOP;
						iIDELTime[0] = alertBaseRes.iStopNextTime + 1;
						ALEXA_DBG("---->>>>player stop one alert.------->>>>by yan\n");
					}
				}

				i32ScheduledState = pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState;
				
				int iTimeOut = -1;
				iTimeOut = RK_Schedule_Get(pDirectiveSetAlert->asAvsSchTm + iIdx);
				ALEXA_INFO("---->>>>iTimeOut = %d-%d --->>>>by yan\n", iTimeOut, isFirst);
				if( iTimeOut >= 0 ){
				switch(isFirst){
					case 1:
						/* If the alerts were scheduled to go off in the last 30 minutes, 
						 * device should trigger the alerts and send all corresponding events.
						 * If more than 30 minutes have elapsed from an alerts scheduled delivery,
						 * it should be discarded and device should send Alexa an AlertStopped
						 * event for each discarded alert.
						*/
						if(iTimeOut >= alertBaseRes.iAlertTimeOut){/* more than 30 minutes */
							pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState = eSCH_STATE_STOP;
							RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STOP);
							ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STOP ---->>>>by yan\n");
						}else{/*in the last 30 minutes*/
							switch(i32ScheduledState)
							{
								case eSCH_STATE_SUBMIT:
								case eSCH_STATE_ALERT_REACH:
									pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState = eSCH_STATE_ALERT_REACH;
									RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STARTED);
									ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STARTED ---->>>>by yan\n");
								break;
								
								case eSCH_STATE_ALERT_REACHED:
								case eSCH_STATE_ALERTING:
									if(iIdx == 0){
										pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState = eSCH_STATE_ALERT_REACHED;
										RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STARTED);
										ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STARTED ---->>>>by yan\n");
									}
								break;
								case eSCH_STATE_STOP:
									RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STOP);
									ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STOP ---->>>>by yan\n");
								break;
							}
						}
					break;
					default:
						switch(i32ScheduledState){
							case eSCH_STATE_SUBMIT:
							case eSCH_STATE_ALERT_REACH:
								pDirectiveSetAlert->asAlertDirective[iIdx].eScheduledState = eSCH_STATE_ALERT_REACH;
								RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STARTED);
								ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STARTED ---->>>>by yan\n");
								iIDELTime[iIdx] = 0;
							break;
							case eSCH_STATE_ALERT_REACHED:
								if(iIdx == 0){
									RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STARTED);
									ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STARTED ---->>>>by yan\n");
								}
								iIDELTime[iIdx] = 0;
							break;
							case eSCH_STATE_STOP:// nomal, afert stop is IDEL, but abnormal(cant sent stop to avs,such as accessToken is NULL ), will sent stop second.
								if( iIDELTime[iIdx]++ > alertBaseRes.iStopNextTime ){
									RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ALERT_STOP);
									ALEXA_INFO("---->>>>eAVS_DIRECTIVE_EVENT_ALERT_STOP ---->>>>by yan\n");
									iIDELTime[iIdx] = 0;
								}
							default:
								iIDELTime[iIdx] = 0;
								break;
						}
				}
				}
			}//for
			
			//if(iIdx == TIMER_MAX ) 
				//ALEXA_INFO("---->>>>alert num = %d---->>>>by yan\n", iIdx);
		}
		break;
		
		default:	
			ALEXA_WARNING("default\n");	
		break;  
		} //switch
		
		isFirst = 0;
		t_NowTime = utils_get_sec_time();
		if(t_NowTime - t_3hours >= 3*60*60){
			t_3hours = t_NowTime;
	   /**
		 * we have to force synchronize ntp time when the local time is 
		 * more than 3hours
		*/
			//only openwrt
			utils_auto_set_time();
		}
		
		/*
		 * send ping event when the connection is maintained for 5 minutes
		*/
		ui64AliveSec = t_NowTime - RK_AVSUpdateAliveConnect();/* 300s || just lost internet */
		if(ui64AliveSec >= DEF_PING_INTERVAL_TIME){
			if(psAlexaRes && psAlexaRes->ui8isTokenValid && psAlexaRes->pAccessToken){
				RK_AVSSetDirectivesEventID(eAVS_ALIVE_PING);
			}
		}

		/* report intervalms to avs */
		ReportInterval_tm = RK_AuidoPlayerReportIntervalMS(&g_astrAvsDirectives.sDirectivePlay.asCurrentPlayStream, ReportInterval_tm);
		
	}while(1);

}

void RK_AlexaDirectivesHandle
(
	S_ALEXA_RES *psAlexaRes
)
{
	pthread_t		threadMediaID;
	pthread_t 		threadTimerID;
	S_AVS_CURL_SET 	*pAvsCurlSet = (S_AVS_CURL_SET *)&psAlexaRes->asAvsCurlSet;
	S_FFPlayback	sFFPlayer;

	/* initialization all avs related directives */
	if(RK_AlexaResourcesParamInit(psAlexaRes) != 0)
		return;
	
	if(RK_AVSDirectivesEventParamInit() != 0)
		return;
	
	RK_Alert_Initial(psAlexaRes);
	
	/* initialization ffplayer hook interface */
	sFFPlayer.ffplayer_open = RK_APBStream_open_callback;
	sFFPlayer.ffplayer_close = RK_APBStream_close_callback;
	sFFPlayer.ffplayer_write = RK_APBStream_write_callback;
	sFFPlayer.ffplayer_init = NULL;
	sFFPlayer.ffplayer_pause = NULL;
	sFFPlayer.ffplayer_read = NULL;
	RK_FFPlayer_paramInit(&sFFPlayer);

	/* create a meida stream pthread*/
	pthread_create(&threadMediaID, 0, RK_AlexaAudioPlayerHandle, psAlexaRes);
	
	/*create a timer pthread*/
	pthread_create(&threadTimerID, 0, RK_AlexaTimerThread, psAlexaRes);
	//debug file descriptor
	directfp1 = fopen("/tmp/finished", "w+");	
	do{
		int found = 0;
		/* Wait a valid directive event come in */
		RK_AVSGetDirectivesEventID(&g_astrAvsDirectives.eEventID);

		switch(g_astrAvsDirectives.eEventID){
			case eAVS_DIRECTIVE_EVENT_SPEECHSTARTED:
				found = 1;
			case eAVS_DIRECTIVE_EVENT_SPEECHFINISHED:
			{
				S_SpeakDirective		*pSpeak = &g_astrAvsDirectives.sDirectiveSpeak;
				struct curl_httppost 	*postData 	= NULL;
				char 					*pJsonData = NULL;
				char					pName[16] = "SpeechFinished";
				int						responecode = 0;

				if(found){
					memset(pName, 0, sizeof(pName));
					strcpy(pName, "SpeechStarted");
				}

				char sMssageReqID[37]; 
	
				RK_Random_uuid(sMssageReqID);
	
				RK_AVSSpeechSynthesizerEvent(&pJsonData, NULL, pName, sMssageReqID, pSpeak->strToken);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);
			
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				if(found)
					speech_finish_state = 0;
			
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
			}
			break;
			case eAVS_DIRECTIVE_EVENT_ALERT_STARTED:		/*start a timer/Alarm */
				found = 1;
			case eAVS_DIRECTIVE_EVENT_SETALERT_SUCCEEDED:	/* set a timer/Alarm */
			case eAVS_DIRECTIVE_EVENT_SETALERT_FAILED:
			{
				ALEXA_DBG("Leave Alert OPT\n");
				S_SetAlertDirectiveSet	*pSetAlerts   = &g_astrAvsDirectives.sDirectiveSetAlert;
				S_SetAlertDirective     *pAlertInput  = &pSetAlerts->sDirectiveSetAlertInput;
				E_SCHEDULE_STATE		eSchState     = eSCH_STATE_WAIT_SUBMIT;
				struct curl_httppost	*postData     = NULL;
				char					*pJsonData    = NULL;
				char					*pName        = NULL;
				char                    *strMessageId = NULL, *strToken = NULL;
				int 					responecode   = 0;
				int		iiIdx = 0;
				int     alertAlerting = 0;
//printf("===>strNamespace:%s, Name:%s, strMessageId:%s\n", g_astrAvsDirectives.sDirectiveSetAlert.strNamespace, g_astrAvsDirectives.sDirectiveSetAlert.strName, g_astrAvsDirectives.sDirectiveSetAlert.strMessageId);
				ALEXA_DBG("directive found=%d\n", found);
				if(found){
					pName = "AlertStarted";
					iiIdx = RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective, (eSchState = eSCH_STATE_ALERT_REACH), TIMER_MAX);
					iiIdx = ((iiIdx < 0 || iiIdx >= TIMER_MAX) ?  RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective, (eSchState = eSCH_STATE_ALERT_REACHED), TIMER_MAX) : iiIdx);
					switch(iiIdx){
						case TIMER_MAX:
						case -1:
						{
							eSchState = eSCH_STATE_IDLE;
						}
						break;
						case 0:
						{
							struct AudioHandler *p_sAhandler = psAlexaRes->psPlaybackCxt->asAhandler + ePB_TYPE_ALERT;
							p_sAhandler->strFileName = ((!strcmp(pSetAlerts->asAlertDirective[iiIdx].strType,"TIMER"))? alertBaseRes.pTimerAudioFile : alertBaseRes.pAlarmAudioFile);
							p_sAhandler->eAudioMask = eAUDIO_MASK_PCM;
							if(RK_Alert_Can_Started(psAlexaRes)){
								ALEXA_DBG("RK_APBOpen Prepare Ring Bell\n");
								p_sAhandler->ui32smplrate = 44100; //cai yang lv.
								p_sAhandler->playseconds= 30*60;
								alertAlerting = !RK_APBOpen(psAlexaRes->psPlaybackCxt, ePB_TYPE_ALERT);
								ALEXA_DBG("RK_APBOpen Ring Bell return %d\n", !alertAlerting);
							}
						}
						default:
							if(eSchState == eSCH_STATE_ALERT_REACH){
								strMessageId = pSetAlerts->asAlertDirective[iiIdx].strMessageId;
								strToken = pSetAlerts->asAlertDirective[iiIdx].strToken;
								if(RK_SysWrState)
									RK_SysWrState(eSYSSTATE_ALERTTIMEUP_NUMADD, 1);
							}
							break;
					}
				}else{
#if 0
					if(pAlertInput->eScheduledState){
						strMessageId = pAlertInput->strMessageId;
						strToken     = pAlertInput->strToken;
						pName = "SetAlertSucceeded";
						eSchState = eSCH_STATE_WAIT_SUBMIT;
					}else{
						eSchState = eSCH_STATE_IDLE;
					}
#else
					strMessageId = pAlertInput->strMessageId;
					strToken     = pAlertInput->strToken;
					if(pAlertInput->eScheduledState){
						pName = "SetAlertSucceeded";
						if(RK_SysWrState)
							RK_SysWrState(eSYSSTATE_ALERTALL_NUMADD, 1);
					}else{
						pName = "SetAlertFailed";
						/////////////////////////////////alert failed hint///////////////////////////////////////////////
						if(RK_SysAudTips)
							RK_SysAudTips(eSYSTIPSCODE_ALERT_FAIL);
					}
					eSchState = eSCH_STATE_WAIT_SUBMIT;
#endif
//					iiIdx = RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective, eSCH_STATE_WAIT_SUBMIT, TIMER_MAX);
//					if( iiIdx < 0 )eSchState = eSCH_STATE_NONE;
				}
				
				if( eSchState == eSCH_STATE_WAIT_SUBMIT || eSchState == eSCH_STATE_ALERT_REACH){	
					RK_SetAlertEvent(&pJsonData, NULL, pName, strMessageId, strToken);
					postData = RK_AVSPackagePostData(pJsonData);
					RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

					RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
					responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

					ALEXA_DBG("%s code:%d\n", pName, responecode);//if send start failed, need send again, until successed.

					if(pJsonData)
						free(pJsonData);
					if(postData)
						curl_formfree(postData);
				}
				
				switch(eSchState){
					case eSCH_STATE_WAIT_SUBMIT:
					{
						iiIdx = RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective, eSCH_STATE_WAIT_SUBMIT, TIMER_MAX);
						if(iiIdx >= 0){
							if(200<=responecode && responecode<300){
								pSetAlerts->asAlertDirective[iiIdx].eScheduledState = eSCH_STATE_SUBMIT;
							}else{
								/*submit */
								RK_Alert_Delete_Index(pSetAlerts, iiIdx, TIMER_MAX);
								if(RK_SysAudTips)
									RK_SysAudTips(eSYSTIPSCODE_ALERT_FAIL);
								RK_Alert_Update_ConfigFile();
							}
						}
					}
					break;
					case eSCH_STATE_ALERT_REACH:
					case eSCH_STATE_ALERT_REACHED:
						pSetAlerts->asAlertDirective[iiIdx].eScheduledState = eSCH_STATE_ALERT_REACHED + alertAlerting;
						break;
					default:
						break;
				}
				if(eSchState==eSCH_STATE_WAIT_SUBMIT || eSchState==eSCH_STATE_ALERT_REACH || alertAlerting){//submit, timeup(reach) and ring bell(alerting) will update config file. 
//					ALEXA_TRACE("Updata Config File Started\n");
					RK_Alert_Update_ConfigFile();
//					ALEXA_TRACE("Updata Config File Over\n");
				}
				if(!found) memset(pAlertInput, 0, sizeof(S_SetAlertDirective));
			}
//			ALEXA_TRACE("Leave Alert OPT\n");
			break;
			case eAVS_DIRECTIVE_EVENT_ALERT_STOP:			/* stop a Timer/Alarm when it is active */	
//				ALEXA_TRACE("---->>>> eAVS_DIRECTIVE_EVENT_ALERT_STOP ---->>>>by yan\n");
				found = 1;
			case eAVS_DIRECTIVE_EVENT_DELETE_ALERT_FAILED:
			case eAVS_DIRECTIVE_EVENT_DELETE_ALERT_SUCCEEDED:			/* Delete a Timer/Alarm */
			{
//				ALEXA_TRACE("Enter Alert OPT\n");
				S_SetAlertDirectiveSet 	*pSetAlerts   = &g_astrAvsDirectives.sDirectiveSetAlert;
				S_DeleteAlertDirective  *pDeleteAlert = &g_astrAvsDirectives.sDirectiveDeleteAlert;
				//S_PBContext				*psPlaybackCxt = psAlexaRes->psPlaybackCxt;
				//struct AudioHandler		*pAlertAudiohandler;
				//S_SetAlertDirective		*pDirective;
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				char					*pName = NULL;
				int 					responecode = 0;
				int iiIdx = -1, iNotCloseSound = 0;//iNotCloseSound only for stop
				char* strMessageId = NULL, *strToken = NULL;
				int ui8PBDone = 0;
				if(found){
					iiIdx = RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective,  eSCH_STATE_STOP, TIMER_MAX);
					if((iiIdx >= 0) && (iiIdx < TIMER_MAX)){//timer according player trigger stop.
						ALEXA_INFO("---->>>> Timer Found Player Triger Alert Stop ---->>>>by yan\n");
						ui8PBDone = 1;
						strMessageId = pSetAlerts->asAlertDirective[iiIdx].strMessageId;
						strToken = pSetAlerts->asAlertDirective[iiIdx].strToken;
						pName = "AlertStopped";
					}else if( pDeleteAlert->cDeleteSucceeded >= eDELALERT_SUCCEEDED ){// delete alert succeeded trigger stop 
						ALEXA_INFO("---->>>> AVS Delete Alert Directive Triger Alert Stop ---->>>>by yan\n");
						iiIdx = pDeleteAlert->cDeleteSucceeded - eDELALERT_SUCCEEDED;
						if(iiIdx >= TIMER_MAX){
							iiIdx = 0;
							iNotCloseSound = 1;
						}
						strMessageId = pSetAlerts->asAlertDirective[iiIdx].strMessageId;
						strToken = pSetAlerts->asAlertDirective[iiIdx].strToken;
						pName = "AlertStopped";
					}
					/*else{ // delete alert failed not trigger stop 
					}*/
					
				}else{
					strMessageId = pDeleteAlert->strMessageId;
					strToken = pDeleteAlert->strToken;
					pName = (pDeleteAlert->cDeleteSucceeded ? "DeleteAlertSucceeded" : "DeleteAlertFailed");
				}
				
				if(pName && strMessageId && strToken){
					RK_SetAlertEvent(&pJsonData, "Alerts", pName, strMessageId, strToken);
					postData = RK_AVSPackagePostData(pJsonData);
					RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);
					//we can start to request a http interaction
					RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
					responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

					ALEXA_DBG("%s code:%d\n", pName, responecode);
					if(pJsonData)
						free(pJsonData);
					if(postData)
						curl_formfree(postData);
				}
				if(found){
					if(ui8PBDone){
//						printf("---->>>> responecode = %d ------->>>>by yan\n", responecode);
						if((responecode >= 200) && (responecode < 300)){
							RK_Alert_Delete_Index(pSetAlerts, iiIdx, TIMER_MAX);
							if(RK_SysWrState)
								RK_SysWrState(eSYSSTATE_ALERTTIMEUP_NUMADD, -1);
						}else if(!iiIdx){
							/*if (alert stop send fail, and the alert is first) then ( move the alert to the last IDLE ALERT).
							 */
							S_SetAlertDirective AlertDirective = pSetAlerts->asAlertDirective[0];
							S_AVS_scheduledTime	AvsSchTm = pSetAlerts->asAvsSchTm[0];
							RK_Alert_Delete_Index(pSetAlerts, 0, TIMER_MAX);
							iiIdx = RK_AlertDirective_Find_State(pSetAlerts->asAlertDirective,  eSCH_STATE_IDLE, TIMER_MAX);
							pSetAlerts->asAlertDirective[iiIdx] = AlertDirective;
							pSetAlerts->asAvsSchTm[iiIdx] = AvsSchTm;
						}
					}else if(iiIdx != -1){
						if(iNotCloseSound) 
							pSetAlerts->asAlertDirective[1].eScheduledState = eSCH_STATE_ALERTING;//in short time(maybe some us), two alert alerting.
						RK_Alert_Delete_Index(pSetAlerts, iiIdx, TIMER_MAX);//
					}
				}else{
					*pDeleteAlert = (S_DeleteAlertDirective){0};
//					memset(pDeleteAlert, 0, sizeof(S_DeleteAlertDirective));
				}
				if(found)// delete directive when just receive, have do update alert config file.
					RK_Alert_Update_ConfigFile();
			}
//			ALEXA_DBG("Leave Alert OPT\n");
			break;
			case eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_FOREGROUND:
				found = 1;
			case eAVS_DIRECTIVE_EVENT_ALERT_ENTERED_BACKGROUND:
			{
				S_SetAlertDirectiveSet	*pSetAlerts = &g_astrAvsDirectives.sDirectiveSetAlert;
				S_SetAlertDirective 	*pDirective;
	//			E_SCHEDULE_STATE		eSchState = eSCH_STATE_WAIT_SUBMIT;
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				char					pName[32] = "AlertEnteredBackground";
				int 					responecode = 0;
				int 	iiIdx;

				if(found){
					memset(pName, 0, sizeof(pName));
					strcpy(pName, "AlertEnteredForeground");
				}
				//mark
				pSetAlerts->bBackGround = (found? 0 : 1);
				for(iiIdx=0; iiIdx<TIMER_MAX; iiIdx++){
					if(pSetAlerts->asAlertDirective[iiIdx].eScheduledState == eSCH_STATE_ALERTING){
						pDirective = &pSetAlerts->asAlertDirective[iiIdx];
						
						RK_SetAlertEvent(&pJsonData, NULL, pName, pDirective->strMessageId, pDirective->strToken);
						postData = RK_AVSPackagePostData(pJsonData);
						RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

						//we can start to request a http interaction
						RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
						responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

						ALEXA_DBG("%s Code:%d\n", pName, responecode);
					}
				}
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
			}
			break;
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_STARTED:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective		*pDirective = NULL;
		//		pthread_t			threadMediaID;
				char				pName[16] = "PlaybackStarted";
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;

			pDirective = &pPlayDirectives->asCurrentPlayStream;
			{
				
				char sMssageReqID[37]; 
	
				RK_Random_uuid(sMssageReqID);
				/* The start time must be recorded */
				pDirective->ui64Startoffset = time(NULL);
				/* Handle PlaybackStarted Event */
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, pName, sMssageReqID, pDirective->strToken, pDirective->offsetInMilliseconds);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);
				/* set audioPlayer callback*/
				RK_AVSSetCurlReadAndWrite(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, \
										NULL, NULL, \
										RK_MediaStreamNearlyFinishedCB, (void *)psAlexaRes, \
										NULL, NULL);
				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
#if 1
				if(psAlexaRes->eAlexaResState & eALEXA_MEDIA_PROMPT){
					RK_APBWrite(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, 1);
					psAlexaRes->isPromptSpeak = 0;
					psAlexaRes->eAlexaResState |= eALEXA_EXPECTSPEECH_1; //mark seven
				}
#endif			
				if(pJsonData)
					free(pJsonData);
						pJsonData = NULL;
				if(postData)
					curl_formfree(postData);
					pJsonData = NULL;
			}
#if 0	//test
			if(pDirective->ui64ReportDelayMS > 0){
				/* commit a ReportDelayElapsed Event */
				RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_ProgressReportDelayElapsed);
			}
#else	
			/* Handle ProgressReportDelayElapsed Event */
			if(pDirective->ui64ReportDelayMS > 0){					
				char sMessageId[37]; 
				long offset = 0;
			
				RK_Random_uuid(sMessageId);

				offset = time(NULL) - pDirective->ui64Startoffset;
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, "ProgressReportDelayElapsed", sMessageId, pDirective->strToken, pDirective->ui64ReportDelayMS+offset);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG(" ProgressReportDelayElapsed Code:%d\n", responecode);
				
				if(pJsonData){
					free(pJsonData);
					pJsonData = NULL;
				}
				if(postData){
					curl_formfree(postData);
					postData = NULL;
				}
			}
		#endif	
#if 0
				/* Handle ProgressReportIntervalElapsed Event */
				if(pDirective->ui64ReportIntervalMS > 0){
					char sMessageId[37]; 
			
					RK_Random_uuid(sMessageId);
				
					RK_AVSPlaybackStartedEvent(&pJsonData, NULL, "ProgressReportIntervalElapsed", sMessageId, pDirective->strToken, pDirective->ui64ReportIntervalMS+10);
					postData = RK_AVSPackagePostData(pJsonData);
					RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

					//we can start to request a http interaction
					RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
					responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

					ALEXA_DBG("ProgressReportIntervalElapsed Code:%d\n", responecode);
					if(pJsonData){
						free(pJsonData);
						pJsonData = NULL;
					}
					if(postData){
						curl_formfree(postData);
						postData = NULL;
					}
				}
#endif				
			}
			break;
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_REPLACE:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				/* restore current play directives */
				memcpy(&pPlayDirectives->asCurrentPlayStream, &pPlayDirectives->asReplacePlayStream, sizeof(S_PlayDirective));
				psAlexaRes->sAvsDirective = &pPlayDirectives->asCurrentPlayStream;
				pPlayDirectives->asCurrentPlayStream.eMplayState = eMPLAY_STATE_PLAYING;
				psAlexaRes->replace_behavior = 0;
				/* The start time must be recorded again*/
				pPlayDirectives->asCurrentPlayStream.ui64Startoffset = time(NULL);

				/*create a Mediastream pthread*/
				pthread_create(&threadMediaID, 0, RK_MediaStreamThread, (void *)psAlexaRes);
				pthread_detach(threadMediaID);
			}
			break;
			case eAVS_DIRECTIVE_EVENT_ProgressReportDelayElapsed:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective		*pDirective = NULL;
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;

				pDirective = &pPlayDirectives->asCurrentPlayStream;
				if(pDirective->eMplayState != eMPLAY_STATE_PLAYING)
					break;

				char sMssageReqID[37]; 
	
				RK_Random_uuid(sMssageReqID);
				
				/* Handle ProgressReportDelayElapsed Event */
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, "ProgressReportDelayElapsed", sMssageReqID, pDirective->strToken, pDirective->ui64ReportDelayMS+8);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG(" ProgressReportDelayElapsed Code:%d\n", responecode);
				if(psAlexaRes->eAlexaResState & eALEXA_MEDIA_PROMPT)
					RK_APBWrite(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, 1);
				psAlexaRes->isPromptSpeak = 0;
				RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, 0);
				psAlexaRes->eAlexaResState &= ~eALEXA_MEDIA_PROMPT;
				
				if(pJsonData){
					free(pJsonData);
					pJsonData = NULL;
				}
				if(postData){
					curl_formfree(postData);
					postData = NULL;
				}			

			}
			break;
			case eAVS_DIRECTIVE_EVENT_ProgressReportIntervalElapsed:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective		*pDirective = NULL;
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;
				char 					sMessageId[37]; 
				/* Handle ProgressReportIntervalElapsed Event */
				//(pDirective->ui64ReportIntervalMS > 0)
				
				pDirective = &pPlayDirectives->asCurrentPlayStream;
				if(pDirective->eMplayState != eMPLAY_STATE_PLAYING || pDirective->ui64Startoffset == 0)
					break;

				RK_Random_uuid(sMessageId);
				/*we must be update the current stearm timestamp and report to avs*/
				pDirective->ui64CurrentOffsetInMilliseconds = (time(NULL) - pDirective->ui64Startoffset) * 1000;
				pDirective->ui64CurrentOffsetInMilliseconds += pDirective->offsetInMilliseconds;
				
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, "ProgressReportIntervalElapsed", sMessageId, pDirective->strToken, pDirective->ui64CurrentOffsetInMilliseconds);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("ProgressReportIntervalElapsed Code:%d\n", responecode);
				if(pJsonData){
					free(pJsonData);
					pJsonData = NULL;
				}
				if(postData){
					curl_formfree(postData);
					postData = NULL;
				}
				
			}
			break;	
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_FINISHED:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective 	*pDirective = &pPlayDirectives->asCurrentPlayStream;
				char				pName[17] = "PlaybackFinished";
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;
				char sMssageReqID[37]; 
	
				RK_Random_uuid(sMssageReqID);

				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, pName, sMssageReqID, pDirective->strToken, pDirective->ui64CurrentOffsetInMilliseconds);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				pPlayDirectives->asCurrentPlayStream.eMplayState = eMPLAY_STATE_FINISHED;
				RK_HLS_MediaChangeState(&psAlexaRes->asMediaFormat, eMEDIA_STATE_STOPED);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
			}
			break;
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_NEARLY_FINISHED:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective 	*pDirective = &pPlayDirectives->asCurrentPlayStream;
				char				pName[32] = "PlaybackNearlyFinished";
				struct curl_httppost	*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;

				char sMssageReqID[37]; 
					
				RK_Random_uuid(sMssageReqID);
				/*we must be update the current stearm timestamp*/
				pDirective->ui64CurrentOffsetInMilliseconds = (time(NULL) - pDirective->ui64Startoffset) * 1000;
				pDirective->ui64CurrentOffsetInMilliseconds += pDirective->offsetInMilliseconds;
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, pName, sMssageReqID, pDirective->strToken, pDirective->ui64CurrentOffsetInMilliseconds);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);
#if 1			
				RK_AVSSetCurlReadAndWrite(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, \
										NULL, NULL, \
										RK_MediaStreamNearlyFinishedCB, (void *)psAlexaRes, \
										NULL, NULL);

#endif
				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				/* clear stream speak flags */
				psAlexaRes->isPromptSpeak = 0;
				
				if(psAlexaRes->eAlexaResState & eALEXA_MEDIA_PROMPT){
					RK_APBWrite(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, 1);
				}else{
					if(responecode == eALEXA_HTTPCODE_NO_CONTENT_204){
//						S_PBContext *pPBCxt = psAlexaRes->psPlaybackCxt;
						pPlayDirectives->asCurrentPlayStream.eMplayState = eMPLAY_STATE_IDLE;
					}else if(!responecode){//(responecode == eALEXA_HTTPCODE_ABORT_997) || 
						/* again commit a PlaybackNearlyFinished Event*/
						RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_NEARLY_FINISHED);
					}
				}
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
			}
			break;
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_PlayDirective 	*pDirective = &pPlayDirectives->asCurrentPlayStream;
			//	S_StopDirective		*pStopDirectives = &g_astrAvsDirectives.sDirectiveStop;
				S_PBContext			*pPBCxt	= psAlexaRes->psPlaybackCxt;
				
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				pName[16] = "PlaybackStopped";
				char				strToken[256] = "0";
				char				*pJsonData = NULL;
				int 				responecode = 0;
				long				ui64offset;
				
				char sMssageReqID[37]; 
					
				RK_Random_uuid(sMssageReqID);

				ALEXA_TRACE("Send eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED.\n");
				/* we must be fored to close ffplay - a decorec*/
				RK_FFCloseStream();
				/* fore to close media play handle*/
				RK_APBClose(pPBCxt, ePB_TYPE_MEDIA1);
				/*we must be update the current stearm timestamp*/
				#if 0
				pDirective->ui64CurrentOffsetInMilliseconds = (time(NULL) - pDirective->ui64Startoffset) * 1000;
				pDirective->ui64CurrentOffsetInMilliseconds += pDirective->offsetInMilliseconds;
				#else
				ui64offset = (time(NULL) - pDirective->ui64Startoffset) * 1000;
				ui64offset += pDirective->offsetInMilliseconds;
				memcpy(strToken, pDirective->strToken, sizeof(strToken));
				#endif
				pDirective->eMplayState = eMPLAY_STATE_STOPPED;
				
				RK_AVSPlaybackStartedEvent(&pJsonData, NULL, pName, sMssageReqID, strToken, ui64offset);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				
#if 0
				if(psAlexaRes->eAlexaResState & eALEXA_MEDIA_PROMPT){
					RK_APBWrite(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, NULL, 1);
					psAlexaRes->isPromptSpeak = 0;
					RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, 0);
					psAlexaRes->eAlexaResState &= ~eALEXA_MEDIA_PROMPT;
					psAlexaRes->eAlexaResState |= eALEXA_EXPECTSPEECH_1;
				}
#endif
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
				
			}
			break;
			case eAVS_DIRECTIVE_EVENT_PLAYBACK_QUEUE_CLEARED:
			{
				S_PlayDirectiveSet *pPlayDirectives = &g_astrAvsDirectives.sDirectivePlay;
				S_ClearQueueDirective	*pClearQueueDirectives = &g_astrAvsDirectives.sDirectiveClearQueue;
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				*pJsonData = NULL;
				int 				responecode = 0;

				/*clear enqueue buffer*/
				if(psAlexaRes->replace_behavior){
					psAlexaRes->replace_behavior = 0;
					memset(&pPlayDirectives->asReplacePlayStream, 0, sizeof(S_PlayDirective));
				}

				if(pClearQueueDirectives->bClearAll){
					RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_STOPPED);
				}
				
				RK_AVSPlaybackQueueClearedEvent(&pJsonData, NULL, "PlaybackQueueCleared", pClearQueueDirectives->strMessageId);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("PlaybackQueueCleared Code:%d\n", responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
				
			}
			break;
			case eAVS_DIRECTIVE_EVENT_VOLUME_CHANGED:
			{
				S_SetVolumeDirective *pDirective = &g_astrAvsDirectives.sDirectiveSetVolume;
				S_SetMuteDirective	 *pMuteDirective = &g_astrAvsDirectives.sDirectiveSetMute;
				S_PBContext 		*pPBCxt = psAlexaRes->psPlaybackCxt;
				char				pName[16] = "VolumeChanged";
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				*pJsonData = NULL;
				int 				responecode = 0;

				/* change device volume */
				RK_APBSetHardwareVolume(pPBCxt, pDirective->i32Volume, 1);	/* to set volume */

				RK_AVSVolumeChangedEvent(&pJsonData, NULL, pName, pDirective->strMessageId, pDirective->i32Volume, pMuteDirective->bMute);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);

			}
			break;
			case eAVS_DIRECTIVE_EVENT_ADJUST_VOLUME_CHANGED:
			{
				S_AdjustVolumeDirective *pDirective = &g_astrAvsDirectives.sDirectiveAdjustVolume;
				S_SetMuteDirective	 *pMuteDirective = &g_astrAvsDirectives.sDirectiveSetMute;
				S_PBContext 		*pPBCxt = psAlexaRes->psPlaybackCxt;
				char				pName[16] = "VolumeChanged";
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				*pJsonData = NULL;
				int 				responecode = 0;
				
				int vol = 0;
				vol = RK_APBSetHardwareVolume(pPBCxt, 0, 0);	/* to get current volume*/
				vol += pDirective->i32Volume;
				/* change device volume */
				RK_APBSetHardwareVolume(pPBCxt, vol, 1);	/* to set volume */
				
				RK_AVSVolumeChangedEvent(&pJsonData, NULL, pName, pDirective->strMessageId, vol, pMuteDirective->bMute);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);

			}
			break;
			case eAVS_DIRECTIVE_EVENT_MUTE_CHANGED:
			{
				S_SetVolumeDirective *pDirective = &g_astrAvsDirectives.sDirectiveSetVolume;
				S_SetMuteDirective	 *pMuteDirective = &g_astrAvsDirectives.sDirectiveSetMute;
				//S_PBContext 		*pPBCxt = psAlexaRes->psPlaybackCxt;
				char				pName[16] = "MuteChanged";
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				*pJsonData = NULL;
				int 				responecode = 0;

				RK_AVSVolumeChangedEvent(&pJsonData, NULL, pName, pMuteDirective->strMessageId, pDirective->i32Volume, pMuteDirective->bMute);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("%s Code:%d\n", pName, responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);

			}
			break;
			case eAVS_DIRECTIVE_EVENT_SYNCHRONIZESTATE:
			{
				//char				pName[16];
				RK_CURL_HTTPPOST		*postData	= NULL;
				char					*pJsonData = NULL;
				int 					responecode = 0;

				RK_SetSynchronizeStateEvent(&pJsonData, 0);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("SynchronizeState Code:%d\n", responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
			}
			break;
			//add System and app control interface @2017-0215
			case eAVS_DIRECTIVE_EVENT_USER_INACTIVITY_REPORT:
			{
				S_ResetUserInactivityDirective *pDirective = &g_astrAvsDirectives.sDirectiveRestUserInactivity;
				
				//S_PBContext 		*pPBCxt = psAlexaRes->psPlaybackCxt;
				RK_CURL_HTTPPOST	*postData	= NULL;
				char				*pJsonData = NULL;
				int 				responecode = 0;
				long				inactive_sec;
				
				inactive_sec = (long)utils_get_sec_time() - pDirective->i32InactiveSec;
				
				RK_AVSUserInactivityReportEvent(&pJsonData, NULL, "UserInactivityReport", pDirective->strMessageId, inactive_sec);
				postData = RK_AVSPackagePostData(pJsonData);
				RK_SetEasyOptionParams(pAvsCurlSet->asCurlSet[eCURL_HND_DIRECTIVE_EVT].curl, psAlexaRes->headerSlist, postData, NULL, false);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT], 1);
				responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_DIRECTIVE_EVT]);

				ALEXA_DBG("UserInactivityReport Code:%d\n", responecode);
				
				if(pJsonData)
					free(pJsonData);
				if(postData)
					curl_formfree(postData);
				
			}
			break;				
			case eAVS_DIRECTIVE_EVENT_EXCEPTIONENCOUNTERED:
			{

			}
			break;				
			case eAVS_DIRECTIVE_EVENT_SWITCH_ENDPOINT:	
			{

			}
			break;
			case eAVS_ALIVE_PING:
			{

				RK_CURL_SLIST	*pPingHeaderList = NULL;
				char strPingHeader[768] = {0};		/*128*6*/
				int iPingHeaderLen;
				int responseCode;
				
				if(psAlexaRes->pAccessToken == NULL){
					ALEXA_WARNING("AccessToken is invalid!\n");
					break;
				}
					
				iPingHeaderLen = RK_AVSPingHeader(strPingHeader, psAlexaRes->pAccessToken);
				if(iPingHeaderLen < 0)
					break;
				
				pPingHeaderList = RK_CurlHttpHeaderPackage(strPingHeader);

				/* set ping request header */
				curl_easy_setopt(pAvsCurlSet->asCurlSet[eCURL_HND_PING].curl, CURLOPT_HTTPHEADER, pPingHeaderList);

				//RK_StartSendPingRequest(psAlexaRes->multiCurlHnd, psAlexaRes->easyCurlHnd[eCURL_HND_PING], &psAlexaRes->asAvsCurlSet);
				//responseCode = RK_WaitResponseCode(psAlexaRes->multiCurlHnd, psAlexaRes->easyCurlHnd[eCURL_HND_PING], &psAlexaRes->asAvsCurlSet, DEF_TIME_OUT);

				//we can start to request a http interaction
				RK_AVSCurlRequestSubmit(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_PING], 1);
				responseCode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_PING]);

				ALEXA_DBG("Ping Code:%d\n", responseCode);
				if(!responseCode){
					RK_AVSSetDirectivesEventID(eAVS_ALIVE_PING);
				}else{
					psAlexaRes->eAlexaResState |= eALEXA_STSTE_NETWORK;
				}
				if(pPingHeaderList)
					curl_slist_free_all(pPingHeaderList);
			}
			break;
			default:
				break;
		}
	}while(1);

   pthread_join(threadTimerID, NULL);
}

#define DEF_TOKEN_PATH		"/tmp/avstoken"

#define ALEXA_TOKEN_DATA 	"grant_type=refresh_token&" \
							"refresh_token=%s&" \
							"client_id=%s"

int RH_AlexaReqToken
(
	S_CURL_SET 	*pCurlSet,
	S_ALEXA_TOKEN 	*pAlexaToken,
	int (*avstoken_callback)(S_ALEXA_TOKEN *psAlexaToken)
)
{
	char pHttpAccessHeader[] =  "Path: /auth/o2/token&Content-type: application/x-www-form-urlencoded&Cache-Control: no-cache&Transfer-Encoding: chunked";
	char pHttpRefreshHeader[] = "Path:/auth/o2/token&Content-type: application/json&Transfer-Encoding: chunked";
	char pSendData[1024] = {0};
	char buffer[2048] = {0};
	RK_CURL_SLIST	*httpHeaderSlist = NULL;
	cJSON 			*pJson = NULL;
	
	int iDataLen = 0;
	int responseCode;
	char	change_login_local = pAlexaToken->change_login;
	if(change_login_local){
		printf(" ---->>>>change_login_local--1 ---->>>>by yan\n");
		cJSON *pSub = NULL;
		int		jsonlength = 0;
		pJson = cJSON_Parse(pAlexaToken->psRefreshJsonData);
		if(pJson == NULL)
			return eALEXA_ERRCODE_FAILED;
		
		pSub = cJSON_GetObjectItem(pJson, "client_id");
		if(pSub != NULL){
			//mark
			jsonlength = strlen(pSub->valuestring);
			pAlexaToken->psClientID = realloc(pAlexaToken->psClientID, jsonlength+1);
			memcpy(pAlexaToken->psClientID, pSub->valuestring, jsonlength);
			pAlexaToken->psClientID[jsonlength] = '\0';

			httpHeaderSlist = RK_AVSCurlHttpHeaderPackage(pHttpRefreshHeader);
			iDataLen = strlen(pAlexaToken->psRefreshJsonData);
			memcpy(pSendData, pAlexaToken->psRefreshJsonData, iDataLen);
		}
	}else if(pAlexaToken->psRefreshToken != NULL){
		httpHeaderSlist = RK_AVSCurlHttpHeaderPackage(pHttpAccessHeader);
		iDataLen = sprintf( pSendData, ALEXA_TOKEN_DATA, pAlexaToken->psRefreshToken, pAlexaToken->psClientID);
	}else	/* No found login */
		return eALEXA_ERRCODE_NO_VALID_RES;
	
	pAlexaToken->tokenfp = fopen( DEF_TOKEN_PATH,	"w+" );
	if( pAlexaToken->tokenfp == NULL ){
		perror("fopen");
		return eALEXA_ERRCODE_FAILED_OPEN_DEV;
	}

	
	RK_AVSTokenEasyParamsSet(pCurlSet->curl, httpHeaderSlist, pSendData, iDataLen);
	
	/*set curl callback */
	RK_AVSSetCurlReadAndWrite(pCurlSet->curl, NULL, NULL, \
						RK_TokenDataFromAvsCB, (void *)pAlexaToken->tokenfp, \
						NULL, NULL);

	RK_AVSCurlRequestSubmit(pCurlSet, 1);

	RK_AVSTriggerPerform();
	
	usleep(10*1000);
	responseCode = RK_AVSWaitRequestCompleteCode(pCurlSet);

	ALEXA_INFO("TokenReq Code:%d\n", responseCode);
	
	fflush(pAlexaToken->tokenfp);
	fclose(pAlexaToken->tokenfp);
	
	if(httpHeaderSlist)
		curl_slist_free_all(httpHeaderSlist);
	
	if(responseCode != eALEXA_HTTPCODE_OK_200){
		sleep(5);
		return eALEXA_ERRCODE_FAILED;
	}

	responseCode = eALEXA_ERRCODE_FAILED;
	/* grab refresh or accesss token message */
	do{
	cJSON * pSub = NULL;
	int 	jsonlength = 0;

	//pAlexaResData->pAccessToken = NULL;
	/* Open storage file again */
	pAlexaToken->tokenfp = fopen(DEF_TOKEN_PATH,	"rb" );
	if( pAlexaToken->tokenfp == NULL ){
		perror("fopen");
		return eALEXA_ERRCODE_WRITE_FILE_FAILED;
	}
	/* Read all context to  the cache buffer */
	fread(buffer, 1, sizeof(buffer), pAlexaToken->tokenfp);
	
	/*get json block*/
	pJson = cJSON_Parse(buffer);
	if(pJson == NULL)
		break;
	
	/* get access_token */
	pSub =  cJSON_GetObjectItem(pJson, "access_token");
	if(pSub == NULL)
		break;

	jsonlength = strlen(pSub->valuestring);
	pAlexaToken->psAccessToken = realloc(pAlexaToken->psAccessToken, jsonlength+1);
	/* realloc mem failure */
	if(pAlexaToken->psAccessToken == NULL)
		break;
	
	memcpy(pAlexaToken->psAccessToken, pSub->valuestring, jsonlength);
	pAlexaToken->psAccessToken[jsonlength] = '\0';
	pAlexaToken->i32AccessTokenLength = jsonlength;
	//printf("Access_Token:\n<%s>\n", pAlexaToken->psAccessToken);
	/*get expires time */
//	pSub =	cJSON_GetObjectItem(pJson, "expires_in");
//	pAlexaToken->ui64ExpireSec = pSub->valueint;
		
	/* get refresh_token */
	if(change_login_local){
		pSub =	cJSON_GetObjectItem(pJson, "refresh_token");
		if(pSub == NULL){
			//pAlexaToken->change_login = 0;	//20170406 cg
			break;
		}
		jsonlength = strlen(pSub->valuestring);
		pAlexaToken->psRefreshToken = realloc(pAlexaToken->psRefreshToken, jsonlength+1);
		/* realloc mem failure */
		if(pAlexaToken->psRefreshToken == NULL){
			//pAlexaToken->change_login = 0;		//20170406 cg
			break;
		}
		memcpy(pAlexaToken->psRefreshToken, pSub->valuestring, jsonlength);
		pAlexaToken->psRefreshToken[jsonlength] = '\0';
		pAlexaToken->i32RefreshTokenLength = jsonlength;
		ALEXA_INFO("psRefreshToken:\n<%s>\n", pAlexaToken->psRefreshToken);

		//RH_WriteAlexaConfig(pAlexaToken);
		avstoken_callback(pAlexaToken);

		if(pAlexaToken->psRefreshJsonData != NULL){
			free(pAlexaToken->psRefreshJsonData);
			pAlexaToken->psRefreshJsonData = NULL;
		}
	
		//pAlexaToken->change_login = 0;		//20170406 cg
	}
	/* prepare to ping */
	//pAlexaResData->pAccessToken = pAlexaToken->psAccessToken;
	responseCode = eALEXA_ERRCODE_SUCCESS;
	//req_token = 0;
	/* update access token expire time when get a new token access_token*/
	pAlexaToken->ui64ExpireSec = utils_get_sec_time();

	}while(0);
	
	if(pJson)
		cJSON_Delete(pJson);

	if(pAlexaToken->tokenfp)
		fclose(pAlexaToken->tokenfp);

	return responseCode;
}


/*******************************************************************************
* Description.: calling this function starts the alexa work
* Input Argument: psAlexaHnd   - a Alexa Handler context to S_ALEXA_HND
* Return Value: 0 is success, negative is failed
*******************************************************************************/
void *HI_AlexaDoHandleThread(void *arg)
{	
	S_ALEXA_RES 	*pAvsResData = (S_ALEXA_RES *)arg;
	
	RK_AVSHttpReqInteraction(pAvsResData->asAvsCurlSet.multiCurlHnd, &pAvsResData->asAvsCurlSet, NULL, 0, 1);		
	
	return NULL;
}

void *HI_AlexaDirectivesThread(void *arg)
{
	RK_AlexaDirectivesHandle((S_ALEXA_RES *)arg);
	
	return NULL;
}
/**
 *
 */
void *HI_AlexaDownchannelThread(void *arg)
{
	S_ALEXA_RES 	*pAvsResData = (S_ALEXA_RES *)arg;
	static struct 	timespec	s_tsReq = {0, 1000};

	/*keep loop until alexa quit*/
	do{

		if(pAvsResData->ui8isTokenValid != 0){
			sleep(2);
			RK_AVSDownchannelHandle(&pAvsResData->asAvsCurlSet, pAvsResData->pAccessToken, RK_DownchannelDataFromAvsCB, arg);
		}

		nanosleep(&s_tsReq, NULL);
		
	}while(1);

	return NULL;
}

void RK_AlexaMasterWorkHandler(void *psResData)
{
	pthread_t 		threadDirectivesID;	/* listen and handle directive event*/
	pthread_t 		threadAvsDoID;	/* alexa interactive pthread*/
	pthread_t 		threadAvsDownchannelID;	/* downchannel */

	RK_AVSCurlMutexInit();

	pthread_create(&threadDirectivesID, 0, HI_AlexaDirectivesThread, (void *)psResData);
	pthread_create(&threadAvsDoID, 0, HI_AlexaDoHandleThread, (void *)psResData);
	/*start downchannel stream */
	pthread_create(&threadAvsDownchannelID, 0, HI_AlexaDownchannelThread, (void *)psResData);
	
	pthread_detach(threadDirectivesID);
    pthread_detach(threadAvsDoID);
    pthread_detach(threadAvsDownchannelID);

}


//
/*  
 * audio_player.c , 2016/08/16 , rak University , China 
 * 	Author: Sevencheng	<dj.zheng@rakwireless.com>
 * modify: cg.yan
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
//zl codec header
#include "vprocTwolf_access.h"
#include "alexa/audio_playerItf.h"
#include "player_io.h"
#include "audio_player.h"
#include "RKLog.h"

#undef bool
#undef false
#undef true
#define bool	uint8_t
#define false	0
#define true	(!false)

static rak_log_t pbbugfs;
#define	DEF_USR_BIND_DEVICE_CONF_FILE_NAME 		"/etc/wiskey/alexa.conf"

//add -
typedef struct {
	unsigned short uGain:7,extuGain:3;
}S_Gain_t;

typedef struct playhandleInfo {
	int    pbHandle;
	int    pbType;
	int    pbState;
	void  *usrprivstart;
	void  *usrprivfinished;
	int    (*pbstart_callback)(void* hnd, void* usrpriv);
	int    (*pbfinished_callback)(void* hnd, void* usrpriv);
	void  *vpbHandle;
	const char *description;
	pthread_mutex_t m_sMutexPBState;
}s_playHandleInfo;

static int RK_PBSetState(E_PB_STATE state, s_playHandleInfo *pPlayHndInfo){
	pthread_mutex_lock(&pPlayHndInfo->m_sMutexPBState);
	int ret = 0;
	if(pPlayHndInfo){
		switch(state){
			case ePB_STATE_CLOSED:{
				pPlayHndInfo->pbState = state;
			}break;
			case ePB_STATE_OPENED:{
				if(pPlayHndInfo->pbState == ePB_STATE_CLOSED)
					pPlayHndInfo->pbState = state;
				else if(pPlayHndInfo->pbState == ePB_STATE_FINISHED)
					pPlayHndInfo->pbState = ePB_STATE_PLAYING;
			}break;
			case ePB_STATE_PLAYING:
			case ePB_STATE_FINISHED:
			case ePB_STATE_PAUSED:{
				if((pPlayHndInfo->pbState & 0x00ff) != ePB_STATE_CLOSED)
					pPlayHndInfo->pbState = state|(pPlayHndInfo->pbState&0xff00);
			}break;
			case ePB_STATE_UNDERRUN:
			case ePB_STATE_OVERRUN:{
				pPlayHndInfo->pbState = state|(pPlayHndInfo->pbState&0x00ff);
			}break;
			default:
				ret=-1;
		}
	}else{
		ret=-1;
	}
	pthread_mutex_unlock(&pPlayHndInfo->m_sMutexPBState);
	return ret;
}


//add -
/* to open a player handler */
static int RK_PBOpenHandler
(
	S_AudioConf *pconf, 
	s_playHandleInfo	*pPlayHndInfo,
	AUDIO_IO *ctrl,
	void (*done_callback)(int , void*)
)
{

	int 				ret = 0;
	S_AVFormat  		psAVFmt;
	enum AVCodecID 		audio_codec;
	memset(&psAVFmt, 0, sizeof(S_AVFormat));// from "uiCmd.m_uiCmd = eMSP_PLAYER_CMD_START" Previous Line move to here.

	switch(pconf->m_eAudioMask){
		case eAUDIO_MASK_MP3:
			audio_codec = AV_CODEC_ID_MP3;
		break;
		case eAUDIO_MASK_AAC:
			audio_codec = AV_CODEC_ID_AAC;
		break;
		case eAUDIO_MASK_PCM:
			audio_codec = AV_CODEC_ID_PCM_S16LE;
		break;
		default:
			if( !pconf->strFileName || !pconf->strFileName[0] ){
				ret = -1;
			}
			break;
	}
	
	if( !pconf->strFileName || !pconf->strFileName[0]){
		if( ret==-1 || audio_codec == AV_CODEC_ID_NONE ){
			LOG_P(pbbugfs, RAK_LOG_ERROR, "Open < %s > No found < E_AUDIO_MASK >\n", pPlayHndInfo->description);
			return -1;
		}
	}
	if(pconf->strFileName) 
		strcpy(psAVFmt.fileName, pconf->strFileName);
	
	psAVFmt.ui32sample_rate = pconf->m_i32smplrate;
    psAVFmt.audio_codec = audio_codec;
    psAVFmt.play_done_cbfunc = done_callback;
	psAVFmt.playseconds = pconf->playseconds;
	psAVFmt.replaytimes = pconf->replaytimes;
	psAVFmt.usrpriv = pPlayHndInfo;
    ret = RK_Audio_OpenHandle(pPlayHndInfo->vpbHandle, &psAVFmt);


#if 0	
    if(ret < 0) //player start success
    {
       LOG_P(bugfs, RAK_LOG_ERROR, "Open device playhandle is failure - ID:0x%x\n", ret);
	}
#endif	
	return ret;
}

static void RK_PBDeinit_AudioHandler(s_playHandleInfo	*au)
{
	RK_PBSetState(ePB_STATE_CLOSED, au);
	au->pbHandle = -1;
}

static int RK_PBDeliverStreamToDevice(void *pvPriv, unsigned char *pAVbuffer, size_t bufferlens, bool isempty)
{
	s_playHandleInfo			*pPlayHndInfo	= (s_playHandleInfo *)pvPriv;
	S_PLAYER_CMD_AUDIO_DATA 	AvsPkt;
	int 						iRetValue;
	
#if 1	
	if(!isempty){
		AvsPkt.frameBuf = (unsigned char *)malloc(bufferlens);
		memcpy(AvsPkt.frameBuf, pAVbuffer, bufferlens);
		AvsPkt.u32frameSize = bufferlens;
		AvsPkt.pbId = pPlayHndInfo->pbHandle;
		AvsPkt.u32lastpkt = 0;
	}else{
		if(pPlayHndInfo->vpbHandle){
			LOG_P(pbbugfs, RAK_LOG_INFO, "Insert the last packet of %s - PacketSize:%d\n", 
				pPlayHndInfo->description, bufferlens);
		}else{
			LOG_P(pbbugfs, RAK_LOG_ERROR, "No found playhandle %s!\n", pPlayHndInfo->description);
		}
		AvsPkt.frameBuf = (unsigned char *)malloc(bufferlens);
		AvsPkt.u32frameSize = bufferlens;
		AvsPkt.pbId = pPlayHndInfo->pbHandle;
		AvsPkt.u32lastpkt = 1;
	}
#endif

	iRetValue = RK_Audio_WriteDataHandle(pPlayHndInfo->vpbHandle, pPlayHndInfo->pbHandle, &AvsPkt);
	if(iRetValue == -7){
		LOG_P(pbbugfs, RAK_LOG_DEBUG, "Writen buffer is overflow - codeID:%d\n", iRetValue);
		free(AvsPkt.frameBuf);
		AvsPkt.frameBuf = NULL;
		iRetValue = ePB_ERR_WRITE_OVERFLOW;
	}else if(iRetValue == -1){
		LOG_P(pbbugfs, RAK_LOG_ERROR, "ePLAYER_ERRCODE_INVALID_PLAY_HANDLE\n");
		RK_PBDeinit_AudioHandler(pPlayHndInfo);
		iRetValue = ePB_ERR_NO_RES;
	}
	
	return iRetValue;
}

static void player_done_callback(int pb, void* req){
	s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo *)req;
	
	if(pPlayHndInfo){
		LOG_P(pbbugfs, RAK_LOG_INFO, "Playhandle: < %s > is %s!\n", pPlayHndInfo->description, (pb?"finished":"start"));
		if(pb){	//finished
			RK_PBSetState(ePB_STATE_FINISHED, pPlayHndInfo);
			if(pPlayHndInfo->pbfinished_callback){
				pPlayHndInfo->pbfinished_callback((void*)pPlayHndInfo, pPlayHndInfo->usrprivfinished);
			}
		}else{	//stated
			if(RK_Playback_state((void *)pPlayHndInfo) == ePB_STATE_CLOSED)
				RK_PBSetState(ePB_STATE_OPENED, pPlayHndInfo);
			RK_PBSetState(ePB_STATE_PLAYING, pPlayHndInfo);
			if(pPlayHndInfo->pbstart_callback){
				pPlayHndInfo->pbstart_callback((void*)pPlayHndInfo, pPlayHndInfo->usrprivstart);
			}
		}
	}else{
		LOG_P(pbbugfs, RAK_LOG_ERROR, "No found the Playhandle type !\n");
	}
	
}

//0:have been finished by player,-1:playing or closed by alexa

int RK_Playback_pause( int pause, void *userPtr)
{
	s_playHandleInfo	*pPlayHndInfo	= (s_playHandleInfo *)userPtr;
	int play_handle = 0;
	int ret;

	if(userPtr == NULL)
		return ePB_ERR_INVALID_USR_RES;
	
	play_handle = pPlayHndInfo->pbHandle;
	
	LOG_P(pbbugfs, RAK_LOG_INFO, "Send %s %s\n", pause?"pause":"resume", pPlayHndInfo->description);
	if(!pause)
		usleep(500*1000);
	
	ret = RK_Audio_PauseHandle(pPlayHndInfo->vpbHandle, play_handle, pause);
	if(ret != 0){
     	LOG_P(pbbugfs, RAK_LOG_ERROR, "%s %s Error!\n", pause?"pause":"resume", pPlayHndInfo->description);
	}else{
		if(pause){
			RK_PBSetState(ePB_STATE_PAUSED, pPlayHndInfo);
		}else{
			RK_PBSetState(ePB_STATE_PLAYING, pPlayHndInfo);
		}
	}

    return ret;
}
/*set dmix mutli stream*/
int RK_Playback_setvolume(unsigned char f16Volume, void *userPtr)
{
	s_playHandleInfo	*pPlayHndInfo	= (s_playHandleInfo *)userPtr;
	int ret = ePB_ERR_SUCCESS;

	if(userPtr == NULL)
		return ePB_ERR_INVALID_USR_RES;

	LOG_P(pbbugfs, RAK_LOG_INFO, "Set %s Softvolume - %d\n", pPlayHndInfo->description, f16Volume);
	ret = RK_Audio_VolumeHandle(pPlayHndInfo->vpbHandle, pPlayHndInfo->pbHandle, f16Volume);
	if(ret){
		LOG_P(pbbugfs, RAK_LOG_ERROR, "%s Error!\n", pPlayHndInfo->description);
		ret = ePB_ERR_FAIL;
	}
	
    return ret;	
}

int RK_PBSaveHWVolume(int volume){
	FILE	*fconf = NULL, *fconfold = NULL;
	char	fbuf[1024] = {0};
	char	*pTmpPtr = NULL;
	int 	ret;
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", "w+");
	fconfold = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	if(fconfold){
		if(fconf){
			while(fgets(fbuf, 1024, fconfold)){
				if(strncmp(fbuf, "dev_volume #=", strlen("dev_volume #="))){
					int len = strlen(fbuf);
					if(fbuf[len-2] == '\r' || fbuf[len-2] == '\n')fbuf[len-2] = 0;
					if(fbuf[len-1] == '\r' || fbuf[len-1] == '\n')fbuf[len-1] = 0;
					if(fbuf[len] == '\r' || fbuf[len] == '\n')fbuf[len] = 0;
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
		fprintf(fconf, "dev_volume #=%d\n", volume);
		fclose(fconf);
		ret = 0;
		LOG_P(pbbugfs, RAK_LOG_INFO, "SaveHardwareVolume to %d successfully\n", volume);
	}else{
		LOG_P(pbbugfs, RAK_LOG_ERROR, "SaveHardwareVolume to %d unsuccessfully\n", volume);
		ret =-1;
	}
	fconf = NULL;
	remove(DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	rename(DEF_USR_BIND_DEVICE_CONF_FILE_NAME".new", DEF_USR_BIND_DEVICE_CONF_FILE_NAME);
	sync();
	
	return ret;
}

/* ******************************************
 * volume(0~100~150) <=> db(-90~1~6)
 * db ≈ 28.409 * log(volume + 0.0627) / log(10) -55.8257;
 * volume ≈ round(pow(10, (db + 55.8257) / 28.409) - 0.0627);
 * *******************************************/
static double db_to_vol_f(double db)
{
	return round(pow(10, (db + 87.610) / 44.222) - 0.883);
}

static double vol_to_db_f(int volume)
{
	return 44.222 * log(volume + 0.883) / log(10) - 87.610;
}

/* *************************************************************************
 * Discription: set/get volume 
 * return value: -1, on error, otherwise, current volume(0~200) are returned.
 */
int RK_Playback_HardwareVolume(int i32Volume, int isSetVol)
{
	static int g_i32Volume = -1;
	int iRetValue = -1;
	if(isSetVol>0){
		if(i32Volume < 0)
			i32Volume = 0;
		if(i32Volume > 100)
			i32Volume = 100;
		LOG_P(pbbugfs, RAK_LOG_INFO, "Volume %d Entery\n", i32Volume);
		if(vol_to_db_f(i32Volume) != vol_to_db_f(g_i32Volume)){
			S_Gain_t sGain = {0};
			int fd;
			VprocTwolfHbiInit("/dev/zl380tw", &fd);
			double gain_db = vol_to_db_f(i32Volume)+20;
			if(gain_db>21)gain_db=21;
			else if(gain_db<-54) gain_db=-54;
			sGain.extuGain = 3;	//round((gain_db < -24)?((-gain_db - 24) / 6) : 0);
			sGain.uGain = round((gain_db < -24)? 0x00 : (gain_db / 0.375 + 64));
			if(g_i32Volume>=0)
				LOG_P(pbbugfs, RAK_LOG_INFO, "Current device volume - %d, set volume %d, set gain_db - %f\n", g_i32Volume, i32Volume, gain_db);
			else
				LOG_P(pbbugfs, RAK_LOG_INFO, "Resume volume - %d, resume gain_db - %f\n", i32Volume, gain_db);
			iRetValue = VprocTwolfHbiWrite(fd, 0x30A, 1, (short unsigned int *)&sGain);
			if(iRetValue)
				iRetValue = VprocTwolfHbiWrite(fd, 0x30A, 1, (short unsigned int *)&sGain);
			iRetValue = (iRetValue? iRetValue : i32Volume);
			if(iRetValue >= 0) g_i32Volume = i32Volume;
			VprocTwolfHbiCleanup(fd);
		}else{
			iRetValue = i32Volume;
			g_i32Volume = i32Volume;
		}
		RK_PBSaveHWVolume(i32Volume);
		LOG_P(pbbugfs, RAK_LOG_INFO, "Volume %d Exit\n", i32Volume);
	}else{
		LOG_P(pbbugfs, RAK_LOG_INFO, "Get Volume Entry\n");
		if(g_i32Volume < 0 || g_i32Volume > 200){
			S_Gain_t sGain={0};
			int fd;
			VprocTwolfHbiInit("/dev/zl380tw", &fd);
			VprocTwolfHbiRead(fd, 0x30A, 1, (short unsigned int *)&sGain);
			if(sGain.extuGain<=5 && sGain.uGain<=0x78){
				int ugain = sGain.uGain;
				int extugain = sGain.extuGain;
				double gain_db = (ugain - 64) * 0.375 - extugain * 6;
				double db = gain_db -20;
				g_i32Volume = db_to_vol_f(db);
			}
			VprocTwolfHbiCleanup(fd);
		}
		iRetValue = g_i32Volume;
		LOG_P(pbbugfs, RAK_LOG_INFO, "Get Volume %d Exit\n", iRetValue);
	}
	return iRetValue;
}
// hardware volume end
/* -1 error and close handle, -7 -  currunt handle overflow */
int RK_Playback_write(unsigned char *buf, int size, void *userPtr)
{
	bool isempty = false;
	int ret = ePB_ERR_SUCCESS;
	do{
		if(userPtr == NULL){
			ret = ePB_ERR_INVALID_USR_RES;
			break;
		}
		if(size == 1 && buf == NULL)
			isempty = true;
		ret = RK_PBDeliverStreamToDevice(userPtr, buf, size, isempty);
	}while(0);
	
	return ret;
}
int RK_Playback_state(void *userPtr)
{
	int state = ePB_ERR_SUCCESS;
	s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo *)userPtr;
	
	pthread_mutex_lock(&pPlayHndInfo->m_sMutexPBState);
	if(pPlayHndInfo)
		state= pPlayHndInfo->pbState;
	else
		state = ePB_ERR_NO_RES;
	pthread_mutex_unlock(&pPlayHndInfo->m_sMutexPBState);
	
	return state;
}
/*force close a handler*/
int RK_Playback_close(void *userPtr)
{
	s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo *)userPtr;
	int 		ret = ePB_ERR_SUCCESS;

	if(pPlayHndInfo == NULL){
		LOG_P(pbbugfs, RAK_LOG_ERROR, "PlayHndInfo Is NULL!\n");
		return ePB_ERR_INVALID_USR_RES;
	}
	ret = RK_Audio_CloseHandle(pPlayHndInfo->vpbHandle, 0);
	if(ret != 0){
		LOG_P(pbbugfs, RAK_LOG_ERROR, "RK_Audio_CloseHandle close %s Is Failed!\n", pPlayHndInfo->description);
		return ePB_ERR_CLOSE_DEV;
	}
	
	RK_PBDeinit_AudioHandler(pPlayHndInfo);
	LOG_P(pbbugfs, RAK_LOG_INFO, "RK_Audio_CloseHandle close %s Is Successed!\n", pPlayHndInfo->description);
	
	return ret;
}

int RK_Playback_open(S_AudioConf *pconf, void *userPtr)
{
	s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo *)userPtr;

	if(userPtr == NULL)
		return ePB_ERR_INVALID_USR_RES;
	//add -	

	pPlayHndInfo->pbHandle = RK_PBOpenHandler(pconf, pPlayHndInfo, NULL, player_done_callback);
	if(pPlayHndInfo->pbHandle < 0){
		RK_PBSetState(ePB_STATE_CLOSED, pPlayHndInfo);
		LOG_P(pbbugfs, RAK_LOG_ERROR, "Open playback %s handle is failed, No found valid handle - %d!\n", pPlayHndInfo->description, pPlayHndInfo->pbHandle);
		return ePB_ERR_NO_RES;
	}
	
	RK_PBSetState(ePB_STATE_OPENED, pPlayHndInfo);

	LOG_P(pbbugfs, RAK_LOG_INFO, "Open Playback handle: %s\n", pPlayHndInfo->description);

	return ePB_ERR_SUCCESS;
}

static int RK_PBVolumeInit(void){
	FILE 	*fconf = NULL;
	char fbuf[1024];
	char *pbuf;
	int volume = 50;
	fconf = fopen(DEF_USR_BIND_DEVICE_CONF_FILE_NAME, "rb");
	if(fconf){
		pbuf = fbuf;
		while(fgets(pbuf, 1024, fconf)){
			int len;
			if(!strncmp(fbuf, "dev_volume #=", (len = strlen("dev_volume")))){
				pbuf += len;
				sscanf(pbuf, " #=%d", &volume);
				LOG_P(pbbugfs, RAK_LOG_DEBUG, "hard ware volume: %d\n", volume);
			}
			pbuf = fbuf;
		}
		fclose(fconf);fconf=NULL;
	}
	if(volume<0)
		volume=50;
	
	RK_Playback_HardwareVolume(volume, 1);
	
}

void RK_Playback_global_init(void)
{
	pbbugfs = rak_log_init("PLAYER", RAK_LOG_INFO, 8, NULL, NULL);
	RK_AudioIO_GlobalInit();
	int fd;
//	VprocTwolfHbiInit("/dev/zl380tw", &fd);
	RK_PBVolumeInit();
	LOG_P(pbbugfs, RAK_LOG_INFO, "PASS\n");
}

int RK_Playback_create
( void **userPtr, const char* description,
	int (*pbstart_callback)(void* hnd, void* usrpriv),    void *usrprivstart,
	int (*pbfinished_callback)(void* hnd, void* usrpriv), void *usrprivfinished
)
{
	/* fix me: this is a temp handle, best can be improved by your */
	LOG_P(pbbugfs, RAK_LOG_TRACE, "Entry!\n");
	int ret = 0;
	void* vhandle = RK_AudioIO_Init();
	if(vhandle){
		s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo*)calloc(1, sizeof(s_playHandleInfo));
		if(pPlayHndInfo){
			pPlayHndInfo->vpbHandle = vhandle;
			pthread_mutex_init(&pPlayHndInfo->m_sMutexPBState, NULL);
			pPlayHndInfo->pbstart_callback = pbstart_callback;
			pPlayHndInfo->pbfinished_callback = pbfinished_callback;
			pPlayHndInfo->usrprivfinished = usrprivfinished;
			pPlayHndInfo->usrprivstart= usrprivstart;
			RK_PBDeinit_AudioHandler(pPlayHndInfo);
			pPlayHndInfo->description = description;
			*userPtr = (void *)pPlayHndInfo;
		}else{
			LOG_P(pbbugfs, RAK_LOG_ERROR, "%s Exit!\n", description?description:"NO description");
			ret = -1;
		}
	}else{
		LOG_P(pbbugfs, RAK_LOG_ERROR, "%s Exit!\n", description?description:"NO description");
		ret = -1;
	}
	LOG_P(pbbugfs, RAK_LOG_TRACE, "%s Exit!\n", description?description:"NO description");
	return 0;
}
/*
change MEDIA/ALERT current state
*/


//add @20161107 merge
int RK_Playback_Query_Playing_seeks(int expect_seeks, void *userPtr)//return 0 on success, negatives on fail to play,
{
	int seeks;
	s_playHandleInfo *pPlayHndInfo = (s_playHandleInfo *)userPtr;
	
	//LOG_P(pbbugfs, RAK_LOG_INFO, "%s Entry!\n", pPlayHndInfo->description);
	
	do{
		char state = (char)RK_Playback_state(pPlayHndInfo);
		if (state == ePB_STATE_CLOSED) {
			//LOG_P(bugfs, RAK_LOG_INFO, "PB have done:%d\n", *pAudioHnd->ui8PBDone);
			break;
		}
		seeks = RK_Audio_QuerySeeks(pPlayHndInfo->vpbHandle, pPlayHndInfo->pbHandle);
		if(seeks <= expect_seeks)
			break;
        usleep(10*1000);
	}while(1);
	
	//LOG_P(pbbugfs, RAK_LOG_INFO, "%s Exit!\n", pPlayHndInfo->description);
	return seeks;
}
//end

S_AudioPlayback rk_audio_playback = {
    .name				= "audio_playback",
    .pb_open        	= RK_Playback_open,
    .pb_close       	= RK_Playback_close,
    .pb_state			= RK_Playback_state,
    .pb_write			= RK_Playback_write,
    .pb_set_dmix_volume = RK_Playback_setvolume,
    .pb_hardwarevolume	= RK_Playback_HardwareVolume,
    .pb_pause 			= RK_Playback_pause,
    .pb_query_seeks		= RK_Playback_Query_Playing_seeks,
};


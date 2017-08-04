//
/*  
 * audio_player.c , 2016/08/16 , rak University , China 
 * 	Author: Sevencheng	<dj.zheng@rakwireless.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//zl codec header
#include "vprocTwolf_access.h"
#include "audio_player.h"
#include "player_io.h"
#include "utils.h"

#define PLAYER_DEBUG
#ifdef PLAYER_DEBUG
#define PLAYER_DBG(...) 	fprintf(stderr, " PB_DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define PLAYER_DBG(...)
#endif

//#define PLAYER_INFO_DBG
#ifdef PLAYER_INFO_DBG
#define PLAYER_INFO(...)	{ fprintf(stderr, " PB_INFO(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define PLAYER_INFO(...) {;}
#endif

#if 1
	#define PLAYER_ERROR(...)		{ fprintf(stderr, "PB_ERROR(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
	#define PLAYER_ERROR(...)		{;}
#endif

#define RK_DEBUG_MAX_MASK_DESC_LENGTH   32

typedef struct {
    unsigned char 	Mask;
    unsigned char   Description[RK_DEBUG_MAX_MASK_DESC_LENGTH];
} DEBUG_MASK_DESCRIPTION;


static DEBUG_MASK_DESCRIPTION player_debug_desc[] = {
    { eOUTPUT_FORMAT_DIALOG, 	"dialog"},
    { eOUTPUT_FORMAT_ALERT, 	"alert"},
    { eOUTPUT_FORMAT_MEDIA1, 	"media stream"},
    { eOUTPUT_FORMAT_MEDIA2, 	"media stream2"},
    {eOUTPUT_FORMAT_SYSTIP,		"system tips sound"},
};


//static S_MSF_GLOBALS 	*pb_global = NULL;
static int              gs_ui8DialogDone = 0;
static int              gs_ui8AlertDone = 0;
static int				gs_ui8Media1Done = 0;
static int				gs_ui8Media2Done = 0;
//add @20170519
static S_PBContext *	gs_pbforsysCueshnd = NULL;
static S_PBContext *	gs_pbforalertCueshnd = NULL;

static int 				gs_chghndtimes = 0;
//add -
typedef struct {
	short lvol:8,hvol:8;
}S_HwVol_t;
static S_HwVol_t g_sHwVol = {-1,-1};


AUDIO_IO *g_sAv_IO = NULL;
//add @20170519
dialog_view_func_t dialog_view =NULL;

void RK_PBChange_handleState(S_PBContext *h, int state);
int RK_Playback_setvolume(S_PBContext *h, E_PB_TYPE ePlaybackType, unsigned char f16Volume);
//add -
/* to open a player handler */
static int RK_PBOpenHandler
(
	E_PB_TYPE 		ePbtype, 
	S_AudioHnd	*pAudioHnd,
	AUDIO_IO *ctrl,
	void (*done_callback)(int , void*)
)
{
	//U_MSF_PLUGIN_CMD	uiCmd;
	//S_PLAYER_REQ		sPlayerRequest;
	//E_PLAY_FUNCTION_ID	eFuncId = FUNCTION_NONE;
	//E_VOICE_FORMAT		eVoiceFmt = -1;
	//int					enqueue = 0;
	int 				ret = 0;
	//iRetValue=0;
	//E_VOICE_TYPE        eVoiceType = TYPE_STREAM;

	E_OUTPUTFORMAT		eFuncId;
	S_AVFormat  		psAVFmt;
	enum AVCodecID 		audio_codec;
	memset(&psAVFmt, 0, sizeof(S_AVFormat));// from "uiCmd.m_uiCmd = eMSP_PLAYER_CMD_START" Previous Line move to here.

	switch(ePbtype){
		case ePB_TYPE_DIALOG:
			eFuncId = eOUTPUT_FORMAT_DIALOG;
		break;
		case ePB_TYPE_ALERT:
			if(pAudioHnd->strFileName == NULL) return -1;
			strcpy(psAVFmt.fileName, pAudioHnd->strFileName);   //modified by yanchonggaodian  +add  for music file
			eFuncId = eOUTPUT_FORMAT_ALERT;
		break;
		case ePB_TYPE_MEDIA1:
		case ePB_TYPE_MEDIA2:	
			eFuncId = eOUTPUT_FORMAT_MEDIA1;
			break;
		case ePB_TYPE_SYSTIP:
			if(pAudioHnd->strFileName == NULL) return -1;
			strcpy(psAVFmt.fileName, pAudioHnd->strFileName);
			eFuncId = eOUTPUT_FORMAT_SYSTIP;
			break;
		default:
			PLAYER_ERROR("No found < E_PB_TYPE >\n");
			break;
	}
	
	switch(pAudioHnd->eAudioMask){
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
			if( !pAudioHnd->strFileName || !pAudioHnd->strFileName[0] )
				PLAYER_ERROR("No found < E_AUDIO_MASK >\n");
			break;
	}
	
	if( !pAudioHnd->strFileName || !pAudioHnd->strFileName[0]){
		if(eFuncId == eOUTPUT_FORMAT_UNKNOW || audio_codec == AV_CODEC_ID_NONE){
			return -1;
		}
	}
	
	psAVFmt.ui32sample_rate = pAudioHnd->ui32smplrate;
	psAVFmt.eOutputFormat = eFuncId;
    psAVFmt.audio_codec = audio_codec;
    psAVFmt.play_done_cbfunc = done_callback;
	psAVFmt.playseconds = pAudioHnd->playseconds;
	psAVFmt.replaytimes = pAudioHnd->replaytimes;
    ret = RK_Audio_OpenHandle(g_sAv_IO, &psAVFmt);
	
    if(ret < 0) //player start success
    {
       PLAYER_ERROR("Open device playhandle is failure - ID:0x%x\n", ret);
	}
	
	return ret;
}

static void RK_Deinit_AudioHandler(S_AudioHnd	*au)
{
	if(au->ui8PBDone != NULL)
		*au->ui8PBDone = 0;
	
	au->ePlaybackState = ePB_STATE_IDLE;
	au->eAudioMask = eAUDIO_MASK_NONE;
	au->i8PbIdx = -1;
	//au->ui8PBDone = NULL;
	au->blast_packet = 0;
	au->max_packet_size = 0;
}

/* to close a player handler */
static int RK_PBCloseHandler(int8_t i8pbId)
{
	int					play_handle = 0;
	int 				ret =0;

	play_handle = (int)i8pbId;

	RK_Audio_CloseHandle(g_sAv_IO, play_handle);

	if(ret != 0){
	   PLAYER_ERROR("Open device playhandle is failure - ID:0x%x\n", ret);
	}
	
	return ret;
}

static int RK_PBDeliverStreamToDevice(void *pvPriv, unsigned char *psResData, size_t *datalen)
{
	//U_MSF_PLUGIN_CMD	 	uiCmd;
	S_AudioHnd				*pAudioHnd	= (S_AudioHnd *)pvPriv;
	S_PLAYER_CMD_AUDIO_DATA AvsPkt;
	size_t 					datasize = *datalen;
	int 					iRetValue;
	
#if 1	
	//uiCmd.m_uiCmd = eMSP_PLAYER_CMD_DATA;

	if(!pAudioHnd->blast_packet){
		AvsPkt.frameBuf = (unsigned char *)malloc(datasize);
		memcpy(AvsPkt.frameBuf, psResData, datasize);
		AvsPkt.u32frameSize = datasize;
		AvsPkt.pbId = pAudioHnd->i8PbIdx;
		AvsPkt.u32lastpkt = 0;
	}else{
		if(pAudioHnd->i8PbIdx < eOUTPUT_FORMAT_CNT){
			PLAYER_DBG("Insert the last packet of %s - PacketSize:%d\n", 
				player_debug_desc[pAudioHnd->i8PbIdx].Description, datasize);
		}else{
			PLAYER_DBG("No found playhandle - %d!\n", pAudioHnd->i8PbIdx);
		}
		
		AvsPkt.frameBuf = (unsigned char *)malloc(datasize);
		AvsPkt.u32frameSize = datasize;
		AvsPkt.pbId = pAudioHnd->i8PbIdx;
		AvsPkt.u32lastpkt = 1;
	}
#endif

	//iRetValue = pb_global->m_asPluginPriv[eMSF_PLUGIN_ID_PLAYER].m_pPluginIF->m_pfnCommand(uiCmd, (const void*)&AvsPkt, NULL);

	iRetValue = RK_Audio_WriteDataHandle(g_sAv_IO, pAudioHnd->i8PbIdx, &AvsPkt);
//	printf("--->len:%d, RK_Audio_WriteDataHandle return value %d\n", datasize, iRetValue);
	if(iRetValue == -7){
		PLAYER_DBG("Writen buffer is overflow - codeID:%d\n", iRetValue);
		free(AvsPkt.frameBuf);
		AvsPkt.frameBuf = NULL;
	}else if(iRetValue == -1){
		PLAYER_ERROR("ePLAYER_ERRCODE_INVALID_PLAY_HANDLE\n");
		RK_Deinit_AudioHandler(pAudioHnd);
	}
	
	return iRetValue;
}

static void player_done_callback(int pb, void* req)
{
//	S_PLAYER_REQ *pReq = (S_PLAYER_REQ *)req;

  //	PLAYER_DBG("playerhandle=%d, funId =%d, status =%ld\n", pb, pReq->funId, pReq->status); 

	int pb_type = *(int *)req;

	if(pb_type < eOUTPUT_FORMAT_CNT)
		PLAYER_DBG("Playhandle: <%d - %s> is done!\n", pb_type, 
			player_debug_desc[pb_type].Description); 
	
	if(pb_type == eOUTPUT_FORMAT_DIALOG){
		gs_ui8DialogDone = 1;
	}else if(pb_type == eOUTPUT_FORMAT_ALERT){
		gs_ui8AlertDone = 1;
		//add @20170519
		if(gs_pbforalertCueshnd){
			S_PBContext *h = gs_pbforalertCueshnd;
			if(h->asAhandler[ePB_TYPE_MEDIA1].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePB_TYPE_MEDIA1, 1);
			}
			if(h->asAhandler[ePB_TYPE_MEDIA2].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePB_TYPE_MEDIA2, 1);
			}
			gs_pbforalertCueshnd=NULL;
		}//add -
	}else if(pb_type == eOUTPUT_FORMAT_MEDIA1){
		gs_ui8Media1Done = 1;
	}else if(pb_type == eOUTPUT_FORMAT_MEDIA2){
		gs_ui8Media2Done = 1;
	}else if(pb_type == ePB_TYPE_SYSTIP){
	//add @20170519
		if(gs_pbforsysCueshnd){
			RK_PBChange_handleState(gs_pbforsysCueshnd, 0);
		}
	//add -	
	}else{
		PLAYER_DBG("No found the Playhandle type - %d!\n", pb_type);
	}
	
}


//0:have been finished by player,-1:playing or closed by alexa
static int RK_Playback_finished(S_PBContext *h, E_PB_TYPE ePlaybackType, int timeout)//ms
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
	int cnt, ret = 0;
	int timeo = timeout/10;
	
	do{
		if(pAudioHnd->ui8PBDone == NULL){
			ret = 1;
			break;
		}
				
		if (*pAudioHnd->ui8PBDone != 0 || pAudioHnd->ePlaybackState == ePB_STATE_IDLE) {
			//PLAYER_INFO("PB have done:%d\n", *pAudioHnd->ui8PBDone);
			break;
		}

        usleep(10*1000);
		cnt++;
		
		if (cnt > timeo && timeout) {
			ret = 1;
			break;
		}
		
	}while(1);
	/*nomarl quite*/
	if(ret == 0){
		RK_Deinit_AudioHandler(pAudioHnd);
	}else{
		return -1;
	}
	
	return 0;
}

static int RK_Playback_pause( S_PBContext *h, E_PB_TYPE ePlaybackType, int pause)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
	int play_handle = 0;
	int ret;

	play_handle = (int)pAudioHnd->i8PbIdx;
	
	if(pause){
		
		PLAYER_INFO("Send pause codeID: %d\n", play_handle);
	}else{
		PLAYER_INFO("Send continue codeID: %d\n", play_handle);
	}
	
	ret = RK_Audio_PauseHandle(g_sAv_IO, play_handle, pause);
	
	if(ret != 0)
	{
     	PLAYER_ERROR("Send pause is failure!\n");
	}

    return ret;
}
/*set dmix mutli stream*/
int RK_Playback_setvolume(S_PBContext *h, E_PB_TYPE ePlaybackType, unsigned char f16Volume)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
	int play_handle = 0;
	int ret;

	play_handle = (int)pAudioHnd->i8PbIdx;
	PLAYER_INFO("Send softvolume - %d\n", f16Volume);
	ret = RK_Audio_VolumeHandle(g_sAv_IO, play_handle, f16Volume);

    return ret;	
}
//set codec hardware volume
static S_HwVol_t RK_GetHWVolume(int fd)
{
	int iRetValue = -1;
	float k1=2.0, k2=1, k3=0.52;
	S_HwVol_t Vol = {-1, -1};
	iRetValue = VprocTwolfHbiRead(fd, 0x238, 1, (short unsigned int *)&Vol);
	if(!iRetValue){
		Vol.lvol +=90;
		Vol.lvol = (Vol.lvol <= 40)? (Vol.lvol / k1) : ((Vol.lvol <= 70)?((Vol.lvol-20) / k2): ((Vol.lvol-44) / k3));
		Vol.hvol +=90;
		Vol.hvol = (Vol.hvol <= 40)? (Vol.hvol / k1) : ((Vol.hvol <= 70)?((Vol.hvol-20) / k2): ((Vol.hvol-44) / k3));
	}else{
		Vol.hvol = -1;
		Vol.lvol = -1;
	}
	return Vol;
}
/* return device current value of volume when get volume from device*/
int RK_Playback_HardwareVolume(int i32Volume, int isSetVol)
{	
	int iRetValue = -1;
	float k1=2.0, k2=1, k3=0.52;
//	pthread_mutex_lock(&hwvolume_mutex);
	if(isSetVol>0){
		if(i32Volume < 0)
			i32Volume = 0;
		if(i32Volume > 100)
			i32Volume = 100;
		{
			S_HwVol_t Vol = g_sHwVol;
			int fd = VprocTwolfGetFd("/dev/zl380tw");
			if(Vol.hvol < 0){
				Vol = RK_GetHWVolume(fd);
				g_sHwVol.hvol = Vol.hvol;
			}
			Vol.hvol = ((Vol.hvol <= 20) ? (k1 * Vol.hvol) : (Vol.hvol <= 50)?(k2 * Vol.hvol + 20): (k3 * Vol.hvol + 44) )-90;
			Vol.lvol = ((i32Volume <= 20) ? (k1 * i32Volume) : (i32Volume <= 50)?(k2 * i32Volume + 20): (k3 * i32Volume + 44) )-90;
			PLAYER_DBG("Current device volume - %d\n", Vol.lvol+90);
			iRetValue = VprocTwolfHbiWrite(fd, 0x238, 1, (short unsigned int *)&Vol);
			if(iRetValue)
				iRetValue = VprocTwolfHbiWrite(fd, 0x238, 1, (short unsigned int *)&Vol);
			short softreset = 4;
			iRetValue = VprocTwolfHbiWrite(fd, 0x6, 1, (short unsigned int *)&softreset);
			if(iRetValue)
				iRetValue = VprocTwolfHbiWrite(fd, 0x6, 1, (short unsigned int *)&softreset);
			iRetValue = (iRetValue? iRetValue : i32Volume);//g_sHardwareVolume
			if(iRetValue >= 0) g_sHwVol.lvol = i32Volume;
		}
	}else{
		if(g_sHwVol.lvol<0 || isSetVol){
			int fd = VprocTwolfGetFd("/dev/zl380tw");
			S_HwVol_t Vol = RK_GetHWVolume(fd);
			if(Vol.lvol>=0){
				g_sHwVol = Vol;
			}
		}
		iRetValue = (g_sHwVol.lvol>=0) ? (g_sHwVol.lvol) : iRetValue;
	}
//	pthread_mutex_unlock(&hwvolume_mutex);
	return iRetValue;
}
// hardware volume end

static int RK_Playback_write( S_PBContext *h, E_PB_TYPE ePlaybackType, unsigned char *buf, int size)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
//	PLAYER_DBG("============RK_Playback_write========= writesize = %d =====\n", size);
	return RK_PBDeliverStreamToDevice(pAudioHnd, buf, &size);
}
/*force close a handler*/
static int RK_Playback_close( S_PBContext *h, E_PB_TYPE ePlaybackType)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
	int 		ret = 0;

	ret = RK_PBCloseHandler(pAudioHnd->i8PbIdx);
	if(ret != 0)
		return ret;

	RK_Deinit_AudioHandler(pAudioHnd);
	
	return ret;
}

static int RK_Playback_open( S_PBContext *h, E_PB_TYPE ePlaybackType)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePlaybackType];
	
	h->ePlaybackType = ePlaybackType;
	////add @20170519
	if(h->ePlaybackType == ePB_TYPE_SYSTIP)	
		RK_PBChange_handleState(h, 1);
	//add -	
	pAudioHnd->i8PbIdx = RK_PBOpenHandler(h->ePlaybackType, pAudioHnd, NULL, player_done_callback);
	if(pAudioHnd->i8PbIdx < 0){
		RK_PBChange_handleState(h, 0); //add @20170519
		PLAYER_ERROR("Open playback handle is failed, No found valid handle - %d!\n", pAudioHnd->i8PbIdx);
		return -1;
	}
	
	PLAYER_DBG("Open Playback handle: %d - %s\n", h->ePlaybackType, player_debug_desc[pAudioHnd->i8PbIdx].Description);
	switch(h->ePlaybackType){
		case ePB_TYPE_DIALOG:
			pAudioHnd->ui8PBDone = (uint8_t *)&gs_ui8DialogDone;
			////add @20170519
			if(dialog_view)
				dialog_view();
			//add -	
		break;
		case ePB_TYPE_ALERT:
			pAudioHnd->ui8PBDone = (uint8_t *)&gs_ui8AlertDone;
			//add @20170519
//			gs_ui8AlertDone=0;
			gs_pbforalertCueshnd=h;
			if(h->asAhandler[ePB_TYPE_MEDIA1].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePB_TYPE_MEDIA1, 0);
			}
			if(h->asAhandler[ePB_TYPE_MEDIA2].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePB_TYPE_MEDIA2, 0);
			}
			//add -
		break;
		case ePB_TYPE_MEDIA1:
			//printf("------------->open:%d, id:%d\n", gs_ui8Media1Done, pAudioHnd->i8PbIdx);
			pAudioHnd->ui8PBDone = (uint8_t *)&gs_ui8Media1Done;
			//add @20170519
			if(h->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePlaybackType, 0);
			}
			//add -
			break;
		case ePB_TYPE_MEDIA2:
			pAudioHnd->ui8PBDone = (uint8_t *)&gs_ui8Media2Done;
			//add @20170519
			if(h->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE){
				RK_Playback_setvolume(h, ePlaybackType, 0);
			}
			//add -
			break;
		case ePB_TYPE_SYSTIP:
			break;
		default:
			PLAYER_ERROR("No found < E_PB_TYPE >\n");
			break;
	}
	
	pAudioHnd->ePlaybackState = ePB_STATE_START;
	
	return 0;
}

static int RK_Playback_init( S_PBContext *h)
{
	/* fix me: this is a temp handle, best can be improved by your */
	dialog_view = h->priv_data; //add @20170519
	g_sAv_IO = (AUDIO_IO *)RK_AudioIO_Init();
	if(g_sAv_IO == NULL){
		PLAYER_ERROR("INIT audio_playback is failure!\n");
		return -1;
	}
	//open codec dev
	int fd;
	VprocTwolfHbiInit("/dev/zl380tw", &fd);
	return 0;
}

/*return 0 - close ok, -1-dialog close faile */
int RK_PBCheckHandler(S_PBContext *h)
{
	int ret = 0;
	PLAYER_INFO("here\n");
	if(h->asAhandler[ePB_TYPE_DIALOG].ePlaybackState != ePB_STATE_IDLE){
		ret = RK_PBCloseHandler(h->asAhandler[ePB_TYPE_DIALOG].i8PbIdx);
	PLAYER_INFO("here - %d\n", ret);
		if(ret != 0)
			return -1;		/* close fail*/
			
	if(h->asAhandler[ePB_TYPE_DIALOG].ui8PBDone != NULL)	
		*h->asAhandler[ePB_TYPE_DIALOG].ui8PBDone = 0;
		RK_Deinit_AudioHandler(&h->asAhandler[ePB_TYPE_DIALOG]);
		sleep(1);	/* sure close handle for only @junhua player*/
	}

	if(h->asAhandler[ePB_TYPE_ALERT].ePlaybackState != ePB_STATE_IDLE){
		ret = 1;
	}else if(h->asAhandler[ePB_TYPE_MEDIA1].ePlaybackState != ePB_STATE_IDLE){
		if(RK_Playback_finished(h, ePB_TYPE_MEDIA1, 10) < 0)	/* 10ms */	
			ret = 1;
	}else if(h->asAhandler[ePB_TYPE_MEDIA2].ePlaybackState != ePB_STATE_IDLE){
		if(RK_Playback_finished(h, ePB_TYPE_MEDIA2, 10) < 0)	/* 10ms */
			ret = 1;
	}

	return ret;
}
/*
change MEDIA/ALERT current state
*/
void RK_PBChange_handleState(S_PBContext *h, int state)
{
	int iIdx, pause = 0;
	unsigned char volume = 1;

	if(state){
		pause = 1;
		volume = 0;
		//add @20170519
		PLAYER_INFO("+\n");
		gs_pbforsysCueshnd = h;
		gs_chghndtimes++;
		if(gs_chghndtimes>1)return;
	}else{
		PLAYER_INFO("-\n");
		gs_chghndtimes--;
		if(!gs_chghndtimes)
			gs_pbforsysCueshnd = NULL;
		if(gs_chghndtimes>0)return;
		//add -
	}

	for(iIdx=1; iIdx<ePB_TYPE_CNT; iIdx++){
		if(iIdx == ePB_TYPE_SYSTIP) //
			continue;//
		if(h->asAhandler[iIdx].ePlaybackState != ePB_STATE_IDLE){
			if(iIdx == ePB_TYPE_ALERT)
				RK_Playback_pause(h, iIdx, pause);
			else
				RK_Playback_setvolume(h, iIdx, volume);
		}
	}

}
	
/* get a idle media play handle */
int RK_PBGetMediaHandler(S_PBContext *h, E_PB_TYPE ePBType)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePBType];
	uint8_t		enalbe = 0, timeo;
	
	do{
		//PLAYER_DBG("Check PlaybackState:%d\n", h->asAhandler[ePBType].ePlaybackState);
		if(h->asAhandler[ePBType].ePlaybackState == ePB_STATE_IDLE){
			h->ePBMeidaIdx = ePBType;
			enalbe = 1;
		}else{
			if(timeo > 500)
				break;/* wait 100s quite */
			if(RK_Playback_finished(h, ePBType, 100) == 0)	/* 100ms */
				continue;
			timeo++;
		}
		
	}while(!enalbe);

	if(enalbe == 0){
		h->ePBMeidaIdx = ePB_TYPE_UNKNOW;
		PLAYER_ERROR("No Wait a vaild Media handle queue!\n");
		return -1;
	}

	return 0;
}
//add @20161107 merge
int RK_Query_Playing(void *arg)//return 0 on success, negatives on fail to play,
{
	int ret = 0;
	printf("---->>>>Alert Can Ring Bell---->>>>by yan\n");
	return ret;
}
//end

S_AudioPlayback rk_audio_playback = {
    .name				= "audio_playback",
	.pb_init			= RK_Playback_init,
    .pb_open        	= RK_Playback_open,
    .pb_close       	= RK_Playback_close,
    .pb_write			= RK_Playback_write,
    .pb_set_dmix_volume = RK_Playback_setvolume,
    .pb_hardwarevolume	= RK_Playback_HardwareVolume,
    .pb_pause 			= RK_Playback_pause,
    .pb_wait_finished 	= RK_Playback_finished,
    .pb_change_handle	= RK_PBChange_handleState,
    .pb_get_runing_handles = RK_Query_Playing,
};


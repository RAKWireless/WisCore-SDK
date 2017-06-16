/*
* player based on libao\libmad development.
* by seven
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "audio_debug.h"
#include "player_io.h"
#include "dec_mp3.h"
#include "dec_raw.h"

typedef struct Playback_handle{
	int 			default_driver;
	char			*fileName;
	int				header_size;
	time_t			stoptime;	/*when the pbID close*/
	int				replaytimes;
	ao_device		*device;			/*ao device handle*/
	unsigned int	ui32sample_rate;
	enum AVCodecID 	audio_codec;
	void 			*decodec;
	E_PBHSTATE		ePBHstate;
	E_OUTPUTFORMAT	eOutputFormat;
	void (*done_callback)(int, void*);
	pthread_mutex_t m_sMutexPBDone;
	struct Playback_handle *next;
}S_PlaybackHnd;

typedef struct _context {
	uint8_t			bInitialized;
	S_PlaybackHnd 	psPBHandle[eOUTPUT_FORMAT_CNT];
	int 			default_driver;
}S_CONTEXT_PB;
//asPBHandle;
S_CONTEXT_PB	g_PlayerContext;
#if 0
static pthread_mutex_t s_sCmdMutex;
static uint32_t s_u32CmdCode;
static void * s_pvCmdArg;

static uint8_t *s_pu8CmdArguBuf = NULL;
static uint32_t s_u32CmdArguBufSize = 0;
#endif
#if 0
enum mad_flow {
  MAD_FLOW_CONTINUE = 0x0000,	/* continue normally */
  MAD_FLOW_STOP     = 0x0010,	/* stop decoding normally */
  MAD_FLOW_BREAK    = 0x0011,	/* stop decoding and signal an error */
  MAD_FLOW_IGNORE   = 0x0020	/* ignore the current frame */
};
#endif
int RK_AVWriteDataHandle(S_PlaybackHnd *pPBHnd, S_PLAYER_CMD_AUDIO_DATA *pAudioPkt);

static int RK_AVCloseHandle(S_PlaybackHnd *pPBHnd);

int player_readpkt_thread(S_PlaybackHnd *pPBHnd)
{
	S_MP3ATTR 			*pMp3attr = (S_MP3ATTR *)pPBHnd->decodec;
	if(pPBHnd->fileName){
	    pMp3attr->fileName = pPBHnd->fileName;
		pMp3attr->stoptime = pPBHnd->stoptime;
		pMp3attr->replaytimes = pPBHnd->replaytimes;
	}
//	pMp3attr->save_pcm_fp = fopen("won.pcm", "w+");

	RK_MADRun_decoder(pMp3attr);

	RK_AVCloseHandle(pPBHnd);
	pMp3attr->fileName = NULL;
#if 0	
	RK_MADClose_stream((S_MP3ATTR *)pPBHnd->decodec);
	
// self free
	if(pPBHnd->decodec){
		AUDIO_INFO("release decodec...............\n");
		free(pPBHnd->decodec);
		pPBHnd->decodec = NULL;
	}

	pPBHnd->ePBHstate = ePBH_STATE_IDLE;

	if(pPBHnd->done_callback)
		pPBHnd->done_callback(pb, (void *)&pPBHnd->eOutputFormat);
#endif

  	return 0;
}

int player_raw_thread(S_PlaybackHnd *pPBHnd)
{
	S_RAWATTR 			*pRawAttr = (S_RAWATTR *)pPBHnd->decodec;
	if(pPBHnd->fileName && pPBHnd->fileName[0]){
		pRawAttr->header_size = pPBHnd->header_size;
		pRawAttr->replaytimes = pPBHnd->replaytimes;
	}
    AUDIO_INFO("codec = AV_CODEC_ID_PCM_S16LE===begin\n");
	RK_RAWRun_pcms16le(pRawAttr, pPBHnd->ui32sample_rate, pPBHnd->fileName, pPBHnd->stoptime);

	RK_AVCloseHandle(pPBHnd);

  	return 0;
}

enum AVCodecID player_search_decodec_by_file(S_AVFormat	*psAVFmt)
{
	char * fileName = psAVFmt->fileName;
	if(!fileName)return AV_CODEC_ID_NONE;
	FILE *stream = fopen(fileName, "rb");
	if(!stream)return AV_CODEC_ID_NONE;
	unsigned char ptr[200];
	unsigned char *str = ptr;
	unsigned char *fmt=NULL;
	int headersize = 0;
	fread((void *)ptr, (size_t)1, (size_t)sizeof(ptr), stream);
	
	//int strncmp(const char *s1, const char *s2, size_t n);
	if(!strncmp((char*)str, "ID3", 3)){
		fclose(stream);
		return AV_CODEC_ID_MP3;
	}
	if(((*(unsigned short*)str) & 0xE0FF) == 0xE0FF){
		unsigned char version,layer,bitrateindex, samplerateiesindex;
//		unsigned char crc,fillbit,privateflag,channelmode,modeextention,copyright, original_negative;
		int index = 0;
		version = (str[1] & 0x18)>>3;
		layer = (str[1] & 0x6)>>1;
//		crc = (str[1] & 0x1)>>0;
		bitrateindex = str[2]>>4;
		samplerateiesindex = (str[2]&0x06)>>2;
//		fillbit = (str[2]&0x02)>>1;
//		privateflag = (str[2] & 0x1)>>0;
//		channelmode = str[3]>>6;
//		modeextention = (str[3]&0x30)>>4;
//		copyright = (str[3]&0x08)>>3;
//		original_negative = (str[3]&0x07)>>2;
//		printf("---->>>>layer=%d; version=%d; bitrateindex=%d; samplerateiesindex=%d\n", layer, version, bitrateindex, samplerateiesindex);
		index = 0;
		while(index<200){
			if((str[index]==0xFF) && ((str[index+1]&0xE0)==0xE0)){
				index+=1;
				continue;
			}
			if(str[index] == 'X' && str[index+1] == 'i' && str[index+2]=='n' && str[index+3]=='g'){
				fmt = str + index;
				break;
			}
			if(str[index] == 'V' && str[index+1] == 'B' && str[index+2]=='R' && str[index+3]=='I'){
				fmt = str + index;
				break;
			}
			if(str[index] == 'I' && str[index+1] == 'n' && str[index+2]=='f' && str[index+3]=='o'){
				fmt = str + index;
				break;
			}
			index++;
		}
		int stroffset = fmt-str;
		if(stroffset>=10 && stroffset<50){
			fclose(stream);
			return AV_CODEC_ID_MP3;
		}
		if(!(layer==0 || version==1 || bitrateindex==0xf || samplerateiesindex==3)){
			fclose(stream);
			return AV_CODEC_ID_MP3;
		}
//		printf("---->>>>layer=%d; version=%d; bitrateindex=%d; samplerateiesindex=%d\n", layer, version, bitrateindex, samplerateiesindex);
	}
#if 1
	str = ptr;
	if(!strncmp((char*)str, "RIFF", 4)){
		str +=8;
		if(!strncmp((char*)str, "WAVEfmt ", 8)){
			str +=8;
		}else{
			fclose(stream);
			return AV_CODEC_ID_PCM_S16LE;
		}
		fmt = str +4;
		str = str + *(int*)str + 4;
		if(!strncmp((char*)str, "fact", 4)){
			str +=12;
		}
		if(!strncmp((char*)str, "data", 4)){
			str +=8;
		}else{
			fclose(stream);
			return AV_CODEC_ID_PCM_S16LE;
		}
		headersize = str - ptr;
		psAVFmt->header_size = headersize;
		short* pchannels = (short*)(fmt+2);
		int* prate = (int*)(fmt+4);
		psAVFmt->ui32sample_rate = (*pchannels) * (*prate) / 2;
		fclose(stream);
		return AV_CODEC_ID_PCM_S16LE;
	}
#endif


#if 0
	str = ptr;
	while(1){
		if(*str==0xff && (*(str+1) & 0xfe) == 0xfe)
			break;
		str++;
		if(str-ptr > sizeof(ptr)-5)
			break;
	}
	
#endif
	fseek(stream, -128L, SEEK_END);
	memset(ptr,0,128);
	fread((void *)ptr, (size_t)1, (size_t)sizeof(ptr), stream);
	if(!strncmp((char*)ptr, "TAG", 3)){
		fclose(stream);
		return AV_CODEC_ID_MP3;
	}
	fclose(stream);
	return AV_CODEC_ID_PCM_S16LE;
}


void *player_begin_thread(void *arg)
{
	S_PlaybackHnd *pPBHnd = (S_PlaybackHnd *)arg;

	do{
			
		switch(pPBHnd->audio_codec){
			case AV_CODEC_ID_MP3:	
			{
				player_readpkt_thread(pPBHnd);
			}
			break;
			case AV_CODEC_ID_AAC:
			{
				AUDIO_WARNING("No support <AAC> audio codec format!\n");
			}
			break;
			case AV_CODEC_ID_PCM_S16LE:
			{
				player_raw_thread(pPBHnd);
				AUDIO_INFO("codec = AV_CODEC_ID_PCM_S16LE====end\n");
			}
			break;
			default:
			AUDIO_ERROR("No found audio codec format!\n");
				break;
		}
		/* current task have finished */
		if(pPBHnd->ePBHstate == ePBH_STATE_IDLE)
			break;
		
	}while(0);
	pPBHnd->ePBHstate = ePBH_STATE_IDLE;
	return NULL;
}

E_PBHSTATE RK_AVCheckHandleState(S_PlaybackHnd *pPBHnd)
{
	E_PBHSTATE ePBHState = ePBH_STATE_IDLE; 

	pthread_mutex_lock(&pPBHnd->m_sMutexPBDone);

	if(pPBHnd->ePBHstate != ePBH_STATE_IDLE)
		ePBHState = ePBH_STATE_BUSY;
		
	pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);

	return ePBHState;
}
//0-ok
static int RK_AVInitHandle(S_PlaybackHnd *pPBHnd, enum AVCodecID enc_type)
{
	int ret = 0;
	memset(pPBHnd, 0, sizeof(S_PlaybackHnd));

	if(enc_type == AV_CODEC_ID_MP3){
		pPBHnd->audio_codec = enc_type;
		pPBHnd->decodec = (void *)RK_MAD_Mp3Attr_alloc();
		ret = RK_MADInitParams((S_MP3ATTR *)pPBHnd->decodec, pPBHnd->default_driver);
		if(pPBHnd->decodec == NULL)
			AUDIO_TRACE("mp3 decodec is null\n");
		else
			AUDIO_TRACE("mp3 decodec is no null\n");
	}else if(enc_type == AV_CODEC_ID_AAC){
		pPBHnd->audio_codec = enc_type;

	}else if(enc_type == AV_CODEC_ID_PCM_S16LE){
		pPBHnd->audio_codec = enc_type;
		pPBHnd->decodec = (void *)RK_RAW_PcmAttr_alloc();
		if(pPBHnd->decodec == NULL)
			AUDIO_TRACE("raw decodec is null\n");
		else
			AUDIO_TRACE("raw decodec is no null\n");

		ret = RK_RAWInitParams((S_RAWATTR *)pPBHnd->decodec, pPBHnd->default_driver);
	}else{
		AUDIO_WARNING("Don't know codec types\n");
		ret = eAUDIO_ERRCODE_NO_VALID_RES;
	}
		
	return ret;
}

static void RK_AVConfigHandle(S_PlaybackHnd *pPBHnd, S_AVFormat *pAVFmt)
{
	pPBHnd->ui32sample_rate = pAVFmt->ui32sample_rate;
	pPBHnd->audio_codec = pAVFmt->audio_codec;
	pPBHnd->eOutputFormat = pAVFmt->eOutputFormat;
	pPBHnd->header_size = pAVFmt->header_size;
	if(pAVFmt->fileName && pAVFmt->fileName[0])
		pPBHnd->fileName = strdup(pAVFmt->fileName);
	else if(pPBHnd->fileName){
		free(pPBHnd->fileName);
		pPBHnd->fileName = NULL;
	}
	if(pAVFmt->playseconds)
		pPBHnd->stoptime = pAVFmt->playseconds + time(NULL);
	pPBHnd->replaytimes = pAVFmt->replaytimes;
	pPBHnd->done_callback = pAVFmt->play_done_cbfunc;
	pPBHnd->ePBHstate = ePBH_STATE_START;
}

static int RK_AVOpenHandle(S_PlaybackHnd *pPBHnd)
{
	int 		res;
	pthread_t	pthread_read	= 0;

	res = pthread_create(&pthread_read, NULL, player_begin_thread, (void *)pPBHnd);
//	pthread_join(pthread_read, NULL);
	pthread_detach(pthread_read);

	return res;
}

static int RK_AVCloseHandle(S_PlaybackHnd *pPBHnd)
{
	int pb = 0;

	pthread_mutex_lock(&pPBHnd->m_sMutexPBDone);	
	if(pPBHnd->ePBHstate == ePBH_STATE_IDLE){
		pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);
		return 0;
	}
	
	switch(pPBHnd->audio_codec){
		case AV_CODEC_ID_MP3:		
		{
			RK_MADClose_stream((S_MP3ATTR *)pPBHnd->decodec);
#if 1
			//1. first release decodec sources
			if(pPBHnd->decodec){
				free(pPBHnd->decodec);
				pPBHnd->decodec = NULL;
			}
#endif			
			pPBHnd->ePBHstate = ePBH_STATE_IDLE;
			//2. then, notice callback handle be closed
			if(pPBHnd->done_callback)
				pPBHnd->done_callback(pb, (void *)&pPBHnd->eOutputFormat);
		}
		break;
		case AV_CODEC_ID_AAC:
		{

		}
		break;
		case AV_CODEC_ID_PCM_S16LE:
		{
			RK_RAWClose_stream((S_RAWATTR *)pPBHnd->decodec);
#if 1
			//1. first release decodec sources
			if(pPBHnd->decodec){
				free(pPBHnd->decodec);
				pPBHnd->decodec = NULL;
			}
#endif			
			pPBHnd->ePBHstate = ePBH_STATE_IDLE;
			//2. then, notice callback handle be closed
			if(pPBHnd->done_callback)
				pPBHnd->done_callback(pb, (void *)&pPBHnd->eOutputFormat);
		}
		break;
		default:
			break;
	}
	
	if(pPBHnd->fileName){
		free(pPBHnd->fileName);
		pPBHnd->fileName = NULL;
	}
	pPBHnd->header_size = 0;
	pPBHnd->stoptime = 0;
	pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);

	return 0;
}

int RK_AVPauseHandle(S_PlaybackHnd *pPBHnd, bool pause)
{
	pthread_mutex_lock(&pPBHnd->m_sMutexPBDone);

	if(pPBHnd->ePBHstate == ePBH_STATE_IDLE){
		pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);
		return -1;	/* no found handle is active*/
	}

	switch(pPBHnd->audio_codec){
		case AV_CODEC_ID_MP3:
		{
			RK_MADPause_stream((S_MP3ATTR *)pPBHnd->decodec, pause);
		}
		break;
		case AV_CODEC_ID_AAC:
		{

		}
		break;
		case AV_CODEC_ID_PCM_S16LE:
		{
			RK_RAWPause_stream((S_RAWATTR *)pPBHnd->decodec, pause);
		}
		break;
		default:
			break;
	}

	pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);	
	
	return 0;
}

int RK_AVVolumeHandle(S_PlaybackHnd *pPBHnd, int f16Vol)
{
	pthread_mutex_lock(&pPBHnd->m_sMutexPBDone);

	if(pPBHnd->ePBHstate == ePBH_STATE_IDLE){
		pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);	
		return -1;	/* no found handle is active*/
	}

	switch(pPBHnd->audio_codec){
		case AV_CODEC_ID_MP3:
		{
			if(f16Vol == 0)
				RK_MADAdjust_volume((S_MP3ATTR *)pPBHnd->decodec, 1);
			else
				RK_MADAdjust_volume((S_MP3ATTR *)pPBHnd->decodec, 0.1);
		}
		break;
		case AV_CODEC_ID_AAC:
		{

		}
		break;
		case AV_CODEC_ID_PCM_S16LE:
		{
			if(f16Vol == 0)
				RK_RAWAdjust_volume((S_RAWATTR *)pPBHnd->decodec, 0.1);
			else
				RK_RAWAdjust_volume((S_RAWATTR *)pPBHnd->decodec, 1);
		}
		break;
		default:
			break;		
	}

	pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);	
	
	return 0;
}


int RK_AVWriteDataHandle(S_PlaybackHnd *pPBHnd, S_PLAYER_CMD_AUDIO_DATA *pAudioPkt)
{
	int ret = 0;
	
	pthread_mutex_lock(&pPBHnd->m_sMutexPBDone);	
	AUDIO_TRACE(" Write handle - %d - %d\n", pPBHnd->eOutputFormat, pPBHnd->ePBHstate);
	if(pPBHnd->ePBHstate == ePBH_STATE_IDLE){
		AUDIO_ERROR(" No valid handle - %d\n", pPBHnd->eOutputFormat);
		pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);
		return eAUDIO_ERRCODE_NO_VALID_RES;
	}

	switch(pPBHnd->audio_codec){
		case AV_CODEC_ID_MP3:{
//			S_MP3ATTR *pMp3Attr;
			S_AVPacket aPacket;
			
			aPacket.data = pAudioPkt->frameBuf;
			aPacket.size = pAudioPkt->u32frameSize;
			aPacket.lastpkt = pAudioPkt->u32lastpkt;

			ret = RK_MADWrite((S_MP3ATTR *)pPBHnd->decodec, &aPacket);
		}
		AUDIO_INFO(" Write handle - %d - %d\n", pPBHnd->eOutputFormat, pPBHnd->ePBHstate);
		break;
		case AV_CODEC_ID_PCM_S16LE:
		{
			S_AVPacket aPacket;
			
			aPacket.data = pAudioPkt->frameBuf;
			aPacket.size = pAudioPkt->u32frameSize;
			aPacket.lastpkt = pAudioPkt->u32lastpkt;

			ret = RK_RAWWrite((S_RAWATTR *)pPBHnd->decodec, &aPacket);
		}
		break;
		default:
			AUDIO_ERROR("No fond Write handle - %d - %d\n", pPBHnd->eOutputFormat, pPBHnd->ePBHstate);
			break;		
	}

	pthread_mutex_unlock(&pPBHnd->m_sMutexPBDone);	
	return ret;
}

AUDIO_IO *RK_AudioIO_Init(void)
{
	int i;

	if(g_PlayerContext.bInitialized == 1){
		AUDIO_WARNING("audio_io already be initialized!\n");
		return NULL;
	}
		
	g_PlayerContext.default_driver = RK_AOInit();

	if(g_PlayerContext.default_driver < 0){
		AUDIO_ERROR("ao_init is failed\n");
		return NULL;
	}
	RK_AODev_open(g_PlayerContext.default_driver, 48000);
	for(i=0; i<eOUTPUT_FORMAT_CNT; i++){
		memset(&g_PlayerContext.psPBHandle[i], 0, sizeof(S_PlaybackHnd));
		g_PlayerContext.psPBHandle[i].default_driver = g_PlayerContext.default_driver;
		if(pthread_mutex_init(&g_PlayerContext.psPBHandle[i].m_sMutexPBDone, NULL) != 0){
			AUDIO_ERROR("ERROR: Could not initialize mutex variable\n");
			return NULL;
		}
	}
	RK_MADInit();
	g_PlayerContext.bInitialized = 1;
	/* TODO */
	return &g_PlayerContext;
}

int RK_Audio_OpenHandle(AUDIO_IO *c, S_AVFormat	*psAVFmt)
{
	int ret = 0;

	S_CONTEXT_PB	*psPlayerCxt = (S_CONTEXT_PB *)c;
	
	if(RK_AVCheckHandleState(&psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat]) != ePBH_STATE_IDLE){
		AUDIO_WARNING("open handle is busying\n");
		return eAUDIO_ERRCODE_FAILED;
	}

	if(psAVFmt->fileName && (*psAVFmt->fileName)){
		psAVFmt->audio_codec = player_search_decodec_by_file(psAVFmt);
		if(psAVFmt->audio_codec == AV_CODEC_ID_NONE){
			AUDIO_WARNING("No found file decode format\n");
			return eAUDIO_ERRCODE_READ_FILE_FAILED;
		}
	}
	
	ret = RK_AVInitHandle(&psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat], psAVFmt->audio_codec);
	if(ret != 0){
		psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat].ePBHstate = ePBH_STATE_IDLE;
		return eAUDIO_ERRCODE_NO_VALID_RES;
	}
	
	RK_AVConfigHandle(&psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat], psAVFmt);

	/* make sure already initialized */
	if(psPlayerCxt->bInitialized != 1){
		psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat].ePBHstate = ePBH_STATE_IDLE;
		return eAUDIO_ERRCODE_NO_VALID_RES;
	}
	/* to open a vaild dev*/
	psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat].default_driver = psPlayerCxt->default_driver;
	RK_AVOpenHandle(&psPlayerCxt->psPBHandle[psAVFmt->eOutputFormat]);

	/* return a vaild handle E_OUTPUTFORMAT*/
	return psAVFmt->eOutputFormat;
}

int RK_Audio_CloseHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOpFmt)
{

	S_CONTEXT_PB	*psPlayerCxt = (S_CONTEXT_PB *)c;
	
	if(eOpFmt >= eOUTPUT_FORMAT_DIALOG || eOpFmt < eOUTPUT_FORMAT_CNT)
		return RK_AVCloseHandle(&psPlayerCxt->psPBHandle[eOpFmt]);
	

	return eAUDIO_ERRCODE_FAILED;
}

int RK_Audio_PauseHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, bool bPause)
{	
	S_CONTEXT_PB	*psPlayerCxt = (S_CONTEXT_PB *)c;

	if(eOPFmt < eOUTPUT_FORMAT_DIALOG || eOPFmt >= eOUTPUT_FORMAT_CNT){
		AUDIO_ERROR("invaild audio handle - %d\n", eOPFmt);
		return eAUDIO_ERRCODE_FAILED;
	}

	return RK_AVPauseHandle(&psPlayerCxt->psPBHandle[eOPFmt], bPause);
}

int RK_Audio_VolumeHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, uint8_t f16Volume)
{
	S_CONTEXT_PB	*psPlayerCxt = (S_CONTEXT_PB *)c;
	
	if(eOPFmt < eOUTPUT_FORMAT_DIALOG || eOPFmt >= eOUTPUT_FORMAT_CNT){
		AUDIO_ERROR("invaild audio handle - %d\n", eOPFmt);
		return eAUDIO_ERRCODE_FAILED;
	}
	
	return RK_AVVolumeHandle(&psPlayerCxt->psPBHandle[eOPFmt], f16Volume);
}

int RK_Audio_WriteDataHandle(AUDIO_IO *c, E_OUTPUTFORMAT eOPFmt, const void *pAVData)
{
	S_CONTEXT_PB	*psPlayerCxt = (S_CONTEXT_PB *)c;
	if(eOPFmt < eOUTPUT_FORMAT_DIALOG || eOPFmt >= eOUTPUT_FORMAT_CNT){
		AUDIO_ERROR("invaild audio handle - %d\n", eOPFmt);
		return eAUDIO_ERRCODE_FAILED;
	}
	return RK_AVWriteDataHandle(&psPlayerCxt->psPBHandle[eOPFmt], (S_PLAYER_CMD_AUDIO_DATA *)pAVData);
}


#if 0
/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int PluginMain(void *strCmdArg)
{
	int		res = 0;

	//FILE *fp;	//16384
	//S_AVPacket auPacket;

	if(RK_AudioIO_Init() != 0)
		return eAUDIO_ERRCODE_NO_VALID_RES;
	
	pause();
FAIL:
	//fclose(fp);	

	/* -- Close and shutdown -- */
	RK_AODeinit();
//	deinit_stream(&sAuPlayer);
	
	return 0;
}
#endif

#include <stdio.h>
#include <stdlib.h>

#include "player_io.h"
#include "audio_debug.h"

AUDIO_IO *g_sAv_IO = NULL;

static void player_done_callback1(int pb, void* req)
{
	printf("--------->>>>11111111111111<<<<--------\n");
#if 0
	S_PLAYER_REQ *pReq = (S_PLAYER_REQ *)req;

  	PLAYER_DBG("playerhandle=%d, funId =%d, status =%ld\n", pb, pReq->funId, pReq->status); 

	 switch(pReq->funId){
	 	case FUNCTION_ALEXA_DIALOG:{
			gs_ui8DialogDone = 1;
	 	}
		break;
	 	case FUNCTION_ALEXA_MUSIC:{
			printf("Media1:%d, Media2:%d\n", gs_iMedia1PBID, gs_iMedia2PBID);
		 	if(gs_iMedia1PBID == pb)
		     	gs_ui8Media1Done = 1;
			else if(gs_iMedia2PBID == pb)
				gs_ui8Media2Done = 1;
	 	}
		break;
		case FUNCTION_ALEXA_ALTER:{
			gs_ui8AlertDone = 1;
#ifdef OPEN_ALERT_LED
//:close LED.
#endif
		}
		break;
	 }
#endif
}

static void player_done_callback2(int pb, void* req)
{
	printf("--------->>>>2222222222222222<<<<--------\n");
#if 0
	S_PLAYER_REQ *pReq = (S_PLAYER_REQ *)req;

  	PLAYER_DBG("playerhandle=%d, funId =%d, status =%ld\n", pb, pReq->funId, pReq->status); 

	 switch(pReq->funId){
	 	case FUNCTION_ALEXA_DIALOG:{
			gs_ui8DialogDone = 1;
	 	}
		break;
	 	case FUNCTION_ALEXA_MUSIC:{
			printf("Media1:%d, Media2:%d\n", gs_iMedia1PBID, gs_iMedia2PBID);
		 	if(gs_iMedia1PBID == pb)
		     	gs_ui8Media1Done = 1;
			else if(gs_iMedia2PBID == pb)
				gs_ui8Media2Done = 1;
	 	}
		break;
		case FUNCTION_ALEXA_ALTER:{
			gs_ui8AlertDone = 1;
#ifdef OPEN_ALERT_LED
//:close LED.
#endif
		}
		break;
	 }
#endif
}


static void StreamDeliver(void *pvPriv, char *psResData, size_t datasize)
{
	//S_ALEXA_HND 	*pAlexaHnd		= (S_ALEXA_HND *)pvPriv;
	S_PLAYER_CMD_AUDIO_DATA AvsPkt;
//	int iRetValue;
	
	AvsPkt.frameBuf = (unsigned char *)malloc(datasize);
	memcpy(AvsPkt.frameBuf, (unsigned char *)psResData, datasize);
	AvsPkt.u32frameSize = datasize;
	AvsPkt.pbId = eOUTPUT_FORMAT_DIALOG;
	AvsPkt.u32lastpkt = 0;
	if(datasize < 2812)
	AvsPkt.u32lastpkt = 1;
	
	RK_Audio_WriteDataHandle(g_sAv_IO, eOUTPUT_FORMAT_DIALOG, &AvsPkt);
}

int main(int argc, char **argv)
{
//	char *path = NULL;
//	char buf[8192];
//	FILE *fp = NULL;
//	size_t	readsize;
	S_AVFormat	psAVFmt1 = {0};
	S_AVFormat	psAVFmt2 = {0};
	g_sAv_IO = RK_AudioIO_Init();
	psAVFmt1.eOutputFormat = eOUTPUT_FORMAT_SYSTIP;
	psAVFmt1.play_done_cbfunc = player_done_callback1;
	psAVFmt1.ui32sample_rate = 44100;
	psAVFmt1.playseconds = 10;
	strcpy(psAVFmt1.fileName, "/usr/lib/Music/siren.mp3");//siren.mp3
	
	psAVFmt2.eOutputFormat = eOUTPUT_FORMAT_DIALOG;//eOUTPUT_FORMAT_MEDIA1;
	psAVFmt2.play_done_cbfunc = player_done_callback2;
	psAVFmt2.ui32sample_rate = 44100;
	psAVFmt2.playseconds = 10;
	strcpy(psAVFmt2.fileName, "/usr/lib/Music/timer.pcm");//timer.pcm
	
	RK_Audio_OpenHandle(g_sAv_IO, &psAVFmt1);
	sleep(2);
	printf("------>>>>2<<<<-----\n");
	RK_Audio_OpenHandle(g_sAv_IO, &psAVFmt2);
#if 0
	sleep(2);
	while(1){
		readsize = fread(buf, 1, 8192, fp);
//			printf("readsize:%d\n", readsize);
		if(readsize>0){
//			RK_Audio_WriteDataHandle(g_sAv_IO, eOUTPUT_FORMAT_DIALOG, buf);
			StreamDeliver(g_sAv_IO, buf, readsize);
		}else{
			break;
		}
	}
#endif
	pause();

}



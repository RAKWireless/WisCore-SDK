//dec_raw.c
//by seven 2017-01-10
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "alexa_media.h"
#include "hls.h"
#include "ffplayer_dec.h"
#include "rk_string.h"
//extern S_AvsDirectives	g_astrAvsDirectives;

typedef enum {
	eMPLAY_CMD_UNKNOW,
	eMPLAY_CMD_REPLACE_ALL,
	eMPLAY_CMD_ENQUEUED,
	eMPLAY_CMD_REPLACE_ENQUEUED,
}E_MPLAY_CMD;

/* for media stream config */
int RK_APBStreamConfig(S_PBContext *h, E_PB_TYPE ePBType, E_AUDIO_MASK ePBFmx)
{
	S_AudioHnd	*pAudioHnd = &h->asAhandler[ePBType];

	if(pAudioHnd->ePlaybackState != ePB_STATE_IDLE){
		return 0;
	}
		
	pAudioHnd->eAudioMask = ePBFmx;
	return RK_APBOpen(h, ePBType);
}

static void RK_MediaStreamPut(S_MediaFormat *psMf, char *stream_au, size_t stream_size)
{
	//if(psMf->fCallBackDeliverMediaStream)
		//psMf->fCallBackDeliverMediaStream((void *)psMf, stream_au, &stream_size);
	S_PBContext *pPBCxt = (S_PBContext *)psMf->priv_data;

	if((pPBCxt == NULL) || (pPBCxt->asAPlayback == NULL)){
		ALEXA_WARNING("Nothing handle media stream!\n");
		return;
	}

#if 0
	fwrite(stream_au, 1, stream_size, directfp1);
	printf("revice len:%ld\n", stream_size);
#else
	RK_APBWrite(pPBCxt, ePB_TYPE_MEDIA1, (unsigned char*)stream_au, stream_size);
#endif
	/*TODO*/
}

size_t RK_MediaBodyDataFromAvsCB(char *pAvsData, size_t size, size_t nitems, void *stream_priv)
{
	S_MediaFormat *pMediaFormat = (S_MediaFormat *)stream_priv;
	int len = 0;
	
	if(pMediaFormat->eMediaType == eMEDIA_TYPE_UNKNOW){
		//char *nested_type;
			return size*nitems;
	}
	
	switch(pMediaFormat->eMediaType){
		case eMEDIA_TYPE_M3U:
		case eMEDIA_TYPE_M3U8:
		case eMEDIA_TYPE_PLS:
			RK_HLS_MediaParsePlaylist(pMediaFormat, pAvsData);
		break;
		case eMEDIA_TYPE_MP3:
		{
			S_PBContext *pPBCxt = (S_PBContext *)pMediaFormat->priv_data;
			int ret;
			ret = RK_APBStreamConfig(pPBCxt, ePB_TYPE_MEDIA1, eAUDIO_MASK_MP3);
			if(ret != 0){
				ALEXA_ERROR("Media isnot be open!\n");
			}else{ /* change media state to playing */
				RK_HLS_MediaChangeState(pMediaFormat, eMEDIA_STATE_PLAYING);
			}
			
			RK_MediaStreamPut(pMediaFormat, pAvsData, size*nitems);
		}
		break;
		case eMEDIA_TYPE_AAC:
		{
			#if 1
			S_PBContext *pPBCxt = (S_PBContext *)pMediaFormat->priv_data;
			int ret;
			ret = RK_APBStreamConfig(pPBCxt, ePB_TYPE_MEDIA1, eAUDIO_MASK_AAC);
			if(ret != 0){
				ALEXA_ERROR("Media isnot be open!\n");
			}else{ /* change media state to playing */
				RK_HLS_MediaChangeState(pMediaFormat, eMEDIA_STATE_PLAYING);
			}
			
			RK_MediaStreamPut(pMediaFormat, pAvsData+len, (size*nitems-len));
			#endif
		}
		break;
		default:{
			ALEXA_INFO("MediaFormat is not support!\n");
		}
		break;
	}

	return size*nitems;
}


size_t RK_MediaHeaderDataFromAvsCB(char *pAvsData, size_t size, size_t nitems, void *stream_priv)
{
	S_MediaFormat *pMediaFormat = (S_MediaFormat *)stream_priv;
	
	printf("len:%d, %s\n", size*nitems, pAvsData);

	RK_HLS_MediaType(pMediaFormat, pAvsData);

    return size*nitems;
}

static void RK_DoMediaInteractive(S_ALEXA_RES *psAlexaRes, char *stream_url)
{
	S_AVS_CURL_SET *pAvsCurlSet = &psAlexaRes->asAvsCurlSet;

	printf("**url:%s**\n", stream_url);
	
	RK_AVSSetUrl(pAvsCurlSet->asCurlSet[eCURL_HND_MEDIA].curl, stream_url);
	RK_AVSSetCurlReadAndWrite(pAvsCurlSet->asCurlSet[eCURL_HND_MEDIA].curl, \
							NULL, NULL, \
							RK_MediaBodyDataFromAvsCB, (void *)&psAlexaRes->asMediaFormat, \
							RK_MediaHeaderDataFromAvsCB, (void *)&psAlexaRes->asMediaFormat);

	RK_AVSCurlRequestSubmit(&pAvsCurlSet->asCurlSet[eCURL_HND_MEDIA], 1);

}

void *RK_MediaStreamThread(void *arg)
{
	S_ALEXA_RES 	*psAlexaRes = (S_ALEXA_RES *)arg;
	S_MediaFormat 	*psMf = &psAlexaRes->asMediaFormat;
	S_HLSContext	*pHlsCxt = &psMf->sHlsCxt;
	
	S_PBContext		*pPBCxt = NULL;
	S_AudioHnd		*pPBHnd = NULL;
	
	S_PlayDirective *pPlay = (S_PlayDirective *)psAlexaRes->sAvsDirective;
	E_TYPE_URI		eHlsTypeUri = eTYPE_URI_UNKNOW;
	int 	ret = 0,	responecode = 0;
	
	/* it will play the song name prompt speech until 
	*  the previous song play is completed if there is
	*  song name promt speech
	*/
	printf("#######################################\n");
	//ALEXA_DBG("MediaID:%d\n", ePB_TYPE_MEDIA1);
	if(psAlexaRes->eAlexaResState & eALEXA_MEDIA_PROMPT){
		/* wait promt speech finished */
		RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_DIALOG, 0);
		psAlexaRes->eAlexaResState &= ~eALEXA_MEDIA_PROMPT;
	}
	/* Get a vaild Media playback handler - block*/
	ret = RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_MEDIA1, 0);
	ALEXA_DBG("GET MediaHandle State - %d\n", ret);
	pPlay->eMplayState = eMPLAY_STATE_PLAYING;
//	ret = RK_APBGetMediaHandler(psAlexaRes->psPlaybackCxt, ePB_TYPE_MEDIA1);
	if(ret != 0){
		if(psAlexaRes->psPlaybackCxt->asAhandler[ePB_TYPE_MEDIA1].ePlaybackState != ePB_STATE_IDLE)
			return NULL;
	}
	
	psMf->priv_data = (void *)psAlexaRes->psPlaybackCxt;
	pPBCxt = psAlexaRes->psPlaybackCxt;
	pPBHnd = &pPBCxt->asAhandler[ePB_TYPE_MEDIA1];
	
	/* 1. we need to parse the new url if it is not supported by libnmedia*/
	eHlsTypeUri = RK_HLS_check_types_uri(pPlay->strUrl);
//	printf("----------------------------------------------\n");
	
	do{
		S_Streamurl urlpkt;
	/* 2. find a vaild url form playlist buffer */	
		if(RK_HLS_Find_Playlist(pHlsCxt, &urlpkt) == 0){
			eHlsTypeUri = urlpkt.m_eTypeUri;
			memcpy(pPlay->strUrl, urlpkt.url, urlpkt.size);
			pPlay->strUrl[urlpkt.size] = '\0';
			
		}else if(pHlsCxt->pRedirectUrl != NULL){
			eHlsTypeUri = urlpkt.m_eTypeUri;
			memcpy(pPlay->strUrl, pHlsCxt->pRedirectUrl, strlen(pHlsCxt->pRedirectUrl)+1);	/*we need cp '\0', so plus one */
			free(pHlsCxt->pRedirectUrl);
			pHlsCxt->pRedirectUrl = NULL;
		}
	/* 3. grab stream data or request and parse a new stream uri */
		if(eHlsTypeUri == eTYPE_URI_STREAM){
			int seek_pos;
			/* set encode format */
			pPBHnd->eAudioMask = eAUDIO_MASK_PCM;
			seek_pos = pPlay->offsetInMilliseconds/1000;
			ALEXA_DBG("offsetMillsec - %ld, Seek - %d\n", pPlay->offsetInMilliseconds, seek_pos);
			responecode = RK_FFLiveStreamHandle((void *)pPBCxt, (int)ePB_TYPE_MEDIA1, pPlay->strUrl, seek_pos, NULL);
			//RK_APBStream_write_callback

		}else{
			RK_DoMediaInteractive(psAlexaRes, pPlay->strUrl);
			/* wait response state of grap media stream */
			responecode = RK_AVSWaitRequestCompleteCode(&psAlexaRes->asAvsCurlSet.asCurlSet[eCURL_HND_MEDIA]);
		}
		
		ALEXA_DBG("Media Httpcode:%d\n", responecode);
		
		/*set laster packet to specify interaction finish*/
		if(pPBHnd->ePlaybackState != ePB_STATE_IDLE){
			pPBHnd->blast_packet = 1;
			if(pPBCxt->asAPlayback){
				ALEXA_TRACE("Insert the last packet of media stream.\n");
				RK_APBWrite(pPBCxt, ePB_TYPE_MEDIA1, NULL, 1);
				pPBHnd->blast_packet = 0;
			}
		}
		
		/* wait media finished */
		RK_APBFinished(psAlexaRes->psPlaybackCxt, ePB_TYPE_MEDIA1, 0);

		/* judge stop directive*/
		if(pPlay->eMplayState == eMPLAY_STATE_STOPPED || responecode == 1)
			return NULL;

	}while((pHlsCxt->n_segments >0) || (pHlsCxt->n_variants > 0) || (pHlsCxt->pRedirectUrl != NULL));

	ALEXA_INFO("replace_behavior:%d\n", psAlexaRes->replace_behavior);
	if(psAlexaRes->replace_behavior){
		RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_REPLACE);
		return NULL;
	}

	if(psMf->eMediaState != eMEDIA_STATE_IGNORE){
	/* commit a PlaybackNearlyFinished Event*/
	RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_NEARLY_FINISHED);
	}
	
	/* commit a PlaybackFinished Event */
	RK_AVSSetDirectivesEventID(eAVS_DIRECTIVE_EVENT_PLAYBACK_FINISHED);
	
	return NULL;
}

int RK_AudioPlayerRequestMedia(S_ALEXA_RES *psAlexaRes, S_PlayDirective *psPlay)
{
	E_MPLAY_CMD	eMplayCmd = eMPLAY_CMD_UNKNOW;
	
	int ret = 0;
	
	if(psPlay == NULL){
		ALEXA_ERROR("No valid S_PlayDirective <NULL>\n");
		return eALEXA_ERRCODE_NO_VALID_RES;
	}

	if(strncmp(psPlay->strPlayBehavior, "REPLACE_ALL", 11) == 0){
		eMplayCmd = eMPLAY_CMD_REPLACE_ALL;
	}else if(strncmp(psPlay->strPlayBehavior, "ENQUEUE", 7) == 0){
		eMplayCmd = eMPLAY_CMD_ENQUEUED;
	}else if(strncmp(psPlay->strPlayBehavior, "REPLACE_ENQUEUED", 16) == 0){
		eMplayCmd = eMPLAY_CMD_REPLACE_ENQUEUED;
	}else{
		ALEXA_ERROR("AVS playBehavior isn't match!\n");
		ret = eALEXA_ERRCODE_FAILED;
	}

	switch(eMplayCmd){
		case eMPLAY_CMD_REPLACE_ALL:
		{
			const char *nested_url;
			S_MediaFormat *psMf = &psAlexaRes->asMediaFormat;
			S_PBContext	  *pPBCxt = psAlexaRes->psPlaybackCxt;
			ALEXA_TRACE("eMediaState:%d, Media PB Type:%d\n", psMf->eMediaState, ePB_TYPE_MEDIA1);		
			/* Immediately to stop pb if current playstate is no idle */
			if(psMf->eMediaState == eMEDIA_STATE_PLAYING){		
				RK_APBClose(pPBCxt, ePB_TYPE_MEDIA1);
				RK_APBClose(pPBCxt, ePB_TYPE_DIALOG);
				RK_HLS_MediaChangeState(psMf, eMEDIA_STATE_STOPED);
			}
			
			if(RK_Strstart(psPlay->strUrl, "http://", &nested_url)){
				RK_HLS_Install(&psMf->sHlsCxt);
			}else if(RK_Strstart(psPlay->strUrl, "https://", &nested_url)){
				RK_HLS_Install(&psMf->sHlsCxt);
			}else{
				ALEXA_TRACE("REPLACE_ALL URL is CID!\n");
				ret = eALEXA_ERRCODE_NO_VALID_RES;		/**/
			}
		}
		break;
		case eMPLAY_CMD_ENQUEUED:
		{
			const char *nested_url;
			if(RK_Strstart(psPlay->strUrl, "http://", &nested_url)){
			//	do_request = 1;
			}else if(RK_Strstart(psPlay->strUrl, "https://", &nested_url)){
			//	do_request = 1;

			}else{
				ALEXA_TRACE("ENQUEUED URL is CID!\n");
				ret = eALEXA_ERRCODE_NO_VALID_RES;		/**/
			}
		}
		break;
		case eMPLAY_CMD_REPLACE_ENQUEUED:
		{
			S_PBContext	  *pPBCxt = psAlexaRes->psPlaybackCxt;
			
			if(pPBCxt->asAhandler[ePB_TYPE_MEDIA1].ePlaybackState == ePB_STATE_IDLE)
				ret = 0;
			else
				ret = 1;		/**/			

			ALEXA_DBG("#### REPLACE_ENQUEUED - <%s> ###!\n", (ret > 0)?"Enqueue":"Immediatly");
			/* TODO */
		}
		break;
		default:
			break;
	}

	return ret;
}



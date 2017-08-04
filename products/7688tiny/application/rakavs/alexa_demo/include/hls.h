//
/*  
 * hls.h 2016/08/24 , rak University , China
 * 	Author: Sevencheng
*/  
#ifndef __HLS_H___
#define __HLS_H___
#include <pthread.h>
#include <stdint.h>

#define URL_SCHEME_CHARS                        \
    "abcdefghijklmnopqrstuvwxyz"                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                \
    "0123456789+-."


#define MAX_URL_SIZE	512
/**
 * Internal time base represented as integer
 */

#define AV_TIME_BASE            1000000

typedef enum {
	eTYPE_URI_UNKNOW,
	eTYPE_URI_STREAM,		/* it is stream uri that can direct grab data */
	eTYPE_URI_SHOUTCAST,	/* shoutcast protocol url -> m3u */
	eTYPE_URI_PLS			/* playlist protocol file */
}E_TYPE_URI;

typedef enum {
	eMEDIA_TYPE_UNKNOW,
	eMEDIA_TYPE_MP3,
	eMEDIA_TYPE_AAC,
	eMEDIA_TYPE_TS,
	eMEDIA_TYPE_M3U,
	eMEDIA_TYPE_M3U8,
	eMEDIA_TYPE_PLS,
	eMEDIA_TYPE_REDIRECT
}E_MEDIA_TYPE;

typedef enum {
	eMEDIA_STATE_IDLE,		//no use
	eMEDIA_STATE_PLAYING,
	eMEDIA_STATE_PAUSE,
	eMEDIA_STATE_STOPED,
	eMEDIA_STATE_IGNORE,
}E_MEDIA_STATE;

typedef struct stream_url{
	E_TYPE_URI m_eTypeUri;
	char url[MAX_URL_SIZE];
	int	size;
	int64_t duration;
	int bandwidth;
}S_Streamurl;

typedef struct PlsUrl{
	S_Streamurl stream_url;
	struct PlsUrl *next;
}S_PlsUrl;

typedef struct Playlisturl{
	S_PlsUrl *first_pkt, *last_pkt;
	int		nb_lists;
	pthread_mutex_t 	mutex;
}S_Playlisturl;

typedef struct HLSContext {
	int m3uexist;			/* 0 - m3u, 1 - m3u8 */
	int	finished;
	int64_t target_duration;
	int start_seq_no;
	int n_segments;
	S_Playlisturl segments;
	int n_variants;
    S_Playlisturl variants;
	char *pRedirectUrl;			/* redirect url for httpcode 302*/
}S_HLSContext;

typedef void (*MediaStreamDeliverCB)(void *pvPriv, char *psResData, size_t *datasize);

typedef struct MediaFormat {
	S_HLSContext 	sHlsCxt;
	E_MEDIA_TYPE 	eMediaType;
	E_MEDIA_STATE	eMediaState;
	unsigned char 	ui32ContentLength;
	void			*priv_data;
	MediaStreamDeliverCB fCallBackDeliverMediaStream;
}S_MediaFormat;

void RK_HLS_Install(S_HLSContext *psHLSCxt);
E_TYPE_URI RK_HLS_check_types_uri(char *pUri);
int RK_HLS_Find_Playlist(S_HLSContext *psHlsCxt, S_Streamurl *url);
int RK_HLS_Insert_Playlist(S_HLSContext *psHlsCxt, S_Streamurl *url, int variants);
void RK_HLS_MediaChangeState(S_MediaFormat *psMFmt, E_MEDIA_STATE	eMediaState);
void RK_HLS_MediaType(S_MediaFormat *psMFmt, char *pData);
int RK_HLS_MediaParsePlaylist(S_MediaFormat *psMFmt, char *pData);

#endif//__HLS_H___

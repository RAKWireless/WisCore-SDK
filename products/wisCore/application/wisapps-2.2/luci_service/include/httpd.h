/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/
#include "RKLog.h"

#define IO_BUFFER 256
#define BUFFER_SIZE 1024

/* the boundary is used for the M-JPEG stream, it separates the multipart stream of pictures */
#define BOUNDARY "boundarydonotcross"

/*
 * this defines the buffer size for a JPG-frame
 * selecting to large values will allocate much wasted RAM for each buffer
 * selecting to small values will lead to crashes due to to small buffers
 */
#define MAX_FRAME_SIZE (256*1024)
#define TEN_K (10*1024)
#define DEF_PARTIAL_SIZE		1024 * 1024
#define DEF_FIRMWARE_OTA_SIZE   16*DEF_PARTIAL_SIZE
#define DEF_FIRMWARE_OTA_PATH   "/tmp/fwupgrade"

#define OUTPUT_PREFIX " o: "

#define OPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", OUTPUT_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

#define DEBUG
#ifdef DEBUG
#define DBG(...) 	fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#define INFO(...)	{ fprintf(stderr, " INFO(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define DBG(...)
#define INFO(...)
#endif

#if 1
	#define ERROR(...)		{ fprintf(stderr, " ERROR(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
	#define ERROR(...)		{;}
#endif

//#define LOG(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }


/*
 * Standard header to be send along with other header information like mimetype.
 *
 * The parameters should ensure the browser does not cache our answer.
 * A browser should connect for each file and not serve files from his cache.
 * Using cached pictures would lead to showing old/outdated pictures
 * Many browser seem to ignore, or at least not always obey those headers
 * since i observed caching of files from time to time.
 */
#define STD_HEADER "Connection: close\r\n" \
    "Server: rakwireless/0.2\r\n" \
    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
    "Pragma: no-cache\r\n" \
    "Expires: 0\r\n"


#define DEF_JSON_HEADER "HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: application/json\r\n\r\n"
#define POST_STATE(buf, x) sprintf(buf,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", x)
/*
 * Maximum number of server sockets (i.e. protocol families) to listen.
 */
//#define MAX_SD_LEN 50

/*
 * Only the following fileypes are supported.
 *
 * Other filetypes are simply ignored!
 * This table is a 1:1 mapping of files extension to a certain mimetype.
 */
static const struct {
    const char *dot_extension;
    const char *mimetype;
} mimetypes[] = {
    { ".html", "text/html" },
    { ".htm",  "text/html" },
    { ".css",  "text/css" },
    { ".js",   "text/javascript" },
    { ".txt",  "text/plain" },
    { ".jpg",  "image/jpeg" },
    { ".jpeg", "image/jpeg" },
    { ".png",  "image/png"},
    { ".gif",  "image/gif" },
    { ".ico",  "image/x-icon" },
    { ".swf",  "application/x-shockwave-flash" },
    { ".cab",  "application/x-shockwave-flash" },
    { ".jar",  "application/java-archive" },
    { ".json", "application/json" }
};

/* the webserver determines between these values for an answer */
typedef enum answer_t {
    A_UNKNOWN,
    A_SNAPSHOT,
    A_STREAM,
    A_COMMAND,
    A_DEV_NAME,
    A_DEV_GROUP,
    A_FILE,
    A_INPUT_JSON,
    A_OUTPUT_JSON,
    A_PROGRAM_JSON,
	A_UPGRADE,
	A_CH_WIRELESS,
	A_WIFI_SCAN,
	A_REST_BOARD,
	A_RESTART,
	A_RESTART_NET,
	A_WIFI_CONFIG,
	A_WIFI_STATUS,
	A_LOGIN_GET_DATA,
	A_LOGIN_SET_DATA,
	A_LOGOUT,
	A_OPEN_TUNNEL,
	A_CLOSE_TUNNEL,
	A_SET_BAUDRATE,
	A_GET_BAUDRATE,
	A_SET_TUNNEL_PORT,
	A_GET_TUNNEL_PORT,
	A_GET_VERSION,
	A_IGNORE,
	A_CH_LANGUAGE,
	A_GET_LANGUAGE,
} E_ANS_DATA;

enum{
	eHTTP_ERRCODE_ANOMALY_RESAVEFW		= -31,
	eHTTP_ERRCODE_UNKNOW_ERROR		= -32,
	eHTTP_ERRCODE_CHKSUM_FAILED			= -33,
	eHTTP_ERRCODE_LACK_SPACE			= -34,
	eHTTP_ERRCODE_VERSION_ERROR			= -35,
};

/*
 * the client sends information with each request
 * this structure is used to store the important parts
 */
typedef struct request {
    E_ANS_DATA 	m_eDataType;
    char 		*m_strParameter;
    char 		*m_strClient;
    char 		*credentials;
} S_REQUEST;

/* the iobuffer structure is used to read from the HTTP-client */
typedef struct {
    int level;              /* how full is the buffer */
    char buffer[IO_BUFFER]; /* the data */
} iobuffer;

/* store configuration for each server instance */
typedef struct {
    int 			m_u32Port;
    char 			*credentials;
    char 			*www_folder;
    uint16_t 		nocommands;
	uint16_t		m_u16Timeout;
	uint16_t		m_u16ConnLimitCnt;
} config;

/* context of each server thread */
typedef struct {
    int 	sd;	//[MAX_SD_LEN];
    int 	id;
   // S_MSF_GLOBALS *pglobal;		//remove
    pthread_t threadID[2];

	int msg_id;
	rak_log_t httplog;

    config conf;
} context;

/*
 * this struct is just defined to allow passing all necessary details to a worker thread
 * "cfd" is for connected/accepted filedescriptor
 */
typedef struct {
    context *m_psContext;
    int 	m_iConnFd;
	BOOLEAN m_bNoHeader;
} cfd;


#define UCI_WIRELESS_CONFIG "/etc/config/wireless"
#define AP_WIFISTATUS "/tmp/ap_wifistatus.conf"
#define STA_WIFISTATUS "/tmp/sta_wifistatus.conf"
#define WIFILIST_CONFIG "/tmp/wifilist.conf"
#define VERSION_CONFIG "/etc/wiskey/version"

pthread_mutex_t g_hMutex;
/*typedef struct{
	char ap_channel[8];

}uci_wifi_config;
*/
/* prototypes */
void *server_thread(void *arg);
void send_error(int fd, int which, char *message);
void send_Output_JSON(int fd, int plugin_number);
void send_Input_JSON(int fd, int plugin_number);
void send_Program_JSON(int fd);
//char * Save_licenseId(char *filePath, char *parameterName, char*parameterVal); //set licenseID


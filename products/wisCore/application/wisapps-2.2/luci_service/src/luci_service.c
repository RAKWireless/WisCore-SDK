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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>

#include "utils.h"
#include "httpd.h"
#include "updateconfigfile.h" //gz
#include "RKIoctrlDef.h"
#include "RKLog.h"


#define OUTPUT_PLUGIN_NAME "HTTP output plugin"
#define DEF_HTTP_VERSION		1
#define eHTTP_DEFAULT_PORT 80
/*
 * keep context for each server
 */

context g_sServer ={//[MAX_INPUT_PLUGINS];
	.conf = {
		.credentials 		= NULL,
		.www_folder  		= NULL,
		.m_u32Port			= eHTTP_DEFAULT_PORT,
		.nocommands			= FALSE,
		.m_u16Timeout		= 1,
		.m_u16ConnLimitCnt	= 32
		},
};	
/******************************************************************************
Description.: print help for this plugin to stdout
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-w | --www ]...........: folder that contains webpages in \n" \
            "                           flat hierarchy (no subfolders)\n" \
            " [-p | --port ]..........: TCP port for this HTTP server\n" \
            " [-c | --credentials ]...: ask for \"username:password\" on connect\n" \
            " [-n | --nocommands ]....: disable execution of commands\n"
            " ---------------------------------------------------------------\n");
}
static inline void reset_getopt(void)
{
    /* optind=1; opterr=1; optopt=63; */
#ifdef __GLIBC__
    optind = 0;
#else
    optind = 1;
#endif

#ifdef HAVE_OPTRESET
    optreset = 1;
#endif
}
/*** plugin interface functions ***/
/******************************************************************************
Description.: Initialize this plugin.
              parse configuration parameters,
              store the parsed values in global variables
Input Value.: All parameters to work with.
              Among many other variables the "param->id" is quite important -
              it is used to distinguish between several server instances
Return Value: 0 if everything is OK, other values signal an error
******************************************************************************/
int PluginInit(int argc, char **argv)
{
    int i;
    //DBG("output #%02d\n", param->id);

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            { "h", 				no_argument, 		0, 0},
            { "help", 			no_argument, 		0, 0},
            { "p", 				required_argument, 	0, 0},
            { "port", 			required_argument, 	0, 0},
            { "c", 				required_argument, 	0, 0},
            { "credentials", 	required_argument, 	0, 0},
            { "w", 				required_argument, 	0, 0},
            { "www", 			required_argument, 	0, 0},
            { "n", 				no_argument, 		0, 0},
            { "nocommands", 	no_argument, 		0, 0},
            { "t", 				required_argument, 	0, 0},
            { 0, 0, 0, 0}
        };

        c = getopt_long_only(argc, argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        switch(option_index) {
            /* h, help */
        case 0:
        case 1:
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 0,1\n");
            help();
            return 1;
            break;

            /* p, port */
        case 2:
        case 3:
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 2,3 %s\n", optarg);
            g_sServer.conf.m_u32Port = atoi(optarg);
            break;

            /* c, credentials */
        case 4:
        case 5:
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 4,5\n");
            g_sServer.conf.credentials = strdup(optarg);
            break;

            /* w, www */
        case 6:
        case 7:
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 6,7\n");
            g_sServer.conf.www_folder = malloc(strlen(optarg) + 2);
            strcpy(g_sServer.conf.www_folder, optarg);
            if(optarg[strlen(optarg)-1] != '/')
                strcat(g_sServer.conf.www_folder, "/");
            break;

            /* n, nocommands */
        case 8:
        case 9:
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 8,9\n");
            g_sServer.conf.nocommands =  atoi(optarg) ? TRUE : FALSE;
            break;
		case 10:
			LOG_P(g_sServer.httplog,RAK_LOG_FINE,"case 10\n");
			g_sServer.conf.m_u16Timeout = atoi(optarg);
			break;
		default:
			LOG_P(g_sServer.httplog,RAK_LOG_FINE,"default case\n");
			help();
			return 1;
        }
    }

   // g_sServer.id = param->id;
    //g_sServer.pglobal = param->global;
	g_sServer.conf.m_u32Port = htons(g_sServer.conf.m_u32Port);
	//g_sPluginIf.m_bInitialized = TRUE;

    OPRINT("www-folder-path...: %s\n", (g_sServer.conf.www_folder == NULL) ? "disabled" : g_sServer.conf.www_folder);
    OPRINT("HTTP TCP port.....: %d\n", g_sServer.conf.m_u32Port);
    OPRINT("username:password.: %s\n", (g_sServer.conf.credentials == NULL) ? "disabled" : g_sServer.conf.credentials);
    OPRINT("commands..........: %s\n", (g_sServer.conf.nocommands) ? "disabled" : "enabled");
    return 0;
}

void *wifistatus_thread()
{

	FILE *wfp,*nfp;
	char *iBytes;
	char *ptr, 
		 pReadBuffer[128];
	SMsgIoctrlNatState netstatus;
	int i;
	int snd_first = 0;
	int old_flag = 0;
	int new_flag = 1;
	int opt_flag = 0;

	while(1){

		pthread_mutex_lock(&g_hMutex);
		load_config(UCI_WIRELESS_CONFIG,g_sServer.httplog);
		const char *wifi_option = search_config("radio0","linkit_mode");
		if(wifi_option == NULL)
		{
			LOG_P(g_sServer.httplog,RAK_LOG_FATAL,"/etc/config/wireless no linkit_mode_opt\n");
			break;
		}

		if(!strcmp(wifi_option ,"sta")){
			if((wfp = popen("iwconfig apcli0","r")) == NULL)
			{
				netstatus.restult = -1;
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"can't popen iwconfig\n");
			}
			while((iBytes = fgets(pReadBuffer,sizeof(pReadBuffer),wfp)) != NULL)
			{

				if((ptr = strstr(pReadBuffer,"ESSID:")) != NULL)
				{
					char *p;
					ptr += strlen("ESSID:") + 1;
					strcpy(pReadBuffer,ptr);
					if(p = strstr(pReadBuffer,"\""))
						*p = '\0';
					if(!strcmp(pReadBuffer,"")){
						if((!strcmp(wifi_option,"sta")) && (opt_flag == 0)){
							i = 1;
							RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_SLOWFLASHING);
							opt_flag = 1;
						}
						netstatus.restult = 1;
						new_flag = 1;

					}else{

						if((!strcmp(wifi_option,"sta")) && (opt_flag == 1))
						{
							i = 0;
							RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_ON);
							opt_flag = 0;
						}
						netstatus.restult = 0;
						new_flag = 0;
						break;
					}
				}
			}
			pclose(wfp);
		}else if(!strcmp(wifi_option ,"ap")){
			if((nfp = popen("swconfig dev rt305x port 0 show","r")) == NULL)
			{
				netstatus.restult = -1;
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"can't popen swconfig\n");
			}
			while((iBytes = fgets(pReadBuffer,sizeof(pReadBuffer),nfp)) != NULL)
			{
				if((ptr = strstr(pReadBuffer,"link: port:0 link:down")) != NULL){
					netstatus.restult = 1;
					new_flag = 1;

				}else if((ptr = strstr(pReadBuffer,"link: port:0 link:up")) != NULL){
					netstatus.restult = 0;
					new_flag = 0;

				}
			}
			pclose(nfp);
		}

		if(snd_first == 0){
			if(!strcmp(wifi_option,"sta")){
				RK_SndIOCtrl(g_sServer.msg_id, &netstatus, sizeof(SMsgIoctrlNatState), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_NATSTATE);
				old_flag = new_flag;
			}else if(!strcmp(wifi_option,"ap")){
				RK_SndIOCtrl(g_sServer.msg_id,&netstatus,sizeof(SMsgIoctrlNatState),eIOTYPE_USER_MSG_AVS,eIOTYPE_MSG_PHYSTATE);
				RK_SndIOCtrl(g_sServer.msg_id, &netstatus, sizeof(SMsgIoctrlNatState), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_NATSTATE);
				old_flag = new_flag;
			
			}
			snd_first = 1;
		}

		if(old_flag != new_flag){
			if(!strcmp(wifi_option,"sta")){
				RK_SndIOCtrl(g_sServer.msg_id, &netstatus, sizeof(SMsgIoctrlNatState), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_NATSTATE);
				old_flag = new_flag;
			}else if(!strcmp(wifi_option,"ap")){
				RK_SndIOCtrl(g_sServer.msg_id,&netstatus,sizeof(SMsgIoctrlNatState),eIOTYPE_USER_MSG_AVS,eIOTYPE_MSG_PHYSTATE);
				RK_SndIOCtrl(g_sServer.msg_id, &netstatus, sizeof(SMsgIoctrlNatState), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_NATSTATE);
				old_flag = new_flag;
			
			}
		}
		unload_config();
		pthread_mutex_unlock(&g_hMutex);

		
		sleep(5);

		}
	return NULL;
}

/******************************************************************************
Description.: This creates and starts the server thread
Input Value.: id determines which server instance to send commands to
Return Value: always 0
******************************************************************************/
int main(int argc, char **argv	)
{
   // DBG("launching server thread #%02d\n", id);

	g_sServer.httplog = rak_log_init("LUCI_SERVICE",RAK_LOG_FINE,8,NULL,NULL);

	g_sServer.msg_id = RK_MsgInit();

	if(PluginInit(argc, argv) != 0)
		return 0;

	pthread_mutex_init(&g_hMutex,NULL);


    /* create thread and pass context to thread function */
    pthread_create(&(g_sServer.threadID[0]), NULL, server_thread, &(g_sServer));
    pthread_create(&(g_sServer.threadID[1]), NULL, wifistatus_thread, NULL);
    //pthread_detach(g_sServer.threadID);

	sleep(3);
	RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_QUICKFLASHING);
    
	pthread_join(g_sServer.threadID[0], 0);
    pthread_join(g_sServer.threadID[1], 0);

	pthread_mutex_destroy(&g_hMutex);

    return 0;
}



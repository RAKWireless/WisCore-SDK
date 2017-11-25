//------------------------------------------------------------------
//
// Copyright (c) Rakwireless Technology Corp. All rights reserved.
//
//------------------------------------------------------------------

#ifndef ___PLUGIN_ALEXA___
#define ___PLUGIN_ALEXA___

#define DEF_ALEXA_VERSION			1
#define ALEXA_MAX_HANDLES         		2       /* max process handle event stream and downchannel */
/* set avs releative configurtion files path */


#define	DEF_USR_BIND_DEVICE_CONF_FILE_NAME 		"/etc/wiskey/alexa.conf"
#define DEF_PRODUCT_ID 				    		"my_device"
#define DEF_ALERT_CONF_FILE_NAME 	    		"/etc/wiskey/alert_struct.conf"
#define DEF_ALERT_CONF_FILE_NAME_TMP    		"/tmp/alert_struct.conf.new"
#define DEFAULT_ALERT_FILE_ALARM 				"/usr/lib/sound/alarm"
#define DEFAULT_ALERT_FILE_TIMER 				"/usr/lib/sound/timer"
#define DEF_DEVICE_INFO_PATH 					"/etc/wiskeyinfo"

#define DEF_TLS_SERVICE_PORT 					9017
#define DEF_TLS_SERVICE_DOMAINNAME 				"license.cn-test-2.hyiot.io"
#define DEF_TLS_CERTNAME 						"/etc/ssl/certs/ca.crt"
#define DEF_TLS_CLIENT_CERTNAME 				"/etc/ssl/certs/dlsclient.crt"
#define DEF_TLS_CLIENT_KEYNAME 					"/etc/ssl/certs/dlsclient.key"
#define DEF_USER_ID 							"8b3f557e"
#define DEF_LICENSE 							"-----------------" /*provided by rak*/

#define STOP_NEXT_TIME 20
#define ALERT_TIME_OUT 10*60 //default 30*60


typedef enum _sysaudiotipscode{
	eSYSTIPSCODE_NONE = -1,
	eSYSTIPSCODE_ALERT_FAIL = 0,
	eSYSTIPSCODE_ALEXA_NOETH,		/* no ethertnet */
	eSYSTIPSCODE_ALEXA_NOITN,		/* no internet */
	eSYSTIPSCODE_ALEXA_NOUSR,
	eSYSTIPSCODE_ALEXA_TROBL,
	eSYSTIPSCODE_ALEXA_LOGINING,
	eSYSTIPSCODE_ALEXA_CAPTURE_START,
	eSYSTIPSCODE_ALEXA_CAPTURE_STOP,
	eSYSTIPSCODE_ALEXA_MUTE_MIC,
	eSYSTIPSCODE_ALEXA_UNMUTE_MIC,
	eSYSTIPSCODE_NUM,
} E_SYSTIPSCODE;

typedef enum _sysstate{
	eSYSSTATE_ALL,
	eSYSSTATE_ALERTTIMEUP_NUMADD,
	eSYSSTATE_ALERTALL_NUMADD,
	eSYSSTATE_ALERTTIME,
	eSYSSTATE_ETHERNET,
	eSYSSTATE_NETWORK,
	eSYSSTATE_ALEXA,
} E_SYSSTATECODE;

#endif	//___PLUGIN_ALEXA___

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

#define STOP_NEXT_TIME 20
#define ALERT_TIME_OUT 10*60 //default 30*60



#endif	//___PLUGIN_ALEXA___

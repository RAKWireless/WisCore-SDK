/*
 * RKIOCTRLDEFs.h
 *	Define IOCTRL Message Type and Context
 *  Created on: 2017-03-09
 *  Author: SEVEN
 */

#ifndef _RKIOCTRL_DEF_H_
#define _RKIOCTRL_DEF_H_

#define MSGKEY 8899 
/////////////////////////////////////////////////////////////////////////////////
/////////////////// Message Type Define//////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// IOCTRL Message Type
typedef enum 
{
	eIOTYPE_USER_MSG_HTTPD 				= 0x0100,
	eIOTYPE_USER_MSG_AVS	 			= 0x0200,
	eIOTYPE_USER_MSG_TUNNEL 			= 0x0300,
	eIOTYPE_USER_MSG_ZWAVE				= 0x0400,
	eIOTYPE_USER_MSG_GPIO_WLED			= 0x0500,
	eIOTYPE_USER_MSG_MAX
}E_IOCTRL_MSGTYPE;

/////////////////////////////////////////////////////////////////////////////////
/////////////////// Message Type Command Define//////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// IOCTRL Message Type Command
typedef enum 
{
	eIOTYPE_MSG_NATSTATE 				= 0x0101,
	eIOTYPE_MSG_PHYSTATE	 			= 0x0102,
	
	eIOTYPE_MSG_AVSLOGIN_GETAUTH		= 0x0201,
	eIOTYPE_MSG_AVSLOGIN_SETTOKEN		= 0x0202,
	eIOTYPE_MSG_AVSLOGIN_RESPONSE		= 0x0203,
	eIOTYPE_MSG_AVSLOGOUT				= 0x0204,
	eIOTYPE_MSG_AVS_BUTTON_MUTE 		= 0x0205,
	eIOTYPE_MSG_AVS_SETLANGUAGE 		= 0x0206,
	eIOTYPE_MSG_AVS_GETLANGUAGE 		= 0x0207,
	
	eIOTYPE_MSG_TUNNEL_START 			= 0x0301,
	eIOTYPE_MSG_TUNNEL_STOP 			= 0x0302,
	eIOTYPE_MSG_SETTUNNELCTRL_REQ		= 0x0303,
	eIOTYPE_MSG_SETTUNNELCTRL_RESP		= 0x0304,
	eIOTYPE_MSG_TUNNEL_SETBAUDRATE 		= 0x0305,
	eIOTYPE_MSG_TUNNEL_GETBAUDRATE 		= 0x0306,
	eIOTYPE_MSG_TUNNEL_SETPORT 			= 0x0307,
	eIOTYPE_MSG_TUNNEL_GETPORT 			= 0x0308,

	eIOTYPE_MSG_GPIO_WLED_ON 			= 0x0501,
	eIOTYPE_MSG_GPIO_WLED_OFF 			= 0x0502,
	eIOTYPE_MSG_GPIO_WLED_SLOWFLASHING 	= 0x0503,
	eIOTYPE_MSG_GPIO_WLED_QUICKFLASHING = 0x0504,
	eIOTYPE_MSG_GPIO_WLED_UPDATE_FLASHING  = 0x0505,


}E_IOCTRL_CMD;

/////////////////////////////////////////////////////////////////////////////////
/////////////////// Type ENUM Define ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
typedef enum
{
	eIOCTRL_OK					= 0x00,
	eIOCTRL_ERR					= -0x01,
	eIOCTRL_ERR_PASSWORD		= eIOCTRL_ERR - 0x01,
	eIOCTRL_ERR_SETCTRL			= eIOCTRL_ERR - 0x02,
	eIOCTRL_ERR_DEVICEINFO		= eIOCTRL_ERR - 0x03,
	eIOCTRL_ERR_LOGIN			= eIOCTRL_ERR - 4,
	eIOCTRL_ERR_LISTWIFIAP		= eIOCTRL_ERR - 5,
	eIOCTRL_ERR_SETWIFI			= eIOCTRL_ERR - 6,
	eIOCTRL_ERR_GETWIFI			= eIOCTRL_ERR - 7,
///////msgsnd---msgrcv/////
	eIOCTRL_ERR_E2BIG			= eIOCTRL_ERR - 8,//only for msgrcv.
	eIOCTRL_ERR_EACCES			= eIOCTRL_ERR - 9,
	eIOCTRL_ERR_EAGAIN			= eIOCTRL_ERR - 10,
	eIOCTRL_ERR_EFAULT			= eIOCTRL_ERR - 11,
	eIOCTRL_ERR_EIDRM			= eIOCTRL_ERR - 12,
	eIOCTRL_ERR_EINTR			= eIOCTRL_ERR - 13,
	eIOCTRL_ERR_EINVAL			= eIOCTRL_ERR - 14,
	eIOCTRL_ERR_ENOMSG			= eIOCTRL_ERR - 15,
//////msgrcv/////
/*
	E2BIG
	EACCES
	EAGAIN
	EFAULT
	EIDRM
	EINTR
	EINVAL
	ENOMSG
	* */
	eIOCTRL_ERR_LISTEVENT		= eIOCTRL_ERR - 8,

}E_AVIOCTRL_ERROR;

/////////////////////////////////////////////////////////////////////////////
///////////////////////// Message Body Define ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/*
eIOTYPE_USER_MSG_NATSTATE                    = 0x0101,
** @struct SMsgIoctrlNatState
*/
typedef struct{
	int u32iCommand;	//refer to E_IOCTRL_HTTPCMD
	int restult;	// 0: online; otherwise: offline.
}SMsgIoctrlNatState;

/*
eIOTYPE_MSG_AVSLOGIN_GETAUTH		= 0x0201,
** @struct SMsgIoctrlAvsGetAuth
*/
typedef struct
{
	int  u32iCommand;	//refer to E_IOCTRL_HTTPCMD
	int restult;	// 0: online; otherwise: offline.
	char client_id[128];
	char redirect_uri[128];
	char grant_type[16];
	char code_verify[136];
	char authorize_code[100];
	unsigned char reserved[4];
}SMsgIoctrlAvsSetAuth;
/*
eIOTYPE_MSG_AVSLOGIN_SETAUTH		= 0x0202
** @struct SMsgIoctrlAvsSetAuth
*/
typedef struct
{
	char product_id[32];
	char product_dsn[64];
	char code_method[8];
	char code_challenge[48];
}SMsgIoctrlAvsGetAuth;

/*
   Tunnel Baudrate & Port Data
** @struct SMsgIoctrlTunnel
*/
typedef struct 
{
	int baudrate;
	int port;
}SMsgIoctrlTunnel;

/*
	CMD
** @struct SMsgIoctrlData
*/
typedef struct msg_buf{
	long msgtype;
	int  u32iCommand;	//refer to E_IOCTRL_CMD
	char next[0];
}SMsgIoctrlData;

int RK_MsgInit(void);
int RK_SndIOCtrl(int msqid, const void *msgp, size_t sz, E_IOCTRL_MSGTYPE eMsgType, E_IOCTRL_CMD command);
int RK_RecvIOCtrl(int msqid, void *msgp, size_t sz, E_IOCTRL_MSGTYPE eMsgType);

#endif //_RKIOCTRL_DEF_H_

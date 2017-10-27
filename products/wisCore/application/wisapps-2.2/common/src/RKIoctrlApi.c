/*RKIoctrlApi.c*/  
#include <stdio.h>   
#include <sys/types.h>   
#include <sys/ipc.h>   
#include <sys/msg.h>   
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../include/RKIoctrlDef.h"

//#define MSGKEY 8899 
#define Pathname "./xkeyideal"
#define MODE IPC_CREAT|IPC_EXCL|0666
#define ID 27 

int RK_MsgInit(void)
{
	int msqid;
	msqid=msgget(MSGKEY,IPC_EXCL);  /*check msgqueue */ 
	if(msqid < 0){  
		msqid = msgget(MSGKEY, MODE);/*create msgqueue */  
		if(msqid <0){
			printf("failed to create msq | errno=%d [%m]\n",errno);
			return eIOCTRL_ERR;
		}
	}
	
	return msqid;
}


E_AVIOCTRL_ERROR errno2avioctrlerrno(int errorno)
{
	switch(errorno){
		case E2BIG:  	return eIOCTRL_ERR_E2BIG;
		case EACCES:  	return eIOCTRL_ERR_EACCES;
		case EAGAIN:  	return eIOCTRL_ERR_EAGAIN;
		case EFAULT:  	return eIOCTRL_ERR_EFAULT;
		case EIDRM:  	return eIOCTRL_ERR_EIDRM;
		case EINTR:  	return eIOCTRL_ERR_EINTR;
		case EINVAL:  	return eIOCTRL_ERR_EINVAL;
		case ENOMSG:  	return eIOCTRL_ERR_ENOMSG;
		default:
			printf("unknown errno\n");return -1;
	}
}


int RK_SndIOCtrl(int msqid, const void *msgp, size_t sz, E_IOCTRL_MSGTYPE eMsgType, E_IOCTRL_CMD command)
{
	if(sz && (msgp==NULL))
		return eIOCTRL_ERR;
	SMsgIoctrlData *msgData;

	size_t headerSize = (size_t)(((SMsgIoctrlData *)0)->next);
	msgData = (SMsgIoctrlData *)malloc(headerSize + sz);
	if(msgData == NULL)
		return eIOCTRL_ERR;
	
	msgData->msgtype = eMsgType;
	msgData->u32iCommand = command;
	
	memcpy(msgData->next, msgp, sz);
	
	int ret = msgsnd(msqid, msgData, headerSize + sz - sizeof(long), IPC_NOWAIT);	//non-block
	free(msgData);
	if(ret<0){
		int tmperrno = errno;
		return errno2avioctrlerrno(tmperrno);
	}
	return eIOCTRL_OK;
	
}


int RK_RecvIOCtrl(int msqid, void *msgp, size_t sz, E_IOCTRL_MSGTYPE eMsgType)
{
	if(msgp==NULL)
		return eIOCTRL_ERR;
	SMsgIoctrlData *msgData = (SMsgIoctrlData *)msgp;
	
	ssize_t rcvsize = msgrcv(msqid, msgData, sz, eMsgType, 0);	//block mode
	if(rcvsize<0){
		int tmperrno = errno;
		return errno2avioctrlerrno(tmperrno);
	}
	return (int)rcvsize;
}

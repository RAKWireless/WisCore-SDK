#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include <linux/version.h>
#include <sys/vfs.h>
#include <netinet/in.h>

#include "RKIoctrlDef.h"
#include "RKUartApi.h"
#include "RKLog.h"

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define DATACNT 1024

typedef struct {
	void *mdp;
	void *mtdp;
	int m_flag;
}P_MAL;

typedef struct{

	int TunnelBaudRate;
	int TunnelBits;
	char TunnelEvent;
	int TunnelStop;
	int uart_fd;
	P_MAL p_mal;
	rak_log_t Tunnellog;

}UART_PARAM;


typedef struct{
	uint8_t tId[2];
	uint8_t tCmdType;
	uint8_t tCmd[2];
	uint8_t tStatus;
	uint8_t tParity[2];
	uint8_t tLength[2];
//	uint8_t tDataBuf[DATACNT];
}TUNNEL_HEAD;

typedef struct {
	int iFullLevel;
	char DataBuf[DATACNT];
}S_IO_BUFFER;

int RK_FlagQuit = 0;
int RK_ConnectCnt = 0;

int TcpPort = 5005;
pthread_t RK_Server;
int RK_AcceptFd;

TUNNEL_HEAD tunnel_head;


void InitIOBuffer(S_IO_BUFFER	*psIOBuf)
{
	memset(psIOBuf->DataBuf, 0, sizeof(psIOBuf->DataBuf));
	psIOBuf->iFullLevel = 0;
}	// InitIOBuffer

int iread(S_IO_BUFFER	*psIOBuf, void	*pHeaderBuf, size_t	len, int iTimeout)
{
	int		copied = 0, rc, i;
	fd_set	fds;
	struct	timeval tv;

	memset(pHeaderBuf, 0, len);

	while((copied < len))
	{
		i = MIN(psIOBuf->iFullLevel, len - copied);
		memcpy(pHeaderBuf + copied, psIOBuf->DataBuf + DATACNT - psIOBuf->iFullLevel, i);

		psIOBuf->iFullLevel -= i;
		copied += i;
		
		if(copied >= len)
			return copied;


		tv.tv_sec = iTimeout;
		tv.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(RK_AcceptFd, &fds);
		
		if((rc = select(RK_AcceptFd+1, &fds, NULL, NULL, &tv)) <= 0)
		{
			if(rc < 0)
				exit(EXIT_FAILURE);

			// this must be a iTimeout 
			return copied;
		}

		InitIOBuffer(psIOBuf);

		if((psIOBuf->iFullLevel = recv(RK_AcceptFd, &psIOBuf->DataBuf, DATACNT,0)) < 0)
			return -1;
		else if(psIOBuf->iFullLevel == 0){
			RK_ConnectCnt --;
		}else
		/* align data to the end of the pHeaderBuf if less than IO_BUFFER bytes were read */
			memmove(psIOBuf->DataBuf + (DATACNT - psIOBuf->iFullLevel), psIOBuf->DataBuf, psIOBuf->iFullLevel);
	}

	return 0;
}	// _read


int recvFromTcp(S_IO_BUFFER *psIOBuf, void *pHeaderBuf, size_t len, int iTimeout)
{
	char c= '\0', 
		*out= pHeaderBuf;
	int i;

	memset(pHeaderBuf, 0, len);

	for(i = 0; i < len ; i++)
	{
		
		if(iread(psIOBuf, &c, 1, iTimeout) <= 0)
		{
			// iTimeout or error occured 
			return -1;
		}
		

		*out++ = c; 
		if(!psIOBuf->iFullLevel) return i+1;
	}

	return i;

}
void *tcptouart_pthread(void *arg)
{
	
	UART_PARAM uart_param = *(UART_PARAM *)arg; 
	uint8_t buff[DATACNT+10];

	int ret,iReadByteCnt;
	S_IO_BUFFER	sIOBuf;
	
	InitIOBuffer(&sIOBuf);

	while(RK_FlagQuit){	
		iReadByteCnt = recvFromTcp(&sIOBuf,buff+10,sizeof(buff),2);
		usleep(1000);
		
		/* send data from tcp to uart + protocol header */
		if(iReadByteCnt > 0)
		{

			buff[0] = tunnel_head.tId[0];
			buff[1] = tunnel_head.tId[1];
			buff[2] = tunnel_head.tCmdType;
			buff[3] = tunnel_head.tCmd[0];
			buff[4] = 0x02;
			buff[5] = tunnel_head.tStatus;
			buff[6] = tunnel_head.tParity[0];
			buff[7] = tunnel_head.tParity[1];
			buff[8] = iReadByteCnt/256;
			buff[9] = iReadByteCnt - buff[8]*256;

			write(uart_param.uart_fd,buff,iReadByteCnt+10);
		}
	}



	return NULL;
}

void RK_Server_cleanup(void *arg){
	int fd = *(int *)arg;
	close(RK_AcceptFd);
	close(fd);
}

// pthread to start tcp server
void *tcp_server_pthread(void *arg)
{
	struct sockaddr_in  addr, client_addr;
	int on;
	int iSocketFd;
	pthread_t tcptouart;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	UART_PARAM uart_param = *(UART_PARAM *)arg; 

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	/* open socket for server */
	if((iSocketFd = socket(PF_INET,SOCK_STREAM,0)) < 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"socket failed\n");
		exit(EXIT_FAILURE);
	}

	/* set cleanup handler to cleanup ressources */
	pthread_cleanup_push(RK_Server_cleanup, &iSocketFd);
	/* ignore "socket already in use" errors */
	on = 1;
	if(setsockopt(iSocketFd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}

	/* configure server address to listen to all local IPs */
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TcpPort);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(iSocketFd,(struct sockaddr*)&addr,sizeof(addr)) != 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}

	/* start listening on socket */
	if(listen(iSocketFd,10) != 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"listen failed\n");
		exit(EXIT_FAILURE);
	}

	/* create a child for every client that connects */
	while( RK_FlagQuit )
	{
		pthread_testcancel();
		RK_AcceptFd = accept(iSocketFd,(struct sockaddr *)&client_addr,&addr_len);
		pthread_testcancel();
		/* start new thread that will handle this TCP connected client */
		if(RK_ConnectCnt >= 1) 
		{
			LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"out of connection limitation!!!RK_ConnectCnt:%d\n",RK_ConnectCnt);
			close(RK_AcceptFd);
			continue;
		}
		else
		{
			struct timeval sTimeout = {1,0};
			if(setsockopt(RK_AcceptFd,SOL_SOCKET,SO_SNDTIMEO,&sTimeout,sizeof(sTimeout)) < 0)
			{
				LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"failed to setsockopt(SO_SNDTIMEO)\n");
			}

			if(pthread_create(&tcptouart,NULL,&tcptouart_pthread,arg) != 0)// create pthread to handle net data
			{
				LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"can't launch tunnel pthread\n");
				close(RK_AcceptFd);
				continue;
			}
			pthread_detach(tcptouart);
			RK_ConnectCnt ++;
		}
	}
	
	pthread_cleanup_pop(1);
	return NULL;
}

/*
 *Uart Protocol:
 *Open Tunnel:  0xAA 0xAA 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
 *Close Tunnel: 0xAA 0xAA 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00
 *Send Data:    0xAA 0xAA 0x00 0x00 0x02 0x00 0x00 0x00 0x00 0x03 0xFB 0x83 0x7C
 *
 */
int recvFromUartToTcp(uint8_t readUartBuf[DATACNT],int i,UART_PARAM uart_param)
{
	if((readUartBuf[0] == 0xAA) && (readUartBuf[1] == 0xAA)) //ID:2 Bytes
	{
		if((readUartBuf[2] == 0x00)) //Cmd Type: 1 Byte 
		{
			if(readUartBuf[3] == 0x00) //Cmd: 2 Bytes
			{
				if(readUartBuf[4] == 0x00)//Cmd: open tunnel
				{
					if((readUartBuf[8] == 0x00) && (readUartBuf[9] == 0x00) && i == 10)
					{
						if(RK_FlagQuit == 0){
							RK_FlagQuit = 1;
							readUartBuf[5] = 0x00;

							memset(&tunnel_head,0,sizeof(TUNNEL_HEAD));

							tunnel_head.tId[0] = readUartBuf[0];
							tunnel_head.tId[1] = readUartBuf[1];
							tunnel_head.tCmdType = readUartBuf[2];
							tunnel_head.tCmd[0] = readUartBuf[3];
							tunnel_head.tCmd[1] = readUartBuf[4];
							tunnel_head.tStatus = readUartBuf[5];
							tunnel_head.tParity[0] = readUartBuf[6];
							tunnel_head.tParity[1] = readUartBuf[7];
							tunnel_head.tLength[0] = readUartBuf[8];
							tunnel_head.tLength[1] = readUartBuf[9];

							/* response cmd from uart when open */
							write(uart_param.uart_fd,readUartBuf,10);
						}
					}
					else
						LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Cmd Open:Data Length Error Or Bits Error\n");
				}
				else if(readUartBuf[4] == 0x01)//Cmd: close tunnel
				{
					if((readUartBuf[8] == 0x00) && (readUartBuf[9] == 0x00) && i == 10)
					{
						if(RK_FlagQuit == 1){
							RK_FlagQuit = 0;
							if((pthread_cancel(RK_Server)) != 0){
								readUartBuf[5] = 0x01;
							}
							else{
								readUartBuf[5] = 0x00;
								if(RK_ConnectCnt)
									RK_ConnectCnt --;
								LOG_P(uart_param.Tunnellog,RAK_LOG_FINE,"Stop Tunnel TCP Server\n");
							}
							/* response cmd from uart when close */
							write(uart_param.uart_fd,readUartBuf,10);
						}
					}
					else
						LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Cmd Close:Data Length Error Or Bits Error\n");

				}
				else if((readUartBuf[4] == 0x02))//Cmd:send data
				{

					int uartlen = readUartBuf[8]*256 + readUartBuf[9];
					if((RK_FlagQuit == 1) && ((i - 10) == uartlen))
					{
						send(RK_AcceptFd,readUartBuf+10,uartlen,0);

					}
				}
				else{
					LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Cmd error\n");
				}
			}
		}
		else if(readUartBuf[2] == 0x01)
		{

			LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Not has Get Cmd now\n");
		}
		else
		{
			LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Cmd Type error\n");
		}
	}
	return 0;
}

void uart_pthread_cleanup(UART_PARAM *uart_param)
{
	RK_CloseUart(uart_param->uart_fd);
	rak_log_destroy(uart_param->Tunnellog);
}

void *uart_pthread(void *arg)
{

	uint8_t readUartBuf[DATACNT+10] = "";
	uint8_t *out = readUartBuf;
	uint8_t *p;
	fd_set fds;
	struct timeval timeout={0,0};
	int ret,i,readlen;
	UART_PARAM uart_param = *(UART_PARAM *)arg; 

	/* set uart Params */
	RK_SetUartOpt(uart_param.uart_fd,uart_param.TunnelBaudRate,uart_param.TunnelBits,uart_param.TunnelEvent,uart_param.TunnelStop);
	
	/* set cleanup handler to cleanup ressources */
	pthread_cleanup_push(uart_pthread_cleanup,arg);

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(uart_param.uart_fd,&fds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 10*1000*1000/uart_param.TunnelBaudRate;

		memset(readUartBuf,0,sizeof(readUartBuf));
		out = readUartBuf;

		for(i=0;i<DATACNT+10;i++)
		{
			timeout.tv_usec = 10*1000*1000/uart_param.TunnelBaudRate;

			/* select to listen uart */
			if(ret = select(uart_param.uart_fd+1,&fds,NULL,NULL,&timeout) <= 0)
			{
				break;
			}

			/* read uart data */
			if((readlen = read(uart_param.uart_fd,out,1)) <= 0)
			{
				LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"Read Uart error\n");
			}
			*out ++;
		}
		
		/* send uart data to tcp */
		recvFromUartToTcp(readUartBuf,i,uart_param);

		usleep(500*1000);

	}

	pthread_cleanup_pop(1);

	return NULL;
}

void msg_pthread_cleanup(UART_PARAM *ptr)
{
	
	free(ptr->p_mal.mdp);
	if(!ptr->p_mal.m_flag)
		free(ptr->p_mal.mtdp);
	rak_log_destroy(ptr->Tunnellog);
}


void *msg_pthread(void *arg)
{

	int msg_id;
	UART_PARAM uart_param = *(UART_PARAM *)arg;

	SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData));
	if(msgData == NULL)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"msgData malloc failed\n");
		return ;
	}

	SMsgIoctrlTunnel *msg_tunnel_data = (SMsgIoctrlTunnel *)malloc(sizeof(SMsgIoctrlTunnel));
	if(msg_tunnel_data == NULL){
		LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"msg_tunnel_data malloc failed\n");
		return ;
	}

	msg_id = RK_MsgInit();

	uart_param.p_mal.mdp=msgData;
	uart_param.p_mal.mtdp=msg_tunnel_data;
	uart_param.p_mal.m_flag=0;

	/* set cleanup handler to cleanup ressources */
	pthread_cleanup_push(msg_pthread_cleanup,arg);

	while(1){
		memset(msgData,0,sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlTunnel));
		memset(msg_tunnel_data,0,sizeof(SMsgIoctrlTunnel));
		RK_RecvIOCtrl(msg_id,msgData,sizeof(SMsgIoctrlData)+sizeof(SMsgIoctrlTunnel),eIOTYPE_USER_MSG_TUNNEL);
		switch(msgData->u32iCommand)
		{

		/* start tcp server through http cmd */
		case eIOTYPE_MSG_TUNNEL_START:
			{
				RK_FlagQuit = 1;
			}
			break;

		/* stop tcp server through http cmd */
		case eIOTYPE_MSG_TUNNEL_STOP:
			{
				LOG_P(uart_param.Tunnellog,RAK_LOG_FINE,"Stop Tunnel TCP Server\n");
				if(RK_FlagQuit == 1){
					RK_FlagQuit = 0;
					pthread_cancel(RK_Server);
					if(RK_ConnectCnt)
						RK_ConnectCnt --;
				}
			}
			break;

		/* set uart1 baudrate */
		case eIOTYPE_MSG_TUNNEL_SETBAUDRATE:
			{
				uart_param.p_mal.m_flag=1;
				msg_tunnel_data = (SMsgIoctrlTunnel *)msgData->next;	
				uart_param.TunnelBaudRate = msg_tunnel_data->baudrate;
				RK_SetUartOpt(uart_param.uart_fd,uart_param.TunnelBaudRate,uart_param.TunnelBits,uart_param.TunnelEvent,uart_param.TunnelStop);
			}
			break;

		/* get uart1 baudrate */
		case eIOTYPE_MSG_TUNNEL_GETBAUDRATE:
			{
				msg_tunnel_data->baudrate = uart_param.TunnelBaudRate;
				RK_SndIOCtrl(msg_id,msg_tunnel_data,sizeof(SMsgIoctrlTunnel),eIOTYPE_USER_MSG_HTTPD,eIOTYPE_MSG_TUNNEL_GETBAUDRATE);
			}
			break;

		/* set tcp server port */
		case eIOTYPE_MSG_TUNNEL_SETPORT:
			{
					
				uart_param.p_mal.m_flag=1;
				msg_tunnel_data = (SMsgIoctrlTunnel *)msgData->next;	
				TcpPort = msg_tunnel_data->port;
			
			}
			break;

		/* get tcp server port */
		case eIOTYPE_MSG_TUNNEL_GETPORT:
			{
				msg_tunnel_data->port = TcpPort;
				RK_SndIOCtrl(msg_id,msg_tunnel_data,sizeof(SMsgIoctrlTunnel),eIOTYPE_USER_MSG_HTTPD,eIOTYPE_MSG_TUNNEL_GETPORT);

			}
			break;

		default:
			LOG_P(uart_param.Tunnellog,RAK_LOG_ERROR,"msg_tunnel Command failed \n");
			break;
		}
	}
	pthread_cleanup_pop(1);
	return NULL;
}


int main(int argc, const char *argv[])
{
	pthread_t tMsg,tuart;
	UART_PARAM uart_param;

	/* set Uart Param */
	uart_param.TunnelBaudRate = 115200;
	uart_param.TunnelBits = 8;
	uart_param.TunnelEvent = 'N';
	uart_param.TunnelStop = 1;

	//* set Uart Protocol to send data */
	tunnel_head.tId[0] = 0xAA;
	tunnel_head.tId[1] = 0xAA;
	tunnel_head.tCmdType = 0x00;
	tunnel_head.tCmd[0] = 0x00;
	tunnel_head.tCmd[1] = 0x02;
	tunnel_head.tStatus = 0x00;
	tunnel_head.tParity[0] = 0x00;
	tunnel_head.tParity[1] = 0x00;
	tunnel_head.tLength[0] = 0x00;
	tunnel_head.tLength[1] = 0x00;

	/* Init Tunnel Log module*/
	uart_param.Tunnellog = rak_log_init("TUNNEL",RAK_LOG_FINE,8,NULL,NULL);

	/* Init Uart1 */
	if((uart_param.uart_fd = RK_InitUart()) < 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_FATAL,"init uart failed\n");
		return -1;
	}

	/* Create pthread to handle message between http process & tunnel */
	if(pthread_create(&tMsg,NULL,&msg_pthread,&uart_param) != 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_FATAL,"can't launch msg pthread\n");
		return -1;
	}

	/* create pthread to handle uart data */
	if(pthread_create(&tuart,NULL,&uart_pthread,&uart_param) != 0)
	{
		LOG_P(uart_param.Tunnellog,RAK_LOG_FATAL,"can't launch msg pthread\n");
		return -1;
	}

	/* create pthread to handle TCP server */
	while(1){
		/* flag to conctrol tcp Server,0: quit, 1:start */
		while( RK_FlagQuit ){

			LOG_P(uart_param.Tunnellog,RAK_LOG_FINE,"Start Tunnel TCP Server\n");
			if(pthread_create(&RK_Server,NULL,&tcp_server_pthread,&uart_param) != 0){
				LOG_P(uart_param.Tunnellog,RAK_LOG_FATAL,"create tcp_server_pthread fail\n");
				return -1;
			}
			pthread_join(RK_Server,0);
		}
		usleep(500*1000);
	}
	return 0;
}

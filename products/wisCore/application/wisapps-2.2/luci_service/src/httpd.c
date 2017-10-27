/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 busybox-project (base64 function)                    #
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

#include "utils.h"
#include "httpd.h"

#include "updateconfigfile.h" //gz
#include "RKIoctrlDef.h"   //swt
#include "RKLog.h"

//static S_MSF_GLOBALS *pglobal;
extern context g_sServer;
volatile uint32_t	g_uiConnThreadCnt		= 0;
#define MAXPARAM 2

/******************************************************************************
Description.: initializes the iobuffer structure properly
Input Value.: pointer to already allocated iobuffer
Return Value: iobuf
******************************************************************************/
void init_iobuffer(iobuffer *iobuf)
{
    memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
    iobuf->level = 0;
}

/******************************************************************************
Description.: initializes the S_REQUEST structure properly
Input Value.: pointer to already allocated req
Return Value: req
******************************************************************************/
void init_request(S_REQUEST *req)
{
    req->m_eDataType        = A_UNKNOWN;
    req->m_eDataType        = A_UNKNOWN;
    req->m_strParameter   	= NULL;
    req->m_strClient      	= NULL;
    req->credentials 		= NULL;
}

/******************************************************************************
Description.: If strings were assigned to the different members free them
              This will fail if strings are static, so always use strdup().
Input Value.: req: pointer to S_REQUEST structure
Return Value: -
******************************************************************************/
void free_request(S_REQUEST *req)
{
    if(req->m_strParameter != NULL) free(req->m_strParameter);
    if(req->m_strClient != NULL) free(req->m_strClient);
    if(req->credentials != NULL) free(req->credentials);
}

/******************************************************************************
Description.: read with timeout, implemented without using signals
              tries to read len bytes and returns if enough bytes were read
              or the timeout was triggered. In case of timeout the return
              value may differ from the requested bytes "len".
Input Value.: * fd.....: fildescriptor to read from
              * iobuf..: iobuffer that allows to use this functions from multiple
                         threads because the complete context is the iobuffer.
              * buffer.: The buffer to store values at, will be set to zero
                         before storing values.
              * len....: the length of buffer
              * timeout: seconds to wait for an answer
Return Value: * buffer.: will become filled with bytes read
              * iobuf..: May get altered to save the context for future calls.
              * func().: bytes copied to buffer or -1 in case of error
******************************************************************************/
int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    int copied = 0, rc, i;
    fd_set fds;
    struct timeval tv;

    memset(buffer, 0, len);

    while((copied < len)) {
        i = MIN(iobuf->level, len - copied);
        memcpy(buffer + copied, iobuf->buffer + IO_BUFFER - iobuf->level, i);

        iobuf->level -= i;
        copied += i;
        if(copied >= len)
            return copied;

        /* select will return in case of timeout or new data arrived */
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if((rc = select(fd + 1, &fds, NULL, NULL, &tv)) <= 0) {
            if(rc < 0)
                exit(EXIT_FAILURE);

            /* this must be a timeout */
            return copied;
        }

        init_iobuffer(iobuf);

        /*
         * there should be at least one byte, because select signalled it.
         * But: It may happen (very seldomly), that the socket gets closed remotly between
         * the select() and the following read. That is the reason for not relying
         * on reading at least one byte.
         */
        if((iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER)) <= 0) {
            /* an error occured */
            return -1;
        }
	//	printf("iobuf->buffer 2:%s\n",iobuf->buffer);

        /* align data to the end of the buffer if less than IO_BUFFER bytes were read */
        memmove(iobuf->buffer + (IO_BUFFER - iobuf->level), iobuf->buffer, iobuf->level);
    }

    return 0;
}

/******************************************************************************
Description.: Read a single line from the provided fildescriptor.
              This funtion will return under two conditions:
              * line end was reached
              * timeout occured
Input Value.: * fd.....: fildescriptor to read from
              * iobuf..: iobuffer that allows to use this functions from multiple
                         threads because the complete context is the iobuffer.
              * buffer.: The buffer to store values at, will be set to zero
                         before storing values.
              * len....: the length of buffer
              * timeout: seconds to wait for an answer
Return Value: * buffer.: will become filled with bytes read
              * iobuf..: May get altered to save the context for future calls.
              * func().: bytes copied to buffer or -1 in case of error
******************************************************************************/
/* read just a single line or timeout */
int _readline(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    char c = '\0', *out = buffer;
    int i;

    memset(buffer, 0, len);

    for(i = 0; i < len && c != '\n'; i++) {
        if(_read(fd, iobuf, &c, 1, timeout) <= 0) {
            /* timeout or error occured */
            return -1;
        }
        *out++ = c;
    }

    return i;
}

/******************************************************************************
Description.: Decodes the data and stores the result to the same buffer.
              The buffer will be large enough, because base64 requires more
              space then plain text.
Hints.......: taken from busybox, but it is GPL code
Input Value.: base64 encoded data
Return Value: plain decoded data
******************************************************************************/
void decodeBase64(char *data)
{
    const unsigned char *in = (const unsigned char *)data;
    /* The decoded size will be at most 3/4 the size of the encoded */
    unsigned ch = 0;
    int i = 0;

    while(*in) {
        int t = *in++;

        if(t >= '0' && t <= '9')
            t = t - '0' + 52;
        else if(t >= 'A' && t <= 'Z')
            t = t - 'A';
        else if(t >= 'a' && t <= 'z')
            t = t - 'a' + 26;
        else if(t == '+')
            t = 62;
        else if(t == '/')
            t = 63;
        else if(t == '=')
            t = 0;
        else
            continue;

        ch = (ch << 6) | t;
        i++;
        if(i == 4) {
            *data++ = (char)(ch >> 16);
            *data++ = (char)(ch >> 8);
            *data++ = (char) ch;
            i = 0;
        }
    }
    *data = '\0';
}

/******************************************************************************
Description.: convert a hexadecimal ASCII character to integer
Input Value.: ASCII character
Return Value: corresponding value between 0 and 15, or -1 in case of error
******************************************************************************/
int hex_char_to_int(char in)
{
    if(in >= '0' && in <= '9')
        return in - '0';

    if(in >= 'a' && in <= 'f')
        return (in - 'a') + 10;

    if(in >= 'A' && in <= 'F')
        return (in - 'A') + 10;

    return -1;
}

/******************************************************************************
Description.: replace %XX with the character code it represents, URI
Input Value.: string to unescape
Return Value: 0 if everything is ok, -1 in case of error
******************************************************************************/
int unescape(char *string)
{
    char *source = string, *destination = string;
    int src, dst, length = strlen(string), rc;

    /* iterate over the string */
    for(dst = 0, src = 0; src < length; src++) {

        /* is it an escape character? */
        if(source[src] != '%') {
            /* no, so just go to the next character */
            destination[dst] = source[src];
            dst++;
            continue;
        }

        /* yes, it is an escaped character */

        /* check if there are enough characters */
        if(src + 2 > length) {
            return -1;
            break;
        }

        /* perform replacement of %## with the corresponding character */
        if((rc = hex_char_to_int(source[src+1])) == -1) return -1;
        destination[dst] = rc * 16;
        if((rc = hex_char_to_int(source[src+2])) == -1) return -1;
        destination[dst] += rc;

        /* advance pointers, here is the reason why the resulting string is shorter */
        dst++; src += 2;
    }

    /* ensure the string is properly finished with a null-character */
    destination[dst] = '\0';

    return 0;
}

/******************************************************************************
Description.: Send error messages and headers.
Input Value.: * fd.....: is the filedescriptor to send the message to
              * which..: HTTP error code, most popular is 404
              * message: append this string to the displayed response
Return Value: -
******************************************************************************/
void send_error(int fd, int which, char *message)
{
    char buffer[BUFFER_SIZE] = {0};

    if(which == 401) {
        sprintf(buffer, "HTTP/1.0 401 Unauthorized\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "WWW-Authenticate: Basic realm=\"MJPG-Streamer\"\r\n" \
                "\r\n" \
                "401: Not Authenticated!\r\n" \
                "%s", message);
    } else if(which == 404) {
        sprintf(buffer, "HTTP/1.0 404 Not Found\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "404: Not Found!\r\n" \
                "%s", message);
    } else if(which == 500) {
        sprintf(buffer, "HTTP/1.0 500 Internal Server Error\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "500: Internal Server Error!\r\n" \
                "%s", message);
    } else if(which == 400) {
        sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "400: Not Found!\r\n" \
                "%s", message);
    } else if(which == 406){
		sprintf(buffer,	"HTTP/1.1 406 Not Acceptable\r\n" \
				"Content-type: text/plain\r\n" \
				STD_HEADER \
				"\r\n" \
				"406: Not Acceptable!\r\n" \
				"%s", message);
	}else {
        sprintf(buffer, "HTTP/1.0 501 Not Implemented\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "501: Not Implemented!\r\n" \
                "%s", message);
    }

    if(write(fd, buffer, strlen(buffer)) < 0) {
        LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"write failed, done anyway\n");
    }
}

/******************************************************************************
Description.: Send HTTP header and copy the content of a file. To keep things
              simple, just a single folder gets searched for the file. Just
              files with known extension and supported mimetype get served.
              If no parameter was given, the file "index.html" will be copied.
Input Value.: * fd.......: filedescriptor to send data to
              * parameter: string that consists of the filename
              * id.......: specifies which server-context is the right one
Return Value: -
******************************************************************************/
void send_file(int id, int fd, char *parameter)
{
    char buffer[BUFFER_SIZE] = {0};
    char *extension, *mimetype = NULL;
    int i, lfd;
    ////config conf = servers[id].conf;
	config conf = g_sServer.conf;

    /* in case no parameter was given */
    if(parameter == NULL || strlen(parameter) == 0)
        parameter = "index.html";

    /* find file-extension */
    char * pch;
    pch = strchr(parameter, '.');
    int lastDot = 0;
    while(pch != NULL) {
        lastDot = pch - parameter;
        pch = strchr(pch + 1, '.');
    }

    if(lastDot == 0) {
        send_error(fd, 400, "No file extension found");
        return;
    } else {
        extension = parameter + lastDot;
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"%s EXTENSION: %s\n", parameter, extension);
    }

    /* determine mime-type */
    for(i = 0; i < LENGTH_OF(mimetypes); i++) {
        if(strcmp(mimetypes[i].dot_extension, extension) == 0) {
            mimetype = (char *)mimetypes[i].mimetype;
            break;
        }
    }

    /* in case of unknown mimetype or extension leave */
    if(mimetype == NULL) {
        send_error(fd, 404, "MIME-TYPE not known");
        return;
    }

    /* now filename, mimetype and extension are known */
    LOG_P(g_sServer.httplog,RAK_LOG_FINE,"trying to serve file \"%s\", extension: \"%s\" mime: \"%s\"\n", parameter, extension, mimetype);

    /* build the absolute path to the file */
    strncat(buffer, conf.www_folder, sizeof(buffer) - 1);
    strncat(buffer, parameter, sizeof(buffer) - strlen(buffer) - 1);

    /* try to open that file */
    if((lfd = open(buffer, O_RDONLY)) < 0) {
        LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"file %s not accessible\n", buffer);
        send_error(fd, 404, "Could not open file");
        return;
    }
    LOG_P(g_sServer.httplog,RAK_LOG_FINE,"opened file: %s\n", buffer);

    /* prepare HTTP header */
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
            "Content-type: %s\r\n" \
            STD_HEADER \
            "\r\n", mimetype);
    i = strlen(buffer);

    /* first transmit HTTP-header, afterwards transmit content of file */
    do {
        if(write(fd, buffer, i) < 0) {
            close(lfd);
            return;
        }
    } while((i = read(lfd, buffer, sizeof(buffer))) > 0);

    /* close file, job done */
    close(lfd);
}


/****************************************
	function : receive file for firmware upgrade
*****************************************/
void DoFirmwareUpgrade(
	int		iSocketFd, 
	int		iContentLength,
	char 		*strBoundary,
	iobuffer sIOBuf
)
{
	int iRetCode=0;
	char		ReqBuffer[BUFFER_SIZE]	= {0};
	int			iReadByteCnt;
	//iobuffer	 sIOBuf;
	uint64_t u64DiskAvailSpace;
	uint64_t u64NeededSize = DEF_FIRMWARE_OTA_SIZE;
	struct statfs sDiskState;		
	if(remove(DEF_FIRMWARE_OTA_PATH) != 0)
	{
		LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"remove %s error\n",DEF_FIRMWARE_OTA_PATH);
	}
	if(access(DEF_FIRMWARE_OTA_PATH, F_OK) != 0)
	{
		if(mkdir(DEF_FIRMWARE_OTA_PATH, 0755) < 0){
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"creat %s error!!\n",DEF_FIRMWARE_OTA_PATH);
			sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_LACK_SPACE);
			write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
			
			return ;
		}
	}
		// Get storage available space
	if(statfs(DEF_FIRMWARE_OTA_PATH, &sDiskState) < 0)
		return ;
	u64DiskAvailSpace =  (uint64_t)sDiskState.f_bsize * (uint64_t)sDiskState.f_bavail;  //get harddisk  remain space;
	if(u64DiskAvailSpace < u64NeededSize)
	{
		LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"lack space %s error!!\n");
		sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_LACK_SPACE);
		write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
		return;
	}
	//
	int		fd;
	char	strFirmwarePath[256]={0,};
	struct stat	st;


	int		iFileStart		= 0,
			iFileBytes		= 0,
			iBoundaryBytes	= strlen(strBoundary),
			iToReadBytes	= 4096,
			iBoundaryStart = 0,
			iFileName		= 0;
	uint8_t	*pu8Firmware	= NULL,
			*pHeadPtr = NULL;
	char strFileName[256]={0,};
	pu8Firmware	= malloc(iToReadBytes);
	int iToremain;
	if(pu8Firmware )
	{
		while(1)
		{
			if(iFileStart && ((iToremain=iContentLength - iFileBytes - iBoundaryBytes - 8) > iToReadBytes) )
			{	
				iReadByteCnt = _read(iSocketFd, &sIOBuf, pu8Firmware, iToReadBytes, 60);
			}
			else
			{	
				iReadByteCnt = _readline(iSocketFd, &sIOBuf, pu8Firmware, iToReadBytes, 60);
				LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iReadByteCnt = %d, iToReadBytes=%d\n",iReadByteCnt, iToReadBytes);
			}
			if(iReadByteCnt <= 0)
			{
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Receiving data is incomplete!\n");
				send_error(iSocketFd, 408, "The server timed out waiting for receiving data!");
				//remove("/tmp/skyeye.lock");
				break;
			}
			iFileBytes += iReadByteCnt;
			//*****************
			if(iFileStart == 0)
			{
				if(iBoundaryStart == 0){
					if(strstr(pu8Firmware, strBoundary) != NULL)
					{
						iBoundaryStart = 1;  //receive boundarystart						
						LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iBoundaryStart = %d\n", iBoundaryStart);
					}else{
						continue;
					}
				}else{					
					if( iFileName == 0 ){
						if((pHeadPtr = strstr(pu8Firmware, "filename=\"")) != NULL)// Parse content length
						{
							pHeadPtr += strlen("filename=\"");	

							sscanf(pHeadPtr ,"%[^\"]",strFileName);
							LOG_P(g_sServer.httplog,RAK_LOG_FINE,"http send file name:%s\n",strFileName);
							sprintf(strFirmwarePath,"%s/%s",DEF_FIRMWARE_OTA_PATH,strFileName);
							iFileName = 1;
							LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iFileName = %d\n", iFileName);
							// Start to receive and save update firmware
							if((fd = open(strFirmwarePath, O_CREAT | O_RDWR)) < 0){									
							//	send_error(iSocketFd, 400, "Malformed HTTP request");
								sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_LACK_SPACE);
								write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
								free(pu8Firmware);
								return;	
							}
						}else{
							continue;
						}
					}

				}
				if((iFileName == 1) && (pu8Firmware[0] == '\r'))
				{
					iFileStart = 1;
					LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iFileStart=%d\n", iFileStart);	
				}
				else
					continue;
			}
			//*****************
			else
			{
				if(iContentLength - iFileBytes - 2*iBoundaryBytes < 4096)
				{
					if(pu8Firmware[iReadByteCnt - 1] == '\n')
					{
						pu8Firmware[iReadByteCnt - 1] = 0;
						
						if(strstr(pu8Firmware, strBoundary) != NULL)
						{
							LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Found boundary at iFileBytes = %d!!!!\n", iFileBytes);
							break;
						}
						else
						{
							pu8Firmware[iReadByteCnt - 1] = '\n';
							
							if(pu8Firmware[iReadByteCnt - 2] == '\r')
								iReadByteCnt -= 2;
						}
					}
				}
				
				write(fd, pu8Firmware, iReadByteCnt);
			}
		}
		

	}
	close(fd);
	#if 1
	if(pu8Firmware != NULL)
		free(pu8Firmware);
	sprintf(ReqBuffer,"mtdtool chksum %s",strFirmwarePath);
	iRetCode = system(ReqBuffer);  //check sum,Open the child process
	
	if(WEXITSTATUS(iRetCode) != 0)
	{
		//if(WEXITSTATUS(iRetCode) == 2)  //Version err
		//{
		//	sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_VERSION_ERROR);
		//	write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
		//	remove(strFirmwarePath);
		//}else{                          //chksum err
			sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_CHKSUM_FAILED);
			write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
		//	remove(strFirmwarePath);
		//}
	}else{
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"mtdtool restore %s\n", strFirmwarePath);
		sprintf(ReqBuffer,"mtdtool restore %s",strFirmwarePath);
		iRetCode = system(ReqBuffer);  //restore,Open the child process
		if(WEXITSTATUS(iRetCode) != 0)
		{
			sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_UNKNOW_ERROR);
			write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
		//	remove(strFirmwarePath);
		}else{
			sprintf(ReqBuffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"0\"}");
			write(iSocketFd, ReqBuffer, strlen(ReqBuffer));
			sleep(1);
			close(iSocketFd);
			sprintf(ReqBuffer,"sysupgrade -q %s",strFirmwarePath);
			LOG_P(g_sServer.httplog,RAK_LOG_FINE,"app start update firmware\n");
			system(ReqBuffer);  //update firmware;
		}	
	}
	#endif
//	if(pHeadPtr != NULL)
//		free(pHeadPtr);		
	remove(strFirmwarePath);
	sync();

	return;
}
int read_wifistatus(char *strConfigFilePath,cfd lcfd)
{
	char strWifiMessage[512] = "";
	char strType[7][100] = {"",};
	int i,j=0;
	FILE	*fConfig = fopen(strConfigFilePath, "r");

	char *p;
	char	*pTmpPtr	= NULL,
			pReadBuf[512];
	int		iReadBytes;

	if(fConfig == NULL)
	{
		LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Failed to open wifilist file to read!\n");
		return 0;
		//		POST_STATE(buffer,-2);
		//	 	write(lcfd.m_iConnFd,buffer,strlen(buffer));
	}
	for(i=0;i<7;i++)
		bzero(strType[i], sizeof(strType[i]));
	while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
	{
		int lenth = strlen(pReadBuf);
		pReadBuf[lenth-1] = '\0';
		if((pTmpPtr = strstr(pReadBuf,"ESSID: ")) != NULL)
		{
			pTmpPtr += strlen("ESSID: ");
			strcpy(strType[0],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"Access Point: ")){

			pTmpPtr += strlen("Access Point: ");
			strcpy(strType[1],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"Channel: ")){

			pTmpPtr += strlen("Channel: ");
			strcpy(strType[2],pTmpPtr);
			if(p = strstr(strType[2]," ("))
				*p = '\0';

		}else if(pTmpPtr = strstr(pReadBuf,"Encryption: ")){

			pTmpPtr += strlen("Encryption: ");
			strcpy(strType[3],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"addr:")){
			pTmpPtr += strlen("addr:");
			strcpy(strType[4],pTmpPtr);
		}else if(pTmpPtr = strstr(pReadBuf,"gateway:")){
			pTmpPtr += strlen("gateway:");
			strcpy(strType[5],pTmpPtr);
		
		}else if(pTmpPtr = strstr(pReadBuf,"Mask:")){
			pTmpPtr += strlen("Mask:");
			strcpy(strType[6],pTmpPtr);
		
		}

	}
	if(strConfigFilePath == AP_WIFISTATUS)
	{
		sprintf(strWifiMessage, 
				"{\"ap_ssid\":%s,\"ap_bssid\":\"%s\",\"ap_channel\":\"%s\",\"ap_addr\":\"%s\",\"ap_mask\":\"%s\",\"ap_gateway\":\"%s\"}", 
				strType[0], strType[1], strType[2], strType[4],strType[6],strType[5]);		
		write(lcfd.m_iConnFd, strWifiMessage, strlen(strWifiMessage));
	}else if(strConfigFilePath == STA_WIFISTATUS){
		sprintf(strWifiMessage, 
				",{\"sta_ssid\":%s,\"sta_bssid\":\"%s\",\"sta_channel\":\"%s\",\"sta_addr\":\"%s\",\"sta_mask\":\"%s\",\"sta_gateway\":\"%s\"}", 
				strType[0], strType[1], strType[2], strType[4],strType[6],strType[5]);		
		write(lcfd.m_iConnFd, strWifiMessage, strlen(strWifiMessage));

	}
	fclose(fConfig);

	return 0;

}


int DoScanWifi(cfd lcfd)
{

	int i,j=0;
	char strType[11][100] = {"",};
	
	char	*pTmpPtr	= NULL,
			pReadBuf[512];

	int		iReadBytes;
	char *p;
	char strConfigFilePath[]	= WIFILIST_CONFIG;
	char strWifiMessage[512] = "";

	FILE	*fConfig = fopen(strConfigFilePath, "r");

	if(fConfig == NULL)
	{
		LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Failed to open wifilist file to read!\n");
		return -2;
	}
	for(i=0;i<11;i++)
		bzero(strType[i], sizeof(strType[i]));
	write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
	write(lcfd.m_iConnFd, "{\"wifimessage\":[", strlen("{\"wifimessage\":["));

	/* read file's messages*/
	while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
	{
		int len = strlen(pReadBuf);
		pReadBuf[len-1] = '\0';
		j++;
		if(j > 5){
			if(!strcmp(strType[1],"unknown")){
				j = 0;
				for(i=0;i<11;i++)
					bzero(strType[i], sizeof(strType[i]));
			}else{
				/* post messages to web */
				sprintf(strWifiMessage, 
						"{\"ssid\":%s,\"bssid\":\"%s\",\"channel\":\"%s\",\"encryption\":\"%s\"}", 
						strType[1], strType[0], strType[2], strType[4]);		
				write(lcfd.m_iConnFd, strWifiMessage, strlen(strWifiMessage));			
				write(lcfd.m_iConnFd, ",", strlen(","));			
				j=0;
				for(i=0;i<11;i++)
					bzero(strType[i], sizeof(strType[i]));
			}
		}
		if((pTmpPtr = strstr(pReadBuf,"Address: ")) != NULL)
		{
			pTmpPtr += strlen("Address: ");
			strcpy(strType[0],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"ESSID: ")){

			pTmpPtr += strlen("ESSID: ");
			strcpy(strType[1],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"Channel: ")){

			pTmpPtr += strlen("Channel: ");
			strcpy(strType[2],pTmpPtr);

		}else if(pTmpPtr = strstr(pReadBuf,"Signal: ")){

			pTmpPtr += strlen("Signal: ");
			strcpy(strType[3],pTmpPtr);
			if(p = strstr(strType[3]," dBm"))
				*p = '\0';

		}else if(pTmpPtr = strstr(pReadBuf,"Encryption: ")){

			pTmpPtr += strlen("Encryption: ");
			strcpy(strType[4],pTmpPtr);

		}

	}

	write(lcfd.m_iConnFd,"]}",strlen("]}"));

	fclose(fConfig);
	return 0;
}

int DoWifiConfigUpdate(void *arg, S_REQUEST req)
{

	char strType[11][100] = {"",};
	int i_Ret;
	char 		buffer[BUFFER_SIZE] = {0};
	char	*pTmpPtr	= NULL,
			pReadBuf[512];
	int		iReadBytes;
	char 		*pToken 		= NULL,
				*p_Token 		= NULL,
				*pSavePtr 		= NULL,
				*pParamValue[MAXPARAM]  = {NULL},
				*pParamName[MAXPARAM]	= {NULL};
	char *p = NULL;

	char *pHeadPtr = arg;
	int url_len;

	pHeadPtr += strlen("/param.cgi?action=update&group=wifi");
	pToken = strtok_r(pHeadPtr,"&",&pSavePtr);

	pthread_mutex_lock(&g_hMutex);
	load_config(UCI_WIRELESS_CONFIG,g_sServer.httplog);
	while(pToken != NULL)
	{
		pParamName[0] = strtok(pToken,"=");
		pParamValue[0] = pParamName[0] + strlen(pParamName[0]) + 1;
		if(p = strstr(pParamValue[0]," HTTP/1.1"))
			*p = '\0';

		url_len = MIN(MAX(strspn(pParamValue[0],"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-= 1234567890%./`~!@#$^&*()+|][}{;:?.<,"),0),100);
		if((req.m_strParameter = malloc(url_len + 1)) == NULL)
		{
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"malloc failed\n");
			return -1;
		}

		memset(req.m_strParameter,0,url_len+1);
		strncpy(req.m_strParameter,pParamValue[0],url_len);
		if(unescape(req.m_strParameter) == -1)
		{
			free(req.m_strParameter);
			return -1;
		}

		if(!strcmp(pParamName[0],"ap_ssid"))
		{

			pParamName[0] = strtok(pParamName[0],"_");
			pParamName[1] = pParamName[0] + strlen(pParamName[0]) + 1;
			i_Ret = change_value(UCI_WIRELESS_CONFIG,pParamName[0],pParamName[1],req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"ap_auth_key"))
		{
			i_Ret = change_value(UCI_WIRELESS_CONFIG,"ap","key",req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"ap_encrypt_type")){

			i_Ret = change_value(UCI_WIRELESS_CONFIG,"ap","encryption",req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"ap_channel")){

			i_Ret = change_value(UCI_WIRELESS_CONFIG,"radio0","channel",req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"sta_ssid")){

			pParamName[0] = strtok(pParamName[0],"_");
			pParamName[1] = pParamName[0] + strlen(pParamName[0]) + 1;
			i_Ret = change_value(UCI_WIRELESS_CONFIG,pParamName[0],pParamName[1],req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"sta_auth_key")){

			i_Ret = change_value(UCI_WIRELESS_CONFIG,"sta","key",req.m_strParameter,g_sServer.httplog);

		}else if(!strcmp(pParamName[0],"sta_encrypt_type")){

			i_Ret = change_value(UCI_WIRELESS_CONFIG,"sta","encryption",req.m_strParameter,g_sServer.httplog);

		}else{
			i_Ret = -1;//commands error	
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Not has this option\n");
			return i_Ret;
		}
		pToken = strtok_r(NULL,"&",&pSavePtr);
		free(req.m_strParameter);
	}
	const char *value_ssid = search_config("sta","ssid");
	const char *value_key =  search_config("sta","key");
	if((value_ssid != NULL))
	{
		int i,j=0,k=0;
		char strConfigFilePath[]	= WIFILIST_CONFIG;

		FILE	*fConfig = fopen(strConfigFilePath, "r");

		if(fConfig == NULL)
		{
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"open wireless failed\n");
			return -1;
		}
		for(i=0;i<11;i++)
			bzero(strType[i], sizeof(strType[i]));

		while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
		{
			int len = strlen(pReadBuf);
			pReadBuf[len-1] = '\0';
			j++;
			if(pTmpPtr = strstr(pReadBuf,"ESSID: ")){
				pTmpPtr += strlen("ESSID:  ");
				strcpy(strType[0],pTmpPtr);
				if(p = strstr(strType[0],"\""))
					*p = '\0';
				if(!strcmp(strType[0],value_ssid)){
					j = 0;
					k = 1;
				}
			}
			if(k == 1){
				if(j == 1){
					if(pTmpPtr = strstr(pReadBuf,"Channel: ")){

						pTmpPtr += strlen("Channel: ");
						strcpy(strType[1],pTmpPtr);
					} 
				}
				if(j == 3){

					if(pTmpPtr = strstr(pReadBuf,"Encryption: ")){

						pTmpPtr += strlen("Encryption: ");
						strcpy(strType[2],pTmpPtr);

						if(strstr(strType[2],"WPA PSK")){
							strcpy(strType[3],"psk");
						}else if(strstr(strType[2],"WPA2 ")){
							strcpy(strType[3],"psk2");
						}else{
							strcpy(strType[3],"none");
						}
						k = 0;
					}
				}
			}
		}
		fclose(fConfig);

		i_Ret = change_value(UCI_WIRELESS_CONFIG,"sta","encryption",strType[3],g_sServer.httplog);

	}
	unload_config();
	pthread_mutex_unlock(&g_hMutex);
	return 0;
}


/******************************************************************************
Description.: Serve a connected TCP-client. This thread function is called
              for each connect of a HTTP client like a webbrowser. It determines
              if it is a valid HTTP request and dispatches between the different
              response options.
Input Value.: arg is the filedescriptor and server-context of the connected TCP
              socket. It must have been allocated so it is freeable by this
              thread function.
Return Value: always NULL
******************************************************************************/
/* thread for clients that connected to this server */
void *client_thread(void *arg)
{
	int ret;
    int 		cnt;
//    char input_suffixed = 0;
    int 		input_number = 0;
	int 		reboot_network = 0;
	int 		mled_flag = 0;
    char 		buffer[BUFFER_SIZE] = {0}, 
		 		*pHeadPtr = buffer;
    iobuffer 	iobuf;
    S_REQUEST 	req;
    cfd 		lcfd; /* local-connected-file-descriptor */
/*******************swt**********************************/
	char 		*pToken 		= NULL,
				*p_Token 		= NULL,
				*pSavePtr 		= NULL,
				*pParamValue[MAXPARAM]  = {NULL},
				*pParamName[MAXPARAM]	= {NULL};

	char *p = NULL;
	char strWifiMessage[512] = "";
	char strType[12][100] = {"",};

	char	*pTmpPtr	= NULL,
			pReadBuf[512];
	int		iReadBytes;
	const char *value;
	int free_tunnelflag = 0;
	int i_Ret;
	SMsgIoctrlAvsGetAuth *msg_avs_get_data;
	SMsgIoctrlAvsSetAuth *msg_avs_set_data = (SMsgIoctrlAvsSetAuth *)malloc(sizeof(SMsgIoctrlAvsSetAuth));
	SMsgIoctrlTunnel *msg_tunnel_data = (SMsgIoctrlTunnel *)malloc(sizeof(SMsgIoctrlTunnel));



//	int iTunnelQuit = 0;	
//	size_t msg_avs_set_len = sizeof(msg_avs_set_data);
//	size_t msg_avs_get_len = sizeof(msg_avs_get_data);

/*********gz add for upgrade*****************/	
	int		iOtaContentLengths		= 0;
	char 		*strBoundary	 	= NULL,
			*strContentLength	= NULL;
/*********gz add for upgrade end ************/				
    /* we really need the fildescriptor and it must be freeable by us */
    if(arg != NULL) {
        memcpy(&lcfd, arg, sizeof(cfd));
        free(arg);
    } else
        return NULL;

    /* initializes the structures */
    init_iobuffer(&iobuf);
//	printf("init iobuffer\n");
    init_request(&req);

//	printf("init request\n");
//	printf("iobuf 1:%s\n",iobuf.buffer);

    /* What does the client want to receive? Read the request. */
    memset(buffer, 0, sizeof(buffer));

	//read(lcfd.m_iConnFd, buffer, sizeof(buffer));
	//printf("Request string: %s\n", buffer);
	
    if((cnt = _readline(lcfd.m_iConnFd, &iobuf, buffer, sizeof(buffer) - 1, 10)) == -1) {
		
        close(lcfd.m_iConnFd);
        return NULL;
    }

	if(buffer[0] == '/')
		lcfd.m_bNoHeader = TRUE;

    /* determine what to deliver */
    if(strstr(buffer, "GET /?action=snapshot") != NULL) {
        req.m_eDataType = A_SNAPSHOT;
#ifdef WXP_COMPAT
    } else if((strstr(buffer, "GET /cam") != NULL) && (strstr(buffer, ".jpg") != NULL)) {
        req.m_eDataType = A_SNAPSHOT;
#endif
       //input_suffixed = 255;
    } else if(strstr(buffer, "GET /?action=stream") != NULL) {
       // input_suffixed = 255;
        req.m_eDataType = A_STREAM;
#ifdef WXP_COMPAT
    } else if((strstr(buffer, "GET /cam") != NULL) && (strstr(buffer, ".mjpg") != NULL)) {
        req.m_eDataType= A_STREAM;
#endif
        //input_suffixed = 255;
    } else if((strstr(buffer, "GET /input") != NULL) && (strstr(buffer, ".json") != NULL)) {
        req.m_eDataType = A_INPUT_JSON;
        //input_suffixed = 255;
    } else if((strstr(buffer, "GET /output") != NULL) && (strstr(buffer, ".json") != NULL)) {
        req.m_eDataType = A_OUTPUT_JSON;
        //input_suffixed = 255;
    } else if(strstr(buffer, "GET /program.json") != NULL) {
        req.m_eDataType = A_PROGRAM_JSON;
        //input_suffixed = 255;
    } else if((pHeadPtr = strstr(buffer, "GET /name.service")) != NULL) {
        req.m_eDataType = A_DEV_NAME;
		req.m_strParameter = strdup(pHeadPtr);
	} else if((pHeadPtr = strstr(buffer, "GET /group.service")) != NULL) {
        req.m_eDataType = A_DEV_GROUP;
		req.m_strParameter = strdup(pHeadPtr);	
/*********gz add for upgrade*****************/	
    } else if((pHeadPtr = strstr(buffer, "/fwupgrade.cgi")) != NULL) {
        req.m_eDataType = A_UPGRADE;
		if((RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_UPDATE_FLASHING)) < 0)
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
    } 

/*****************************************add http_api by swt***********************************************/

	/* command to change wireless config */
	else if(pHeadPtr = strstr(buffer,"/param.cgi?action=update&group=wifi")){

		req.m_eDataType = A_CH_WIRELESS;
		system("iwinfo ra0 scan > /tmp/wifilist.conf");

		i_Ret = DoWifiConfigUpdate(pHeadPtr,req);
		if(i_Ret < 0)
		{
			send_error(lcfd.m_iConnFd,500,"could not properly unescape command parameter string");	
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"could not properly unescape command parameter string\n");
			close(lcfd.m_iConnFd);
			return NULL;
		}
			
	/* command to recover wireless from sta mode to ap mode */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=resetboard")){
	
		req.m_eDataType = A_REST_BOARD;
	
	/* command to restart board */
	}else if(pHeadPtr = strstr(buffer,"/restart.cgi")){
		
		req.m_eDataType = A_RESTART;
	
	/* command to restart wifi */	
	}else if(pHeadPtr = strstr(buffer,"/param.cgi?action=restart_net")){
	
		req.m_eDataType = A_RESTART_NET;
	
	/* command to get nearby routers' message */
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=get_wifilist")){
		
		/* echo routers' messages to a file, then read & post to web */
		req.m_eDataType = A_WIFI_SCAN;
		system("iwinfo ra0 scan > /tmp/wifilist.conf");
	
	/* command to get current wifi status */	
	}else if((strstr(buffer,"/server.command?command=get_wifistatus")) != NULL){
		req.m_eDataType = A_WIFI_STATUS;

	/* command to change local language */
	}else if(pHeadPtr = strstr(buffer,"/param.cgi?action=ch_language&value=")){
		char lang_data[10]; //buff for language

		if(p = strstr(pHeadPtr," HTTP/1.1"))
			*p = '\0';
		req.m_eDataType = A_CH_LANGUAGE;
		pHeadPtr += strlen("/param.cgi?action=ch_language&value=");
		strcpy(lang_data,pHeadPtr);

		if(!(strcmp(lang_data,"en-GB") && strcmp(lang_data,"en-US") && strcmp(lang_data,"de-DE"))){
			
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, lang_data, sizeof(lang_data), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVS_SETLANGUAGE)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
		
		}else{

			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Language Changed Error\n");
			i_Ret = -1;
		}

	/* command to get local language */
	}else if(pHeadPtr = strstr(buffer,"/param.cgi?action=get_language")){

		req.m_eDataType = A_GET_LANGUAGE;
		
	/* command to get code from board when login alexa */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=login&type=request")){

		req.m_eDataType = A_LOGIN_GET_DATA;
		
	/* command to set code to server when longin alexa */
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=login&type=response")){

		req.m_eDataType = A_LOGIN_SET_DATA;
		pHeadPtr += strlen("/server.command?command=login&type=response");
		pToken = strtok_r(pHeadPtr,"&",&pSavePtr);

		memset(msg_avs_set_data,0,sizeof(SMsgIoctrlAvsSetAuth));
		printf("start set data\n");	
		while(pToken != NULL)
		{
			pParamName[0] = strtok(pToken,"=");
			pParamValue[0] = pParamName[0] + strlen(pParamName[0]) + 1;
			if(p = strstr(pParamValue[0]," HTTP/1.1"))
				*p = '\0';
	
			if(!strcmp(pParamName[0],"client_id"))
			{

				strncpy(msg_avs_set_data->client_id , pParamValue[0],strlen(pParamValue[0]));
			
			}else if(!strcmp(pParamName[0],"redirect_uri"))
			{
			
				strncpy(msg_avs_set_data->redirect_uri , pParamValue[0],strlen(pParamValue[0]));
			
			}else if(!strcmp(pParamName[0],"authorize_code")){
				strncpy(msg_avs_set_data->authorize_code , pParamValue[0] ,strlen(pParamValue[0]));

			}else{
				i_Ret = -1;//commands error	
				break;
			}
			pToken = strtok_r(NULL,"&",&pSavePtr);
		}

	/* command to logout alexa */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=logout")){

		req.m_eDataType = A_LOGOUT;


	/* command to open tunnel */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=open_tunnel")){
		req.m_eDataType = A_OPEN_TUNNEL;

	/* command to close tunnel */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=close_tunnel")){
		req.m_eDataType = A_CLOSE_TUNNEL;

	/* command to set tunnel_uart baudrate */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=tunnel&set_baudrate&value=")){
		req.m_eDataType = A_SET_BAUDRATE;
		pHeadPtr += strlen("/server.command?command=tunnel&set_baudrate&value=");
		memset(msg_tunnel_data,0,sizeof(SMsgIoctrlTunnel));

		if(p = strstr(pHeadPtr," HTTP/1.1"))
			*p = '\0';

		msg_tunnel_data->baudrate = atoi(pHeadPtr);

	/* command to get tunnel_uart baudrate */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=tunnel&get_baudrate")){
		req.m_eDataType = A_GET_BAUDRATE;

	/* command to set tunnel_tcp port */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=tunnel&set_port&value=")){
		req.m_eDataType = A_SET_TUNNEL_PORT;
		pHeadPtr += strlen("/server.command?command=tunnel&set_port&value=");
		memset(msg_tunnel_data,0,sizeof(SMsgIoctrlTunnel));

		if(p = strstr(pHeadPtr," HTTP/1.1"))
			*p = '\0';

		msg_tunnel_data->port = atoi(pHeadPtr);

	/* command to get tunnel_tcp port */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=tunnel&get_port")){
		req.m_eDataType = A_GET_TUNNEL_PORT;

	/* command to get version */	
	}else if(pHeadPtr = strstr(buffer,"/server.command?command=get_version")){
		req.m_eDataType = A_GET_VERSION;
	}else if(pHeadPtr = strstr(buffer,"favicon.ico")){
		req.m_eDataType = A_IGNORE;

	}else {//request a file
        int len;

        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"try to serve a file\n");
        req.m_eDataType = A_FILE;
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"request a fil buffer:%s\n", buffer);
        if((pHeadPtr = strstr(buffer, "GET /")) == NULL) {
            LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"HTTP request seems to be malformed\n");
            send_error(lcfd.m_iConnFd, 400, "Malformed HTTP request");
            close(lcfd.m_iConnFd);
            return NULL;
        }

        pHeadPtr += strlen("GET /");
        len = MIN(MAX(strspn(pHeadPtr, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890`~!@#$%^&*()=+|[]{};:?"), 0), 100);
        req.m_strParameter = malloc(len + 1);
        if(req.m_strParameter == NULL) {
            exit(EXIT_FAILURE);
        }
        memset(req.m_strParameter, 0, len + 1);
        strncpy(req.m_strParameter, pHeadPtr, len);

        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"strParameter (len: %d): \"%s\"\n", len, req.m_strParameter);
    }

#if 0
    /*
     * Since when we are working with multiple input plugins
     * there are some url which could have a _[plugin number suffix]
     * For compatibility reasons it could be left in that case the output will be
     * generated from the 0. input plugin
     */
    if(input_suffixed) {
        char *sch = strchr(buffer, '_');
        if(sch != NULL) {  // there is an _ in the url so the input number should be present
            DBG("sch %s\n", sch + 1); // FIXME if more than 10 input plugin is added
            char numStr[3];
            memset(numStr, 0, 3);
            strncpy(numStr, sch + 1, 1);
            input_number = atoi(numStr);
        }
        DBG("input plugin_no: %d\n", input_number);
    }
#endif
    /*
     * parse the rest of the HTTP-request
     * the end of the request-header is marked by a single, empty line with "\r\n"
     */
    do {
        memset(buffer, 0, sizeof(buffer));

        if((cnt = _readline(lcfd.m_iConnFd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
            free_request(&req);
            close(lcfd.m_iConnFd);
            return NULL;
        }
        if(strstr(buffer, "User-Agent: ") != NULL) {
            req.m_strClient = strdup(buffer + strlen("User-Agent: "));
        } else if(strstr(buffer, "Authorization: Basic ") != NULL) {
            req.credentials = strdup(buffer + strlen("Authorization: Basic "));
            decodeBase64(req.credentials);
            LOG_P(g_sServer.httplog,RAK_LOG_FINE,"username:password: %s\n", req.credentials);
/*********gz add for upgrade*****************/				
        }  else if((pHeadPtr = strstr(buffer, "Content-Length: ")) != NULL)// Parse content length
	{
		pHeadPtr += strlen("Content-Length: ");
		
//		size_t strcnt = strspn(pHeadPtr,"1234567890");
//		if((strContentLength = strndup(pHeadPtr, strcnt)) == NULL)
		if((strContentLength = strndup(pHeadPtr, strspn(pHeadPtr, "1234567890"))) == NULL)
		{
			send_error(lcfd.m_iConnFd, 500, "Could not allocate memory!");
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"could not allocate memory\n");
			goto LABEL_CLIENT_THREAD_EXIT;
		}
		else
		{
			iOtaContentLengths = strtol(strContentLength, NULL, 10);  //str to num;			
			free(strContentLength);			
			LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iOtaContentLengths: %d\n", iOtaContentLengths);	//file length;
		}
	}
	// Parse boundary string
	else if((pHeadPtr = strcasestr(buffer, "boundary=")) != NULL)
	{
		pHeadPtr += strlen("boundary=");
		
		if((strBoundary = strndup(pHeadPtr, strspn(pHeadPtr, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-1234567890"))) == NULL)
		{
			send_error(lcfd.m_iConnFd, 500, "could not allocate memory");
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"could not allocate memory\n");
			goto LABEL_CLIENT_THREAD_EXIT;
		}
		
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"strBoundary: %s - %d bytes\n", strBoundary, strlen(strBoundary));  //print boundary
/*********gz add for upgrade end************/				
	}

    } while(cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n'));
#if 0 // don't need check username and password
    /* check for username and password if parameter -c was given */
    if(lcfd.m_psContext->conf.credentials != NULL) {
        if(req.credentials == NULL || strcmp(lcfd.m_psContext->conf.credentials, req.credentials) != 0) {
            DBG("access denied\n");
            send_error(lcfd.m_iConnFd, 401, "username and password do not match to configuration");
            close(lcfd.m_iConnFd);
            if(req.m_strParameter != NULL) free(req.m_strParameter);
            if(req.m_strClient != NULL) free(req.m_strClient);
            if(req.credentials != NULL) free(req.credentials);
            return NULL;
        }
        DBG("access granted\n");
    }
#endif
    /* now it's time to answer */
#if 0
    if(!(input_number < pglobal->incnt)) {
        DBG("Input number: %d out of range (valid: 0..%d)\n", input_number, pglobal->incnt-1);
        send_error(lcfd.m_iConnFd, 404, "Invalid input plugin number");
        req.m_eDataType = A_UNKNOWN;
    }
#endif
    switch(req.m_eDataType) {
    case A_SNAPSHOT:
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for snapshot from input: %d\n", input_number);
     //   send_snapshot(lcfd.m_iConnFd, input_number);
        break;
    case A_STREAM:
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for stream from input: %d\n", input_number);
       // send_stream(lcfd.m_iConnFd, input_number);
        break;
	case A_DEV_NAME:
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for device name.services.\n");
		//DoParamUpdate(lcfd.m_iConnFd, req.m_strParameter);
		break;
	case A_DEV_GROUP:
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for device name.services.\n");
		//DoZoneActivity(lcfd.m_iConnFd, req.m_strParameter);
		break;
    case A_COMMAND:
        if(lcfd.m_psContext->conf.nocommands) {
            send_error(lcfd.m_iConnFd, 501, "this server is configured to not accept commands");
            break;
        }
       // command(lcfd.m_psContext->id, lcfd.m_iConnFd, req.m_strParameter);
        break;
    case A_INPUT_JSON:
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for the Input plugin descriptor JSON file\n");
        //send_Input_JSON(lcfd.m_iConnFd, input_number);
        break;
    case A_OUTPUT_JSON:
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for the Output plugin descriptor JSON file\n");
        //send_Output_JSON(lcfd.m_iConnFd, input_number);
        break;
    case A_PROGRAM_JSON:
        LOG_P(g_sServer.httplog,RAK_LOG_FINE,"Request for the program descriptor JSON file\n");
      //  send_Program_JSON(lcfd.m_iConnFd);
        break;
    case A_FILE:  
        if(lcfd.m_psContext->conf.www_folder == NULL)
            send_error(lcfd.m_iConnFd, 501, "no www-folder configured");
        else
            send_file(lcfd.m_psContext->id, lcfd.m_iConnFd, req.m_strParameter);
        break;
	case A_UPGRADE:
		{
			if((iOtaContentLengths == 0)||(strBoundary == NULL))  //http error
			{	
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Receiving data message is error !\n");
				sprintf(buffer,"HTTP/1.1 200 OK\r\n"STD_HEADER"Content-type: text/html\r\n\r\n{\"value\": \"%d\"}", eHTTP_ERRCODE_ANOMALY_RESAVEFW);
				write(lcfd.m_iConnFd, buffer, strlen(buffer));
				goto LABEL_CLIENT_THREAD_EXIT;
				
			}

		//	U_MSF_PLUGIN_CMD	 ucmd;
		//	sendDate_t			 op_data;
			uint8_t u8date = 1;	
		//	ucmd.m_uiCmd = eMSP_SPI_CMD_7628_UPDATE;
		//	op_data.len  = 1;
		//	op_data.date = &u8date;

			//ret = pglobal->m_asPluginPriv[eMSF_PLUGIN_ID_SPI_CMD].m_pPluginIF->m_pfnCommand(ucmd, (const void*)&op_data, NULL);
			if (ret <0)
			{
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR," send bt cmd error !!!\n");
			}
			LOG_P(g_sServer.httplog,RAK_LOG_FINE,"iOtaContentLengths: %d\n", iOtaContentLengths);	//file length;
			DoFirmwareUpgrade(lcfd.m_iConnFd, iOtaContentLengths, strBoundary , iobuf );
			if(strBoundary != NULL)
				free(strBoundary);
			u8date = 0;	
			//pglobal->m_asPluginPriv[eMSF_PLUGIN_ID_SPI_CMD].m_pPluginIF->m_pfnCommand(ucmd, (const void*)&op_data, NULL);

		}
		break;
	case A_CH_WIRELESS:
		{
			 // post value to web
			 POST_STATE(buffer,i_Ret);
			 write(lcfd.m_iConnFd,buffer,strlen(buffer));
		}
		break;

	case A_REST_BOARD:
		{
			// recoverboard & tell board to logout alexa
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVSLOGOUT)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer,strlen(buffer));
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_OFF)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			system("recoverboard.sh");
		}
		break;

	case A_RESTART:
		{
			// restart board & post value to web
			 POST_STATE(buffer,0);
			 write(lcfd.m_iConnFd,buffer,strlen(buffer));
		
			 system("reboot");
		}
		break;

	case A_RESTART_NET:
		{
			// restart wifi & post value to web
			pthread_mutex_lock(&g_hMutex);
			load_config(UCI_WIRELESS_CONFIG,g_sServer.httplog);
			const char *value_ssid = search_config("sta","ssid");
			if(value_ssid != NULL)
				i_Ret = change_value(UCI_WIRELESS_CONFIG,"radio0","linkit_mode","sta",g_sServer.httplog);
			else
				mled_flag = 1;
			unload_config();
			pthread_mutex_unlock(&g_hMutex);
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer,strlen(buffer));
			reboot_network=1;
		}
		break;

	case A_WIFI_STATUS:
		{
			// get wifistatus & post value to web
			system("wifistatus.sh");

			write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
			write(lcfd.m_iConnFd, "{\"wifistatus\":[", strlen("{\"wifistatus\":["));
			read_wifistatus(AP_WIFISTATUS,lcfd);
			read_wifistatus(STA_WIFISTATUS,lcfd);
			write(lcfd.m_iConnFd,"]}",strlen("]}"));
			sleep(1);
			system("rm /tmp/*_wifistatus.conf");
			sync();
		}
		break;
		

	case A_WIFI_SCAN:
		{
			// function to scan nearby wifi 
			DoScanWifi(lcfd);
		}
		break;

	case A_CH_LANGUAGE:
		{
			// change language post value to web
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer,strlen(buffer));
		}
		break;

	case A_GET_LANGUAGE:
		{
			char strLocalLang[32] = {0, };
			char local_lang[10];
			if((RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVS_GETLANGUAGE)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");

			SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + sizeof(local_lang));

			memset(msgData,0,sizeof(msgData));
			memset(local_lang,0,sizeof(local_lang));

			RK_RecvIOCtrl(lcfd.m_psContext->msg_id, msgData, sizeof(SMsgIoctrlData) + sizeof(local_lang), eIOTYPE_USER_MSG_HTTPD);
			if(msgData->u32iCommand == eIOTYPE_MSG_AVS_GETLANGUAGE)
			{
				strcpy(local_lang, msgData->next);
				sprintf(strLocalLang, "{\"local language\":\"%s\"}", local_lang);		
				
				write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
				write(lcfd.m_iConnFd, strLocalLang, strlen(strLocalLang));

			}

			free(msgData);
		}
		break;


	case A_LOGIN_GET_DATA:
		{
			// get code from board when login alexa & post value to web
			printf("start to get data\n");
			char strLoginGet[1024] = {0,};
			SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlAvsGetAuth));
			if(msgData == NULL)
				i_Ret = -1;
			memset(msgData,0,sizeof(msgData));
			memset(strLoginGet,0,sizeof(strLoginGet));

			if((RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVSLOGIN_GETAUTH)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");

			
			RK_RecvIOCtrl(lcfd.m_psContext->msg_id, msgData, sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlAvsGetAuth), eIOTYPE_USER_MSG_HTTPD);
			if(msgData->u32iCommand == eIOTYPE_MSG_AVSLOGIN_GETAUTH)
			{
				msg_avs_get_data = (SMsgIoctrlAvsGetAuth *)msgData->next;
				sprintf(strLoginGet, 
						"{\"product_id\":\"%s\",\"product_dsn\":\"%s\",\"codechallengemethod\":\"%s\",\"codechallenge\":\"%s\"}", 
						msg_avs_get_data->product_id, msg_avs_get_data->product_dsn, msg_avs_get_data->code_method, msg_avs_get_data->code_challenge);		
				
				write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
				write(lcfd.m_iConnFd, strLoginGet, strlen(strLoginGet));	

			}
			else
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"error cmd: %d\n",msgData->u32iCommand);
			free(msgData);
		}
		break;

	case A_LOGIN_SET_DATA:
		{
			// set code to server when login alexa & post value to web
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, msg_avs_set_data, sizeof(SMsgIoctrlAvsSetAuth), eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVSLOGIN_SETTOKEN)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + sizeof(int));
			//SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + 10);
			if(msgData == NULL){

				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"malloc failed\n");
				break;
			}
			
			memset(msgData,0,sizeof(msgData));

			RK_RecvIOCtrl(lcfd.m_psContext->msg_id, msgData, sizeof(SMsgIoctrlData) + sizeof(int), eIOTYPE_USER_MSG_HTTPD);
			if(msgData->u32iCommand == eIOTYPE_MSG_AVSLOGIN_RESPONSE)
			{
				i_Ret = (int)msgData->next[0];
				LOG_P(g_sServer.httplog,RAK_LOG_FINE,"=====>return value: %d\n",i_Ret);
				POST_STATE(buffer,i_Ret);
				write(lcfd.m_iConnFd,buffer,strlen(buffer));
			}
			else
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"error cmd: %d\n",msgData->u32iCommand);
			free(msgData);
		}
		break;

	case A_LOGOUT:
		{
			// logout alexa 
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_AVS, eIOTYPE_MSG_AVSLOGOUT)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer, strlen(buffer));	
		}
		break;
	
	case A_OPEN_TUNNEL:
		{
			// open tunnel
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_START)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer,strlen(buffer));
		}
		break;

	case A_CLOSE_TUNNEL:
		{
			// close tunnel
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_STOP)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer,strlen(buffer));
		}
		break;

	case A_SET_BAUDRATE:
		{
			// set tunnel_uart baudrate & post to web
			free_tunnelflag = 1;
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, msg_tunnel_data, sizeof(SMsgIoctrlTunnel), eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_SETBAUDRATE)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer, strlen(buffer));	
				
		}
		break;

	case A_GET_BAUDRATE:
		{
			// get tunnel_uart baudrate & post to web
			free_tunnelflag = 1;
			SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlTunnel));

			memset(msgData,0,sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlTunnel));
			memset(msg_tunnel_data,0,sizeof(SMsgIoctrlTunnel));

			if((RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_GETBAUDRATE)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");

			RK_RecvIOCtrl(lcfd.m_psContext->msg_id,msgData,sizeof(SMsgIoctrlData)+sizeof(SMsgIoctrlTunnel),eIOTYPE_USER_MSG_HTTPD);
			if(msgData->u32iCommand == eIOTYPE_MSG_TUNNEL_GETBAUDRATE)
			{
				msg_tunnel_data = (SMsgIoctrlTunnel *)msgData->next;
				i_Ret = msg_tunnel_data->baudrate;
				POST_STATE(buffer,i_Ret);
				write(lcfd.m_iConnFd,buffer,strlen(buffer));
			}
			free(msgData);

		}
		break;

	case A_SET_TUNNEL_PORT:
		{
			// set tunnel_tcp port & post to web
			free_tunnelflag = 1;
			if((i_Ret = RK_SndIOCtrl(lcfd.m_psContext->msg_id, msg_tunnel_data, sizeof(SMsgIoctrlTunnel), eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_SETPORT)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
			POST_STATE(buffer,i_Ret);
			write(lcfd.m_iConnFd,buffer, strlen(buffer));	
		}
		break;

	case A_GET_TUNNEL_PORT:
		{
			// get tunnel_tcp port & post to web
			free_tunnelflag = 1;
			SMsgIoctrlData *msgData = (SMsgIoctrlData *)malloc(sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlTunnel));
			
			memset(msgData,0,sizeof(SMsgIoctrlData) + sizeof(SMsgIoctrlTunnel));
			memset(msg_tunnel_data,0,sizeof(SMsgIoctrlTunnel));

			if((RK_SndIOCtrl(lcfd.m_psContext->msg_id, NULL, 0, eIOTYPE_USER_MSG_TUNNEL, eIOTYPE_MSG_TUNNEL_GETPORT)) < 0)
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");

			RK_RecvIOCtrl(lcfd.m_psContext->msg_id,msgData,sizeof(SMsgIoctrlData)+sizeof(SMsgIoctrlTunnel),eIOTYPE_USER_MSG_HTTPD);
			if(msgData->u32iCommand == eIOTYPE_MSG_TUNNEL_GETPORT)
			{
				msg_tunnel_data = (SMsgIoctrlTunnel *)msgData->next;
				i_Ret = msg_tunnel_data->port;
				POST_STATE(buffer,i_Ret);
				write(lcfd.m_iConnFd,buffer,strlen(buffer));
			}
			free(msgData);
		}
		break;

	case A_GET_VERSION:
		{
			// get version & post to web
			char strConfigFilePath[]	= VERSION_CONFIG;
		
			FILE	*fConfig = fopen(strConfigFilePath, "r");
			
			if(fConfig == NULL)
			{
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Failed to open wifilist file to read!\n");
				POST_STATE(buffer,-2);
				write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
			 	write(lcfd.m_iConnFd,buffer,strlen(buffer));
				goto LABEL_CLIENT_THREAD_EXIT;
			}
			while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
			write(lcfd.m_iConnFd,DEF_JSON_HEADER,strlen(DEF_JSON_HEADER));
			write(lcfd.m_iConnFd,pReadBuf,strlen(pReadBuf));
		}
		break;

	case A_IGNORE:
		break;

    default:
        LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"unknown request,%d\n", req.m_eDataType);
		break;
    }
	
LABEL_CLIENT_THREAD_EXIT:

    close(lcfd.m_iConnFd);
    free_request(&req);
	free(msg_avs_set_data);
	if(!free_tunnelflag){
		free(msg_tunnel_data);
	}

	// restart wifi 
	if(reboot_network){
		if((RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_SLOWFLASHING)) < 0)
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
		system("wifi up");
	}
	// when update firmware quick blink
	if(mled_flag)
	{
		sleep(5);	
		if((RK_SndIOCtrl(g_sServer.msg_id, NULL, 0, eIOTYPE_USER_MSG_GPIO_WLED, eIOTYPE_MSG_GPIO_WLED_QUICKFLASHING)) < 0)
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"msgsnd failed\n");
		mled_flag = 0;
	}
    LOG_P(g_sServer.httplog,RAK_LOG_FINE,"leaving HTTP client thread\n");
    return NULL;
}

/******************************************************************************
Description.: This function cleans up ressources allocated by the server_thread
Input Value.: arg is not used
Return Value: -
******************************************************************************/
void server_cleanup(void *arg)
{
    context *pcontext = arg;

    OPRINT("cleaning up ressources allocated by server thread #%02d\n", pcontext->id);

    close(pcontext->sd);
}

/******************************************************************************
Description.: Open a TCP socket and wait for clients to connect. If clients
              connect, start a new thread for each accepted connection.
Input Value.: arg is a pointer to the globals struct
Return Value: always NULL, will only return on exit
******************************************************************************/
void *server_thread(void *arg)
{
	struct sockaddr_in addr, client_addr;
	int on;
	pthread_t client;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	context *pcontext = arg;
	//  pglobal = pcontext->pglobal;

	/* set cleanup handler to cleanup ressources */
	pthread_cleanup_push(server_cleanup, pcontext);

	/* open socket for server */
	pcontext->sd = socket(PF_INET, SOCK_STREAM, 0);
	if ( pcontext->sd < 0 ) {
		fprintf(stderr, "socket failed\n");
		exit(EXIT_FAILURE);
	}

	/* ignore "socket already in use" errors */
	on = 1;
	if (setsockopt(pcontext->sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}

	/* perhaps we will use this keep-alive feature oneday */
	/* setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)); */

	/* configure server address to listen to all local IPs */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = pcontext->conf.m_u32Port; /* is already in right byteorder */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if ( bind(pcontext->sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
		perror("bind");
		OPRINT("%s(): bind(%d) failed", __FUNCTION__, htons(pcontext->conf.m_u32Port));
		closelog();
		exit(EXIT_FAILURE);
	}

	/* start listening on socket */
	if ( listen(pcontext->sd, 10) != 0 ) {
		fprintf(stderr, "listen failed\n");
		exit(EXIT_FAILURE);
	}

	/* create a child for every client that connects */
	while ( 1 ) {
		//int *pfd = (int *)malloc(sizeof(int));
		cfd *pcfd = malloc(sizeof(cfd));

		if (pcfd == NULL) {
			fprintf(stderr, "failed to allocate (a very small amount of) memory\n");
			exit(EXIT_FAILURE);
		}

		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"waiting for clients to connect\n");
		pcfd->m_iConnFd = accept(pcontext->sd, (struct sockaddr *)&client_addr, &addr_len);
		pcfd->m_psContext = pcontext;

		/* start new thread that will handle this TCP connected client */
		LOG_P(g_sServer.httplog,RAK_LOG_FINE,"create thread to handle client that just established a connection:%s\n", inet_ntoa(client_addr.sin_addr));
		//syslog(RAK_LOG_INFO, "serving client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		if(g_uiConnThreadCnt >= g_sServer.conf.m_u16ConnLimitCnt)
		{
			LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Out of connection limitation!\n");
			close(pcfd->m_iConnFd);
			free(pcfd);
			continue;
		}
		else{
			pcfd->m_bNoHeader = FALSE;
			if(g_sServer.conf.m_u16Timeout > 0)
			{
				struct timeval sTimeout = { g_sServer.conf.m_u16Timeout, 0 };

				if(setsockopt(pcfd->m_iConnFd, SOL_SOCKET, SO_SNDTIMEO, &sTimeout, sizeof(sTimeout)) < 0)
					LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"Failed to setsockopt SO_SNDTIMEO!\n");
			}

			if( pthread_create(&client, NULL, &client_thread, pcfd) != 0 ) {
				LOG_P(g_sServer.httplog,RAK_LOG_ERROR,"could not launch another client thread\n");
				close(pcfd->m_iConnFd);
				free(pcfd);
				continue;
			}

			pthread_detach(client);
		}

	}

	LOG_P(g_sServer.httplog,RAK_LOG_FINE,"leaving server thread, calling cleanup function now\n");
	pthread_cleanup_pop(1);


	if(g_sServer.conf.www_folder != NULL)
		free(g_sServer.conf.www_folder);

	if(g_sServer.conf.credentials != NULL)
		free(g_sServer.conf.credentials);

	return NULL;
}


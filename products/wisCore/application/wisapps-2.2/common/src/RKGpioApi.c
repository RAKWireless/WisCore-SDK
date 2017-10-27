#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include "../include/RKGpioApi.h"
//
// dir = 'i' for input
// dir = 'o' for output
//
int RK_GpioOpen(int port , int dir)
{
	FILE *fp;
	char port_str[80];
	// equivalent shell command "echo 32 > export" to export the port 
	if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) {
		printf("Cannot open export file.\n");
		return(-1);
	}
	fprintf(fp, "%d", port); 
	fclose(fp);

	// equivalent shell command "echo out > direction" to set the port as an input  
	sprintf(port_str , "/sys/class/gpio/gpio%d/direction" , port);
	if ((fp = fopen(port_str, "rb+")) == NULL) {
		printf("Cannot open direction file\n [%s]\n" , port_str);
		return(-1);
	}
	if (dir == eGPIO_OPEN_INPUT)
		fprintf(fp, "in");

	if (dir == eGPIO_OPEN_OUTPUT)
		fprintf(fp, "out");

	fclose(fp);
	return 0;
}

int RK_GpioValueOpen(int port)
{
	int fd;
	char port_str[80];
	sprintf(port_str,"/sys/class/gpio/gpio%d/value",port);
	if((fd = open(port_str,O_RDONLY)) < 0)
	{
		printf("open value failed  %s\n",port_str);
		return -1;
	}

	return fd;
}

int RK_GpioValueClose(int fd)
{
	close(fd);
	return 0;
}

int RK_GpioWrite(int port, int value)
{
	FILE *fp;
	char port_str[80];

	sprintf(port_str , "/sys/class/gpio/gpio%d/value" , port);
	if ((fp = fopen(port_str, "rb+")) == NULL) {
		printf("Cannot open value file\n [%s]\n", port_str);
		return(-1);
	}
	if(value)
		fwrite("1", 1, 1, fp);
	else
		fwrite("0", 1, 1, fp);
	fclose(fp);
	return (0);
}

int RK_GpioRead(int port)
{
	FILE *fp;
	char port_str[80];
	char buffer[10];
	int value;

	// equivalent shell command "cat value"    		
	sprintf(port_str , "/sys/class/gpio/gpio%d/value" , port);
	if ((fp = fopen(port_str, "rb+")) == NULL) {
		printf("Cannot open value file for input\n[%s]\n",port_str);
		return(-1);
	}
	fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
	value = atoi(buffer);
	fclose(fp);
	return value;
}

// 0-->none, 1-->rising, 2-->falling, 3-->both

int RK_GpioEdge(int port, int edge)

{
	const char dir_str[] = "none\0rising\0falling\0both"; 
	int ptr;
	char path[64];  
	int fd; 

	switch(edge){

	case eGPIO_EDGE_NONE:
		ptr = 0;
		break;

	case eGPIO_EDGE_RISE:
		ptr = 5;
		break;

	case eGPIO_EDGE_FALL:
		ptr = 12;
		break;

	case eGPIO_EDGE_BOTH:
		ptr = 20;
		break;

	default:
		ptr = 0;
	} 

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", port);  
	fd = open(path, O_WRONLY);  
	if (fd < 0) {  
		printf("Failed to open gpio edge for writing!\n");  
		return -1;  
	}  

	if ((write(fd, &dir_str[ptr], strlen(&dir_str[ptr]))) < 0) {  

		printf("Failed to set edge!\n");  

		return -1;  

	}  
	close(fd);  
	return 0;  
}

int RK_GpioClose( int port )
{
	FILE *fp;
	// equivalent shell command "echo gpio > unexport" to export the port 
	if ((fp = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
		printf("Cannot open export file.\n");
		return(-1);
	}
	fprintf(fp, "%d", port); 
	fclose(fp);
	return 0;
}


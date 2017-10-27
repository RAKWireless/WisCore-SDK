#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <uci.h>
#include <stdlib.h>
#include "RKLog.h"

static pthread_rwlock_t	gs_ahRWLock = PTHREAD_RWLOCK_INITIALIZER;
/*************************swt***********************************************/
//#define UCI_CONFIG_FILE "/etc/config/wireless"
static struct uci_context *ctx = NULL;
struct uci_section *sect;
struct uci_element *elem;
struct uci_package *pkg;
char *tmp;
const char *value;
struct uci_ptr ptr;


int load_config(const char *uci_config_file, rak_log_t ucilog)
{
	
	if((ctx = uci_alloc_context()) == NULL)
		LOG_P(ucilog,RAK_LOG_ERROR,"uci_alloc_context failed\n");
	if(UCI_OK != uci_load(ctx,uci_config_file,&pkg))
		LOG_P(ucilog,RAK_LOG_ERROR,"uci_load failed\n");

	return 0;
}

const char *search_config(const char *section,const char *option)
{

	uci_foreach_element(&pkg->sections,elem)
	{
		sect = uci_to_section(elem);
		if(!strcmp("wifi-device",sect->type))
		{
			if(!strcmp(section,sect->e.name))
			{
				value = uci_lookup_option_string(ctx,sect,option);
			}
		}else if(!strcmp("wifi-iface",sect->type)){
			if(!strcmp(section,sect->e.name))
			{
				value = uci_lookup_option_string(ctx,sect,option);
			}
		}
	}
	return value;
}

int change_value(const char *uci_config_file,const char *section,const char *option,const char *value, rak_log_t ucilog)
{
	int ret;
	memset(&ptr,0,sizeof(ptr));
	if((sect = uci_lookup_section(ctx,pkg,section)) == NULL)
	{
		LOG_P(ucilog,RAK_LOG_ERROR,"lookup section failed\n");
		return -2;
	}
	ptr.package = uci_config_file;
	ptr.section = section;
	ptr.option = option;
	ptr.value = value;

	ptr.p = pkg;
	ptr.s = sect;

	if((ret = uci_set(ctx,&ptr)) !=0)
	{
		LOG_P(ucilog,RAK_LOG_ERROR,"uci_set failed\n");
		return -3;
	}
	uci_save(ctx,pkg);
	uci_commit(ctx,&pkg,false);
	return 0;
}
int unload_config(){
	
	uci_unload(ctx,pkg);
	uci_free_context(ctx);
	ctx = NULL;
	return 0;
}
/*************************************swt*************************************/
int 	// > 0: string data, 0: only newline, < 0: EOF
ReadLine(
	char	*pBuf,
	int		iBufByteSize,
	FILE	*stream
)
{
	int     chGet,
			iReadBytes = 0;

	while((chGet = fgetc(stream)) != EOF)
	{
		if(chGet == '\r')
			chGet = '\n';
		
		pBuf[iReadBytes++] = chGet;
		
		if(chGet == '\n')
			break;

		if(iReadBytes == iBufByteSize)
			break;
	}

	if(iReadBytes == 0 && chGet == EOF)
		return -1;

	pBuf[iReadBytes] = 0;

	return iReadBytes;
}	// ReadLine


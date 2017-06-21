#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <uci.h>
#include <stdlib.h>

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


int load_config(const char *uci_config_file)
{
	
	if((ctx = uci_alloc_context()) == NULL)
		printf("uci_alloc_context failed\n");
	if(UCI_OK != uci_load(ctx,uci_config_file,&pkg))
		printf("uci_load failed\n");

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

int change_value(const char *uci_config_file,const char *section,const char *option,const char *value)
{
	int ret;
	memset(&ptr,0,sizeof(ptr));
	if((sect = uci_lookup_section(ctx,pkg,section)) == NULL)
	{
		printf("lookup section failed\n");
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
		printf("uci_set failed\n");
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

int
ReadPluginParamStr(
	char	*strPluginName,
	char	*strParamName,
	char	*strParamValue,
	int		iParamByteSize		// Byte size of strParamValue
)
{
	if(strPluginName == NULL || strParamName == NULL || strParamValue == NULL || iParamByteSize == 0)
		return -1;
	
	char	*strConfigFilePath		= strPluginName;
	
	if(strConfigFilePath == NULL)
		return -1;
	
	if(pthread_rwlock_rdlock(&gs_ahRWLock) != 0)
	{
		printf("Failed to do pthread_rwlock_rdlock because %s!\n", strerror(errno));
		return -1;
	}
	
	FILE	*fConfig = fopen(strConfigFilePath, "r");
	
	char	*pTmpPtr	= NULL,
			pReadBuf[256];
	int		iReadBytes,
			iRet = -1;
	
	if(fConfig == NULL)
	{
		printf("Failed to open configuration file to read!\n");
		iRet = -1;
		goto LABEL_ReadPluginParamStr_UNLOCK;
	}
	
	memset(strParamValue, 0, iParamByteSize);
	
	while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
	{
		if((pTmpPtr = strstr(pReadBuf, strParamName)) == pReadBuf)
		{
			pTmpPtr += strlen(strParamName);
			sscanf(pTmpPtr, " #=%s", strParamValue);
			iRet = 0;
			break;
		}
	}
	
	fclose(fConfig);

LABEL_ReadPluginParamStr_UNLOCK:	
	if(pthread_rwlock_unlock(&gs_ahRWLock) != 0)
		printf("Failed to do pthread_rwlock_unlock because %s\n", strerror(errno));
	
	return iRet;
}	// ReadPluginParamStr

int
WritePluginParamStr(
	char	*strPluginName,
	char	*strParamName,
	char	*strParamValue
)
{
	if(strPluginName == NULL || strParamName == NULL )
		return -1;
	
	char	*strConfigFilePath		= strPluginName,
			*strNewConfigFilePath	= (char *)malloc(strlen(strPluginName) +5);
	
	if(strConfigFilePath == NULL || strNewConfigFilePath == NULL)
		return -1;
	
	sprintf(strNewConfigFilePath, "%s.new", strPluginName);
	
	if(pthread_rwlock_wrlock(&gs_ahRWLock) != 0)
	{
		printf("Failed to do pthread_rwlock_wrlock because %s!\n", strerror(errno));
		return -1;
	}
	
	FILE	*fConfig		= fopen(strConfigFilePath, "r"),
			*fNewConfig 	= fopen(strNewConfigFilePath, "w+");
	char		pReadBuf[256];
	int		iReadBytes,
			iRet = 0;
	int iParamFlag=0;
	
	if(fConfig == NULL || fNewConfig == NULL)
	{
		printf("Failed to open configuration file to write!\n");
		iRet = -1;
		goto LABEL_WritePluginParamStr_UNLOCK;
	}
	while((iReadBytes = ReadLine(pReadBuf, sizeof(pReadBuf), fConfig)) > 0)
	{
		// If strParamName occurs at beginning, rewrite it with new strParamValue
		if(strstr(pReadBuf, strParamName) == pReadBuf)
		{
			if(strParamValue == NULL)
				fprintf(fNewConfig, "%s #=\n", strParamName);
			else
				fprintf(fNewConfig, "%s #=%s\n", strParamName, strParamValue);
			iParamFlag = 1;
		}
		// Keep original strng data for others
		else
			fprintf(fNewConfig, "%s", pReadBuf);
	}
	if(!iParamFlag){
			if(strParamValue == NULL)
				fprintf(fNewConfig, "%s #=\n", strParamName);
			else
				fprintf(fNewConfig, "%s #=%s\n", strParamName, strParamValue);
	}

	
	fclose(fConfig);
	fclose(fNewConfig);
	remove(strConfigFilePath);
	rename(strNewConfigFilePath, strConfigFilePath);
	sync();

LABEL_WritePluginParamStr_UNLOCK:
	if(pthread_rwlock_unlock(&gs_ahRWLock) != 0)
		printf("Failed to do pthread_rwlock_unlock because %s\n", strerror(errno));
	//free(strConfigFilePath);
	free(strNewConfigFilePath);
	return iRet;
}	// WritePluginParamStr

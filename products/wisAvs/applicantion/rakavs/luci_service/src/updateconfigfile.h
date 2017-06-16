#ifndef __UPDATECONFIGFILE_H__
#define __UPDATECONFIGFILE_H__


extern pthread_rwlock_t	gs_ahRWLock;
int 	// > 0: string data, 0: only newline, < 0: EOF
ReadLine(
	char	*pBuf,
	int		iBufByteSize,
	FILE	*stream
);
int WritePluginParamStr(
	char	*strPluginName,
	char	*strParamName,
	char	*strParamValue
);

int
ReadPluginParamStr(
	char	*strPluginName,
	char	*strParamName,
	char	*strParamValue,
	int		iParamByteSize		// Byte size of strParamValue
);

/********************swt***********************/
int load_config(const char *uci_config_file);
int unload_config();
const char *search_config(
		const char *section,
		const char *option
);
int change_value(
		const char *uci_config_file,
		const char *section,
		const char *option,
		const char *value
);

#endif

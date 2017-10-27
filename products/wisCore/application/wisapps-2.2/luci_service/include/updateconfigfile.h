#ifndef __UPDATECONFIGFILE_H__
#define __UPDATECONFIGFILE_H__

#include "RKLog.h"

extern pthread_rwlock_t	gs_ahRWLock;
int 	// > 0: string data, 0: only newline, < 0: EOF
ReadLine(
	char	*pBuf,
	int		iBufByteSize,
	FILE	*stream
);

/********************swt***********************/
int load_config(const char *uci_config_file, rak_log_t ucilog);
int unload_config();
const char *search_config(
		const char *section,
		const char *option
);
int change_value(
		const char *uci_config_file,
		const char *section,
		const char *option,
		const char *value,
		rak_log_t ucilog
);

#endif

/* *************
 * Auth: yanchonggaodian
 * Date: 20170630 
 * Company: RAK
 * *************/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>
#include <time.h>

#include "RKLog.h"



static pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct __log_internal_t__{
	const char              *module;
	log_outputv_callback_t   log_output_v;
	log_outputvs_callback_t  log_output_vs;
	log_level_t              level;
	int                      houroffset;
	void                    *priv;
}log_internal_t;


const char* get_loglevel_str(log_level_t level);
int rak_log_output_vs(char**outaddr, rak_log_t plog, log_level_t level, 
                         const char * filename, const char* funcname, const int lineno, 
						 const char *fmt, va_list va_l);


int rak_log_output_v(rak_log_t plog, log_level_t level, 
						const char * filename, const char* funcname, const int lineno, 
						const char *fmt, va_list va_l);


const char* get_loglevel_str(log_level_t level)
{
	switch(level){
		case RAK_LOG_FATAL:   return "FATAL";
		case RAK_LOG_ERROR:   return "ERROR";
		case RAK_LOG_WARN:    return "WARN ";
		case RAK_LOG_INFO:    return "INFO ";
		case RAK_LOG_DEBUG:   return "DEBUG";
		case RAK_LOG_TRACE:   return "TRACE";
		case RAK_LOG_FINE:    return "FINE ";
		default:
			return " ";
	}
}

rak_log_t rak_log_init(const char *module, log_level_t level, int houroffset, log_outputv_callback_t log_outputv_callback, log_outputvs_callback_t log_outputvs_callback)
{
	log_internal_t * log = (log_internal_t*)malloc(sizeof(log_internal_t));
	if(houroffset > 24)houroffset=24;
	else if(houroffset < -24)houroffset=-24;
	if(log){
		log->module          = module;
		log->level           = level;
		log->houroffset      = houroffset;
		log->log_output_v    = log_outputv_callback;
		log->log_output_vs   = log_outputvs_callback;
		log->priv            = NULL;
	}
	return (rak_log_t)log;
}

rak_log_t rak_log_destroy(rak_log_t log)
{
	if(log){
		free(log);
		log=NULL;
	}
	return log;
}

int rak_log_set_level(rak_log_t log, log_level_t level)
{
	return ((log)? (((log_internal_t*)log)->level = level) : RAK_LOG_STATUS_ERR);
}

log_level_t rak_log_get_level(rak_log_t log)
{
	return (log? ((log_internal_t*)log)->level : RAK_LOG_STATUS_ERR);
}

int rak_log_set_module(rak_log_t log, const char* module)
{
	return ((log)? ((((log_internal_t*)log)->module = module), 0) : -1);
}

const char * rak_log_get_module(rak_log_t log)
{
	return (log? ((log_internal_t*)log)->module : NULL);
}

int rak_log_set_log_outputv_func(rak_log_t log, log_outputv_callback_t func)
{
	return (log? ((((log_internal_t*)log)->log_output_v = func), 0) : -1);
}

log_outputv_callback_t rak_log_get_log_outputv_func(rak_log_t log)
{
	return (log? ((log_internal_t*)log)->log_output_v : NULL);
}

int rak_log_set_log_outputvs_func(rak_log_t log, log_outputvs_callback_t func)
{
	return (log? ((((log_internal_t*)log)->log_output_vs = func), 0) : -1);
}

log_outputvs_callback_t rak_log_get_log_outputvs_func(rak_log_t log)
{
	return (log? ((log_internal_t*)log)->log_output_vs : NULL);
}

int rak_log_set_privdata(rak_log_t log, void * data)
{
	return (log? ((((log_internal_t*)log)->priv = data), 0) : -1);
}

void * rak_log_get_privdata(rak_log_t log)
{
	return (log? ((((log_internal_t*)log)->priv)) : NULL);
}

static int rak_log_sprint_prefix(char* dest, log_internal_t* log, log_level_t level, 
							 const char * filename, const char* funcname, const int lineno)
{
	int prefix_len;
	time_t time_l = time(NULL) + log->houroffset * 60 * 60;
	struct tm tm;
	gmtime_r(&time_l, &tm);

	prefix_len  = dest ? (int)strftime(dest,24,"[ %Y-%m-%d %H:%M:%S ]",&tm) : 23;
	prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, " ");
	int prefix_len_flag = prefix_len;
	if(log->module)
		prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "%s", log->module);
	const char *strlevel = get_loglevel_str(level);
	if(log->module && *strlevel != ' ')
		prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "_");
	if(*strlevel != ' ')
		prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "%s", strlevel);
	if((log->module || *strlevel != ' ') && (filename || funcname)) {
		prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, " ");
	}
	if(filename || funcname){
		if(filename){
			char * filenamedup = strdup(filename);
			prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "%s", basename(filenamedup));
			free(filenamedup);
		}
		if(filename && funcname)
			prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, ":");
		if(funcname)
			prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "%s", funcname);
		if(lineno>=0)
			prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, ":%d", lineno);
	}
	if(prefix_len > prefix_len_flag)
		prefix_len += snprintf(dest ? dest + prefix_len : NULL, dest? (size_t)prefix_len : 0, "$ ");
	
	return prefix_len;
}

static int rak_log_print_prefix(log_internal_t* log, log_level_t level, 
							 const char * filename, const char* funcname, const int lineno)
{
	int prefix_len;
	struct tm tm;
	char timestr[24];
	time_t time_l = time(NULL) + log->houroffset * 60 * 60;
	gmtime_r(&time_l, &tm);
	
	strftime(timestr,24,"[ %Y-%m-%d %H:%M:%S ]",&tm);
	prefix_len = printf("%s ", timestr);
	int prefix_len_flag = prefix_len;
	if(log->module)
		prefix_len += printf("%s", log->module);
	const char *strlevel = get_loglevel_str(level);
	if(log->module && *strlevel != ' ')
		prefix_len += printf("%s", "_");
	if(*strlevel != ' ')
		prefix_len += printf("%s", strlevel);
	if((log->module || *strlevel != ' ') && (filename || funcname)) {
		prefix_len += printf("%s", " ");
	}
	if(filename || funcname){
		if(filename){
			char * filenamedup = strdup(filename);
			prefix_len += printf("%s", basename(filenamedup));
			free(filenamedup);
		}
		if(filename && funcname)
			prefix_len += printf(":");
		if(funcname)
			prefix_len += printf("%s", funcname);
		if(lineno>=0)
			prefix_len += printf(":%d", lineno);
	}
	if(prefix_len > prefix_len_flag)
		prefix_len += printf(": ");
	
	return prefix_len;
}

int rak_log_output_vs(char**outaddr, rak_log_t plog, log_level_t level, 
                         const char * filename, const char* funcname, const int lineno, 
						 const char *fmt, va_list va_l)
{
	int ret = 0;
	pthread_mutex_lock(&logmutex);
	log_internal_t *log=(log_internal_t*)plog;
	if(log->log_output_vs){
		ret = log->log_output_vs(outaddr, plog, level, filename, funcname, lineno, fmt, va_l);
		pthread_mutex_unlock(&logmutex);
		return ret;
	}
	if(level <= log->level){
		int prefix_len, len;
		{
			va_list va_dest;
			va_copy(va_dest, va_l);
			len = (prefix_len = rak_log_sprint_prefix(NULL, log, level, filename, funcname, lineno)) + vsnprintf(NULL, 0, fmt, va_l);
			va_l = va_dest;
		}
		if(outaddr && len>=0){
			char * p;
			p = (char*)realloc(*outaddr, (size_t)len+1);
			if(p){
				*outaddr = p;
			}else{
				rak_log_output_strfree(outaddr);
				*outaddr = (char*)realloc(*outaddr, (size_t)len+1);
			}
			prefix_len = rak_log_sprint_prefix(*outaddr, log, level, filename, funcname, lineno);

			len = prefix_len + vsprintf(((char*)(*outaddr)) + prefix_len, fmt, va_l);

		}
		ret = len;
	}
	pthread_mutex_unlock(&logmutex);
	return ret;
}

int rak_log_output_s(char**outaddr, rak_log_t plog, log_level_t level, 
                        const char *filename, const char* funcname, const int lineno, 
                        const char *fmt,...)
{
	
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = rak_log_output_vs(outaddr, plog, level, filename, funcname, lineno, fmt, ap);
	va_end(ap);
	return ret;
}

void rak_log_output_strfree(char**outaddr)
{
	if(outaddr && *outaddr)
		free(*outaddr),*outaddr=NULL;
}


int rak_log_output_v(rak_log_t plog, log_level_t level, 
						const char * filename, const char* funcname, const int lineno, 
						const char *fmt, va_list va_l)
{
	int ret = 0;
	pthread_mutex_lock(&logmutex);
	log_internal_t *log = (log_internal_t*)plog;
	if(log->log_output_v){
		ret = log->log_output_v(plog, level, filename, funcname, lineno, fmt, va_l);
		pthread_mutex_unlock(&logmutex);
		return ret;
	}
	if(level <= log->level){
		int len = rak_log_print_prefix(log, level, filename, funcname, lineno);
		ret = len + vprintf(fmt, va_l);
	}
	pthread_mutex_unlock(&logmutex);
	return ret;
}



int rak_log_output(rak_log_t plog, log_level_t level, 
						const char * filename, const char* funcname, const int lineno, 
						const char *fmt,...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = rak_log_output_v(plog, level, filename, funcname, lineno, fmt, ap);
	va_end(ap);
	return ret;
}





/* *************
 * Auth: yanchonggaodian
 * Date: 20170630 
 * Company: RAK
 * *************/

#ifndef __RAKLOG_H__
#define __RAKLOG_H__
#include <stdarg.h>

#ifdef __cplusplus  
extern "C"  {
#endif 
typedef void *rak_log_t;    
typedef enum{//// if initialed log level is initedlevel, then log don't output msg when thecurrentloglevel is bigger than initedlevel.
	RAK_LOG_STATUS_ERR = -1,// only for the case: when type of return is log_level_t, return it on failed.
	RAK_LOG_OFF,            // only for the case: when initial rak_log_t object, close all log info.
	RAK_LOG_FATAL,          // Fatal error log level
	RAK_LOG_ERROR,          // General error log level
	RAK_LOG_WARN,           // Warning log level
	RAK_LOG_INFO,           // important info log level
	RAK_LOG_DEBUG,          // debug info log level
	RAK_LOG_TRACE,          // Trace debug info log level
	RAK_LOG_FINE,           // Fine debug info log level
}log_level_t;    

/* *************************************************************************************************************
 * plog:  the value of rak_log_t object, returned by rak_log_init. 
 * level: thecurrentloglevel 
 * ************************************************************************************************************/
#define LOG_P(plog, level,...) rak_log_output((rak_log_t)(long)(plog), (log_level_t)((long)(level)), NULL, __func__, __LINE__, (const char*)(long)__VA_ARGS__)



/* *************************************************************************************************************
 * pdest: @char**;          *pdest will be chg and realloc memory, output string will be stored into *pdest
 * plog:  @rak_log_t;           the value returned by rak_log_init.
 * level: @log_level_t;     level>=RAK_LOG_FATAL && level<=RAK_LOG_FINE.
 * *************************************************************************************************************/
#define LOG_S(pdest, plog, level,...) rak_log_output_s((char**)(long)(pdest), ((rak_log_t)(long)(plog)), (log_level_t)(long)(level), __FILE__, __func__, __LINE__, __VA_ARGS__)


#ifdef __cplusplus  
#define LOG(FIRST,SECOND,...) LOG_INLINE(__FILE__, __func__, __LINE__, FIRST, SECOND, __VA_ARGS__)
#else
//some low version compriler maybe not support
#define LOG(FIRST,SECOND,...)      _Generic((SECOND), rak_log_t:LOG_S(FIRST,SECOND,__VA_ARGS__,NULL), int:LOG_P(FIRST,SECOND,__VA_ARGS__), default:LOG_S(FIRST,SECOND,__VA_ARGS__,NULL))
#endif

/* **************************************************************************************************************
 * the type for declare a function pointer for usr custom log function.
 * in the function instance, usr can get usr's custom data pointer by rak_log_get_privdata(plog);
 * in any case, usr can set usr's custom data by rak_log_set_privdata(plog, (void *) data);
 * outaddr: @char**;          *outaddr will be chg and realloc memory, output string will be stored into *outaddr
 * plog:    @rak_log_t;           the value returned by rak_log_init.
 * level:   @log_level_t;     level>=RAK_LOG_FATAL && level<=RAK_LOG_FINE.
 * filename:@const char*;     the name of file.
 * funcname:@const char*;     the name of the function.
 * lineno:  @const int;       the line number of the log in file.
 * fmt:     @const char*;     like vprintf function in c standard library.
 * va_l:    @va_list;         like vprintf function in c standard library.
 * *************************************************************************************************************/
typedef int (*log_outputvs_callback_t)(char**outaddr, rak_log_t plog, log_level_t level, 
                        const char *filename, const char* funcname, const int lineno, 
                        const char *fmt,  va_list va_l);


/* *************************************************************************************************************
 * the type for declare a function pointer for usr custom log function.
 * in the function instance, usr can get usr's custom data pointer by rak_log_get_privdata(plog);
 * in any case, usr can set usr's custom data by rak_log_set_privdata(plog, (void *) data);
 * plog:    @rak_log_t;           the value returned by rak_log_init.
 * level:   @log_level_t;     level>=RAK_LOG_FATAL && level<=RAK_LOG_FINE.
 * filename:@const char*;     the name of file.
 * funcname:@const char*;     the name of the function.
 * lineno:  @const int;       the line number of the log in file.
 * fmt:     @const char*;     like vprintf function in c standard library.
 * va_l:    @va_list;         like vprintf function in c standard library.
 * *************************************************************************************************************/
typedef int (*log_outputv_callback_t)(rak_log_t, log_level_t level, 
                        const char *filename, const char* funcname, const int lineno, 
                        const char *fmt, va_list va_l);

/* *******************************************************************************************************************************************
 * initial log resource. include: module_name, log_level, and custom function log_outputv_callback and log_outputvs_callback(the two func can 
 * be ingnored).
 * module:                @const char*;             module name, can be NULL if not output module name.
 * level:                 @log_level_t;             level>=RAK_LOG_OFF && level<=LOG_ALL.
 * log_outputv_callback:  @log_outputv_callback_t;  custom function for rak_log_output, should be NULL, if usr NOT instance log_outputv_callback_t.
 * log_outputvs_callback: @log_outputvs_callback_t; custom function for rak_log_output, should be NULL, if usr NOT instance log_outputvs_callback_t.
 * ******************************************************************************************************************************************/
 rak_log_t rak_log_init(const char *module, log_level_t level, int houroffset, log_outputv_callback_t log_outputv_callback, log_outputvs_callback_t log_outputvs_callback);

/* after rak_log_init, you must call it for destroy the log hnd, when you are sure that do not use the log again.*/
rak_log_t rak_log_destroy(rak_log_t log);


/* after LOG_S or rak_log_output_s, you must call it for free memory, when you are sure that do not use the it again.*/
void rak_log_output_strfree(char**outaddr);

/* for change log level */
int rak_log_set_level(rak_log_t log, log_level_t level);
/* for change log module name */
int rak_log_set_module(rak_log_t log, const char* module);
/* for set log_outputv_callback_t type custom function*/
int rak_log_set_log_outputv_func(rak_log_t log, log_outputv_callback_t func);
/* for set log_outputvs_callback_t type custom function*/
int rak_log_set_log_outputvs_func(rak_log_t log, log_outputvs_callback_t func);
/* for set custom data for custom function */
int rak_log_set_privdata(rak_log_t log, void * data);


/* for get log level */
log_level_t rak_log_get_level(rak_log_t log);
/* for get log module name */
const char * rak_log_get_module(rak_log_t log);
/* for get log log_outputv_callback_t type custom function pointer. */
log_outputv_callback_t rak_log_get_log_outputv_func(rak_log_t log);
/* for get log log_outputvs_callback_t type custom function pointer. */
log_outputvs_callback_t rak_log_get_log_outputvs_func(rak_log_t log);
/* for get custom data for custom function */
void * rak_log_get_privdata(rak_log_t log);



/* *******************************************************************************************************************************************
 * the function will output log msg into *outaddr if the level less than or equal to the level set by rak_log_init or rak_log_set_level.
 * outaddr: @char**;          *outaddr will be chg and realloc memory, log msg will be stored into *outaddr,
 *                            NULL may result in Segmentation fault expect for instance log_outputvs_callback_t function.
 * plog:    @rak_log_t;           the value returned by rak_log_init.
 * level:   @log_level_t;     level>=RAK_LOG_FATAL && level<=RAK_LOG_FINE.
 * filename:@const char*;     the name of file.
 * funcname:@const char*;     the name of the function.
 * lineno:  @const int;       the line number of the log in file.
 * fmt:     @const char*;     like vprintf function in c standard library.
 * va_l:    @va_list;         like vprintf function in c standard library.
 * ******************************************************************************************************************************************/
int rak_log_output_s(char**outaddr, rak_log_t plog, log_level_t level,
                        const char *filename, const char* funcname, const int lineno,
                        const char *fmt,...);



/* *****************************************************************************************************************************************
 * the function will output log msg into stdout if the level less than or equal to the level set by rak_log_init or rak_log_set_level.
 * plog:    @rak_log_t;           the value returned by rak_log_init.
 * level:   @log_level_t;     level>=RAK_LOG_FATAL && level<=RAK_LOG_FINE.
 * filename:@const char*;     the name of file.
 * funcname:@const char*;     the name of the function.
 * lineno:  @const int;       the line number of the log in file.
 * fmt:     @const char*;     like vprintf function in c standard library.
 * va_l:    @va_list;         like vprintf function in c standard library.
 * ****************************************************************************************************************************************/
int rak_log_output(rak_log_t plog, log_level_t level, 
						const char * filename, const char* funcname, const int lineno, 
						const char *fmt,...);

#ifdef __cplusplus  
}
#endif 

#ifdef __cplusplus  
extern "C" {
int rak_log_output_vs(char**outaddr, rak_log_t plog, log_level_t level,
                         const char * filename, const char* funcname, const int lineno,
                         const char *fmt, va_list va_l);


int rak_log_output_v(rak_log_t plog, log_level_t level,
                        const char * filename, const char* funcname, const int lineno,
                        const char *fmt, va_list va_l);
}

inline int LOG_INLINE(const char * filename, const char* funcname, const int lineno,
		char**outaddr, rak_log_t plog, log_level_t level,
		const char *fmt, ...){
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = rak_log_output_vs(outaddr, plog, level, filename, funcname, lineno, fmt, ap);
	va_end(ap);
	return ret;
}
inline int LOG_INLINE(const char * filename, const char* funcname, const int lineno,
		rak_log_t plog, log_level_t level,
		const char *fmt,...){
	int ret;
	va_list ap;
	va_start(ap, fmt);
	
	ret = rak_log_output_v(plog, level, filename, funcname, lineno, fmt, ap);
	va_end(ap);
	return ret;
}
#endif

#endif

//debug.h
//by sevencheng
#ifndef __AUDIO_DEBUG_H__
#define __AUDIO_DEBUG_H__

#define AUDIO_DEBUG
#ifdef AUDIO_DEBUG
#define AUDIO_DBG(...) 	({fprintf(stderr, " AUDIO_DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__);})
#define AUDIO_WARNING(...)	({ fprintf(stderr, " AUDIO_WARNING(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); })
#else
#define AUDIO_DBG(...)
#define AUDIO_WARNING(...)
#endif

//#define AUDIO_INFO_DBG
#ifdef AUDIO_INFO_DBG
#define AUDIO_INFO(...)	({ fprintf(stderr, " AUDIO_INFO(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); })
#else
#define AUDIO_INFO(...)
#endif

//#define AUDIO_TRACE_DBG
#ifdef AUDIO_TRACE_DBG
#define AUDIO_TRACE(...)	({ fprintf(stderr, " AUDIO_TRACE(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); })
#else
#define AUDIO_TRACE(...)
#endif

#if 1
	#define AUDIO_ERROR(...)		({ fprintf(stderr, " AUDIO_ERROR(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); })
#else
	#define AUDIO_ERROR(...)		{;}
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
#undef bool
#undef false
#undef true
#define bool	uint8_t
#define false	0
#define true	(!false)

#endif	//__AUDIO_DEBUG_H__

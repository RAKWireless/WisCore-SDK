#ifndef __RKLOG_H__
#define __RKLOG_H__

#define RK_ERROR(...)		{ fprintf(stderr, " ERROR(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); }

#endif

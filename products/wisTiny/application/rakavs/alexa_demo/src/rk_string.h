//
/*  
 * rk_string.h 2016/08/20 , rak University , China 
 * 	Author: Sevencheng
*/  
#ifndef __RK_STRING_H___
#define __RK_STRING_H___

int RK_Strstart(const char *str, const char *pfx, const char **ptr);
int RK_Isspace(int c);
int RK_ReadSliceLine(char *s, char *buf, int max_len);

#endif//__RK_STRING_H___
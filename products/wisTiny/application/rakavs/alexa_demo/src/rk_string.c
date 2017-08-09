//
/*  
 * rk_string.c , 2016/08/20 , rak University , China 
 * 	Author: Sevencheng
 */  
#include <stdio.h>  
#include <stdlib.h>  
#include <stdint.h>
#include <string.h>

/**
 * Return non-zero if pfx is a prefix of str. If it is, *ptr is set to
 * the address of the first character in str after the prefix.
 *
 * @param str input string
 * @param pfx prefix to test
 * @param ptr updated if the prefix is matched inside str
 * @return non-zero if the prefix matches, zero otherwise
 */
int RK_Strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}
/**
 * Locale-independent conversion of ASCII isspace.
 */
int RK_Isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
           c == '\v';
}
struct _io_buf{
	char *IOBuf;
	int lsize;
};
static int RK_GetLine(char *s, struct _io_buf *pIO, int max_len)
{
	int cnt = 0;
	char c;
	do{
		c = *s;
		if(c && cnt < max_len)
			pIO->IOBuf[cnt++] = c;
		s++;
	}while(c != '\n' && c != '\r' && *s);
	pIO->lsize = cnt;
	s++;
	if (c == '\r' && *s == '\n')
        cnt++;

	pIO->IOBuf[pIO->lsize] = 0;

	return cnt;
}

int RK_ReadSliceLine(char *s, char *buf, int max_len)
{
	struct _io_buf sIoBuf;
	int len;
	
	sIoBuf.IOBuf = buf;
	
	len = RK_GetLine(s, &sIoBuf, max_len);
	if(len > 0 && RK_Isspace(sIoBuf.IOBuf[sIoBuf.lsize - 1])){
		sIoBuf.IOBuf[sIoBuf.lsize - 1] = '\0';
	}
	
	return len;
}



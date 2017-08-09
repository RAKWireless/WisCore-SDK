/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/
#ifndef __UTILS__
#define __UTILS__
#define ABS(a) (((a) < 0) ? -(a) : (a))
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#define LENGTH_OF(x) (sizeof(x)/sizeof(x[0]))

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

/* BOOLEAN MACRO */
#if defined(FALSE)	&&	FALSE != 0
	#error FALSE is not defined as 0.
#else
	#undef FALSE
#endif

#if defined(TRUE)	&&	TRUE != 1
	#error TRUE is not defined as 1.
#else
	#undef TRUE
#endif

/* Type definition */
typedef enum{
	FALSE		=0,
	TRUE		=1,
} BOOLEAN;
/*__BOOLEAN MACRO */

#define RK_NULL     0L
#define RK_SUCCESS  0
#define RK_FAILURE  (-1)

#endif

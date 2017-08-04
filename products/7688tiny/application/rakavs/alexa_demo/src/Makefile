.SUFFIXES : .x .o .c .s

CUR_PATH = $(shell pwd)
CROSS_TOOLCHAIN ?= /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2
UNAME ?= mt7628_op

ifeq ($(UNAME), Linux)
#INC += -I../include
LIB += -L/usr/lib/x86_64-linux-gnu -L/usr/lib
CC = gcc -O2 $(INC) #$(LIB)#-static
STRIP ?= strip
endif

ifeq ($(UNAME), mt7628_op)
LIB += -L$(CROSS_TOOLCHAIN)/$(TARGET_DIR)/usr/lib -L../lib
INC += -I$(CROSS_TOOLCHAIN)/$(TOOL_DIR)/usr/include -I../include -I../../common/include
CC = mipsel-openwrt-linux-gcc -O2 -D_LARGEFILE64_SOURCE=1 $(INC)
LDFLAGS=$(LIB)
STRIP=mipsel-openwrt-linux-strip
endif

TARGET = alexa_run_demo
SRCS := plugin_alexa.c \
		audio_player.c \
		audio_capture.c \
		audio_queue.c \
		alexa_common.c \
		alexa_alert.c \
		alexa_media.c \
		../../common/src/RKIoctrlApi.c \
		../../common/src/RKGpioApi.c \
		led.c \
		rk_string.c
		
#getcodechallenge.c 
EXT_LIBS= -lpthread -lcrypto -lssl -lcurl -lcJSON -lrkaudio -lavio -lnmedia -lavs -lzl380tw -lfdk-aac

all: 
	$(CC) $(LDFLAGS) $(EXT_LIBS) -o $(TARGET) $(SRCS)
#	$(STRIP) $(TARGET) 


clean:
	@rm -f $(TARGET) *.o -v  
	

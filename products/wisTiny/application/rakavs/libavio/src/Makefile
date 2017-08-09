.SUFFIXES : .x .o .c .s

CUR_PATH = $(shell pwd)
CROSS_TOOLCHAIN ?= /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2
UNAME=mt7628_op


ifeq ($(UNAME), mt7628_op)
LIB_LFLAGS += -L$(CROSS_TOOLCHAIN)/$(TARGET_DIR)/lib 
LIB_LFLAGS += -L$(CROSS_TOOLCHAIN)/$(TOOL_DIR)/lib 
LIB_LFLAGS += -L./ -L../lib

INC_CFLAGS += -I$(CROSS_TOOLCHAIN)/$(TARGET_DIR)/usr/include 
INC_CFLAGS += -I$(CROSS_TOOLCHAIN)/$(TOOL_DIR)/include 
INC_CFLAGS += -I../include

CC := mipsel-openwrt-linux-gcc  -Wall -O2 -g 

#-O2 -D_LARGEFILE64_SOURCE=1 $(INC) -I../include
STRIP := mipsel-openwrt-linux-strip
endif


SRCS := audio_ao.c audio_queue.c dec_mp3.c player_io.c 
EXT_LIBS= -lasound -lm -lao -lmad
INCS += -I../include

all: libavio.so

libavio.so: player_io.o audio_queue.o dec_mp3.o dec_raw.o audio_ao.o
	$(CC) $^ $(LIB_LFLAGS) $(EXT_LIBS) $(INC_CFLAGS) -shared -fPIC -o $@

audio_queue.o: audio_queue.c
	$(CC) $(INCS) -fPIC -shared -c -g $^
audio_ao.o: audio_ao.c
	$(CC) $(INCS) -fPIC -shared -c -g $^
dec_mp3.o: dec_mp3.c
	$(CC) $(INCS) -fPIC -shared -c -g $^
dec_raw.o: dec_raw.c
	$(CC) $(INCS) -fPIC -shared -c -g $^
player_io.o: player_io.c
	$(CC) $(INCS) -fPIC -shared -c -g $^
test: av_test

av_test: av_test.c libavio.so
	$(CC) $^ $(LIB_LFLAGS) $(EXT_LIBS) $(INC_CFLAGS) -static  -o $@
	
clean:
	@rm -f *.o -v
distclean:
	@rm -f *.o *.so -v
	

install: libavio.so
	@cp libavio.so ../../alexa_demo/lib -v

installtest: av_test
	@cp av_test ~/nfsroot/alexa_demo/ -v

copy: libavio.so
	@cp libavio.so ~/nfsroot/alexa_demo/ -v

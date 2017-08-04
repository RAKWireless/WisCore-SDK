#!/bin/sh

#set -e
# set cross_toolchain environment variable
export PATH=$PATH:/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin
# set STAGING_DIR path
export STAGING_DIR=/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/

export TARGET_DIR=target-mipsel_24kec+dsp_uClibc-0.9.33.2/
export TOOL_DIR=toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2
echo "clean dir"
make clean
echo "make ..."
make CROSS_TOOLCHAIN=/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2
#CC=mipsel-openwrt-linux-gcc 

echo "###########################################"
echo "generate library <libavio.so> "
echo "copys libavio.so ../../alexa_demo/lib/ "
echo "copys avcodec.h ../../alexa_demo/include"
echo "copys player_io.h ../../alexa_demo/include"
echo "###########################################"
cp libavio.so ../../alexa_demo/lib/
cp avcodec.h  ../../alexa_demo/include 
cp player_io.h ../../alexa_demo/include


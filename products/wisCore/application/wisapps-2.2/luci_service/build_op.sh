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
echo "generate executable file  <luci_service> "
echo "###########################################"



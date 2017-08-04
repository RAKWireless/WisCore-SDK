
set(CMAKE_C_COMPILER /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc)
set(CMAKE_CXX_COMPILER /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-g++)


set(CMAKE_FIND_ROOT_PATH /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2
)


set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#set install path prefix

if(("${CMAKE_STAGING_PREFIX}" STREQUAL ""))
	set(STAGING_DIR "/usr/local")
else()
	set(STAGING_DIR "${CMAKE_STAGING_PREFIX}")
endif()

set(CMAKE_FIND_NO_INSTALL_PREFIX TRUE)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/makefile "export STAGING_DIR=${STAGING_DIR}\ninclude Makefile\nuninstall:\n\txargs rm -rfv < install_manifest.txt")

list(INSERT  CMAKE_PREFIX_PATH 0 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2
					/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr
					/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/local
					/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2
					/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/usr
					/opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/usr/local
)
					
include_directories(BEFORE SYSTEM 
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/include
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/include
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/local/include
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/include
			 /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/usr/include
)





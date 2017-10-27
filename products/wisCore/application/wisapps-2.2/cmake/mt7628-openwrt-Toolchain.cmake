
if( DEFINED ENV{STAGING_DIR} AND DEFINED ENV{TOOLCHAIN_DIR})
	set(STAGING_DIR $ENV{STAGING_DIR})
	set(TOOLCHAIN_DIR $ENV{TOOLCHAIN_DIR})
else()
	set(STAGING_DIR /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/target-mipsel_24kec+dsp_uClibc-0.9.33.2)
	set(TOOLCHAIN_DIR /opt/mipsel-openwrt-linux-4.8.3/mipsel-4.8.3.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2)
endif()

if( DEFINED ENV{TARGET_CC} AND DEFINED ENV{TARGET_CXX})
	set(CMAKE_C_COMPILER $ENV{TARGET_CC})
	set(CMAKE_CXX_COMPILER $ENV{TARGET_CXX})
else()
	set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/mipsel-openwrt-linux-gcc)
	set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/mipsel-openwrt-linux-g++)
endif()

if(DEFINED ENV{TARGET_AR})
	set(CMAKE_AR $ENV{TARGET_AR})
else()
	set(CMAKE_AR ${TOOLCHAIN_DIR}/bin/mipsel-openwrt-linux-ar)
endif()

if(DEFINED ENV{TARGET_RANLIB})
	set(CMAKE_RANLIB $ENV{TARGET_RANLIB})
else()
	set(CMAKE_RANLIB ${TOOLCHAIN_DIR}/bin/mipsel-openwrt-linux-ranlib)
endif()

#mark_as_advanced(CMAKE_STRIP)

if(DEFINED ENV{TARGET_STRIP})
	set(CMAKE_STRIP $ENV{TARGET_STRIP})
else()
	set(CMAKE_STRIP ${TOOLCHAIN_DIR}/bin/mipsel-openwrt-linux-strip)
endif()

if(DEFINED ENV{TARGET_NM})
	set(CMAKE_NM $ENV{TARGET_NM})
else()
	set(CMAKE_NM ${TOOLCHAIN_DIR}/bin/mipsel-openwrt-linux-nm)
endif()





#message("${STAGING_DIR}\n${TOOLCHAIN_DIR}")

set(CMAKE_FIND_ROOT_PATH ${STAGING_DIR}  ${TOOLCHAIN_DIR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
#message("PATH=${PATH}")
#set install path prefix

#if(("${CMAKE_STAGING_PREFIX}" STREQUAL ""))
#	set(STAGING_DIR "/usr/local")
#else()
#	set(STAGING_DIR "${CMAKE_STAGING_PREFIX}")
#endif()

set(CMAKE_FIND_NO_INSTALL_PREFIX TRUE)
#file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/makefile "export STAGING_DIR=${STAGING_DIR}\ninclude Makefile\nuninstall:\n\txargs rm -rfv < install_manifest.txt")

list(INSERT  CMAKE_PREFIX_PATH 0 ${STAGING_DIR}   ${STAGING_DIR}/usr   ${STAGING_DIR}/usr/local
				 ${TOOLCHAIN_DIR} ${TOOLCHAIN_DIR}/usr ${TOOLCHAIN_DIR}/usr/local
)
					
include_directories(BEFORE SYSTEM ${STAGING_DIR}/include   ${STAGING_DIR}/usr/include   ${STAGING_DIR}/usr/local/include
							   ${TOOLCHAIN_DIR}/usr/include ${TOOLCHAIN_DIR}/usr/local/include
)





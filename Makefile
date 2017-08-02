#By Seven created

export ROOTDIR=$(PWD)
include config.mk

export OPWRT_WORKSPACE=$(OPENWRT_PATH)/$(OPENWRT_VERSION)-$(OPENWRT_VENDOR)-$(OPENWRT_VENDOR_VERSION)

KERNEL_BIN=openwrt-ramips-mt7628-mt7628-squashfs-sysupgrade.bin

.PHONY: compile clean install uninstall
all: alexa_bin compile

compile:
	@echo "Building openwrt..."
	$(MAKE) -C $(OPWRT_WORKSPACE) -j1 V=s
	cp $(OPWRT_WORKSPACE)/bin/ramips/$(KERNEL_BIN) out/target/bin
	@echo "Building openwrt finished."
	@echo "kernel image into out/target/bin"

alexa_bin:
	@if [ ! -e alexa_bin_dir ] ; then \
		mkdir alexa_bin_dir ; \
	elif [ -d alexa_bin_dir ] ; then \
		rm alexa_bin_dir/* -rf ; \
	else \
		rm -rf alexa_bin_dir ; \
	fi 
	cd alexa_bin_dir && cmake ${PRODUCT_PATH}/wisAvs/application/rakavs -DCMAKE_TOOLCHAIN_FILE=cmake/mt7628-openwrt-Toolchain.cmake && make ;\
	cp -av bin/*   ${OPWRT_WORKSPACE}/package/utils/wisapps/src; \
	cp -av lib/*   ${OPWRT_WORKSPACE}/package/utils/wisapps/src/lib; \
	cd ../ && rm alexa_bin_dir -rf 
	cp -av ${PRODUCT_PATH}/wisAvs/application/rakavs/alexa_demo/lib/* ${OPWRT_WORKSPACE}/package/utils/wisapps/src/lib/;


clean:

	rm $(OPENWRT_PATH)/15.05-rak-rc2 -rf
	
install:



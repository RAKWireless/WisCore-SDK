#By Seven 2017-0306
#make files.
SUBDIRS=target package

export OPWRT_WORKSPACE=$(OPENWRT_PATH)/$(OPENWRT_VERSION)-$(OPENWRT_VENDOR)-$(OPENWRT_VENDOR_VERSION)

.PHONY: compile clean install uninstall

compile:
	cp -rf opwrt-wisavs-topv002-1-20171016.config $(OPWRT_WORKSPACE)/.config
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		(echo $(PWD)); \
		(cd $$subdir && make || exit "$$?"); \
	done;
	mkdir -p $(OPWRT_WORKSPACE)/dl
	cd application; tar zcf wisapps-2.2.tar.gz wisapps-2.2; mv wisapps-2.2.tar.gz -v $(OPWRT_WORKSPACE)/dl

clean:

install:

uninstall:


#By Seven created

export ROOTDIR=$(PWD)
include config.mk

export OPWRT_WORKSPACE=$(OPENWRT_PATH)/$(OPENWRT_VERSION)-$(OPENWRT_VENDOR)-$(OPENWRT_VENDOR_VERSION)
export HOST_PATH=${PATH}
KERNEL_BIN=openwrt-ramips-mt7628-mt7628-squashfs-sysupgrade.bin
ifeq ($(BUILD_TARGET), 7688tiny)
KERNEL_BIN=openwrt-ramips-mt7688-LinkIt7688-squashfs-sysupgrade.bin
else ifeq ($(BUILD_TARGET), RAK831)
KERNEL_BIN=openwrt-ramips-mt7688-LinkIt7688-squashfs-sysupgrade.bin
endif

.PHONY: compile clean install uninstall

compile:
	@echo "Building openwrt..."
	$(MAKE) -C $(OPWRT_WORKSPACE) -j1 V=s
	cp $(OPWRT_WORKSPACE)/bin/ramips/$(KERNEL_BIN) out/target/bin
	@echo "Building openwrt finished."
	@echo "kernel image into out/target/bin"

clean:
	
install:



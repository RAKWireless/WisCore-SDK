#seven
include $(TOPDIR)/rules.mk
#include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:= wistools
PKG_RELEASE:=1
PKG_VERSION:=1.0
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk
#PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)


define Package/wistools
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:= RAK WisCore tools
	DEPENDS:=+kmod-iio-core +libiio +libao +alsa-lib +libcurl +libuci
endef

define Package/wistools/description
	hive apps building test
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/src
	$(CP) ./src/* $(PKG_BUILD_DIR)/src/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/src \
	$(TARGET_CONFIGURE_OPTS) CFLAGS="$(TARGET_CFLAGS)"
endef

	#$(TARGET_CONFIGURE_OPTS) CFLAGS="$(TARGET_CFLAGS) -I$(LINUX_DIR)/include"
define Package/wistools/install
	$(INSTALL_DIR) $(1)/usr
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DIR) $(1)/www
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc/wiskey

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/msload_go.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/16kk.raw $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/wifistatus.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/recoverboard.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/factory_test.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/unabto_tunnel $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/product_test $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/mtdtool $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/tw_zl380_tool $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/twmcsmupgrade $(1)/usr/bin
	
	$(INSTALL_DIR) $(1)/lib/modules/3.18.45
	$(CP) $(PKG_BUILD_DIR)/src/*.ko $(1)/lib/modules/3.18.45


	$(CP) $(PKG_BUILD_DIR)/src/version $(1)/etc/wiskey
	$(CP) $(PKG_BUILD_DIR)/src/mcsmfirmware.tar.gz $(1)/etc/wiskey
	$(CP) $(PKG_BUILD_DIR)/src/htdocs $(1)/www

endef
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/16k.pcm $(1)/usr/bin
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/48k.pcm $(1)/usr/bin
	#$(CP) $(PKG_BUILD_DIR)/src/wisalexainfo $(1)/etc
	
$(eval $(call BuildPackage,wistools))


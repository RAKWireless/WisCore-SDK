#
# Copyright (C) 2006-2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

# Here the wisapps-2.2.tar.gz from
# WisCore-SDK/products/wisCore/application/wisapps-2.2

PKG_NAME:=wisapps
PKG_VERSION:=2.2
PKG_RELEASE:=2

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz


include $(INCLUDE_DIR)/package.mk

define Package/wisapps
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=RAK WisApps (alexa libavio luci_service gpio_ctrl tunnel)
  DEPENDS:=libcurl libpthread libcrypto libssl librt libubox libao libasound libuci libz libm
endef

#define Build/Prepare
#       URL:=http://rakwireless.com/wisapps/download
#	mkdir -p $(PKG_BUILD_DIR)
#	tar zxf  
#endef

define Build/Compile
	cd ${PKG_BUILD_DIR} && export PATH=${HOST_PATH} && cmake ${PKG_BUILD_DIR} -DCMAKE_TOOLCHAIN_FILE=cmake/mt7628-openwrt-Toolchain.cmake -DCMAKE_INSTALL_PREFIX=$(PKG_INSTALL_DIR)/usr/wisapps && make 
endef

define Package/wisapps/install

	$(INSTALL_DIR) $(1)/usr/{s,}bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc

	make install -C ${PKG_BUILD_DIR}

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/wisapps/bin/alexa_run_demo $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/wisapps/bin/tunnel         $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/wisapps/bin/luci_service   $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/wisapps/bin/gpio_ctrl       $(1)/usr/bin/

	$(CP)          $(PKG_INSTALL_DIR)/usr/wisapps/lib/lib*.so*       $(1)/usr/lib/
	$(CP)	       $(PKG_INSTALL_DIR)/usr/wisapps/lib/sound		 $(1)/usr/lib/
	$(CP)	       $(PKG_INSTALL_DIR)/usr/wisapps/etc/ssl  	 $(1)/etc/

endef



$(eval $(call BuildPackage,wisapps))

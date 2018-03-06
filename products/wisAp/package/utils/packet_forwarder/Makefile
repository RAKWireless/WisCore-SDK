#
# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=packet_forwarder
PKG_VERSION:=4.0.1
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/Lora-net/packet_forwarder.git
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=d0226eae6e7b6bbaec6117d0d2372bf17819c438
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz

PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/packet_forwarder/Default
  TITLE:=Semtech packet_forwarder package
endef

define Package/packet_forwarder
$(call Package/packet_forwarder/Default)
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:= +PACKAGE_lora-gateway +librt +libpthread
endef

define Package/packet_forwarder/description
  lora-gateway is a library to build a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

TARGET_CFLAGS += $(FPIC) -O2 -Wall -Wextra -std=c99 -Iinc -I. -lm

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/packet_forwarder
endef

define Package/packet_forwarder/install
	$(INSTALL_DIR) $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lora_pkt_fwd/*.json $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lora_pkt_fwd/lora_pkt_fwd $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lora_pkt_fwd/update_gwid.sh $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_ack/util_ack $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_tx_test/util_tx_test $(1)/usr/bin/packet_forwarder
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_sink/util_sink $(1)/usr/bin/packet_forwarder
endef

$(eval $(call BuildPackage,packet_forwarder))

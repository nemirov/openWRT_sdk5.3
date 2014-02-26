#
# Top level makefile for simple application
#

include $(TOPDIR)/rules.mk

PKG_NAME:=sdk
PKG_VERSION:=1.1b
PKG_RELEASE:=7

include $(INCLUDE_DIR)/package.mk

define Package/sdk
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=SDK -- manage system sdk_5.3 TRIADA
  DEPENDS:=+libpthread
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/Configure

endef



TARGET_CFLAGS += $(FPIC)


define Package/sdk/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk $(1)/bin/

endef

$(eval $(call BuildPackage,sdk))

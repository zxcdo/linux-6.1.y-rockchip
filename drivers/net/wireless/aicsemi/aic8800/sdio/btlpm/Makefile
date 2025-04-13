ifeq ($(CONFIG_AIC8800_SDIO_BTLPM), y)
  CONFIG_AIC8800_BTLPM_SUPPORT = m
endif

MODULE_NAME=aic8800_sdio_bt
obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) = $(MODULE_NAME).o
$(MODULE_NAME)-y += aic8800_btlpm.o

ccflags-y += -I$(srctree)/$(src)/../bsp

CONFIG_SUPPORT_LPM ?= n
CONFIG_AUTO_PM ?= n

aic8800_btlpm-y := \
	aic_bluetooth_main.o \
	rfkill.o \

aic8800_btlpm-$(CONFIG_SUPPORT_LPM) += lpm.o

ccflags-y += -DAIC_TRACE_INCLUDE_PATH=$(src)

ccflags-$(CONFIG_AUTO_PM) += -DCONFIG_AUTO_PM

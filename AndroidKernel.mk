#Android makefile to build kernel as a part of Android Build

ifeq ($(TARGET_PREBUILT_KERNEL),)

#KERNEL_OUT := $(shell pwd)/kernel
KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_INT_KERNEL)
endif
PRIVATE_TOPDIR=$(shell pwd)
#CROSS_COMPILE:=CROSS_COMPILE=$(PRIVATE_TOPDIR)/$(TARGET_TOOLS_PREFIX)
CROSS_COMPILE:=CROSS_COMPILE=arm-none-linux-gnueabi-

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) $(KERNEL_DEFCONFIG)
#	$(MAKE) -C kernel ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) $(KERNEL_DEFCONFIG)

$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy > $(KERNEL_OUT)/piggy

$(TARGET_OUT)/lib/modules:
	mkdir -p $(TARGET_OUT)/lib/modules

$(TARGET_PREBUILT_INT_KERNEL): $(KERNEL_OUT) $(KERNEL_CONFIG) $(TARGET_OUT)/lib/modules
	echo TARGET_PREBUILT_INT_KERNEL=$(TARGET_PREBUILT_INT_KERNEL)
	echo KERNEL_OUT=$(KERNEL_OUT)
	echo KERNEL_CONFIG=$(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) 
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) modules 
#	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) uImage modules 
#	$(MAKE) -C kernel ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) 
#	$(MAKE) -C kernel ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) uImage modules 
#	cp -uv `find $(KERNEL_OUT)/drivers/staging/ti-st/ -name *.ko` $(TARGET_OUT)/lib/modules
	cp $(KERNEL_OUT)/arch/arm/boot/zImage $(PRODUCT_OUT)

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) tags
#	$(MAKE) -C kernel ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE)-C kernel O=../$(KERNEL_OUT) ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) menuconfig
#	     $(MAKE)-C kernel ARCH=arm $(CROSS_COMPILE) FIH_PROJ=$(TARGET_DEVICE) menuconfig
	cp $(KERNEL_OUT)/.config kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

endif

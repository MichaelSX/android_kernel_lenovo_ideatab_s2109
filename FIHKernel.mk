#Android makefile to build kernel as a part of Android Build
TARGET_DEVICE=GOX
KERNEL_DEFCONFIG=android_4430gox_defconfig
$(info TARGET_DEVICE=$(TARGET_DEVICE))
$(info KERNEL_DEFCONFIG=$(KERNEL_DEFCONFIG))

FIH_MODEL_PCB=FIHV_GOX_B0
FIH_MODEL_NAME=$(TARGET_DEVICE)

PRODUCT_OUT := out/target/product/$(TARGET_DEVICE)
TARGET_OUT_INTERMEDIATES := $(PRODUCT_OUT)/obj
TARGET_OUT := $(PRODUCT_OUT)/system

kernel_out := $(PRODUCT_OUT)/kernel

include kernel/AndroidKernel.mk

.PHONY: kernelonly
kernelonly: $(kernel_out)

$(kernel_out): $(TARGET_PREBUILT_INT_KERNEL)
	cp $< $@

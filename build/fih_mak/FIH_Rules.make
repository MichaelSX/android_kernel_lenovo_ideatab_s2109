
#*****************************************************************************
# FIH BEGIN SECTION

ifeq ($(FIH_BUILD_FTM),YES)
FIH_CONFIG_BUILD_FTM = y
endif

# set the cust include file
ifeq ($(FIH_BOOTLOADER_SW_VER),)
    FIH_DF_BOOTLOADER_SW_VER = -DFIH_BOOTLOADER_SW_VER=\"GOX-0-0000\"
else
    FIH_DF_BOOTLOADER_SW_VER = -DFIH_BOOTLOADER_SW_VER=\"$(FIH_BOOTLOADER_SW_VER)\"
endif
ifneq ($(FIH_CONFIG_UBOOT_DEBUG_LEVEL),)
    FIH_DF_CONFIG_UBOOT_DEBUG_LEVEL = -DCONFIG_UBOOT_DEBUG_LEVEL=$(FIH_CONFIG_UBOOT_DEBUG_LEVEL)
endif
ifeq ($(FIH_MODEL_NAME),)
    FIH_MODEL_NAME = $(FIH_PROJ)
endif
ifeq ($(FIH_MODEL_PCB),)
    FIH_MODEL_PCB=FIHV_GOX_B0
endif
FIH_MODEL_NAME_LC := $(shell perl -e 'print lc("$(FIH_MODEL_NAME)")')
FIH_CUST_FILE	:= fih_cust_$(FIH_MODEL_NAME_LC).h
FIH_CUSTH	:= -DFIH_CUST_H=\"$(FIH_CUST_FILE)\"

# set the model and hw pcb version compiler option
FIH_DF_MODEL := -DFIH_MODEL=FIHV_$(FIH_MODEL_NAME)
FIH_DF_MODEL_PCB := -DFIH_MODEL_PCB_REV=$(FIH_MODEL_PCB)

# set the ftm compiler option
ifeq ($(FIH_CONFIG_BUILD_FTM),y)
FIH_DF_FTM = -DFIH_FEATURE_FTM
endif

FIH_CFLAGS := -I$(FIH_PROJECT_ROOT_DIR)/build/fih_include $(FIH_CUSTH) $(FIH_DF_MODEL) $(FIH_DF_MODEL_PCB) $(FIH_DF_FTM) $(FIH_DF_BOOTLOADER_SW_VER) $(FIH_DF_CONFIG_UBOOT_DEBUG_LEVEL)

DUMMY			:= $(shell if [ ! -e $(FIH_PROJECT_ROOT_DIR)/build/fih_mak/fih_$(FIH_MODEL_NAME_LC).make ];then touch $(FIH_PROJECT_ROOT_DIR)/build/fih_mak/fih_$(FIH_MODEL_NAME_LC).make;fi;)
include $(FIH_PROJECT_ROOT_DIR)/build/fih_mak/fih_$(FIH_MODEL_NAME_LC).make

# END SECTION

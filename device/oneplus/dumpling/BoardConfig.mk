#
# Copyright (C) 2026 The Evolution X Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/oneplus/dumpling

# Architecture (MSM8998 Kryo 280, 64-bit only)
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := kryo

TARGET_BOARD_PLATFORM := msm8998

# Bootloader / Assert
TARGET_BOOTLOADER_BOARD_NAME := msm8998
TARGET_OTA_ASSERT_DEVICE := dumpling,OnePlus5T,oneplus5t

# Prebuilt verified Mainline Linux 6.18.0-msm8998+ (#52)
TARGET_FORCE_PREBUILT_KERNEL := true
TARGET_PREBUILT_KERNEL := $(DEVICE_PATH)/prebuilt/kernel/Image.gz-dtb
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_OFFSET := 0x00008000
BOARD_RAMDISK_OFFSET := 0x01000000
BOARD_KERNEL_TAGS_OFFSET := 0x00000100
BOARD_KERNEL_PAGESIZE := 4096
BOARD_BOOT_HEADER_VERSION := 0
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_KERNEL_CMDLINE := console=tty0 androidboot.hardware=qcom swiotlb=2048 loop.max_part=7 firmware_class.path=/vendor/firmware androidboot.boot_devices=soc@0/1da4000.ufshc androidboot.selinux=permissive printk.devkmsg=on

# Fixed UFS partitions matching OnePlus 5T stock GPT
BOARD_USES_METADATA_PARTITION := true
BOARD_ROOT_EXTRA_SYMLINKS += /mnt/vendor/persist:/persist
BOARD_FLASH_BLOCK_SIZE := 262144 # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864
BOARD_CACHEIMAGE_PARTITION_SIZE := 268435456
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3221225472
BOARD_VENDORIMAGE_PARTITION_SIZE := 1073741824
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs

# Compressed read-only EROFS for system (including system_ext + product) and vendor
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_PRODUCT := system/product
TARGET_COPY_OUT_SYSTEM_EXT := system/system_ext

# Evolution X Full Recovery Image (Linux 6.18)
TARGET_NO_RECOVERY := false
BOARD_USES_FULL_RECOVERY_IMAGE := true
BOARD_HAS_NO_SELECT_BUTTON := true
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom

# Display / Graphics (Mainline DRM/KMS + Minigbm MSM + ANGLE/SwiftShader fallback)
TARGET_USES_HWC2 := true
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
TARGET_VULKAN_SUPPORT := true
$(call add_soong_config_namespace,minigbm)
$(call add_soong_config_var_value,minigbm,platform,msm)

# Lineage Lights auto-discovery (/sys/class/backlight/c994000.dsi.0)
$(call soong_config_set_bool,lineagelight,scan_for_backlight_devices,true)

# Wi-Fi (ath10k_snoc nl80211)
BOARD_WLAN_DEVICE := ath10k
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_HOSTAPD_DRIVER := NL80211
$(call add_soong_config_namespace,wpa_supplicant)
$(call add_soong_config_var_value,wpa_supplicant,platform_version,$(PLATFORM_VERSION))
$(call add_soong_config_var_value,wpa_supplicant,nl80211_driver,CONFIG_DRIVER_NL80211_QCA)

# Properties & SELinux
BOARD_VENDOR_SEPOLICY_DIRS += \
    $(DEVICE_PATH)/sepolicy/vendor \
    external/minigbm/cros_gralloc/sepolicy

BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true

# VINTF Device Manifest (assembled with vintf_fragments & APEX manifests)
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml

#
# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# 64-bit only Android 17 target
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit device configuration
$(call inherit-product, device/oneplus/dumpling/device.mk)

# GMS / Mini GApps (Play Store, Google Phone, Messages, Contacts, Calendar, Chrome, Gboard, SetupWizard)
WITH_GMS := true
DONT_DEXPREOPT_PREBUILTS := true
RELAX_USES_LIBRARY_CHECK := true
WITH_ADB_INSECURE := true

# Adreno 540 renders UI smoothly at 60 FPS without multi-pass SurfaceFlinger background blur
TARGET_ENABLE_BLUR := false

# Inherit LineageOS 24 & Mini GApps configuration
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)
$(call inherit-product, vendor/gms/gms_mini.mk)

PRODUCT_PRODUCT_PROPERTIES += \
    ro.com.google.ime.theme_id=5 \
    ro.com.google.ime.system_lm_dir=/product/usr/share/ime/google/d3_lms \
    ro.setupwizard.enterprise_mode=1 \
    ro.setupwizard.esim_cid_ignore=00000001 \
    setupwizard.feature.baseline_setupwizard_enabled=true \
    setupwizard.feature.portal_notification=true \
    setupwizard.feature.enable_quick_start_flow=true \
    setupwizard.feature.enable_restore_anytime=true \
    setupwizard.feature.enable_wifi_tracker=true \
    setupwizard.feature.lifecycle_refactoring=true \
    setupwizard.feature.notification_refactoring=true \
    setupwizard.feature.show_pai_screen_in_main_flow.carrier1839=false \
    setupwizard.feature.show_pixel_tos=true \
    setupwizard.feature.show_support_link_in_deferred_setup=false

PRODUCT_NAME := lineage_dumpling
PRODUCT_DEVICE := dumpling
PRODUCT_BRAND := OnePlus
PRODUCT_MODEL := ONEPLUS A5010
PRODUCT_MANUFACTURER := OnePlus

PRODUCT_GMS_CLIENTID_BASE := android-oneplus

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="OnePlus5T-user 10 QKQ1.191014.012 2010292145 release-keys" \
    BuildFingerprint=OnePlus/OnePlus5T/OnePlus5T:10/QKQ1.191014.012/2010292145:user/release-keys \
    DeviceName=OnePlus5T \
    DeviceProduct=OnePlus5T

# Non-A/B device
AB_OTA_UPDATER := false

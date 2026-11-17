#
# Copyright (C) 2026 The Evolution X Project
# SPDX-License-Identifier: Apache-2.0
#

LOCAL_PATH := device/oneplus/dumpling

# Enable updating of APEXes / flattenable fallback
$(call inherit-product, $(SRC_TARGET_DIR)/product/updatable_apex.mk)

# Dalvik heap configuration (6GB LPDDR4X)
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)

# Screen density (401 DPI on 6.01" 1080x2160)
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi
PRODUCT_AAPT_PREBUILT_DPI := xxxhdpi xxhdpi xhdpi hdpi

# Overlays
DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH) \
    hardware/lineage/interfaces/light

# Dynamic Partitions & fastbootd
PRODUCT_PACKAGES += \
    fastbootd

# Graphics: drm_hwcomposer + minigbm (msm) + Mesa3D Freedreno (Adreno 540) + SwiftShader Vulkan fallback
PRODUCT_REQUIRES_INSECURE_EXECMEM_FOR_SWIFTSHADER := true

PRODUCT_PACKAGES += \
    com.android.hardware.graphics.composer.drm_hwcomposer \
    android.hardware.graphics.allocator-service.minigbm \
    mapper.minigbm \
    gralloc.minigbm_msm \
    libminigbm_gralloc \
    vulkan.pastel \
    com.oneplus5t.keyhandler

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/mesa/lib64/libgallium_dri.so:$(TARGET_COPY_OUT_VENDOR)/lib64/libgallium_dri.so \
    $(LOCAL_PATH)/prebuilt/mesa/lib64/libgallium_dri.so:$(TARGET_COPY_OUT_VENDOR)/lib64/dri/libgallium_dri.so \
    $(LOCAL_PATH)/prebuilt/mesa/lib64/egl/libEGL_mesa.so:$(TARGET_COPY_OUT_VENDOR)/lib64/egl/libEGL_mesa.so \
    $(LOCAL_PATH)/prebuilt/mesa/lib64/egl/libGLESv1_CM_mesa.so:$(TARGET_COPY_OUT_VENDOR)/lib64/egl/libGLESv1_CM_mesa.so \
    $(LOCAL_PATH)/prebuilt/mesa/lib64/egl/libGLESv2_mesa.so:$(TARGET_COPY_OUT_VENDOR)/lib64/egl/libGLESv2_mesa.so

PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.egl=mesa \
    ro.hardware.gralloc=minigbm_msm \
    drm.gpu.vendor_name=msm \
    ro.hardware.vulkan=pastel \
    ro.sf.lcd_density=420 \
    vendor.minigbm.debug=nocompression \
    ro.hardware.hwcomposer=drm \
    vendor.hwc.drm.device=/dev/dri/card0 \
    debug.renderengine.backend=skiaglthreaded \
    debug.hwui.renderer=skiagl \
    ro.surface_flinger.supports_background_blur=0 \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
    ro.surface_flinger.use_context_priority=true \
    ro.surface_flinger.has_HDR_display=false \
    ro.surface_flinger.has_wide_color_display=false \
    ro.surface_flinger.use_color_management=false \
    debug.sf.latch_unsignaled=0 \
    debug.sf.auto_latch_unsignaled=false \
    ro.opengles.version=196610 \
    wifi.interface=wlan0

# Audio (Mainline ALSA QDSP6 WCD9335 + CPE + TFA9890 on card 0)
$(call inherit-product, hardware/interfaces/audio/aidl/default/audio_effects.mk)
$(call inherit-product, frameworks/av/services/audiopolicy/audio_policy_config_vendor_1.mk)
PRODUCT_PACKAGES += \
    com.android.hardware.audio \
    tinymix \
    tinyplay

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/configs/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_with_le_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml

# Wi-Fi (WCN3990 ath10k_snoc)
PRODUCT_PACKAGES += \
    com.android.hardware.wifi \
    com.android.hardware.wpa_supplicant \
    com.android.hardware.net.nlinterceptor

# Bluetooth (WCN3990 hci0 Linux BTPROTO_HCI + factory .bt_nv.bin BD_ADDR init)
PRODUCT_PACKAGES += \
    android.hardware.bluetooth-service.default \
    oneplus5t-bt-init

# USB FunctionFS MTP/ADB (Mainline Linux 6.18 dwc3 a800000.usb)
PRODUCT_PACKAGES += \
    com.android.hardware.usb

PRODUCT_PROPERTY_OVERRIDES += \
    sys.usb.mtp.device_type=3

PRODUCT_PACKAGES += \
    vendor_bt_firmware_mountpoint \
    vendor_dsp_mountpoint \
    vendor_firmware_mnt_mountpoint

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    sys.use_memfd=true \
    persist.sys.usb.config=adb \
    ro.adb.secure=0

# Health, Power, Lights, Vibrator, Thermal, Memtrack
PRODUCT_PACKAGES += \
    android.hardware.health-service.example \
    android.hardware.health-service.example_recovery \
    com.android.hardware.power \
    android.hardware.light-service.lineage \
    android.hardware.vibrator-service.oneplus5t \
    com.android.hardware.thermal \
    com.android.hardware.memtrack \
    com.android.hardware.dumpstate

# Security, DRM (Widevine L3 ClearKey), KeyMint & Gatekeeper (Android 17 AIDL)
PRODUCT_PACKAGES += \
    com.android.hardware.keymint.rust_nonsecure \
    com.android.hardware.gatekeeper.nonsecure \
    com.android.hardware.security.authgraph \
    com.android.hardware.security.secretkeeper \
    android.hardware.drm@latest-service.clearkey \
    com.android.hardware.biometrics.fingerprint.virtual

# Sensors, GNSS & Telephony framework
PRODUCT_PACKAGES += \
    android.hardware.sensors-service.oneplus5t \
    com.android.hardware.gnss \
    v4l2-ctl

$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_vendor.mk)

# Prebuilt GPU firmware in early ramdisk so drm/msm loads before /vendor mounts
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/firmware/a530_pm4.fw:$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/a530_pm4.fw \
    $(LOCAL_PATH)/prebuilt/firmware/a530_pfp.fw:$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/a530_pfp.fw \
    $(LOCAL_PATH)/prebuilt/firmware/a540_gpmu.fw2:$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/a540_gpmu.fw2

# Mainline Linux 6.18 QCOM / QCA / WCN3990 Firmware (/vendor/firmware)
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/prebuilt/firmware,$(TARGET_COPY_OUT_VENDOR)/firmware)

# Verified 6.18 Kernel Modules (installed to /vendor/lib/modules & recovery)
KERNEL_MODULE_FILES := $(wildcard $(LOCAL_PATH)/prebuilt/kernel/modules/*.ko)
PRODUCT_COPY_FILES += \
    $(foreach mod,$(KERNEL_MODULE_FILES),$(mod):$(TARGET_COPY_OUT_VENDOR)/lib/modules/$(notdir $(mod))) \
    $(foreach mod,$(KERNEL_MODULE_FILES),$(mod):$(TARGET_COPY_OUT_RECOVERY)/root/vendor/lib/modules/$(notdir $(mod)))

# Static QMI / Remoteproc Userspace Daemons & Clean-Room Utilities
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/prebuilt/bin/rmtfs:$(TARGET_COPY_OUT_VENDOR)/bin/rmtfs \
    $(LOCAL_PATH)/prebuilt/bin/tqftpserv:$(TARGET_COPY_OUT_VENDOR)/bin/tqftpserv \
    $(LOCAL_PATH)/prebuilt/bin/diag-router:$(TARGET_COPY_OUT_VENDOR)/bin/diag-router \
    $(LOCAL_PATH)/prebuilt/bin/sns-reg2-qmi:$(TARGET_COPY_OUT_VENDOR)/bin/sns-reg2-qmi \
    $(LOCAL_PATH)/prebuilt/bin/wlan-mac-qmi:$(TARGET_COPY_OUT_VENDOR)/bin/wlan-mac-qmi \
    $(LOCAL_PATH)/prebuilt/bin/qrtr-lookup:$(TARGET_COPY_OUT_VENDOR)/bin/qrtr-lookup \
    $(LOCAL_PATH)/prebuilt/bin/qrtr-cfg:$(TARGET_COPY_OUT_VENDOR)/bin/qrtr-cfg \
    $(LOCAL_PATH)/prebuilt/bin/oneplus5t-sysfs-hals:$(TARGET_COPY_OUT_VENDOR)/bin/oneplus5t-sysfs-hals \
    $(LOCAL_PATH)/prebuilt/bin/oneplus5t-sensors-slpi:$(TARGET_COPY_OUT_VENDOR)/bin/oneplus5t-sensors-slpi \
    $(LOCAL_PATH)/prebuilt/bin/oneplus5t-ril-qmi:$(TARGET_COPY_OUT_VENDOR)/bin/oneplus5t-ril-qmi

# Init, Fstab, Ueventd
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:$(TARGET_COPY_OUT_RAMDISK)/fstab.qcom \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:$(TARGET_COPY_OUT_RECOVERY)/root/first_stage_ramdisk/fstab.qcom \
    $(LOCAL_PATH)/rootdir/etc/init.qcom.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.qcom.rc \
    $(LOCAL_PATH)/rootdir/etc/init.qcom.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.qcom.usb.rc \
    $(LOCAL_PATH)/rootdir/etc/init.recovery.qcom.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.qcom.rc \
    $(LOCAL_PATH)/rootdir/etc/ueventd.qcom.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \
    $(LOCAL_PATH)/rootdir/bin/init.oneplus5t.hw.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.oneplus5t.hw.sh \
    $(LOCAL_PATH)/configs/idc/Synaptics_s3508.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/Synaptics_s3508.idc \
    $(LOCAL_PATH)/configs/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    $(LOCAL_PATH)/configs/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf

# Permissions XMLs
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.telephony.ims.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.ims.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2023-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.vulkan.deqp.level.xml \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml


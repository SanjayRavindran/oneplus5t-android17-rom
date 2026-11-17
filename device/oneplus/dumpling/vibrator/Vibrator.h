#pragma once

#include <aidl/android/hardware/vibrator/BnVibrator.h>
#include <android-base/thread_annotations.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace aidl::android::hardware::vibrator {

class Vibrator : public BnVibrator {
  public:
    Vibrator();
    ~Vibrator() override;

    ndk::ScopedAStatus getCapabilities(int32_t* _aidl_return) override;
    ndk::ScopedAStatus off() override;
    ndk::ScopedAStatus on(int32_t timeoutMs,
                          const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus perform(Effect effect, EffectStrength strength,
                               const std::shared_ptr<IVibratorCallback>& callback,
                               int32_t* _aidl_return) override;
    ndk::ScopedAStatus performVendorEffect(
            const VendorEffect& effect,
            const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus getSupportedEffects(std::vector<Effect>* _aidl_return) override;
    ndk::ScopedAStatus setAmplitude(float amplitude) override;
    ndk::ScopedAStatus setExternalControl(bool enabled) override;
    ndk::ScopedAStatus getCompositionDelayMax(int32_t* maxDelayMs) override;
    ndk::ScopedAStatus getCompositionSizeMax(int32_t* maxSize) override;
    ndk::ScopedAStatus getSupportedPrimitives(std::vector<CompositePrimitive>* supported) override;
    ndk::ScopedAStatus getPrimitiveDuration(CompositePrimitive primitive,
                                            int32_t* durationMs) override;
    ndk::ScopedAStatus compose(const std::vector<CompositeEffect>& composite,
                               const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus getSupportedAlwaysOnEffects(std::vector<Effect>* _aidl_return) override;
    ndk::ScopedAStatus alwaysOnEnable(int32_t id, Effect effect, EffectStrength strength) override;
    ndk::ScopedAStatus alwaysOnDisable(int32_t id) override;
    ndk::ScopedAStatus getResonantFrequency(float* resonantFreqHz) override;
    ndk::ScopedAStatus getQFactor(float* qFactor) override;
    ndk::ScopedAStatus getFrequencyResolution(float* freqResolutionHz) override;
    ndk::ScopedAStatus getFrequencyMinimum(float* freqMinimumHz) override;
    ndk::ScopedAStatus getBandwidthAmplitudeMap(std::vector<float>* _aidl_return) override;
    ndk::ScopedAStatus getPwlePrimitiveDurationMax(int32_t* durationMs) override;
    ndk::ScopedAStatus getPwleCompositionSizeMax(int32_t* maxSize) override;
    ndk::ScopedAStatus getSupportedBraking(std::vector<Braking>* supported) override;
    ndk::ScopedAStatus composePwle(const std::vector<PrimitivePwle>& composite,
                                   const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus getFrequencyToOutputAccelerationMap(
            std::vector<FrequencyAccelerationMapEntry>* _aidl_return) override;
    ndk::ScopedAStatus getPwleV2PrimitiveDurationMaxMillis(int32_t* maxDurationMs) override;
    ndk::ScopedAStatus getPwleV2PrimitiveDurationMinMillis(int32_t* minDurationMs) override;
    ndk::ScopedAStatus getPwleV2CompositionSizeMax(int32_t* maxSize) override;
    ndk::ScopedAStatus composePwleV2(const CompositePwleV2& composite,
                                     const std::shared_ptr<IVibratorCallback>& callback) override;

    void setGlobalVibrationCallback(const std::shared_ptr<IVibratorCallback>& callback);

  private:
    bool ensureDeviceOpenLocked() REQUIRES(mMutex);
    void stopHardwareLocked() REQUIRES(mMutex);
    bool startHardwareLocked(int32_t timeoutMs, float amplitude) REQUIRES(mMutex);
    ndk::ScopedAStatus triggerVibrate(int32_t timeoutMs, float amplitude,
                                      const std::shared_ptr<IVibratorCallback>& callback);

    mutable std::mutex mMutex;
    std::condition_variable mCv;
    int mFd GUARDED_BY(mMutex) = -1;
    int32_t mEffectId GUARDED_BY(mMutex) = -1;
    float mAmplitude GUARDED_BY(mMutex) = 0.8f;
    bool mIsVibrating GUARDED_BY(mMutex) = false;
    uint64_t mGeneration GUARDED_BY(mMutex) = 0;
    int32_t mCapabilities GUARDED_BY(mMutex) = 0;
    std::shared_ptr<IVibratorCallback> mVibrationCallback GUARDED_BY(mMutex);
    std::shared_ptr<IVibratorCallback> mGlobalVibrationCallback GUARDED_BY(mMutex);
};

}  // namespace aidl::android::hardware::vibrator

#define LOG_TAG "android.hardware.vibrator-service.oneplus5t"

#include "Vibrator.h"

#include <android-base/logging.h>
#include <cerrno>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace aidl::android::hardware::vibrator {

namespace {
constexpr float kResonantFrequencyHz = 150.0f;
constexpr float kQFactor = 11.0f;
constexpr int32_t kMaxDurationMs = 60000;

float strengthToAmplitude(EffectStrength strength) {
    switch (strength) {
        case EffectStrength::LIGHT:
            return 0.40f;
        case EffectStrength::MEDIUM:
            return 0.70f;
        case EffectStrength::STRONG:
            return 1.00f;
        default:
            return -1.0f;
    }
}

int32_t effectToDurationMs(Effect effect) {
    switch (effect) {
        case Effect::TICK:
            return 15;
        case Effect::CLICK:
            return 25;
        case Effect::DOUBLE_CLICK:
            return 30;
        case Effect::HEAVY_CLICK:
            return 45;
        default:
            return -1;
    }
}

void notifyCallbackAsync(std::shared_ptr<IVibratorCallback> cb1,
                         std::shared_ptr<IVibratorCallback> cb2) {
    if (!cb1 && !cb2) return;
    std::thread([cb1 = std::move(cb1), cb2 = std::move(cb2)]() {
        if (cb1) (void)cb1->onComplete();
        if (cb2) (void)cb2->onComplete();
    }).detach();
}
}  // namespace

Vibrator::Vibrator() {
    std::lock_guard lock(mMutex);
    ensureDeviceOpenLocked();
}

Vibrator::~Vibrator() {
    std::lock_guard lock(mMutex);
    ++mGeneration;
    mCv.notify_all();
    stopHardwareLocked();
    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }
}

bool Vibrator::ensureDeviceOpenLocked() {
    if (mFd >= 0) return true;
    for (int i = 0; i < 32; ++i) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        char name[128] = {};
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
            strcmp(name, "spmi_haptics") == 0) {
            mFd = fd;
            LOG(INFO) << "Opened spmi_haptics at " << path;
            return true;
        }
        close(fd);
    }
    LOG(WARNING) << "spmi_haptics input device not found";
    return false;
}

void Vibrator::stopHardwareLocked() {
    if (mFd >= 0 && mEffectId >= 0) {
        struct input_event play = {};
        play.type = EV_FF;
        play.code = static_cast<uint16_t>(mEffectId);
        play.value = 0;
        (void)write(mFd, &play, sizeof(play));
        (void)ioctl(mFd, EVIOCRMFF, mEffectId);
        mEffectId = -1;
    }
}

bool Vibrator::startHardwareLocked(int32_t timeoutMs, float amplitude) {
    if (!ensureDeviceOpenLocked()) return false;
    stopHardwareLocked();

    int32_t clampedMs = std::clamp(timeoutMs, 1, kMaxDurationMs);
    float clampedAmp = std::clamp(amplitude, 0.05f, 1.0f);
    uint16_t mag = static_cast<uint16_t>(clampedAmp * 0xFFFFU);

    struct ff_effect eff = {};
    eff.type = FF_RUMBLE;
    eff.id = -1;
    eff.u.rumble.strong_magnitude = mag;
    eff.u.rumble.weak_magnitude = mag;
    eff.replay.length = static_cast<uint16_t>(clampedMs);
    eff.replay.delay = 0;

    if (ioctl(mFd, EVIOCSFF, &eff) < 0) {
        LOG(ERROR) << "EVIOCSFF failed: " << strerror(errno);
        return false;
    }
    mEffectId = eff.id;

    struct input_event play = {};
    play.type = EV_FF;
    play.code = static_cast<uint16_t>(eff.id);
    play.value = 1;
    if (write(mFd, &play, sizeof(play)) != sizeof(play)) {
        LOG(ERROR) << "EV_FF write failed: " << strerror(errno);
        (void)ioctl(mFd, EVIOCRMFF, mEffectId);
        mEffectId = -1;
        return false;
    }
    return true;
}

ndk::ScopedAStatus Vibrator::triggerVibrate(int32_t timeoutMs, float amplitude,
                                            const std::shared_ptr<IVibratorCallback>& callback) {
    if (timeoutMs <= 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::shared_ptr<IVibratorCallback> oldCb, oldGlobalCb;
    uint64_t gen = 0;
    {
        std::lock_guard lock(mMutex);
        oldCb = std::move(mVibrationCallback);
        oldGlobalCb = std::move(mGlobalVibrationCallback);
        mVibrationCallback = callback;
        mIsVibrating = startHardwareLocked(timeoutMs, amplitude);
        gen = ++mGeneration;
        mCv.notify_all();
    }
    notifyCallbackAsync(std::move(oldCb), std::move(oldGlobalCb));

    std::thread([this, gen, timeoutMs, callback]() {
            std::shared_ptr<IVibratorCallback> cb, gcb;
            {
                std::unique_lock lock(mMutex);
                mCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [this, gen]() { return mGeneration != gen; });
                if (mGeneration != gen) return;
                stopHardwareLocked();
                mIsVibrating = false;
                if (mVibrationCallback == callback) {
                    cb = std::move(mVibrationCallback);
                }
                gcb = std::move(mGlobalVibrationCallback);
            }
            if (cb) (void)cb->onComplete();
            if (gcb) (void)gcb->onComplete();
        }).detach();
    return ndk::ScopedAStatus::ok();
}

void Vibrator::setGlobalVibrationCallback(const std::shared_ptr<IVibratorCallback>& callback) {
    std::shared_ptr<IVibratorCallback> immediate;
    {
        std::lock_guard lock(mMutex);
        if (mIsVibrating) {
            mGlobalVibrationCallback = callback;
        } else {
            immediate = callback;
        }
    }
    if (immediate) notifyCallbackAsync(std::move(immediate), nullptr);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    std::lock_guard lock(mMutex);
    if (mCapabilities == 0) {
        mCapabilities = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                        IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_GET_RESONANT_FREQUENCY |
                        IVibrator::CAP_GET_Q_FACTOR;
    }
    *_aidl_return = mCapabilities;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    std::shared_ptr<IVibratorCallback> cb, gcb;
    {
        std::lock_guard lock(mMutex);
        ++mGeneration;
        mCv.notify_all();
        stopHardwareLocked();
        mIsVibrating = false;
        cb = std::move(mVibrationCallback);
        gcb = std::move(mGlobalVibrationCallback);
    }
    notifyCallbackAsync(std::move(cb), std::move(gcb));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback>& callback) {
    float amp = 0.8f;
    {
        std::lock_guard lock(mMutex);
        amp = mAmplitude;
    }
    return triggerVibrate(timeoutMs, amp, callback);
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback>& callback,
                                     int32_t* _aidl_return) {
    int32_t durationMs = effectToDurationMs(effect);
    float amp = strengthToAmplitude(strength);
    if (durationMs < 0 || amp < 0.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    *_aidl_return = durationMs;
    return triggerVibrate(durationMs, amp, callback);
}

ndk::ScopedAStatus Vibrator::performVendorEffect(
        const VendorEffect&, const std::shared_ptr<IVibratorCallback>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
    *_aidl_return = {Effect::CLICK, Effect::TICK, Effect::DOUBLE_CLICK, Effect::HEAVY_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard lock(mMutex);
    mAmplitude = amplitude;
    if (mIsVibrating && mFd >= 0 && mEffectId >= 0) {
        uint16_t mag = static_cast<uint16_t>(std::clamp(amplitude, 0.05f, 1.0f) * 0xFFFFU);
        struct ff_effect eff = {};
        eff.type = FF_RUMBLE;
        eff.id = static_cast<int16_t>(mEffectId);
        eff.u.rumble.strong_magnitude = mag;
        eff.u.rumble.weak_magnitude = mag;
        eff.replay.length = 5000;
        (void)ioctl(mFd, EVIOCSFF, &eff);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* maxDelayMs) {
    *maxDelayMs = 0;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* maxSize) {
    *maxSize = 0;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getSupportedPrimitives(std::vector<CompositePrimitive>* supported) {
    supported->clear();
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive, int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>&,
                                     const std::shared_ptr<IVibratorCallback>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect>* _aidl_return) {
    _aidl_return->clear();
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t, Effect, EffectStrength) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getResonantFrequency(float* resonantFreqHz) {
    *resonantFreqHz = kResonantFrequencyHz;
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Vibrator::getQFactor(float* qFactor) {
    *qFactor = kQFactor;
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus Vibrator::getFrequencyResolution(float*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getFrequencyMinimum(float*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getBandwidthAmplitudeMap(std::vector<float>*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getPwlePrimitiveDurationMax(int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getPwleCompositionSizeMax(int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getSupportedBraking(std::vector<Braking>*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>&,
                                         const std::shared_ptr<IVibratorCallback>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getFrequencyToOutputAccelerationMap(
        std::vector<FrequencyAccelerationMapEntry>*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getPwleV2PrimitiveDurationMaxMillis(int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getPwleV2PrimitiveDurationMinMillis(int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::getPwleV2CompositionSizeMax(int32_t*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::composePwleV2(const CompositePwleV2&,
                                           const std::shared_ptr<IVibratorCallback>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::vibrator

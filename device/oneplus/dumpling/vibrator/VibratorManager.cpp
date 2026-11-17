#include "VibratorManager.h"

namespace aidl::android::hardware::vibrator {

static constexpr int32_t kDefaultVibratorId = 1;

ndk::ScopedAStatus VibratorManager::getCapabilities(int32_t* _aidl_return) {
    *_aidl_return = IVibratorManager::CAP_SYNC | IVibratorManager::CAP_PREPARE_ON |
                    IVibratorManager::CAP_PREPARE_PERFORM |
                    IVibratorManager::CAP_MIXED_TRIGGER_ON |
                    IVibratorManager::CAP_MIXED_TRIGGER_PERFORM |
                    IVibratorManager::CAP_TRIGGER_CALLBACK;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::getVibratorIds(std::vector<int32_t>* _aidl_return) {
    *_aidl_return = {kDefaultVibratorId};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::getVibrator(int32_t vibratorId,
                                                std::shared_ptr<IVibrator>* _aidl_return) {
    if (vibratorId == kDefaultVibratorId) {
        *_aidl_return = mDefaultVibrator;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = nullptr;
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus VibratorManager::prepareSynced(const std::vector<int32_t>& vibratorIds) {
    if (vibratorIds.size() != 1 || vibratorIds[0] != kDefaultVibratorId) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard lock(mMutex);
    if (mIsPreparing) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    mIsPreparing = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::triggerSynced(
        const std::shared_ptr<IVibratorCallback>& callback) {
    std::lock_guard lock(mMutex);
    if (!mIsPreparing) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    mIsPreparing = false;
    mDefaultVibrator->setGlobalVibrationCallback(callback);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::cancelSynced() {
    std::lock_guard lock(mMutex);
    if (!mIsPreparing) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    mIsPreparing = false;
    (void)mDefaultVibrator->off();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::startSession(
        const std::vector<int32_t>&, const VibrationSessionConfig&,
        const std::shared_ptr<IVibratorCallback>&, std::shared_ptr<IVibrationSession>*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus VibratorManager::clearSessions() {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::startHapticGeneratorSession(
        const std::vector<int32_t>&, const HapticGeneratorConfig&,
        const std::shared_ptr<IVibratorCallback>&, HapticGeneratorSession*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::vibrator

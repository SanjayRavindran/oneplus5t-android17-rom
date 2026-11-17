#pragma once

#include "Vibrator.h"
#include <aidl/android/hardware/vibrator/BnVibratorManager.h>

#include <mutex>
#include <vector>

namespace aidl::android::hardware::vibrator {

class VibratorManager : public BnVibratorManager {
  public:
    explicit VibratorManager(std::shared_ptr<Vibrator> vibrator)
        : mDefaultVibrator(std::move(vibrator)) {}

    ndk::ScopedAStatus getCapabilities(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getVibratorIds(std::vector<int32_t>* _aidl_return) override;
    ndk::ScopedAStatus getVibrator(int32_t vibratorId,
                                   std::shared_ptr<IVibrator>* _aidl_return) override;
    ndk::ScopedAStatus prepareSynced(const std::vector<int32_t>& vibratorIds) override;
    ndk::ScopedAStatus triggerSynced(const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus cancelSynced() override;
    ndk::ScopedAStatus startSession(const std::vector<int32_t>& vibratorIds,
                                    const VibrationSessionConfig& config,
                                    const std::shared_ptr<IVibratorCallback>& callback,
                                    std::shared_ptr<IVibrationSession>* _aidl_return) override;
    ndk::ScopedAStatus clearSessions() override;
    ndk::ScopedAStatus startHapticGeneratorSession(
            const std::vector<int32_t>& vibratorIds, const HapticGeneratorConfig& config,
            const std::shared_ptr<IVibratorCallback>& callback,
            HapticGeneratorSession* _aidl_return) override;

  private:
    std::shared_ptr<Vibrator> mDefaultVibrator;
    std::mutex mMutex;
    bool mIsPreparing = false;
};

}  // namespace aidl::android::hardware::vibrator

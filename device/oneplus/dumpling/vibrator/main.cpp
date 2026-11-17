#include "Vibrator.h"
#include "VibratorManager.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::vibrator::Vibrator;
using aidl::android::hardware::vibrator::VibratorManager;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto vib = ndk::SharedRefBase::make<Vibrator>();
    binder_status_t status = AServiceManager_addService(
            vib->asBinder().get(), Vibrator::makeServiceName("default").c_str());
    CHECK_EQ(status, STATUS_OK);

    auto vibManager = ndk::SharedRefBase::make<VibratorManager>(vib);
    status = AServiceManager_addService(
            vibManager->asBinder().get(), VibratorManager::makeServiceName("default").c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}

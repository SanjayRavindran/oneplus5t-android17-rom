#include "SlpiSensors.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::sensors::SlpiSensors;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto sensors = ndk::SharedRefBase::make<SlpiSensors>();
    const std::string name = std::string() + SlpiSensors::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(sensors->asBinder().get(), name.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}

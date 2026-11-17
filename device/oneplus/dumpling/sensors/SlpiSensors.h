#pragma once

#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <aidl/android/hardware/sensors/BnSensors.h>
#include <fmq/AidlMessageQueue.h>
#include <hardware_legacy/power.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace aidl::android::hardware::sensors {

using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;
using ::android::hardware::EventFlag;

struct SlpiSensorState {
    SensorInfo info;
    uint8_t slpiSensorId = 0;
    uint8_t slpiDataType = 0;
    uint16_t minHz = 5;
    uint16_t maxHz = 5;
    uint16_t currentHz = 5;
    float q16Scale = 1.0f / 65536.0f;
    bool isOnChange = false;
    bool enabled = false;
    bool hasLastScalar = false;
    float lastScalar = 0.0f;
};

class SlpiSensors : public BnSensors {
    static constexpr const char* kWakeLockName = "SensorsHAL_WAKEUP";

  public:
    SlpiSensors();
    ~SlpiSensors() override;

    ::ndk::ScopedAStatus activate(int32_t in_sensorHandle, bool in_enabled) override;
    ::ndk::ScopedAStatus batch(int32_t in_sensorHandle, int64_t in_samplingPeriodNs,
                               int64_t in_maxReportLatencyNs) override;
    ::ndk::ScopedAStatus configDirectReport(
            int32_t in_sensorHandle, int32_t in_channelHandle,
            ISensors::RateLevel in_rate, int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus flush(int32_t in_sensorHandle) override;
    ::ndk::ScopedAStatus getSensorsList(std::vector<SensorInfo>* _aidl_return) override;
    ::ndk::ScopedAStatus initialize(
            const MQDescriptor<Event, SynchronizedReadWrite>& in_eventQueueDescriptor,
            const MQDescriptor<int32_t, SynchronizedReadWrite>& in_wakeLockDescriptor,
            const std::shared_ptr<ISensorsCallback>& in_sensorsCallback) override;
    ::ndk::ScopedAStatus injectSensorData(const Event& in_event) override;
    ::ndk::ScopedAStatus registerDirectChannel(
            const ISensors::SharedMemInfo& in_mem, int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus setOperationMode(ISensors::OperationMode in_mode) override;
    ::ndk::ScopedAStatus unregisterDirectChannel(int32_t in_channelHandle) override;

  private:
    void initSensorCatalog();
    void slpiLoop();
    void sendLookup();
    void applyReport(const SlpiSensorState& s, bool enable);
    void handleIndication(const uint8_t* buf, size_t len);
    void postEvents(const std::vector<Event>& events, bool wakeup);
    void readWakeLockFMQ();
    void updateWakeLock(int32_t eventsWritten, int32_t eventsHandled);
    void deleteEventFlagLocked();

    std::mutex mStateLock;
    std::map<int32_t, SlpiSensorState> mSensors;
    OperationMode mMode = OperationMode::NORMAL;

    int mSock = -1;
    std::mutex mQmiLock;
    bool mSlpiOnline = false;
    uint16_t mSlpiNode = 0;
    uint32_t mSlpiPort = 0;
    uint16_t mTxnId = 1;

    std::atomic_bool mStop{false};
    std::thread mSlpiThread;

    std::shared_ptr<ISensorsCallback> mCallback;
    std::mutex mWriteLock;
    std::unique_ptr<AidlMessageQueue<Event, SynchronizedReadWrite>> mEventQueue;
    std::unique_ptr<AidlMessageQueue<int32_t, SynchronizedReadWrite>> mWakeLockQueue;
    EventFlag* mEventQueueFlag = nullptr;

    std::atomic_bool mReadWakeLockQueueRun{false};
    std::thread mWakeLockThread;
    std::mutex mWakeLockLock;
    int32_t mOutstandingWakeUpEvents = 0;
    uint32_t mAutoReleaseWakeLockTime = 0;
    bool mHasWakeLock = false;
};

}  // namespace aidl::android::hardware::sensors

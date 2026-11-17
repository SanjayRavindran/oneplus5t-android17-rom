#define LOG_TAG "android.hardware.sensors-service.oneplus5t"

#include "SlpiSensors.h"

#include <android-base/logging.h>
#include <linux/qrtr.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/SystemClock.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aidl::android::hardware::sensors {

namespace {
constexpr uint32_t kSnsSmgrSvc = 0x100;
constexpr uint16_t kMsgBufferingReq = 0x0021;
constexpr uint16_t kMsgBufferingInd = 0x0022;
constexpr uint8_t kActionAdd = 1;
constexpr uint8_t kActionDelete = 2;

void addTlvU8(std::vector<uint8_t>& out, uint8_t type, uint8_t val) {
    out.push_back(type);
    out.push_back(1);
    out.push_back(0);
    out.push_back(val);
}

void addTlvU32(std::vector<uint8_t>& out, uint8_t type, uint32_t val) {
    out.push_back(type);
    out.push_back(4);
    out.push_back(0);
    uint8_t b[4];
    memcpy(b, &val, 4);
    out.insert(out.end(), b, b + 4);
}
}  // namespace

SlpiSensors::SlpiSensors() {
    initSensorCatalog();
    mSock = socket(AF_QIPCRTR, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (mSock < 0) {
        PLOG(ERROR) << "Failed to create AF_QIPCRTR socket";
    }
    mSlpiThread = std::thread(&SlpiSensors::slpiLoop, this);
}

SlpiSensors::~SlpiSensors() {
    mStop = true;
    if (mReadWakeLockQueueRun.load()) {
        mReadWakeLockQueueRun = false;
        if (mWakeLockThread.joinable()) mWakeLockThread.join();
    }
    if (mSlpiThread.joinable()) mSlpiThread.join();
    if (mSock >= 0) {
        close(mSock);
        mSock = -1;
    }
    std::lock_guard<std::mutex> lock(mWriteLock);
    deleteEventFlagLocked();
}

void SlpiSensors::initSensorCatalog() {
    auto add = [this](int32_t handle, const char* name, const char* vendor, SensorType type,
                      float maxRange, float resolution, float power, int32_t minDelayUs,
                      int32_t maxDelayUs, uint32_t flags, uint8_t sid, uint8_t dt,
                      uint16_t minHz, uint16_t maxHz, float scale, bool onChange) {
        SlpiSensorState s;
        s.info.sensorHandle = handle;
        s.info.name = name;
        s.info.vendor = vendor;
        s.info.version = 1;
        s.info.type = type;
        s.info.typeAsString = "";
        s.info.maxRange = maxRange;
        s.info.resolution = resolution;
        s.info.power = power;
        s.info.minDelayUs = minDelayUs;
        s.info.maxDelayUs = maxDelayUs;
        s.info.fifoReservedEventCount = 0;
        s.info.fifoMaxEventCount = 0;
        s.info.requiredPermission = "";
        s.info.flags = flags;
        s.slpiSensorId = sid;
        s.slpiDataType = dt;
        s.minHz = minHz;
        s.maxHz = maxHz;
        s.currentHz = minHz;
        s.q16Scale = scale;
        s.isOnChange = onChange;
        mSensors[handle] = std::move(s);
    };

    // Live hardware IDs verified via QMI SNS_SMGR_SVC (0x100) on OnePlus 5T (dumpling)
    add(1, "BMI160 Accelerometer", "BOSCH", SensorType::ACCELEROMETER,
        78.4532f, 0.0023956f, 0.18f, 2500, 76923, 0,
        0, 0, 13, 400, 1.0f / 65536.0f, false);

    add(2, "BMI160 Gyroscope", "BOSCH", SensorType::GYROSCOPE,
        34.9066f, 0.001065f, 0.85f, 2500, 200000, 0,
        10, 0, 5, 400, 1.0f / 65536.0f, false);

    // SLPI outputs magnetometer in Q16 Gauss; Android expects microtesla (1 G = 100 uT).
    add(3, "AK09911 Magnetometer", "AKM", SensorType::MAGNETIC_FIELD,
        4912.0f, 0.6f, 2.8f, 10000, 100000, 0,
        20, 0, 10, 100, 100.0f / 65536.0f, false);

    add(4, "PA224 Proximity", "TXC", SensorType::PROXIMITY,
        10.0f, 1.0f, 0.15f, 200000, 200000,
        static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE) |
                static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_WAKE_UP),
        40, 0, 5, 5, 1.0f / 65536.0f, true);

    add(5, "TCS3400 Ambient Light", "ams AG", SensorType::LIGHT,
        65535.0f, 1.0f, 0.15f, 200000, 200000,
        static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE),
        60, 1, 5, 5, 1.0f / 65536.0f, true);
}

void SlpiSensors::sendLookup() {
    if (mSock < 0) return;
    struct sockaddr_qrtr sq = {};
    socklen_t slen = sizeof(sq);
    if (getsockname(mSock, reinterpret_cast<struct sockaddr*>(&sq), &slen) < 0) return;

    struct qrtr_ctrl_pkt pkt = {};
    pkt.cmd = QRTR_TYPE_NEW_LOOKUP;
    pkt.server.service = kSnsSmgrSvc;

    struct sockaddr_qrtr ns = {};
    ns.sq_family = AF_QIPCRTR;
    ns.sq_node = sq.sq_node;
    ns.sq_port = QRTR_PORT_CTRL;
    (void)sendto(mSock, &pkt, sizeof(pkt), 0, reinterpret_cast<struct sockaddr*>(&ns), sizeof(ns));
}

void SlpiSensors::applyReport(const SlpiSensorState& s, bool enable) {
    std::lock_guard<std::mutex> qlock(mQmiLock);
    if (mSock < 0 || !mSlpiOnline) return;

    std::vector<uint8_t> payload;
    addTlvU8(payload, 0x01, static_cast<uint8_t>(s.info.sensorHandle));
    addTlvU8(payload, 0x02, enable ? kActionAdd : kActionDelete);
    uint16_t hz = std::clamp(s.currentHz, s.minHz, s.maxHz);
    addTlvU32(payload, 0x03, static_cast<uint32_t>(hz) << 16);

    // TLV 0x04: Sampled item count(1) + 8-byte item
    payload.push_back(0x04);
    payload.push_back(9);
    payload.push_back(0);
    payload.push_back(1);  // Count
    payload.push_back(s.slpiSensorId);
    payload.push_back(s.slpiDataType);
    payload.push_back(1);  // RECENT_SAMPLE
    payload.push_back(0);  // FULL_CAL
    uint8_t sr[2] = {static_cast<uint8_t>(hz & 0xff), static_cast<uint8_t>((hz >> 8) & 0xff)};
    payload.push_back(sr[0]);
    payload.push_back(sr[1]);
    payload.push_back(0);  // SampleCount = 0
    payload.push_back(0);

    std::vector<uint8_t> pkt(7 + payload.size());
    uint16_t txn = mTxnId++;
    uint16_t plen = static_cast<uint16_t>(payload.size());
    pkt[0] = 0x00;  // Request
    pkt[1] = static_cast<uint8_t>(txn & 0xff);
    pkt[2] = static_cast<uint8_t>((txn >> 8) & 0xff);
    pkt[3] = static_cast<uint8_t>(kMsgBufferingReq & 0xff);
    pkt[4] = static_cast<uint8_t>((kMsgBufferingReq >> 8) & 0xff);
    pkt[5] = static_cast<uint8_t>(plen & 0xff);
    pkt[6] = static_cast<uint8_t>((plen >> 8) & 0xff);
    memcpy(pkt.data() + 7, payload.data(), payload.size());

    struct sockaddr_qrtr dst = {};
    dst.sq_family = AF_QIPCRTR;
    dst.sq_node = mSlpiNode;
    dst.sq_port = mSlpiPort;
    (void)sendto(mSock, pkt.data(), pkt.size(), 0,
                 reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
}

void SlpiSensors::slpiLoop() {
    if (mSock < 0) return;
    sendLookup();

    uint8_t buf[4096];
    int lookupTicks = 0;
    while (!mStop.load()) {
        struct pollfd pfd = {mSock, POLLIN, 0};
        int prc = poll(&pfd, 1, 500);
        if (prc == 0) {
            bool online = false;
            {
                std::lock_guard<std::mutex> qlock(mQmiLock);
                online = mSlpiOnline;
            }
            if (!online && (++lookupTicks % 2 == 0)) {
                sendLookup();
            }
            continue;
        }
        if (prc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        struct sockaddr_qrtr src = {};
        socklen_t slen = sizeof(src);
        ssize_t n = recvfrom(mSock, buf, sizeof(buf), 0,
                             reinterpret_cast<struct sockaddr*>(&src), &slen);
        if (n <= 0) continue;

        if (src.sq_port == QRTR_PORT_CTRL && n >= static_cast<ssize_t>(sizeof(struct qrtr_ctrl_pkt))) {
            struct qrtr_ctrl_pkt cp = {};
            memcpy(&cp, buf, sizeof(cp));
            if (cp.cmd == QRTR_TYPE_NEW_SERVER && cp.server.service == kSnsSmgrSvc &&
                cp.server.port != 0) {
                bool newlyOnline = false;
                {
                    std::lock_guard<std::mutex> qlock(mQmiLock);
                    if (!mSlpiOnline || mSlpiNode != cp.server.node || mSlpiPort != cp.server.port) {
                        mSlpiNode = static_cast<uint16_t>(cp.server.node);
                        mSlpiPort = cp.server.port;
                        mSlpiOnline = true;
                        newlyOnline = true;
                    }
                }
                if (newlyOnline) {
                    LOG(INFO) << "SLPI SNS_SMGR_SVC online at node=" << cp.server.node
                              << " port=" << cp.server.port;
                    std::lock_guard<std::mutex> slock(mStateLock);
                    for (const auto& [handle, state] : mSensors) {
                        if (state.enabled) {
                            applyReport(state, true);
                        }
                    }
                }
            } else if (cp.cmd == QRTR_TYPE_DEL_SERVER && cp.server.service == kSnsSmgrSvc) {
                std::lock_guard<std::mutex> qlock(mQmiLock);
                if (mSlpiNode == cp.server.node && mSlpiPort == cp.server.port) {
                    mSlpiOnline = false;
                    LOG(WARNING) << "SLPI SNS_SMGR_SVC went offline";
                }
            }
            continue;
        }

        handleIndication(buf, static_cast<size_t>(n));
    }
}

void SlpiSensors::handleIndication(const uint8_t* buf, size_t len) {
    if (len < 7) return;
    uint8_t ctl = buf[0];
    uint16_t msgId = static_cast<uint16_t>(buf[3] | (buf[4] << 8));
    if (ctl != 0x04 || msgId != kMsgBufferingInd) return;

    int32_t reportId = -1;
    std::vector<Event> outEvents;
    bool isWakeUp = false;

    size_t off = 7;
    const uint8_t* samplesPtr = nullptr;
    uint8_t samplesCnt = 0;

    while (off + 3 <= len) {
        uint8_t t = buf[off];
        uint16_t l = static_cast<uint16_t>(buf[off + 1] | (buf[off + 2] << 8));
        off += 3;
        if (off + l > len) break;
        if (t == 0x01 && l >= 1) {
            reportId = static_cast<int32_t>(buf[off]);
        } else if (t == 0x03 && l >= 1) {
            samplesCnt = buf[off];
            size_t avail = (l - 1) / 16;
            if (samplesCnt > avail) samplesCnt = static_cast<uint8_t>(avail);
            samplesPtr = buf + off + 1;
        }
        off += l;
    }

    if (reportId < 1 || samplesCnt == 0 || samplesPtr == nullptr) return;

    std::lock_guard<std::mutex> slock(mStateLock);
    auto it = mSensors.find(reportId);
    if (it == mSensors.end() || !it->second.enabled) return;

    SlpiSensorState& s = it->second;
    isWakeUp = (s.info.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_WAKE_UP)) != 0;
    int64_t nowNs = ::android::elapsedRealtimeNano();
    int64_t stepNs = 1000000000LL / std::max<uint16_t>(s.currentHz, 1);

    for (uint8_t i = 0; i < samplesCnt; ++i) {
        const uint8_t* e = samplesPtr + (i * 16);
        int32_t d[3];
        memcpy(d, e, 12);

        Event ev;
        ev.sensorHandle = s.info.sensorHandle;
        ev.sensorType = s.info.type;
        ev.timestamp = nowNs - static_cast<int64_t>(samplesCnt - 1 - i) * stepNs;

        if (s.info.type == SensorType::PROXIMITY || s.info.type == SensorType::LIGHT) {
            float v = static_cast<float>(d[0]) * s.q16Scale;
            if (s.info.type == SensorType::PROXIMITY) {
                v = std::clamp(v, 0.0f, s.info.maxRange);
                if (s.hasLastScalar && std::fabs(v - s.lastScalar) < 0.5f) continue;
            } else {
                v = std::max(0.0f, v);
                if (s.hasLastScalar && std::fabs(v - s.lastScalar) < 1.0f) continue;
            }
            s.hasLastScalar = true;
            s.lastScalar = v;
            ev.payload.set<Event::EventPayload::Tag::scalar>(v);
        } else {
            Event::EventPayload::Vec3 v3 = {
                    .x = static_cast<float>(d[0]) * s.q16Scale,
                    .y = static_cast<float>(d[1]) * s.q16Scale,
                    .z = static_cast<float>(d[2]) * s.q16Scale,
                    .status = SensorStatus::ACCURACY_HIGH,
            };
            ev.payload.set<Event::EventPayload::Tag::vec3>(v3);
        }
        outEvents.push_back(std::move(ev));
    }

    if (!outEvents.empty()) {
        postEvents(outEvents, isWakeUp);
    }
}

ndk::ScopedAStatus SlpiSensors::activate(int32_t in_sensorHandle, bool in_enabled) {
    SlpiSensorState copy;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mStateLock);
        auto it = mSensors.find(in_sensorHandle);
        if (it == mSensors.end()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        if (it->second.enabled != in_enabled) {
            it->second.enabled = in_enabled;
            if (in_enabled) {
                it->second.hasLastScalar = false;
            }
            copy = it->second;
            changed = true;
        }
    }
    if (changed) {
        applyReport(copy, in_enabled);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SlpiSensors::batch(int32_t in_sensorHandle, int64_t in_samplingPeriodNs,
                                      int64_t /* in_maxReportLatencyNs */) {
    SlpiSensorState copy;
    bool reapply = false;
    {
        std::lock_guard<std::mutex> lock(mStateLock);
        auto it = mSensors.find(in_sensorHandle);
        if (it == mSensors.end()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        int64_t periodNs = std::clamp<int64_t>(in_samplingPeriodNs,
                                               static_cast<int64_t>(it->second.info.minDelayUs) * 1000LL,
                                               static_cast<int64_t>(it->second.info.maxDelayUs) * 1000LL);
        uint16_t hz = (periodNs > 0)
                              ? static_cast<uint16_t>(
                                        std::clamp(1000000000LL / periodNs,
                                                   static_cast<long long>(it->second.minHz),
                                                   static_cast<long long>(it->second.maxHz)))
                              : it->second.minHz;
        if (it->second.currentHz != hz) {
            it->second.currentHz = hz;
            if (it->second.enabled) {
                copy = it->second;
                reapply = true;
            }
        }
    }
    if (reapply) {
        applyReport(copy, false);
        applyReport(copy, true);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SlpiSensors::configDirectReport(int32_t, int32_t, ISensors::RateLevel,
                                                   int32_t* _aidl_return) {
    *_aidl_return = EX_UNSUPPORTED_OPERATION;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus SlpiSensors::flush(int32_t in_sensorHandle) {
    bool isWakeUp = false;
    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(mStateLock);
        auto it = mSensors.find(in_sensorHandle);
        if (it == mSensors.end()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        enabled = it->second.enabled;
        isWakeUp = (it->second.info.flags &
                    static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_WAKE_UP)) != 0;
    }
    if (!enabled) {
        return ndk::ScopedAStatus::fromServiceSpecificError(
                static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
    }
    Event ev;
    ev.sensorHandle = in_sensorHandle;
    ev.sensorType = SensorType::META_DATA;
    ev.timestamp = ::android::elapsedRealtimeNano();
    Event::EventPayload::MetaData meta = {
            .what = Event::EventPayload::MetaData::MetaDataEventType::META_DATA_FLUSH_COMPLETE,
    };
    ev.payload.set<Event::EventPayload::Tag::meta>(meta);
    postEvents({ev}, isWakeUp);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SlpiSensors::getSensorsList(std::vector<SensorInfo>* _aidl_return) {
    std::lock_guard<std::mutex> lock(mStateLock);
    _aidl_return->clear();
    for (const auto& [handle, s] : mSensors) {
        _aidl_return->push_back(s.info);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SlpiSensors::initialize(
        const MQDescriptor<Event, SynchronizedReadWrite>& in_eventQueueDescriptor,
        const MQDescriptor<int32_t, SynchronizedReadWrite>& in_wakeLockDescriptor,
        const std::shared_ptr<ISensorsCallback>& in_sensorsCallback) {
    std::vector<SlpiSensorState> disableList;
    {
        std::lock_guard<std::mutex> lock(mStateLock);
        for (auto& [handle, s] : mSensors) {
            if (s.enabled) {
                s.enabled = false;
                disableList.push_back(s);
            }
        }
    }
    for (const auto& s : disableList) {
        applyReport(s, false);
    }

    if (mReadWakeLockQueueRun.load()) {
        mReadWakeLockQueueRun = false;
        if (mWakeLockThread.joinable()) mWakeLockThread.join();
    }

    mCallback = in_sensorsCallback;
    ndk::ScopedAStatus result = ndk::ScopedAStatus::ok();
    {
        std::lock_guard<std::mutex> lock(mWriteLock);
        mEventQueue = std::make_unique<AidlMessageQueue<Event, SynchronizedReadWrite>>(
                in_eventQueueDescriptor, true);
        deleteEventFlagLocked();
        if (EventFlag::createEventFlag(mEventQueue->getEventFlagWord(), &mEventQueueFlag) !=
            ::android::OK) {
            result = ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        mWakeLockQueue = std::make_unique<AidlMessageQueue<int32_t, SynchronizedReadWrite>>(
                in_wakeLockDescriptor, true);
        if (!mCallback || !mEventQueue || !mWakeLockQueue || mEventQueueFlag == nullptr) {
            result = ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    mReadWakeLockQueueRun = true;
    mWakeLockThread = std::thread(&SlpiSensors::readWakeLockFMQ, this);
    return result;
}

ndk::ScopedAStatus SlpiSensors::injectSensorData(const Event&) {
    return ndk::ScopedAStatus::fromServiceSpecificError(
            static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
}

ndk::ScopedAStatus SlpiSensors::registerDirectChannel(const ISensors::SharedMemInfo&,
                                                      int32_t* _aidl_return) {
    *_aidl_return = EX_UNSUPPORTED_OPERATION;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus SlpiSensors::setOperationMode(ISensors::OperationMode in_mode) {
    mMode = in_mode;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SlpiSensors::unregisterDirectChannel(int32_t) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

void SlpiSensors::postEvents(const std::vector<Event>& events, bool wakeup) {
    std::lock_guard<std::mutex> lock(mWriteLock);
    if (mEventQueue == nullptr || events.empty()) return;
    if (mEventQueue->write(events.data(), events.size())) {
        if (mEventQueueFlag == nullptr) return;
        mEventQueueFlag->wake(
                static_cast<uint32_t>(BnSensors::EVENT_QUEUE_FLAG_BITS_READ_AND_PROCESS));
        if (wakeup) {
            updateWakeLock(static_cast<int32_t>(events.size()), 0);
        }
    }
}

void SlpiSensors::deleteEventFlagLocked() {
    if (mEventQueueFlag != nullptr) {
        (void)EventFlag::deleteEventFlag(&mEventQueueFlag);
        mEventQueueFlag = nullptr;
    }
}

void SlpiSensors::readWakeLockFMQ() {
    while (mReadWakeLockQueueRun.load()) {
        constexpr int64_t kReadTimeoutNs = 500LL * 1000LL * 1000LL;
        int32_t eventsHandled = 0;
        if (mWakeLockQueue) {
            mWakeLockQueue->readBlocking(
                    &eventsHandled, 1, 0,
                    static_cast<uint32_t>(WAKE_LOCK_QUEUE_FLAG_BITS_DATA_WRITTEN), kReadTimeoutNs);
        }
        updateWakeLock(0, eventsHandled);
    }
}

void SlpiSensors::updateWakeLock(int32_t eventsWritten, int32_t eventsHandled) {
    std::lock_guard<std::mutex> lock(mWakeLockLock);
    int32_t newVal = mOutstandingWakeUpEvents + eventsWritten - eventsHandled;
    mOutstandingWakeUpEvents = std::max(0, newVal);

    if (eventsWritten > 0) {
        mAutoReleaseWakeLockTime =
                static_cast<uint32_t>(::android::uptimeMillis()) +
                static_cast<uint32_t>(WAKE_LOCK_TIMEOUT_SECONDS) * 1000U;
    }

    if (!mHasWakeLock && mOutstandingWakeUpEvents > 0 &&
        acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLockName) == 0) {
        mHasWakeLock = true;
    } else if (mHasWakeLock) {
        if (static_cast<uint32_t>(::android::uptimeMillis()) > mAutoReleaseWakeLockTime) {
            mOutstandingWakeUpEvents = 0;
        }
        if (mOutstandingWakeUpEvents == 0 && release_wake_lock(kWakeLockName) == 0) {
            mHasWakeLock = false;
        }
    }
}

}  // namespace aidl::android::hardware::sensors

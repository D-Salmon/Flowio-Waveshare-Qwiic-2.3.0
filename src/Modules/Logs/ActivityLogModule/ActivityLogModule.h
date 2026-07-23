#pragma once
/**
 * @file ActivityLogModule.h
 * @brief PSRAM activity ring with bounded rotating SPIFFS persistence.
 */

#include "Core/Module.h"
#include "Core/Services/IActivityLog.h"
#include "Core/Services/ITime.h"
#include <freertos/queue.h>

class ActivityLogModule : public Module {
public:
    ModuleId moduleId() const override { return ModuleId::ActivityLog; }
    const char* taskName() const override { return "activitylog"; }
    uint8_t dependencyCount() const override { return 1; }
    ModuleId dependency(uint8_t i) const override {
        return i == 0 ? ModuleId::LogHub : ModuleId::Unknown;
    }
    uint8_t taskCount() const override { return 1; }
    const ModuleTaskSpec* taskSpecs() const override { return singleLoopTaskSpec(); }
    uint16_t taskStackSize() const override { return 4096; }

    void init(ConfigStore& cfg, ServiceRegistry& services) override;
    void loop() override;

private:
    static constexpr uint16_t kCapacity = 768;
    static constexpr uint16_t kFallbackCapacity = 128;
    static constexpr uint8_t kPersistQueueLen = 24;
    static constexpr size_t kFileMaxBytes = 96U * 1024U;
    static constexpr size_t kLineMax = 384U;
    static constexpr const char* kLogPath = "/activity.log";
    static constexpr const char* kRotatedLogPath = "/activity.1.log";

    static bool serviceEmit_(void* ctx, const ActivityEvent* event);
    static void serviceGetStats_(void* ctx, ActivityLogStats* out);
    static uint16_t serviceReadPage_(void* ctx,
                                     uint16_t offset,
                                     uint16_t limit,
                                     ActivityLogReplayWriter writer,
                                     void* writerCtx);
    static bool serviceClear_(void* ctx);

    bool emit_(const ActivityEvent& event);
    bool clear_();
    void appendRing_(const ActivityEvent& event);
    void replayFile_(const char* path);
    bool persist_(const ActivityEvent& event);
    uint32_t epochNow_();
    void emitBootEvent_();

    ActivityEvent* entries_ = nullptr;
    uint16_t capacity_ = 0;
    uint16_t head_ = 0;
    uint16_t count_ = 0;
    uint32_t droppedCount_ = 0;
    uint32_t persistedCount_ = 0;
    uint32_t persistDropCount_ = 0;
    uint32_t nextSeq_ = 1;
    bool inPsram_ = false;
    bool spiffsReady_ = false;
    bool bootEventPending_ = true;
    uint32_t bootEventSinceMs_ = 0;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    QueueHandle_t persistQueue_ = nullptr;
    ActivityLogService service_{};
    ServiceRegistry* services_ = nullptr;
    const TimeService* timeSvc_ = nullptr;
};

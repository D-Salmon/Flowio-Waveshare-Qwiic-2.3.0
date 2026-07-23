#pragma once
/**
 * @file IActivityLog.h
 * @brief User-facing persistent activity journal service.
 */

#include <stddef.h>
#include <stdint.h>

enum class ActivityDomain : uint8_t {
    System = 0,
    PoolLogic = 1,
    PoolDevice = 2,
};

enum class ActivitySource : uint8_t {
    System = 0,
    Auto = 1,
    Manual = 2,
    Scheduler = 3,
    Safety = 4,
    Boot = 5,
};

enum class ActivitySeverity : uint8_t {
    Info = 0,
    Success = 1,
    Warning = 2,
    Alarm = 3,
};

enum class ActivityCode : uint16_t {
    SystemBoot = 1,
    SystemConfigChanged = 2,
    FiltrationPlanCalculated = 100,
    DeviceStartRequested = 110,
    DeviceStopRequested = 111,
    DeviceStarted = 112,
    DeviceStopped = 113,
    InterlockTriggered = 120,
};

constexpr uint8_t ACTIVITY_TARGET_NONE = 0xFF;
constexpr size_t ACTIVITY_TITLE_MAX = 48;
constexpr size_t ACTIVITY_DETAIL_MAX = 128;
constexpr size_t ACTIVITY_ICON_MAX = 24;

struct ActivityEvent {
    uint32_t seq = 0;
    uint32_t tsMs = 0;
    uint32_t epochSec = 0;
    uint16_t code = 0;
    uint8_t domain = (uint8_t)ActivityDomain::System;
    uint8_t source = (uint8_t)ActivitySource::System;
    uint8_t severity = (uint8_t)ActivitySeverity::Info;
    uint8_t targetSlot = ACTIVITY_TARGET_NONE;
    bool state = false;
    char title[ACTIVITY_TITLE_MAX] = {0};
    char detail[ACTIVITY_DETAIL_MAX] = {0};
    char icon[ACTIVITY_ICON_MAX] = {0};
};

struct ActivityLogStats {
    uint16_t capacity = 0;
    uint16_t count = 0;
    uint32_t droppedCount = 0;
    uint32_t persistedCount = 0;
    uint32_t persistDropCount = 0;
    bool psram = false;
    bool spiffs = false;
};

using ActivityLogReplayWriter = bool (*)(void* writerCtx,
                                         const ActivityEvent& event,
                                         uint16_t index,
                                         uint16_t total);

struct ActivityLogService {
    bool (*emit)(void* ctx, const ActivityEvent* event);
    void (*getStats)(void* ctx, ActivityLogStats* out);
    uint16_t (*readPage)(void* ctx,
                         uint16_t offset,
                         uint16_t limit,
                         ActivityLogReplayWriter writer,
                         void* writerCtx);
    bool (*clear)(void* ctx);
    void* ctx;
};

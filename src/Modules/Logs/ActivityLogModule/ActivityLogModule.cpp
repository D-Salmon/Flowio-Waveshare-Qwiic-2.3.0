/**
 * @file ActivityLogModule.cpp
 * @brief Activity journal implementation.
 */

#include "ActivityLogModule.h"

#include "Core/FirmwareVersion.h"
#include "Core/LogModuleIds.h"
#include "Core/Services/Services.h"

#define LOG_MODULE_ID ((LogModuleId)LogModuleIdValue::ActivityLogModule)
#include "Core/ModuleLog.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <string.h>

namespace {
void copyText_(char* out, size_t outLen, const char* in)
{
    if (!out || outLen == 0U) return;
    snprintf(out, outLen, "%s", in ? in : "");
}

const char* resetReason_(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON: return "poweron";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT: return "brownout";
        default: return "unknown";
    }
}
}  // namespace

bool ActivityLogModule::serviceEmit_(void* ctx, const ActivityEvent* event)
{
    ActivityLogModule* self = static_cast<ActivityLogModule*>(ctx);
    return self && event && self->emit_(*event);
}

void ActivityLogModule::serviceGetStats_(void* ctx, ActivityLogStats* out)
{
    ActivityLogModule* self = static_cast<ActivityLogModule*>(ctx);
    if (!self || !out) return;
    portENTER_CRITICAL(&self->mux_);
    out->capacity = self->capacity_;
    out->count = self->count_;
    out->droppedCount = self->droppedCount_;
    out->persistedCount = self->persistedCount_;
    out->persistDropCount = self->persistDropCount_;
    out->psram = self->inPsram_;
    out->spiffs = self->spiffsReady_;
    portEXIT_CRITICAL(&self->mux_);
}

uint16_t ActivityLogModule::serviceReadPage_(void* ctx,
                                             uint16_t offset,
                                             uint16_t limit,
                                             ActivityLogReplayWriter writer,
                                             void* writerCtx)
{
    ActivityLogModule* self = static_cast<ActivityLogModule*>(ctx);
    if (!self || !writer || limit == 0U || !self->entries_) return 0U;

    uint16_t total = 0U;
    portENTER_CRITICAL(&self->mux_);
    total = self->count_;
    portEXIT_CRITICAL(&self->mux_);
    if (offset >= total) return 0U;

    const uint16_t available = (uint16_t)(total - offset);
    const uint16_t wanted = limit < available ? limit : available;
    uint16_t sent = 0U;
    for (uint16_t i = 0U; i < wanted; ++i) {
        ActivityEvent event{};
        portENTER_CRITICAL(&self->mux_);
        const uint16_t idx = (uint16_t)((self->head_ + offset + i) % self->capacity_);
        event = self->entries_[idx];
        portEXIT_CRITICAL(&self->mux_);
        if (!writer(writerCtx, event, (uint16_t)(offset + i), total)) break;
        ++sent;
    }
    return sent;
}

bool ActivityLogModule::serviceClear_(void* ctx)
{
    ActivityLogModule* self = static_cast<ActivityLogModule*>(ctx);
    return self && self->clear_();
}

void ActivityLogModule::init(ConfigStore&, ServiceRegistry& services)
{
    services_ = &services;
    entries_ = static_cast<ActivityEvent*>(
        heap_caps_calloc(kCapacity, sizeof(ActivityEvent), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (entries_) {
        capacity_ = kCapacity;
        inPsram_ = true;
    } else {
        entries_ = static_cast<ActivityEvent*>(
            heap_caps_calloc(kFallbackCapacity, sizeof(ActivityEvent), MALLOC_CAP_8BIT));
        if (entries_) capacity_ = kFallbackCapacity;
    }

    persistQueue_ = xQueueCreate(kPersistQueueLen, sizeof(ActivityEvent));
    service_ = {&ActivityLogModule::serviceEmit_,
                &ActivityLogModule::serviceGetStats_,
                &ActivityLogModule::serviceReadPage_,
                &ActivityLogModule::serviceClear_,
                this};
    if (!services.add(ServiceId::ActivityLog, &service_)) {
        LOGE("Activity log service registration failed");
    }

    spiffsReady_ = SPIFFS.begin(false);
    if (spiffsReady_) {
        replayFile_(kRotatedLogPath);
        replayFile_(kLogPath);
    }
    bootEventSinceMs_ = millis();
    LOGI("Activity log ready entries=%u psram=%u spiffs=%u",
         (unsigned)capacity_,
         inPsram_ ? 1U : 0U,
         spiffsReady_ ? 1U : 0U);
}

void ActivityLogModule::loop()
{
    if (bootEventPending_) {
        const bool timeReady = epochNow_() != 0U;
        const bool timedOut = (uint32_t)(millis() - bootEventSinceMs_) >= 30000U;
        if (timeReady || timedOut) {
            emitBootEvent_();
            bootEventPending_ = false;
        }
    }

    ActivityEvent event{};
    if (persistQueue_ && xQueueReceive(persistQueue_, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!persist_(event)) ++persistDropCount_;
    }
}

uint32_t ActivityLogModule::epochNow_()
{
    if (!timeSvc_ && services_) timeSvc_ = services_->get<TimeService>(ServiceId::Time);
    if (!timeSvc_ || !timeSvc_->isSynced || !timeSvc_->epoch ||
        !timeSvc_->isSynced(timeSvc_->ctx)) {
        return 0U;
    }
    const uint64_t epoch = timeSvc_->epoch(timeSvc_->ctx);
    return (epoch >= 1609459200ULL && epoch <= UINT32_MAX) ? (uint32_t)epoch : 0U;
}

bool ActivityLogModule::emit_(const ActivityEvent& input)
{
    if (!entries_ || capacity_ == 0U) return false;
    ActivityEvent event = input;
    if (event.seq == 0U) event.seq = nextSeq_++;
    if (event.tsMs == 0U) event.tsMs = millis();
    if (event.epochSec == 0U) event.epochSec = epochNow_();
    event.title[sizeof(event.title) - 1U] = '\0';
    event.detail[sizeof(event.detail) - 1U] = '\0';
    event.icon[sizeof(event.icon) - 1U] = '\0';
    if (event.icon[0] == '\0') copyText_(event.icon, sizeof(event.icon), "history");

    appendRing_(event);
    if (spiffsReady_ && persistQueue_ && xQueueSend(persistQueue_, &event, 0) != pdTRUE) {
        ++persistDropCount_;
    }
    return true;
}

void ActivityLogModule::appendRing_(const ActivityEvent& event)
{
    portENTER_CRITICAL(&mux_);
    uint16_t idx = 0U;
    if (count_ < capacity_) {
        idx = (uint16_t)((head_ + count_) % capacity_);
        ++count_;
    } else {
        idx = head_;
        head_ = (uint16_t)((head_ + 1U) % capacity_);
        ++droppedCount_;
    }
    entries_[idx] = event;
    if (event.seq >= nextSeq_) nextSeq_ = event.seq + 1U;
    portEXIT_CRITICAL(&mux_);
}

bool ActivityLogModule::clear_()
{
    if (persistQueue_) xQueueReset(persistQueue_);
    portENTER_CRITICAL(&mux_);
    head_ = 0U;
    count_ = 0U;
    droppedCount_ = 0U;
    persistedCount_ = 0U;
    persistDropCount_ = 0U;
    nextSeq_ = 1U;
    portEXIT_CRITICAL(&mux_);

    bool ok = true;
    if (spiffsReady_) {
        if (SPIFFS.exists(kLogPath) && !SPIFFS.remove(kLogPath)) ok = false;
        if (SPIFFS.exists(kRotatedLogPath) && !SPIFFS.remove(kRotatedLogPath)) ok = false;
    }
    return ok;
}

void ActivityLogModule::replayFile_(const char* path)
{
    if (!path || !SPIFFS.exists(path)) return;
    File file = SPIFFS.open(path, FILE_READ);
    if (!file) return;

    char line[kLineMax] = {0};
    while (file.available()) {
        const size_t n = file.readBytesUntil('\n', line, sizeof(line) - 1U);
        line[n] = '\0';
        StaticJsonDocument<384> doc;
        if (n == 0U || deserializeJson(doc, line) != DeserializationError::Ok) continue;
        ActivityEvent event{};
        event.seq = doc["seq"] | 0U;
        event.tsMs = doc["ts"] | 0U;
        event.epochSec = doc["epoch"] | 0U;
        event.code = doc["code"] | 0U;
        event.domain = doc["domain"] | 0U;
        event.source = doc["source"] | 0U;
        event.severity = doc["severity"] | 0U;
        event.targetSlot = doc["slot"] | ACTIVITY_TARGET_NONE;
        event.state = doc["state"] | false;
        copyText_(event.title, sizeof(event.title), doc["title"] | "");
        copyText_(event.detail, sizeof(event.detail), doc["detail"] | "");
        copyText_(event.icon, sizeof(event.icon), doc["icon"] | "history");
        if (event.seq != 0U && event.title[0] != '\0') appendRing_(event);
    }
    file.close();
}

bool ActivityLogModule::persist_(const ActivityEvent& event)
{
    StaticJsonDocument<384> doc;
    doc["seq"] = event.seq;
    doc["ts"] = event.tsMs;
    doc["epoch"] = event.epochSec;
    doc["code"] = event.code;
    doc["domain"] = event.domain;
    doc["source"] = event.source;
    doc["severity"] = event.severity;
    doc["slot"] = event.targetSlot;
    doc["state"] = event.state;
    doc["title"] = event.title;
    doc["detail"] = event.detail;
    doc["icon"] = event.icon;

    char line[kLineMax] = {0};
    const size_t len = serializeJson(doc, line, sizeof(line));
    if (len == 0U || len >= sizeof(line)) return false;

    File current = SPIFFS.open(kLogPath, FILE_READ);
    const size_t currentSize = current ? current.size() : 0U;
    if (current) current.close();
    if (currentSize + len + 1U > kFileMaxBytes) {
        if (SPIFFS.exists(kRotatedLogPath)) SPIFFS.remove(kRotatedLogPath);
        if (SPIFFS.exists(kLogPath)) SPIFFS.rename(kLogPath, kRotatedLogPath);
    }

    File file = SPIFFS.open(kLogPath, FILE_APPEND);
    if (!file) return false;
    const bool ok = file.print(line) == len && file.print('\n') == 1U;
    file.close();
    if (ok) ++persistedCount_;
    return ok;
}

void ActivityLogModule::emitBootEvent_()
{
    ActivityEvent event{};
    event.code = (uint16_t)ActivityCode::SystemBoot;
    event.domain = (uint8_t)ActivityDomain::System;
    event.source = (uint8_t)ActivitySource::Boot;
    event.severity = (uint8_t)ActivitySeverity::Info;
    copyText_(event.title, sizeof(event.title), "Flow.io a demarre");
    snprintf(event.detail,
             sizeof(event.detail),
             "Firmware %s, reset=%s",
             FirmwareVersion::Full,
             resetReason_(esp_reset_reason()));
    copyText_(event.icon, sizeof(event.icon), "power_settings_new");
    (void)emit_(event);
}

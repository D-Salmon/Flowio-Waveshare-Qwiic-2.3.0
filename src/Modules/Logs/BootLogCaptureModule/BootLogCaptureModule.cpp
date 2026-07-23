#include "BootLogCaptureModule.h"

#if FLOW_ENABLE_BOOT_LOG_CAPTURE

#include "Core/LogModuleIds.h"
#define LOG_MODULE_ID ((LogModuleId)LogModuleIdValue::BootLogCaptureModule)
#include "Core/ModuleLog.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace {
const BootLogCaptureService* gBootLogCaptureService = nullptr;
}

const BootLogCaptureService* bootLogCaptureService()
{
    return gBootLogCaptureService;
}

void markBootLogCaptureComplete()
{
    if (gBootLogCaptureService && gBootLogCaptureService->markComplete) {
        gBootLogCaptureService->markComplete(gBootLogCaptureService->ctx);
    }
}

void BootLogCaptureModule::init(ConfigStore&, ServiceRegistry& services)
{
    service_ = {&BootLogCaptureModule::serviceMarkComplete_,
                &BootLogCaptureModule::serviceGetStats_,
                &BootLogCaptureModule::serviceReadPage_,
                this};
    gBootLogCaptureService = &service_;

    if (!psramFound()) {
        LOGW("Boot log capture disabled: PSRAM not found");
        return;
    }

    const size_t bytes = (size_t)kCapacity * sizeof(LogEntry);
    entries_ = static_cast<LogEntry*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!entries_) {
        LOGW("Boot log capture disabled: PSRAM allocation failed");
        return;
    }
    capacity_ = kCapacity;
    capturing_ = true;

    const LogSinkRegistryService* sinks = services.get<LogSinkRegistryService>(ServiceId::LogSinks);
    const LogSinkService sink{&BootLogCaptureModule::sinkWrite_, this};
    if (!sinks || !sinks->add || !sinks->add(sinks->ctx, sink)) {
        capturing_ = false;
        LOGW("Boot log capture disabled: sink registry unavailable");
        return;
    }
    LOGI("Boot log capture enabled entries=%u", (unsigned)capacity_);
}

void BootLogCaptureModule::sinkWrite_(void* ctx, const LogEntry& entry)
{
    BootLogCaptureModule* self = static_cast<BootLogCaptureModule*>(ctx);
    if (self) self->write_(entry);
}

void BootLogCaptureModule::write_(const LogEntry& entry)
{
    if (!entries_ || capacity_ == 0U) return;
    portENTER_CRITICAL(&mux_);
    if (!capturing_) {
        portEXIT_CRITICAL(&mux_);
        return;
    }
    uint16_t idx = 0U;
    if (count_ < capacity_) {
        idx = (uint16_t)((head_ + count_) % capacity_);
        ++count_;
    } else {
        idx = head_;
        head_ = (uint16_t)((head_ + 1U) % capacity_);
        ++droppedCount_;
    }
    entries_[idx] = entry;
    portEXIT_CRITICAL(&mux_);
}

void BootLogCaptureModule::serviceMarkComplete_(void* ctx)
{
    BootLogCaptureModule* self = static_cast<BootLogCaptureModule*>(ctx);
    if (self) self->markComplete_();
}

void BootLogCaptureModule::markComplete_()
{
    portENTER_CRITICAL(&mux_);
    capturing_ = false;
    complete_ = true;
    portEXIT_CRITICAL(&mux_);
}

void BootLogCaptureModule::serviceGetStats_(void* ctx, BootLogCaptureStats* out)
{
    BootLogCaptureModule* self = static_cast<BootLogCaptureModule*>(ctx);
    if (!self || !out) return;
    portENTER_CRITICAL(&self->mux_);
    out->capacity = self->capacity_;
    out->count = self->count_;
    out->droppedCount = self->droppedCount_;
    out->capturing = self->capturing_;
    out->complete = self->complete_;
    out->psram = self->entries_ != nullptr;
    portEXIT_CRITICAL(&self->mux_);
}

uint16_t BootLogCaptureModule::serviceReadPage_(void* ctx,
                                                uint16_t offset,
                                                uint16_t limit,
                                                BootLogCaptureReplayWriter writer,
                                                void* writerCtx)
{
    BootLogCaptureModule* self = static_cast<BootLogCaptureModule*>(ctx);
    if (!self || !writer || !self->entries_ || limit == 0U) return 0U;
    uint16_t total = 0U;
    portENTER_CRITICAL(&self->mux_);
    total = self->count_;
    portEXIT_CRITICAL(&self->mux_);
    if (offset >= total) return 0U;

    const uint16_t available = (uint16_t)(total - offset);
    const uint16_t wanted = limit < available ? limit : available;
    uint16_t sent = 0U;
    for (uint16_t i = 0U; i < wanted; ++i) {
        LogEntry entry{};
        portENTER_CRITICAL(&self->mux_);
        const uint16_t idx = (uint16_t)((self->head_ + offset + i) % self->capacity_);
        entry = self->entries_[idx];
        portEXIT_CRITICAL(&self->mux_);
        if (!writer(writerCtx, entry, (uint16_t)(offset + i), total)) break;
        ++sent;
    }
    return sent;
}

#endif

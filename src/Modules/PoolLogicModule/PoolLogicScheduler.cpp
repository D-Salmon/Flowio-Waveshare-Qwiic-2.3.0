/**
 * @file PoolLogicScheduler.cpp
 * @brief Temperature-derived, next-cycle filtration planning.
 */

#include "PoolLogicModule.h"
#include "Modules/PoolLogicModule/FiltrationWindow.h"

#include <cmath>
#include <cstring>

#define LOG_MODULE_ID ((LogModuleId)LogModuleIdValue::PoolLogicModule)
#include "Core/ModuleLog.h"

namespace {
constexpr uint16_t kMinutesPerDay = 24U * 60U;
}

void PoolLogicModule::ensureDailySlot_()
{
    if (!schedSvc_ || !schedSvc_->setSlot) {
        LOGW("time.scheduler service unavailable");
        return;
    }

    // A continuous (24 h) plan has no stop edge. The 02:00 anchor therefore
    // provides a safe daily point to adopt a staged cooler-water plan and arm
    // a fresh five-minute measurement.
    TimeSchedulerSlot recalc{};
    recalc.slot = SLOT_DAILY_RECALC;
    recalc.eventId = POOLLOGIC_EVENT_DAILY_RECALC;
    recalc.enabled = true;
    recalc.hasEnd = false;
    recalc.replayStartOnBoot = false;
    recalc.mode = TimeSchedulerMode::RecurringClock;
    recalc.weekdayMask = TIME_WEEKDAY_ALL;
    recalc.startHour = 2;
    recalc.startMinute = 0;
    strncpy(recalc.label, "poollogic_plan_rollover", sizeof(recalc.label) - 1U);
    recalc.label[sizeof(recalc.label) - 1U] = '\0';

    if (!schedSvc_->setSlot(schedSvc_->ctx, &recalc)) {
        LOGW("Failed to set scheduler slot %u", (unsigned)SLOT_DAILY_RECALC);
    }
}

bool PoolLogicModule::computeFiltrationWindow_(float waterTemp,
                                               uint16_t& startMinuteOut,
                                               uint16_t& stopMinuteOut,
                                               uint16_t& durationMinuteOut)
{
    FiltrationWindowInput in{};
    in.waterTemp = waterTemp;

    FiltrationWindowOutput out{};
    if (!computeFiltrationWindowDeterministic(in, out)) return false;
    startMinuteOut = out.startMinuteOfDay;
    stopMinuteOut = out.stopMinuteOfDay;
    durationMinuteOut = out.durationMinutes;
    return true;
}

bool PoolLogicModule::applyFiltrationWindowSlot_(uint16_t startMinute,
                                                 uint16_t stopMinute,
                                                 uint16_t durationMinute)
{
    if (!schedSvc_ || !schedSvc_->setSlot) {
        LOGW("No time.scheduler service available");
        return false;
    }
    if (durationMinute < 120U || durationMinute > kMinutesPerDay ||
        startMinute >= kMinutesPerDay || stopMinute >= kMinutesPerDay) {
        LOGW("Invalid filtration plan start=%u stop=%u duration=%u",
             (unsigned)startMinute,
             (unsigned)stopMinute,
             (unsigned)durationMinute);
        return false;
    }

    const bool continuous = durationMinute == kMinutesPerDay;
    TimeSchedulerSlot window{};
    window.slot = SLOT_FILTR_WINDOW;
    window.eventId = POOLLOGIC_EVENT_FILTRATION_WINDOW;
    window.enabled = !continuous;
    window.hasEnd = true;
    window.replayStartOnBoot = true;
    window.mode = TimeSchedulerMode::RecurringClock;
    window.weekdayMask = TIME_WEEKDAY_ALL;
    window.startHour = (uint8_t)(startMinute / 60U);
    window.startMinute = (uint8_t)(startMinute % 60U);
    window.endHour = (uint8_t)(stopMinute / 60U);
    window.endMinute = (uint8_t)(stopMinute % 60U);
    strncpy(window.label, "poollogic_filtration", sizeof(window.label) - 1U);
    window.label[sizeof(window.label) - 1U] = '\0';

    if (!schedSvc_->setSlot(schedSvc_->ctx, &window)) {
        LOGW("Failed to set filtration window slot=%u", (unsigned)SLOT_FILTR_WINDOW);
        return false;
    }

    bool windowActive = continuous;
    if (!continuous && schedSvc_->isActive) {
        windowActive = schedSvc_->isActive(schedSvc_->ctx, SLOT_FILTR_WINDOW);
    }

    filtrationContinuous_ = continuous;
    filtrationActiveDurationMinute_ = durationMinute;
    portENTER_CRITICAL(&pendingMux_);
    filtrationWindowActive_ = windowActive;
    pendingFiltrationReconcile_ = true;
    portEXIT_CRITICAL(&pendingMux_);

    LOGI("Filtration plan active duration=%umin start=%02u:%02u stop=%02u:%02u continuous=%u",
         (unsigned)durationMinute,
         (unsigned)(startMinute / 60U),
         (unsigned)(startMinute % 60U),
         (unsigned)(stopMinute / 60U),
         (unsigned)(stopMinute % 60U),
         continuous ? 1U : 0U);
    return true;
}

bool PoolLogicModule::calculateAndStoreNextFiltrationPlan_(float waterTemp, bool applyImmediately)
{
    uint16_t startMinute = 0U;
    uint16_t stopMinute = 0U;
    uint16_t durationMinute = 0U;
    if (!computeFiltrationWindow_(waterTemp, startMinute, stopMinute, durationMinute)) {
        LOGW("Invalid water temperature value");
        return false;
    }

    bool stored = true;
    if (cfgStore_) {
        // Duration is the validity marker and is committed last so a power loss
        // cannot expose a partially written plan as valid.
        stored = cfgStore_->set(calcDurationVar_, (uint16_t)0U);
        stored = cfgStore_->set(calcStartVar_, startMinute) && stored;
        stored = cfgStore_->set(calcStopVar_, stopMinute) && stored;
        stored = cfgStore_->set(calcDurationVar_, durationMinute) && stored;
    } else {
        filtrationCalcStartMinute_ = startMinute;
        filtrationCalcStopMinute_ = stopMinute;
        filtrationCalcDurationMinute_ = durationMinute;
    }

    if (!stored) {
        LOGW("Failed to persist next filtration plan");
        return false;
    }

    if (cfgMqttPub_) {
        cfgMqttPub_->requestFullSync(MqttPublishPriority::Normal);
    }

    LOGI("Filtration plan calculated for next cycle water=%.2fC duration=%umin start=%02u:%02u stop=%02u:%02u",
         (double)waterTemp,
         (unsigned)durationMinute,
         (unsigned)(startMinute / 60U),
         (unsigned)(startMinute % 60U),
         (unsigned)(stopMinute / 60U),
         (unsigned)(stopMinute % 60U));

    char detail[128] = {0};
    snprintf(detail,
             sizeof(detail),
             "Eau %.2f C, %u min, %02u:%02u-%02u:%02u",
             (double)waterTemp,
             (unsigned)durationMinute,
             (unsigned)(startMinute / 60U),
             (unsigned)(startMinute % 60U),
             (unsigned)(stopMinute / 60U),
             (unsigned)(stopMinute % 60U));
    emitActivity_(ActivityCode::FiltrationPlanCalculated,
                  "Prochaine filtration calculee",
                  detail,
                  filtrationDeviceSlot_,
                  true,
                  ActivitySeverity::Success,
                  ActivitySource::Scheduler);

    return !applyImmediately ||
           applyFiltrationWindowSlot_(startMinute, stopMinute, durationMinute);
}

bool PoolLogicModule::applyStoredFiltrationPlan_()
{
    return applyFiltrationWindowSlot_(filtrationCalcStartMinute_,
                                      filtrationCalcStopMinute_,
                                      filtrationCalcDurationMinute_);
}

void PoolLogicModule::armFiltrationTemperatureSampling_()
{
    filtrationTempSamplingArmed_ = true;
    filtrationTempPlanStored_ = false;
    filtrationTempLastSampleMs_ = 0U;
    filtrationTempSampleSum_ = 0.0f;
    filtrationTempSampleCount_ = 0U;
}

void PoolLogicModule::updateFiltrationTemperatureSampling_(uint32_t nowMs,
                                                           bool haveWaterTemp,
                                                           float waterTemp)
{
    if (!filtrationTempSamplingArmed_ || !filtrationFsm_.on) return;

    const uint32_t runMs = nowMs - filtrationFsm_.stateSinceMs;
    if (runMs >= FILTRATION_TEMP_SAMPLE_START_MS &&
        runMs < FILTRATION_TEMP_SAMPLE_END_MS &&
        haveWaterTemp &&
        std::isfinite(waterTemp) &&
        (filtrationTempLastSampleMs_ == 0U ||
         (uint32_t)(nowMs - filtrationTempLastSampleMs_) >= FILTRATION_TEMP_SAMPLE_INTERVAL_MS)) {
        filtrationTempSampleSum_ += waterTemp;
        ++filtrationTempSampleCount_;
        filtrationTempLastSampleMs_ = nowMs;
    }

    if (runMs < FILTRATION_TEMP_SAMPLE_END_MS) return;

    filtrationTempSamplingArmed_ = false;
    if (filtrationTempSampleCount_ == 0U) {
        LOGW("Filtration temperature unavailable after five minutes; keeping previous plan");
        return;
    }

    const float average = filtrationTempSampleSum_ / (float)filtrationTempSampleCount_;
    filtrationTempPlanStored_ = calculateAndStoreNextFiltrationPlan_(average, false);
}

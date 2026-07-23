/**
 * @file FiltrationWindow.cpp
 * @brief Deterministic filtration window computation helper implementation.
 */

#include "Modules/PoolLogicModule/FiltrationWindow.h"
#include <math.h>

namespace {
constexpr uint16_t kMinutesPerDay = 24U * 60U;
constexpr uint16_t kWinterStartMinute = 23U * 60U;
constexpr uint16_t kSolarPivotMinute = 15U * 60U;

uint16_t normalizeMinute_(int32_t minute)
{
    int32_t normalized = minute % (int32_t)kMinutesPerDay;
    if (normalized < 0) normalized += kMinutesPerDay;
    return (uint16_t)normalized;
}

float durationHoursForTemperature_(float waterTemp)
{
    if (waterTemp < 12.0f) return 2.0f;
    if (waterTemp <= 25.0f) return waterTemp * 0.5f;
    if (waterTemp <= 28.0f) return waterTemp * (2.0f / 3.0f);

    // Continuous, kink-free transition through the values agreed for the hot
    // range: 28 C -> 18 h 40, 29 C -> 21 h, 30 C -> 24 h.
    if (waterTemp <= 29.0f) {
        constexpr float at28 = 28.0f * (2.0f / 3.0f);
        return at28 + ((waterTemp - 28.0f) * (21.0f - at28));
    }
    if (waterTemp < 30.0f) {
        return 21.0f + ((waterTemp - 29.0f) * 3.0f);
    }
    return 24.0f;
}
}  // namespace

bool computeFiltrationWindowDeterministic(const FiltrationWindowInput& in, FiltrationWindowOutput& out)
{
    if (!isfinite(in.waterTemp)) return false;

    const float durationHours = durationHoursForTemperature_(in.waterTemp);
    uint16_t durationMinutes = (uint16_t)lroundf(durationHours * 60.0f);
    if (durationMinutes < 120U) durationMinutes = 120U;
    if (durationMinutes > kMinutesPerDay) durationMinutes = kMinutesPerDay;

    out = {};
    out.durationMinutes = durationMinutes;
    if (durationMinutes >= kMinutesPerDay) {
        out.continuous = true;
        return true;
    }

    if (in.waterTemp < 12.0f) {
        out.startMinuteOfDay = kWinterStartMinute;
        out.stopMinuteOfDay = 60U;
        return true;
    }

    const int32_t start = (int32_t)kSolarPivotMinute - (int32_t)(durationMinutes / 2U);
    out.startMinuteOfDay = normalizeMinute_(start);
    out.stopMinuteOfDay = normalizeMinute_((int32_t)out.startMinuteOfDay + durationMinutes);
    return true;
}

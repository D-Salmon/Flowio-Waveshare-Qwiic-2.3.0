#pragma once
/**
 * @file FiltrationWindow.h
 * @brief Deterministic filtration window computation helper.
 */

#include <stdint.h>

struct FiltrationWindowInput {
    float waterTemp = 0.0f;
};

struct FiltrationWindowOutput {
    uint16_t startMinuteOfDay = 0;
    uint16_t stopMinuteOfDay = 0;
    uint16_t durationMinutes = 0;
    bool continuous = false;
};

bool computeFiltrationWindowDeterministic(const FiltrationWindowInput& in, FiltrationWindowOutput& out);

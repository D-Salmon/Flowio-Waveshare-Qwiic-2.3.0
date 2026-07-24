#include <unity.h>
#include <math.h>

#include "Modules/PoolLogicModule/FiltrationWindow.h"

void test_temp_below_low_uses_min_duration()
{
    FiltrationWindowInput in{};
    in.waterTemp = 5.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(120, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(23 * 60, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(60, out.stopMinuteOfDay);
    TEST_ASSERT_FALSE(out.continuous);
}

void test_24c_uses_low_temperature_linear_ramp()
{
    FiltrationWindowInput in{};
    in.waterTemp = 24.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(11 * 60 + 42, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(9 * 60 + 9, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(20 * 60 + 51, out.stopMinuteOfDay);
}

void test_26c_uses_high_temperature_linear_ramp()
{
    FiltrationWindowInput in{};
    in.waterTemp = 26.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(14 * 60 + 48, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(7 * 60 + 36, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(22 * 60 + 24, out.stopMinuteOfDay);
}

void test_27c_uses_high_temperature_linear_ramp()
{
    FiltrationWindowInput in{};
    in.waterTemp = 27.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(17 * 60 + 6, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(6 * 60 + 27, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(23 * 60 + 33, out.stopMinuteOfDay);
}

void test_29c_uses_high_temperature_linear_ramp_and_wraps_midnight()
{
    FiltrationWindowInput in{};
    in.waterTemp = 29.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(21 * 60 + 42, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(4 * 60 + 9, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(1 * 60 + 51, out.stopMinuteOfDay);
}

void test_32c_is_capped_at_continuous_operation()
{
    FiltrationWindowInput in{};
    in.waterTemp = 32.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(24 * 60, out.durationMinutes);
    TEST_ASSERT_TRUE(out.continuous);
}

void test_nan_temperature_returns_false()
{
    FiltrationWindowInput in{};
    in.waterTemp = NAN;

    FiltrationWindowOutput out{};
    TEST_ASSERT_FALSE(computeFiltrationWindowDeterministic(in, out));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_temp_below_low_uses_min_duration);
    RUN_TEST(test_24c_uses_low_temperature_linear_ramp);
    RUN_TEST(test_26c_uses_high_temperature_linear_ramp);
    RUN_TEST(test_27c_uses_high_temperature_linear_ramp);
    RUN_TEST(test_29c_uses_high_temperature_linear_ramp_and_wraps_midnight);
    RUN_TEST(test_32c_is_capped_at_continuous_operation);
    RUN_TEST(test_nan_temperature_returns_false);
    return UNITY_END();
}

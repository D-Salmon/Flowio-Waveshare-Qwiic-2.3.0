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

void test_24c_runs_twelve_hours_centered_on_15h()
{
    FiltrationWindowInput in{};
    in.waterTemp = 24.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(12 * 60, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(9 * 60, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(21 * 60, out.stopMinuteOfDay);
}

void test_27c_uses_two_thirds_rule()
{
    FiltrationWindowInput in{};
    in.waterTemp = 27.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(18 * 60, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(6 * 60, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(0, out.stopMinuteOfDay);
}

void test_29c_uses_twenty_one_hours_and_wraps_midnight()
{
    FiltrationWindowInput in{};
    in.waterTemp = 29.0f;

    FiltrationWindowOutput out{};
    TEST_ASSERT_TRUE(computeFiltrationWindowDeterministic(in, out));
    TEST_ASSERT_EQUAL_UINT16(21 * 60, out.durationMinutes);
    TEST_ASSERT_EQUAL_UINT16(4 * 60 + 30, out.startMinuteOfDay);
    TEST_ASSERT_EQUAL_UINT16(1 * 60 + 30, out.stopMinuteOfDay);
}

void test_30c_is_continuous()
{
    FiltrationWindowInput in{};
    in.waterTemp = 30.0f;

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
    RUN_TEST(test_24c_runs_twelve_hours_centered_on_15h);
    RUN_TEST(test_27c_uses_two_thirds_rule);
    RUN_TEST(test_29c_uses_twenty_one_hours_and_wraps_midnight);
    RUN_TEST(test_30c_is_continuous);
    RUN_TEST(test_nan_temperature_returns_false);
    return UNITY_END();
}

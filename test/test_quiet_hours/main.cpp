#include <unity.h>
#include "logic/quiet_hours.h"

void setUp() {}
void tearDown() {}

static const int H = 3600;

void test_window_simple() {
    TEST_ASSERT_TRUE(inQuietWindow(2 * H, 1, 5));
    TEST_ASSERT_FALSE(inQuietWindow(6 * H, 1, 5));
    TEST_ASSERT_TRUE(inQuietWindow(1 * H, 1, 5));      // start inclusive
    TEST_ASSERT_FALSE(inQuietWindow(5 * H, 1, 5));     // end exclusive
}

void test_window_wraps_midnight() {
    TEST_ASSERT_TRUE(inQuietWindow(23 * H + 1800, 23, 7));
    TEST_ASSERT_TRUE(inQuietWindow(3 * H, 23, 7));
    TEST_ASSERT_FALSE(inQuietWindow(12 * H, 23, 7));
}

void test_equal_hours_is_no_window() {
    TEST_ASSERT_FALSE(inQuietWindow(5 * H, 5, 5));
}

void test_sleep_untouched_outside() {
    // 12:00 + 1h = 13:00, outside 23-7.
    TEST_ASSERT_EQUAL_UINT32(3600, quietAdjustedSleep(12 * H, 3600, true, 23, 7));
}

void test_sleep_untouched_when_disabled() {
    TEST_ASSERT_EQUAL_UINT32(3600, quietAdjustedSleep(23 * H, 3600, false, 23, 7));
}

void test_sleep_extends_to_window_end_wrapped() {
    // 22:30 + 1h = 23:30 inside 23-7 -> sleep until 07:00 = 8.5h.
    TEST_ASSERT_EQUAL_UINT32(30600, quietAdjustedSleep(22 * H + 1800, 3600, true, 23, 7));
}

void test_sleep_extends_simple_window() {
    // 00:30 + 1h = 01:30 inside 1-5 -> until 05:00 = 4.5h.
    TEST_ASSERT_EQUAL_UINT32(16200, quietAdjustedSleep(1800, 3600, true, 1, 5));
}

void test_wake_exactly_at_end_passes() {
    // 04:00 + 1h = 05:00 = window end (exclusive) -> untouched.
    TEST_ASSERT_EQUAL_UINT32(3600, quietAdjustedSleep(4 * H, 3600, true, 1, 5));
}

void test_seconds_until_end() {
    TEST_ASSERT_EQUAL_UINT32(0, secondsUntilQuietEnd(12 * H, 23, 7));
    TEST_ASSERT_EQUAL_UINT32(4 * H, secondsUntilQuietEnd(3 * H, 23, 7));
    TEST_ASSERT_EQUAL_UINT32(8 * H, secondsUntilQuietEnd(23 * H, 23, 7));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_window_simple);
    RUN_TEST(test_window_wraps_midnight);
    RUN_TEST(test_equal_hours_is_no_window);
    RUN_TEST(test_sleep_untouched_outside);
    RUN_TEST(test_sleep_untouched_when_disabled);
    RUN_TEST(test_sleep_extends_to_window_end_wrapped);
    RUN_TEST(test_sleep_extends_simple_window);
    RUN_TEST(test_wake_exactly_at_end_passes);
    RUN_TEST(test_seconds_until_end);
    return UNITY_END();
}

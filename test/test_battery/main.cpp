#include <unity.h>
#include "logic/battery_curve.h"

void setUp() {}
void tearDown() {}

void test_full_at_4200() { TEST_ASSERT_EQUAL(100, batteryPercentFromMv(4200)); }
void test_clamped_above() { TEST_ASSERT_EQUAL(100, batteryPercentFromMv(4350)); }
void test_empty_at_3000() { TEST_ASSERT_EQUAL(0, batteryPercentFromMv(3000)); }
void test_clamped_below() { TEST_ASSERT_EQUAL(0, batteryPercentFromMv(2500)); }
void test_interpolates_midcurve() { TEST_ASSERT_EQUAL(38, batteryPercentFromMv(3650)); }
void test_interpolates_top() { TEST_ASSERT_EQUAL(97, batteryPercentFromMv(4150)); }
void test_interpolates_bottom() { TEST_ASSERT_EQUAL(2, batteryPercentFromMv(3250)); }

void test_level_high_at_full() { TEST_ASSERT_TRUE(batteryLevelBucket(100) == BatteryLevel::High); }
void test_level_high_boundary() { TEST_ASSERT_TRUE(batteryLevelBucket(41) == BatteryLevel::High); }
void test_level_medium_boundary_high() { TEST_ASSERT_TRUE(batteryLevelBucket(40) == BatteryLevel::Medium); }
void test_level_medium_boundary_low() { TEST_ASSERT_TRUE(batteryLevelBucket(16) == BatteryLevel::Medium); }
void test_level_low_boundary() { TEST_ASSERT_TRUE(batteryLevelBucket(15) == BatteryLevel::Low); }
void test_level_low_at_zero() { TEST_ASSERT_TRUE(batteryLevelBucket(0) == BatteryLevel::Low); }

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_at_4200);
    RUN_TEST(test_clamped_above);
    RUN_TEST(test_empty_at_3000);
    RUN_TEST(test_clamped_below);
    RUN_TEST(test_interpolates_midcurve);
    RUN_TEST(test_interpolates_top);
    RUN_TEST(test_interpolates_bottom);
    RUN_TEST(test_level_high_at_full);
    RUN_TEST(test_level_high_boundary);
    RUN_TEST(test_level_medium_boundary_high);
    RUN_TEST(test_level_medium_boundary_low);
    RUN_TEST(test_level_low_boundary);
    RUN_TEST(test_level_low_at_zero);
    return UNITY_END();
}

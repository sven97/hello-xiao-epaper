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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_at_4200);
    RUN_TEST(test_clamped_above);
    RUN_TEST(test_empty_at_3000);
    RUN_TEST(test_clamped_below);
    RUN_TEST(test_interpolates_midcurve);
    RUN_TEST(test_interpolates_top);
    RUN_TEST(test_interpolates_bottom);
    return UNITY_END();
}

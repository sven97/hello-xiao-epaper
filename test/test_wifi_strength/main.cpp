#include <unity.h>
#include "logic/wifi_strength.h"

void setUp() {}
void tearDown() {}

void test_strong_at_good_signal() { TEST_ASSERT_TRUE(wifiStrengthBucket(-50) == WifiStrength::Strong); }
void test_strong_boundary() { TEST_ASSERT_TRUE(wifiStrengthBucket(-60) == WifiStrength::Strong); }
void test_fair_just_below_strong() { TEST_ASSERT_TRUE(wifiStrengthBucket(-61) == WifiStrength::Fair); }
void test_fair_boundary() { TEST_ASSERT_TRUE(wifiStrengthBucket(-75) == WifiStrength::Fair); }
void test_weak_below_fair() { TEST_ASSERT_TRUE(wifiStrengthBucket(-76) == WifiStrength::Weak); }
void test_weak_very_low() { TEST_ASSERT_TRUE(wifiStrengthBucket(-95) == WifiStrength::Weak); }

void test_label_strings() {
    TEST_ASSERT_EQUAL_STRING("strong", wifiStrengthLabel(WifiStrength::Strong));
    TEST_ASSERT_EQUAL_STRING("fair", wifiStrengthLabel(WifiStrength::Fair));
    TEST_ASSERT_EQUAL_STRING("weak", wifiStrengthLabel(WifiStrength::Weak));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_strong_at_good_signal);
    RUN_TEST(test_strong_boundary);
    RUN_TEST(test_fair_just_below_strong);
    RUN_TEST(test_fair_boundary);
    RUN_TEST(test_weak_below_fair);
    RUN_TEST(test_weak_very_low);
    RUN_TEST(test_label_strings);
    return UNITY_END();
}

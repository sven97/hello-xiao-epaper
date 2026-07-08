#include <Arduino.h>
#include <TFT_eSPI.h>

EPaper epaper;

const uint16_t COLORS[6] = {TFT_BLACK, TFT_WHITE, TFT_RED,
                            TFT_YELLOW, TFT_GREEN, TFT_BLUE};
const char *COLOR_NAMES[6] = {"BLACK", "WHITE", "RED",
                              "YELLOW", "GREEN", "BLUE"};

// EE02 user buttons: active-LOW, external pull-ups on board
constexpr uint8_t BTN_PREV = 2;
constexpr uint8_t BTN_REFRESH = 3;
constexpr uint8_t BTN_NEXT = 5;
constexpr uint8_t LED_PIN = 21; // active-LOW

constexpr int SCREEN_COUNT = 3;
int screenIndex = 0;

void drawColorBars() {
    epaper.fillScreen(TFT_WHITE);
    for (int i = 0; i < 6; i++)
        epaper.fillRect(i * 200, 0, 200, 1200, COLORS[i]);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    for (int i = 0; i < 6; i++)
        epaper.drawString(COLOR_NAMES[i], i * 200 + 20, 1250, 4);
    epaper.drawString("screen 1/3: color bars", 20, 1400, 4);
}

void drawInfoScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("screen 2/3: info", 20, 40, 4);
    epaper.setTextColor(TFT_RED, TFT_WHITE);
    epaper.drawString("XIAO ePaper Display Board EE02", 20, 140, 4);
    epaper.setTextColor(TFT_BLUE, TFT_WHITE);
    epaper.drawString("13.3\" Spectra 6, 1200 x 1600", 20, 220, 4);
    epaper.setTextColor(TFT_GREEN, TFT_WHITE);
    epaper.drawString("buttons: GPIO2 prev / GPIO3 refresh / GPIO5 next",
                      20, 300, 4);
    epaper.setTextColor(TFT_BLACK, TFT_YELLOW);
    epaper.drawString("uptime (ms): " + String(millis()), 20, 380, 4);
}

void drawCheckerboard() {
    epaper.fillScreen(TFT_WHITE);
    for (int y = 0; y < 1600; y += 100)
        for (int x = 0; x < 1200; x += 100)
            epaper.fillRect(x, y, 100, 100,
                            COLORS[((x + y) / 100) % 6]);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("screen 3/3: checkerboard", 20, 20, 4);
}

void showScreen(int idx) {
    digitalWrite(LED_PIN, LOW); // LED on = busy
    Serial.printf("drawing screen %d (update takes ~20-30 s)...\n", idx + 1);
    switch (idx) {
        case 0: drawColorBars(); break;
        case 1: drawInfoScreen(); break;
        case 2: drawCheckerboard(); break;
    }
    epaper.update();
    digitalWrite(LED_PIN, HIGH); // LED off = ready
    Serial.println("ready — press a button");
}

// Returns true once per physical press (falling edge + debounce)
bool pressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        delay(30); // debounce
        if (digitalRead(pin) == LOW) {
            while (digitalRead(pin) == LOW) delay(10); // wait for release
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("ee02-playground: task 3 — button demo");

    pinMode(BTN_PREV, INPUT);    // external pull-ups on board
    pinMode(BTN_REFRESH, INPUT);
    pinMode(BTN_NEXT, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    epaper.begin();
    showScreen(screenIndex);
}

void loop() {
    if (pressed(BTN_NEXT)) {
        screenIndex = (screenIndex + 1) % SCREEN_COUNT;
        showScreen(screenIndex);
    } else if (pressed(BTN_PREV)) {
        screenIndex = (screenIndex + SCREEN_COUNT - 1) % SCREEN_COUNT;
        showScreen(screenIndex);
    } else if (pressed(BTN_REFRESH)) {
        showScreen(screenIndex);
    }
    delay(10);
}

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <JPEGDecoder.h>

EPaper epaper;

constexpr uint8_t BTN_PREV = 2;
constexpr uint8_t BTN_REFRESH = 3;
constexpr uint8_t BTN_NEXT = 5;
constexpr uint8_t LED_PIN = 21; // active-LOW

const char *AP_NAME = "EE02-Setup";
// picsum serves progressive JPEGs, which embedded decoders can't parse;
// images.weserv.nl re-encodes to baseline. The random= value defeats
// weserv's cache so every fetch is a different picture.
String imageUrl() {
    return "https://images.weserv.nl/?url=picsum.photos/1200/1600"
           "%3Frandom%3D" + String(esp_random()) + "&output=jpg";
}

void showError(const String &msg) {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_RED, TFT_WHITE);
    epaper.drawString("ERROR", 20, 40, 4);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString(msg, 20, 120, 4);
    epaper.update();
}

void showProvisioningScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("Wi-Fi setup", 20, 40, 4);
    epaper.drawString("1. On your phone, join the Wi-Fi network:", 20, 160, 4);
    epaper.setTextColor(TFT_BLUE, TFT_WHITE);
    epaper.drawString(AP_NAME, 60, 220, 4);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("2. Open http://192.168.4.1 in a browser", 20, 300, 4);
    epaper.drawString("3. Pick your 2.4 GHz network, enter its password", 20, 360, 4);
    epaper.drawString("The board remembers it for future boots.", 20, 480, 4);
    epaper.drawString("Hold the refresh button at power-on to forget.", 20, 540, 4);
    epaper.update();
}

void configModeCallback(WiFiManager *wm) {
    Serial.printf("config portal up: join \"%s\", then open http://%s\n",
                  AP_NAME, WiFi.softAPIP().toString().c_str());
    Serial.println("drawing provisioning instructions (takes ~20-30 s)...");
    showProvisioningScreen();
    Serial.println("instructions on panel");
}

// Connect with saved credentials, or open the captive portal on first
// boot / after forget. Blocks until connected or portal timeout.
bool connectWifi() {
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(300); // give up after 5 min, don't hang forever
    Serial.println("connecting (saved credentials, or captive portal)...");
    bool ok = wm.autoConnect(AP_NAME);
    if (ok) {
        Serial.printf("connected to %s, IP %s, RSSI %d dBm\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
    } else {
        Serial.println("provisioning timed out");
    }
    return ok;
}

// Decode the JPEG in buf and push it MCU block by MCU block.
// Image is exactly 1200x1600 so blocks tile without clipping.
void renderJpeg(uint8_t *buf, size_t len) {
    JpegDec.decodeArray(buf, len);
    Serial.printf("jpeg: %d x %d, MCU %d x %d\n", JpegDec.width,
                  JpegDec.height, JpegDec.MCUWidth, JpegDec.MCUHeight);
    while (JpegDec.read()) {
        int x = JpegDec.MCUx * JpegDec.MCUWidth;
        int y = JpegDec.MCUy * JpegDec.MCUHeight;
        epaper.pushImage(x, y, JpegDec.MCUWidth, JpegDec.MCUHeight,
                         JpegDec.pImage);
    }
}

bool fetchAndShowImage() {
    digitalWrite(LED_PIN, LOW);
    WiFiClientSecure client;
    client.setInsecure(); // learning repo: skip cert validation
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String url = imageUrl();
    http.begin(client, url);
    http.setTimeout(20000);
    Serial.printf("GET %s\n", url.c_str());
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", code);
        http.end();
        showError("HTTP " + String(code) + " from image server");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    int len = http.getSize();
    Serial.printf("content-length: %d\n", len);
    if (len <= 0) {
        http.end();
        showError("server sent no Content-Length");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    uint8_t *buf = (uint8_t *)ps_malloc(len); // PSRAM
    if (!buf) {
        http.end();
        showError("PSRAM alloc failed");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    size_t got = 0;
    uint32_t lastData = millis();
    while (got < (size_t)len && millis() - lastData < 20000) {
        size_t avail = stream->available();
        if (avail) {
            got += stream->readBytes(buf + got, min(avail, (size_t)len - got));
            lastData = millis();
        } else {
            delay(10);
        }
    }
    http.end();
    Serial.printf("received %u / %d bytes\n", (unsigned)got, len);
    if (got < (size_t)len) {
        free(buf);
        showError("download incomplete");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }

    epaper.fillScreen(TFT_WHITE);
    renderJpeg(buf, got);
    free(buf);
    Serial.println("updating panel (takes ~20-30 s)...");
    epaper.update();
    Serial.println("done — press refresh (GPIO3) for a new image");
    digitalWrite(LED_PIN, HIGH);
    return true;
}

bool pressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        delay(30);
        if (digitalRead(pin) == LOW) {
            while (digitalRead(pin) == LOW) delay(10);
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("ee02-playground: task 4 — wifi provisioning + picsum fetch");

    pinMode(BTN_PREV, INPUT);    // external pull-ups on board
    pinMode(BTN_REFRESH, INPUT);
    pinMode(BTN_NEXT, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    epaper.begin();

    if (digitalRead(BTN_REFRESH) == LOW) {
        Serial.println("refresh held at boot — forgetting saved wifi");
        WiFiManager wm;
        wm.resetSettings();
    }

    if (!connectWifi()) {
        showError("wifi setup failed or timed out");
        return;
    }
    fetchAndShowImage();
}

void loop() {
    if (pressed(BTN_REFRESH)) {
        if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
            showError("wifi reconnect failed");
            return;
        }
        fetchAndShowImage();
    }
    delay(10);
}

#include <WiFi.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

// --- Pins ---
constexpr int PIN_WS = 15;
constexpr int PIN_SCK = 33;
constexpr int PIN_SD_MIC = 32;
constexpr int PIN_FLEX = 36;
constexpr int PIN_SD_CS = 5;
constexpr int PIN_AMP_DIN = 22;
constexpr int PIN_AMP_BCLK = 26;
constexpr int PIN_AMP_LRC = 25;
constexpr int PIN_AMP_SD = 27;
constexpr int PIN_CONTINUITY = 39;
constexpr int PIN_BAT_ADC = 34;

// --- Network ---
const char* WIFI_SSID = "601";
const char* WIFI_PASS = "13752366658";
const char* SERVER_HOST = "frp-bus.com";
constexpr uint16_t SERVER_PORT = 47169;

Audio audio;
Preferences prefs;
WiFiClient tcpClient;

uint8_t mood = 50;
bool serverMode = true;
bool flexPrevHigh = false;
unsigned long lastStatusMs = 0;
unsigned long lastMoodDecayMs = 0;
unsigned long laughCooldownUntil = 0;
unsigned long luCooldownUntil = 0;
unsigned long nextSoundCaptureMs = 0;

static uint8_t clampU8(int v) { return (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v)); }

void persistMood() {
  prefs.putUChar("mood", mood);
}

uint8_t readBatteryPercent() {
  int raw = analogRead(PIN_BAT_ADC);
  float adcV = (raw / 4095.0f) * 3.3f;
  float battV = adcV * 2.0f;  // 100k/100k divider
  int pct = (int)(((battV - 3.2f) / (4.2f - 3.2f)) * 100.0f);
  return clampU8(pct);
}

void playWav(const char* path) {
  if (!SD.exists(path)) return;
  audio.connecttoFS(SD, path);
  while (audio.isRunning()) {
    audio.loop();
    delay(2);
  }
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long beginMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - beginMs < 30000UL) delay(300);
  return WiFi.status() == WL_CONNECTED;
}

bool connectServer() {
  if (WiFi.status() != WL_CONNECTED) return false;
  unsigned long beginMs = millis();
  while (millis() - beginMs < 60000UL) {
    if (tcpClient.connect(SERVER_HOST, SERVER_PORT)) return true;
    delay(1500);
  }
  return false;
}

void sendJson(const String& payload) {
  if (serverMode && tcpClient.connected()) tcpClient.println(payload);
}

void sendStatus(const char* reason = "tick") {
  uint8_t satiety = readBatteryPercent();
  String msg = "{\"type\":\"status\",\"reason\":\"" + String(reason) +
               "\",\"satiety\":" + String(satiety) +
               ",\"mood\":" + String(mood) + "}";
  sendJson(msg);
}

void uploadAudioForAI() {
  // Lightweight trigger: loud sound wake-up, keyword filtering is done on server STT result.
  sendJson("{\"type\":\"audio_trigger\",\"seconds\":10}");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FLEX, INPUT);
  pinMode(PIN_CONTINUITY, INPUT);
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, HIGH);

  analogReadResolution(12);
  prefs.begin("pet", false);
  mood = prefs.getUChar("mood", 50);

  SPI.begin(18, 19, 23, PIN_SD_CS);
  SD.begin(PIN_SD_CS);
  audio.setPinout(PIN_AMP_BCLK, PIN_AMP_LRC, PIN_AMP_DIN);
  audio.setVolume(12);
  playWav("/start.wav");

  bool wifiOk = connectWifi();
  bool serverOk = wifiOk && connectServer();
  serverMode = serverOk;
  if (!serverMode) playWav("/error.wav");
  if (serverMode) sendJson("{\"type\":\"heartbeat\"}");

  lastStatusMs = millis();
  lastMoodDecayMs = millis();
}

void loop() {
  audio.loop();

  const unsigned long now = millis();

  bool flexHigh = digitalRead(PIN_FLEX) == HIGH;
  if (flexHigh && !flexPrevHigh) {
    mood = clampU8(mood + 1);
    persistMood();
    sendStatus("flex");
    if (now > laughCooldownUntil) {
      playWav("/laugh.wav");
      laughCooldownUntil = now + 5000UL;
    }
  }
  flexPrevHigh = flexHigh;

  if (now - lastMoodDecayMs > 3600000UL) {
    mood = clampU8(mood - 1);
    persistMood();
    sendStatus("hour_decay");
    lastMoodDecayMs = now;
  }

  bool continuityLow = analogRead(PIN_CONTINUITY) < 200;
  if (continuityLow) {
    if (now > luCooldownUntil) {
      mood = 100;
      persistMood();
      sendJson("{\"type\":\"event\",\"name\":\"gpio39_short\"}");
      luCooldownUntil = now + 86400000UL;
    }
    playWav("/lu.wav");
  }

  if (serverMode && (!tcpClient.connected())) serverMode = false;

  if (serverMode && now - lastStatusMs > 3000UL) {
    sendStatus();
    lastStatusMs = now;
  }

  if (now > nextSoundCaptureMs) {
    uploadAudioForAI();
    nextSoundCaptureMs = now + 15000UL;
  }

  delay(20);
}

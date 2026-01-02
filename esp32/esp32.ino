#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/* ================= SUPABASE ================= */
#define SUPABASE_URL "https://pdoleujjkhxczcqwuiah.supabase.co/rest/v1/weather_data"
#define SUPABASE_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBkb2xldWpqa2h4Y3pjcXd1aWFoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjczNTA2MjMsImV4cCI6MjA4MjkyNjYyM30.L1y3B2PArmKL_wT5iozfdeKNbzsC0aKO1RJ9Dya7-O0"

/* ================= SERIAL ================= */
#define RX2 16
#define TX2 17

/* ================= RESET + BUZZER ================= */
#define WIFI_RESET_PIN 27
#define BUZZER_PIN 26
#define HOLD_TIME 3000   // 3 sec

unsigned long pressStart = 0;
bool resetTriggered = false;

/* ================= BEEP ================= */
void successBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX2, TX2);

  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFiManager wm;
  if (!wm.autoConnect("WeatherStation-Setup")) {
    ESP.restart();
  }

  Serial.println("✅ WiFi Connected");
  Serial.println(WiFi.localIP());
}

/* ================= LOOP ================= */
void loop() {

  /* ===== BUTTON HOLD RESET (ANYTIME) ===== */
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    if (pressStart == 0) pressStart = millis();

    if (!resetTriggered && millis() - pressStart >= HOLD_TIME) {
      resetTriggered = true;
      Serial.println("🔁 WiFi Reset Triggered");

      successBeep();

      WiFiManager wm;
      wm.resetSettings();
      delay(800);
      ESP.restart();
    }
  } else {
    pressStart = 0;
    resetTriggered = false;
  }

  /* ===== SERIAL DATA HANDLING ===== */
  static String buffer = "";

  while (Serial2.available()) {
    char c = Serial2.read();

    if (c == '\n') {
      buffer.trim();
      if (buffer.length() < 10) {
        buffer = "";
        return;
      }

      // Split CSV (8 values)
      String parts[8];
      int start = 0;
      for (int i = 0; i < 8; i++) {
        int idx = buffer.indexOf(',', start);
        if (idx == -1) {
          parts[i] = buffer.substring(start);
          break;
        }
        parts[i] = buffer.substring(start, idx);
        start = idx + 1;
      }

      float dhtTemp  = parts[0].toFloat();
      float dhtHum   = parts[1].toFloat();
      float bmpTemp  = parts[2].toFloat();
      float pressure = parts[3].toFloat();
      float altitude = parts[4].toFloat();
      String rainTxt = parts[5];      // TEXT
      int rainRaw    = parts[6].toInt(); // 0 / 1
      int aqi        = parts[7].toInt();

      StaticJsonDocument<256> doc;
      doc["dhttemp"]     = dhtTemp;
      doc["dhthumidity"] = dhtHum;
      doc["bmptemp"]     = bmpTemp;
      doc["pressure"]    = pressure;
      doc["altitude"]    = altitude;
      doc["rainstatus"]  = rainTxt;
      doc["rainraw"]     = rainRaw;
      doc["aqi"]         = aqi;

      String payload;
      serializeJson(doc, payload);

      HTTPClient http;
      http.begin(SUPABASE_URL);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("apikey", SUPABASE_KEY);
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      http.addHeader("Prefer", "return=minimal");

      int code = http.POST(payload);

      if (code == 201 || code == 204) {
        Serial.println("✅ Data sent to Supabase");
      } else {
        Serial.print("❌ Error: ");
        Serial.println(code);
        Serial.println(http.getString());
      }

      http.end();
      buffer = "";
    } else {
      buffer += c;
    }
  }
}

#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <math.h>

/* -------------------- CONFIG -------------------- */
#define DHTPIN 4
#define DHTTYPE DHT11

#define MQ135_PIN A0
#define RAIN_PIN  7    // Changed to digital pin

#define SEALEVELPRESSURE_HPA 1013.25

// MQ135 calibration
#define RL_VALUE 10000.0          // Load resistance (10k)
#define RO_CLEAN_AIR_FACTOR 3.6   // From datasheet

/* -------------------- OBJECTS -------------------- */
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;

/* -------------------- GLOBALS -------------------- */
float Ro = 10000.0;

/* -------------------- MQ135 FUNCTIONS -------------------- */
float getResistance(int adc) {
  if (adc == 0) adc = 1;
  return RL_VALUE * (1023.0 / adc - 1.0);
}

float getPPM(float rs) {
  float ratio = rs / Ro;
  // CO2 curve approximation
  return 116.6020682 * pow(ratio, -2.769034857);
}

void calibrateMQ135() {
  float rs = 0;
  for (int i = 0; i < 50; i++) {
    rs += getResistance(analogRead(MQ135_PIN));
    delay(100);
  }
  rs /= 50.0;
  Ro = rs / RO_CLEAN_AIR_FACTOR;
}

/* -------------------- SETUP -------------------- */
void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(RAIN_PIN, INPUT); // Set digital pin as input

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280_FAIL");
    while (1);
  }

  calibrateMQ135();   // MUST be done in clean air
}

/* -------------------- LOOP -------------------- */
void loop() {

  // --- DHT11 ---
  float dhtTemp = dht.readTemperature();
  float dhtHum  = dht.readHumidity();

  if (isnan(dhtTemp) || isnan(dhtHum)) {
    dhtTemp = -1; // fallback handled in JS
    dhtHum  = -1;
  }

  // --- BMP280 ---
  float bmpTemp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0; // hPa
  float altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  // --- Rain Sensor (Digital) ---
  int rainDigital = digitalRead(RAIN_PIN);
  // Typically LOW = raining, HIGH = dry. Adjust if needed
  String rainStatus = (rainDigital == LOW) ? "Raining 🌧️" : "No Rain ☀️";

  // --- MQ135 ---
  int mqRaw = analogRead(MQ135_PIN);
  float rs = getResistance(mqRaw);
  float ppm = getPPM(rs);

  // Map CO2 ppm to 0-300 AQI for dashboard
  int aqi = map(constrain(ppm, 0, 300), 0, 300, 0, 300);

  // --- SERIAL FORMAT ---
  // dhtTemp,dhtHum,bmpTemp,pressure,altitude,rainStatus,rainDigital,aqi
  Serial.print(dhtTemp);     Serial.print(",");
  Serial.print(dhtHum);      Serial.print(",");
  Serial.print(bmpTemp);     Serial.print(",");
  Serial.print(pressure);    Serial.print(",");
  Serial.print(altitude);    Serial.print(",");
  Serial.print(rainStatus);  Serial.print(",");
  Serial.print(rainDigital); Serial.print(",");
  Serial.println(aqi);

  delay(1100); // send data every 1.1 seconds
}

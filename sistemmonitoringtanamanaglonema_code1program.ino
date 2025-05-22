#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AntaresESPHTTP.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Konfigurasi Antares
#define ACCESSKEY "236f4fed5db3b5d2:959aa6827c00c6fe"
#define WIFISSID "Galaxy"
#define PASSWORD "12345678"

#define projectName "MonitorTanaman"
#define deviceName "kelembabantanaman"

// Konfigurasi Pin
#define DHTPIN 5
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define pump 23
#define LED_PIN 2

// Inisialisasi Objek
DHT dht(DHTPIN, DHTTYPE);
AntaresESPHTTP antares(ACCESSKEY);
LiquidCrystal_I2C lcd(0x27, 16, 2);
HTTPClient http;

// Variabel dan Konstanta
const int AirValue = 2000;
const int WaterValue = 1000;
int soilMoistureValue = 0;
int soilMoisturePercent = 0;
float temperature = 0;
float humidity = 0;
const int moistureThresholdLow = 50;  // Batas kelembapan rendah
const int moistureThresholdHigh = 75; // Batas kelembapan tinggi
bool lastPumpState = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // 5 detik
bool wifiConnected = false;

// Fungsi untuk Menghubungkan WiFi
void setupWiFi() {
  int attempts = 0;
  const int maxAttempts = 20;

  Serial.println("\nConnecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi..");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFISSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi Connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    delay(2000);
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi Connection Failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    delay(2000);
  }
}

// Fungsi untuk Mengirim Data ke Antares
void sendDataToAntares() {
  if (!wifiConnected) {
    Serial.println("WiFi tidak terhubung, data tidak dikirim.");
    return;
  }

  String payload = "{\"m2m:cin\": {\"con\": \"{\\\"soil_moisture\\\": " + String(soilMoisturePercent) +
                    ", \\\"temperature\\\": " + String(temperature) +
                    ", \\\"humidity\\\": " + String(humidity) +
                    ", \\\"pump_status\\\": \\\"" + (lastPumpState ? "ON" : "OFF") + "\\\" }\"}}";

  String uri = "https://platform.antares.id:8443/~/antares-cse/antares-id/" + 
               String(projectName) + "/" + String(deviceName);

  http.begin(uri);
  http.addHeader("Content-Type", "application/json;ty=4");
  http.addHeader("Accept", "application/json");
  http.addHeader("X-M2M-Origin", ACCESSKEY);

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    lcd.setCursor(15, 0);
    lcd.print("*");
    delay(500);
    lcd.setCursor(15, 0);
    lcd.print(" ");
  } else {
    Serial.println("Gagal mengirim data ke Antares!");
    Serial.println("HTTP Error: " + String(httpResponseCode));
    lcd.setCursor(15, 0);
    lcd.print("!");
    delay(500);
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(SOIL_PIN, INPUT);
  pinMode(pump, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(pump, HIGH);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Smart Garden ");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);

  dht.begin();
  setupWiFi();

  lcd.clear();
  lcd.print("M:");
  lcd.setCursor(8, 0);
  lcd.print("P:");
  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.setCursor(8, 1);
  lcd.print("T:");
}

void updateLCD() {
  lcd.setCursor(2, 0);
  lcd.print("    ");
  lcd.setCursor(2, 0);
  lcd.print(soilMoisturePercent);
  lcd.print("%");

  lcd.setCursor(10, 0);
  lcd.print(lastPumpState ? "ON " : "OFF");

  lcd.setCursor(2, 1);
  lcd.print("    ");
  lcd.setCursor(2, 1);
  lcd.print(humidity, 1);
  lcd.print("%");

  lcd.setCursor(10, 1);
  lcd.print("    ");
  lcd.setCursor(10, 1);
  lcd.print(temperature, 1);
  lcd.print("C");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  // Membaca sensor kelembapan tanah
  soilMoistureValue = analogRead(SOIL_PIN);
  soilMoisturePercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

  // Membaca sensor DHT
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Memastikan sensor DHT memberikan pembacaan yang valid
  if (!isnan(temperature) && !isnan(humidity)) {

    // Kontrol Pompa
    if (soilMoisturePercent < moistureThresholdLow) {
      if (!lastPumpState) {
        digitalWrite(pump, LOW); // Pompa menyala jika kelembapan < 50%
        lastPumpState = true;
        Serial.println("Pump: ON");
      }
    } else if (soilMoisturePercent > moistureThresholdHigh) {
      if (lastPumpState) {
        digitalWrite(pump, HIGH); // Pompa mati jika kelembapan > 75%
        lastPumpState = false;
        Serial.println("Pump: OFF");
      }
    }

    // Memperbarui tampilan LCD
    updateLCD();

    // Menampilkan data di Serial Monitor
    Serial.println("\n--------------------");
    Serial.printf("Soil Moisture: %d%%\n", soilMoisturePercent);
    Serial.printf("Temperature: %.1f°C\n", temperature);
    Serial.printf("Humidity: %.1f%%\n", humidity);
    Serial.printf("Pump Status: %s\n", lastPumpState ? "ON" : "OFF");

    // Mengirim data ke Antares setiap interval yang ditentukan
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval) {
      digitalWrite(LED_PIN, HIGH);
      sendDataToAntares();
      digitalWrite(LED_PIN, LOW);
      lastSendTime = currentTime;
    }
  } else {
    Serial.println("Error membaca sensor DHT!");
  }

  delay(5000); // Delay 5 detik sebelum membaca sensor lagi
}
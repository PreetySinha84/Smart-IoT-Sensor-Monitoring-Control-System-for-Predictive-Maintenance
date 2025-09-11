#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// Wi-Fi Credentials (PLACEHOLDER)
const char* ssid = "YOUR_WIFI_SSID"; // ENTER YOUR WI-FI NAME
const char* password = "YOUR_WIFI_PASSWORD";  // ENTER YOUR WI-FI PASSWORD

// Firebase Setup (PLACEHOLDER)
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

const char* FIREBASE_HOST = "https://<your-database-name>.firebaseio.com";  //REPLACE WITH YOUR FIREBASE URL
const char* FIREBASE_DB_SECRET = "<YOUR_FIREBASE_DB_SECRET>";   //REPLACE WITH YOUR FIREBASE SECRET

// ThingSpeak Config
const char* thingspeak_server = "api.thingspeak.com";
String ts_api_key = "<YOUR_THINGSPEAK_API_KEY>"; //REPLACE WITH YOUR THINGSPEAK API KEY

// Clients
WiFiClient espClient;
WiFiClientSecure secureClient;
WiFiClient httpClient;

//  Sensors
#define DHTPIN D4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;
#define MQ4_PIN A0

bool sensorEnabled = false;

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  secureClient.setInsecure();

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi connected.");

  // Firebase Config
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_DB_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  dht.begin();
  bmp.begin();
}

void loop() {
  // Firebase Switch ON/OFF
  if (Firebase.getString(firebaseData, "/SensorControl/Switch")) {
    String switchState = firebaseData.stringData();
    sensorEnabled = (switchState == "ON");
    Serial.println("Firebase Switch: " + switchState);
  } else {
    Serial.println("⚠ Failed to read switch from Firebase");
    sensorEnabled = false;
  }

  if (sensorEnabled) {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float pressure = bmp.readPressure() / 100.0;
    int gas = analogRead(MQ4_PIN);

    if (!isnan(temperature) && !isnan(humidity) && pressure > 0) {
      //  Send to ThingSpeak
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://" + String(thingspeak_server) + "/update?api_key=" + ts_api_key +
                     "&field1=" + String(temperature) +
                     "&field2=" + String(humidity) +
                     "&field3=" + String(pressure) +
                     "&field4=" + String(gas);
        http.begin(httpClient, url);
        http.GET();
        http.end();
        Serial.println("📡 Sent to ThingSpeak: " + url);
      }

      //  Send to Firebase SensorData
      Firebase.setFloat(firebaseData, "/SensorData/temperature", temperature);
      Firebase.setFloat(firebaseData, "/SensorData/humidity", humidity);
      Firebase.setFloat(firebaseData, "/SensorData/pressure", pressure);
      Firebase.setInt(firebaseData, "/SensorData/gas", gas);

      //  Send Alert Flags to Firebase /Alerts
      if (temperature > 35) {
        Firebase.setString(firebaseData, "/Alerts/temperature", "HIGH");
      } else {
        Firebase.setString(firebaseData, "/Alerts/temperature", "NORMAL");
      }

      if (humidity > 70) {
        Firebase.setString(firebaseData, "/Alerts/humidity", "HIGH");
      } else {
        Firebase.setString(firebaseData, "/Alerts/humidity", "NORMAL");
      }

      if (pressure < 1000) {
        Firebase.setString(firebaseData, "/Alerts/pressure", "LOW");
      } else {
        Firebase.setString(firebaseData, "/Alerts/pressure", "NORMAL");
      }

      if (gas > 400) {
        Firebase.setString(firebaseData, "/Alerts/gas", "LEAK");
      } else {
        Firebase.setString(firebaseData, "/Alerts/gas", "NORMAL");
      }

      Serial.println(" Firebase updated with sensor data & alerts.");
    } else {
      Serial.println(" Sensor read error.");
    }

  } else {
    Serial.println("⚠ Sensors disabled (Switch OFF)");
  }
  delay(15000);  // 15 sec delay
}
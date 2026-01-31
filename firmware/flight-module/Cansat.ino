#include <esp_now.h>
#include <WiFi.h>
#include "DHT.h"
#include <ArduinoJson.h>

// --- Sensor Definitions ---
#define DHT_PIN     4
#define MQ135_PIN   34
#define MQ131_PIN   35
#define DHT_TYPE    DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// --- REPLACE WITH YOUR GROUND STATION'S MAC ADDRESS ---
uint8_t broadcastAddress[] = {0xF4, 0x65, 0x0B, 0x47, 0x0A, 0x54};

// --- !! IMPORTANT CALIBRATION VALUES !! ---
// Value of the load resistor on your MQ-131 board in Ohms (usually 10k or 20k)
#define RL_VALUE_OZONE   10000 
// Resistance of the MQ-131 sensor in clean air (you must find this value)
// Start with 280000 and follow the calibration step below.
float R0_OZONE = 280000.0; 

// --- Data Structure (must match ground station) ---
typedef struct struct_message {
  float temp;
  float humid;
  int aq_val;
  int ozone_val;
  int aqi;
  int state;
  int rssi;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;
float last_good_temp = 25.0;
float last_good_humid = 60.0;

// =========================================================================
// --- NEW, CORRECTED SENSOR AND AQI FUNCTIONS ---
// =========================================================================

// Calculates Ozone concentration in PPB using the sensor's resistance ratio
float getOzonePpb(int rawAdc) {
  // Convert ADC value to voltage
  float sensor_volt = (rawAdc / 4095.0) * 3.3;
  // Calculate sensor resistance (Rs)
  float Rs = ((3.3 * RL_VALUE_OZONE) / sensor_volt) - RL_VALUE_OZONE;
  // Calculate ratio of Rs/R0
  float ratio = Rs / R0_OZONE;
  // Use the formula from the datasheet's log-log graph to find concentration
  // y = a * x^b  -->  ppb = 10.166 * (ratio)^-1.451
  float ppb = 10.166 * pow(ratio, -1.451);
  return ppb > 0 ? ppb : 0;
}

// Correctly calculates AQI based on EPA standards for Ozone
int calculateAqi(float concentration_ppb) {
  float aqi = 0;
  float c = concentration_ppb; // shorthand

  if (c >= 0 && c <= 54) {
    aqi = map(c * 10, 0, 540, 0, 50);
  } else if (c > 54 && c <= 70) {
    aqi = map(c * 10, 550, 700, 51, 100);
  } else if (c > 70 && c <= 85) {
    aqi = map(c * 10, 710, 850, 101, 150);
  } else if (c > 85 && c <= 105) {
    aqi = map(c * 10, 860, 1050, 151, 200);
  } else if (c > 105 && c <= 200) {
    aqi = map(c * 10, 1060, 2000, 201, 300);
  } else {
    aqi = 301; // Or higher, classified as Hazardous
  }
  return int(aqi);
}

// Classifies the environment state based on AQI
void classifyEnvironment() {
  if (myData.aqi > 100) myData.state = 2;       // Pollution Event
  else if (myData.humid > 80.0) myData.state = 1; // High Humidity (adjusted threshold)
  else myData.state = 0;                          // Normal
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Peer add failed");
    return;
  }
}

void loop() {
  delay(2000);
  
  // --- Read Sensors ---
  float temp_reading = dht.readTemperature();
  float humid_reading = dht.readHumidity();
  if (!isnan(temp_reading)) last_good_temp = temp_reading;
  if (!isnan(humid_reading)) last_good_humid = humid_reading;
  myData.temp = last_good_temp;
  myData.humid = last_good_humid;
  myData.aq_val = analogRead(MQ135_PIN);
  myData.ozone_val = analogRead(MQ131_PIN);

  // --- Perform Calculations using NEW functions ---
  float ozone_ppb = getOzonePpb(myData.ozone_val);
  myData.aqi = calculateAqi(ozone_ppb); // We now only use the Ozone AQI for simplicity
  classifyEnvironment();

  // --- Send via ESP-NOW ---
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  // --- JSON output for Serial debugging ---
  StaticJsonDocument<200> doc;
  doc["temp"] = myData.temp;
  doc["humid"] = myData.humid;
  doc["aq_val"] = myData.aq_val;
  doc["ozone_val"] = myData.ozone_val;
  doc["ozone_ppb"] = ozone_ppb; // Also printing the calculated ppb for checking
  doc["aqi"] = myData.aqi;
  doc["state"] = myData.state;
  String output;
  serializeJson(doc, output);
  Serial.println(output);
}
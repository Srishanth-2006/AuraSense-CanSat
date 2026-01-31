#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// --- Data Structure (must match CanSat) ---
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
volatile bool newData = false;

// --- Anomaly Detection Variables (from your original code) ---
const int LEARNING_COUNT = 30;
int readings_counter = 0;
bool learning_complete = false;
long aq_sum = 0;
long ozone_sum = 0;
float aq_mean = 0;
float ozone_mean = 0;
float aq_std_dev = 0;
float ozone_std_dev = 0;
int aq_history[LEARNING_COUNT];
int ozone_history[LEARNING_COUNT];

// --- STABILITY FIX: JSON objects are created once, globally ---
StaticJsonDocument<200> doc;
String json_output;

// --- Callback function when data is received ---
void OnDataRecv(const esp_now_recv_info_t * mac_info, const uint8_t *incomingData, int len) {
  // Ensure the received data has the correct size to prevent errors
  if (len == sizeof(myData)) {
    memcpy(&myData, incomingData, sizeof(myData));
    myData.rssi = mac_info->rx_ctrl->rssi;
    newData = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500); // Small delay for serial port to stabilize
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  
  // Register the callback function to handle incoming data
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Ground Station Ready. Starting learning phase...");
}

void loop() {
  // Only process data when the 'newData' flag is set by the callback
  if (newData) {
    newData = false; // Reset the flag
    
    // --- LEARNING PHASE LOGIC (Unchanged from your original code) ---
    if (!learning_complete) {
      aq_sum += myData.aq_val;
      ozone_sum += myData.ozone_val;
      aq_history[readings_counter] = myData.aq_val;
      ozone_history[readings_counter] = myData.ozone_val;
      readings_counter++;
      Serial.printf("Learning... [%d/%d]\n", readings_counter, LEARNING_COUNT);
      
      if (readings_counter >= LEARNING_COUNT) {
        aq_mean = (float)aq_sum / LEARNING_COUNT;
        ozone_mean = (float)ozone_sum / LEARNING_COUNT;
        float aq_std_dev_sum = 0;
        float ozone_std_dev_sum = 0;
        for (int i = 0; i < LEARNING_COUNT; i++) {
          aq_std_dev_sum += pow(aq_history[i] - aq_mean, 2);
          ozone_std_dev_sum += pow(ozone_history[i] - ozone_mean, 2);
        }
        aq_std_dev = sqrt(aq_std_dev_sum / LEARNING_COUNT);
        ozone_std_dev = sqrt(ozone_std_dev_sum / LEARNING_COUNT);
        learning_complete = true;
        Serial.println("\n--- Learning Complete. Now monitoring data. ---");
        Serial.printf("AQ Baseline -> Mean: %.2f, StdDev: %.2f\n", aq_mean, aq_std_dev);
        Serial.printf("Ozone Baseline -> Mean: %.2f, StdDev: %.2f\n", ozone_mean, ozone_std_dev);
      }
    } else {
      // --- STABLE JSON OUTPUT (This part is now fixed) ---
      doc["temp"] = myData.temp;
      doc["humid"] = myData.humid;
      doc["aq_val"] = myData.aq_val;
      doc["ozone_val"] = myData.ozone_val;
      doc["aqi"] = myData.aqi;
      doc["state"] = myData.state;
      doc["rssi"] = myData.rssi;
      
      serializeJson(doc, json_output);
      Serial.println(json_output);
      json_output.clear(); // Clear the string for the next use
    }
  }
}
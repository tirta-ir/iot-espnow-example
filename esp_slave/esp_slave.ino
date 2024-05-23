#include <esp_now.h>  // Include the ESP-NOW library
#include <WiFi.h>     // Include the WiFi library
#include <FastLED.h>  // Include the FastLED library for LED WS2812

// Define LED pin and wifi channel
#define CHANNEL 1
#define DATA_PIN 15
#define NUM_LEDS 8

CRGB leds[NUM_LEDS];  // Define an array of LEDs

// Define data structure for acceleration data
struct __attribute__((packed)) accelData {
  float x;
  float y;
  float z;
};

// Initialize ESP-NOW protocol for slave device
void InitESPNow() {
  WiFi.disconnect();  // Disconnect from any WiFi network
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();  // Restart if ESP-NOW initialization fails
  }
}

// Configure AP SSID
void configDeviceAP() {
  const char *SSID = "Slave_1";  // Set the SSID for the Access Point
  bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);  // Set up the Access Point with a password
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
    Serial.print("AP CHANNEL "); 
    Serial.println(WiFi.channel());  // Print the channel the AP is broadcasting on
  }
}

void setup() {
  Serial.begin(115200);  // Start serial communication at 115200 baud
  WiFi.mode(WIFI_AP);    // Set the WiFi mode to Access Point
  configDeviceAP();      // Configure the device as an Access Point
  
  // Print AP MAC address
  Serial.print("AP MAC: "); 
  Serial.println(WiFi.softAPmacAddress());  // Print the MAC address of the AP
  
  InitESPNow();  // Initialize ESP-NOW
  esp_now_register_recv_cb(OnDataRecv);  // Register the receive callback function
  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);  // Initialize the FastLED library
}

// Callback function that is executed when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];  // Array to hold MAC address as a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);  // Print the MAC address of the sender
  
  accelData *receivedData = (accelData *)data;  // Cast the received data to the accelData structure
  Serial.print("Received accelData: ");
  Serial.print("X: "); Serial.print(receivedData->x);
  Serial.print(", Y: "); Serial.print(receivedData->y);
  Serial.print(", Z: "); Serial.println(receivedData->z);  // Print the received acceleration data

  static accelData lastData = *receivedData;  // Keep track of the last received data
  static unsigned long lastChange = millis();  // Keep track of the last change time
  static unsigned long lastToggle = 0;  // Keep track of the last toggle time
  static bool ledState = false;  // Keep track of the LED state

  if (abs(receivedData->x - lastData.x) > 0.05 || abs(receivedData->y - lastData.y) > 0.05 || abs(receivedData->z - lastData.z) > 0.05) {
    // If any coordinate changes more than 0.05
    if (millis() - lastToggle >= 25) {  // 2 Hz is every 500 ms
      ledState = !ledState;  // Toggle the LED state
      lastToggle = millis();  // Update the last toggle time
    }
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = ledState ? CRGB::Green : CRGB::Black;  // If ledState is true, turn the LEDs green, else turn them off
    }
    lastData = *receivedData;  // Update the last data
    lastChange = millis();  // Update the last change time
  } else {
    // If no coordinate changes more than 0.05
    if (millis() - lastChange < 10000) {  // If less than 10 seconds have passed since the last change
      if (millis() - lastToggle >= 25) {  // 2 Hz is every 500 ms
        ledState = !ledState;  // Toggle the LED state
        lastToggle = millis();  // Update the last toggle time
      }
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ledState ? CRGB::Green : CRGB::Black;  // If ledState is true, turn the LEDs green, else turn them off
      }
    } else {
      // If 10 seconds have passed since the last change, turn off the LEDs
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
    }
  }
  FastLED.show();  // Update the LED display
}

void loop() {
  // Chill
}

//2024-may-mti
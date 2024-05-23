#include <esp_now.h>  // Include the ESP-NOW library
#include <WiFi.h>     // Include the WiFi library
#include <esp_wifi.h> // Include the ESP WiFi library
#include <MPU6050.h>  // Include the MPU6050 library for Gyroscope
#include <FastLED.h>  // Include the FastLED library for LED WS2812

// Global copy of slave information
MPU6050 mpu;  // Create an instance of MPU6050
esp_now_peer_info_t slave;  // Structure to hold slave information

#define CHANNEL 1  // Define the WiFi channel to be used
#define PRINTSCANRESULTS 0  // Define whether to print scan results
#define DELETEBEFOREPAIR 0  // Define whether to delete peer before pairing

// Define LED pin and number of LEDs
#define DATA_PIN 15
#define NUM_LEDS 8

CRGB leds[NUM_LEDS];  // Array to hold LED data

// Define a structure for acceleration data
struct __attribute__((packed)) accelData {
  float x;
  float y;
  float z;
};

accelData data;  // Create an instance of accelData

// Init ESP-NOW with fallback
void InitESPNow() {
  WiFi.disconnect();  // Disconnect from any WiFi network
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();  // Restart if ESP-NOW initialization fails
  }
}

// Scan for slaves in AP mode
void ScanForSlave() {
  int16_t scanResults = WiFi.scanNetworks(false, false, false, 300, CHANNEL); // Scan only on one channel
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));  // Reset the slave information

  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave") == 0) {
        // SSID of interest
        Serial.println("Found a Slave.");
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        slave.channel = CHANNEL; // Pick a channel
        slave.encrypt = 0; // No encryption

        slaveFound = 1;
        // We are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }

  if (slaveFound) {
    Serial.println("Slave Found, processing..");
  } else {
    Serial.println("Slave Not Found, trying again.");
  }

  // Clean up RAM
  WiFi.scanDelete();
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    Serial.print("Slave Status: ");
    // Check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if (exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    } else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        return true;
      } else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}

// Delete the paired slave
void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else {
    Serial.println("Not sure what happened");
  }
}

// Send data to the slave
void sendData() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);  // Get acceleration data from MPU6050
  data.x = ax / 16384.0; // Scale for a +/- 2g setting
  data.y = ay / 16384.0; // Scale for a +/- 2g setting
  data.z = az / 16384.0; // Scale for a +/- 2g setting
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.print("Sending: X: "); Serial.print(data.x);
  Serial.print(", Y: "); Serial.print(data.y);
  Serial.print(", Z: "); Serial.println(data.z);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&data, sizeof(data));  // Send data via ESP-NOW
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
  } else {
    Serial.println("Not sure what happened");
  }
}

// Callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);  // Start serial communication at 115200 baud
  Wire.begin();  // Initialize I2C communication
  mpu.initialize();  // Initialize the MPU6050
  // Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.println("ESPNow/Basic/Master Example");
  // This is the MAC address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("STA CHANNEL "); Serial.println(WiFi.channel());
  // Init ESP-NOW with a fallback logic
  InitESPNow();
  // Once ESP-NOW is successfully initialized, register for Send Callback to
  // get the status of transmitted packets
  esp_now_register_send_cb(OnDataSent);

  // Initialize FastLED
  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
}

// Add a variable to store the timestamp
unsigned long ledOnTimestamp = 0;

void loop() {
  // In the loop we scan for slave
  ScanForSlave();
  // If Slave is found, it would be populated in `slave` variable
  // We will check if `slave` is defined and then we proceed further
  if (slave.channel == CHANNEL) { // Check if slave channel is defined
    // `slave` is defined
    // Add slave as peer if it has not been added already
    bool isPaired = manageSlave();
    if (isPaired) {
      // Pair success or already paired
      // Send data to device
      sendData();
      // Get the acceleration data
      int16_t ax, ay, az;
      mpu.getAcceleration(&ax, &ay, &az);
      data.x = ax / 16384.0; // Scale for a +/- 2g setting
      data.y = ay / 16384.0; // Scale for a +/- 2g setting
      data.z = az / 16384.0; // Scale for a +/- 2g setting

      static accelData lastData = data; // Keep track of the last data

      if (abs(data.x - lastData.x) > 0.05 || abs(data.y - lastData.y) > 0.05 || abs(data.z - lastData.z) > 0.05) {
        // If any coordinate changes more than 0.05, turn the LEDs green
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Green;
        }
        lastData = data; // Update the last data
        ledOnTimestamp = millis(); // Update the timestamp
      } else if (millis() - ledOnTimestamp < 10000) {
        // If no coordinate changes more than 0.05 and less than 10 seconds have passed, keep the LEDs on
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Green;
        }
      } else {
        // If no coordinate changes more than 0.05 and more than 10 seconds have passed, turn off the LEDs
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
      }
      FastLED.show();  // Update the LED display
    } else {
      // Slave pair failed
      Serial.println("Slave pair failed!");
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
  }
  // Wait for 10 milliseconds to run the logic again
  delay(10);
}

//2024-may-mti

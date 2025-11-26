#include <esp_now.h>
#include <WiFi.h>

// PIR sensor pin
#define PIR_PIN 2   // change if needed

// MAC address of the INSIDE ESP32-S3 (receiver)
uint8_t receiverAddress[] = {0x0C, 0xB8, 0x15, 0xC3, 0xA2, 0xD8};

// Track last motion state so we only send on changes (optional)
int lastMotion = -1;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);

  // Wi-Fi in station mode
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register send callback
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Outside node (motion sender) ready");
}

void loop() {
  int motion = digitalRead(PIR_PIN);  // HIGH = motion detected

  if (motion != lastMotion) {
    lastMotion = motion;

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&motion, sizeof(motion));

    if (result == ESP_OK) {
      Serial.print("Sent: ");
      Serial.println(motion == HIGH ? "MOTION" : "NO MOTION");
    } else {
      Serial.println("Error sending data");
    }
  }

  delay(100);
}

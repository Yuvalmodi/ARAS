#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --- PIN DEFINITIONS ---
#define IGNITION_RELAY_PIN  12
#define VEHICLE_BUZZER_PIN  13

// Telemetry Payload Structure (Must precisely match the Helmet signature)
typedef struct struct_message {
    bool isWorn;
    bool crashDetected;
} struct_message;

struct_message incomingTelemetry;

// Interlock Feedback Function
void onDataReceive(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&incomingTelemetry, incomingData, sizeof(incomingTelemetry));
    
    if (incomingTelemetry.crashDetected) {
        Serial.println("CRASH VECTOR DETECTED - FORWARDING SOS EMERGENCY ALERT");
        digitalWrite(VEHICLE_BUZZER_PIN, HIGH);
        digitalWrite(IGNITION_RELAY_PIN, LOW); // Force safely kill engine connection [cite: 59]
        return;
    }
    
    if (incomingTelemetry.isWorn) {
        Serial.println("Helmet Securely Latched. Ignition Interlock Released.");
        digitalWrite(IGNITION_RELAY_PIN, HIGH); // Complete ignition starter loop 
        digitalWrite(VEHICLE_BUZZER_PIN, LOW);
    } else {
        Serial.println("SAFETY VIOLATION: Helmet removed or unstrapped. Cutting ignition.");
        digitalWrite(IGNITION_RELAY_PIN, LOW);  // Disengage relay to stall/prevent start 
        digitalWrite(VEHICLE_BUZZER_PIN, HIGH); // Fire alarm warning
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(IGNITION_RELAY_PIN, OUTPUT);
    pinMode(VEHICLE_BUZZER_PIN, OUTPUT);
    
    // Ensure vehicle remains safely locked down upon cold boot
    digitalWrite(IGNITION_RELAY_PIN, LOW); [cite: 21, 59]
    digitalWrite(VEHICLE_BUZZER_PIN, LOW);
    
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Failed initializing vehicle ESP-NOW subsystem.");
        return;
    }
    
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataReceive));
    Serial.println("Scooter System Armed. Awaiting Helmet secure handshake...");
}

void loop() {
    // Isolated system execution loops safely inside the interrupt callbacks.
    delay(100);
}

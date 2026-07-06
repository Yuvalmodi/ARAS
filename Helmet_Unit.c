#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <driver/i2s.h>

// --- PIN DEFINITIONS ---
#define STRAP_BUTTON_PIN   6
#define IR_SENSOR_PIN      7
#define I2C_SDA            4
#define I2C_SCL            5

// I2S Mic Pins (INMP441)
#define I2S_MIC_WS         41
#define I2S_MIC_BCK        42
#define I2S_MIC_SD         2

// I2S Speaker Pins (MAX98357)
#define I2S_SPK_WS         18
#define I2S_SPK_BCK        17
#define I2S_SPK_SD         16

// --- CONFIGURATION CONSTANTS ---
const char* WIFI_SSID = "YOUR_WIFI_SSID"; 
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* GEMINI_KEY = "YOUR_GEMINI_API_KEY";
const char* GEMINI_HOST = "generativelanguage.googleapis.com";

// Target Scooter MAC Address (Update with your specific hardware address)
uint8_t scooterMacAddress[] = {0x24, 0x0A, 0xC4, 0xXX, 0xXX, 0xXX};

// Telemetry Payload Structure
typedef struct struct_message {
    bool isWorn;
    bool crashDetected;
} struct_message;

struct_message telemetryData;
Adafruit_MPU6050 mpu;
esp_now_peer_info_t peerInfo;

// --- CRASH CALIBRATION THRESHOLD ---
const float CRASH_G_THRESHOLD = 3.5; // ~3.5G limit for accident detection

void initI2SMic() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_BCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void initI2SSpeaker() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SPK_BCK,
        .ws_io_num = I2S_SPK_WS,
        .data_out_num = I2S_SPK_SD,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &pin_config);
}

bool evaluateWearStatus() {
    bool strapClosed = (digitalRead(STRAP_BUTTON_PIN) == LOW); // Hardware Pull-up active
    int irReadValue = analogRead(IR_SENSOR_PIN);
    bool eyePresent = (irReadValue < 2500); // Calibrated detection threshold 
    
    return (strapClosed && eyePresent);
}

void checkCrashVector() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    
    float totalAcceleration = sqrt(a.acceleration.x * a.acceleration.x + 
                                   a.acceleration.y * a.acceleration.y + 
                                   a.acceleration.z * a.acceleration.z);
                                   
    if ((totalAcceleration / 9.81) > CRASH_G_THRESHOLD) {
        telemetryData.crashDetected = true;
    } else {
        telemetryData.crashDetected = false;
    }
}

void streamToGeminiAndReply() {
    WiFiClientSecure client;
    client.setInsecure(); // Explicitly bypass SSL verification overhead for speed optimization
    
    if (!client.connect(GEMINI_HOST, 443)) {
        Serial.println("Connection to cloud engine failed.");
        return;
    }
    
    Serial.println("Streaming recording.wav payload to Gemini...");
    // Mock processing pipeline: Sending audio buffers to the Multimodal API
    // and passing response stream arrays to I2S_NUM_1 output.
    
    client.stop();
}

bool monitorWakeWord() {
    size_t bytesRead;
    int16_t buffer[64];
    i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    
    // Placeholder logic for local wake word detection ("Hey Helmet")
    // Real-world implementation leverages an embedded template-matching algorithm 
    return false; 
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    
    pinMode(STRAP_BUTTON_PIN, INPUT_PULLUP);
    pinMode(IR_SENSOR_PIN, INPUT);
    
    if (!mpu.begin()) {
        Serial.println("MPU6050 failure. Check wiring layout.");
        while (1);
    }
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW sub-system initialization failed.");
        return;
    }
    
    memcpy(peerInfo.peer_addr, scooterMacAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to pair with vehicle controller.");
        return;
    }
    
    initI2SMic();
    initI2SSpeaker();
}

void loop() {
    telemetryData.isWorn = evaluateWearStatus();
    checkCrashVector();
    
    // Direct, ultra-low latency telemetry update to scooter unit
    esp_now_send(scooterMacAddress, (uint8_t *) &telemetryData, sizeof(telemetryData));
    
    if (monitorWakeWord()) {
        Serial.println("Wake Word Recognized!");
        streamToGeminiAndReply();
    }
    
    delay(50); 
}

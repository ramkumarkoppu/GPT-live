#include "AudioTools.h"
#include <WiFi.h>
#include <WiFiUdp.h>

/* UDP target */
constexpr char *kUdpAddress{"192.168.1.123"};
constexpr uint16_t kUdpPort{12345};

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};
constexpr uint8_t kAudioDuration{5U};
constexpr uint32_t kAudioTotalBytes{(kSampleRate_hz * kChannelCount * (kBitsPerSample / 8) * kAudioDuration)}; // 5 seconds of audio = 16000Hz * 2 channels * 4 bytes = 128000 bytes / sec = > s seconds = 640000 bytes
constexpr uint16_t kAudioBufferSize{1024U};

static WiFiUDP s_udp;
static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample};
static I2SStream s_i2s_in{};
static I2SConfig s_i2s_config;
static uint8_t s_AudioBuffer[kAudioBufferSize]{};

static void connectWifi(void);
static void setupI2SInput(void);

void setup()
{
    // Setup UART for debug prints and input for volume controls and audio output device selection
    Serial.begin(115200);
    while (!Serial)
        ;

    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    connectWifi();
    setupI2SInput();
    // Wait a bit for I2S to stabilize
    delay(500);

    Serial.printf("Sending %u seconds of audio via UDP to %s:%u\n", kAudioDuration, kUdpAddress, kUdpPort);
    size_t total_sent{0};
    size_t bytes_read{0};

    // Send Audio in chunks
    while (total_sent < kAudioTotalBytes)
    {
        // Read audio data
        bytes_read = s_i2s_in.readBytes(s_AudioBuffer, kAudioBufferSize);

        if (bytes_read)
        {
            // Send via UDP
            s_udp.beginPacket(kUdpAddress, kUdpPort);
            s_udp.write(s_AudioBuffer, bytes_read);
            s_udp.endPacket();

            total_sent += bytes_read;

            // Progress indicator
            if ((total_sent % 64000) == 0)
            {
                Serial.printf("Sent %u bytes (%.1f seconds)\n", total_sent, (total_sent / 128000.0));
            }
        }
        else
        {
            Serial.println("Warning: No data read from I2S");
            delay(10);
        }
    }
}

void loop()
{
    // Nothing - runs once
}

static void connectWifi(void)
{
    Serial.println("Enter your WiFi SSID: ");
    while (Serial.available() == 0)
        ;
    String ssid = Serial.readStringUntil('\n');
    ssid.trim();

    Serial.println("Enter your WiFi password: ");
    while (Serial.available() == 0)
        ;
    String password = Serial.readStringUntil('\n');
    password.trim();

    Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
}

static void setupI2SInput(void)
{
    Serial.println("Starting I2S input...");
    s_i2s_config = s_i2s_in.defaultConfig(RX_MODE);
    s_i2s_config.copyFrom(s_info);

    // XVF3800 pins
    s_i2s_config.pin_bck = 8;
    s_i2s_config.pin_ws = 7;
    s_i2s_config.pin_data = kI2sTxPin;    // Tx data pin
    s_i2s_config.pin_data_rx = kI2sRxPin; // Rx data pin
    s_i2s_config.is_master = true;        // our uc is master of I2S
    s_i2s_in.begin(s_i2s_config);
    Serial.println("I2S input started");
}

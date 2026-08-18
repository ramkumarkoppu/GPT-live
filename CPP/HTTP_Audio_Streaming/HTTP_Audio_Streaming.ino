#include <AudioTools.h>
#include <WiFi.h>
#include <HTTPClient.h>

/* HTTP server */
constexpr char *kserverUrl{"http://mini5.local:8000/upload"};

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};
constexpr uint8_t kAudioDuration{5U};
constexpr uint32_t kAudioTotalBytes{(kSampleRate_hz * kChannelCount * (kBitsPerSample / 8) * kAudioDuration)}; // 5 seconds of audio = 16000Hz * 2 channels * 4 bytes = 128000 bytes / sec = > s seconds = 640000 bytes
constexpr uint8_t kWifiConnectRetryCnt{20U};

static WiFiUDP s_udp;
static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample};
static I2SStream s_i2s_in{};
static I2SConfig s_i2s_config;

static void connectWifi(void);
static void setupI2SInput(void);

void setup()
{
    // Setup UART for debug prints
    Serial.begin(115200);
    while (!Serial)
        ;

    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    uint8_t *audioBuffer = static_cast<uint8_t *>(malloc(kAudioTotalBytes));
    if (!audioBuffer)
    {
        Serial.println("Failed to allocate memory!");
        return;
    }

    connectWifi();
    setupI2SInput();
    // Wait a bit for I2S to stabilize
    delay(500);

    Serial.println("Recording 5 seconds of audio...");
    size_t total_read{0};
    size_t bytes_read{0};

    uint32_t start_time = millis();

    // Send Audio in chunks
    while (total_read < kAudioTotalBytes)
    {
        // Read audio data
        bytes_read = s_i2s_in.readBytes((audioBuffer + total_read), std::min<size_t>(4096, (kAudioTotalBytes - total_read)));

        if (bytes_read)
        {
            total_read += bytes_read;

            // Progress indicator
            if ((total_read % 128000) == 0)
            {
                Serial.printf("Recorded %u bytes (%.1f seconds)\n", total_read, (total_read / 128000.0));
            }
        }
        else
        {
            Serial.println("Warning: No data read from I2S");
            delay(10);
        }
    }

    uint32_t record_time = millis() - start_time;
    Serial.printf("Recoding complete! %u bytes in %lu ms\n", total_read, record_time);

    /* Send via HTTP POST */
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http{};

        Serial.printf("Sending audio to %s\n", kserverUrl);
        http.begin(kserverUrl);
        http.addHeader("Content-Type", "application/octet-stream");
        http.addHeader("X-Sample-Rate", String(s_info.sample_rate));
        http.addHeader("X-Channels", String(s_info.channels));
        http.addHeader("X-Bits-Per-Sample", String(s_info.bits_per_sample));

        int16_t httpResponseCode = http.POST(audioBuffer, total_read);

        if (httpResponseCode > 0)
        {
            Serial.printf("HTTP Response code: %d\n", httpResponseCode);
            String response = http.getString();
            Serial.println("Response: " + response);
        }
        else
        {
            Serial.printf("Error code: %d\n", httpResponseCode);
            Serial.println("Error: " + http.errorToString(httpResponseCode));
        }

        http.end();
    }
    else
    {
        Serial.println("WiFi not connected");
    }

    free(audioBuffer);
    Serial.println("Done!");
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
    uint8_t attempts{0U};
    while ((WiFi.status() != WL_CONNECTED) && (attempts < kWifiConnectRetryCnt))
    {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nConnected!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("\nFailed to connect");
    }
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

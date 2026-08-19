#include <AudioTools.h>       // For handling I2S audio and WAV processing
#include <WiFi.h>             // For WiFi connectivity
#include <WebSocketsClient.h> // For WebSockets communication

constexpr char *kWsHost{"192.168.1.123"}; // Raspberry PI IP address where the WebSockets server is running.
constexpr uint16_t kWsPort{12345};        // Server port
constexpr char *kWsPath{"/"};

constexpr uint16_t kSampleRate_hz{48000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};

constexpr uint16_t kBufferSize{1024U};
constexpr uint32_t kAudioBytesPerSec{kSampleRate_hz * kChannelCount * static_cast<uint16_t>(kBitsPerSample / 8)}; // Size of each WebSocket data packet (in bytes)
constexpr uint32_t kAudioTotalBytes{kAudioBytesPerSec * 5};                                                       // Number of packets to send (5 seconds of audio at 48KHz)
constexpr uint8_t kWifiConnectRetryCnt{5U};
constexpr uint16_t kWsReconnectInterval{2000U};
constexpr uint16_t kWsConnectTimeout{10000U};

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample};
static I2SStream s_i2s_in{};   // I2S audio input stream
static I2SConfig s_i2s_config; // I2S hardware configuration

static WebSocketsClient s_webSocket;
static bool s_wsConnected{false};

static void connectWifi(void);
static void setupI2SInput(void);
static void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

void setup()
{
    uint8_t *audio_buffer{nullptr};

    // Setup UART for debug prints
    Serial.begin(115200);
    while (!Serial)
        ;

    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    connectWifi();

    if (psramFound())
    {
        audio_buffer = static_cast<uint8_t *>(ps_malloc(kAudioTotalBytes));
        if (!audio_buffer)
        {
            return;
        }
        Serial.println("Using PSRAM for audio buffer");
    }
    else
    {
        audio_buffer = static_cast<uint8_t *>(malloc(kAudioTotalBytes));
        if (!audio_buffer)
        {
            Serial.println("no PSRAM found - audio buffer allocation failed");
            return;
        }
    }

    setupI2SInput();
    delay(500);

    /* Connect WebSocket */
    s_webSocket.begin(kWsHost, kWsPort, kWsPath);
    s_webSocket.onEvent(webSocketEvent);
    s_webSocket.setReconnectInterval(kWsReconnectInterval);

    Serial.println("Waiting for WEbSocket connection...");
    uint32_t waitStart = millis();
    while (!s_wsConnected && ((millis() - waitStart) < kWsConnectTimeout))
    {
        s_webSocket.loop();
        delay(10);
    }
    if (!s_wsConnected)
    {
        Serial.println("Failed to connect to WEbSocket server!");
        return;
    }

    /* Capture audio and send */
    Serial.println("Capturing 5 seconds of audio...");
    size_t total_captured{0U};

    while (total_captured < kAudioTotalBytes)
    {
        size_t to_read = std::min<size_t>(kBufferSize, static_cast<size_t>(kAudioTotalBytes - total_captured));
        size_t bytes_read = s_i2s_in.readBytes((audio_buffer + total_captured), to_read);
        if (bytes_read)
        {
            total_captured += bytes_read;
            if ((total_captured % kAudioBytesPerSec) == 0)
            {
                Serial.printf("Captured %u bytes (%.1f sec)\n", total_captured, (static_cast<float>(total_captured) / kAudioBytesPerSec));
            }
        }
        else
        {
            delay(1);
        }
    }

    Serial.printf("Sending %u bytes via WebSocket...\n", total_captured);
    const size_t SEND_CHUNK{4096U};
    size_t total_sent{0U};

    while (total_sent < total_captured)
    {
        size_t chunk = std::min<size_t>(SEND_CHUNK, static_cast<size_t>(total_captured - total_sent));
        s_webSocket.sendBIN((audio_buffer + total_sent), chunk);
        s_webSocket.loop();
        total_sent += chunk;
    }

    Serial.printf("Finished! Sent %u bytes total\n", total_sent);
    s_webSocket.disconnect();
    free(audio_buffer);
}

void loop()
{
    // Nothing to do — one-shot capture/send completes in setup()
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
    s_i2s_config.is_master = false;       // XVF3800 is the master of I2S
    s_i2s_in.begin(s_i2s_config);
    Serial.println("I2S input started");
}

static void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED)
    {
        Serial.println("WebSocket connected");
        s_wsConnected = true;
    }
    else if (type == WStype_DISCONNECTED)
    {
        Serial.println("WebSocket disconnected");
        s_wsConnected = false;
    }
}

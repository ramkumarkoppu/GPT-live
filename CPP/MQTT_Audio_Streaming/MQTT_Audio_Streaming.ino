#include <AudioTools.h>        // For handling I2S audio and WAV processing
#include <WiFi.h>              // For WiFi connectivity
#include <ArduinoMqttClient.h> // For MQTT communication

/* MQTT broker configuration */
constexpr char *kMqttBroker{"192.168.1.123"};    // Raspberry PI IP address where the MQTT service is running.
constexpr char *kMqttTopic{"xvf3800/audio.wav"}; // Topic to publish audio to
constexpr uint16_t kMqttPort{1883};              // Standard MQTT port

constexpr uint16_t kMqttPacketSize{1024U};  // Size of each MQTT data packet (in bytes)
constexpr uint16_t kMqttNumOfPackets{375U}; // Number of packets to send (~3 seconds of audio at 16KHz)
constexpr uint8_t kWifiConnectRetryCnt{5U};

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};

static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample};
static I2SStream s_i2s_in{};   // I2S audio input stream
static I2SConfig s_i2s_config; // I2S hardware configuration

// Create MQTT and Wifi clients
WiFiClient s_wifiClient{};
MqttClient s_mqttClient{s_wifiClient};

// Stream that encodes audio in WAV format and sends it via MQTT.
EncodedAudioStream s_out_stream(&s_mqttClient, new WAVEncoder());
StreamCopy s_copier(s_out_stream, s_i2s_in, kMqttPacketSize); // Handles copying I2S data into the MQTT stream

static void connectWifi(void);
static void setupI2SInput(void);
static void connectMQTT(void);

void setup()
{
    // Setup UART for debug prints
    Serial.begin(115200);
    while (!Serial)
        ;

    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    connectWifi();
    connectMQTT();
    setupI2SInput();
    s_out_stream.begin(s_info); // Initialize WAV encoder with audio format.
    // Start a new MQTT message and reserve enough space for the full audio stream.
    s_mqttClient.beginMessage(kMqttTopic, kMqttPacketSize * kMqttNumOfPackets, true);
    // Copy audio from I2S microphone into MQTT stream.
    s_copier.copyN(kMqttNumOfPackets); // Copies a fixed number of packets (~3 seconds of audio).
    s_mqttClient.endMessage();         // Finalize the MQTT message and send it.
    Serial.println("Audio stream sent via MQTT!");
}

void loop()
{
    s_mqttClient.poll(); // Keep MQTT connection alive (important if broker expects pings)
    delay(5000);
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

static void connectMQTT(void)
{
    s_mqttClient.setId("xvf3800_stream");
    Serial.printf("Connecting to MQTT broker: %s\n", kMqttBroker);

    if (!s_mqttClient.connect(kMqttBroker, kMqttPort))
    {
        Serial.print("MQTT connection failed! code: ");
        Serial.println(s_mqttClient.connectError());
        while (true)
            ; // Stop here if connection fails
    }

    Serial.println("Connected to MQTT broker!");
}

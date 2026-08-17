#include "AudioTools.h"

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};

static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample}; // Stereo, 32-bit
static I2SStream s_i2s{};
static I2SConfig s_cfg{};
static uint8_t s_buffer[kSampleRate_hz * kChannelCount * (kBitsPerSample / 8)];

void setup()
{
    /* Setup UART for debugging */
    Serial.begin(115200);
    // Wait for it to initialization complete.
    while (!Serial)
        ;
    // Attach Audio Tools library logger to our UART.
    //AudioLogger::instance().begin(Serial, AudioLogger::Info);

    /* Setup I2S interface */
    Serial.println("Starting I2S...");
    s_cfg = s_i2s.defaultConfig(TX_MODE);
    s_cfg.copyFrom(s_info);
    // Custom I2S output pins
    s_cfg.pin_bck = 8;
    s_cfg.pin_ws = 7;
    s_cfg.pin_data = kI2sTxPin;    // Tx data pin
    s_cfg.pin_data_rx = kI2sRxPin; // Rx data pin
    s_cfg.is_master = true;        // our uc is master of I2S
    s_i2s.begin(s_cfg);
    Serial.println("started...");
}

void loop()
{
    size_t bytes_read{0U};
    size_t bytes_write{0U};

    s_i2s.end();

    // Record audio into the local buffer
    s_cfg.rx_tx_mode = RX_MODE;
    s_i2s.begin(s_cfg);
    bytes_read = s_i2s.readBytes(s_buffer, sizeof(s_buffer));
    Serial.printf("bytes_read: %u\n", bytes_read);
    s_i2s.end();

    // Transmit recored audio to the I2s device
    s_cfg.rx_tx_mode = TX_MODE;
    s_i2s.begin(s_cfg);
    bytes_write = s_i2s.write(s_buffer, bytes_read);
    Serial.printf("bytes_write: %u\n", bytes_write);
    s_i2s.end();

    while(true);
}

#include "AudioTools.h"

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint16_t kFrequency_hz{440}; // Square wave
constexpr int16_t kAmplitude{500};     // peak value
constexpr uint16_t kHalfWaveLength{kSampleRate_hz / kFrequency_hz};
constexpr uint16_t kNoOfSamples{32000U};

static AudioInfo s_info{kSampleRate_hz, 2, 32}; // Stereo, 32-bit
static I2SStream s_i2s{};
static I2SConfig s_cfg{};
static int32_t s_sample{kAmplitude};
static uint16_t s_count{0U};

static void printSamplesAndCount(int32_t& nonZero);

void setup()
{
    /* Setup UART for debugging */
    Serial.begin(115200);
    // Wait for it to initialization complete.
    while (!Serial)
        ;
    // Attach Audio Tools library logger to our UART.
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    /* Setup I2S interface */
    s_cfg = s_i2s.defaultConfig(RXTX_MODE); // full duplex
    s_cfg.copyFrom(s_info);
    s_cfg.pin_bck = 8;
    s_cfg.pin_ws = 7;
    s_cfg.pin_data = kI2sTxPin;    // Tx data pin
    s_cfg.pin_data_rx = kI2sRxPin; // Rx data pin
    s_cfg.is_master = true;        // our uc is master of I2S
    s_i2s.begin(s_cfg);

    Serial.println("I2S full-duplex test start");
}

void loop()
{
    /* 1. Generate and write 32K samples of square waves */
    for (uint16_t i{0U}; i < kNoOfSamples; i++)
    {
        if ((s_count % kHalfWaveLength) == 0)
        {
            s_sample = -s_sample; // Toggle polarity for sqaure wave.
        }
        s_i2s.write(reinterpret_cast<uint8_t const*>(&s_sample), sizeof(s_sample));
        s_count++;
    }

    /* 2. First read attempt */
    int32_t nonZero{0};
    Serial.println("First read attempt:");
    printSamplesAndCount(nonZero);
    Serial.printf("Valid samples: %d\n", nonZero);

    /* 3. Check pass/fail or do second attempt */
    if (nonZero > kSampleRate_hz)
    {
        Serial.println("I2S Rx Pass");
    }
    else
    {
        Serial.println("Valid sample below threshold, trying second read...");
        nonZero = 0;
        Serial.println("Second read attempt:");
        printSamplesAndCount(nonZero);
        Serial.printf("Valid samples: %d\n", nonZero);
        if (nonZero > kSampleRate_hz)
        {
            Serial.println("I2S Rx Pass");
        }
        else
        {
            Serial.println("I2S Rx Fail");
        }
    }
    Serial.println("Test complete");
    while (true)
        ; // stop here
}

static void printSamplesAndCount(int32_t& nonZero)
{
    nonZero = 0;
    bool truncated{false};

    for (uint16_t i{0U}; i < kNoOfSamples; i++)
    {
        int32_t rxSample;
        size_t n = s_i2s.readBytes(reinterpret_cast<uint8_t*>(&rxSample), sizeof(rxSample));
        if (n == sizeof(rxSample))
        {
            if ((rxSample != 0) && (rxSample != 0xFFFFFFFF))
            {
                nonZero++;
            }
            if (i < 200)
            {
                Serial.printf("%d ", rxSample);
            }
            else if (!truncated)
            {
                Serial.println("... (truncated)");
                truncated = true;
            }
        }
    }
    Serial.println();
}

#include "AudioTools.h"
#include <Wire.h>

// AIC3104 I2c address
#define AIC3104_ADDR (0x18)

// Register addresses
#define AIC3104_PAGE_CTRL (0x00)
#define AIC3104_LEFT_DAC_VOLUME (0x2B)
#define AIC3104_RIGHT_DAC_VOLUME (0x2C)
#define AIC3104_HPLOUT_LEVEL (0x33)
#define AIC3104_HPROUT_LEVEL (0x41)
#define AIC3104_LEFT_LOP_LEVEL (0x56)
#define AIC3104_RIGHT_LOP_LEVEL (0x5D)

constexpr uint8_t kI2sTxPin{44}; // I2S Tx data pin
constexpr uint8_t kI2sRxPin{43}; // I2S Rx data pin

constexpr uint16_t kSampleRate_hz{16000};
constexpr uint8_t kChannelCount{2U}; // Stereo
constexpr uint8_t kBitsPerSample{32U};
constexpr int32_t kAmplitude{655360000}; // 10000 << 16: ~-10dBFS in 32-bit, headroom for +9dB analog boost.
constexpr uint8_t kVolumeMin{0U};
constexpr uint8_t kVolumeMax{17U};
constexpr uint8_t kVolumeDefault{8U};

static AudioInfo s_info{kSampleRate_hz, kChannelCount, kBitsPerSample}; // 16khZ, Stereo, 16-bit
static SineWaveGenerator<int32_t> s_sineWave{kAmplitude};
static GeneratedSoundStream<int32_t> s_sound{s_sineWave};
static I2SStream s_i2s{};
static StreamCopy s_copier(s_i2s, s_sound);
// Volume: range 0-17 (0-8 = DAC, 9-17 = analog boost)
static uint8_t s_volume{kVolumeDefault};
static bool s_useHPOUT{true}; // true = use HPLOUT, false = use LOP
static bool s_muted{false};

static void aic3204_reg_write(uint8_t reg, uint8_t val);
static void setupAIC3104(void);
static void setVolume(uint8_t volume);

void setup()
{
    // Setup UART for debug prints and input for volume controls and audio output device selection
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("Type either + or - to control volume levels, and m to switch btween headphone and lineout");

    // Setup Audio code on I2C
    setupAIC3104();
    setVolume(s_volume);

    auto config = s_i2s.defaultConfig(TX_MODE);
    config.copyFrom(s_info);
    config.pin_bck = 8;
    config.pin_ws = 7;
    config.pin_data = kI2sTxPin;
    config.is_master = true;
    s_i2s.begin(config);
    s_sineWave.begin(s_info, N_A4); // 440Hz tone
}

void loop()
{
    s_copier.copy();

    if (Serial.available())
    {
        char c = Serial.read();
        switch (c)
        {
        case 's':
            // toggle mute
            s_muted = !s_muted;
            if (s_muted)
            {
                aic3204_reg_write(AIC3104_LEFT_DAC_VOLUME, 0x80);  // bit 7 set = DAC muted.
                aic3204_reg_write(AIC3104_RIGHT_DAC_VOLUME, 0x80); // bit 7 set = DAC muted.
                Serial.println("Muted");
            }
            else
            {
                setVolume(s_volume); // reqrites the DAC volume registers, clearing the mute bit.
                Serial.println("Unmuted");
            }
            break;
        case '+':
            if (s_volume < kVolumeMax)
            {
                setVolume(s_volume + 1);
            }
            break;
        case '-':
            if (s_volume > kVolumeMin)
            {
                setVolume(s_volume - 1);
            }
            break;
        case 'm':
            s_useHPOUT = !s_useHPOUT;
            setVolume(s_volume);
            Serial.print("Switched to ");
            Serial.println(s_useHPOUT ? "HPLOUT (headphone)" : "LOP (line out)");
            break;
        case '\r':
        case '\n':
            break; // line endings from the terminal - ignore
        default:
            Serial.println("Type either + or - to control volume levels, and m to switch btween headphone and lineout");
            break;
        }
    }
}

static void aic3204_reg_write(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(AIC3104_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static void setupAIC3104(void)
{
    Wire.begin();
    aic3204_reg_write(AIC3104_PAGE_CTRL, 0x00);

    // Set default 0dB DAC volume.
    aic3204_reg_write(AIC3104_LEFT_DAC_VOLUME, 0x00);
    aic3204_reg_write(AIC3104_RIGHT_DAC_VOLUME, 0x00);

    // Set output to 0dB, unmuted, powered up
    aic3204_reg_write(AIC3104_HPLOUT_LEVEL, 0x0D);
    aic3204_reg_write(AIC3104_HPROUT_LEVEL, 0x0D);
    aic3204_reg_write(AIC3104_LEFT_LOP_LEVEL, 0x0B);
    aic3204_reg_write(AIC3104_RIGHT_LOP_LEVEL, 0x0B);
}

static void setVolume(uint8_t volume)
{
    volume = constrain(volume, kVolumeMin, kVolumeMax);
    s_volume = volume;
    s_muted = false; // writing the DAC volume registers below clears the hardware mute bit

    if (volume <= kVolumeDefault)
    {
        // DAC attenuation
        uint8_t dacVal{static_cast<uint8_t>((kVolumeDefault - volume) * 9)}; // volume 0 = -36dB ...volume 8 = 0dB
        aic3204_reg_write(AIC3104_LEFT_DAC_VOLUME, dacVal);
        aic3204_reg_write(AIC3104_RIGHT_DAC_VOLUME, dacVal);

        // Output level fixed to 0dB
        aic3204_reg_write(AIC3104_HPLOUT_LEVEL, 0x0D);
        aic3204_reg_write(AIC3104_HPROUT_LEVEL, 0x0D);
        aic3204_reg_write(AIC3104_LEFT_LOP_LEVEL, 0x0B);
        aic3204_reg_write(AIC3104_RIGHT_LOP_LEVEL, 0x0B);
    }
    else
    {
        // DAC to 0dB
        aic3204_reg_write(AIC3104_LEFT_DAC_VOLUME, 0x00);
        aic3204_reg_write(AIC3104_RIGHT_DAC_VOLUME, 0x00);

        // Boost output gain via HPLOUT or LOP
        uint8_t gain{static_cast<uint8_t>(volume - kVolumeDefault)}; // from +1 to +9dB
        uint8_t outVal{static_cast<uint8_t>((gain << 4) | 0x0B)};    // Set gain and power/mute bits.

        if (s_useHPOUT)
        {
            aic3204_reg_write(AIC3104_HPLOUT_LEVEL, outVal);
            aic3204_reg_write(AIC3104_HPROUT_LEVEL, outVal);
        }
        else
        {
            aic3204_reg_write(AIC3104_LEFT_LOP_LEVEL, outVal);
            aic3204_reg_write(AIC3104_RIGHT_LOP_LEVEL, outVal);
        }
    }

    // Debug info
    Serial.print("Volume set to ");
    Serial.print(s_volume);
    Serial.print(" (");
    if (volume <= kVolumeDefault)
    {
        Serial.print("-" + String((kVolumeDefault - volume) * 4.5) + "dB");
    }
    else
    {
        Serial.print("+" + String((volume - kVolumeDefault)) + "dB");
    }
    Serial.println(")");
}

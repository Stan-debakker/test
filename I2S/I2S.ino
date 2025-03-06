/*
  I2S Audio Sweep Generator
  I2SSweepDemoBM3.ino
  Generate a Frequency Sweep using I2S
  Requires ESP32 with PSRAM enables (ESP32 WROOM used in demo)
  Requires AdafruitMAX9857A I2S Amplifier or equivilent
  
  DroneBot Workshop 2024
  https://dronebotworkshop.com
*/

// Include drivers & libraries
#include <driver/i2s_std.h>
#include <cmath>

// I2S configuration
#define I2S_SAMPLE_RATE 44100  // Now that we have PSRAM, we can use a higher sample rate
#define I2S_PORT I2S_NUM_0
#define I2S_BCK_IO GPIO_NUM_26  // BCK pin
#define I2S_WS_IO GPIO_NUM_25   // WS pin
#define I2S_DO_IO GPIO_NUM_22   // Data Out pin

// Audio settings
#define SWEEP_START_FREQ 400.0f
#define SWEEP_END_FREQ 2000.0f
#define SWEEP_DURATION 3.0f  // Duration of the sweep in seconds

// I2S driver handle
i2s_chan_handle_t i2s_handle;

// Function to Initialize I2S
bool init_i2s() {

  // Configure I2S Channel 0 in Master Mode
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);

  // Create new I2S Channel Handle
  esp_err_t ret_val = i2s_new_channel(&chan_cfg, &i2s_handle, NULL);
  if (ret_val != ESP_OK) {
    Serial.printf("Error creating I2S channel: %s\n", esp_err_to_name(ret_val));
    return false;
  }

  // Clock settings, slot configuration (16-bit mono), and GPIO pins for BCK, WS, and DOUT
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = I2S_BCK_IO,
      .ws = I2S_GPIO_UNUSED,//I2S_WS_IO,
      .dout = I2S_DO_IO,
      .din = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  // Initialize the channel in standard (Philips) mode.
  ret_val = i2s_channel_init_std_mode(i2s_handle, &std_cfg);
  if (ret_val != ESP_OK) {
    Serial.printf("Error initializing I2S standard mode: %s\n", esp_err_to_name(ret_val));
    return false;
  }

  return true;
}

void setup() {

  // Start Serial Monitor
  Serial.begin(115200);

  // Initialize I2S
  if (init_i2s()) {
    Serial.println("\t\tI2S Initialized Successfully");
    i2s_channel_enable(i2s_handle);
  } else {
    Serial.println("\t\tFailed to Initialize I2S");
  }
}

void loop() {

  // Establish Frequency and Phase at beginning of sweep
  static float currentFreq = SWEEP_START_FREQ;
  static float phase = 0.0f;

  // Calculate the number of samples for the entire sweep
  uint32_t numSamples = (uint32_t)(SWEEP_DURATION * I2S_SAMPLE_RATE);

  // Allocate memory in PSRAM
  int16_t *samples = (int16_t *)heap_caps_malloc(numSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (samples == NULL) {
    Serial.println("PSRAM allocation failed!");
    return;  // Handle the error appropriately
  }

  for (uint32_t i = 0; i < numSamples; i++) {
    // Linear frequency sweep
    currentFreq = SWEEP_START_FREQ + (SWEEP_END_FREQ - SWEEP_START_FREQ) * (float)i / numSamples;

    // Generate a sine wave sample
    samples[i] = (int16_t)(0.5 * 32767.0f * sin(phase));

    // Update the phase
    phase += 2.0f * PI * currentFreq / I2S_SAMPLE_RATE;
    if (phase > 2.0f * PI) {
      phase -= 2.0f * PI;
    }
  }

  // Write samples to I2S
  size_t bytesWritten;
  esp_err_t err = i2s_channel_write(i2s_handle, samples, numSamples * sizeof(int16_t), &bytesWritten, 1000);
  if (err != ESP_OK) {
    Serial.printf("I2S write error: %s\n", esp_err_to_name(err));
  }
  if (bytesWritten != numSamples * sizeof(int16_t)) {
    Serial.println("I2S write timeout or incomplete write");
  }

  free(samples);  // Free the PSRAM
}
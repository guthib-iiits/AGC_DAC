#include <BluetoothA2DPSink.h>
#include "esp_bt.h"

// ===== RGB LED PINS =====
#define LED_R 14
#define LED_G 12
#define LED_B 13

// PWM channels
#define CH_R 0
#define CH_G 1
#define CH_B 2
#define LEDC_FREQ 12000
#define LEDC_RES 8  // 0–255

// Buttons
#define BTN_PLAY 32
#define BTN_NEXT 33

BluetoothA2DPSink a2dp_sink;

bool isPlaying = false;
bool isConnected = false;
unsigned long bootTime;

// ===== SET RGB COLOR =====
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(CH_R, r);
  ledcWrite(CH_G, g);
  ledcWrite(CH_B, b);
}

void led_red()   { setColor(255, 0,   0); }
void led_blue()  { setColor(0,   0, 255); }
void led_green() { setColor(0, 255,   0); }
void led_off()   { setColor(0,   0,   0); }

// ===== BREATHING BLUE =====
void breathingBlue() {
  static int x = 0;
  static int dir = 2;
  x += dir;
  if (x >= 255 || x <= 0) dir = -dir;
  setColor(0, 0, x);
  delay(8);
}

// ===== GREEN PULSE VISUALIZER =====
void greenPulse() {
  static int g = 0;
  static int dir = 4;
  g += dir;
  if (g >= 255 || g <= 0) dir = -dir;
  setColor(0, g, 0);
  delay(10);
}

// ===== RAINBOW CYCLE =====
void rainbowCycle() {
  static uint16_t hue = 0;
  hue = (hue + 3) % 1536;

  uint8_t r, g, b;
  uint16_t h = hue;

  if (h < 256) { r = 255; g = h; b = 0; }
  else if (h < 512) { r = 511-h; g = 255; b = 0; }
  else if (h < 768) { r = 0; g = 255; b = h-512; }
  else if (h < 1024) { r = 0; g = 1023-h; b = 255; }
  else if (h < 1280) { r = h-1024; g = 0; b = 255; }
  else { r = 255; g = 0; b = 1535-h; }

  setColor(r, g, b);
  delay(5);
}

// ===== I2S CONFIG =====
void setup_i2s() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 12,
    .dma_buf_len = 128,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num   = 26,
    .ws_io_num    = 25,
    .data_out_num = 27,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  a2dp_sink.set_i2s_config(cfg);
  a2dp_sink.set_pin_config(pins);
}

// ===== CONNECTION CALLBACK (bool) =====
void connection_callback(bool connected) {
  isConnected = connected;

  if (!connected) {
    isPlaying = false;
  }
}

// ===== AUDIO STATE CALLBACK =====
void audio_state_callback(esp_a2d_audio_state_t state, void* obj) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED)
    isPlaying = true;
  else
    isPlaying = false;
}

// ===== BUTTONS =====
void handleButtons() {
  static unsigned long pressStart = 0;

  // NEXT / PREVIOUS (long press)
  if (!digitalRead(BTN_NEXT)) {
    if (pressStart == 0) pressStart = millis();

    if (millis() - pressStart > 1000) {
      a2dp_sink.previous();
      delay(600);
    }
  } else {
    if (pressStart != 0 && millis() - pressStart < 1000) {
      a2dp_sink.next();
    }
    pressStart = 0;
  }

  // PLAY/PAUSE
  if (!digitalRead(BTN_PLAY)) {
    if (isPlaying) {
      a2dp_sink.pause();
      isPlaying = false;
    } else {
      a2dp_sink.play();
      isPlaying = true;
    }
    delay(250);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  bootTime = millis();

  // LED PWM setup
  ledcSetup(CH_R, LEDC_FREQ, LEDC_RES);
  ledcSetup(CH_G, LEDC_FREQ, LEDC_RES);
  ledcSetup(CH_B, LEDC_FREQ, LEDC_RES);

  ledcAttachPin(LED_R, CH_R);
  ledcAttachPin(LED_G, CH_G);
  ledcAttachPin(LED_B, CH_B);

  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);

  setup_i2s();

  a2dp_sink.set_task_core(0);
  a2dp_sink.set_avrc_connection_state_callback(connection_callback);
  a2dp_sink.set_on_audio_state_changed(audio_state_callback);

  a2dp_sink.start("Sripathy's DAC");
}

// ===== LOOP =====
void loop() {
  handleButtons();

  //---------------------------------------
  // MODE C — LED STATE MACHINE
  //---------------------------------------

  if (!isConnected) {
    // FIRST 5 SECONDS → RAINBOW PAIRING
    if (millis() - bootTime < 5000)
      rainbowCycle();
    else
      led_red();  // IDLE DISCONNECTED
  }
  else if (isConnected && !isPlaying) {
    breathingBlue();   // PAUSED
  }
  else {
    greenPulse();      // PLAYING
  }
}

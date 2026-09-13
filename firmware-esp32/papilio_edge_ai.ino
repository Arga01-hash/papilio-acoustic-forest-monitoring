#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// Library TensorFlow Lite Micro (Native ESP32)
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model_data.h"
#include "scaler_data.h"

// ==========================================
// 1. KONFIGURASI LORA RA-02 (ESP32)
// ==========================================
#define NODE_ID       "NODE_01"
#define LORA_FREQ     433E6
#define LORA_SS       5
#define LORA_RST      14
#define LORA_DIO0     4

// ==========================================
// 2. KONFIGURASI GPS NEO-6M (ESP32 UART2)
// ==========================================
#define GPS_RX_PIN    16 // Terhubung ke TX GPS
#define GPS_TX_PIN    17 // Terhubung ke RX GPS
#define BANTIMURUNG_LAT "-5.016222"
#define BANTIMURUNG_LON "119.685667"

TinyGPSPlus gps;
HardwareSerial SerialGPS(2);

// ==========================================
// 3. KONFIGURASI INMP441 (I2S) + NOISE GATE
// ==========================================
#define I2S_WS        27
#define I2S_SD        33
#define I2S_SCK       32
#define I2S_PORT      I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_BUFFER_SAMPLES 2048 

#define GAIN_BOOSTER          2.5f   // Penguat amplitudo digital
#define NOISE_GATE_THRESHOLD  2500.0f // Ambang volume AC minimal agar AI aktif

// ==========================================
// 4. KONFIGURASI TENSORFLOW LITE & FFT
// ==========================================
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;
  
  constexpr int kTensorArenaSize = 16 * 1024;
  uint8_t tensor_arena[kTensorArenaSize];
}

#define FFT_SIZE 512
double vReal[FFT_SIZE];
double vImag[FFT_SIZE];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SIZE, 16000);

#define NUM_FEATURES 26

unsigned long lastSendTime = 0;
const long intervalAlert = 10000; // Minimal jeda 10 detik antar pengiriman LoRa
int noLockCount = 0;

// ==========================================
// INIT I2S DRIVER
// ==========================================
void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[SYSTEM] INMP441 I2S Ready!");
}

// ==========================================
// INIT TFLITE MICRO
// ==========================================
void initTFLite() {
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  model = tflite::GetModel(model_data);
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  interpreter->AllocateTensors();
  input = interpreter->input(0);
  output = interpreter->output(0);
  Serial.println("[SYSTEM] Model AI TFLite Ready!");
}

// ==========================================
// EKSTRAKSI 26 FITUR MFCC
// ==========================================
void calculateMFCC26(const int16_t* audioBuffer, int bufferLength, float features_out[26]) {
  float mfcc_frames[10][13];
  int total_frames = 0;

  for (int offset = 0; offset + FFT_SIZE <= bufferLength && total_frames < 10; offset += 256) {
    for (int i = 0; i < FFT_SIZE; i++) {
      vReal[i] = (double)audioBuffer[offset + i];
      vImag[i] = 0.0;
    }

    FFT.dcRemoval();

    for (int i = 0; i < FFT_SIZE; i++) {
      double window = 0.54 - 0.46 * cos((2 * M_PI * i) / (FFT_SIZE - 1));
      vReal[i] *= window;
    }

    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    for (int m = 0; m < 13; m++) {
      double energy = 0.0;
      int startBin = m * 15 + 1;
      int endBin = startBin + 15;
      if (endBin >= (FFT_SIZE / 2)) endBin = (FFT_SIZE / 2);

      for (int b = startBin; b < endBin; b++) {
        energy += (vReal[b] * vReal[b]); 
      }
      mfcc_frames[total_frames][m] = log(fmax(energy, 1e-6));
    }
    total_frames++;
  }

  for (int m = 0; m < 13; m++) {
    float sum = 0.0;
    for (int f = 0; f < total_frames; f++) sum += mfcc_frames[f][m];
    float mean = sum / (total_frames > 0 ? total_frames : 1);
    features_out[m] = mean;

    float variance_sum = 0.0;
    for (int f = 0; f < total_frames; f++) {
      variance_sum += pow(mfcc_frames[f][m] - mean, 2);
    }
    features_out[m + 13] = sqrt(variance_sum / (total_frames > 0 ? total_frames : 1));
  }
}

// ==========================================
// MEMBACA AUDIO LIVE & ELIMINASI DC OFFSET
// ==========================================
bool readLiveMFCC(float features_out[26]) {
  static int32_t i2sRawBuffer[I2S_BUFFER_SAMPLES];
  static int16_t audioBuffer[I2S_BUFFER_SAMPLES];
  size_t bytesRead = 0;

  i2s_read(I2S_PORT, (char*)i2sRawBuffer, sizeof(i2sRawBuffer), &bytesRead, portMAX_DELAY);
  int samplesRead = bytesRead / sizeof(int32_t);

  if (samplesRead <= 0) return false;

  long long totalRaw = 0;
  for (int i = 0; i < samplesRead; i++) {
    int32_t sample = i2sRawBuffer[i] >> 14; 
    audioBuffer[i] = (int16_t)sample;
    totalRaw += audioBuffer[i];
  }

  int16_t dcOffset = totalRaw / samplesRead;

  long long sumAbsAC = 0;
  for (int i = 0; i < samplesRead; i++) {
    int32_t cleanSample = audioBuffer[i] - dcOffset; 
    cleanSample = (int32_t)(cleanSample * GAIN_BOOSTER);

    if (cleanSample > 32767) cleanSample = 32767;
    if (cleanSample < -32768) cleanSample = -32768;

    audioBuffer[i] = (int16_t)cleanSample;
    sumAbsAC += abs(audioBuffer[i]);
  }

  float avgVolume = (float)sumAbsAC / samplesRead;

  Serial.print("[AUDIO LEVEL] Volume AC: ");
  Serial.print(avgVolume, 2);
  Serial.print(" | DC Offset Dibuang: ");
  Serial.println(dcOffset);

  if (avgVolume < NOISE_GATE_THRESHOLD) {
    Serial.println("🤫 [NOISE GATE] Suasana Senyap -> Skip AI.");
    return false;
  }

  calculateMFCC26(audioBuffer, samplesRead, features_out);
  return true;
}

// ==========================================
// INFERENSI AI
// ==========================================
bool classifyAudioFeatures(float raw_features[26]) {
  for (int i = 0; i < NUM_FEATURES; i++) {
    float normalized = (raw_features[i] - scaler_mean[i]) / scaler_scale[i];
    input->data.f[i] = normalized;
  }

  if (interpreter->Invoke() != kTfLiteOk) {
    return false;
  }

  float chainsawScore = output->data.f[0];
  Serial.print("[AI INFERENCE] Probabilitas Chainsaw: ");
  Serial.print(chainsawScore * 100, 2);
  Serial.println("%");

  return (chainsawScore >= 0.90);
}

// ==========================================
// PENGIRIMAN ALERT LORA
// ==========================================
void sendAlert(String detectionType) {
  String latitudeStr = BANTIMURUNG_LAT;
  String longitudeStr = BANTIMURUNG_LON;

  if (gps.location.isValid() && gps.location.age() < 2000) {
    latitudeStr  = String(gps.location.lat(), 6);
    longitudeStr = String(gps.location.lng(), 6);
  }

  String payload = "ALERT," + String(NODE_ID) + "," + detectionType + "," + latitudeStr + "," + longitudeStr;
  Serial.println("[LORA TRANSMIT] Memancarkan Payload: " + payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  
  // ✅ PERBAIKAN: Ubah jadi Synchronous (Menunggu sampai sinyal radio tuntas terkirim 100%)
  LoRa.endPacket(true); 

  delay(2000);
i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[SYSTEM] Alert sukses dikirim.");
}

// ==========================================
// SETUP & LOOP NODE
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("     P.A.P.I.L.I.O END-NODE LIVE SYSTEM   ");
  Serial.println("==========================================");

  initI2S();
  initTFLite();

  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[ERROR] Inisialisasi LoRa Gagal!");
    while (1);
  }

  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0xF3);

  Serial.println("[SYSTEM] Node Siap!\n");
}

void loop() {
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  float raw_features[26];
  bool isAudioActive = readLiveMFCC(raw_features);

  static int chainsawCounter = 0;

  if (!isAudioActive) {
    if (chainsawCounter > 0) chainsawCounter--;
    Serial.println("🟢 [STATUS] Aman / Senyap.");
    Serial.println("-----------------------------\n");
    delay(150);
    return;
  }

  bool isChainsaw = classifyAudioFeatures(raw_features);

  if (isChainsaw) {
    chainsawCounter++;
    Serial.print("⚠️ [WARNING] Potensi Gergaji Terdeteksi! Count: ");
    Serial.println(chainsawCounter);
    
    if (chainsawCounter >= 4) { 
      if (millis() - lastSendTime >= intervalAlert) {
        lastSendTime = millis();
        Serial.println("🚨 [ALERT DETECTED] Gergaji Mesin Sah Terdeteksi AI!");
        sendAlert("CHAINSAW");
      }
      chainsawCounter = 0;
    }
  } else {
    if (chainsawCounter > 0) chainsawCounter--;
    Serial.println("🟢 [STATUS] Suara Terdeteksi Tapi Bukan Gergaji Mesin.");
  }

  Serial.println("-----------------------------\n");
  delay(100);
}

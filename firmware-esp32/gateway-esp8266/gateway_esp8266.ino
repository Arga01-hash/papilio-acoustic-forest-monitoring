#include <SPI.h>
#include <LoRa.h>

// ==========================================
// KONFIGURASI PIN LORA DARI DOSEN (TETAP 100%)
// ==========================================
#define LORA_SS    D8  // GPIO 15 (HSPI CS)
#define LORA_RST   D0  // GPIO 16
#define LORA_DIO0  D1  // GPIO 5

unsigned long lastHeartbeat = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED Mati bawaan ESP8266 Active-LOW

  Serial.begin(115200);
  delay(1500); // Jeda aman boot

  Serial.println("\n\n==========================================");
  Serial.println("  ESP8266 GATEWAY LORA (PA.PI.L.I.O READY)");
  Serial.println("==========================================");

  // Inisialisasi Radio LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("[ERROR] Modul LoRa Ra-02 Gagal Ditemukan!");
    while (1) {
      // Kedipkan LED cepat jika hardware ada masalah
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);
    }
  }

  // Parameter Radio Wajib Sama dengan ESP32 Node
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0xF3);

  Serial.println("[SYSTEM] LoRa Radio OK! Mendengarkan data...");
  
  // Kedip 3x tanda Siap
  for(int i=0; i<3; i++){
    digitalWrite(LED_BUILTIN, LOW); delay(100);
    digitalWrite(LED_BUILTIN, HIGH); delay(100);
  }
}

void loop() {
  // Cek paket LoRa masuk dari udara
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();

    // Kedipkan LED saat menerima data radio
    digitalWrite(LED_BUILTIN, LOW); 
    
    // Kirim data ke Raspberry Pi
    Serial.println(incoming + "," + String(rssi));
    
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Indikator Heartbeat Serial setiap 5 detik
  if (millis() - lastHeartbeat >= 5000) {
    lastHeartbeat = millis();
    Serial.println("[HEARTBEAT] Gateway ESP8266 Standby...");
  }
}

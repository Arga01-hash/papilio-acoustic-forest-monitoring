# P.A.P.I.L.I.O – Off-Grid Acoustic Forest Monitoring System

**Proactive Acoustic Protection & Intelligent LoRa IoT Observer**  
*KMIPN VIII 2026 Finalist Project – Politeknik Negeri Ujung Pandang*

---

## 🌲 Overview
P.A.P.I.L.I.O is an autonomous, off-grid IoT system engineered for tropical rainforest monitoring and real-time illegal logging detection in cellular blank-spot areas.

## 🚀 Key Features
* **On-Device Edge AI (TinyML)**: Uses INMP441 microphone + ESP32 to extract MFCC audio features for instant chainsaw detection (98% validation accuracy) without internet connectivity.
* **Sub-GHz LoRa Mesh Telemetry**: Operates on 433 MHz using Multi-Hop Relay topology to bypass dense forest canopy and karst terrain obstacles.
* **Instant Alert Gateway**: ESP8266 receiver pairs with a Raspberry Pi 4 gateway to route raw payload data to the Telegram API in under 3 seconds.
* **Low-Cost Deployment**: Built with a production cost of ~Rp 460.000 per end-node.

## 📁 Repository Structure
```text
├── firmware-esp32/       # ESP32 TinyML Audio Classification & LoRa TX
├── gateway-esp8266/      # ESP8266 LoRa RX Firmware (Serial Bridge)
├── gateway-raspberry/    # Python Gateway Script for Telegram Notification API
└── README.md 
```
## 👥 Team LoRa-Rangers
*Argazora Ziya Anindya - Hardware & Embedded Systems Engineer (@Arga01-hash)

*A. Muh. Fathur Ramadhan - Edge AI & TinyML Developer (@andimuhfathur)

*Aldi Alfriansyah - Product Design & Proposal Lead

*Advisor: Ir. Dahlia Nur, M.T.

## 🔮 Future Roadmap
* **Next-Gen Hardware Optimization**: Enhancing node durability, antenna coverage, and enclosure resilience for extreme field environments.
* **Advanced Edge AI & Multi-Event Detection**: Expanding audio and sensor classification models for broader environmental threat monitoring.
* **Dynamic Network Topology**: Upgrading transmission architecture for scalable coverage and enhanced communication reliability.
* **Sustainable Power Architecture**: Implementing modular renewable energy solutions for perpetual off-grid operation.

import serial
import time
import requests

# ==========================================
# KONFIGURASI TELEGRAM BOT & SERIAL
# ==========================================
BOT_TOKEN = "Masukkan token bot telegram anda disini"
CHAT_ID = "Masukkan chat id bot anda disini"
SERIAL_PORT = "sesuaikan serial port dengan gateway esp 8266 anda"
BAUD_RATE = 115200 

def send_telegram_alert(node_id, alert_type, lat, lon, rssi):
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

    if alert_type == "CHAINSAW":
        title = "🚨 *PERINGATAN DARURAT: PENEBANGAN LIAR!* 🪓"
    elif alert_type == "THEFT":
        title = "⚠️ *PERINGATAN: NODE DICURI / DIGANGGU!* 🚨"
    else:
        title = "📢 *NOTIFIKASI SISTEM P.A.P.I.L.I.O*"

    message = (
        f"{title}\n\n"
        f"📌 *ID Node* : `{node_id}`\n"
        f"📡 *Sinyal LoRa* : `{rssi} dBm`\n"
        f"📍 *Lokasi Presisi GPS*:\n"
        f"https://maps.google.com/?q={lat},{lon}"
    )

    payload = {
        "chat_id": CHAT_ID,
        "text": message,
        "parse_mode": "Markdown"
    }

    try:
        response = requests.post(url, json=payload, timeout=5)
        if response.status_code == 200:
            print("[TELEGRAM] Notifikasi darurat berhasil dikirim ke grup!")
        else:
            print(f"[TELEGRAM ERROR] Gagal kirim: {response.text}")
    except Exception as e:
        print(f"[NETWORK ERROR] Gagal terhubung ke Telegram API: {e}")

def main():
    print(f"[SYSTEM] Membuka Port Serial {SERIAL_PORT}...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        print("[SYSTEM] Mendengarkan data serial dari ESP8266 Gateway...\n")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Cetak seluruh log serial dari ESP8266
                    print(f"[SERIAL OUT]: {line}")

                    # Deteksi jika ada pesan ALERT dari LoRa
                    if line.startswith("ALERT"):
                        parts = line.split(",")
                        if len(parts) >= 6:
                            _, node_id, alert_type, lat, lon, rssi = parts[:6]
                            send_telegram_alert(node_id, alert_type, lat, lon, rssi)
            time.sleep(0.05)

    except serial.SerialException as e:
        print(f"[SERIAL ERROR] Periksa koneksi USB ESP8266: {e}")
    except KeyboardInterrupt:
        print("\n[SYSTEM] Gateway dihentikan.")

if __name__ == "__main__":
    main()

"""
Background USB RTC Sync Daemon for SpeedTracer
Watches for ESP32-S3 USB COM port connection and automatically syncs the clock.
Run in background during development:
    python tools/usb_time_sync_daemon.py
"""

import time
import serial
import serial.tools.list_ports

def find_esp32_port():
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if "com4" in p.device.lower() or "303a" in hwid or "usb-serial" in desc or "usb jtag" in desc or "ch340" in desc or "cp210" in desc:
            return p.device
    return None

def main():
    print("USB RTC Auto-Sync Daemon running. Plug in your device via USB...")
    last_synced_time = 0
    while True:
        try:
            port = find_esp32_port()
            if port:
                # If not synced in the last 60 seconds, sync now
                if time.time() - last_synced_time > 60:
                    try:
                        ser = serial.Serial(port, 115200, timeout=1.0)
                        epoch = int(time.time())
                        ser.write(f"CMD:SYNC_TIME {epoch}\n".encode())
                        ser.flush()
                        time.sleep(0.1)
                        resp = ser.readline().decode(errors="ignore").strip()
                        ser.close()
                        if "OK:TIME_SYNC" in resp or resp:
                            print(f"[{time.strftime('%X')}] Synced clock on {port}: {resp}")
                            last_synced_time = time.time()
                    except Exception:
                        pass
            else:
                last_synced_time = 0 # reset when disconnected so it syncs immediately on reconnect
        except Exception as e:
            pass
        time.sleep(1.5)

if __name__ == "__main__":
    main()

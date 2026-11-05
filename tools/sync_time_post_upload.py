Import("env")
import time
import serial
import serial.tools.list_ports

def post_upload_sync_time(source, target, env):
    print("\n[POST-UPLOAD] Waiting for ESP32-S3 to boot and syncing RTC time...")
    port = env.subst("$UPLOAD_PORT")
    if not port or port == "None":
        ports = [p.device for p in serial.tools.list_ports.comports() if "USB" in p.description or "COM" in p.device]
        port = ports[0] if ports else "COM4"

    # Wait for ESP32-S3 reboot after flash reset
    time.sleep(2.5)

    synced = False
    for attempt in range(12):
        try:
            ser = serial.Serial(port, 115200, timeout=0.8)
            time.sleep(0.05)
            epoch = int(time.time())
            ser.write(f"CMD:SYNC_TIME {epoch}\n".encode())
            ser.flush()
            t0 = time.time()
            while time.time() - t0 < 0.8:
                if ser.in_waiting:
                    line = ser.readline().decode(errors="ignore").strip()
                    if "OK:TIME_SYNC" in line:
                        print(f"[POST-UPLOAD] Confirmed! Device responded: {line} ({time.ctime(epoch)}) on {port}\n")
                        synced = True
                        break
            ser.close()
            if synced:
                break
        except Exception:
            pass
        time.sleep(0.4)

    if not synced:
        print(f"[POST-UPLOAD] Direct sync attempt on {port}...")
        try:
            ser = serial.Serial(port, 115200, timeout=1.5)
            epoch = int(time.time())
            ser.write(f"CMD:SYNC_TIME {epoch}\n".encode())
            ser.flush()
            time.sleep(0.2)
            resp = ser.read_all().decode(errors="ignore").strip()
            ser.close()
            print(f"[POST-UPLOAD] Sent CMD:SYNC_TIME {epoch}. Device: {resp}\n")
        except Exception as e:
            print(f"[POST-UPLOAD] Warning: {e}\n")

env.AddPostAction("upload", post_upload_sync_time)

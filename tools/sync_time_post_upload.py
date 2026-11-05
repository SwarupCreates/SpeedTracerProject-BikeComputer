Import("env")
import time
import serial
import serial.tools.list_ports

def post_upload_sync_time(source, target, env):
    print("\n[POST-UPLOAD] Waiting for ESP32-S3 USB CDC to enumerate and syncing RTC time...")
    time.sleep(2.0)
    port = env.subst("$UPLOAD_PORT")
    if not port or port == "None":
        ports = [p.device for p in serial.tools.list_ports.comports() if "USB" in p.description or "COM" in p.device]
        port = ports[0] if ports else "COM4"
    try:
        ser = serial.Serial(port, 115200, timeout=1.5)
        epoch = int(time.time())
        ser.write(f"CMD:SYNC_TIME {epoch}\n".encode())
        ser.flush()
        time.sleep(0.15)
        resp = ser.read_all().decode(errors="ignore")
        ser.close()
        print(f"[POST-UPLOAD] PC Clock synced: epoch={epoch} ({time.ctime(epoch)}) on {port}")
        if resp:
            print(f"[POST-UPLOAD] Device: {resp.strip()}\n")
    except Exception as e:
        print(f"[POST-UPLOAD] Time sync notice: {e}\n")

env.AddPostAction("upload", post_upload_sync_time)

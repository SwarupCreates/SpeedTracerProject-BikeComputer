import sys
import time
import serial
import serial.tools.list_ports

def sync_time(port=None):
    if not port:
        ports = [p.device for p in serial.tools.list_ports.comports() if "USB" in p.description or "COM" in p.device]
        port = ports[0] if ports else "COM4"
    print(f"Connecting to {port} at 115200 baud...")
    try:
        ser = serial.Serial(port, 115200, timeout=1.5)
        epoch = int(time.time())
        ser.write(f"CMD:SYNC_TIME {epoch}\n".encode())
        ser.flush()
        print(f"Sent: CMD:SYNC_TIME {epoch} ({time.ctime(epoch)})")
        time.sleep(0.15)
        resp = ser.read_all().decode(errors="ignore")
        if resp:
            print(f"Device response: {resp.strip()}")
        ser.close()
        print("Time sync complete!")
    except Exception as e:
        print(f"Error syncing time: {e}")

if __name__ == "__main__":
    p = sys.argv[1] if len(sys.argv) > 1 else None
    sync_time(p)

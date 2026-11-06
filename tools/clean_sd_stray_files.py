import serial
import time

def send_cmd(ser, cmd, timeout=2.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    ser.flush()
    start_time = time.time()
    resp = []
    while time.time() - start_time < timeout:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            resp.append(line)
            if "OK:" in line or "ERR:" in line or "END_LIST" in line:
                break
    return resp

def main():
    ser = serial.Serial('COM4', 115200, timeout=1.0)
    time.sleep(0.5)

    # First get list of files in /activities
    lines = []
    ser.reset_input_buffer()
    ser.write(b"CMD:LIST_FILES /activities\n")
    ser.flush()
    start_time = time.time()
    while time.time() - start_time < 3.0:
        l = ser.readline().decode('utf-8', errors='ignore').strip()
        if l:
            lines.append(l)
            if "END_LIST" in l:
                break

    to_delete = []
    for line in lines:
        if line.startswith("START_LIST") or line.startswith("END_LIST"):
            continue
        parts = line.split()
        if not parts:
            continue
        fname = parts[0]
        # Keep our evening ride
        if "Evening_Ride_post_service_from_bike_studio" in fname:
            print(f"KEEPING: {fname}")
            continue
        to_delete.append(f"/activities/{fname}")

    # Check /routes as well
    ser.reset_input_buffer()
    ser.write(b"CMD:LIST_FILES /routes\n")
    ser.flush()
    start_time = time.time()
    lines_routes = []
    while time.time() - start_time < 3.0:
        l = ser.readline().decode('utf-8', errors='ignore').strip()
        if l:
            lines_routes.append(l)
            if "END_LIST" in l:
                break

    for line in lines_routes:
        if line.startswith("START_LIST") or line.startswith("END_LIST"):
            continue
        parts = line.split()
        if not parts:
            continue
        fname = parts[0]
        if "Evening_Ride_post_service_from_bike_studio" in fname:
            print(f"KEEPING ROUTE: {fname}")
            continue
        to_delete.append(f"/routes/{fname}")

    print("\n--- Deleting Stray Files ---")
    for fpath in to_delete:
        cmd = f"CMD:DELETE_FILE {fpath}"
        res = send_cmd(ser, cmd)
        print(f"Deleting {fpath}: {' '.join(res)}")
        time.sleep(0.05)

    print("\n--- Verifying /activities remaining ---")
    ser.reset_input_buffer()
    ser.write(b"CMD:LIST_FILES /activities\n")
    ser.flush()
    start_time = time.time()
    while time.time() - start_time < 3.0:
        l = ser.readline().decode('utf-8', errors='ignore').strip()
        if l:
            print(l)
            if "END_LIST" in l:
                break

    print("\n--- Verifying /routes remaining ---")
    ser.reset_input_buffer()
    ser.write(b"CMD:LIST_FILES /routes\n")
    ser.flush()
    start_time = time.time()
    while time.time() - start_time < 3.0:
        l = ser.readline().decode('utf-8', errors='ignore').strip()
        if l:
            print(l)
            if "END_LIST" in l:
                break

    ser.close()

if __name__ == '__main__':
    main()

import serial
import time
import sys

def send_cmd(ser, cmd, end_marker="END_LIST", timeout=3.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    ser.flush()
    lines = []
    start_time = time.time()
    while time.time() - start_time < timeout:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            lines.append(line)
            if end_marker and end_marker in line:
                break
    return lines

def main():
    try:
        ser = serial.Serial('COM4', 115200, timeout=1.0)
    except Exception as e:
        print(f"Error opening COM4: {e}")
        return

    time.sleep(0.5)

    print("--- Listing / ---")
    for l in send_cmd(ser, "CMD:LIST_FILES /"):
        print(l)

    print("\n--- Listing /activities ---")
    for l in send_cmd(ser, "CMD:LIST_FILES /activities"):
        print(l)

    print("\n--- Listing /routes ---")
    for l in send_cmd(ser, "CMD:LIST_FILES /routes"):
        print(l)

    ser.close()

if __name__ == '__main__':
    main()

import serial
import time

ser = serial.Serial('COM4', 115200, timeout=1)
time.sleep(0.05)
ser.write(b'CMD:GPS_LOG\n')
ser.flush()

lines = []
t0 = time.time()
cur_line = bytearray()
while time.time() - t0 < 4.0:
    if ser.in_waiting:
        b = ser.read()
        if b == b'\n':
            l = cur_line.decode('ascii', errors='replace').strip()
            if l.startswith('$'):
                lines.append(l)
            cur_line = bytearray()
        elif b != b'\r':
            cur_line.extend(b)
ser.close()

# Group GSV lines
gsv_lines = [l for l in lines if 'GSV' in l]
sats = {}
for l in gsv_lines:
    parts = l.split('*')[0].split(',')
    # $GPGSV,total,num,total_sats, prn1,ele1,az1,snr1, prn2...
    if len(parts) >= 8:
        c_code = parts[0][1:3]
        c_name = {'GP': 'GPS (US)', 'GL': 'GLONASS (RU)', 'GB': 'BeiDou (CN)', 'GA': 'Galileo (EU)', 'GQ': 'QZSS (JP)'}.get(c_code, c_code)
        for i in range(4, len(parts)-3, 4):
            prn = parts[i]
            elev = parts[i+1] if parts[i+1] else '-'
            azim = parts[i+2] if parts[i+2] else '-'
            snr = parts[i+3] if (i+3 < len(parts) and parts[i+3]) else '0'
            if prn and prn.isdigit():
                sats[(c_name, int(prn))] = (elev, azim, int(snr) if snr.isdigit() else 0)

print(f"Total Unique Satellites in View: {len(sats)}")
print("="*70)
print(f"{'Constellation':<16} {'PRN':<6} {'Elevation':<11} {'Azimuth':<11} {'Signal (SNR)':<15}")
print("-"*70)
for (c_name, prn), (elev, azim, snr) in sorted(sats.items(), key=lambda x: x[1][2], reverse=True):
    bar = "#" * (snr // 2)
    strength = "Locked/Strong" if snr >= 30 else "Acquiring" if snr >= 15 else "Searching"
    print(f"{c_name:<16} #{prn:<5} {elev+' deg':<11} {azim+' deg':<11} {snr:>2} dB-Hz {bar} ({strength})")

print("\nRecent Raw NMEA Packets:")
for l in lines[-6:]:
    print(" ", l)

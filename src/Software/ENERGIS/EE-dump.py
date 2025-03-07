#!/usr/bin/env python3
import serial
import sys
import time

def main():
    # Adjust these as necessary:
    default_port = "COM9"      # On Windows, e.g. COM3; on Linux, e.g. /dev/ttyACM0 or /dev/ttyUSB0
    baud_rate = 115200         # Set your baud rate

    # Allow passing the serial port as a command line argument
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = default_port

    try:
        ser = serial.Serial(port, baud_rate, timeout=1)
    except Exception as e:
        print(f"Failed to open serial port {port}: {e}")
        sys.exit(1)

    print(f"Connected to {port} at {baud_rate} baud.")
    print("Waiting for EEPROM dump...")

    dump_lines = []
    recording = False

    try:
        while True:
            # Read a line from the serial port.
            line_bytes = ser.readline()
            if not line_bytes:
                continue
            # Decode the bytes to a string
            line = line_bytes.decode('utf-8', errors='ignore').strip()
            if line == "":
                continue

            # Check for dump markers
            if line == "EE_DUMP_START":
                recording = True
                print("Dump started...")
                continue
            elif line == "EE_DUMP_END":
                print("Dump ended.")
                break

            if recording:
                dump_lines.append(line)
                # Optionally, print the line to console
                print(line)
    except KeyboardInterrupt:
        print("Interrupted by user.")
    finally:
        ser.close()

    # Write the recorded dump to a text file.
    try:
        with open("eeprom_dump.txt", "w") as f:
            for dump_line in dump_lines:
                f.write(dump_line + "\n")
        print("EEPROM dump saved to eeprom_dump.txt")
    except Exception as e:
        print(f"Error writing to file: {e}")

if __name__ == "__main__":
    main()

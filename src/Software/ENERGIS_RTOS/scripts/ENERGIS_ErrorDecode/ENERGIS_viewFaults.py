import re
import serial
import customtkinter as ctk
import textwrap 

# ======================== Energis error code decoding ========================

# Split-out name tables / descriptions
from _error_tables import MODULE_NAMES, SEVERITY_NAMES, FID_NAMES
from _code_descriptions import EID_NAMES

EVENT_LOG_BLOCK_SIZE = 0x0200  # bytes
DESC_COL_WIDTH = 30

def decode_error_code(code: int) -> dict:
    """Decode a 16-bit Energis error code into structured fields."""
    module = (code >> 12) & 0xF
    severity = (code >> 8) & 0xF
    fid = (code >> 4) & 0xF
    eid = code & 0xF

    module_name = MODULE_NAMES.get(module, f"UNKNOWN(0x{module:X})")
    severity_name = SEVERITY_NAMES.get(severity, f"0x{severity:X}")
    fid_name = FID_NAMES.get(module, {}).get(fid, f"FID 0x{fid:X}")

    # NEW: Include severity in the lookup chain
    eid_desc = EID_NAMES.get(module, {}).get(fid, {}).get(severity, {}).get(eid, "")
    desc = f"{module_name}: {eid_desc}" if eid_desc else ""

    return {
        "code": code,
        "module": module,
        "module_name": module_name,
        "severity": severity,
        "severity_name": severity_name,
        "fid": fid,
        "fid_name": fid_name,
        "eid": eid,
        "description": desc,
    }


def extract_event_region_bytes(dump_text: str) -> list[int]:
    """
    Extract EEPROM bytes from an Energis dump.

    We ignore the header ("Addr 00 01 ..") and only take lines starting with
    "0xNNNN", then parse the 16 hex bytes after the address.
    """
    bytes_out: list[int] = []

    for line in dump_text.splitlines():
        line = line.strip()
        if not (line.startswith("0x") or line.startswith("0X")):
            continue

        parts = line.split()
        # parts[0] is the address, ignore
        for token in parts[1:]:
            if re.fullmatch(r"[0-9A-Fa-f]{2}", token):
                bytes_out.append(int(token, 16))

    if len(bytes_out) > EVENT_LOG_BLOCK_SIZE:
        bytes_out = bytes_out[:EVENT_LOG_BLOCK_SIZE]

    return bytes_out


def decode_event_log_region(byte_values: list[int]) -> tuple[int, list[int]]:
    """
    Decode a log region: pointer + ring buffer entries.

    Layout (event_log.c):
        [0..1] : uint16_t write pointer (little-endian)
        [2..]  : entries, 2 bytes each, BIG-endian 16-bit error code.

    Returns:
        pointer (int), ordered_codes (list[int]),
        ordered oldest → newest, skipping 0xFFFF / 0x0000.
    """
    if len(byte_values) < 4:
        return 0, []

    # Pointer in little-endian
    ptr = byte_values[0] | (byte_values[1] << 8)

    entries_raw: list[int] = []
    for i in range(2, len(byte_values), 2):
        if i + 1 >= len(byte_values):
            break
        hi = byte_values[i]
        lo = byte_values[i + 1]
        code = (hi << 8) | lo
        entries_raw.append(code)

    max_entries = len(entries_raw)
    if max_entries == 0:
        return ptr, []

    if ptr >= max_entries:
        ptr = 0

    ordered: list[int] = []
    for idx in range(max_entries):
        j = (ptr + idx) % max_entries
        code = entries_raw[j]
        if code in (0xFFFF, 0x0000):
            continue
        ordered.append(code)

    return ptr, ordered


# =============================== UART helper ================================

def fetch_dump_over_serial(port: str, baudrate: int, command: str, timeout: float = 3.0) -> str:
    """
    Open a serial port, send command, and read until EE_DUMP_END appears.
    """
    full_cmd = command.strip() + "\r\n"
    text_chunks: list[str] = []

    with serial.Serial(port=port, baudrate=baudrate, timeout=timeout) as ser:
        # Clear any pending crap
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        ser.write(full_cmd.encode("utf-8"))
        ser.flush()

        while True:
            line_bytes = ser.readline()  # reads until \n or timeout
            if not line_bytes:
                break

            line = line_bytes.decode("utf-8", errors="replace")
            text_chunks.append(line)

            if "EE_DUMP_END" in line:
                break

    return "".join(text_chunks)


# ================================ GUI app ==================================

class EnergisLogViewerUART(ctk.CTk):
    def __init__(self):
        super().__init__()

        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("dark-blue")

        self.title("ENERGIS Failure Memory Viewer (UART)")
        self.geometry("1200x750")
        self.minsize(900, 600)

        # Shared fonts
        self.font_mono = ctk.CTkFont(family="Consolas", size=13)
        self.font_ui = ctk.CTkFont(size=13)

        self.grid_rowconfigure(1, weight=1)

        # Weight columns for left/right frames
        self.grid_columnconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=8)

        # ---- Serial config bar ----
        cfg_frame = ctk.CTkFrame(self)
        cfg_frame.grid(row=0, column=0, columnspan=2, padx=10, pady=(10, 5), sticky="we")
        cfg_frame.grid_columnconfigure(0, weight=0)
        cfg_frame.grid_columnconfigure(1, weight=0)
        cfg_frame.grid_columnconfigure(2, weight=0)
        cfg_frame.grid_columnconfigure(3, weight=0)
        cfg_frame.grid_columnconfigure(4, weight=1)

        port_label = ctk.CTkLabel(cfg_frame, text="Port:", font=self.font_ui)
        port_label.grid(row=0, column=0, padx=(8, 4), pady=8, sticky="w")
        self.port_entry = ctk.CTkEntry(cfg_frame, width=120)
        self.port_entry.insert(0, "COM3")  # adjust to your actual port
        self.port_entry.grid(row=0, column=1, padx=(0, 12), pady=8, sticky="w")

        baud_label = ctk.CTkLabel(cfg_frame, text="Baud:", font=self.font_ui)
        baud_label.grid(row=0, column=2, padx=(0, 4), pady=8, sticky="w")
        self.baud_entry = ctk.CTkEntry(cfg_frame, width=100)
        self.baud_entry.insert(0, "115200")
        self.baud_entry.grid(row=0, column=3, padx=(0, 12), pady=8, sticky="w")

        self.status_label = ctk.CTkLabel(cfg_frame, text="", anchor="w", font=self.font_ui)
        self.status_label.grid(row=0, column=4, padx=10, pady=8, sticky="we")

        # ---- Left: raw dump ----
        left_frame = ctk.CTkFrame(self)
        left_frame.grid(row=1, column=0, padx=(10, 5), pady=(5, 10), sticky="nsew")
        left_frame.grid_rowconfigure(2, weight=1)
        left_frame.grid_columnconfigure(0, weight=1)

        left_label = ctk.CTkLabel(left_frame, text="Raw EEPROM dump from ENERGIS", font=self.font_ui)
        left_label.grid(row=0, column=0, padx=8, pady=(8, 4), sticky="w")

        button_frame = ctk.CTkFrame(left_frame)
        button_frame.grid(row=1, column=0, padx=8, pady=(0, 4), sticky="we")
        button_frame.grid_columnconfigure(0, weight=0)
        button_frame.grid_columnconfigure(1, weight=0)
        button_frame.grid_columnconfigure(2, weight=1)

        error_btn = ctk.CTkButton(button_frame, text="Read ERROR log", command=self.on_read_error)
        error_btn.grid(row=0, column=0, padx=(0, 8), pady=4, sticky="w")

        warn_btn = ctk.CTkButton(button_frame, text="Read WARNING log", command=self.on_read_warning)
        warn_btn.grid(row=0, column=1, padx=(0, 8), pady=4, sticky="w")

        clear_btn = ctk.CTkButton(button_frame, text="Clear", command=self.on_clear, fg_color="#444444")
        clear_btn.grid(row=0, column=2, padx=(0, 0), pady=4, sticky="e")

        self.raw_text = ctk.CTkTextbox(
            left_frame,
            wrap="none",
            font=self.font_mono,
        )
        self.raw_text.grid(row=2, column=0, padx=8, pady=(0, 8), sticky="nsew")

        # ---- Right: decoded ----
        right_frame = ctk.CTkFrame(self)
        right_frame.grid(row=1, column=1, padx=(5, 10), pady=(5, 10), sticky="nsew")
        right_frame.grid_rowconfigure(1, weight=1)
        right_frame.grid_columnconfigure(0, weight=1)

        right_label = ctk.CTkLabel(right_frame, text="Decoded log (oldest → newest)", font=self.font_ui)
        right_label.grid(row=0, column=0, padx=8, pady=(8, 4), sticky="w")

        self.decoded_text = ctk.CTkTextbox(
            right_frame,
            wrap="none",
            font=self.font_mono,
        )
        self.decoded_text.grid(row=1, column=0, padx=8, pady=(0, 8), sticky="nsew")

    # -------------------- GUI callbacks --------------------

    def on_clear(self):
        self.raw_text.delete("1.0", "end")
        self.decoded_text.delete("1.0", "end")
        self.status_label.configure(text="")

    def _get_serial_config(self) -> tuple[str, int]:
        port = self.port_entry.get().strip()
        baud_str = self.baud_entry.get().strip()
        if not port:
            raise ValueError("Serial port is empty")
        if not baud_str.isdigit():
            raise ValueError("Baud must be integer")
        return port, int(baud_str)

    def on_read_error(self):
        self._read_and_decode("read_error", "ERROR")

    def on_read_warning(self):
        self._read_and_decode("read_warning", "WARNING")

    def _read_and_decode(self, command: str, label: str):
        self.decoded_text.delete("1.0", "end")

        try:
            port, baud = self._get_serial_config()
        except ValueError as e:
            self.status_label.configure(text=f"Config error: {e}")
            return

        self.status_label.configure(text=f"Opening {port} @ {baud} for {label} log…")
        self.update_idletasks()

        try:
            dump_text = fetch_dump_over_serial(port, baud, command)
        except Exception as e:
            self.status_label.configure(text=f"Serial error: {e}")
            return

        if not dump_text.strip():
            self.status_label.configure(text=f"No data received for {label} log.")
            return

        self.raw_text.delete("1.0", "end")
        self.raw_text.insert("1.0", dump_text)

        bytes_list = extract_event_region_bytes(dump_text)
        if not bytes_list:
            self.status_label.configure(text=f"No EEPROM bytes detected in {label} dump.")
            self.decoded_text.insert("1.0", f"{label} log: empty or invalid dump.\n")
            return

        ptr, codes = decode_event_log_region(bytes_list)
        if not codes:
            self.status_label.configure(text=f"{label} log empty (pointer={ptr}).")
            self.decoded_text.insert("1.0", f"{label} event log is empty.\n")
            return

        self.status_label.configure(
            text=f"{label} log: pointer={ptr}, entries={len(codes)} (oldest → newest)."
        )

        lines: list[str] = []
        lines.append(f"{label} log decoded")
        lines.append(f"Pointer (next write index): {ptr}")
        lines.append(f"Entries decoded: {len(codes)}")
        lines.append("")
        lines.append("Idx  Code     Severity  Module/File               EID  Description")
        lines.append("---- -------- --------- ------------------------- ---- ------------------------------")

        for idx, code in enumerate(codes):
            info = decode_error_code(code)
            code_str = f"0x{info['code']:04X}"
            sev_str = f"[{info['severity_name']}]"
            mod_str = info["module_name"]
            file_str = info["fid_name"]
            eid_str = f"0x{info['eid']:X}"
            desc = info["description"] or ""

            base = (
                f"{idx+1:3d}  "
                f"{code_str:<8} "
                f"{sev_str:<9} "
                f"{mod_str:<7}/{file_str:<18} "
                f"{eid_str:<4} "
            )

            wrapped = textwrap.wrap(desc, width=DESC_COL_WIDTH) or [""]
            lines.append(base + wrapped[0])
            for w in wrapped[1:]:
                lines.append(" " * len(base) + w)
            lines.append("")
        self.decoded_text.insert("1.0", "\n".join(lines))


if __name__ == "__main__":
    app = EnergisLogViewerUART()
    app.mainloop()
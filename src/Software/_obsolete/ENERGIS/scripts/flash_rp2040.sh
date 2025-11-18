#!/usr/bin/env bash
# flash_rp2040.sh
set -euo pipefail

# --- Helpers ---
script_dir="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"
conf_file="${script_dir}/serial_port.linux.conf"

die() { echo "Error: $*" >&2; exit 1; }

read_config() {
  [[ -f "$conf_file" ]] || die "Config file not found: $conf_file"
  while IFS='=' read -r key val; do
    [[ -z "${key// }" || "$key" =~ ^[[:space:]]*# ]] && continue
    key="$(echo "$key" | xargs)"; val="$(echo "$val" | xargs)"
    case "$key" in
      PORT) PORT="$val" ;;
      BAUD) BAUD="$val" ;;
      FIRMWARE) FIRMWARE="$val" ;;
      PY_UPLOADER) PY_UPLOADER="$val" ;;
      LOG_DIR) LOG_DIR="$val" ;; # optional here
    esac
  done < "$conf_file"
}

set_config_value() {
  local k="$1" v="$2"
  if grep -qE "^[[:space:]]*${k}[[:space:]]*=" "$conf_file"; then
    sed -i "s|^[[:space:]]*${k}[[:space:]]*=.*|${k}=${v}|g" "$conf_file"
  else
    printf '%s=%s\n' "$k" "$v" >>"$conf_file"
  fi
}

abs_path() {
  local p="$1"
  [[ "$p" = /* ]] && { printf '%s\n' "$p"; return; }
  printf '%s\n' "$(realpath -m "$script_dir/$p")"
}

list_serial_ports() {
  local line dev vendor model product serial name
  for dev in /dev/ttyACM* /dev/ttyUSB*; do
    [[ -e "$dev" ]] || continue
    vendor="$(udevadm info -q property -n "$dev" 2>/dev/null | awk -F= '/^ID_VENDOR=/ {print $2; exit}')"
    model="$(udevadm info -q property -n "$dev" 2>/dev/null | awk -F= '/^ID_MODEL=/ {print $2; exit}')"
    product="$(udevadm info -q property -n "$dev" 2>/dev/null | awk -F= '/^ID_MODEL_FROM_DATABASE=/ {print $2; exit}')"
    serial="$(udevadm info -q property -n "$dev" 2>/dev/null | awk -F= '/^ID_SERIAL_SHORT=/ {print $2; exit}')"
    name="$dev"
    [[ -n "$product" ]] && name="$name — $product" || [[ -n "$vendor$model" ]] && name="$name — $vendor $model"
    [[ -n "$serial" ]] && name="$name (S/N: $serial)"
    printf '%s|%s\n' "$dev" "$name"
  done
}

select_serial_port() {
  local current="$1"
  mapfile -t ports < <(list_serial_ports)
  if [[ ${#ports[@]} -eq 0 ]]; then die "No serial ports detected."; fi
  local found=""
  for line in "${ports[@]}"; do
    [[ "${line%%|*}" == "$current" ]] && found="$current" && break
  done
  if [[ -n "$found" ]]; then printf '%s\n' "$found"; return; fi
  echo "Configured PORT '$current' not found."
  echo "Available ports:"
  local i=0
  for line in "${ports[@]}"; do
    echo "  [$i] ${line#*|}"
    ((i++))
  done
  local sel
  while :; do
    read -rp "Select port index: " sel
    [[ "$sel" =~ ^[0-9]+$ && "$sel" -ge 0 && "$sel" -lt ${#ports[@]} ]] && break
  done
  local chosen="${ports[$sel]%%|*}"
  set_config_value PORT "$chosen"
  echo "Using PORT '$chosen' and updated config."
  printf '%s\n' "$chosen"
}

# --- Main ---
read_config
: "${PORT:?Missing 'PORT' in config}"
: "${BAUD:?Missing 'BAUD' in config}"
: "${FIRMWARE:?Missing 'FIRMWARE' in config}"
: "${PY_UPLOADER:?Missing 'PY_UPLOADER' in config}"

PORT="$(select_serial_port "$PORT")"

FIRMWARE_ABS="$(abs_path "$FIRMWARE")"
PY_UPLOADER_ABS="$(abs_path "$PY_UPLOADER")"

[[ -f "$PY_UPLOADER_ABS" ]] || die "Python uploader not found: $PY_UPLOADER_ABS"
[[ -f "$FIRMWARE_ABS"   ]] || die "Firmware file not found: $FIRMWARE_ABS"

python3 "$PY_UPLOADER_ABS" --p "$PORT" --b "$BAUD" --f "$FIRMWARE_ABS"
echo "[OK] Flash done."

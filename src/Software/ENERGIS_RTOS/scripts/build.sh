#!/usr/bin/env bash
set -euo pipefail

# Build script for ENERGIS_RTOS on Linux
# - Configures CMake if needed
# - Builds with Ninja (preferred) or Make

script_dir="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"
project_root="$(realpath -m "${script_dir}/..")"
build_dir="${project_root}/build"

log() { echo "[build] $*"; }
die() { echo "[build:ERROR] $*" >&2; exit 1; }

# Detect pico-sdk path (use env if provided, else local workspace pico-sdk)
: "${PICO_SDK_PATH:=${project_root}/pico-sdk}"
[[ -d "${PICO_SDK_PATH}" ]] || die "PICO_SDK_PATH not found at '${PICO_SDK_PATH}'. Set env PICO_SDK_PATH to your Pico SDK."

# Generator preference: Ninja if available
GEN=""
if command -v ninja >/dev/null 2>&1; then
  GEN="-G Ninja"
  log "Using Ninja generator"
else
  log "Ninja not found; falling back to default CMake generator"
fi

mkdir -p "${build_dir}"

# Configure if missing or if CMakeCache.txt absent
if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
  log "Configuring CMake in '${build_dir}'"
  cmake -S "${project_root}" -B "${build_dir}" ${GEN} -DPICO_SDK_PATH="${PICO_SDK_PATH}" -DCMAKE_BUILD_TYPE=Release
else
  log "CMake already configured; skipping"
fi

# Build
log "Building project"
cmake --build "${build_dir}" --parallel

log "Build complete. Artifacts should be in '${build_dir}'."

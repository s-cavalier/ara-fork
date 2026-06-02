#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP="${APP:-rf_classify}"
SKIP_TOOLCHAIN_LLVM="${SKIP_TOOLCHAIN_LLVM:-0}"
SKIP_VERILATOR_INSTALL="${SKIP_VERILATOR_INSTALL:-0}"
UPDATE_HARDWARE_DEPS="${UPDATE_HARDWARE_DEPS:-0}"

# Typical first full setup:
#   UPDATE_HARDWARE_DEPS=1 ./build.sh
#
# Fast rerun after the toolchain, Verilator, and hardware deps are already built:
#   SKIP_TOOLCHAIN_LLVM=1 SKIP_VERILATOR_INSTALL=1 ./build.sh
#
# Run the simulator with:
#   ./run.sh

if [[ "${CLEAN_VERILATOR:-0}" == "1" ]]; then
  rm -rf "${ROOT_DIR}/hardware/build/verilator"
fi

echo "Initializing git submodules..."
make -C "${ROOT_DIR}" git-submodules

if [[ "${SKIP_TOOLCHAIN_LLVM}" != "1" ]]; then
  echo "Building LLVM RISC-V toolchain..."
  make -C "${ROOT_DIR}" toolchain-llvm
else
  echo "Skipping LLVM RISC-V toolchain build."
fi

if [[ "${SKIP_VERILATOR_INSTALL}" != "1" ]]; then
  echo "Building and installing Verilator..."
  make -C "${ROOT_DIR}" verilator
else
  echo "Skipping Verilator install build."
fi

if [[ "${UPDATE_HARDWARE_DEPS}" == "1" ]]; then
  echo "Checking out hardware dependencies..."
  make -C "${ROOT_DIR}/hardware" checkout

  echo "Applying hardware patches..."
  make -C "${ROOT_DIR}/hardware" apply-patches

  echo "Cleaning generated Verilator model after dependency patching..."
  rm -rf "${ROOT_DIR}/hardware/build/verilator"
else
  echo "Skipping hardware dependency checkout and patches."
fi

echo "Building app: ${APP}"
make -C "${ROOT_DIR}/apps" "bin/${APP}"

echo "Building Ara Verilator model..."
make -C "${ROOT_DIR}/hardware" verilate

echo "Build complete. Run with ./run.sh"

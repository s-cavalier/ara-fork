#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP="${APP:-rf_classify}"
RFIO_PORT="${RFIO_PORT:-9090}"
RFIO_BIND="${RFIO_BIND:-127.0.0.1}"
SKIP_TOOLCHAIN_LLVM="${SKIP_TOOLCHAIN_LLVM:-0}"
SKIP_VERILATOR_INSTALL="${SKIP_VERILATOR_INSTALL:-0}"
SKIP_HARDWARE_DEPS="${SKIP_HARDWARE_DEPS:-0}"
SKIP_RUN="${SKIP_RUN:-0}"

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

if [[ "${SKIP_HARDWARE_DEPS}" != "1" ]]; then
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

if [[ "${SKIP_RUN}" == "1" ]]; then
  echo "Skipping simv run."
  exit 0
fi

echo "Running ${APP} with RFIO at ${RFIO_BIND}:${RFIO_PORT}"
make -C "${ROOT_DIR}/hardware" \
  app="${APP}" \
  rfio_bind="${RFIO_BIND}" \
  rfio_port="${RFIO_PORT}" \
  simv

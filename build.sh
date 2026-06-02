#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP="${APP:-rf_classify}"
RFIO_PORT="${RFIO_PORT:-9090}"
RFIO_BIND="${RFIO_BIND:-127.0.0.1}"

if [[ "${CLEAN_VERILATOR:-0}" == "1" ]]; then
  rm -rf "${ROOT_DIR}/hardware/build/verilator"
fi

echo "Building Ara Verilator model..."
make -C "${ROOT_DIR}/hardware" verilate

echo "Building app: ${APP}"
make -C "${ROOT_DIR}/apps" "bin/${APP}"

echo "Running ${APP} with RFIO at ${RFIO_BIND}:${RFIO_PORT}"
make -C "${ROOT_DIR}/hardware" \
  app="${APP}" \
  rfio_bind="${RFIO_BIND}" \
  rfio_port="${RFIO_PORT}" \
  simv

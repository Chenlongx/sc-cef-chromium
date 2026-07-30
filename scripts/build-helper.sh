#!/usr/bin/env bash
# Build sc-cef-helper (CEF app bundle + Helpers on macOS when third_party/cef present).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/helper/build"
CEF_ROOT="${CEF_ROOT:-${ROOT}/third_party/cef}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"
if ! command -v "${CMAKE_BIN}" >/dev/null 2>&1; then
  if [[ -x /tmp/cmake-3.30.5-macos-universal/CMake.app/Contents/bin/cmake ]]; then
    CMAKE_BIN=/tmp/cmake-3.30.5-macos-universal/CMake.app/Contents/bin/cmake
  fi
fi

mkdir -p "${BUILD}"
CMAKE_ARGS=(-S "${ROOT}/helper" -B "${BUILD}" -DCEF_ROOT="${CEF_ROOT}" -DCMAKE_BUILD_TYPE=Release)
if [[ "${SC_FORCE_STUB:-0}" == "1" ]]; then
  CMAKE_ARGS+=(-DSC_FORCE_STUB=ON)
fi
echo "[build-helper] using ${CMAKE_BIN}"
"${CMAKE_BIN}" "${CMAKE_ARGS[@]}"
"${CMAKE_BIN}" --build "${BUILD}" --config Release -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

APP="${BUILD}/Release/sc-cef-helper.app"
[[ -d "${APP}" ]] || APP="${BUILD}/sc-cef-helper.app"
BIN="${APP}/Contents/MacOS/sc-cef-helper"
if [[ -x "${BIN}" ]]; then
  echo "[build-helper] app: ${APP}"
  ls -la "${APP}/Contents/Frameworks" | head -20
  ln -sfn "${BIN}" "${BUILD}/sc-cef-helper"
  ln -sfn "${APP}" "${BUILD}/sc-cef-helper.app"
  echo "[build-helper] binary symlink: ${BUILD}/sc-cef-helper -> ${BIN}"
elif [[ -x "${BUILD}/sc-cef-helper" ]]; then
  echo "[build-helper] binary: ${BUILD}/sc-cef-helper"
  ls -la "${BUILD}/sc-cef-helper"
else
  echo "[build-helper] ERROR: no sc-cef-helper output"
  find "${BUILD}" -name 'sc-cef-helper*' | head -40
  exit 1
fi

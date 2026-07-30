#!/usr/bin/env bash
# Apply milestone patches onto Chromium / CEF src tree.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MILESTONE="${1:-}"
SRC="${CEF_SRC:-${ROOT}/third_party/chromium}"

if [[ -z "${MILESTONE}" ]]; then
  echo "Usage: $0 M1|M2|M3"
  exit 1
fi

DIR=""
case "${MILESTONE}" in
  M1) DIR="${ROOT}/patches/M1_navigator_network" ;;
  M2) DIR="${ROOT}/patches/M2_canvas_webgl_audio" ;;
  M3) DIR="${ROOT}/patches/M3_fonts_webrtc" ;;
  *) echo "Unknown milestone: ${MILESTONE}"; exit 1 ;;
esac

if [[ ! -d "${SRC}" ]]; then
  echo "[apply-patches] Chromium/CEF src not found at ${SRC}"
  echo "Set CEF_SRC=... or run fetch-cef / checkout chromium first."
  exit 1
fi

shopt -s nullglob
patches=("${DIR}"/*.patch)
if [[ ${#patches[@]} -eq 0 ]]; then
  echo "[apply-patches] No .patch files in ${DIR} yet (scaffold only)."
  exit 0
fi

for p in "${patches[@]}"; do
  if grep -q 'TEMPLATE' "${p}" && ! grep -q '^@@ ' "${p}"; then
    echo "[apply-patches] Skipping template (regenerate later): $(basename "$p")"
    continue
  fi
  echo "[apply-patches] Applying $(basename "$p")"
  patch -d "${SRC}" -p1 < "${p}"
done
echo "[apply-patches] Done ${MILESTONE}"

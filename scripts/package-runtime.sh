#!/usr/bin/env bash
# Package sc-cef-runtime-{platform}.zip for SC-WS-CMR / R2.
#
# macOS zip layout (CEF app bundle):
#   sc-cef-helper.app/Contents/MacOS/sc-cef-helper
#   sc-cef-helper.app/Contents/Frameworks/{CEF + Helper.app…}
#   RUNTIME.json
# binaryRelativePath = sc-cef-helper.app/Contents/MacOS/sc-cef-helper
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
PLATFORM="${1:-}"

detect_platform() {
  local os arch
  os="$(uname -s | tr '[:upper:]' '[:lower:]')"
  arch="$(uname -m)"
  case "${os}" in
    darwin) [[ "${arch}" == "arm64" ]] && echo "darwin-arm64" || echo "darwin-x64" ;;
    linux) echo "linux-x64" ;;
    mingw*|msys*|cygwin*) echo "win32-x64" ;;
    *) echo "darwin-arm64" ;;
  esac
}

[[ -z "${PLATFORM}" ]] && PLATFORM="$(detect_platform)"

STAGE="${ROOT}/dist/stage-${PLATFORM}"
OUT_DIR="${ROOT}/dist/${VERSION}"
ZIP_NAME="sc-cef-runtime-${PLATFORM}.zip"
ZIP_PATH="${OUT_DIR}/${ZIP_NAME}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}" "${OUT_DIR}"

if [[ "${PLATFORM}" == darwin-* ]]; then
  APP="${ROOT}/helper/build/Release/sc-cef-helper.app"
  [[ -d "${APP}/Contents/MacOS" ]] || APP="${ROOT}/helper/build/sc-cef-helper.app"
  # Resolve symlink so zip embeds the real Frameworks + Helpers (not a 0-byte link).
  if [[ -L "${APP}" ]]; then
    APP="$(cd "${APP}" && pwd -P)"
  fi
  if [[ ! -d "${APP}/Contents/MacOS" ]]; then
    echo "[package-runtime] missing sc-cef-helper.app — run ./scripts/build-helper.sh"
    exit 1
  fi
  rm -rf "${STAGE}/sc-cef-helper.app"
  ditto "${APP}" "${STAGE}/sc-cef-helper.app"
  REL_PATH="sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
else
  BINARY_NAME="sc-cef-helper"
  [[ "${PLATFORM}" == "win32-x64" ]] && BINARY_NAME="sc-cef-helper.exe"
  REL_PATH="bin/${BINARY_NAME}"
  HELPER="${ROOT}/helper/build/${BINARY_NAME}"
  [[ -f "${HELPER}" ]] || HELPER="${ROOT}/helper/build/Release/${BINARY_NAME}"
  [[ -f "${HELPER}" ]] || { echo "helper not found"; exit 1; }
  mkdir -p "${STAGE}/bin"
  cp "${HELPER}" "${STAGE}/${REL_PATH}"
  chmod +x "${STAGE}/${REL_PATH}" 2>/dev/null || true
fi

cat > "${STAGE}/RUNTIME.json" <<EOF
{
  "version": "${VERSION}",
  "platform": "${PLATFORM}",
  "binaryRelativePath": "${REL_PATH}",
  "controlProtocol": 1,
  "fingerprintEngine": 2,
  "fingerprintEngineNative": true,
  "packagedAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

rm -f "${ZIP_PATH}"
( cd "${STAGE}" && zip -ry "${ZIP_PATH}" . )
SHA="$(shasum -a 256 "${ZIP_PATH}" | awk '{print $1}')"
SIZE="$(wc -c < "${ZIP_PATH}" | tr -d ' ')"
cat > "${OUT_DIR}/${ZIP_NAME}.meta.json" <<EOF
{
  "platform": "${PLATFORM}",
  "version": "${VERSION}",
  "file": "${ZIP_NAME}",
  "sha256": "${SHA}",
  "sizeBytes": ${SIZE},
  "binaryRelativePath": "${REL_PATH}",
  "url": "https://downloads.mediamingle.cn/mediamingle-downloads/Chromium/${VERSION}/${ZIP_NAME}"
}
EOF
echo "[package-runtime] ${ZIP_PATH}"
echo "[package-runtime] sha256=${SHA} size=${SIZE} path=${REL_PATH}"

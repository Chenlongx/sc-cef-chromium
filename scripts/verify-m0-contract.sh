#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
PLATFORM="${1:-darwin-arm64}"
ZIP="${ROOT}/dist/${VERSION}/sc-cef-runtime-${PLATFORM}.zip"
[[ -f "${ZIP}" ]] || { echo "missing ${ZIP}"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "${TMP}"' EXIT
unzip -q "${ZIP}" -d "${TMP}"
if [[ "${PLATFORM}" == darwin-* ]]; then
  BIN="sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
  [[ -f "${TMP}/${BIN}" ]] || { echo "FAIL missing ${BIN}"; exit 1; }
  [[ -d "${TMP}/sc-cef-helper.app/Contents/Frameworks/sc-cef-helper Helper.app" ]] || { echo "FAIL missing Helper.app"; exit 1; }
  [[ -d "${TMP}/sc-cef-helper.app/Contents/Frameworks/Chromium Embedded Framework.framework" ]] || { echo "FAIL missing CEF framework"; exit 1; }
else
  BIN="bin/sc-cef-helper"; [[ "${PLATFORM}" == win32-x64 ]] && BIN="bin/sc-cef-helper.exe"
  [[ -f "${TMP}/${BIN}" ]] || { echo "FAIL missing ${BIN}"; exit 1; }
fi
python3 - <<PY
import json
m=json.load(open("${ROOT}/dist/${VERSION}/sc-cef-runtime-${PLATFORM}.zip.meta.json"))
assert m["binaryRelativePath"].endswith("sc-cef-helper") or m["binaryRelativePath"].endswith(".exe")
print("[m0-contract] OK", m["url"], m["binaryRelativePath"])
PY

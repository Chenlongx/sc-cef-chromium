#!/usr/bin/env bash
# Headed detector run via CEF DevTools (requires Helper.app bundle build).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HELPER="${ROOT}/helper/build/Release/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
[[ -x "${HELPER}" ]] || HELPER="${ROOT}/helper/build/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
PY="${ROOT}/.venv-ci/bin/python"
if [[ ! -x "${PY}" ]]; then
  python3 -m venv "${ROOT}/.venv-ci"
  "${ROOT}/.venv-ci/bin/pip" install -q websocket-client
  PY="${ROOT}/.venv-ci/bin/python"
fi
[[ -x "${HELPER}" ]] || { echo "missing helper app — run ./scripts/build-helper.sh"; exit 1; }
exec "${PY}" -u "${ROOT}/scripts/headed_detector.py"

#!/usr/bin/env bash
# Phase A + B3 acceptance: headed detectors with gates; optional inject-off run.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${ROOT}/.venv-ci/bin/python"
HELPER="${ROOT}/helper/build/Release/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
[[ -x "${HELPER}" ]] || HELPER="${ROOT}/helper/build/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
[[ -x "${HELPER}" ]] || { echo "missing helper — ./scripts/build-helper.sh"; exit 1; }
if [[ ! -x "${PY}" ]]; then
  python3 -m venv "${ROOT}/.venv-ci"
  "${ROOT}/.venv-ci/bin/pip" install -q websocket-client
  PY="${ROOT}/.venv-ci/bin/python"
fi

echo "[kernel-accept] Phase A — inject ON, windowed, CDP timezone"
unset SC_CEF_WINDOWLESS || true
export SC_CEF_FP_INJECT=1
"${PY}" -u "${ROOT}/scripts/headed_detector.py"
cp "${ROOT}/dist/headed-detector-report.json" "${ROOT}/dist/headed-detector-report.phase-a.json"

echo "[kernel-accept] Phase B3 probe — inject OFF (CDP/network only on stock CEF)"
export SC_CEF_FP_INJECT=0
set +e
"${PY}" -u "${ROOT}/scripts/headed_detector.py"
inj0=$?
set -e
cp "${ROOT}/dist/headed-detector-report.json" "${ROOT}/dist/headed-detector-report.inject-off.json" || true

python3 - <<'PY'
import json
from pathlib import Path
root = Path("/Users/Admin/sc-cef-chromium/dist")
a = json.loads((root / "headed-detector-report.phase-a.json").read_text())
b_path = root / "headed-detector-report.inject-off.json"
b = json.loads(b_path.read_text()) if b_path.exists() else {}
summary = {
  "phaseA_ok": a.get("ok"),
  "phaseA_failed": a.get("failedGates"),
  "injectOff_ok": b.get("ok"),
  "injectOff_failed": b.get("failedGates"),
  "injectOff_timezone": (b.get("gates") or {}).get("timezoneMatchesConfig"),
  "note": "Full CreepJS no-lie without inject requires custom CEF (Blink 0002). "
          "Timezone should still pass via CDP Emulation on stock CEF.",
}
(root / "kernel-accept-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2))
if not a.get("ok"):
  raise SystemExit(1)
PY

echo "[kernel-accept] OK (Phase A gates). Summary: dist/kernel-accept-summary.json"

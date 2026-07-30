#!/usr/bin/env bash
# M4 detector / engine CI — seed stability + helper PROTOCOL smoke.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HELPER="${ROOT}/helper/build/sc-cef-helper"
mkdir -p "${ROOT}/dist"

if [[ ! -x "${HELPER}" ]]; then
  echo "[detector-ci] building helper..."
  bash "${ROOT}/scripts/build-helper.sh"
fi

python3 - <<PY
import hashlib, json, tempfile, subprocess, time, socket, pathlib
root = pathlib.Path("${ROOT}")

def seeded_noise(seed, index):
    x = (seed ^ (index * 0x9E3779B9)) & 0xFFFFFFFF
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= (x >> 17) & 0xFFFFFFFF
    x ^= (x << 5) & 0xFFFFFFFF
    return x & 0xFFFFFFFF

def canvas_hash(seed):
    rgba = bytearray(16 * 4)
    for i in range(0, 16, 17):
        n = seeded_noise(seed, i)
        off = i * 4
        rgba[off] ^= n & 1
        rgba[off + 1] ^= (n >> 1) & 1
        rgba[off + 2] ^= (n >> 2) & 1
    return hashlib.sha256(bytes(rgba)).hexdigest()

h1 = canvas_hash(42)
assert h1 == canvas_hash(42) and h1 != canvas_hash(43)
print("[detector-ci] canvas seed stability OK", h1[:12])

sample = {
  "version": 1, "seed": "ci-seed-1", "profileId": "ci", "templateId": "ci",
  "userAgent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
  "platform": "Win32", "vendor": "Google Inc.", "language": "en-US", "languages": ["en-US", "en"],
  "timezone": "America/New_York",
  "screen": {"width": 1920, "height": 1080, "availWidth": 1920, "availHeight": 1040, "colorDepth": 24, "pixelDepth": 24},
  "hardware": {"concurrency": 8, "deviceMemory": 8},
  "webgl": {"vendor": "Google Inc. (NVIDIA)", "renderer": "ANGLE (NVIDIA)"},
  "canvasSeed": 42, "audioSeed": 99, "fontProfile": ["Arial"], "mediaDevices": [],
  "webRtcMode": "proxy_only", "userAgentData": {"brands": [], "mobile": False, "platform": "Windows"},
  "extras": {"maxTouchPoints": 0, "doNotTrack": None, "pdfViewerEnabled": True, "cookieEnabled": True,
             "outerWidth": 1920, "outerHeight": 1080, "screenX": 0, "screenY": 0, "speechVoices": [],
             "pluginCount": 0, "permissionDefaults": {}}
}
td = tempfile.mkdtemp()
fp = pathlib.Path(td) / "fingerprint-engine.json"
fp.write_text(json.dumps(sample))
profile = pathlib.Path(td) / "profile"
profile.mkdir()
helper = root / "helper" / "build" / "sc-cef-helper"
proc = subprocess.Popen([str(helper), "--remote-debugging-port=9340"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
port = None
deadline = time.time() + 10
while time.time() < deadline and port is None:
    line = proc.stdout.readline()
    if line and "CONTROL_PORT=" in line:
        port = int(line.strip().split("=", 1)[1])
if not port:
    raise SystemExit("no CONTROL_PORT: " + (proc.stderr.read() or ""))
sock = socket.create_connection(("127.0.0.1", port), timeout=5)
req = {
    "id": "req_ci", "type": "create-browser", "accountId": 1,
    "profilePath": str(profile), "url": "about:blank",
    "bounds": {"x": 0, "y": 0, "width": 800, "height": 600},
    "fingerprintConfigPath": str(fp),
}
sock.sendall((json.dumps(req) + "\n").encode())
sock.settimeout(20)
resp = json.loads(sock.recv(65536).decode().strip().split("\n")[0])
sock.close()
print("[detector-ci] create-browser", resp)
assert resp.get("ok") is True, resp
assert resp.get("fingerprintEngineNative") is True
proc.terminate()
try:
    proc.wait(timeout=3)
except Exception:
    proc.kill()
report = {
    "ok": True,
    "canvasHash": h1,
    "createBrowser": resp,
    "cefRevision": (root / "CEF_REVISION").read_text() if (root / "CEF_REVISION").exists() else "",
    "note": "Headed CreepJS/BrowserScan still required before AdsPower marketing claims",
}
(root / "dist" / "detector-ci-report.json").write_text(json.dumps(report, indent=2) + "\n")
print("[detector-ci] PASS")
PY

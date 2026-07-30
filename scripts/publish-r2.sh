#!/usr/bin/env bash
# Build manifest.json from dist/{VERSION}/*.meta.json and optionally upload to R2.
#
# Env:
#   R2_BUCKET          — e.g. mediamingle-downloads (optional)
#   R2_PREFIX          — default Chromium
#   SKIP_UPLOAD=1      — only write local manifest
#   WRANGLER=wrangler  — CLI used when uploading
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
OUT_DIR="${ROOT}/dist/${VERSION}"
CDN_BASE="https://downloads.mediamingle.cn/Chromium"
PREFIX="${R2_PREFIX:-Chromium}"
WRANGLER="${WRANGLER:-wrangler}"

if [[ ! -d "${OUT_DIR}" ]]; then
  echo "[publish-r2] missing ${OUT_DIR} — run package-runtime.sh first"
  exit 1
fi

MANIFEST="${OUT_DIR}/manifest.json"
ARTIFACTS_JSON="[]"

shopt -s nullglob
for meta in "${OUT_DIR}"/*.meta.json; do
  ARTIFACTS_JSON="$(python3 - <<PY
import json
artifacts = json.loads('''${ARTIFACTS_JSON}''')
with open("${meta}") as f:
    m = json.load(f)
artifacts.append({
  "platform": m["platform"],
  "url": m["url"],
  "sha256": m["sha256"],
  "sizeBytes": int(m["sizeBytes"]),
  "binaryRelativePath": m["binaryRelativePath"],
})
print(json.dumps(artifacts))
PY
)"
done

python3 - <<PY
import json
from datetime import date
manifest = {
  "version": "${VERSION}",
  "controlProtocol": 1,
  "fingerprintEngine": 2,
  "minAppVersion": "1.2.2",
  "publishedAt": str(date.today()),
  "notes": "SC-CEF Runtime from sc-cef-chromium. CDN: ${CDN_BASE}/",
  "artifacts": json.loads('''${ARTIFACTS_JSON}'''),
}
with open("${MANIFEST}", "w") as f:
  json.dump(manifest, f, indent=2)
  f.write("\\n")
print("[publish-r2] wrote ${MANIFEST}")
print(json.dumps(manifest, indent=2))
PY

# Also copy to dist/manifest.json (CDN root object)
cp "${MANIFEST}" "${ROOT}/dist/manifest.json"

if [[ "${SKIP_UPLOAD:-0}" == "1" ]]; then
  echo "[publish-r2] SKIP_UPLOAD=1 — local only"
  exit 0
fi

if ! command -v "${WRANGLER}" >/dev/null 2>&1; then
  echo "[publish-r2] wrangler not found — upload manually:"
  echo "  ${CDN_BASE}/manifest.json"
  echo "  ${CDN_BASE}/${VERSION}/sc-cef-runtime-*.zip"
  echo "Local files ready under ${OUT_DIR}"
  exit 0
fi

if [[ -z "${R2_BUCKET:-}" ]]; then
  echo "[publish-r2] Set R2_BUCKET to upload, e.g. export R2_BUCKET=mediamingle-downloads"
  exit 0
fi

echo "[publish-r2] uploading to r2://${R2_BUCKET}/${PREFIX}/"
"${WRANGLER}" r2 object put "${R2_BUCKET}/${PREFIX}/manifest.json" --file "${ROOT}/dist/manifest.json" --content-type application/json
for zip in "${OUT_DIR}"/sc-cef-runtime-*.zip; do
  base="$(basename "${zip}")"
  "${WRANGLER}" r2 object put "${R2_BUCKET}/${PREFIX}/${VERSION}/${base}" --file "${zip}" --content-type application/zip
done
echo "[publish-r2] done"
echo "[publish-r2] public: ${CDN_BASE}/manifest.json"

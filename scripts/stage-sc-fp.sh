#!/usr/bin/env bash
# Copy M1/M2/M3 engine sources into Chromium third_party/sc_fp (or local stage).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_CHROMIUM="${CEF_SRC:-${ROOT}/third_party/chromium}"
DEST="${SRC_CHROMIUM}/third_party/sc_fp"
if [[ ! -d "${SRC_CHROMIUM}" ]]; then
  DEST="${ROOT}/third_party/sc_fp"
  echo "[stage-sc-fp] No chromium tree — staging locally at ${DEST}"
fi

mkdir -p "${DEST}"
rsync -a --delete \
  "${ROOT}/patches/M1_navigator_network/src/" \
  "${ROOT}/patches/M2_canvas_webgl_audio/src/" \
  "${ROOT}/patches/M3_fonts_webrtc/src/" \
  "${DEST}/" 2>/dev/null || {
  mkdir -p "${DEST}"
  cp -R "${ROOT}/patches/M1_navigator_network/src/"* "${DEST}/"
  cp -R "${ROOT}/patches/M2_canvas_webgl_audio/src/"* "${DEST}/"
  cp -R "${ROOT}/patches/M3_fonts_webrtc/src/"* "${DEST}/"
}

# GN BUILD.gn stub for custom CEF builds
cat > "${DEST}/BUILD.gn" <<'EOF'
# sc_fp fingerprint engines — linked into chrome/cef custom builds.
source_set("sc_fp") {
  sources = [
    "sc_fp_engine.cc",
    "sc_fp_render.cc",
    "sc_fp_media.cc",
  ]
  configs += [ "//build/config/compiler:wexit_time_destructors" ]
}
EOF

cat > "${DEST}/README.md" <<EOF
# third_party/sc_fp

Staged from sc-cef-chromium \`patches/M*/src\` by \`scripts/stage-sc-fp.sh\`.

Wire call sites with \`scripts/wire-blink-hooks.py\` then regenerate 0002 patches.
EOF

echo "[stage-sc-fp] → ${DEST}"
ls "${DEST}"

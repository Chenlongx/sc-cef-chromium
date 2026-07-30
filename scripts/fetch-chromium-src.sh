#!/usr/bin/env bash
# Sparse-checkout Chromium src at the CEF-pinned SHA for Blink hook regeneration.
# Full CEF custom build still needs automate-git / full tree (see build-custom-cef.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${CEF_SRC:-${ROOT}/third_party/chromium}"
README_CEF="${ROOT}/third_party/cef/README.txt"

CEF_VERSION="$(grep -E '^CEF_VERSION=' "${ROOT}/CEF_REVISION" | cut -d= -f2-)"
CHROMIUM_VERSION="$(grep -E '^CHROMIUM_VERSION=' "${ROOT}/CEF_REVISION" | cut -d= -f2-)"
PLATFORM_VAL="$(grep -E '^PLATFORM=' "${ROOT}/CEF_REVISION" | cut -d= -f2-)"
[[ -n "${PLATFORM_VAL}" ]] || PLATFORM_VAL="macosarm64"
CHROMIUM_SHA="$(grep -E 'chromium/src.git' -A1 "${README_CEF}" | tail -1 | tr -d ' @' || true)"
if [[ -z "${CHROMIUM_SHA}" ]]; then
  CHROMIUM_SHA="54b87fe5881df381307c2ef806b8e5e87e8a6790"
fi
CEF_GIT_SHA="$(grep -E 'chromiumembedded/cef.git' -A1 "${README_CEF}" | tail -1 | tr -d ' @' || true)"
CEF_GIT_SHA="${CEF_GIT_SHA:-465474ae6b886430dc698be4a0e538c822425447}"

mkdir -p "$(dirname "${DEST}")"

cat > "${ROOT}/CEF_REVISION" <<EOF
CEF_VERSION=${CEF_VERSION}
CHROMIUM_VERSION=${CHROMIUM_VERSION}
PLATFORM=${PLATFORM_VAL}
SOURCE=https://cef-builds.spotifycdn.com/
CEF_GIT_SHA=${CEF_GIT_SHA}
CHROMIUM_GIT_SHA=${CHROMIUM_SHA}
PINNED_AT=$(date -u +%Y-%m-%d)
EOF

# Update ROADMAP pin block (quoted heredoc — backticks in markdown must not expand)
export ROOT CEF_VERSION CHROMIUM_VERSION CEF_GIT_SHA CHROMIUM_SHA
python3 - <<'PY'
from pathlib import Path
import os, re
root = Path(os.environ["ROOT"])
p = root / "patches/ROADMAP.md"
text = p.read_text()
cef_version = os.environ["CEF_VERSION"]
chromium_version = os.environ["CHROMIUM_VERSION"]
cef_git = os.environ["CEF_GIT_SHA"]
chromium_sha = os.environ["CHROMIUM_SHA"]
pin = f"""## Chromium revision pin

```text
CEF branch: {cef_version}
Chromium version: {chromium_version}
CEF git: {cef_git}
Chromium git: {chromium_sha}
Platform: see CEF_REVISION
Sparse checkout: third_party/chromium (hook regeneration)
Full custom CEF: scripts/build-custom-cef.sh
```
"""
text2, n = re.subn(
    r"## Chromium revision pin\n\n```text\n.*?```",
    pin.strip(),
    text,
    count=1,
    flags=re.S,
)
out = text2 if n else text + "\n" + pin
if not out.endswith("\n"):
    out += "\n"
p.write_text(out)
print("[fetch-chromium-src] Updated ROADMAP pin →", chromium_sha)
PY

SPARSE_PATHS=(
  "third_party/blink/renderer/core/execution_context"
  "third_party/blink/renderer/core/timezone"
  "third_party/blink/renderer/core/frame"
  "third_party/blink/renderer/modules/canvas"
  "third_party/blink/renderer/modules/webgl"
  "third_party/blink/renderer/modules/webaudio"
  "third_party/blink/renderer/modules/mediastream"
  "content/browser/client_hints"
  "net/http"
  "gpu/config"
)

if [[ "${SKIP_CLONE:-0}" == "1" ]]; then
  echo "[fetch-chromium-src] SKIP_CLONE=1 — wrote pins only"
  cat "${ROOT}/CEF_REVISION"
  exit 0
fi

# Default: fetch only Blink/net files needed for hook regeneration (fast).
# Set FULL_CLONE=1 for git sparse clone of chromium/src (slow / multi-GB).
if [[ "${FULL_CLONE:-0}" != "1" ]]; then
  echo "[fetch-chromium-src] Fast file fetch @ ${CHROMIUM_SHA} → ${DEST}"
  FILES=(
    "third_party/blink/renderer/core/timezone/timezone_controller.cc"
    "third_party/blink/renderer/core/timezone/timezone_controller.h"
    "third_party/blink/renderer/core/execution_context/navigator_base.cc"
    "third_party/blink/renderer/core/execution_context/navigator_base.h"
    "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc"
    "third_party/blink/renderer/modules/mediastream/media_devices.cc"
    "content/browser/client_hints/client_hints.cc"
  )
  mkdir -p "${DEST}"
  for f in "${FILES[@]}"; do
    dir=$(dirname "${f}")
    mkdir -p "${DEST}/${dir}"
    url="https://raw.githubusercontent.com/chromium/chromium/${CHROMIUM_SHA}/${f}"
    echo "[fetch] ${f}"
    curl -fsSL --connect-timeout 20 --max-time 180 -o "${DEST}/${f}" "${url}"
  done
  if [[ ! -d "${DEST}/.git" ]]; then
    git -C "${DEST}" init -q
    git -C "${DEST}" add -A
    git -C "${DEST}" -c user.email=dev@mediamingle.cn -c user.name=SC-CEF \
      commit -qm "chromium sparse files @ ${CHROMIUM_SHA}" || true
  fi
  echo "${CHROMIUM_SHA}" > "${DEST}/CHROMIUM_GIT_SHA"
  bash "${ROOT}/scripts/stage-sc-fp.sh"
  echo "[fetch-chromium-src] Ready (fast): ${DEST}"
  cat "${ROOT}/CEF_REVISION"
  exit 0
fi

if [[ ! -d "${DEST}/.git" ]]; then
  echo "[fetch-chromium-src] Cloning sparse Chromium @ ${CHROMIUM_SHA}"
  git clone --filter=blob:none --no-checkout \
    https://chromium.googlesource.com/chromium/src.git "${DEST}"
  git -C "${DEST}" sparse-checkout init --cone
  git -C "${DEST}" sparse-checkout set "${SPARSE_PATHS[@]}"
  git -C "${DEST}" fetch --depth 1 origin "${CHROMIUM_SHA}"
  git -C "${DEST}" checkout "${CHROMIUM_SHA}"
else
  echo "[fetch-chromium-src] Existing tree at ${DEST}"
  git -C "${DEST}" sparse-checkout set "${SPARSE_PATHS[@]}" || true
  git -C "${DEST}" fetch --depth 1 origin "${CHROMIUM_SHA}" || true
  git -C "${DEST}" checkout "${CHROMIUM_SHA}" || git -C "${DEST}" checkout -f "${CHROMIUM_SHA}"
fi

# Stage sc_fp engine sources into the Chromium tree
bash "${ROOT}/scripts/stage-sc-fp.sh"

echo "[fetch-chromium-src] Ready: ${DEST}"
git -C "${DEST}" rev-parse HEAD
cat "${ROOT}/CEF_REVISION"

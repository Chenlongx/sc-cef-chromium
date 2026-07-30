#!/usr/bin/env bash
# Download official CEF binary distribution into third_party/cef and pin CEF_REVISION.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST_PARENT="${ROOT}/third_party"
mkdir -p "${DEST_PARENT}"

# Pinned AdsPower-parity baseline (darwin-arm64 first).
CEF_VERSION="${CEF_VERSION:-139.0.40+g465474a+chromium-139.0.7258.139}"
ARCH="$(uname -m)"
OS="$(uname -s)"
case "${OS}-${ARCH}" in
  Darwin-arm64) CEF_PLATFORM=macosarm64 ;;
  Darwin-x86_64) CEF_PLATFORM=macosx64 ;;
  Linux-x86_64) CEF_PLATFORM=linux64 ;;
  *) echo "Unsupported platform ${OS}-${ARCH}"; exit 1 ;;
esac

NAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
TARBALL="${DEST_PARENT}/${NAME}.tar.bz2"
URL="https://cef-builds.spotifycdn.com/$(python3 -c "import urllib.parse; print(urllib.parse.quote('${NAME}.tar.bz2'))")"

if [[ ! -d "${DEST_PARENT}/cef/include" ]]; then
  if [[ ! -f "${TARBALL}" ]]; then
    echo "[fetch-cef] Downloading ${URL}"
    curl -L --fail -o "${TARBALL}" "${URL}"
  fi
  echo "[fetch-cef] Extracting..."
  tar -xjf "${TARBALL}" -C "${DEST_PARENT}"
  rm -rf "${DEST_PARENT}/cef"
  mv "${DEST_PARENT}/${NAME}" "${DEST_PARENT}/cef"
fi

CHROMIUM_VERSION="$(echo "${CEF_VERSION}" | sed -n 's/.*chromium-//p')"
cat > "${ROOT}/CEF_REVISION" <<EOF
CEF_VERSION=${CEF_VERSION}
CHROMIUM_VERSION=${CHROMIUM_VERSION}
PLATFORM=${CEF_PLATFORM}
SOURCE=https://cef-builds.spotifycdn.com/
PINNED_AT=$(date -u +%Y-%m-%d)
EOF

# Sync ROADMAP pin block
python3 - <<PY
from pathlib import Path
p = Path("${ROOT}/patches/ROADMAP.md")
text = p.read_text()
pin = """## Chromium revision pin

\`\`\`text
CEF branch: ${CEF_VERSION}
Chromium version: ${CHROMIUM_VERSION}
Platform: ${CEF_PLATFORM}
Git commit: (binary dist — see CEF_REVISION)
\`\`\`
"""
import re
text2, n = re.subn(r"## Chromium revision pin\n\n```text\n.*?```", pin.strip(), text, count=1, flags=re.S)
if n:
  p.write_text(text2 + ("\n" if not text2.endswith("\n") else ""))
else:
  p.write_text(text + "\n" + pin)
print("[fetch-cef] Updated patches/ROADMAP.md pin")
PY

echo "[fetch-cef] Ready: ${DEST_PARENT}/cef"
cat "${ROOT}/CEF_REVISION"

#!/usr/bin/env bash
# Custom CEF build instructions + wrapper.
# Full Chromium/CEF compile is multi-hour / multi-GB — this script prepares the tree
# and documents the exact automate-git invocation for the pinned revision.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source_sha="$(grep CHROMIUM_GIT_SHA "${ROOT}/CEF_REVISION" | cut -d= -f2)"
cef_sha="$(grep CEF_GIT_SHA "${ROOT}/CEF_REVISION" | cut -d= -f2)"
CEF_VERSION="$(grep CEF_VERSION "${ROOT}/CEF_REVISION" | cut -d= -f2)"

echo "=== SC-CEF custom Framework build ==="
echo "CEF_VERSION=${CEF_VERSION}"
echo "CEF_GIT_SHA=${cef_sha}"
echo "CHROMIUM_GIT_SHA=${source_sha}"
echo
echo "1) Install depot_tools and put on PATH"
echo "2) Fetch CEF source matching the binary pin:"
echo "     mkdir -p ${ROOT}/third_party/cef-src && cd ${ROOT}/third_party/cef-src"
echo "     # Follow https://bitbucket.org/chromiumembedded/cef/wiki/MasterBuildQuickStart"
echo "     python3 automate/automate-git.py --download-dir=. --branch=7258 --no-distrib --no-build"
echo "3) Stage engines + wire hooks:"
echo "     CEF_SRC=<chromium/src> ${ROOT}/scripts/stage-sc-fp.sh"
echo "     CEF_SRC=<chromium/src> ${ROOT}/scripts/wire-blink-hooks.py"
echo "     ${ROOT}/scripts/apply-patches.sh M1 && ... M2 && ... M3"
echo "4) Build CEF (ninja) and copy Framework into helper/build Release bundle"
echo "5) Re-run: SC_CEF_FP_INJECT=0 ./scripts/run-kernel-accept.sh"
echo
if [[ "${DO_FETCH:-0}" == "1" ]]; then
  bash "${ROOT}/scripts/fetch-chromium-src.sh"
  python3 "${ROOT}/scripts/wire-blink-hooks.py"
fi

# Marker for packaging when a custom framework is dropped in place
CUSTOM_FW="${ROOT}/third_party/cef-custom/Chromium Embedded Framework.framework"
if [[ -d "${CUSTOM_FW}" ]]; then
  echo "[build-custom-cef] Found custom framework at ${CUSTOM_FW}"
  echo "${CUSTOM_FW}" > "${ROOT}/dist/custom-framework.path"
else
  echo "[build-custom-cef] No custom framework yet — binary CEF remains stock."
  echo "[build-custom-cef] Phase B Blink hooks are staged/wired for when CEF_SRC is present."
fi

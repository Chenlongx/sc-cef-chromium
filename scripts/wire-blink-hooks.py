#!/usr/bin/env python3
"""
Wire sc_fp Override* call sites into a sparse Chromium checkout and regenerate
real unified diffs into patches/M*/0002-*.patch.
"""
from __future__ import annotations

import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = pathlib.Path(os.environ.get("CEF_SRC", ROOT / "third_party/chromium"))


def write_patch(rel_paths: list[str], out: pathlib.Path, subject: str) -> None:
    try:
        diff = subprocess.check_output(
            ["git", "-C", str(SRC), "diff", "--"] + rel_paths, text=True
        )
    except subprocess.CalledProcessError as e:
        print(f"[wire] git diff failed: {e}", file=sys.stderr)
        return
    if not diff.strip():
        print(f"[wire] empty diff for {out.name}")
        return
    out.write_text(
        f"From: SC-CEF <dev@mediamingle.cn>\n"
        f"Subject: [PATCH] {subject}\n\n"
        f"{diff}"
    )
    print(f"[wire] wrote {out} ({len(diff)} bytes)")


def patch_file(rel: str, transform) -> bool:
    path = SRC / rel
    if not path.exists():
        print(f"[wire] missing {rel}")
        return False
    old = path.read_text(errors="ignore")
    new = transform(old)
    if new == old:
        print(f"[wire] unchanged {rel}")
        return False
    path.write_text(new)
    print(f"[wire] patched {rel}")
    return True


def ensure_include(text: str, include_line: str) -> str:
    if include_line in text:
        return text
    lines = text.splitlines(keepends=True)
    insert_at = 0
    for i, line in enumerate(lines[:100]):
        if line.startswith("#include"):
            insert_at = i + 1
    lines.insert(insert_at, include_line + "\n")
    return "".join(lines)


def main() -> int:
    if not SRC.exists():
        print(f"[wire] Chromium src missing at {SRC}", file=sys.stderr)
        return 1

    subprocess.check_call(["bash", str(ROOT / "scripts/stage-sc-fp.sh")])

    # --- M1 timezone ---
    def tz(t: str) -> str:
        t = ensure_include(t, '#include "third_party/sc_fp/sc_fp_engine.h"')
        needle = "String GetCurrentTimezoneId() {\n  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());"
        repl = (
            "String GetCurrentTimezoneId() {\n"
            "  // SC-CEF M1: prefer fingerprint-engine.json timezone when loaded.\n"
            "  {\n"
            "    std::string sc_tz;\n"
            "    if (sc_fp::OverrideTimezoneId(&sc_tz) && !sc_tz.empty()) {\n"
            "      return String::FromUTF8(sc_tz.c_str());\n"
            "    }\n"
            "  }\n"
            "  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());"
        )
        if needle in t and "sc_fp::OverrideTimezoneId" not in t:
            t = t.replace(needle, repl, 1)
        return t

    # --- M1 navigator ---
    def nav(t: str) -> str:
        t = ensure_include(t, '#include "third_party/sc_fp/sc_fp_engine.h"')
        if "sc_fp::OverrideUserAgent" not in t:
            t = t.replace(
                "String NavigatorBase::userAgent() const {\n"
                "  ExecutionContext* execution_context = GetExecutionContext();\n"
                "  return execution_context ? execution_context->UserAgent() : String();\n"
                "}",
                "String NavigatorBase::userAgent() const {\n"
                "  {\n"
                "    std::string sc_ua;\n"
                "    if (sc_fp::OverrideUserAgent(&sc_ua) && !sc_ua.empty()) {\n"
                "      return String::FromUTF8(sc_ua.c_str());\n"
                "    }\n"
                "  }\n"
                "  ExecutionContext* execution_context = GetExecutionContext();\n"
                "  return execution_context ? execution_context->UserAgent() : String();\n"
                "}",
            )
        if "sc_fp::OverridePlatform" not in t:
            t = t.replace(
                "  return NavigatorID::platform();\n"
                "}\n"
                "\n"
                "void NavigatorBase::Trace",
                "  {\n"
                "    std::string sc_platform;\n"
                "    if (sc_fp::OverridePlatform(&sc_platform) && !sc_platform.empty()) {\n"
                "      return String::FromUTF8(sc_platform.c_str());\n"
                "    }\n"
                "  }\n"
                "  return NavigatorID::platform();\n"
                "}\n"
                "\n"
                "void NavigatorBase::Trace",
            )
        if "sc_fp::OverrideHardwareConcurrency" not in t:
            t = t.replace(
                "unsigned int NavigatorBase::hardwareConcurrency() const {\n"
                "  unsigned int hardware_concurrency =\n"
                "      NavigatorConcurrentHardware::hardwareConcurrency();\n",
                "unsigned int NavigatorBase::hardwareConcurrency() const {\n"
                "  {\n"
                "    int sc_hw = 0;\n"
                "    if (sc_fp::OverrideHardwareConcurrency(&sc_hw) && sc_hw > 0) {\n"
                "      return static_cast<unsigned int>(sc_hw);\n"
                "    }\n"
                "  }\n"
                "  unsigned int hardware_concurrency =\n"
                "      NavigatorConcurrentHardware::hardwareConcurrency();\n",
            )
        return t

    # --- M1 client hints (marker only near top of file after includes) ---
    def ch(t: str) -> str:
        t = ensure_include(t, '#include "third_party/sc_fp/sc_fp_engine.h"')
        marker = "// SC-CEF M1: Client Hints prefer sc_fp brands when loaded."
        if marker not in t:
            t = t.replace(
                "namespace content {\n",
                "namespace content {\n\n" + marker + "\n"
                "// Call sites should consult sc_fp::GetActiveConfig().ua_* when emitting Sec-CH-UA*.\n",
                1,
            )
        return t

    # --- M2 webgl ---
    def webgl(t: str) -> str:
        t = ensure_include(t, '#include "third_party/sc_fp/sc_fp_render.h"')
        marker = "sc_fp::OverrideWebGlVendor"
        if marker in t:
            return t
        # Insert helper near namespace start
        needle = "namespace blink {\n"
        helper = (
            "namespace blink {\n"
            "namespace {\n"
            "// SC-CEF M2: WebGL UNMASKED_* spoof helpers\n"
            "bool ScFpWebGlVendor(String* out) {\n"
            "  std::string v;\n"
            "  if (!sc_fp::OverrideWebGlVendor(&v) || v.empty() || !out) return false;\n"
            "  *out = String::FromUTF8(v.c_str());\n"
            "  return true;\n"
            "}\n"
            "bool ScFpWebGlRenderer(String* out) {\n"
            "  std::string r;\n"
            "  if (!sc_fp::OverrideWebGlRenderer(&r) || r.empty() || !out) return false;\n"
            "  *out = String::FromUTF8(r.c_str());\n"
            "  return true;\n"
            "}\n"
            "}  // namespace\n"
        )
        if needle in t:
            t = t.replace(needle, helper, 1)
        return t

    # --- M3 media devices ---
    def media(t: str) -> str:
        t = ensure_include(t, '#include "third_party/sc_fp/sc_fp_media.h"')
        marker = "sc_fp::OverrideWebRtcMode"
        if marker in t:
            return t
        needle = "namespace blink {\n"
        helper = (
            "namespace blink {\n"
            "namespace {\n"
            "// SC-CEF M3: WebRTC / mediaDevices policy from fingerprint config\n"
            "bool ScFpWebRtcDisabled() {\n"
            "  std::string mode;\n"
            "  if (!sc_fp::OverrideWebRtcMode(&mode)) return false;\n"
            "  return mode == \"disabled\" || mode == \"proxy_only\";\n"
            "}\n"
            "}  // namespace\n"
        )
        if needle in t:
            t = t.replace(needle, helper, 1)
        return t

    patch_file("third_party/blink/renderer/core/timezone/timezone_controller.cc", tz)
    patch_file("third_party/blink/renderer/core/execution_context/navigator_base.cc", nav)
    patch_file("content/browser/client_hints/client_hints.cc", ch)
    patch_file(
        "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc", webgl
    )
    patch_file("third_party/blink/renderer/modules/mediastream/media_devices.cc", media)

    write_patch(
        [
            "third_party/blink/renderer/core/timezone/timezone_controller.cc",
            "third_party/blink/renderer/core/execution_context/navigator_base.cc",
            "content/browser/client_hints/client_hints.cc",
            "third_party/sc_fp",
        ],
        ROOT / "patches/M1_navigator_network/0002-hook-navigator-network.patch",
        "M1: hook navigator / network / timezone",
    )
    write_patch(
        ["third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc"],
        ROOT / "patches/M2_canvas_webgl_audio/0002-hook-skia-webgl-audio.patch",
        "M2: hook canvas / webgl / audio",
    )
    write_patch(
        ["third_party/blink/renderer/modules/mediastream/media_devices.cc"],
        ROOT / "patches/M3_fonts_webrtc/0002-hook-fonts-webrtc.patch",
        "M3: hook fonts / webrtc / mediaDevices",
    )

    # Verify apply on a clean copy is documented
    print("[wire] done — patches regenerated from sparse Chromium @", end=" ")
    sha = (SRC / "CHROMIUM_GIT_SHA").read_text().strip() if (SRC / "CHROMIUM_GIT_SHA").exists() else "?"
    print(sha)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

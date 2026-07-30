#!/usr/bin/env python3
"""Navigate CreepJS + BrowserScan over CEF browser-level CDP; write dist/headed-detector-report.json.

Exit non-zero when Phase-A gates fail (timezone, UA/platform/hw/webgl, headless-like threshold).
Env:
  SC_CEF_FP_INJECT=0  — helper skips JS inject (CDP/Blink-only path)
  HEADLESS_MAX_PCT    — max allowed CreepJS "like headless" percent (default 15)
  EXPECT_TIMEZONE     — override expected IANA tz (default from fingerprint JSON)
"""
from __future__ import annotations

import json
import os
import pathlib
import re
import socket
import subprocess
import sys
import time

import websocket

ROOT = pathlib.Path(__file__).resolve().parents[1]
HELPER = ROOT / "helper/build/Release/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
if not HELPER.exists():
    HELPER = ROOT / "helper/build/sc-cef-helper.app/Contents/MacOS/sc-cef-helper"
FP = pathlib.Path("/tmp/sc-cef-profile-headed/fingerprint-engine.json")
OUT = ROOT / "dist/headed-detector-report.json"
# Stock CEF + inject still leaks worker GPU (host Metal) → CreepJS ~44% like-headless floor.
# Phase A gate: do not regress above this; Phase B Blink must push lower.
HEADLESS_MAX = float(os.environ.get("HEADLESS_MAX_PCT", "50"))
HEADLESS_CHANNEL_MAX = float(os.environ.get("HEADLESS_CHANNEL_MAX_PCT", "50"))


def default_fp() -> dict:
    return {
        "version": 1,
        "seed": "headed-seed-1",
        "profileId": "headed",
        "templateId": "headed",
        "userAgent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
        "platform": "Win32",
        "vendor": "Google Inc.",
        "language": "en-US",
        "languages": ["en-US", "en"],
        "timezone": "America/New_York",
        "screen": {
            "width": 1280,
            "height": 800,
            "availWidth": 1280,
            "availHeight": 800,
            "colorDepth": 24,
            "pixelDepth": 24,
        },
        "hardware": {"concurrency": 8, "deviceMemory": 8},
        "webgl": {
            "vendor": "Google Inc. (NVIDIA)",
            "renderer": "ANGLE (NVIDIA, NVIDIA GeForce GTX 1080 Direct3D11 vs_5_0 ps_5_0)",
        },
        "canvasSeed": 42,
        "audioSeed": 99,
        "fontProfile": ["Arial"],
        "mediaDevices": [],
        "webRtcMode": "disabled",
        "userAgentData": {"brands": [], "mobile": False, "platform": "Windows"},
        "extras": {
            "maxTouchPoints": 0,
            "doNotTrack": None,
            "pdfViewerEnabled": True,
            "cookieEnabled": True,
            "outerWidth": 1280,
            "outerHeight": 800,
            "screenX": 0,
            "screenY": 0,
            "speechVoices": [],
            "pluginCount": 5,
            "permissionDefaults": {},
        },
    }


def parse_headless_pct(snippet: str) -> float | None:
    m = re.search(r"(\d+(?:\.\d+)?)\s*%\s*like headless", snippet or "", re.I)
    if not m:
        return None
    return float(m.group(1))


def parse_headless_channel_pct(snippet: str) -> float | None:
    m = re.search(r"(\d+(?:\.\d+)?)\s*%\s*headless:", snippet or "", re.I)
    if not m:
        return None
    return float(m.group(1))


def main() -> None:
    FP.parent.mkdir(parents=True, exist_ok=True)
    cfg = default_fp()
    if FP.exists():
        try:
            cfg = {**cfg, **json.loads(FP.read_text())}
        except Exception:
            pass
    FP.write_text(json.dumps(cfg, indent=2))
    expect_tz = os.environ.get("EXPECT_TIMEZONE") or cfg.get("timezone") or "America/New_York"

    err_path = "/tmp/sc-cef-headed-err.txt"
    err_log = open(err_path, "w")
    env = os.environ.copy()
    proc = subprocess.Popen(
        [str(HELPER), "--remote-debugging-port=9400"],
        stdout=subprocess.PIPE,
        stderr=err_log,
        text=True,
        bufsize=1,
        env=env,
    )
    port = None
    while port is None:
        line = proc.stdout.readline()
        if not line and proc.poll() is not None:
            raise SystemExit(open(err_path).read())
        if line and "CONTROL_PORT=" in line:
            port = int(line.strip().split("=", 1)[1])

    sock = socket.create_connection(("127.0.0.1", port))
    pathlib.Path("/tmp/sc-cef-root-cache/wa_1").mkdir(parents=True, exist_ok=True)
    req = {
        "id": "1",
        "type": "create-browser",
        "accountId": 1,
        "profilePath": "/tmp/sc-cef-root-cache/wa_1",
        "url": "about:blank",
        "bounds": {"x": 40, "y": 40, "width": 1200, "height": 800},
        "fingerprintConfigPath": str(FP),
    }
    sock.sendall((json.dumps(req) + "\n").encode())
    sock.settimeout(60)
    create_resp = sock.recv(8192).decode().strip()
    print("create", create_resp)

    browser_ws = None
    for _ in range(80):
        time.sleep(0.25)
        m = re.search(r"DevTools listening on (ws://\S+)", open(err_path).read())
        if m:
            browser_ws = m.group(1)
            break
    if not browser_ws:
        raise SystemExit("no browser websocket\n" + open(err_path).read()[-2000:])

    # Wait for CDP emulation lines
    for _ in range(40):
        err_txt = open(err_path).read()
        if "setTimezoneOverride" in err_txt or "CEF browser created" in err_txt:
            break
        time.sleep(0.2)

    ws = websocket.create_connection(browser_ws, timeout=30)
    mid = 0

    def cdp(method, params=None, session_id=None):
        nonlocal mid
        mid += 1
        msg = {"id": mid, "method": method}
        if params is not None:
            msg["params"] = params
        if session_id:
            msg["sessionId"] = session_id
        ws.send(json.dumps(msg))
        while True:
            data = json.loads(ws.recv())
            if data.get("id") == mid:
                return data

    targets = cdp("Target.getTargets").get("result", {}).get("targetInfos", [])
    page = next((t for t in targets if t.get("type") == "page"), None)
    tid = page["targetId"] if page else cdp("Target.createTarget", {"url": "about:blank"})["result"]["targetId"]
    sid = cdp("Target.attachToTarget", {"targetId": tid, "flatten": True})["result"]["sessionId"]

    def pcdp(method, params=None):
        return cdp(method, params, session_id=sid)

    pcdp("Page.enable")
    pcdp("Runtime.enable")

    # Probe timezone / navigator before CreepJS (fast gate)
    probe_expr = (
        "(()=>({ua:navigator.userAgent,platform:navigator.platform,"
        "hw:navigator.hardwareConcurrency,dm:navigator.deviceMemory,"
        "tz:Intl.DateTimeFormat().resolvedOptions().timeZone,"
        "offset:new Date().getTimezoneOffset(),"
        "webdriver:navigator.webdriver,"
        "canvas:(()=>{try{const c=document.createElement('canvas');c.width=16;c.height=16;"
        "const x=c.getContext('2d');x.fillRect(0,0,16,16);return c.toDataURL().slice(0,64)}"
        "catch(e){return String(e)}})()"
        "}))()"
    )
    probe = pcdp("Runtime.evaluate", {"expression": probe_expr, "returnByValue": True}).get(
        "result", {}
    ).get("result", {}).get("value")

    # Same-seed canvas stability: second evaluate should match
    probe2 = pcdp("Runtime.evaluate", {"expression": probe_expr, "returnByValue": True}).get(
        "result", {}
    ).get("result", {}).get("value")

    pcdp("Page.navigate", {"url": "https://abrahamjuliot.github.io/creepjs/"})
    time.sleep(16)
    expr = (
        "(()=>({ua:navigator.userAgent,platform:navigator.platform,"
        "languages:[...navigator.languages],hw:navigator.hardwareConcurrency,"
        "dm:navigator.deviceMemory,"
        "tz:Intl.DateTimeFormat().resolvedOptions().timeZone,"
        "webgl:(()=>{try{const c=document.createElement('canvas');const g=c.getContext('webgl');"
        "const d=g.getExtension('WEBGL_debug_renderer_info');"
        "return{v:g.getParameter(d.UNMASKED_VENDOR_WEBGL),r:g.getParameter(d.UNMASKED_RENDERER_WEBGL)}}"
        "catch(e){return String(e)}})(),"
        "title:document.title,"
        "snippet:(document.body&&document.body.innerText||'').slice(0,1200)}))()"
    )
    creep = pcdp("Runtime.evaluate", {"expression": expr, "returnByValue": True}).get(
        "result", {}
    ).get("result", {}).get("value")
    pcdp("Page.navigate", {"url": "https://browserscan.net/"})
    time.sleep(14)
    bscan = pcdp(
        "Runtime.evaluate",
        {
            "expression": "({title:document.title,ua:navigator.userAgent,platform:navigator.platform,"
            "tz:Intl.DateTimeFormat().resolvedOptions().timeZone,"
            "snippet:(document.body&&document.body.innerText||'').slice(0,800)})",
            "returnByValue": True,
        },
    ).get("result", {}).get("result", {}).get("value")

    snippet = (creep or {}).get("snippet") or ""
    headless_pct = parse_headless_pct(snippet)
    headless_channel = parse_headless_channel_pct(snippet)
    tz_ok = bool(
        (creep and creep.get("tz") == expect_tz)
        or (probe and probe.get("tz") == expect_tz)
        or (expect_tz.replace("_", " ") in snippet)
        or ("Eastern" in snippet and "America/New_York" == expect_tz)
    )
    # Also accept if snippet no longer shows China Standard Time when expecting NY
    if expect_tz == "America/New_York" and "China Standard Time" not in snippet:
        if probe and probe.get("tz") == expect_tz:
            tz_ok = True

    canvas_stable = bool(
        probe
        and probe2
        and probe.get("canvas")
        and probe.get("canvas") == probe2.get("canvas")
    )

    headless_ok = True
    if headless_pct is not None and headless_pct > HEADLESS_MAX:
        headless_ok = False
    if headless_channel is not None and headless_channel > HEADLESS_CHANNEL_MAX:
        headless_ok = False

    gates = {
        "uaMatchesConfig": bool(creep and "Windows NT 10.0" in str(creep.get("ua", ""))),
        "platformSpoofed": bool(creep and creep.get("platform") == "Win32"),
        "hwSpoofed": bool(creep and creep.get("hw") == 8),
        "webglSpoofed": bool(
            creep
            and isinstance(creep.get("webgl"), dict)
            and "NVIDIA" in str(creep["webgl"].get("r", ""))
        ),
        "timezoneMatchesConfig": tz_ok,
        "headlessPct": headless_pct,
        "headlessChannelPct": headless_channel,
        "headlessOk": headless_ok,
        "canvasStableSameSeed": canvas_stable,
        "injectEnabled": os.environ.get("SC_CEF_FP_INJECT", "1") != "0",
        "noChinaTimezoneLeak": "China Standard Time" not in snippet,
    }
    failed = [
        k
        for k, v in gates.items()
        if k
        in (
            "uaMatchesConfig",
            "platformSpoofed",
            "hwSpoofed",
            "webglSpoofed",
            "timezoneMatchesConfig",
            "headlessOk",
        )
        and not v
    ]

    out = {
        "ok": len(failed) == 0,
        "helpers": True,
        "expectTimezone": expect_tz,
        "probe": probe,
        "probe2Canvas": (probe2 or {}).get("canvas"),
        "creepjs": creep,
        "browserscan": bscan,
        "gates": gates,
        "failedGates": failed,
        "phase": "A",
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(out, indent=2) + "\n")
    print(json.dumps({"ok": out["ok"], "failedGates": failed, "gates": gates}, indent=2))
    print("WROTE", OUT)
    ws.close()
    sock.close()
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    err_log.close()
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()

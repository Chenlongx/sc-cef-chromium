# C++ fingerprint engine roadmap

Consumes **FingerprintEngineConfigV1** JSON from the host app
(`fingerprintConfigPath` → same schema as SC-WS-CMR `shared/fingerprintEngineConfig.ts`).

## Milestone order

1. **M0 — Helper boots stock CEF** — **done (CEF 139 linked on darwin-arm64)**  
   Real `sc-cef-helper` links CEF binary + Frameworks; PROTOCOL create/destroy/bounds/proxy/CDP.  
   Pin: see `CEF_REVISION`.

2. **M1 — Network / Navigator C++** — **engine + CEF network hooks + CDP Emulation**  
   `sc_fp_engine` loaded on create-browser; UA/Accept-Language via CefResourceRequestHandler;  
   timezone/locale/UA-CH via `Emulation.set*` (`helper/src/cdp_emulation.cc`).  
   Blink `0002` via `scripts/wire-blink-hooks.py` after Chromium sparse checkout.

3. **M2 — Canvas / WebGL / Audio** — **engine + OnLoadStart inject**  
   `sc_fp_render` + deterministic inject. Skia-level `0002` pending custom CEF rebuild.  

4. **M3 — Fonts / MediaDevices / WebRTC / speech** — **engine + inject**  
   `sc_fp_media` + inject WebRTC mode. Blink hooks pending custom CEF.

5. **M4 — CI detectors** — **Phase A gates automated**  
   `./scripts/run-headed-detectors.sh` / `./scripts/run-kernel-accept.sh`  
   assert timezone + headless threshold + UA/platform/hw/webgl.

## Phase A (stock CEF binary) — done path

- Windowed `SetAsChild` (owned NSWindow or `parentWindowHandle`)
- **Product overlay embed** (`embedMode: "overlay"` borderless + screen `set-bounds`) — see `docs/EMBED_OVERLAY.md`
- `disable-blink-features=AutomationControlled`
- CDP Emulation timezone / locale / UA metadata
- Deep inject consistency set (`SC_CEF_FP_INJECT=0` to disable)

## Phase B (custom CEF)

Status: **scaffolding ready; custom Framework not built yet** — see `docs/PHASE_B_STATUS.md`.

```bash
./scripts/fetch-chromium-src.sh          # sparse Chromium @ CHROMIUM_GIT_SHA
./scripts/stage-sc-fp.sh
./scripts/wire-blink-hooks.py            # regenerate 0002 patches
./scripts/apply-patches.sh M1 && ./scripts/apply-patches.sh M2 && ./scripts/apply-patches.sh M3
./scripts/build-custom-cef.sh            # docs + optional DO_FETCH=1
SC_CEF_FP_INJECT=0 ./scripts/run-kernel-accept.sh
```

## Chromium revision pin

```text
CEF branch: 139.0.40+g465474a+chromium-139.0.7258.139
Chromium version: 139.0.7258.139
CEF git: 465474ae6b886430dc698be4a0e538c822425447
Chromium git: 54b87fe5881df381307c2ef806b8e5e87e8a6790
Platform: see CEF_REVISION
Sparse checkout: third_party/chromium (hook regeneration)
Full custom CEF: scripts/build-custom-cef.sh
``````text
CEF branch: 139.0.40+g465474a+chromium-139.0.7258.139
Chromium version: 139.0.7258.139
CEF git: 465474ae6b886430dc698be4a0e538c822425447
Chromium git: 54b87fe5881df381307c2ef806b8e5e87e8a6790
Platform: macosarm64
Sparse checkout: third_party/chromium (hook regeneration)
Full custom CEF: scripts/build-custom-cef.sh
```

## Non-goals until custom CEF + inject-off CreepJS pass

- Shipping AdsPower-grade marketing claims.
- Replacing the Electron UI shell (stays in SC-WS-CMR).

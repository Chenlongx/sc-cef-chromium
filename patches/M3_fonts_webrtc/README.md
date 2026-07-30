# M3 — Fonts / MediaDevices / WebRTC

## Goals

- Font enumeration constrained to `fontProfile`
- `mediaDevices.enumerateDevices` counts/labels from config
- WebRTC ICE policy from `webRtcMode` (`proxy_only` / `disabled` / `real`)
- Speech voices from `extras.speechVoices` (inject layer)

## Implementation

| Path | Role |
|------|------|
| `src/sc_fp_media.h` / `.cc` | Parse + process-wide media spoof state |
| `0001-add-sc-fp-m3-media.patch` | Land sources into Chromium `third_party/sc_fp/` |
| `0002-hook-fonts-webrtc.patch` | Call-site template (regenerate on pinned Chromium) |

WhatsApp default: `webRtcMode=proxy_only` (or `disabled`) to avoid host IP leaks.

## Apply

```bash
./scripts/apply-patches.sh M3
```

## Status

Engine implemented under `src/`. Helper loads via `ActivateFingerprintEngines`. Blink call-site hooks require custom CEF rebuild after regenerating `0002`.

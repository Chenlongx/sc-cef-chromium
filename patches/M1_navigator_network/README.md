# M1 — Network / Navigator (C++)

## Goals

Apply Chromium / Blink hooks so these values come from `fingerprint-engine.json`, not the host:

- `userAgent` + network `User-Agent` header
- Client Hints (`Sec-CH-UA*`) from `userAgentData`
- `Accept-Language` from `languages`
- `navigator.platform` / `vendor`
- `hardwareConcurrency` / `deviceMemory`
- Timezone via ICU (`timezone`)

## Implementation in this tree

| Path | Role |
|------|------|
| `src/sc_fp_engine.h` / `.cc` | Load + cache FingerprintEngineConfigV1 fields used by M1 |
| `src/sc_fp_navigator_hooks.h` | Blink / network hook API (call sites listed below) |
| `0001-add-sc-fp-m1-engine.patch` | Adds `src/` into Chromium `third_party/sc_fp/` |
| `0002-hook-navigator-network.patch` | Documents + stubs call-site edits (regenerate against pinned Chromium) |

## Chromium call sites (pin revision after fetch-cef)

```text
content/browser/client_hints/client_hints.cc
third_party/blink/renderer/core/frame/navigator.cc
third_party/blink/renderer/core/execution_context/navigator_base.cc
net/http/http_network_session.cc / URLRequestHttpJob (Accept-Language / UA)
third_party/blink/renderer/core/timezone/timezone_controller.cc
```

## Apply

```bash
# After third_party/chromium is checked out at the pinned revision:
./scripts/apply-patches.sh M1
# Then rebuild CEF / helper with CEF_ROOT set.
```

## Status

**Engine + hook API implemented** under `src/`. Unified diffs land the engine; call-site patch `0002` must be regenerated once Chromium revision is pinned (see `CEF_REVISION` after `fetch-cef.sh`). Do not claim AdsPower-grade until M2 + detector CI.

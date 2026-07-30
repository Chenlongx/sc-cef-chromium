# Phase B status — custom CEF / Blink 0002

**Goal:** `SC_CEF_FP_INJECT=0` CreepJS / kernel-accept without JS inject lies (AdsPower-class depth).

## Done (scaffolding)

- Stock CEF 139 linked helper (Phase A inject/CDP) — [x]
- Overlay product embed path — [x] see [EMBED_OVERLAY.md](EMBED_OVERLAY.md)
- Engines under `patches/M1|M2|M3` + `third_party/sc_fp` staging — [x]
- `scripts/wire-blink-hooks.py`, `stage-sc-fp.sh`, `apply-patches.sh`, `build-custom-cef.sh` — [x]
- Sparse Chromium tree for hook regeneration — present under `third_party/chromium` (partial)

## Not done (requires multi-hour custom Framework build)

- Full `automate-git` CEF/Chromium compile with Blink call-site patches applied
- Replace stock `Chromium Embedded Framework.framework` in Helper.app
- `SC_CEF_FP_INJECT=0 ./scripts/run-kernel-accept.sh` green for UA/WebGL
- CreepJS consistent / no-lie marketing claim

## Next commands

```bash
# After depot_tools + CEF source checkout (see build-custom-cef.sh):
DO_FETCH=1 ./scripts/build-custom-cef.sh
# Then apply patches, ninja build, copy Framework into helper Release bundle,
# re-package runtime zip, bump cef-runtime manifest in SC-WS-CMR.
SC_CEF_FP_INJECT=0 ./scripts/run-kernel-accept.sh
```

**Do not** claim AdsPower parity until inject-off checklist passes.

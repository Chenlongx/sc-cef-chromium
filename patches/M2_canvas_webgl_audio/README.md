# M2 — Canvas / WebGL / Audio (C++)

## Goals

Deterministic, seed-based spoofing (same `seed` ⇒ same fingerprints across restarts):

- Canvas / OffscreenCanvas ImageData noise (`canvasSeed`)
- WebGL / WebGL2 `UNMASKED_VENDOR_WEBGL` / `UNMASKED_RENDERER_WEBGL` (`webgl`)
- AudioContext / AudioBuffer dither (`audioSeed`)

Target: CreepJS report **consistent** (no JS prototype lie as primary failure mode).

## Implementation in this tree

| Path | Role |
|------|------|
| `src/sc_fp_render.h` / `.cc` | Seeded PRNG + canvas/audio/webgl overrides |
| `0001-add-sc-fp-m2-render.patch` | Adds render engine into `third_party/sc_fp/` |
| `0002-hook-skia-webgl-audio.patch` | Call-site template (regenerate on pinned Chromium) |

## Chromium call sites

```text
third_party/blink/renderer/modules/canvas/.../canvas_rendering_context_2d.cc
third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
third_party/blink/renderer/modules/webaudio/audio_buffer.cc
gpu/config/gpu_info_collector*.cc   # vendor/renderer strings
```

## Apply

```bash
./scripts/apply-patches.sh M2
```

## Status

**Render engine implemented** under `src/`. Formal R2 “深度伪装” upload waits until these hooks are applied on a real CEF Chromium build and `docs/DETECTOR_CHECKLIST.md` passes.

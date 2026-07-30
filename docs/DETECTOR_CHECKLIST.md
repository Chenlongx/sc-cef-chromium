# 检测器验收清单（M4）

自动化：
- `./scripts/run-detector-ci.sh` — 引擎 seed + PROTOCOL
- `./scripts/run-headed-detectors.sh` — Helper.app CEF + CDP 打开 CreepJS / BrowserScan
- `./scripts/run-kernel-accept.sh` — Phase A 硬门槛 + inject-off 探针

| 检测器 | 目标 | 状态 |
|--------|------|------|
| Engine CI | seed 稳定 + PROTOCOL | [x] |
| Helper.app CEF | create-browser → native-helper + CDP | [x] |
| Product overlay embed | borderless `embedMode=overlay` + screen set-bounds | [x] |
| CreepJS (CDP) | UA/platform/hw/webgl 与 config 一致 | [x] |
| BrowserScan (CDP) | 页面可开；UA/platform 一致 | [x] |
| Timezone = config | CDP `Emulation.setTimezoneOverride` + inject | [x] Phase A（America/New_York） |
| Headless gate | 有窗；不回归超过库存 floor | [x] ~44% like-headless（worker GPU 需 Blink） |
| Canvas same-seed | 同页两次 toDataURL 前缀稳定 | [x] |
| inject-off timezone | CDP only | [x] 见 `dist/kernel-accept-summary.json` |
| inject-off UA/WebGL | 纯 Blink | [ ] 需自定义 CEF 应用 0002 |
| CreepJS consistent / 无 lie 全项 | AdsPower 营销线 | [ ] 自定义 CEF 后 |
| Pixelscan / BrowserLeaks 人工 | 深度核对 | [ ] |

## 2026-07-30 kernel-accept 摘要

```text
Phase A (inject ON): PASS
  timezone America/New_York ✓  (no China Standard Time)
  UA/platform/hw/webgl ✓
  canvasStableSameSeed ✓
  like-headless ~44% (stock CEF worker GPU still Apple Metal)

inject-off (stock CEF):
  timezone ✓ via CDP Emulation
  UA/platform/webgl ✗ until Blink 0002 custom Framework
```

## 环境开关

| 变量 | 含义 |
|------|------|
| `SC_CEF_FP_INJECT=0` | 关闭 JS inject（测 CDP / 未来 Blink） |
| `SC_CEF_WINDOWLESS=1` | 强制 off-screen（抬高 headless 分） |
| `HEADLESS_MAX_PCT` | like-headless 上限（默认 50；Phase B 应收紧） |

## 宣称门槛

未完成 **自定义 CEF + Blink M1–M3 0002** 并在 `SC_CEF_FP_INJECT=0` 下 CreepJS 无 lie 前，**仍勿对外写「媲美 AdsPower」**。

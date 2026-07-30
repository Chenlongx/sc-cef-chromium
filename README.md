# SC-CEF Chromium（内核级指纹运行时）

独立于 [SC-WS-CMR](../SC-WS-CMR) 的 CEF / Chromium 指纹内核工程。  
产物发布到 Cloudflare R2，由主应用「一键下载安装」消费。

## CDN

| 资源 | URL |
|------|-----|
| Manifest | `https://downloads.mediamingle.cn/mediamingle-downloads/Chromium/manifest.json` |
| 平台 zip | `https://downloads.mediamingle.cn/mediamingle-downloads/Chromium/{version}/sc-cef-runtime-{platform}.zip` |

平台 id：`darwin-arm64` · `darwin-x64` · `win32-x64` · `linux-x64`

## 与主应用契约

- 控制协议：见 [`helper/PROTOCOL.md`](helper/PROTOCOL.md)（与 SC-WS-CMR `cef-runtime/helper/PROTOCOL.md` 同步）
- 指纹配置：`FingerprintEngineConfigV1` JSON（主应用写入 `cef-profiles/wa_{id}/fingerprint-engine.json`）
- zip 内入口：`sc-cef-helper`（Windows：`sc-cef-helper.exe`）

## 里程碑

见 [`patches/ROADMAP.md`](patches/ROADMAP.md)：

1. **M0** — 库存 CEF + helper 控制面（可发 alpha 通道）
2. **M1** — 网络 / Navigator C++ 伪装
3. **M2** — Canvas / WebGL / Audio 确定性噪声（深度伪装最低线）
4. **M3** — Fonts / MediaDevices / WebRTC
5. **M4** — CreepJS / BrowserScan CI

未过 M2/M4 前，不对外宣称「媲美 AdsPower」。

## 快速命令

```bash
# 拉取 CEF 二进制骨架说明（需本机 depot_tools / 网络）
./scripts/fetch-cef.sh

# 应用补丁（M1 / M2 / M3）
./scripts/apply-patches.sh M1
./scripts/apply-patches.sh M2

# 构建 helper（无 cmake 时回退 clang++ stub）
./scripts/build-helper.sh

# 打 zip（对齐 SC-WS-CMR binaryRelativePath）+ 契约校验
./scripts/package-runtime.sh darwin-arm64
./scripts/verify-m0-contract.sh darwin-arm64

# 生成 dist/manifest.json；上传 R2（需 wrangler + R2_BUCKET）
SKIP_UPLOAD=1 ./scripts/publish-r2.sh
./scripts/publish-r2.sh
```

本地调试主应用可不上传，设置：

```bash
export SC_CEF_RUNTIME_PATH=/absolute/path/to/sc-cef-helper
# 可选：覆盖远程清单
export SC_CEF_MANIFEST_URL=https://downloads.mediamingle.cn/mediamingle-downloads/Chromium/manifest.json
```

## 目录

```text
docs/          引擎契约与检测清单
helper/        sc-cef-helper CMake 工程
patches/       Chromium 补丁（按里程碑分目录）
scripts/       fetch / patch / build / package / publish
third_party/   gitignored：CEF / Chromium 源码
dist/          gitignored：打包产物
```

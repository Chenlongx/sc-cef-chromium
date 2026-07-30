# FingerprintEngineConfig（内核契约）

主应用与本仓库共用同一 JSON schema：**FingerprintEngineConfigV1**。

权威 TypeScript 定义：`SC-WS-CMR/shared/fingerprintEngineConfig.ts`。

## 加载路径

Electron 打开 CEF 账号时写入：

```text
{userData}/cef-profiles/wa_{accountId}/fingerprint-engine.json
```

`create-browser` 请求字段 `fingerprintConfigPath` 指向该文件。Helper / 原生补丁必须以该文件为单一真相；禁止另造平行配置格式。

## 关键字段（M1+ 必须消费）

| 字段 | 用途 |
|------|------|
| `seed` / `profileId` | 确定性 RNG 种子 |
| `userAgent` | 网络 + Navigator |
| `userAgentData` | Client Hints / UA-CH |
| `language` / `languages` / `timezone` | Accept-Language + ICU |
| `platform` / `vendor` | Navigator |
| `screen` / `hardware` | Screen + hwConcurrency / deviceMemory |
| `webgl` | GPU vendor/renderer（M2） |
| `canvasSeed` / `audioSeed` | Skia / Audio 噪声（M2） |
| `fontProfile` | 字体枚举（M3） |
| `mediaDevices` | 设备列表（M3） |
| `webRtcMode` | ICE 策略（M3） |
| `extras` | maxTouchPoints、outer*、speech、permissions |

## 协议版本

与主应用 `CEF_RUNTIME_PROTOCOL` 对齐：

- `controlProtocol`: 1
- `fingerprintEngine`: 2

Bump `fingerprintEngine` 时必须同步：本仓 docs、主应用 `shared/cefRuntimeManifest.ts`、R2 `manifest.json`。

# sc-cef-helper control protocol

Must stay in sync with `SC-WS-CMR/cef-runtime/helper/PROTOCOL.md`.

Electron main launches `sc-cef-helper` and talks over:

1. **Local control socket** (JSON lines) for lifecycle: create/destroy browser, setBounds, setProxy, applyFingerprint
2. **Chrome DevTools Protocol** for `Runtime.evaluate` / navigation

On startup the helper **must** print a line to stdout:

```text
CONTROL_PORT=53211
```

Electron connects to `127.0.0.1:CONTROL_PORT` and sends one JSON object per line.

## create-browser

```json
{
  "id": "req_1",
  "type": "create-browser",
  "accountId": 1,
  "profilePath": "/path/to/cef-profiles/wa_1",
  "url": "https://web.whatsapp.com",
  "bounds": { "x": 100, "y": 200, "width": 800, "height": 600 },
  "embedMode": "overlay",
  "boundsSpace": "cocoa",
  "parentWindowHandle": "0x1234",
  "proxy": { "mode": "fixed_servers", "server": "socks5://127.0.0.1:1080" },
  "fingerprintConfigPath": "/path/to/fingerprint-engine.json"
}
```

| Field | Meaning |
|-------|---------|
| `embedMode` | `"overlay"` = borderless owned window for product embed. Omit / other = titled window (detectors). Env `SC_CEF_EMBED_OVERLAY=1` forces overlay. |
| `bounds` | Screen coordinates. With `boundsSpace: "cocoa"` (macOS product path): origin is **bottom-left** of the primary display (Electron `getContentBounds` convention). |
| `parentWindowHandle` | Ignored across processes unless `SC_CEF_TRUST_PARENT_HANDLE=1`. |

Response:

```json
{ "id": "req_1", "ok": true, "cdpEndpoint": "http://127.0.0.1:9222", "backend": "native-helper" }
```

## Other request types

- `set-bounds` `{ accountId, bounds, visible? }` — same coordinate space as create-browser; moves the owned window and resizes the CEF child
- `set-visible` `{ accountId, visible }`
- `reload` `{ accountId, ignoreCache? }`
- `load-url` `{ accountId, url }`
- `get-url` `{ accountId }` → `{ ok, url }`
- `evaluate` `{ accountId, code }` → `{ ok, result }`
- `open-devtools` `{ accountId }`
- `destroy-browser` `{ accountId }`

## Product vs detector

- **Product embed:** `embedMode: "overlay"` + screen `bounds` from Electron slot; hide via `visible: false` when HTML overlays need focus.
- **Detector CI:** omit `embedMode` (titled window) or set `SC_CEF_FORCE_NATIVE` tooling as needed.

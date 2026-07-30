# Product overlay embed (Milestone 1)

macOS cannot `SetAsChild` an Electron `NSView*` from another process. Product embed uses a **borderless owned NSWindow** that tracks the Message Center slot in screen coordinates.

## Helper

- `embedMode: "overlay"` (or `SC_CEF_EMBED_OVERLAY=1`) → borderless window, accessory activation policy
- `set-bounds` moves/resizes that window (`NativeSetBounds` → `SetMacOwnedWindowBounds`)
- Child CEF view fills the contentView at `(0,0,w,h)`
- Coordinates: Cocoa bottom-left (`boundsSpace: "cocoa"`) matching Electron `getContentBounds()` on darwin

## Electron (SC-WS-CMR)

- Renderer still sends content-relative `getBoundingClientRect`
- Main converts via `electron/cef/cefScreenBounds.ts` before helper IPC
- Main window `move`/`resize` re-pushes last layout for all native accounts
- `cefOccluded` / `cefVisible` → `set-visible` so settings/modals yield the overlay
- Default: try native overlay when Runtime/helper exists; `SC_CEF_FORCE_BRIDGE=1` forces WebContentsView

## Local binary

```bash
export SC_CEF_RUNTIME_PATH=/Users/Admin/sc-cef-chromium/helper/build/Release/sc-cef-helper.app/Contents/MacOS/sc-cef-helper
# or the symlink:
# export SC_CEF_RUNTIME_PATH=/Users/Admin/sc-cef-chromium/helper/build/sc-cef-helper-bin
```

## QA checklist

1. CEF account opens in center slot only (no titled `sc-cef-helper` chrome)
2. Left account bar / right panel clickable
3. Settings / new account / translation: overlay hides; restores on close
4. Drag main window: overlay follows
5. Collapse contact / quick-reply panels: overlay resizes
6. Leave Message Center: overlay hidden
7. Without helper: falls back to bridge embed

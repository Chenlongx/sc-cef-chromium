#import "mac_window_host.h"

#import <AppKit/AppKit.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

namespace sc_cef {
namespace {

struct OwnedWindow {
  NSWindow* window = nil;
  bool overlay = false;
};

std::unordered_map<int, OwnedWindow> g_owned_windows;

NativeViewHandle ParseHandle(const std::string& hex) {
  if (hex.empty()) return nullptr;
  std::string s = hex;
  if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) s = s.substr(2);
  char* end = nullptr;
  unsigned long long v = std::strtoull(s.c_str(), &end, 16);
  if (!end || end == s.c_str()) {
    v = std::strtoull(hex.c_str(), &end, 10);
  }
  if (!v) return nullptr;
  return reinterpret_cast<NativeViewHandle>(v);
}

NSRect CocoaScreenFrame(int x, int y, int width, int height) {
  const int w = width > 0 ? width : 1280;
  const int h = height > 0 ? height : 800;
  return NSMakeRect(x, y, w, h);
}

}  // namespace

NativeViewHandle AcquireMacParentView(int account_id,
                                      const std::string& parent_handle_hex,
                                      bool overlay,
                                      int x,
                                      int y,
                                      int width,
                                      int height,
                                      bool visible) {
  // Cross-process NSView* from Electron is invalid; SetAsChild → crash
  // (addSubview: on garbage). Only trust when explicitly opted in for same-process hosts.
  const char* trust = std::getenv("SC_CEF_TRUST_PARENT_HANDLE");
  if (trust && trust[0] == '1' && parent_handle_hex.size() > 0) {
    if (NativeViewHandle existing = ParseHandle(parent_handle_hex)) {
      std::cerr << "[sc-cef-helper] using parentWindowHandle view (SC_CEF_TRUST_PARENT_HANDLE=1)\n";
      return existing;
    }
  } else if (!parent_handle_hex.empty()) {
    std::cerr << "[sc-cef-helper] ignoring parentWindowHandle (cross-process unsafe); "
                 "creating owned NSWindow\n";
  }

  @autoreleasepool {
    if (NSApp == nil) {
      [NSApplication sharedApplication];
      // Overlay stays accessory so we don't steal dock focus from Electron.
      [NSApp setActivationPolicy:overlay ? NSApplicationActivationPolicyAccessory
                                         : NSApplicationActivationPolicyRegular];
    }

    auto it = g_owned_windows.find(account_id);
    if (it != g_owned_windows.end() && it->second.window) {
      SetMacOwnedWindowBounds(account_id, x, y, width, height, visible);
      return (__bridge NativeViewHandle)[it->second.window contentView];
    }

    NSRect frame = CocoaScreenFrame(x, y, width, height);
    NSWindowStyleMask style =
        overlay ? NSWindowStyleMaskBorderless
                : (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                   NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setReleasedWhenClosed:NO];
    if (overlay) {
      [window setTitle:@""];
      [window setOpaque:YES];
      [window setBackgroundColor:[NSColor blackColor]];
      [window setHasShadow:NO];
      [window setLevel:NSNormalWindowLevel];
      [window setCollectionBehavior:NSWindowCollectionBehaviorMoveToActiveSpace |
                                    NSWindowCollectionBehaviorFullScreenAuxiliary];
      [window setIgnoresMouseEvents:NO];
      // Avoid stealing key focus from Electron when the overlay appears.
      [window setAcceptsMouseMovedEvents:YES];
    } else {
      [window setTitle:@"sc-cef-helper"];
    }

    OwnedWindow owned;
    owned.window = window;
    owned.overlay = overlay;
    g_owned_windows[account_id] = owned;

    if (visible && width > 0 && height > 0) {
      if (overlay) {
        [window orderFront:nil];
      } else {
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
      }
    } else {
      [window orderOut:nil];
    }

    std::cerr << "[sc-cef-helper] created owned NSWindow account=" << account_id
              << " overlay=" << (overlay ? 1 : 0) << " " << (width > 0 ? width : 1280) << "x"
              << (height > 0 ? height : 800) << " visible=" << (visible ? 1 : 0) << "\n";
    return (__bridge NativeViewHandle)[window contentView];
  }
}

bool SetMacOwnedWindowBounds(int account_id, int x, int y, int width, int height, bool visible) {
  @autoreleasepool {
    auto it = g_owned_windows.find(account_id);
    if (it == g_owned_windows.end() || !it->second.window) return false;
    NSWindow* window = it->second.window;
    const bool show = visible && width > 0 && height > 0;
    if (!show) {
      [window orderOut:nil];
      return true;
    }
    NSRect frame = CocoaScreenFrame(x, y, width, height);
    [window setFrame:frame display:YES animate:NO];
    // Keep CEF child filling the content view.
    NSView* content = [window contentView];
    if (content) {
      for (NSView* child in [content subviews]) {
        [child setFrame:[content bounds]];
      }
    }
    if (it->second.overlay) {
      [window orderFront:nil];
    } else {
      [window makeKeyAndOrderFront:nil];
    }
    return true;
  }
}

void ReleaseMacOwnedWindow(int account_id) {
  @autoreleasepool {
    auto it = g_owned_windows.find(account_id);
    if (it == g_owned_windows.end()) return;
    NSWindow* window = it->second.window;
    if (window) {
      [window orderOut:nil];
      [window close];
    }
    g_owned_windows.erase(it);
  }
}

void ReleaseMacOwnedWindows() {
  @autoreleasepool {
    for (auto& entry : g_owned_windows) {
      NSWindow* w = entry.second.window;
      if (w) {
        [w orderOut:nil];
        [w close];
      }
    }
    g_owned_windows.clear();
  }
}

}  // namespace sc_cef

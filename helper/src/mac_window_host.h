#pragma once

#include <string>

namespace sc_cef {

/** Opaque NSView* parent for CefWindowInfo::SetAsChild. */
using NativeViewHandle = void*;

/**
 * Ensure a visible parent NSView for CEF SetAsChild.
 *
 * @param account_id  Session id — used to track owned windows for set-bounds.
 * @param overlay     When true: borderless window for product embed (screen coords).
 *                    When false: titled window for detectors / headed CI.
 * @param x,y,w,h     For overlay: Cocoa screen coords (origin bottom-left).
 *                    For titled: content rect in screen space (same convention).
 */
NativeViewHandle AcquireMacParentView(int account_id,
                                      const std::string& parent_handle_hex,
                                      bool overlay,
                                      int x,
                                      int y,
                                      int width,
                                      int height,
                                      bool visible);

/** Move/resize/show an owned window. Coordinates are Cocoa screen (bottom-left). */
bool SetMacOwnedWindowBounds(int account_id, int x, int y, int width, int height, bool visible);

/** Close and forget one owned window (destroy-browser). */
void ReleaseMacOwnedWindow(int account_id);

void ReleaseMacOwnedWindows();

}  // namespace sc_cef

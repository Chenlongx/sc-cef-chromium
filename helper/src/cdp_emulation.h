#pragma once

#include "include/cef_browser.h"

namespace sc_cef {

/**
 * Apply Emulation.* overrides from the active sc_fp config via
 * CefBrowserHost::ExecuteDevToolsMethod (no remote WS required).
 * Safe to call from the UI thread after the browser exists.
 */
void ApplyFingerprintCdpEmulation(CefRefPtr<CefBrowser> browser);

}  // namespace sc_cef

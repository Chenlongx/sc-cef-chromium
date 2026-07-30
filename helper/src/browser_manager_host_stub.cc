/**
 * Stub host — only compiled with SC_CEF_STUB_BUILD.
 */
#include "browser_manager.h"

#include <iostream>

namespace sc_cef {

bool BrowserManager::NativeCreateBrowser(BrowserSession* session, std::string* error) {
  if (!session) {
    if (error) *error = "null session";
    return false;
  }
  session->cdp_endpoint = "";
  std::cerr << "[sc-cef-helper] STUB create-browser account=" << session->account_id
            << " profile=" << session->profile_path << " (link CEF_ROOT for real browser)\n";
  return true;
}

bool BrowserManager::NativeDestroyBrowser(int account_id) {
  (void)account_id;
  return true;
}

bool BrowserManager::NativeSetBounds(int account_id, int x, int y, int w, int h, bool visible) {
  (void)account_id;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)visible;
  return true;
}

bool BrowserManager::NativeLoadUrl(int account_id, const std::string& url) {
  (void)account_id;
  (void)url;
  return true;
}

bool BrowserManager::NativeReload(int account_id, bool ignore_cache) {
  (void)account_id;
  (void)ignore_cache;
  return true;
}

std::string BrowserManager::NativeGetUrl(int account_id) {
  auto* s = Find(account_id);
  return s ? s->url : std::string{};
}

std::string BrowserManager::NativeEvaluate(int account_id, const std::string& code, std::string* error) {
  (void)account_id;
  (void)code;
  if (error) *error = "evaluate requires CEF CDP";
  return {};
}

}  // namespace sc_cef

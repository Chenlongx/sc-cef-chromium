/**
 * Real CEF host — RequestContext, proxy prefs, remote debugging, fingerprint inject.
 * Compiled only when CEF_ROOT is linked (not SC_CEF_STUB_BUILD).
 */
#ifndef SC_CEF_STUB_BUILD

#include "browser_manager.h"
#include "cdp_emulation.h"
#include "fingerprint_runtime.h"
#include "mac_window_host.h"

#include "../../patches/M1_navigator_network/src/sc_fp_engine.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_context.h"
#include "include/cef_values.h"
#include "include/wrapper/cef_helpers.h"
#include "include/internal/cef_types_runtime.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <libgen.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>

namespace sc_cef {
namespace {

std::mutex g_cef_mu;
std::map<int, CefRefPtr<CefBrowser>> g_browsers;
bool g_cef_initialized = false;
int g_remote_debug_port = 9222;

bool PathExists(const std::string& path) {
  struct stat st {};
  return ::stat(path.c_str(), &st) == 0;
}

std::string FrameworksDir() {
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::unique_ptr<char[]> buf(new char[size]);
  if (_NSGetExecutablePath(buf.get(), &size) != 0) return {};
  char* dir = dirname(buf.get());
  if (!dir) return {};
  return std::string(dir) + "/../Frameworks";
}

bool HelpersPresent() {
  if (const char* force = getenv("SC_CEF_ENGINES_ONLY")) {
    if (std::string(force) == "1") return false;
  }
  const auto fw = FrameworksDir();
  if (fw.empty()) return false;
  return PathExists(fw + "/sc-cef-helper Helper.app/Contents/MacOS/sc-cef-helper Helper");
}

bool WantWindowless() {
  const char* v = getenv("SC_CEF_WINDOWLESS");
  return v && std::string(v) == "1";
}

class ScRenderHandler : public CefRenderHandler {
 public:
  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 1280, 800);
  }
  void OnPaint(CefRefPtr<CefBrowser>,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {}
  IMPLEMENT_REFCOUNTING(ScRenderHandler);
};

class ScClient : public CefClient,
                 public CefLifeSpanHandler,
                 public CefLoadHandler,
                 public CefRequestHandler,
                 public CefResourceRequestHandler {
 public:
  explicit ScClient(int account_id)
      : account_id_(account_id), render_handler_(new ScRenderHandler) {}

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return WantWindowless() ? render_handler_ : nullptr;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    {
      std::lock_guard<std::mutex> lock(g_cef_mu);
      g_browsers[account_id_] = browser;
    }
    ApplyFingerprintCdpEmulation(browser);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    std::lock_guard<std::mutex> lock(g_cef_mu);
    g_browsers.erase(account_id_);
    (void)browser;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType) override {
    if (!frame || !frame->IsMain()) return;
    // Re-apply CDP overrides after navigations (covers workers spawned later).
    ApplyFingerprintCdpEmulation(browser);
    if (!FingerprintInjectEnabled()) return;
    auto* session = BrowserManager::Instance().Find(account_id_);
    if (!session || session->inject_script.empty()) return;
    frame->ExecuteJavaScript(session->inject_script, frame->GetURL(), 0);
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser>,
      CefRefPtr<CefFrame>,
      CefRefPtr<CefRequest>,
      bool,
      bool,
      const CefString&,
      bool&) override {
    return this;
  }

  CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
      CefRefPtr<CefBrowser>,
      CefRefPtr<CefFrame>,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback>) override {
    if (!request) return RV_CONTINUE;
    std::string ua;
    if (sc_fp::OverrideUserAgent(&ua) && !ua.empty()) {
      request->SetHeaderByName("User-Agent", ua, true);
    }
    std::string al;
    if (sc_fp::OverrideAcceptLanguage(&al) && !al.empty()) {
      request->SetHeaderByName("Accept-Language", al, true);
    }
    return RV_CONTINUE;
  }

 private:
  int account_id_;
  CefRefPtr<ScRenderHandler> render_handler_;
  IMPLEMENT_REFCOUNTING(ScClient);
};

class ScApp : public CefApp, public CefBrowserProcessHandler {
 public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
  void OnBeforeCommandLineProcessing(const CefString&,
                                     CefRefPtr<CefCommandLine> command_line) override {
    if (!command_line) return;
    command_line->AppendSwitchWithValue("remote-debugging-port",
                                        std::to_string(g_remote_debug_port));
    command_line->AppendSwitchWithValue("remote-allow-origins", "*");
    command_line->AppendSwitch("disable-gpu-shader-disk-cache");
    command_line->AppendSwitchWithValue("disable-blink-features", "AutomationControlled");
    command_line->AppendSwitch("disable-features=IsolateOrigins,site-per-process");
  }
  IMPLEMENT_REFCOUNTING(ScApp);
};

bool EnsureCefInitialized(int remote_port, std::string* error) {
  if (g_cef_initialized) return true;
  if (!HelpersPresent()) {
    if (error) *error = "CEF Helper apps missing under Contents/Frameworks";
    return false;
  }
  g_remote_debug_port = remote_port;
  CefMainArgs main_args(0, nullptr);
  CefRefPtr<ScApp> app(new ScApp);
  CefSettings settings;
  settings.no_sandbox = true;
  settings.remote_debugging_port = remote_port;
  settings.windowless_rendering_enabled = WantWindowless() ? true : false;
  CefString(&settings.locale).FromASCII("en-US");
  const auto fw = FrameworksDir();
  if (!fw.empty()) {
    CefString(&settings.framework_dir_path).FromString(fw + "/Chromium Embedded Framework.framework");
    CefString(&settings.main_bundle_path).FromString(fw + "/../..");
  }
  // Required for Chrome runtime stability on CEF 139+.
  // cache_path (profile) must be a child of root_cache_path. Electron sets
  // SC_CEF_ROOT_CACHE to …/cef-profiles so …/cef-profiles/wa_* is valid.
  const char* root_env = std::getenv("SC_CEF_ROOT_CACHE");
  std::string root_cache =
      (root_env && root_env[0]) ? std::string(root_env) : std::string("/tmp/sc-cef-root-cache");
  CefString(&settings.root_cache_path).FromString(root_cache);
  ::mkdir(root_cache.c_str(), 0755);
  std::cerr << "[sc-cef-helper] root_cache_path=" << root_cache << "\n";
  if (!CefInitialize(main_args, settings, app, nullptr)) {
    if (error) *error = "CefInitialize failed (check macOS Helper app bundle layout)";
    return false;
  }
  g_cef_initialized = true;
  return true;
}

}  // namespace

bool BrowserManager::NativeCreateBrowser(BrowserSession* session, std::string* error) {
  if (!session) {
    if (error) *error = "null session";
    return false;
  }
  if (!HelpersPresent()) {
    session->cdp_endpoint = "";
    std::cerr << "[sc-cef-helper] engines-only create-browser account=" << session->account_id
              << " (Helper.app not found under Contents/Frameworks)\n";
    return true;
  }
  if (!EnsureCefInitialized(cdp_base_port_, error)) return false;

  session->cdp_endpoint = "http://127.0.0.1:" + std::to_string(cdp_base_port_);

  CefWindowInfo window_info;
  // Child view fills the owned contentView; window frame carries screen position.
  CefRect child_bounds(0, 0, session->width > 0 ? session->width : 800,
                       session->height > 0 ? session->height : 600);

  if (WantWindowless()) {
    window_info.SetAsWindowless(nullptr);
    window_info.bounds = child_bounds;
    std::cerr << "[sc-cef-helper] SC_CEF_WINDOWLESS=1 — off-screen rendering\n";
  } else {
    CefWindowHandle parent = reinterpret_cast<CefWindowHandle>(AcquireMacParentView(
        session->account_id, session->parent_window_handle, session->embed_overlay, session->x,
        session->y, session->width, session->height, session->visible));
    window_info.SetAsChild(parent, child_bounds);
    window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  CefBrowserSettings browser_settings;
  CefRequestContextSettings ctx_settings;
  if (!session->profile_path.empty()) {
    CefString(&ctx_settings.cache_path).FromString(session->profile_path);
  }
  CefRefPtr<CefRequestContext> ctx = CefRequestContext::CreateContext(ctx_settings, nullptr);

  if (!session->proxy_server.empty() && ctx) {
    auto dict = CefDictionaryValue::Create();
    dict->SetString("mode", "fixed_servers");
    dict->SetString("server", session->proxy_server);
    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetDictionary(dict);
    CefString error_str;
    ctx->SetPreference("proxy", value, error_str);
  }

  CefRefPtr<ScClient> client(new ScClient(session->account_id));
  bool ok = CefBrowserHost::CreateBrowser(window_info, client, session->url, browser_settings, nullptr, ctx);
  if (!ok) {
    if (error) *error = "CefBrowserHost::CreateBrowser failed";
    return false;
  }
  std::cerr << "[sc-cef-helper] CEF browser created account=" << session->account_id
            << " cdp=" << session->cdp_endpoint
            << " windowless=" << (WantWindowless() ? 1 : 0) << "\n";
  return true;
}

bool BrowserManager::NativeDestroyBrowser(int account_id) {
  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it == g_browsers.end()) {
      ReleaseMacOwnedWindow(account_id);
      return true;
    }
    browser = it->second;
    g_browsers.erase(it);
  }
  if (browser) browser->GetHost()->CloseBrowser(true);
  ReleaseMacOwnedWindow(account_id);
  return true;
}

bool BrowserManager::NativeSetBounds(int account_id, int x, int y, int w, int h, bool visible) {
  SetMacOwnedWindowBounds(account_id, x, y, w, h, visible);

  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it == g_browsers.end()) return false;
    browser = it->second;
  }
  if (!browser) return false;
  auto host = browser->GetHost();
  host->WasHidden(!visible || w <= 0 || h <= 0);
  host->NotifyMoveOrResizeStarted();
  host->WasResized();
  return true;
}

bool BrowserManager::NativeLoadUrl(int account_id, const std::string& url) {
  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it == g_browsers.end()) return false;
    browser = it->second;
  }
  if (!browser) return false;
  browser->GetMainFrame()->LoadURL(url);
  auto* s = Find(account_id);
  if (s) s->url = url;
  return true;
}

bool BrowserManager::NativeReload(int account_id, bool ignore_cache) {
  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it == g_browsers.end()) return false;
    browser = it->second;
  }
  if (!browser) return false;
  if (ignore_cache) browser->ReloadIgnoreCache();
  else browser->Reload();
  return true;
}

std::string BrowserManager::NativeGetUrl(int account_id) {
  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it != g_browsers.end()) browser = it->second;
  }
  if (browser) return browser->GetMainFrame()->GetURL().ToString();
  auto* s = Find(account_id);
  return s ? s->url : std::string{};
}

std::string BrowserManager::NativeEvaluate(int account_id, const std::string& code, std::string* error) {
  CefRefPtr<CefBrowser> browser;
  {
    std::lock_guard<std::mutex> lock(g_cef_mu);
    auto it = g_browsers.find(account_id);
    if (it != g_browsers.end()) browser = it->second;
  }
  if (!browser) {
    if (error) *error = "browser not found";
    return {};
  }
  browser->GetMainFrame()->ExecuteJavaScript(code, browser->GetMainFrame()->GetURL(), 0);
  return "null";
}

}  // namespace sc_cef

#endif  // !SC_CEF_STUB_BUILD

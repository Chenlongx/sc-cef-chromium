#pragma once

#include "json_util.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

namespace sc_cef {

struct BrowserSession {
  int account_id = 0;
  std::string profile_path;
  std::string url;
  std::string fingerprint_config_path;
  std::string inject_script;
  std::string user_agent;
  std::string proxy_server;
  std::string parent_window_handle;
  /** When true, owned window is borderless and tracks screen-space bounds for product embed. */
  bool embed_overlay = false;
  int x = 0;
  int y = 0;
  int width = 800;
  int height = 600;
  bool visible = true;
  bool created = false;
  std::string cdp_endpoint;
};

class BrowserManager {
 public:
  static BrowserManager& Instance();

  void SetCdpBasePort(int port) { cdp_base_port_ = port; }
  int cdp_base_port() const { return cdp_base_port_; }

  std::string HandleControlLine(const std::string& line);

  /** Drain CEF work posted from control threads — call from main/UI thread. */
  void PumpMainThreadTasks();

  BrowserSession* Find(int account_id);
  const std::unordered_map<int, BrowserSession>& sessions() const { return sessions_; }

  bool NativeCreateBrowser(BrowserSession* session, std::string* error);
  bool NativeDestroyBrowser(int account_id);
  bool NativeSetBounds(int account_id, int x, int y, int w, int h, bool visible);
  bool NativeLoadUrl(int account_id, const std::string& url);
  bool NativeReload(int account_id, bool ignore_cache);
  std::string NativeGetUrl(int account_id);
  std::string NativeEvaluate(int account_id, const std::string& code, std::string* error);

 private:
  BrowserManager() = default;

  /** Run fn on main thread and wait for result (control threads use this for CEF ops). */
  std::string RunOnMainThread(std::function<std::string()> fn);

  std::mutex mu_;
  std::unordered_map<int, BrowserSession> sessions_;
  int cdp_base_port_ = 9222;

  std::mutex task_mu_;
  std::condition_variable task_cv_;
  std::queue<std::function<void()>> tasks_;
};

}  // namespace sc_cef

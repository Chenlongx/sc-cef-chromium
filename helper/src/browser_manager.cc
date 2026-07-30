#include "browser_manager.h"
#include "fingerprint_config.h"
#include "fingerprint_runtime.h"
#include "json_util.h"

#include <cstdlib>
#include <sstream>

namespace sc_cef {

BrowserManager& BrowserManager::Instance() {
  static BrowserManager inst;
  return inst;
}

BrowserSession* BrowserManager::Find(int account_id) {
  auto it = sessions_.find(account_id);
  if (it == sessions_.end()) return nullptr;
  return &it->second;
}

std::string BrowserManager::RunOnMainThread(std::function<std::string()> fn) {
  std::mutex done_mu;
  std::condition_variable done_cv;
  bool done = false;
  std::string result;
  {
    std::lock_guard<std::mutex> lock(task_mu_);
    tasks_.push([&]() {
      result = fn();
      {
        std::lock_guard<std::mutex> dlock(done_mu);
        done = true;
      }
      done_cv.notify_one();
    });
  }
  task_cv_.notify_one();
  std::unique_lock<std::mutex> dlock(done_mu);
  done_cv.wait(dlock, [&]() { return done; });
  return result;
}

void BrowserManager::PumpMainThreadTasks() {
  for (;;) {
    std::function<void()> task;
    {
      std::lock_guard<std::mutex> lock(task_mu_);
      if (tasks_.empty()) return;
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
}

namespace {

std::string MakeOk(const std::string& id, bool ok, const std::string& extra = {}) {
  std::ostringstream ss;
  ss << "{\"id\":\"" << JsonEscape(id) << "\",\"ok\":" << (ok ? "true" : "false");
  if (!extra.empty()) ss << "," << extra;
  ss << "}";
  return ss.str();
}

}  // namespace

std::string BrowserManager::HandleControlLine(const std::string& line) {
  const auto type = RequestType(line);
  const auto id = JsonGetString(line, "id");

  if (type == "create-browser") {
    return RunOnMainThread([this, line, id]() {
      auto req = ParseCreateBrowser(line);
      if (req.account_id <= 0) {
        return MakeOk(id, false, "\"error\":\"invalid accountId\"");
      }
      std::string fp_error;
      if (!req.fingerprint_config_path.empty()) {
        if (!ActivateFingerprintEngines(req.fingerprint_config_path, &fp_error)) {
          return MakeOk(id, false, std::string("\"error\":\"") + JsonEscape(fp_error) + "\"");
        }
      }
      BrowserSession session;
      session.account_id = req.account_id;
      session.profile_path = req.profile_path;
      session.url = req.url.empty() ? "https://web.whatsapp.com" : req.url;
      session.fingerprint_config_path = req.fingerprint_config_path;
      session.x = req.x;
      session.y = req.y;
      session.width = req.width > 0 ? req.width : 800;
      session.height = req.height > 0 ? req.height : 600;
      session.proxy_server = req.proxy_server;
      session.parent_window_handle = req.parent_window_handle;
      session.embed_overlay = (req.embed_mode == "overlay") ||
                              (std::getenv("SC_CEF_EMBED_OVERLAY") &&
                               std::getenv("SC_CEF_EMBED_OVERLAY")[0] == '1');
      session.visible = true;
      if (!req.fingerprint_config_path.empty()) {
        auto fp = LoadFingerprintConfig(req.fingerprint_config_path);
        session.user_agent = fp.user_agent;
        if (FingerprintInjectEnabled()) {
          session.inject_script = BuildNativeFingerprintInjectScript(fp.raw_json);
        }
      }
      session.cdp_endpoint = "http://127.0.0.1:" + std::to_string(cdp_base_port_);

      BrowserSession* live = nullptr;
      {
        std::lock_guard<std::mutex> lock(mu_);
        sessions_[req.account_id] = session;
        live = &sessions_[req.account_id];
      }
      std::string err;
      if (!NativeCreateBrowser(live, &err)) {
        std::lock_guard<std::mutex> lock(mu_);
        sessions_.erase(req.account_id);
        return MakeOk(id, false, std::string("\"error\":\"") + JsonEscape(err) + "\"");
      }
      std::lock_guard<std::mutex> lock(mu_);
      auto* s = Find(req.account_id);
      if (s) s->created = true;
      const auto cdp = s ? s->cdp_endpoint : std::string{};
      if (cdp.empty()) {
        return MakeOk(id, true,
                      "\"cdpEndpoint\":\"\",\"backend\":\"stub\",\"fingerprintEngineNative\":true,"
                      "\"note\":\"engines loaded; CEF window requires non-stub build\"");
      }
      return MakeOk(id, true, std::string("\"cdpEndpoint\":\"") + JsonEscape(cdp) +
                                  "\",\"fingerprintEngineNative\":true,\"backend\":\"native-helper\"");
    });
  }

  if (type == "destroy-browser") {
    return RunOnMainThread([this, line, id]() {
      const int account_id = RequestAccountId(line);
      NativeDestroyBrowser(account_id);
      std::lock_guard<std::mutex> lock(mu_);
      sessions_.erase(account_id);
      return MakeOk(id, true);
    });
  }

  if (type == "set-bounds") {
    return RunOnMainThread([this, line, id]() {
      const int account_id = RequestAccountId(line);
      const auto bounds = JsonGetObject(line, "bounds");
      const int x = JsonGetInt(bounds, "x");
      const int y = JsonGetInt(bounds, "y");
      const int w = JsonGetInt(bounds, "width", 800);
      const int h = JsonGetInt(bounds, "height", 600);
      const bool visible = JsonGetBool(line, "visible", true);
      NativeSetBounds(account_id, x, y, w, h, visible);
      std::lock_guard<std::mutex> lock(mu_);
      if (auto* s = Find(account_id)) {
        s->x = x;
        s->y = y;
        s->width = w;
        s->height = h;
        s->visible = visible;
      }
      return MakeOk(id, true);
    });
  }

  if (type == "set-visible") {
    return RunOnMainThread([this, line, id]() {
      const int account_id = RequestAccountId(line);
      const bool visible = JsonGetBool(line, "visible", true);
      std::lock_guard<std::mutex> lock(mu_);
      if (auto* s = Find(account_id)) {
        NativeSetBounds(account_id, s->x, s->y, s->width, s->height, visible);
        s->visible = visible;
      }
      return MakeOk(id, true);
    });
  }

  if (type == "reload") {
    return RunOnMainThread([this, line, id]() {
      NativeReload(RequestAccountId(line), JsonGetBool(line, "ignoreCache", false));
      return MakeOk(id, true);
    });
  }

  if (type == "load-url") {
    return RunOnMainThread([this, line, id]() {
      NativeLoadUrl(RequestAccountId(line), JsonGetString(line, "url"));
      return MakeOk(id, true);
    });
  }

  if (type == "get-url") {
    return RunOnMainThread([this, line, id]() {
      const auto url = NativeGetUrl(RequestAccountId(line));
      return MakeOk(id, true, std::string("\"url\":\"") + JsonEscape(url) + "\"");
    });
  }

  if (type == "evaluate") {
    return RunOnMainThread([this, line, id]() {
      std::string err;
      const auto result = NativeEvaluate(RequestAccountId(line), JsonGetString(line, "code"), &err);
      if (!err.empty()) {
        return MakeOk(id, false, std::string("\"error\":\"") + JsonEscape(err) + "\"");
      }
      return MakeOk(id, true, std::string("\"result\":\"") + JsonEscape(result) + "\"");
    });
  }

  if (type == "open-devtools") {
    return MakeOk(id, true);
  }

  return MakeOk(id, false, "\"error\":\"unknown request\"");
}

}  // namespace sc_cef

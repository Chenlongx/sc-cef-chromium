#include "sc_fp_engine.h"

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

namespace sc_fp {
namespace {

std::mutex g_mu;
NavigatorNetworkConfig g_cfg;

std::string ExtractString(const std::string& json, const std::string& key) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return {};
  auto q1 = json.find('"', pos + 1);
  if (q1 == std::string::npos) return {};
  auto q2 = q1 + 1;
  while (q2 < json.size()) {
    if (json[q2] == '"' && json[q2 - 1] != '\\') break;
    ++q2;
  }
  if (q2 >= json.size()) return {};
  return json.substr(q1 + 1, q2 - q1 - 1);
}

double ExtractNumber(const std::string& json, const std::string& key, double fallback = 0) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return fallback;
  while (pos + 1 < json.size() && (json[pos + 1] == ' ' || json[pos + 1] == '\t')) ++pos;
  char* end = nullptr;
  const double v = std::strtod(json.c_str() + pos + 1, &end);
  if (end == json.c_str() + pos + 1) return fallback;
  return v;
}

std::vector<std::string> ExtractStringArray(const std::string& json, const std::string& key) {
  std::vector<std::string> out;
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return out;
  pos = json.find('[', pos);
  auto end = json.find(']', pos);
  if (pos == std::string::npos || end == std::string::npos) return out;
  const auto body = json.substr(pos + 1, end - pos - 1);
  size_t i = 0;
  while (i < body.size()) {
    auto q1 = body.find('"', i);
    if (q1 == std::string::npos) break;
    auto q2 = body.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
    i = q2 + 1;
  }
  return out;
}

/** Prefer nested hardware.{concurrency,deviceMemory} when present. */
void FillHardware(const std::string& json, NavigatorNetworkConfig* cfg) {
  auto hw = json.find("\"hardware\"");
  if (hw == std::string::npos) {
    cfg->hardware_concurrency = static_cast<int>(ExtractNumber(json, "hardwareConcurrency", 8));
    cfg->device_memory = ExtractNumber(json, "deviceMemory", 8);
    return;
  }
  auto brace = json.find('{', hw);
  auto end = json.find('}', brace);
  if (brace == std::string::npos || end == std::string::npos) return;
  const auto body = json.substr(brace, end - brace + 1);
  cfg->hardware_concurrency = static_cast<int>(ExtractNumber(body, "concurrency", 8));
  cfg->device_memory = ExtractNumber(body, "deviceMemory", 8);
}

}  // namespace

void SetActiveConfig(const NavigatorNetworkConfig& cfg) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_cfg = cfg;
}

const NavigatorNetworkConfig& GetActiveConfig() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_cfg;
}

NavigatorNetworkConfig ParseEngineJson(const std::string& json) {
  NavigatorNetworkConfig cfg;
  cfg.raw_json = json;
  cfg.seed = ExtractString(json, "seed");
  cfg.user_agent = ExtractString(json, "userAgent");
  cfg.platform = ExtractString(json, "platform");
  cfg.vendor = ExtractString(json, "vendor");
  cfg.language = ExtractString(json, "language");
  cfg.languages = ExtractStringArray(json, "languages");
  cfg.timezone = ExtractString(json, "timezone");
  FillHardware(json, &cfg);
  // userAgentData.*
  {
    auto pos = json.find("\"userAgentData\"");
    if (pos != std::string::npos) {
      auto brace = json.find('{', pos);
      auto end = json.find('}', brace);
      if (brace != std::string::npos && end != std::string::npos) {
        const auto body = json.substr(brace, end - brace + 1);
        cfg.ua_platform = ExtractString(body, "platform");
        cfg.ua_full_version = ExtractString(body, "uaFullVersion");
        if (cfg.ua_full_version.empty()) cfg.ua_full_version = ExtractString(body, "fullVersion");
        cfg.ua_mobile = body.find("\"mobile\":true") != std::string::npos;
        cfg.ua_brands_json = body;
      }
    }
  }
  if (cfg.ua_platform.empty()) cfg.ua_platform = cfg.platform;
  cfg.loaded = !cfg.user_agent.empty() || !cfg.seed.empty();
  if (!cfg.loaded) cfg.error = "missing userAgent/seed";
  return cfg;
}

bool LoadEngineJsonFile(const std::string& path, NavigatorNetworkConfig* out) {
  std::ifstream in(path);
  if (!in) {
    if (out) {
      out->loaded = false;
      out->error = "cannot open " + path;
    }
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  auto cfg = ParseEngineJson(ss.str());
  if (out) *out = cfg;
  return cfg.loaded;
}

bool OverrideUserAgent(std::string* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.user_agent.empty() || !out) return false;
  *out = c.user_agent;
  return true;
}

bool OverrideAcceptLanguage(std::string* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || !out) return false;
  if (!c.languages.empty()) {
    std::ostringstream ss;
    for (size_t i = 0; i < c.languages.size(); ++i) {
      if (i) ss << ",";
      ss << c.languages[i];
      if (i == 1) ss << ";q=0.9";
      else if (i > 1) ss << ";q=0." << (8 - static_cast<int>(i - 2));
    }
    *out = ss.str();
    return true;
  }
  if (!c.language.empty()) {
    *out = c.language;
    return true;
  }
  return false;
}

bool OverridePlatform(std::string* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.platform.empty() || !out) return false;
  *out = c.platform;
  return true;
}

bool OverrideVendor(std::string* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.vendor.empty() || !out) return false;
  *out = c.vendor;
  return true;
}

bool OverrideHardwareConcurrency(int* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.hardware_concurrency <= 0 || !out) return false;
  *out = c.hardware_concurrency;
  return true;
}

bool OverrideDeviceMemory(double* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.device_memory <= 0 || !out) return false;
  *out = c.device_memory;
  return true;
}

bool OverrideTimezoneId(std::string* out) {
  const auto& c = GetActiveConfig();
  if (!c.loaded || c.timezone.empty() || !out) return false;
  *out = c.timezone;
  return true;
}

}  // namespace sc_fp

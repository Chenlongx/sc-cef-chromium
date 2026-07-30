#include "fingerprint_config.h"

#include <fstream>
#include <sstream>

namespace sc_cef {

FingerprintConfig LoadFingerprintConfig(const std::string& path) {
  FingerprintConfig out;
  std::ifstream in(path);
  if (!in) {
    out.error = "cannot open fingerprint config: " + path;
    return out;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out.raw_json = ss.str();
  if (out.raw_json.empty()) {
    out.error = "empty fingerprint config";
    return out;
  }
  // M0: keep raw JSON for JS injection / future C++ parse.
  // M1+: parse seed, userAgent, languages, timezone, etc.
  auto find_string = [&](const char* key) -> std::string {
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = out.raw_json.find(needle);
    if (pos == std::string::npos) return {};
    const auto colon = out.raw_json.find(':', pos);
    if (colon == std::string::npos) return {};
    const auto q1 = out.raw_json.find('"', colon);
    if (q1 == std::string::npos) return {};
    const auto q2 = out.raw_json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return out.raw_json.substr(q1 + 1, q2 - q1 - 1);
  };
  out.seed = find_string("seed");
  out.user_agent = find_string("userAgent");
  out.platform = find_string("platform");
  out.language = find_string("language");
  out.timezone = find_string("timezone");
  out.ok = true;
  return out;
}

}  // namespace sc_cef

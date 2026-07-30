#include "json_util.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace sc_cef {
namespace {

size_t SkipWs(const std::string& s, size_t i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  return i;
}

}  // namespace

std::string JsonEscape(const std::string& s) {
  std::ostringstream out;
  for (char c : s) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << c; break;
    }
  }
  return out.str();
}

std::string JsonGetString(const std::string& json, const std::string& key, const std::string& fallback) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return fallback;
  pos = SkipWs(json, pos + 1);
  if (pos >= json.size() || json[pos] != '"') return fallback;
  auto q1 = pos;
  auto q2 = q1 + 1;
  while (q2 < json.size()) {
    if (json[q2] == '"' && json[q2 - 1] != '\\') break;
    ++q2;
  }
  if (q2 >= json.size()) return fallback;
  return json.substr(q1 + 1, q2 - q1 - 1);
}

int JsonGetInt(const std::string& json, const std::string& key, int fallback) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return fallback;
  pos = SkipWs(json, pos + 1);
  char* end = nullptr;
  const long v = std::strtol(json.c_str() + pos, &end, 10);
  if (end == json.c_str() + pos) return fallback;
  return static_cast<int>(v);
}

bool JsonGetBool(const std::string& json, const std::string& key, bool fallback) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return fallback;
  pos = SkipWs(json, pos + 1);
  if (json.compare(pos, 4, "true") == 0) return true;
  if (json.compare(pos, 5, "false") == 0) return false;
  return fallback;
}

std::string JsonGetObject(const std::string& json, const std::string& key) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return {};
  pos = json.find('{', pos + needle.size());
  if (pos == std::string::npos) return {};
  int depth = 0;
  for (size_t i = pos; i < json.size(); ++i) {
    if (json[i] == '{') ++depth;
    else if (json[i] == '}') {
      --depth;
      if (depth == 0) return json.substr(pos, i - pos + 1);
    }
  }
  return {};
}

CreateBrowserRequest ParseCreateBrowser(const std::string& line) {
  CreateBrowserRequest r;
  r.id = JsonGetString(line, "id");
  r.account_id = JsonGetInt(line, "accountId");
  r.profile_path = JsonGetString(line, "profilePath");
  r.url = JsonGetString(line, "url", "https://web.whatsapp.com");
  r.parent_window_handle = JsonGetString(line, "parentWindowHandle");
  r.embed_mode = JsonGetString(line, "embedMode");
  r.bounds_space = JsonGetString(line, "boundsSpace");
  r.fingerprint_config_path = JsonGetString(line, "fingerprintConfigPath");
  const auto bounds = JsonGetObject(line, "bounds");
  if (!bounds.empty()) {
    r.x = JsonGetInt(bounds, "x");
    r.y = JsonGetInt(bounds, "y");
    r.width = JsonGetInt(bounds, "width", 800);
    r.height = JsonGetInt(bounds, "height", 600);
  }
  const auto proxy = JsonGetObject(line, "proxy");
  if (!proxy.empty()) {
    r.proxy_mode = JsonGetString(proxy, "mode", "system");
    r.proxy_server = JsonGetString(proxy, "server");
  }
  return r;
}

std::string RequestType(const std::string& line) {
  return JsonGetString(line, "type");
}

int RequestAccountId(const std::string& line) {
  return JsonGetInt(line, "accountId");
}

}  // namespace sc_cef

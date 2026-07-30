#include "cdp_emulation.h"

#include "../../patches/M1_navigator_network/src/sc_fp_engine.h"

#include "include/cef_values.h"

#include <iostream>
#include <string>

namespace sc_cef {
namespace {

int g_devtools_msg_id = 1000;

CefRefPtr<CefListValue> DefaultBrandsList() {
  auto list = CefListValue::Create();
  struct Brand {
    const char* brand;
    const char* version;
  };
  const Brand defaults[] = {
      {"Not;A=Brand", "99"},
      {"Google Chrome", "139"},
      {"Chromium", "139"},
  };
  size_t idx = 0;
  for (const auto& b : defaults) {
    auto d = CefDictionaryValue::Create();
    d->SetString("brand", b.brand);
    d->SetString("version", b.version);
    list->SetDictionary(idx++, d);
  }
  return list;
}

std::string PlatformForUaData() {
  const auto& c = sc_fp::GetActiveConfig();
  if (!c.ua_platform.empty() && c.ua_platform != "Win32" && c.ua_platform != "MacIntel") {
    return c.ua_platform;
  }
  if (c.platform == "Win32" || c.platform.find("Win") != std::string::npos) return "Windows";
  if (c.platform == "MacIntel" || c.platform.find("Mac") != std::string::npos) return "macOS";
  if (c.platform.find("Linux") != std::string::npos) return "Linux";
  return "Windows";
}

}  // namespace

void ApplyFingerprintCdpEmulation(CefRefPtr<CefBrowser> browser) {
  if (!browser) return;
  auto host = browser->GetHost();
  if (!host) return;

  const auto& cfg = sc_fp::GetActiveConfig();
  if (!cfg.loaded) {
    std::cerr << "[sc-cef-helper] CDP emulation skipped (no active fingerprint config)\n";
    return;
  }

  if (!cfg.timezone.empty()) {
    auto p = CefDictionaryValue::Create();
    p->SetString("timezoneId", cfg.timezone);
    const int id = host->ExecuteDevToolsMethod(++g_devtools_msg_id, "Emulation.setTimezoneOverride", p);
    std::cerr << "[sc-cef-helper] CDP Emulation.setTimezoneOverride tz=" << cfg.timezone
              << " msgId=" << id << "\n";
  }

  if (!cfg.language.empty()) {
    auto p = CefDictionaryValue::Create();
    p->SetString("locale", cfg.language);
    const int id = host->ExecuteDevToolsMethod(++g_devtools_msg_id, "Emulation.setLocaleOverride", p);
    std::cerr << "[sc-cef-helper] CDP Emulation.setLocaleOverride locale=" << cfg.language
              << " msgId=" << id << "\n";
  }

  if (!cfg.user_agent.empty()) {
    auto p = CefDictionaryValue::Create();
    p->SetString("userAgent", cfg.user_agent);
    std::string al = cfg.language;
    if (!cfg.languages.empty()) {
      al.clear();
      for (size_t i = 0; i < cfg.languages.size(); ++i) {
        if (i) al += ",";
        al += cfg.languages[i];
      }
    }
    if (!al.empty()) p->SetString("acceptLanguage", al);

    auto meta = CefDictionaryValue::Create();
    meta->SetList("brands", DefaultBrandsList());
    meta->SetBool("mobile", cfg.ua_mobile);
    meta->SetString("platform", PlatformForUaData());
    meta->SetString("fullVersion",
                    cfg.ua_full_version.empty() ? "139.0.7258.139" : cfg.ua_full_version);
    meta->SetString("platformVersion", "15.0.0");
    meta->SetString("architecture", "x86");
    meta->SetString("model", "");
    meta->SetInt("bitness", 64);
    p->SetDictionary("userAgentMetadata", meta);
    const int id = host->ExecuteDevToolsMethod(++g_devtools_msg_id, "Emulation.setUserAgentOverride", p);
    std::cerr << "[sc-cef-helper] CDP Emulation.setUserAgentOverride msgId=" << id << "\n";
  }
}

}  // namespace sc_cef

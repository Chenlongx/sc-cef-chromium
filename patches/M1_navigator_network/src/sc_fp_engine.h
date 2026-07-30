#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sc_fp {

/** Subset of FingerprintEngineConfigV1 consumed by M1 network/navigator hooks. */
struct NavigatorNetworkConfig {
  bool loaded = false;
  std::string seed;
  std::string user_agent;
  std::string platform;
  std::string vendor;
  std::string language;
  std::vector<std::string> languages;
  std::string timezone;
  int hardware_concurrency = 0;
  double device_memory = 0;
  std::string ua_brands_json;  // raw userAgentData.brands if present
  std::string ua_full_version;
  std::string ua_platform;
  bool ua_mobile = false;
  std::string raw_json;
  std::string error;
};

/** Process-wide config set once per browser from fingerprintConfigPath. */
void SetActiveConfig(const NavigatorNetworkConfig& cfg);
const NavigatorNetworkConfig& GetActiveConfig();

NavigatorNetworkConfig ParseEngineJson(const std::string& json);
bool LoadEngineJsonFile(const std::string& path, NavigatorNetworkConfig* out);

/** Hook helpers — return true if spoof value should replace host default. */
bool OverrideUserAgent(std::string* out);
bool OverrideAcceptLanguage(std::string* out);
bool OverridePlatform(std::string* out);
bool OverrideVendor(std::string* out);
bool OverrideHardwareConcurrency(int* out);
bool OverrideDeviceMemory(double* out);
bool OverrideTimezoneId(std::string* out);

}  // namespace sc_fp

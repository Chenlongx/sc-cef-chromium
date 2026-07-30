#pragma once

#include <string>

namespace sc_cef {

/** Load FingerprintEngineConfigV1 JSON from disk (fingerprintConfigPath). */
struct FingerprintConfig {
  bool ok = false;
  std::string seed;
  std::string user_agent;
  std::string platform;
  std::string language;
  std::string timezone;
  std::string raw_json;
  std::string error;
};

FingerprintConfig LoadFingerprintConfig(const std::string& path);

}  // namespace sc_cef

#pragma once

/**
 * Blink / net call-site glue for M1.
 *
 * After copying third_party/sc_fp into Chromium, wire these into:
 * - NavigatorBase::platform / userAgent / hardwareConcurrency
 * - ClientHints
 * - HttpNetworkTransaction Accept-Language
 * - TimezoneController
 *
 * Example (navigator_base.cc):
 *
 *   #include "third_party/sc_fp/sc_fp_engine.h"
 *   String NavigatorBase::platform() {
 *     std::string spoof;
 *     if (sc_fp::OverridePlatform(&spoof)) return String::FromUTF8(spoof);
 *     ... existing ...
 *   }
 */

#include "sc_fp_engine.h"

namespace sc_fp {

inline void ApplyFromFingerprintConfigPath(const std::string& path) {
  NavigatorNetworkConfig cfg;
  if (LoadEngineJsonFile(path, &cfg)) {
    SetActiveConfig(cfg);
  }
}

}  // namespace sc_fp

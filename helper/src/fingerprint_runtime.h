#pragma once

#include <string>

namespace sc_cef {

/**
 * Builds a document-start inject script from FingerprintEngineConfigV1 JSON.
 * Used by CefRenderProcessHandler / OnContextCreated until Blink patches land
 * in a custom CEF build. Same seeds as M1/M2/M3 C++ engines.
 */
std::string BuildNativeFingerprintInjectScript(const std::string& engine_json);

/** Apply process-wide M1/M2/M3 engine state from fingerprint-engine.json path. */
bool ActivateFingerprintEngines(const std::string& config_path, std::string* error);

/** True unless SC_CEF_FP_INJECT=0 (pure CDP / Blink-kernel comparison runs). */
bool FingerprintInjectEnabled();

}  // namespace sc_cef

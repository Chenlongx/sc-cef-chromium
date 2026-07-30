#pragma once

#include <map>
#include <string>
#include <vector>

namespace sc_cef {

/** Tiny JSON helpers for control protocol (no external deps). */
std::string JsonGetString(const std::string& json, const std::string& key, const std::string& fallback = {});
int JsonGetInt(const std::string& json, const std::string& key, int fallback = 0);
bool JsonGetBool(const std::string& json, const std::string& key, bool fallback = false);
std::string JsonEscape(const std::string& s);
std::string JsonGetObject(const std::string& json, const std::string& key);

struct CreateBrowserRequest {
  std::string id;
  int account_id = 0;
  std::string profile_path;
  std::string url;
  int x = 0;
  int y = 0;
  int width = 800;
  int height = 600;
  std::string parent_window_handle;
  /** "overlay" = borderless embed window; anything else = titled detector window. */
  std::string embed_mode;
  /** "cocoa" = bottom-left screen coords (macOS Electron); default treated as cocoa on mac. */
  std::string bounds_space;
  std::string proxy_mode;
  std::string proxy_server;
  std::string fingerprint_config_path;
};

CreateBrowserRequest ParseCreateBrowser(const std::string& line);
std::string RequestType(const std::string& line);
int RequestAccountId(const std::string& line);

}  // namespace sc_cef

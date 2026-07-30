#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sc_fp {

struct MediaDeviceSpec {
  std::string kind;  // audioinput|audiooutput|videoinput
  std::string label;
  std::string device_id;
};

struct MediaSpoofConfig {
  bool loaded = false;
  std::string seed;
  std::vector<std::string> font_profile;
  std::vector<MediaDeviceSpec> media_devices;
  std::string webrtc_mode;  // proxy_only | disabled | real
  std::string error;
};

void SetMediaConfig(const MediaSpoofConfig& cfg);
const MediaSpoofConfig& GetMediaConfig();

MediaSpoofConfig ParseMediaJson(const std::string& json);
bool LoadMediaJsonFile(const std::string& path, MediaSpoofConfig* out);

/** True if font family should appear in enumeration. Empty profile = allow all. */
bool AllowFontFamily(const std::string& family);

bool OverrideWebRtcMode(std::string* out);

}  // namespace sc_fp

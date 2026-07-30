#include "sc_fp_media.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>

namespace sc_fp {
namespace {

std::mutex g_mu;
MediaSpoofConfig g_cfg;

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

void FillMediaDevices(const std::string& json, MediaSpoofConfig* cfg) {
  auto pos = json.find("\"mediaDevices\"");
  if (pos == std::string::npos) return;
  // Support either array or object with audioInputs/videoInputs counts via labels arrays.
  auto arr = json.find('[', pos);
  auto brace = json.find('{', pos);
  if (arr != std::string::npos && (brace == std::string::npos || arr < brace)) {
    // Array of {kind,label,deviceId}
    auto end = json.find(']', arr);
    if (end == std::string::npos) return;
    const auto body = json.substr(arr + 1, end - arr - 1);
    size_t i = 0;
    while ((i = body.find('{', i)) != std::string::npos) {
      auto j = body.find('}', i);
      if (j == std::string::npos) break;
      const auto obj = body.substr(i, j - i + 1);
      MediaDeviceSpec d;
      d.kind = ExtractString(obj, "kind");
      d.label = ExtractString(obj, "label");
      d.device_id = ExtractString(obj, "deviceId");
      if (d.kind.empty()) d.kind = "audioinput";
      cfg->media_devices.push_back(d);
      i = j + 1;
    }
    return;
  }
  if (brace == std::string::npos) return;
  // Object form — synthesize minimal devices from counts if present.
  MediaDeviceSpec mic;
  mic.kind = "audioinput";
  mic.label = "Default Microphone";
  mic.device_id = "sc-mic-0";
  MediaDeviceSpec cam;
  cam.kind = "videoinput";
  cam.label = "Default Camera";
  cam.device_id = "sc-cam-0";
  MediaDeviceSpec speaker;
  speaker.kind = "audiooutput";
  speaker.label = "Default Speaker";
  speaker.device_id = "sc-spk-0";
  cfg->media_devices = {mic, speaker, cam};
}

}  // namespace

void SetMediaConfig(const MediaSpoofConfig& cfg) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_cfg = cfg;
}

const MediaSpoofConfig& GetMediaConfig() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_cfg;
}

MediaSpoofConfig ParseMediaJson(const std::string& json) {
  MediaSpoofConfig cfg;
  cfg.seed = ExtractString(json, "seed");
  cfg.font_profile = ExtractStringArray(json, "fontProfile");
  cfg.webrtc_mode = ExtractString(json, "webRtcMode");
  if (cfg.webrtc_mode.empty()) cfg.webrtc_mode = "proxy_only";
  FillMediaDevices(json, &cfg);
  cfg.loaded = !cfg.seed.empty() || !cfg.font_profile.empty() || !cfg.media_devices.empty();
  if (!cfg.loaded) cfg.error = "missing media spoof fields";
  return cfg;
}

bool LoadMediaJsonFile(const std::string& path, MediaSpoofConfig* out) {
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
  auto cfg = ParseMediaJson(ss.str());
  if (out) *out = cfg;
  return cfg.loaded;
}

bool AllowFontFamily(const std::string& family) {
  const auto& c = GetMediaConfig();
  if (!c.loaded || c.font_profile.empty()) return true;
  return std::find(c.font_profile.begin(), c.font_profile.end(), family) != c.font_profile.end();
}

bool OverrideWebRtcMode(std::string* out) {
  const auto& c = GetMediaConfig();
  if (!c.loaded || c.webrtc_mode.empty() || !out) return false;
  *out = c.webrtc_mode;
  return true;
}

}  // namespace sc_fp

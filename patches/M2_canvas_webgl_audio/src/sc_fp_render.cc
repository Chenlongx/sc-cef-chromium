#include "sc_fp_render.h"

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

namespace sc_fp {
namespace {

std::mutex g_mu;
RenderSpoofConfig g_cfg;

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

uint32_t ExtractU32(const std::string& json, const std::string& key, uint32_t fallback = 0) {
  const auto needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return fallback;
  while (pos + 1 < json.size() && (json[pos + 1] == ' ' || json[pos + 1] == '\t')) ++pos;
  char* end = nullptr;
  const unsigned long v = std::strtoul(json.c_str() + pos + 1, &end, 10);
  if (end == json.c_str() + pos + 1) return fallback;
  return static_cast<uint32_t>(v);
}

void FillWebGl(const std::string& json, RenderSpoofConfig* cfg) {
  auto w = json.find("\"webgl\"");
  if (w == std::string::npos) return;
  auto brace = json.find('{', w);
  auto end = json.find('}', brace);
  if (brace == std::string::npos || end == std::string::npos) return;
  const auto body = json.substr(brace, end - brace + 1);
  cfg->webgl_vendor = ExtractString(body, "vendor");
  cfg->webgl_renderer = ExtractString(body, "renderer");
}

}  // namespace

void SetRenderConfig(const RenderSpoofConfig& cfg) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_cfg = cfg;
}

const RenderSpoofConfig& GetRenderConfig() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_cfg;
}

RenderSpoofConfig ParseRenderJson(const std::string& json) {
  RenderSpoofConfig cfg;
  cfg.seed = ExtractString(json, "seed");
  cfg.canvas_seed = ExtractU32(json, "canvasSeed", 0);
  cfg.audio_seed = ExtractU32(json, "audioSeed", 0);
  FillWebGl(json, &cfg);
  cfg.loaded = cfg.canvas_seed != 0 || cfg.audio_seed != 0 || !cfg.webgl_vendor.empty() ||
               !cfg.seed.empty();
  if (!cfg.loaded) cfg.error = "missing canvas/audio/webgl seeds";
  return cfg;
}

bool LoadRenderJsonFile(const std::string& path, RenderSpoofConfig* out) {
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
  auto cfg = ParseRenderJson(ss.str());
  if (out) *out = cfg;
  return cfg.loaded;
}

uint32_t SeededNoise(uint32_t seed, uint32_t index) {
  uint32_t x = seed ^ (index * 0x9E3779B9u);
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

void ApplyCanvasNoise(uint8_t* rgba, size_t pixel_count, uint32_t canvas_seed) {
  if (!rgba || pixel_count == 0 || canvas_seed == 0) return;
  // Sparse LSB flip — keeps visuals intact, changes hash.
  for (size_t i = 0; i < pixel_count; i += 17) {
    const uint32_t n = SeededNoise(canvas_seed, static_cast<uint32_t>(i));
    const size_t off = i * 4;
    rgba[off + 0] ^= static_cast<uint8_t>(n & 1u);
    rgba[off + 1] ^= static_cast<uint8_t>((n >> 1) & 1u);
    rgba[off + 2] ^= static_cast<uint8_t>((n >> 2) & 1u);
  }
}

void ApplyAudioDither(float* samples, size_t count, uint32_t audio_seed) {
  if (!samples || count == 0 || audio_seed == 0) return;
  const size_t n = count < 256 ? count : 256;
  for (size_t i = 0; i < n; ++i) {
    const uint32_t r = SeededNoise(audio_seed, static_cast<uint32_t>(i));
    const float delta = (static_cast<int>(r % 7) - 3) * 1e-7f;
    samples[i] += delta;
  }
}

bool OverrideWebGlVendor(std::string* out) {
  const auto& c = GetRenderConfig();
  if (!c.loaded || c.webgl_vendor.empty() || !out) return false;
  *out = c.webgl_vendor;
  return true;
}

bool OverrideWebGlRenderer(std::string* out) {
  const auto& c = GetRenderConfig();
  if (!c.loaded || c.webgl_renderer.empty() || !out) return false;
  *out = c.webgl_renderer;
  return true;
}

}  // namespace sc_fp

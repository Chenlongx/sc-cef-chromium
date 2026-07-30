#pragma once

#include <cstdint>
#include <string>

namespace sc_fp {

struct RenderSpoofConfig {
  bool loaded = false;
  std::string seed;
  uint32_t canvas_seed = 0;
  uint32_t audio_seed = 0;
  std::string webgl_vendor;
  std::string webgl_renderer;
  std::string error;
};

void SetRenderConfig(const RenderSpoofConfig& cfg);
const RenderSpoofConfig& GetRenderConfig();

RenderSpoofConfig ParseRenderJson(const std::string& json);
bool LoadRenderJsonFile(const std::string& path, RenderSpoofConfig* out);

/** Deterministic xorshift from seed+channel — same inputs ⇒ same noise. */
uint32_t SeededNoise(uint32_t seed, uint32_t index);

/**
 * Apply subtle RGBA noise in-place (canvas getImageData / toDataURL path).
 * Mutates at most 1 LSB per channel on a sparse subset of pixels.
 */
void ApplyCanvasNoise(uint8_t* rgba, size_t pixel_count, uint32_t canvas_seed);

/** Dither first N float samples for AudioBuffer copy paths. */
void ApplyAudioDither(float* samples, size_t count, uint32_t audio_seed);

bool OverrideWebGlVendor(std::string* out);
bool OverrideWebGlRenderer(std::string* out);

}  // namespace sc_fp

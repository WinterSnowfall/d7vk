#pragma once

namespace dxvk::ddrawCaps {

  constexpr uint32_t MaxClipPlanes           = 6;
  constexpr uint32_t MaxTextureDimension     = 8192; // TODO: Check native D3D7

  constexpr uint32_t MaxSimultaneousTextures = 8;
  constexpr uint32_t TextureStageCount       = MaxSimultaneousTextures;
  constexpr uint32_t MaxTextureBlendStages   = MaxSimultaneousTextures;

  constexpr uint32_t MaxEnabledLights        = 8;

  constexpr uint8_t  IndexBufferCount        = 5;

  // ZBIAS can be an integer from 0 to 16 and needs to be remapped to float
  constexpr float    ZBIAS_SCALE             = -1.0f / ((1u << 16) - 1); // Consider D16 precision
  constexpr float    ZBIAS_SCALE_INV         = 1 / ZBIAS_SCALE;

}
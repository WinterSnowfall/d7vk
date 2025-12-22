#pragma once

#include "ddraw_include.h"

namespace dxvk::ddrawCaps {

  constexpr uint32_t MaxClipPlanes                = 6;
  constexpr uint32_t MaxTextureDimension          = 8192; // TODO: Check native D3D7

  constexpr uint32_t MaxSimultaneousTextures      = 8;
  constexpr uint32_t TextureStageCount            = MaxSimultaneousTextures;
  constexpr uint32_t MaxTextureBlendStages        = MaxSimultaneousTextures;

  constexpr uint32_t MaxEnabledLights             = 8;

  constexpr uint32_t IndexBufferCount             = 5;

}
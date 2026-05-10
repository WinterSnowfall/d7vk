#pragma once

#include "ddraw_include.h"

#include "../util/config/config.h"
#include "../util/util_math.h"

namespace dxvk {

  enum class FSAAEmulation {
    Disabled,
    Enabled,
    Forced
  };

  enum class D3DLegacyPresentGuard {
    Auto,
    Disabled,
    Strict
  };

  struct D3DOptions {

    /// Enforces the use of DDSCL_MULTITHREADED
    bool forceMultiThreaded;

    /// Use SWVP mode for all D3D9 devices
    bool forceSWVP;

    /// Advertise support for R3G3B2
    bool supportR3G3B2;

    /// Advertise support for D16
    bool supportD16;

    /// Advertise support for any 32-bit bitmask depth foramts
    bool support32BitDepth;

    /// Replaces any use of D32 with D24X8
    bool useD24X8forD32;

    /// Report any 8-bit display modes as being 16-bit
    bool mask8BitModes;

    /// Report POW2 texture dimension restrictions
    bool forcePOW2Textures;

    /// Respect DISCARD only on DYNAMIC + WRITEONLY buffers
    bool forceLegacyDiscard;

    /// Process vertices on the CPU, instead of relaying to D3D9
    bool cpuProcessVertices;

    /// Correction offset for X/Y vertex position
    float vertexOffset;

    /// Resize the back buffer size to screen size when needed
    bool backBufferResize;

    /// Blits back to the proxied flippable surface and back again for presentation
    bool forceLegacyPresent;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Forces windowed mode presentation in EXCLUSIVE/FULLSCREEN mode
    bool ignoreExclusiveMode;

    /// Automatically generate all texture mip maps on the GPU
    bool autoGenMipMaps;

    /// Allow cross-device resource (surfaces/textures) use
    bool deviceResourceSharing;

    /// Masks the color key values based on surface format color depth
    bool colorKeyMasking;

    /// Uses a tolerance interval for color key inverval matching
    bool colorKeyTolerance;

    /// Extends features and relaxes validations to enable apitrace debugging
    bool apitraceMode;

    /// Enumerate with legacy/official implementation device names
    bool legacyDeviceNames;

    /// By default guards against legacy presents while inside of a scene
    D3DLegacyPresentGuard legacyPresentGuard;

    /// Uses supported MSAA up to 4x to emulate FSAA
    FSAAEmulation emulateFSAA;

    D3DOptions() {}

    D3DOptions(const Config& config) {
      // D3D7/6/5/DDraw options
      this->forceMultiThreaded    = config.getOption<bool>   ("ddraw.forceMultiThreaded",    false);
      this->forceSWVP             = config.getOption<bool>   ("ddraw.forceSWVP",             false);
      this->supportR3G3B2         = config.getOption<bool>   ("ddraw.supportR3G3B2",         false);
      this->supportD16            = config.getOption<bool>   ("ddraw.supportD16",             true);
      this->support32BitDepth     = config.getOption<bool>   ("ddraw.support32BitDepth",      true);
      this->useD24X8forD32        = config.getOption<bool>   ("ddraw.useD24X8forD32",        false);
      this->mask8BitModes         = config.getOption<bool>   ("ddraw.mask8BitModes",         false);
      this->forcePOW2Textures     = config.getOption<bool>   ("ddraw.forcePOW2Textures",     false);
      this->forceLegacyDiscard    = config.getOption<bool>   ("ddraw.forceLegacyDiscard",    false);
      this->cpuProcessVertices    = config.getOption<bool>   ("ddraw.cpuProcessVertices",     true);
      this->vertexOffset          = config.getOption<float>  ("ddraw.vertexOffset",           0.0f);
      this->backBufferResize      = config.getOption<bool>   ("ddraw.backBufferResize",       true);
      this->forceLegacyPresent    = config.getOption<bool>   ("ddraw.forceLegacyPresent",    false);
      this->ignoreGammaRamp       = config.getOption<bool>   ("ddraw.ignoreGammaRamp",       false);
      this->ignoreExclusiveMode   = config.getOption<bool>   ("ddraw.ignoreExclusiveMode",   false);
      this->autoGenMipMaps        = config.getOption<bool>   ("ddraw.autoGenMipMaps",        false);
      this->deviceResourceSharing = config.getOption<bool>   ("ddraw.deviceResourceSharing", false);
      this->colorKeyMasking       = config.getOption<bool>   ("ddraw.colorKeyMasking",       false);
      this->colorKeyTolerance     = config.getOption<bool>   ("ddraw.colorKeyTolerance",     false);
      this->legacyDeviceNames     = config.getOption<bool>   ("ddraw.legacyDeviceNames",     false);
      this->apitraceMode          = config.getOption<bool>   ("ddraw.apitraceMode",          false);

      // Clamp the vertex offset in the (sensible) -1.0f/1.0f range
      this->vertexOffset = dxvk::fclamp(this->vertexOffset, -1.0f, 1.0f);

      std::string legacyPresentGuardStr = Config::toLower(config.getOption<std::string>("ddraw.legacyPresentGuard", "auto"));
      if (legacyPresentGuardStr == "strict") {
        this->legacyPresentGuard = D3DLegacyPresentGuard::Strict;
      } else if (legacyPresentGuardStr == "disabled") {
        this->legacyPresentGuard = D3DLegacyPresentGuard::Disabled;
      } else {
        this->legacyPresentGuard = D3DLegacyPresentGuard::Auto;
      }

      std::string emulateFSAAStr = Config::toLower(config.getOption<std::string>("ddraw.emulateFSAA", "auto"));
      if (emulateFSAAStr == "true") {
        this->emulateFSAA = FSAAEmulation::Enabled;
      } else if (emulateFSAAStr == "forced") {
        this->emulateFSAA = FSAAEmulation::Forced;
      } else {
        this->emulateFSAA = FSAAEmulation::Disabled;
      }
    }

  };

}

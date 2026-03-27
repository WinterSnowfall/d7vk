#pragma once

#include "ddraw_include.h"

#include "../util/config/config.h"

namespace dxvk {

  enum class FSAAEmulation {
    Disabled,
    Enabled,
    Forced
  };

  enum class D3DBackBufferGuard {
    Disabled,
    Enabled,
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

    /// Replaces any use of D32 with D24X8
    bool useD24X8forD32;

    /// Report any 8-bit display modes as being 16-bit
    bool mask8BitModes;

    /// Report POW2 texture dimension restrictions
    bool forcePOW2Textures;

    /// Respect DISCARD only on DYNAMIC + WRITEONLY buffers
    bool forceLegacyDiscard;

    /// Circumvents the texelFetch color key shader path
    bool colorKeyCompatibility;

    /// Map all back buffers onto a single D3D9 back buffer
    bool forceSingleBackBuffer;

    /// Resize the back buffer size to screen size when needed
    bool backBufferResize;

    /// Write back D3D9 back buffers during blits/locks on DDraw surfaces
    bool backBufferWriteBack;

    /// Write back D3D9 depth stencils during blits/locks on DDraw surfaces
    bool depthWriteBack;

    /// Upload or skip upload of depth stencils
    bool uploadDepthStencils;

    /// Blits back to the proxied flippable surface and presents with DDraw
    bool forceProxiedPresent;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Forces windowed mode presentation in EXCLUSIVE/FULLSCREEN mode
    bool ignoreExclusiveMode;

    /// Automatically generate all texture mip maps on the GPU
    bool autoGenMipMaps;

    /// Enables all surface write-backs, but comes with performance penalties
    bool apitraceMode;

    /// Uses supported MSAA up to 4x to emulate FSAA
    FSAAEmulation emulateFSAA;

    /// Determines how uploads for back buffer blits are handled
    D3DBackBufferGuard backBufferGuard;

    /// Max available memory override, shared with the D3D9 backend
    uint32_t maxAvailableMemory;

    D3DOptions() {}

    D3DOptions(const Config& config) {
      // D3D7/6/5/DDraw options
      this->forceMultiThreaded    = config.getOption<bool>   ("ddraw.forceMultiThreaded",    false);
      this->forceSWVP             = config.getOption<bool>   ("ddraw.forceSWVP",             false);
      this->supportR3G3B2         = config.getOption<bool>   ("ddraw.supportR3G3B2",         false);
      this->supportD16            = config.getOption<bool>   ("ddraw.supportD16",             true);
      this->useD24X8forD32        = config.getOption<bool>   ("ddraw.useD24X8forD32",        false);
      this->mask8BitModes         = config.getOption<bool>   ("ddraw.mask8BitModes",         false);
      this->forcePOW2Textures     = config.getOption<bool>   ("ddraw.forcePOW2Textures",     false);
      this->forceLegacyDiscard    = config.getOption<bool>   ("ddraw.forceLegacyDiscard",    false);
      this->colorKeyCompatibility = config.getOption<bool>   ("ddraw.colorKeyCompatibility", false);
      this->forceSingleBackBuffer = config.getOption<bool>   ("ddraw.forceSingleBackBuffer", false);
      this->backBufferResize      = config.getOption<bool>   ("ddraw.backBufferResize",       true);
      this->backBufferWriteBack   = config.getOption<bool>   ("ddraw.backBufferWriteBack",   false);
      this->depthWriteBack        = config.getOption<bool>   ("ddraw.depthWriteBack",        false);
      this->uploadDepthStencils   = config.getOption<bool>   ("ddraw.uploadDepthStencils",    true);
      this->forceProxiedPresent   = config.getOption<bool>   ("ddraw.forceProxiedPresent",   false);
      this->ignoreGammaRamp       = config.getOption<bool>   ("ddraw.ignoreGammaRamp",       false);
      this->ignoreExclusiveMode   = config.getOption<bool>   ("ddraw.ignoreExclusiveMode",   false);
      this->autoGenMipMaps        = config.getOption<bool>   ("ddraw.autoGenMipMaps",        false);
      this->apitraceMode          = config.getOption<bool>   ("ddraw.apitraceMode",          false);
      // D3D9 options
      this->maxAvailableMemory    = config.getOption<int32_t>("d3d9.maxAvailableMemory",      2040);

      std::string emulateFSAAStr = Config::toLower(config.getOption<std::string>("ddraw.emulateFSAA", "auto"));
      if (emulateFSAAStr == "true") {
        this->emulateFSAA = FSAAEmulation::Enabled;
      } else if (emulateFSAAStr == "forced") {
        this->emulateFSAA = FSAAEmulation::Forced;
      } else {
        this->emulateFSAA = FSAAEmulation::Disabled;
      }

      std::string backBufferGuardStr = Config::toLower(config.getOption<std::string>("ddraw.backBufferGuard", "auto"));
      if (backBufferGuardStr == "strict") {
        this->backBufferGuard = D3DBackBufferGuard::Strict;
      } else if (backBufferGuardStr == "disabled") {
        this->backBufferGuard = D3DBackBufferGuard::Disabled;
      } else {
        this->backBufferGuard = D3DBackBufferGuard::Enabled;
      }
    }

  };

}

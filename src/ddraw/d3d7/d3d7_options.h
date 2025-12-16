#pragma once

#include "../ddraw_include.h"
#include "../util/config/config.h"

namespace dxvk {

  enum class D3D7BackBufferGuard {
    Disabled,
    Enabled,
    Strict
  };

  struct D3D7Options {

    /// Creates a SWVP D3D9 device even when a T&L HAL (HWVP) device is requested
    bool forceSWVPDevice;

    /// Fully disables support for AA, lowering memory bandwidth pressure
    bool disableAASupport;

    /// Forces enables AA, regardless of application preference
    bool forceEnableAA;

    /// Blits back to the proxied render target and flips the surface -
    /// this is currently required by any game that blits cursors directly onto the front buffer
    bool forceProxiedPresent;

    /// Map all back buffers onto a single D3D9 back buffer. Some applications have broken flip
    /// implementations or simply do not use all the back buffers they create, which causes issues.
    bool forceSingleBackBuffer;

    /// If we detect a back buffer size larger than the application set display mode during device
    /// creation, we will use the mode size for the D3D9 back buffer. This is useful for full-screen
    /// presentation on Wayland, or in other situations when back buffer dimensions get altered in-flight.
    bool backBufferResize;

    /// Presents on every EndScene call as well, which may help with video playback in some cases
    bool presentOnEndScene;

    /// Forward query interface calls to the proxied objects
    bool proxiedQueryInterface;

    /// Allows or denies query interface calls to older DDraw/D3D interfaces. Used for debugging.
    bool legacyQueryInterface;

    /// Determines how to handle proxy back buffer blits done by the application
    D3D7BackBufferGuard backBufferGuard;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Automatically generate texture mip maps and ignore those copied (or not copied)
    /// by the application - this is currently used as a workaround for all UE1 titles
    bool autoGenMipMaps;

    /// Place vertex buffers in the MANAGED pool when using a T&L HAL device
    bool managedTNLBuffers;

    /// Max available memory override, shared with the D3D9 backend
    uint32_t maxAvailableMemory;

    D3D7Options() {}

    D3D7Options(const Config& config) {
      this->forceSWVPDevice       = config.getOption<bool>   ("d3d7.forceSWVPDevice",       false);
      this->disableAASupport      = config.getOption<bool>   ("d3d7.disableAASupport",      false);
      this->forceEnableAA         = config.getOption<bool>   ("d3d7.forceEnableAA",         false);
      this->forceProxiedPresent   = config.getOption<bool>   ("d3d7.forceProxiedPresent",   false);
      this->forceSingleBackBuffer = config.getOption<bool>   ("d3d7.forceSingleBackBuffer", false);
      this->backBufferResize      = config.getOption<bool>   ("d3d7.backBufferResize",      true);
      this->presentOnEndScene     = config.getOption<bool>   ("d3d7.presentOnEndScene",     false);
      this->proxiedQueryInterface = config.getOption<bool>   ("d3d7.proxiedQueryInterface", false);
      this->legacyQueryInterface  = config.getOption<bool>   ("d3d7.legacyQueryInterface",  true);
      this->ignoreGammaRamp       = config.getOption<bool>   ("d3d7.ignoreGammaRamp",       false);
      this->autoGenMipMaps        = config.getOption<bool>   ("d3d7.autoGenMipMaps",        false);
      this->managedTNLBuffers     = config.getOption<bool>   ("d3d7.managedTNLBuffers",     false);

      // D3D9 options
      this->maxAvailableMemory    = config.getOption<int32_t>("d3d9.maxAvailableMemory",     1024);

      std::string backBufferGuardStr = Config::toLower(config.getOption<std::string> ("d3d7.backBufferGuard", "auto"));
      if (backBufferGuardStr == "strict") {
        this->backBufferGuard = D3D7BackBufferGuard::Strict;
      } else if (backBufferGuardStr == "disabled") {
        this->backBufferGuard = D3D7BackBufferGuard::Disabled;
      } else {
        this->backBufferGuard = D3D7BackBufferGuard::Enabled;
      }
    }

  };

}

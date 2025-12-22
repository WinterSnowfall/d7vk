#pragma once

#include "ddraw_include.h"

#include "../util/config/config.h"

namespace dxvk {

  enum class D3DBackBufferGuard {
    Disabled,
    Enabled,
    Strict
  };

  struct D3DOptions {

    /// Creates a SWVP D3D9 device even when a T&L HAL (HWVP) device is requested
    bool forceSWVPDevice;

    /// Fully disables support for AA, lowering memory bandwidth pressure
    bool disableAASupport;

    /// Forces enables AA, regardless of application preference
    bool forceEnableAA;

    /// Blits back to the proxied render target and flips the surface.
    /// This is currently required by any game that blits cursors or
    /// does presentation through blits directly onto the front buffer.
    bool forceProxiedPresent;

    /// Map all back buffers onto a single D3D9 back buffer. Some applications have broken flip
    /// implementations or simply do not use all the back buffers they create, which causes issues.
    bool forceSingleBackBuffer;

    /// Force enable multithreaded protections. Some applications that don't
    /// properly submit DDSCL_MULTITHREADED flags will crash otherwise.
    bool forceMultiThreaded;

    /// If we detect a back buffer size larger than the application set display mode during device
    /// creation, we will use the mode size for the D3D9 back buffer. This is useful for full-screen
    /// presentation on Wayland, or in other situations when back buffer dimensions get altered in-flight.
    bool backBufferResize;

    /// Presents on every EndScene call as well, which may help with video playback in some cases
    bool presentOnEndScene;

    /// Automatically generate texture mip maps and ignore those copied (or not copied)
    /// by the application. This is currently used as a workaround for all UE1 titles.
    bool autoGenMipMaps;

    /// Place vertex buffers in the MANAGED pool when using a T&L HAL device
    bool managedTNLBuffers;

    /// Replaces any use of D32 with D24X8. Needed for games such as
    /// Sacrifice, which won't enable 32-bit modes without D32 support.
    bool useD24X8forD32;

    /// Max available memory override, shared with the D3D9 backend
    uint32_t maxAvailableMemory;

    /// Forward query interface calls to the proxied objects
    bool proxiedQueryInterface;

    /// Forward surface creation calls to the proxied ddraw interfaces
    bool proxiedLegacySurfaces;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Determines how to handle proxy back buffer blits done by the application
    D3DBackBufferGuard backBufferGuard;

    D3DOptions() {}

    D3DOptions(const Config& config) {
      this->forceSWVPDevice       = config.getOption<bool>   ("d3d7.forceSWVPDevice",        false);
      this->disableAASupport      = config.getOption<bool>   ("d3d7.disableAASupport",       false);
      this->forceEnableAA         = config.getOption<bool>   ("d3d7.forceEnableAA",          false);
      this->forceProxiedPresent   = config.getOption<bool>   ("d3d7.forceProxiedPresent",    false);
      this->forceSingleBackBuffer = config.getOption<bool>   ("d3d7.forceSingleBackBuffer",  false);
      this->forceMultiThreaded    = config.getOption<bool>   ("d3d7.forceMultiThreaded",     false);
      this->backBufferResize      = config.getOption<bool>   ("d3d7.backBufferResize",       true);
      this->presentOnEndScene     = config.getOption<bool>   ("d3d7.presentOnEndScene",      false);
      this->autoGenMipMaps        = config.getOption<bool>   ("d3d7.autoGenMipMaps",         false);
      this->useD24X8forD32        = config.getOption<bool>   ("d3d7.useD24X8forD32",         false);
      this->managedTNLBuffers     = config.getOption<bool>   ("d3d7.managedTNLBuffers",      false);
      // DDraw options
      this->proxiedQueryInterface = config.getOption<bool>   ("ddraw.proxiedQueryInterface", false);
      this->proxiedLegacySurfaces = config.getOption<bool>   ("ddraw.proxiedLegacySurfaces", false);
      this->ignoreGammaRamp       = config.getOption<bool>   ("ddraw.ignoreGammaRamp",       false);
      // D3D9 options
      this->maxAvailableMemory    = config.getOption<int32_t>("d3d9.maxAvailableMemory",     1024);

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

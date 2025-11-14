#pragma once

#include "d3d7_include.h"
#include "../util/config/config.h"

namespace dxvk {

  struct D3D7Options {

    /// Forces a desired MSAA level on the d3d9 device/default swapchain
    int32_t forceMSAA;

    /// Blits back to the proxied render target and flips the surface -
    /// this is currently required by any game that blits cursors directly onto the front buffer
    bool forceProxiedPresent;

    /// Presents on every EndScene call as well, which may help with video playback in some cases
    bool presentOnEndScene;

    /// Forward query interface calls to the proxied objects
    bool proxiedQueryInterface;

    /// Ignore all direct proxy back buffer blits done by the application
    bool strictBackBufferGuard;

    /// Bypass direct uploads to d3d9 and proxy all GetDC calls on ddraw surfaces
    bool proxiedGetDC;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Automatically generate texture mip maps and ignore those copied (or not copied)
    /// by the application - this is currently used as a workaround for all UE1 titles
    bool autoGenMipMaps;

    D3D7Options() {}

    D3D7Options(const Config& config) {
      this->forceMSAA             = config.getOption<int32_t>("d3d7.forceMSAA",                -1);
      this->forceProxiedPresent   = config.getOption<bool>   ("d3d7.forceProxiedPresent",   false);
      this->presentOnEndScene     = config.getOption<bool>   ("d3d7.presentOnEndScene",     false);
      this->proxiedQueryInterface = config.getOption<bool>   ("d3d7.proxiedQueryInterface", false);
      this->strictBackBufferGuard = config.getOption<bool>   ("d3d7.strictBackBufferGuard", false);
      this->proxiedGetDC          = config.getOption<bool>   ("d3d7.proxiedGetDC",          false);
      this->ignoreGammaRamp       = config.getOption<bool>   ("d3d7.ignoreGammaRamp",       false);
      this->autoGenMipMaps        = config.getOption<bool>   ("d3d7.autoGenMipMaps",        false);
    }

  };

}

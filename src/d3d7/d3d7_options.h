#pragma once

#include "d3d7_include.h"
#include "../util/config/config.h"

namespace dxvk {

  struct D3D7Options {

    /// Forces a desired MSAA level on the d3d9 device/default swapchain
    int32_t forceMSAA;

    /// Presents on every EndScene call as well, which may help with video playback in some cases
    bool presentOnEndScene;

    /// Forward query interface calls to the proxied objects
    bool proxiedQueryInterface;

    /// Ignore all direct proxy back buffer blits done by the application
    bool strictBackBufferGuard;

    /// Bypass direct uploads to d3d9 and proxy all GetDC calls on ddraw surfaces
    bool proxiedGetDC;

    D3D7Options() {}

    D3D7Options(const Config& config) {
      this->forceMSAA             = config.getOption<int32_t>("d3d7.forceMSAA",                -1);
      this->presentOnEndScene     = config.getOption<bool>   ("d3d7.presentOnEndScene",     false);
      this->proxiedQueryInterface = config.getOption<bool>   ("d3d7.proxiedQueryInterface", false);
      this->strictBackBufferGuard = config.getOption<bool>   ("d3d7.strictBackBufferGuard", false);
      this->proxiedGetDC          = config.getOption<bool>   ("d3d7.proxiedGetDC",          false);
    }

  };

}

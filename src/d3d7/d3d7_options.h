#pragma once

#include "d3d7_include.h"
#include "../util/config/config.h"

namespace dxvk {

  struct D3D7Options {

    /// Ignore all direct proxy back buffer blits done by the application
    bool strictBackBufferGuard;

    /// Bypass direct uploads to d3d9 and proxy all GetDC calls on ddraw surfaces
    bool proxiedGetDC;

    D3D7Options() {}

    D3D7Options(const Config& config) {
      this->strictBackBufferGuard = config.getOption<bool>   ("d3d7.strictBackBufferGuard", false);
      this->proxiedGetDC          = config.getOption<bool>   ("d3d7.proxiedGetDC",          false);
    }

  };

}

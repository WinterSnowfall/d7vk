#pragma once

#include "ddraw_include.h"

#include "../util/config/config.h"

namespace dxvk {

  enum class FSAAEmulation {
    Disabled,
    Enabled,
    Forced
  };

  enum class D3DDeviceTypeOverride {
    None,
    SWVP,
    SWVPMixed,
    HWVP,
    HWVPMixed
  };

  enum class D3DBackBufferGuard {
    Disabled,
    Enabled,
    Strict
  };

  struct D3DOptions {

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

    /// Determines if the currently set D3D9 Z buffer is written back
    /// during blits/locks on DDraw surfaces. Default disabled, as it
    /// negatively affects performance, but is sometimes needed for corectness.
    bool depthWriteBack;

    /// Replaces any use of D32 with D24X8. Needed for games such as
    /// Sacrifice, which won't enable 32-bit modes without D32 support.
    bool useD24X8forD32;

    /// Proxies all D3D5 calls, to allow mixed D3D3 execute buffer use
    bool proxiedExecuteBuffers;

    /// Max available memory override, shared with the D3D9 backend
    uint32_t maxAvailableMemory;

    /// Blits back to the proxied render target and flips the surface.
    /// This is currently required by any game that blits cursors or
    /// does presentation through blits directly onto the front/back buffers.
    bool forceProxiedPresent;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Forces windowed mode presentation (direct blits to the primary surface),
    /// even in exclusive full-screen, since some games rely on it for presentation
    bool ignoreExclusiveMode;

    /// Automatically generate texture mip maps and ignore those copied (or not copied)
    /// by the application. This is currently used as a workaround for all UE1 titles.
    bool autoGenMipMaps;

    /// Immediately perform all texture related operations and uploads instead of dirtying.
    /// Will negatively affect performance and is only useful for debugging.
    bool apitraceMode;

    /// Uses supported MSAA up to 4x to emulate D3D5 and higher order-dependent
    /// and order-independent full scene anti-aliasing. Disabled by default.
    FSAAEmulation emulateFSAA;

    /// Force the use of a certain D3D9 device type. Sometimes needed to handle dubious
    /// application buffer placement and/or other types of SWVP-related issues.
    D3DDeviceTypeOverride deviceTypeOverride;

    /// Determines how to handle proxy back buffer blits done by the application
    D3DBackBufferGuard backBufferGuard;

    D3DOptions() {}

    D3DOptions(const Config& config) {
      // D3D7/6/5 options
      this->forceMultiThreaded    = config.getOption<bool>   ("d3d7.forceMultiThreaded",     false);
      this->useD24X8forD32        = config.getOption<bool>   ("d3d7.useD24X8forD32",         false);
      this->proxiedExecuteBuffers = config.getOption<bool>   ("d3d7.proxiedExecuteBuffers",  false);
      // DDraw options
      this->forceSingleBackBuffer = config.getOption<bool>   ("ddraw.forceSingleBackBuffer", false);
      this->backBufferResize      = config.getOption<bool>   ("ddraw.backBufferResize",       true);
      this->depthWriteBack        = config.getOption<bool>   ("ddraw.depthWriteBack",        false);
      this->forceProxiedPresent   = config.getOption<bool>   ("ddraw.forceProxiedPresent",   false);
      this->ignoreGammaRamp       = config.getOption<bool>   ("ddraw.ignoreGammaRamp",       false);
      this->ignoreExclusiveMode   = config.getOption<bool>   ("ddraw.ignoreExclusiveMode",   false);
      this->autoGenMipMaps        = config.getOption<bool>   ("ddraw.autoGenMipMaps",        false);
      this->apitraceMode          = config.getOption<bool>   ("ddraw.apitraceMode",          false);
      // D3D9 options
      this->maxAvailableMemory    = config.getOption<int32_t>("d3d9.maxAvailableMemory",      1024);

      std::string emulateFSAAStr = Config::toLower(config.getOption<std::string>("d3d7.emulateFSAA", "auto"));
      if (emulateFSAAStr == "true") {
        this->emulateFSAA = FSAAEmulation::Enabled;
      } else if (emulateFSAAStr == "forced") {
        this->emulateFSAA = FSAAEmulation::Forced;
      } else {
        this->emulateFSAA = FSAAEmulation::Disabled;
      }

      std::string deviceTypeOverrideStr = Config::toLower(config.getOption<std::string>("d3d7.deviceTypeOverride", "auto"));
      if (deviceTypeOverrideStr == "swvp") {
        this->deviceTypeOverride = D3DDeviceTypeOverride::SWVP;
      } else if (deviceTypeOverrideStr == "swvpmixed") {
        this->deviceTypeOverride = D3DDeviceTypeOverride::SWVPMixed;
      } else if (deviceTypeOverrideStr == "hwvp") {
        this->deviceTypeOverride = D3DDeviceTypeOverride::HWVP;
      } else if (deviceTypeOverrideStr == "hwvpmixed") {
        this->deviceTypeOverride = D3DDeviceTypeOverride::HWVPMixed;
      } else {
        this->deviceTypeOverride = D3DDeviceTypeOverride::None;
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

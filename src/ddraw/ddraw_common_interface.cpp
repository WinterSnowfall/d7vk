#include "ddraw_common_interface.h"

#include <algorithm>

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw7/ddraw7_surface.h"

#include "d3d5/d3d5_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d7/d3d7_device.h"

namespace dxvk {

  DDrawCommonInterface::DDrawCommonInterface(const D3DOptions& options)
    : m_options ( options ) {
  }

  DDrawCommonInterface::~DDrawCommonInterface() {
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
    if (likely(it != m_surfaces.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
      if (unlikely(it != m_surfaces.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), surface);
      if (likely(it != m_surfaces.end())) {
        m_surfaces.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface2* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces2.begin(), m_surfaces2.end(), surface);
    if (likely(it != m_surfaces2.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface2* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces2.begin(), m_surfaces2.end(), surface);
      if (unlikely(it != m_surfaces2.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces2.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface2* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces2.begin(), m_surfaces2.end(), surface);
      if (likely(it != m_surfaces2.end())) {
        m_surfaces2.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface3* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces3.begin(), m_surfaces3.end(), surface);
    if (likely(it != m_surfaces3.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface3* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces3.begin(), m_surfaces3.end(), surface);
      if (unlikely(it != m_surfaces3.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces3.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface3* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces3.begin(), m_surfaces3.end(), surface);
      if (likely(it != m_surfaces3.end())) {
        m_surfaces3.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface4* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
    if (likely(it != m_surfaces4.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
      if (unlikely(it != m_surfaces4.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces4.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces4.begin(), m_surfaces4.end(), surface);
      if (likely(it != m_surfaces4.end())) {
        m_surfaces4.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  bool DDrawCommonInterface::IsWrappedSurface(IDirectDrawSurface7* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
    if (likely(it != m_surfaces7.end()))
      return true;

    return false;
  }

  void DDrawCommonInterface::AddWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
      if (unlikely(it != m_surfaces7.end())) {
        Logger::warn("DDrawCommonInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces7.push_back(surface);
      }
    }
  }

  void DDrawCommonInterface::RemoveWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      auto it = std::find(m_surfaces7.begin(), m_surfaces7.end(), surface);
      if (likely(it != m_surfaces7.end())) {
        m_surfaces7.erase(it);
      } else {
        Logger::warn("DDrawCommonInterface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

  d3d9::IDirect3DDevice9* DDrawCommonInterface::GetD3D9Device() {
    if (m_device7 != nullptr) {
      return m_device7->GetD3D9();
    } else if (m_device6 != nullptr) {
      return m_device6->GetD3D9();
    } else if (m_device5 != nullptr) {
      return m_device5->GetD3D9();
    }

    return nullptr;
  }

  d3d9::D3DMULTISAMPLE_TYPE DDrawCommonInterface::GetMultiSampleType() {
    if (m_device7 != nullptr) {
      return m_device7->GetMultiSampleType();
    } else if (m_device6 != nullptr) {
      return m_device6->GetMultiSampleType();
    } else if (m_device5 != nullptr) {
      return m_device5->GetMultiSampleType();
    }

    return d3d9::D3DMULTISAMPLE_NONE;
  }

  d3d9::D3DPRESENT_PARAMETERS DDrawCommonInterface::GetPresentParameters() {
    if (m_device7 != nullptr) {
      return m_device7->GetPresentParameters();
    } else if (m_device6 != nullptr) {
      return m_device6->GetPresentParameters();
    }

    return d3d9::D3DPRESENT_PARAMETERS();
  }

  HRESULT DDrawCommonInterface::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    if (m_device7 != nullptr) {
      return m_device7->ResetD3D9Swapchain(params);
    } else if (m_device6 != nullptr) {
      return m_device6->ResetD3D9Swapchain(params);
    }

    return DDERR_GENERIC;
  }

  bool DDrawCommonInterface::IsCurrentRenderTarget(DDrawSurface* surface) const {
    return m_device5 != nullptr ? m_device5->GetRenderTarget() == surface : false;
  }

  bool DDrawCommonInterface::IsCurrentRenderTarget(DDraw4Surface* surface) const {
    return m_device6 != nullptr ? m_device6->GetRenderTarget() == surface : false;
  }

  bool DDrawCommonInterface::IsCurrentRenderTarget(DDraw7Surface* surface) const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget() == surface : false;
  }

  bool DDrawCommonInterface::IsCurrentDepthStencil(DDrawSurface* surface) const {
    return m_device5 != nullptr ? m_device5->GetDepthStencil() == surface : false;
  }

  bool DDrawCommonInterface::IsCurrentDepthStencil(DDraw4Surface* surface) const {
    return m_device6 != nullptr ? m_device6->GetDepthStencil() == surface : false;
  }

  bool DDrawCommonInterface::IsCurrentDepthStencil(DDraw7Surface* surface) const {
    return m_device7 != nullptr ? m_device7->GetDepthStencil() == surface : false;
  }

}
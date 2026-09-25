#include "d3d_common_device.h"

#include "ddraw_common_surface.h"

#include "d3d_common_interface.h"

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw7/ddraw7_surface.h"

#include "d3d7/d3d7_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

#include <algorithm>

namespace dxvk {

  D3DCommonDevice::D3DCommonDevice(
        DDrawCommonInterface* commonIntf,
        GUID deviceGUID,
        const d3d9::D3DPRESENT_PARAMETERS* pParams9,
        DWORD creationFlags9)
    : m_commonIntf     ( commonIntf )
    , m_deviceGUID     ( deviceGUID )
    , m_params9        ( *pParams9 )
    , m_creationFlags9 ( creationFlags9 ) {
  }

  D3DCommonDevice::~D3DCommonDevice() {
    // Dissasociate every bound viewport from this device
    for (auto& viewport : m_viewports) {
      viewport->SetCommonD3DDevice(nullptr);
    }

    if (m_commonIntf->GetCommonD3DDevice() == this)
      m_commonIntf->SetCommonD3DDevice(nullptr);
  }

  HRESULT STDMETHODCALLTYPE D3DCommonDevice::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = ref(this);
    return S_OK;
  }

  D3DCommonInterface* D3DCommonDevice::GetCommonD3DInterface() const {
    if (m_device7 != nullptr) {
      return m_device7->GetParent() != nullptr ? m_device7->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device6 != nullptr) {
      return m_device6->GetParent() != nullptr ? m_device6->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device5 != nullptr) {
      return m_device5->GetParent() != nullptr ? m_device5->GetParent()->GetCommonD3DInterface() : nullptr;
    } else if (m_device3 != nullptr) {
      D3D3Interface* d3d3Intf = m_device3->GetParent()->GetCommonInterface()->GetD3D3Interface();
      return d3d3Intf != nullptr ? d3d3Intf->GetCommonD3DInterface() : nullptr;
    }

    return nullptr;
  }

  // D3D5/3 has no way of disabling/re-enabling VSync, so skip the D3D5/3 devices
  HRESULT D3DCommonDevice::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    if (m_device7 != nullptr) {
      return m_device7->ResetD3D9Swapchain(params);
    } else if (m_device6 != nullptr) {
      return m_device6->ResetD3D9Swapchain(params);
    }

    return DDERR_UNSUPPORTED;
  }

  DDrawSurface* D3DCommonDevice::GetCurrentRenderTarget() const {
    return m_device5 != nullptr ? m_device5->GetRenderTarget() :
           m_device3 != nullptr ? m_device3->GetRenderTarget() : nullptr;
  }

  DDraw4Surface* D3DCommonDevice::GetCurrentRenderTarget4() const {
    return m_device6 != nullptr ? m_device6->GetRenderTarget() : nullptr;
  }

  DDraw7Surface* D3DCommonDevice::GetCurrentRenderTarget7() const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget() : nullptr;
  }

  // Only needed by D3D6 and earlier viewports, so skip the D3D7 device
  DDrawCommonSurface* D3DCommonDevice::GetCommonRenderTarget() const {
    if (m_device6 != nullptr) {
      DDraw4Surface* rt = m_device6->GetRenderTarget();
      return rt != nullptr ? rt->GetCommonSurface() : nullptr;
    }
    if (m_device5 != nullptr) {
      DDrawSurface* rt = m_device5->GetRenderTarget();
      return rt != nullptr ? rt->GetCommonSurface() : nullptr;
    }
    if (m_device3 != nullptr) {
      DDrawSurface* rt = m_device3->GetRenderTarget();
      return rt != nullptr ? rt->GetCommonSurface() : nullptr;
    }

    return nullptr;
  }

  // Only needed by D3D6 and earlier viewports, so skip the D3D7 device
  DDrawCommonSurface* D3DCommonDevice::GetCommonDepthStencil() const {
    if (m_device6 != nullptr) {
      DDraw4Surface* ds = m_device6->GetDepthStencil();
      return ds != nullptr ? ds->GetCommonSurface() : nullptr;
    }
    if (m_device5 != nullptr) {
      DDrawSurface* ds = m_device5->GetDepthStencil();
      return ds != nullptr ? ds->GetCommonSurface() : nullptr;
    }
    if (m_device3 != nullptr) {
      DDrawSurface* ds = m_device3->GetDepthStencil();
      return ds != nullptr ? ds->GetCommonSurface() : nullptr;
    }

    return nullptr;
  }

  bool D3DCommonDevice::IsCurrentRenderTarget(DDrawCommonSurface* commonSurface) const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device6 != nullptr ? m_device6->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device5 != nullptr ? m_device5->GetRenderTarget()->GetCommonSurface() == commonSurface :
           m_device3 != nullptr ? m_device3->GetRenderTarget()->GetCommonSurface() == commonSurface : false;
  }

  // Only needed by D3D6 and earlier viewport clears, so skip the D3D7 device
  void D3DCommonDevice::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if (m_device6 != nullptr) {
      m_device6->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device5 != nullptr) {
      m_device5->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device3 != nullptr) {
      m_device3->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    }
  }

  HRESULT D3DCommonDevice::GetClipStatusCommon(D3DCLIPSTATUS* clip_status) {
    d3d9::D3DVIEWPORT9 viewport9;
    HRESULT hr = m_device9->GetViewport(&viewport9);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    clip_status->dwFlags = D3DCLIPSTATUS_EXTENTS2;
    clip_status->dwStatus = 0u;
    clip_status->minx = viewport9.X;
    clip_status->maxx = viewport9.X + viewport9.Width;
    clip_status->miny = viewport9.Y;
    clip_status->maxy = viewport9.Y + viewport9.Height;
    clip_status->minz = 0.0f;
    clip_status->maxz = 0.0f;

    return D3D_OK;
  }

  HRESULT D3DCommonDevice::AddViewportCommon(D3DViewport *viewport) {
    auto it = std::find(m_viewports.begin(), m_viewports.end(), viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3DCommonDevice::AddViewportCommon: Pre-existing viewport found");
    } else {
      m_viewports.push_back(viewport);
      viewport->SetCommonD3DDevice(this);
    }

    return D3D_OK;
  }

  HRESULT D3DCommonDevice::DeleteViewportCommon(D3DViewport* viewport) {
    auto it = std::find(m_viewports.begin(), m_viewports.end(), viewport);
    if (likely(it != m_viewports.end())) {
      viewport->SetCommonD3DDevice(nullptr);
      // Clear the current viewport if it is deleted from the device
      if (m_currentViewport == viewport)
        m_currentViewport = nullptr;
      m_viewports.erase(it);
    } else {
      Logger::warn("D3DCommonDevice::DeleteViewportCommon: Viewport not found");
    }

    return D3D_OK;
  }

  HRESULT D3DCommonDevice::NextViewportCommon(D3DViewport* viewport, D3DViewport** nextViewport, DWORD flags) {
    if (flags & D3DNEXT_HEAD) {
      if (likely(m_viewports.size() > 0))
        *nextViewport = m_viewports.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(nextViewport == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(m_viewports.size() > 0))
        Logger::warn("D3DCommonDevice::NextViewportCommon: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_viewports.size() > 0))
        *nextViewport = m_viewports.back().ref();
    }

    return D3D_OK;
  }

}
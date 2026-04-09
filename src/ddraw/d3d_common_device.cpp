#include "d3d_common_device.h"

#include "d3d_common_interface.h"

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw7/ddraw7_surface.h"

#include "d3d7/d3d7_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonDevice::D3DCommonDevice(
        DDrawCommonInterface* commonIntf,
        DWORD creationFlags9,
        uint32_t totalMemory)
    : m_commonIntf     ( commonIntf )
    , m_totalMemory    ( totalMemory )
    , m_creationFlags9 ( creationFlags9 ) {
  }

  D3DCommonDevice::~D3DCommonDevice() {
    if (m_commonIntf->GetCommonD3DDevice() == this)
      m_commonIntf->SetCommonD3DDevice(nullptr);
  }

  d3d9::IDirect3DDevice9* D3DCommonDevice::GetD3D9Device() {
    if (m_device7 != nullptr) {
      return m_device7->GetD3D9();
    } else if (m_device6 != nullptr) {
      return m_device6->GetD3D9();
    } else if (m_device5 != nullptr) {
      return m_device5->GetD3D9();
    } else if (m_device3 != nullptr) {
      return m_device3->GetD3D9();
    }

    return nullptr;
  }

  d3d9::D3DMULTISAMPLE_TYPE D3DCommonDevice::GetMultiSampleType() {
    if (m_device7 != nullptr) {
      return m_device7->GetMultiSampleType();
    } else if (m_device6 != nullptr) {
      return m_device6->GetMultiSampleType();
    } else if (m_device5 != nullptr) {
      return m_device5->GetMultiSampleType();
    } else if (m_device3 != nullptr) {
      return m_device3->GetMultiSampleType();
    }

    return d3d9::D3DMULTISAMPLE_NONE;
  }

  d3d9::D3DPRESENT_PARAMETERS D3DCommonDevice::GetPresentParameters() {
    if (m_device7 != nullptr) {
      return m_device7->GetPresentParameters();
    } else if (m_device6 != nullptr) {
      return m_device6->GetPresentParameters();
    } else if (m_device5 != nullptr) {
      return m_device5->GetPresentParameters();
    } else if (m_device3 != nullptr) {
      return m_device3->GetPresentParameters();
    }

    return d3d9::D3DPRESENT_PARAMETERS();
  }

  HRESULT D3DCommonDevice::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    if (m_device7 != nullptr) {
      return m_device7->ResetD3D9Swapchain(params);
    } else if (m_device6 != nullptr) {
      return m_device6->ResetD3D9Swapchain(params);
    }
    // D3D5/3 has no way of disabling/re-enabling VSync

    return DDERR_GENERIC;
  }

  bool D3DCommonDevice::IsCurrentRenderTarget(DDrawSurface* surface) const {
    return m_device5 != nullptr ? m_device5->GetRenderTarget() == surface :
           m_device3 != nullptr ? m_device3->GetRenderTarget() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentRenderTarget(DDraw4Surface* surface) const {
    return m_device6 != nullptr ? m_device6->GetRenderTarget() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentRenderTarget(DDraw7Surface* surface) const {
    return m_device7 != nullptr ? m_device7->GetRenderTarget() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentD3D9RenderTarget(d3d9::IDirect3DSurface9* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    if (m_device7 != nullptr) {
      return surface == m_device7->GetRenderTarget()->GetD3D9();
    } else if (m_device6 != nullptr) {
      return surface == m_device6->GetRenderTarget()->GetD3D9();
    } else if (m_device5 != nullptr) {
      return surface == m_device5->GetRenderTarget()->GetD3D9();
    } else if (m_device3 != nullptr) {
      return surface == m_device3->GetRenderTarget()->GetD3D9();
    }

    return false;
  }

  bool D3DCommonDevice::IsCurrentDepthStencil(DDrawSurface* surface) const {
    return m_device5 != nullptr ? m_device5->GetDepthStencil() == surface :
           m_device3 != nullptr ? m_device3->GetDepthStencil() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentDepthStencil(DDraw4Surface* surface) const {
    return m_device6 != nullptr ? m_device6->GetDepthStencil() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentDepthStencil(DDraw7Surface* surface) const {
    return m_device7 != nullptr ? m_device7->GetDepthStencil() == surface : false;
  }

  bool D3DCommonDevice::IsCurrentD3D9DepthStencil(d3d9::IDirect3DSurface9* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    if (m_device7 != nullptr) {
      return surface == m_device7->GetDepthStencil()->GetD3D9();
    } else if (m_device6 != nullptr) {
      return surface == m_device6->GetDepthStencil()->GetD3D9();
    } else if (m_device5 != nullptr) {
      return surface == m_device5->GetDepthStencil()->GetD3D9();
    } else if (m_device3 != nullptr) {
      return surface == m_device3->GetDepthStencil()->GetD3D9();
    }

    return false;
  }

}
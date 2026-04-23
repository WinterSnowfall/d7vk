#include "ddraw_common_surface.h"

#include "ddraw7/ddraw7_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw2/ddraw3_surface.h"
#include "ddraw2/ddraw2_surface.h"
#include "ddraw/ddraw_surface.h"

namespace dxvk {

  DDrawCommonSurface::DDrawCommonSurface(DDrawCommonInterface* commonIntf)
    : m_commonIntf ( commonIntf ) {
  }

  DDrawCommonSurface::~DDrawCommonSurface() {
    if (unlikely(IsPrimarySurface() && m_commonIntf->GetPrimarySurface() == this))
      m_commonIntf->SetPrimarySurface(nullptr);
  }

  IUnknown* DDrawCommonSurface::GetShadowSurfaceProxied() {
    if (m_surf7 != nullptr) {
      DDraw7Surface* shadow7 = m_surf7->GetShadowSurface();
      if (shadow7 != nullptr)
        return shadow7->GetProxied();
    }
    if (m_surf4 != nullptr) {
      DDraw4Surface* shadow4 = m_surf4->GetShadowSurface();
      if (shadow4 != nullptr)
        return shadow4->GetProxied();
    }
    if (m_surf3 != nullptr) {
      DDraw3Surface* shadow3 = m_surf3->GetShadowSurface();
      if (shadow3 != nullptr)
        return shadow3->GetProxied();
    }
    if (m_surf2 != nullptr) {
      DDraw2Surface* shadow2 = m_surf2->GetShadowSurface();
      if (shadow2 != nullptr)
        return shadow2->GetProxied();
    }
    if (m_surf != nullptr) {
      DDrawSurface* shadow = m_surf->GetShadowSurface();
      if (shadow != nullptr)
        return shadow->GetProxied();
    }

    return nullptr;
  }

  DDrawCommonSurface* DDrawCommonSurface::GetShadowCommonSurface() {
    if (m_surf7 != nullptr) {
      DDraw7Surface* shadow7 = m_surf7->GetShadowSurface();
      if (shadow7 != nullptr)
        return shadow7->GetCommonSurface();
    }
    if (m_surf4 != nullptr) {
      DDraw4Surface* shadow4 = m_surf4->GetShadowSurface();
      if (shadow4 != nullptr)
        return shadow4->GetCommonSurface();
    }
    if (m_surf3 != nullptr) {
      DDraw3Surface* shadow3 = m_surf3->GetShadowSurface();
      if (shadow3 != nullptr)
        return shadow3->GetCommonSurface();
    }
    if (m_surf2 != nullptr) {
      DDraw2Surface* shadow2 = m_surf2->GetShadowSurface();
      if (shadow2 != nullptr)
        return shadow2->GetCommonSurface();
    }
    if (m_surf != nullptr) {
      DDrawSurface* shadow = m_surf->GetShadowSurface();
      if (shadow != nullptr)
        return shadow->GetCommonSurface();
    }

    return nullptr;
  }

  HRESULT DDrawCommonSurface::RefreshSurfaceDescripton() {
    HRESULT hr;

    DDSURFACEDESC2 desc2;
    desc2.dwSize = sizeof(DDSURFACEDESC2);

    if (m_surf7 != nullptr) {
      hr = m_surf7->GetProxied()->GetSurfaceDesc(&desc2);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc2 = desc2;
    } else if (m_surf4 != nullptr) {
      hr = m_surf4->GetProxied()->GetSurfaceDesc(&desc2);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc2 = desc2;
    }

    DDSURFACEDESC desc;
    desc.dwSize = sizeof(DDSURFACEDESC);

    if (m_surf != nullptr) {
      hr = m_surf->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
    } else if (m_surf2 != nullptr) {
      hr = m_surf2->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
    } else if (m_surf3 != nullptr) {
      hr = m_surf3->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
    }

    return DD_OK;
  }

}
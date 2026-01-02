#include "ddraw_surface.h"

#include "ddraw_interface.h"

#include "ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDrawSurface::s_surfCount = 0;

  DDrawSurface::DDrawSurface(
        Com<IDirectDrawSurface>&& surfProxy,
        DDrawInterface* pParent,
        DDrawSurface* pParentSurf,
        DDraw7Surface* origin,
        bool isChildObject)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawSurface, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_isChildObject ( isChildObject )
    , m_parentSurf ( pParentSurf )
    , m_origin ( origin ) {
    if (likely(!IsLegacyInterface())) {
      m_parent->AddWrappedSurface(this);

      // Retrieve and cache the proxy surface desc
      m_desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&m_desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDrawSurface: ERROR! Failed to retrieve new surface desc!");
      } else {
        // Determine the d3d9 surface format
        m_format = ConvertFormat(m_desc.ddpfPixelFormat);
      }
    }

    m_surfCount = ++s_surfCount;

    if (likely(!IsLegacyInterface())) {
      ListSurfaceDetails();
    } else {
      Logger::debug(str::format("DDrawSurface: Created a new surface nr. [[1-", m_surfCount, "]]"));
    }
  }

  DDrawSurface::~DDrawSurface() {
    if (likely(!IsLegacyInterface()))
      m_parent->RemoveWrappedSurface(this);

    Logger::debug(str::format("DDrawSurface: Surface nr. [[1-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDrawSurface, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDrawSurface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDrawSurface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawSurface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Apparently, the standard way of creating a HAL D3D3 device...
    if (riid == IID_IDirect3DHALDevice) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDrawSurface::QueryInterface: Query for IID_IDirect3DHALDevice");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::info("DDrawSurface::QueryInterface: Query for IID_IDirect3DHALDevice");
      Logger::warn("DDrawSurface::QueryInterface: Use of unsupported D3D3 device");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Apparently, the standard way of creating a RGB D3D3 device...
    if (riid == IID_IDirect3DRGBDevice) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDrawSurface::QueryInterface: Query for IID_IDirect3DRGBDevice");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::info("DDrawSurface::QueryInterface: Query for IID_IDirect3DRGBDevice");
      Logger::warn("DDrawSurface::QueryInterface: Use of unsupported D3D3 device");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (likely(IsLegacyInterface())) {
        return m_origin->QueryInterface(riid, ppvObject);
      }

      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some applications check the supported API level by querying the various newer surface GUIDs...
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface2");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface2");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface3");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface3");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface4");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface4");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface7");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture");
      return m_proxy->QueryInterface(riid, ppvObject);
    }

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawSurface::AddAttachedSurface: Proxy");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    Logger::debug("<<< DDrawSurface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDrawSurface::AddAttachedSurface: Attaching unwrapped surface");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(ddrawSurface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddrawSurface->SetParentSurface(this);
    m_depthStencil = ddrawSurface;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDrawSurface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawSurface::Blt: Proxy");
      return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    Logger::debug("<<< DDrawSurface::Blt: Proxy");

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDrawSurface::Blt: Received an unwrapped source surface");
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddrawSurface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::BltBatch: Proxy");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawSurface::BltFast: Proxy");
      return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    }

    Logger::debug("<<< DDrawSurface::BltFast: Proxy");

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDrawSurface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddrawSurface->GetProxied(), lpSrcRect, dwTrans);
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawSurface::DeleteAttachedSurface: Proxy");
      return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
    }

    Logger::debug("<<< DDrawSurface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr))
        Logger::warn("DDrawSurface::DeleteAttachedSurface: Deleting unwrapped surface");

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, ddrawSurface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(m_depthStencil == ddrawSurface)) {
      ddrawSurface->ClearParentSurface();
      m_depthStencil = nullptr;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDrawSurface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDrawSurface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::Flip: Proxy");
    return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE *lplpDDAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawSurface::GetAttachedSurface: Proxy");
      return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
    }

    Logger::debug("<<< DDrawSurface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP))
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for an offscreen plain surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for a depth stencil");
    else if (lpDDSCaps->dwCaps & DDSCAPS_MIPMAP)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for a texture");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for an overlay");

    Com<IDirectDrawSurface> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDrawSurface::GetAttachedSurface: Failed to find the requested surface");
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    if (likely(!m_parent->IsWrappedSurface(surface.ptr()))) {
      Logger::debug("DDrawSurface::GetAttachedSurface: Got a new unwrapped surface");
      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          Com<DDrawSurface> ddrawSurface = new DDrawSurface(std::move(surface), m_parent, this, nullptr, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddrawSurface->GetProxied()),
                                     std::forward_as_tuple(ddrawSurface.ptr()));
          // Do NOT ref here since we're managing the attached object lifecycle
          *lplpDDAttachedSurface = ddrawSurface.ptr();
        } else {
          *lplpDDAttachedSurface = attachedSurfaceIter->second.ptr();
        }
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        *lplpDDAttachedSurface = nullptr;
        return DDERR_GENERIC;
      }
    // Can potentially happen with manually attached surfaces
    } else {
      Logger::debug("DDrawSurface::GetAttachedSurface: Got an existing wrapped surface");
      Com<DDrawSurface> ddrawSurface = static_cast<DDrawSurface*>(surface.ptr());
      *lplpDDAttachedSurface = ddrawSurface.ref();
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug(">>> DDrawSurface::GetCaps: Forwarded");
    // TODO: Convert LPDDSCAPS <-> LPDDSCAPS2
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDrawSurface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    *lplpDDClipper = m_clipper.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDrawSurface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetDC(HDC *lphDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawSurface::GetDC: Forwarded");
      return m_origin->GetDC(lphDC);
    }

    Logger::debug("<<< DDrawSurface::GetDC: Proxy");
    return m_proxy->GetDC(lphDC);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDrawSurface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug(">>> DDrawSurface::GetPalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    *lplpDDPalette = m_palette.ref();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawSurface::GetPixelFormat: Forwarded");
      return m_origin->GetPixelFormat(lpDDPixelFormat);
    }

    Logger::debug("<<< DDrawSurface::GetPixelFormat: Proxy");
    return m_proxy->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDrawSurface::GetSurfaceDesc: Forwarded");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDrawSurface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::IsLost() {
    Logger::debug("<<< DDrawSurface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDrawSurface::Lock: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::ReleaseDC(HDC hDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawSurface::ReleaseDC: Forwarded");
      return m_origin->ReleaseDC(hDC);
    }

    Logger::debug("<<< DDrawSurface::ReleaseDC: Proxy");
    return m_proxy->ReleaseDC(hDC);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Restore() {
    Logger::debug("<<< DDrawSurface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    if (likely(IsLegacyInterface())) {
      Logger::debug("<<< DDrawSurface::SetClipper: Proxy");
      return m_proxy->SetClipper(lpDDClipper);
    }

    Logger::debug("<<< DDrawSurface::SetClipper: Proxy");

    // A nullptr lpDDClipper gets the current clipper detached
    if (lpDDClipper == nullptr) {
      HRESULT hr = m_proxy->SetClipper(lpDDClipper);
      if (unlikely(FAILED(hr)))
        return hr;

      m_clipper = nullptr;
    } else {
      DDrawClipper* ddrawClipper = static_cast<DDrawClipper*>(lpDDClipper);

      HRESULT hr = m_proxy->SetClipper(ddrawClipper->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_clipper = ddrawClipper;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDrawSurface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDrawSurface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    if (likely(IsLegacyInterface())) {
      Logger::debug("<<< DDrawSurface::SetPalette: Proxy");
      return m_proxy->SetPalette(lpDDPalette);
    }

    Logger::debug("<<< DDrawSurface::SetPalette: Proxy");

    // A nullptr lpDDPalette gets the current palette detached
    if (lpDDPalette == nullptr) {
      HRESULT hr = m_proxy->SetPalette(lpDDPalette);
      if (unlikely(FAILED(hr)))
        return hr;

      m_palette = nullptr;
    } else {
      DDrawPalette* ddrawPalette = static_cast<DDrawPalette*>(lpDDPalette);

      HRESULT hr = m_proxy->SetPalette(ddrawPalette->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_palette = ddrawPalette;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Unlock(LPVOID lpSurfaceData) {
    Logger::debug("<<< DDrawSurface::Unlock: Proxy");
    return m_proxy->Unlock(lpSurfaceData);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDrawSurface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference) {
    Logger::debug("<<< DDrawSurface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

}

#include "ddraw3_surface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d5/d3d5_device.h"

namespace dxvk {

  uint32_t DDraw3Surface::s_surfCount = 0;

  DDraw3Surface::DDraw3Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface3>&& surfProxy,
        DDrawSurface* pParent,
        DDraw7Surface* origin)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_commonSurf ( commonSurf )
    , m_origin ( origin ) {
    // m_commonSurf can never be null for IDirectDrawSurface3
    m_commonIntf = m_parent != nullptr ? m_parent->GetCommonInterface() : m_origin->GetCommonInterface();

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw3Surface: Created a new surface nr. [[3-", m_surfCount, "]]"));
  }

  DDraw3Surface::~DDraw3Surface() {
    Logger::debug(str::format("DDraw3Surface: Surface nr. [[3-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw3Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw3Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw3Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (likely(IsLegacyInterface())) {
        return m_origin->QueryInterface(riid, ppvObject);
      }

      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface2");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface4");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface4");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface7");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirect3DTexture");
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

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface) {
    Logger::warn("<<< DDraw3Surface::AddAttachedSurface: Proxy");
    return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw3Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw3Surface::Blt: Proxy");
      return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::Blt: Proxy");
      return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    Logger::debug("<<< DDraw3Surface::Blt: Proxy");

    D3D5Device* device5 = m_parent->GetD3D5Device();
    if (likely(device5 != nullptr)) {
      D3D5DeviceLock lock = device5->LockDevice();

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && device5->HasDrawn() &&
         (m_parent->IsPrimarySurface() || m_parent->IsFrontBuffer() || m_parent->IsBackBufferOrFlippable()) &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && device5->HasDrawn() && m_parent->IsPrimarySurface()) {
        device5->ResetDrawTracking();
        device5->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    // We are pretty much guaranteed to get wrapped surfaces with IDirectDrawSurface3
    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSrcSurface);
    HRESULT hr = m_proxy->Blt(lpDestRect, ddraw3Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_parent->IsTexture()) {
        HRESULT hrUpload = m_parent->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw3Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw3Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw3Surface::BltFast: Proxy");
      return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::BltFast: Proxy");
      return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    }

    Logger::debug("<<< DDraw3Surface::BltFast: Proxy");

    D3D5Device* device5 = m_parent->GetD3D5Device();
    if (likely(device5 != nullptr)) {
      D3D5DeviceLock lock = device5->LockDevice();

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && device5->HasDrawn() &&
         (m_parent->IsPrimarySurface() || m_parent->IsFrontBuffer() || m_parent->IsBackBufferOrFlippable()) &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && device5->HasDrawn() && m_parent->IsPrimarySurface()) {
        device5->ResetDrawTracking();
        device5->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    // We are pretty much guaranteed to get wrapped surfaces with IDirectDrawSurface3
    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSrcSurface);
    HRESULT hr = m_proxy->BltFast(dwX, dwY, ddraw3Surface->GetProxied(), lpSrcRect, dwTrans);

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_parent->IsTexture()) {
        HRESULT hrUpload = m_parent->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw3Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface) {
    Logger::warn("<<< DDraw3Surface::DeleteAttachedSurface: Proxy");
    return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw3Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDraw3Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Flip(LPDIRECTDRAWSURFACE3 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::warn("*** DDraw3Surface::Flip: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE3 *lplpDDAttachedSurface) {
    Logger::warn("<<< DDraw3Surface::GetAttachedSurface: Proxy");
    return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug("<<< DDraw3Surface::GetCaps: Proxy");
    // TODO: Convert LPDDSCAPS <-> LPDDSCAPS2
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDraw3Surface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw3Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetDC(HDC *lphDC) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::GetDC: Forwarded");
      return m_origin->GetDC(lphDC);
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::GetDC: Proxy");
      return m_proxy->GetDC(lphDC);
    }

    Logger::debug(">>> DDraw3Surface::GetDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!m_parent->IsInitialized())) {
      Logger::debug("DDraw3Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    D3D5Device* device5 = m_parent->GetD3D5Device();
    if (device5 != nullptr && !(device5->HasDrawn() &&
        (m_parent->IsPrimarySurface() || m_parent->IsFrontBuffer() ||
         m_parent->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw3Surface::GetDC: Not yet drawn flippable surface");
      return m_proxy->GetDC(lphDC);
    }

    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lphDC);

    HRESULT hr = m_parent->GetD3D9()->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw3Surface::GetDC: Failed to get D3D9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDraw3Surface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug(">>> DDraw3Surface::GetPalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::GetPixelFormat: Forwarded");
      return m_origin->GetPixelFormat(lpDDPixelFormat);
    }

    Logger::debug("<<< DDraw3Surface::GetPixelFormat: Proxy");
    return m_proxy->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw3Surface::GetSurfaceDesc: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw3Surface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::IsLost() {
    Logger::debug("<<< DDraw3Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw3Surface::Lock: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::ReleaseDC(HDC hDC) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::ReleaseDC: Forwarded");
      return m_origin->ReleaseDC(hDC);
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::ReleaseDC: Proxy");
      return m_proxy->ReleaseDC(hDC);
    }

    Logger::debug(">>> DDraw3Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (m_parent->IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!m_parent->IsInitialized())) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    D3D5Device* device5 = m_parent->GetD3D5Device();
    if (device5 != nullptr && !(device5->HasDrawn() &&
       (m_parent->IsPrimarySurface() || m_parent->IsFrontBuffer() ||
        m_parent->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet drawn flippable surface");
      if (m_parent->IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    HRESULT hr = m_parent->GetD3D9()->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw3Surface::ReleaseDC: Failed to release d3d9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Restore() {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw3Surface::Restore: Proxy");
      return m_proxy->Restore();
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::ReleaseDC: Proxy");
      return m_proxy->Restore();
    }

    Logger::debug("<<< DDraw3Surface::Restore: Proxy");

    HRESULT hr = m_proxy->Restore();
    if (unlikely(FAILED(hr)))
      return hr;

    if (!m_parent->IsTexture()) {
      m_parent->InitializeOrUploadD3D9();
    } else {
      m_commonSurf->DirtyMipMaps();
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw3Surface::SetClipper: Proxy");

    // A nullptr lpDDClipper gets the current clipper detached
    if (lpDDClipper == nullptr) {
      HRESULT hr = m_proxy->SetClipper(lpDDClipper);
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetClipper(nullptr);
    } else {
      DDrawClipper* ddrawClipper = static_cast<DDrawClipper*>(lpDDClipper);

      HRESULT hr = m_proxy->SetClipper(ddrawClipper->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetClipper(ddrawClipper);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw3Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw3Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw3Surface::SetPalette: Proxy");

    // A nullptr lpDDPalette gets the current palette detached
    if (lpDDPalette == nullptr) {
      HRESULT hr = m_proxy->SetPalette(lpDDPalette);
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(nullptr);
    } else {
      DDrawPalette* ddrawPalette = static_cast<DDrawPalette*>(lpDDPalette);

      HRESULT hr = m_proxy->SetPalette(ddrawPalette->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(ddrawPalette);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Unlock(LPVOID lpSurfaceData) {
    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::GetDC: Proxy");
      return m_proxy->Unlock(lpSurfaceData);
    }

    Logger::debug("<<< DDraw3Surface::Unlock: Proxy");

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_parent->IsTexture()) {
        HRESULT hrUpload = m_parent->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw3Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE3 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSReference) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw3Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    if (unlikely(m_parent == nullptr)) {
      Logger::warn("<<< DDraw3Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    Logger::debug(">>> DDraw3Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_parent);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::PageLock(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::PageLock: Proxy");
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::PageUnlock(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::PageUnlock: Proxy");
    return m_proxy->PageUnlock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetSurfaceDesc(LPDDSURFACEDESC lpDDSD, DWORD dwFlags) {
    Logger::warn("<<< DDraw3Surface::SetSurfaceDesc: Proxy");
    return m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
  }

}

#include "ddraw3_surface.h"

#include "../ddraw/ddraw_gamma.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d5/d3d5_device.h"

namespace dxvk {

  uint32_t DDraw3Surface::s_surfCount = 0;

  DDraw3Surface::DDraw3Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface3>&& surfProxy,
        DDrawSurface* pParent,
        IUnknown* origin)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_commonSurf ( commonSurf )
    , m_origin ( origin ) {
    // Retrieve and cache the proxy surface desc
    m_desc.dwSize = sizeof(DDSURFACEDESC);
    HRESULT hr = m_proxy->GetSurfaceDesc(&m_desc);

    if (unlikely(FAILED(hr))) {
      throw DxvkError("DDraw3Surface: ERROR! Failed to retrieve new surface desc!");
    }

    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw3Surface: ERROR! Failed to retrieve the common interface!");
    }

    m_commonSurf->SetDD3Surface(this);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw3Surface: Created a new surface nr. [[3-", m_surfCount, "]]"));
  }

  DDraw3Surface::~DDraw3Surface() {
    m_commonSurf->SetDD3Surface(nullptr);

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
      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(std::move(gammaControlProxied), m_commonSurf->GetDDSurface()));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface7");

      Com<IDirectDrawSurface7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD7Interface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for existing IDirect3DTexture/2");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw3Surface::QueryInterface: Query for IDirect3DTexture/2");
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
    Logger::debug("<<< DDraw3Surface::Blt: Proxy");

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d5Device->HasDrawn() &&
         (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()) &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d5Device->HasDrawn() && m_parent->IsPrimarySurface()) {
        m_d3d5Device->ResetDrawTracking();
        m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    // We are pretty much guaranteed to get wrapped surfaces with IDirectDrawSurface3
    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSrcSurface);
    HRESULT hr = m_proxy->Blt(lpDestRect, ddraw3Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
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
    Logger::debug("<<< DDraw3Surface::BltFast: Proxy");

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d5Device->HasDrawn() &&
         (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()) &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d5Device->HasDrawn() && m_parent->IsPrimarySurface()) {
        m_d3d5Device->ResetDrawTracking();
        m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
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
    Logger::debug(">>> DDraw3Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_desc.ddsCaps;

    return DD_OK;
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
    Logger::debug(">>> DDraw3Surface::GetDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw3Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D5Device();
    if (m_d3d5Device != nullptr && !(m_d3d5Device->HasDrawn() &&
        (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
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
    Logger::debug(">>> DDraw3Surface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_desc.ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw3Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = m_desc;

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw3Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw3Surface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::IsLost() {
    Logger::debug("<<< DDraw3Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw3Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw3Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D5Device();
    if (m_d3d5Device != nullptr && !(m_d3d5Device->HasDrawn() &&
       (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet drawn flippable surface");
      if (IsTexture())
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
    Logger::debug("<<< DDraw3Surface::Restore: Proxy");
    return m_proxy->Restore();
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
    Logger::debug("<<< DDraw3Surface::Unlock: Proxy");

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
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
    if (unlikely(m_commonIntf->GetDDInterface() == nullptr)) {
      Logger::warn("<<< DDraw2Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    Logger::debug(">>> DDraw3Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_commonIntf->GetDDInterface());

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
    Logger::debug("<<< DDraw3Surface::SetSurfaceDesc: Proxy");

    // Can be used only to set the surface data and pixel format
    // used by an explicit system-memory surface (will be validated)
    HRESULT hr = m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    // Update the new surface description
    m_desc = { };
    m_desc.dwSize = sizeof(DDSURFACEDESC);
    m_proxy->GetSurfaceDesc(&m_desc);

    // We may need to recreate the d3d9 object based on the new desc
    m_d3d9 = nullptr;

    if (!IsTexture()) {
      InitializeOrUploadD3D9();
    } else {
      m_commonSurf->DirtyMipMaps();
    }

    return hr;
  }

}

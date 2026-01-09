#include "ddraw2_surface.h"

#include "../ddraw/ddraw_gamma.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d5/d3d5_device.h"

namespace dxvk {

  uint32_t DDraw2Surface::s_surfCount = 0;

  DDraw2Surface::DDraw2Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface2>&& surfProxy,
        DDrawSurface* pParent,
        IUnknown* origin)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface2, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_commonSurf ( commonSurf )
    , m_origin ( origin ) {
    // Retrieve and cache the proxy surface desc
    m_desc.dwSize = sizeof(DDSURFACEDESC);
    HRESULT hr = m_proxy->GetSurfaceDesc(&m_desc);

    if (unlikely(FAILED(hr))) {
      throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve new surface desc!");
    }

    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve the common interface!");
    }

    m_commonSurf->SetDD2Surface(this);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw2Surface: Created a new surface nr. [[2-", m_surfCount, "]]"));
  }

  DDraw2Surface::~DDraw2Surface() {
    m_commonSurf->SetDD2Surface(nullptr);

    Logger::debug(str::format("DDraw2Surface: Surface nr. [[2-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirectDrawSurface2, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw2Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw2Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw2Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawGammaControl");
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
        Logger::warn("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::warn("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::warn("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::warn("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface7");

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
        Logger::debug("DDraw2Surface::QueryInterface: Query for existing IDirect3DTexture/2");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw2Surface::QueryInterface: Query for IDirect3DTexture/2");
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

  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    Logger::warn("<<< DDraw2Surface::AddAttachedSurface: Proxy");
    return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw2Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw2Surface::Blt: Proxy");

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

    // We are pretty much guaranteed to get wrapped surfaces with IDirectDrawSurface2
    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
    HRESULT hr = m_proxy->Blt(lpDestRect, ddraw2Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDrawSurface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw2Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw2Surface::BltFast: Proxy");

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

    // We are pretty much guaranteed to get wrapped surfaces with IDirectDrawSurface2
    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
    HRESULT hr = m_proxy->BltFast(dwX, dwY, ddraw2Surface->GetProxied(), lpSrcRect, dwTrans);

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw2Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    Logger::warn("<<< DDraw2Surface::DeleteAttachedSurface: Proxy");
    return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw2Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDraw2Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Flip(LPDIRECTDRAWSURFACE2 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    // Lost surfaces are not flippable
    HRESULT hr = m_proxy->IsLost();
    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw2Surface::Flip: Lost surface");
      return hr;
    }

    if (unlikely(!(IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw2Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw2Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw2Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw2Surface> surf2 = static_cast<DDraw2Surface*>(lpDDSurfaceTargetOverride);
    if (lpDDSurfaceTargetOverride != nullptr) {
      if (unlikely(!surf2->GetParent()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw2Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      Logger::debug("*** DDraw2Surface::Flip: Presenting");

      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      m_d3d5Device->ResetDrawTracking();

      if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!m_parent->IsInitialized()))
          m_parent->IntializeD3D9(m_d3d5Device->GetRenderTarget() == m_parent);

        BlitToDDrawSurface<IDirectDrawSurface2, DDSURFACEDESC>(m_proxy.ptr(), m_d3d5Device->GetRenderTarget()->GetD3D9());

        if (likely(m_d3d5Device->GetRenderTarget() == m_parent)) {
          if (lpDDSurfaceTargetOverride != nullptr) {
            m_d3d5Device->SetFlipRTFlags(surf2->GetParent()->GetProxied(), dwFlags);
          } else {
            m_d3d5Device->SetFlipRTFlags(nullptr, dwFlags);
          }
        }
        if (lpDDSurfaceTargetOverride != nullptr) {
          return m_proxy->Flip(surf2->GetProxied(), dwFlags);
        } else {
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        }
      }

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw2Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = m_d3d5Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = m_d3d5Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw2Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      }

      m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
      Logger::debug("*** DDraw2Surface::Flip: We have presented, allegedly");
    // If we don't have a valid D3D5 device, this means a D3D3 application
    // is trying to flip the surface. Allow that for compatibility reasons.
    } else {
      Logger::debug("<<< DDraw2Surface::Flip: Proxy");
      m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE2 *lplpDDAttachedSurface) {
    Logger::warn("<<< DDraw2Surface::GetAttachedSurface: Proxy");
    return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug(">>> DDraw2Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_desc.ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDraw2Surface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw2Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDC(HDC *lphDC) {
    Logger::debug(">>> DDraw2Surface::GetDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw2Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D5Device();
    if (m_d3d5Device != nullptr && !(m_d3d5Device->HasDrawn() &&
        (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw2Surface::GetDC: Not yet drawn flippable surface");
      return m_proxy->GetDC(lphDC);
    }

    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lphDC);

    HRESULT hr = m_parent->GetD3D9()->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw2Surface::GetDC: Failed to get D3D9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDraw2Surface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug(">>> DDraw2Surface::GetPalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug(">>> DDraw2Surface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_desc.ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw2Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = m_desc;

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw2Surface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::IsLost() {
    Logger::debug("<<< DDraw2Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw2Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw2Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw2Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D5Device();
    if (m_d3d5Device != nullptr && !(m_d3d5Device->HasDrawn() &&
       (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw2Surface::ReleaseDC: Not yet drawn flippable surface");
      if (IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    HRESULT hr = m_parent->GetD3D9()->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw2Surface::ReleaseDC: Failed to release d3d9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Restore() {
    Logger::debug("<<< DDraw2Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw2Surface::SetClipper: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw2Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw2Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw2Surface::SetPalette: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Unlock(LPVOID lpSurfaceData) {
    Logger::debug("<<< DDraw2Surface::Unlock: Proxy");

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw2Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE2 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSReference) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(m_commonIntf->GetDDInterface() == nullptr)) {
      Logger::warn("<<< DDraw2Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    Logger::debug(">>> DDraw2Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_commonIntf->GetDDInterface());

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageLock(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::PageLock: Proxy");
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageUnlock(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::PageUnlock: Proxy");
    return m_proxy->PageUnlock(dwFlags);
  }

}

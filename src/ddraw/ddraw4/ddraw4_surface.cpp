#include "ddraw4_surface.h"

#include "ddraw4_interface.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDraw4Surface::s_surfCount = 0;

  DDraw4Surface::DDraw4Surface(
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw4Surface* pParentSurf,
        DDraw7Surface* origin,
        bool isChildObject)
    : DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_isChildObject ( isChildObject )
    , m_parentSurf ( pParentSurf )
    , m_origin ( origin ) {
    if (likely(!IsLegacyInterface())) {
      m_parent->AddWrappedSurface(this);

      // Retrieve and cache the proxy surface desc
      m_desc.dwSize = sizeof(DDSURFACEDESC2);
      HRESULT hr = m_proxy->GetSurfaceDesc(&m_desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        // Determine the d3d9 surface format
        m_format = ConvertFormat(m_desc.ddpfPixelFormat);
      }
    }

    m_surfCount = ++s_surfCount;

    if (likely(!IsLegacyInterface())) {
      ListSurfaceDetails();
    } else {
      Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", m_surfCount, "]]"));
    }
  }

  DDraw4Surface::~DDraw4Surface() {
    if (likely(!IsLegacyInterface()))
      m_parent->RemoveWrappedSurface(this);

    Logger::debug(str::format("DDraw4Surface: Surface nr. [[4-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface4)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw4Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw4Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (unlikely(IsLegacyInterface())) {
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirect3DTexture");
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::AddAttachedSurface: Proxy");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    Logger::debug("<<< DDraw4Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw4Surface::AddAttachedSurface: Attaching unwrapped surface");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(ddraw4Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddraw4Surface->SetParentSurface(this);
    m_depthStencil = ddraw4Surface;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw4Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::Blt: Proxy");
      return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    Logger::debug("<<< DDraw4Surface::Blt: Proxy");

    const bool exclusiveMode = m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d6Device->HasDrawn() &&
         (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()) &&
          m_parent->GetOptions()->backBufferGuard != D3D7BackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d6Device->HasDrawn() && IsPrimarySurface()) {
        m_d3d6Device->ResetDrawTracking();
        m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        if (likely(!m_parent->GetOptions()->proxiedQueryInterface)) {
          Logger::warn("DDraw4Surface::Blt: Received an unwrapped source surface");
        } else {
          Logger::debug("DDraw4Surface::Blt: Received an unwrapped source surface");
        }
      }
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw4Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
    }

    return hr;
  }

  // The docs state: "The IDirectDrawSurface4::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::BltFast: Proxy");
      return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    }

    Logger::debug("<<< DDraw4Surface::BltFast: Proxy");

    const bool exclusiveMode = m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d6Device->HasDrawn() && (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable())) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d6Device->HasDrawn() && IsPrimarySurface()) {
        m_d3d6Device->ResetDrawTracking();
        m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw4Surface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw4Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw4Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::DeleteAttachedSurface: Proxy");
      return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
    }

    Logger::debug("<<< DDraw4Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr))
        Logger::warn("DDraw4Surface::DeleteAttachedSurface: Deleting unwrapped surface");

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, ddraw4Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(m_depthStencil == ddraw4Surface)) {
      ddraw4Surface->ClearParentSurface();
      m_depthStencil = nullptr;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw4Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback) {
    Logger::debug("<<< DDraw4Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("*** DDraw4Surface::Flip: Ignoring");
      return DD_OK;
    }

    Logger::debug("*** DDraw4Surface::Flip: Presenting");

    // Lost surfaces are not flippable
    HRESULT hr = m_proxy->IsLost();
    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw4Surface::Flip: Lost surface");
      return hr;
    }

    if (unlikely(!(IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw4Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw4Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw4Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw4Surface> surf4;
    if (m_parent->IsWrappedSurface(lpDDSurfaceTargetOverride)) {
      surf4 = static_cast<DDraw4Surface*>(lpDDSurfaceTargetOverride);

      if (unlikely(!surf4->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw4Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();

      m_d3d6Device->ResetDrawTracking();

      if (unlikely(m_parent->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!IsInitialized()))
          IntializeD3D9(m_d3d6Device->GetRenderTarget() == this);

        BlitToDDrawSurface(m_proxy.ptr(), m_d3d6Device->GetRenderTarget()->GetD3D9());

        if (unlikely(!m_parent->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
          if (unlikely(lpDDSurfaceTargetOverride != nullptr))
            Logger::debug("DDraw7Surface::Flip: Received unwrapped surface");
          if (likely(m_d3d6Device->GetRenderTarget() == this))
            m_d3d6Device->SetFlipRTFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        } else {
          if (likely(m_d3d6Device->GetRenderTarget() == this))
            m_d3d6Device->SetFlipRTFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(surf4->GetProxied(), dwFlags);
        }
      }

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_parent->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = m_d3d6Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = m_d3d6Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_parent->SetWaitForVBlank(false);
        }
      }

      m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::GetAttachedSurface: Proxy");
      return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
    }

    Logger::debug("<<< DDraw4Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP))
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for an offscreen plain surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for a depth stencil");
    else if ((lpDDSCaps->dwCaps  & DDSCAPS_MIPMAP)
          || (lpDDSCaps->dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL))
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for a texture");
    else if (lpDDSCaps->dwCaps2 & DDSCAPS_OVERLAY)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for an overlay");

    Com<IDirectDrawSurface4> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDraw4Surface::GetAttachedSurface: Failed to find the requested surface");
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    if (likely(!m_parent->IsWrappedSurface(surface.ptr()))) {
      Logger::debug("DDraw4Surface::GetAttachedSurface: Got a new unwrapped surface");
      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(std::move(surface), m_parent, this, nullptr, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw4Surface.ptr()));
          // Do NOT ref here since we're managing the attached object lifecycle
          *lplpDDAttachedSurface = ddraw4Surface.ptr();
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
      Logger::debug("DDraw4Surface::GetAttachedSurface: Got an existing wrapped surface");
      Com<DDraw4Surface> ddraw4Surface = static_cast<DDraw4Surface*>(surface.ptr());
      *lplpDDAttachedSurface = ddraw4Surface.ref();
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetCaps: Forwarded");
      return m_origin->GetCaps(lpDDSCaps);
    }

    Logger::debug(">>> DDraw4Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_desc.ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug("<<< DDraw4Surface::GetClipper: Proxy");
    return m_proxy->GetClipper(lplpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw4Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDC(HDC *lphDC) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetDC: Forwarded");
      return m_origin->GetDC(lphDC);
    }

    if (unlikely(m_parent->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw4Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D6Device();
    if (m_d3d6Device != nullptr && !(m_d3d6Device->HasDrawn() &&
        (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw4Surface::GetDC: Not yet drawn flippable surface");
      return m_proxy->GetDC(lphDC);
    }

    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lphDC);

    HRESULT hr = m_d3d9->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw4Surface::GetDC: Failed to get D3D9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDraw4Surface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug("<<< DDraw4Surface::GetPalette: Proxy");
    return m_proxy->GetPalette(lplpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetPixelFormat: Forwarded");
      return m_origin->GetPixelFormat(lpDDPixelFormat);
    }

    Logger::debug(">>> DDraw4Surface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_desc.ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetSurfaceDesc: Forwarded");
      return m_origin->GetSurfaceDesc(lpDDSurfaceDesc);
    }

    Logger::debug(">>> DDraw4Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC2)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = m_desc;

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw4Surface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::IsLost() {
    Logger::debug("<<< DDraw4Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::Lock: Forwarded");
      return m_origin->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    }
    // TODO: Directly lock d3d9 surfaces and skip surface uploads on unlock,
    // but it can get very involved, especially when dealing with DXT formats.
    Logger::debug("<<< DDraw4Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ReleaseDC(HDC hDC) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::ReleaseDC: Forwarded");
      return m_origin->ReleaseDC(hDC);
    }

    Logger::debug(">>> DDraw7Surface::ReleaseDC");

    if (unlikely(m_parent->GetOptions()->forceProxiedPresent)) {
      if (IsTexture())
        m_dirtyMipMaps = true;
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D6Device();
    if (m_d3d6Device != nullptr && !(m_d3d6Device->HasDrawn() &&
       (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw7Surface::ReleaseDC: Not yet drawn flippable surface");
      if (IsTexture())
        m_dirtyMipMaps = true;
      return m_proxy->ReleaseDC(hDC);
    }

    HRESULT hr = m_d3d9->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::ReleaseDC: Failed to release d3d9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Restore() {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Surface::Restore: Proxy");
      return m_proxy->Restore();
    }

    Logger::debug("<<< DDraw4Surface::Restore: Proxy");

    HRESULT hr = m_proxy->Restore();
    if (unlikely(FAILED(hr)))
      return hr;

    if (IsTexture())
      DirtyMipMaps();

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Surface::SetClipper: Proxy");
      return m_proxy->SetClipper(lpDDClipper);
    }

    Logger::debug("<<< DDraw4Surface::SetClipper: Proxy");

    HRESULT hr = m_proxy->SetClipper(lpDDClipper);

    if (likely(SUCCEEDED(hr) && lpDDClipper != nullptr)) {
      // A few games apparently call SetCooperativeLevel AFTER creating the
      // d3d device, let alone the interface, which is technically illegal,
      // but hey, like that ever stopped anyone...
      if (unlikely(IsRenderTarget() && m_parent->GetHWND() == nullptr)) {
        Logger::debug("DDraw4Surface::SetClipper: Using hwnd from clipper");
        HWND hwnd;
        lpDDClipper->GetHWnd(&hwnd);
        m_parent->SetHWND(hwnd);
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw4Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw4Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw4Surface::SetPalette: Proxy");
    return m_proxy->SetPalette(lpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Unlock(LPRECT lpSurfaceData) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::Unlock: Forwarded");
      return m_origin->Unlock(lpSurfaceData);
    }

    Logger::debug("<<< DDraw4Surface::Unlock: Proxy");

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Surface::UpdateOverlay: Proxy");
      return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
    }

    Logger::debug("<<< DDraw4Surface::UpdateOverlay: Proxy");

    if (unlikely(!m_parent->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw4Surface::UpdateOverlay: Called with an unwrapped surface");
      return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw4Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Surface::UpdateOverlayZOrder: Proxy");
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }

    Logger::debug("<<< DDraw4Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw4Surface::UpdateOverlayZOrder: Called with an unwrapped surface");
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw4Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    Logger::debug(">>> DDraw4Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_parent);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::PageLock(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::PageLock: Proxy");
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::PageUnlock(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::PageUnlock: Proxy");
    return m_proxy->PageUnlock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Surface::SetSurfaceDesc: Proxy");
      return m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    }

    Logger::debug("<<< DDraw4Surface::SetSurfaceDesc: Proxy");

    // Can be used only to set the surface data and pixel format
    // used by an explicit system-memory surface (will be validated)
    HRESULT hr = m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    // Update the new surface description
    m_desc = { };
    m_desc.dwSize = sizeof(DDSURFACEDESC2);
    m_proxy->GetSurfaceDesc(&m_desc);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::SetPrivateData: Proxy");
    return m_proxy->SetPrivateData(tag, pData, cbSize, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    Logger::debug("<<< DDraw4Surface::GetPrivateData: Proxy");
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::FreePrivateData(REFGUID tag) {
    Logger::debug("<<< DDraw4Surface::FreePrivateData: Proxy");
    return m_proxy->FreePrivateData(tag);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetUniquenessValue(LPDWORD pValue) {
    Logger::debug("<<< DDraw4Surface::GetUniquenessValue: Proxy");
    return m_proxy->GetUniquenessValue(pValue);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ChangeUniquenessValue() {
    Logger::debug("<<< Called DDraw4Surface::ChangeUniquenessValue: Proxy");
    return m_proxy->ChangeUniquenessValue();
  }

  HRESULT DDraw4Surface::InitializeD3D9RenderTarget() {
    HRESULT hr = DD_OK;

    RefreshD3D6Device();

    if (unlikely(!IsInitialized()))
      hr = IntializeD3D9(true);

    return hr;
  }

  HRESULT DDraw4Surface::InitializeOrUploadD3D9() {
    HRESULT hr = DDERR_GENERIC;

    RefreshD3D6Device();

    if (likely(IsInitialized())) {
      hr = UploadSurfaceData();
    } else {
      hr = IntializeD3D9(false);
    }

    return hr;
  }

  inline HRESULT DDraw4Surface::IntializeD3D9(const bool initRT) {
    Logger::debug(str::format("DDraw4Surface::IntializeD3D9: Initializing nr. [[4-", m_surfCount, "]]"));

    if (unlikely(m_d3d6Device == nullptr)) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Null device, can't initalize right now");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw4Surface::IntializeD3D9: Surface has 0 height or width");
      return DD_OK;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(m_format == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDraw4Surface::IntializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;

    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(m_format == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDraw4Surface::IntializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    HRESULT hr;

    // In some cases we get passed offscreen plain surfaces with no data whatsoever in
    // ddpfPixelFormat, so we need to fall back to whatever the D3D9 back buffer is using.
    if (unlikely(m_format == d3d9::D3DFMT_UNKNOWN)) {
      Com<d3d9::IDirect3DSurface9> backBuffer;
      hr = m_d3d6Device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &backBuffer);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to get D3D9 back buffer");
        return hr;
      }

      d3d9::D3DSURFACE_DESC bbDesc;
      hr = backBuffer->GetDesc(&bbDesc);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to determine format for offscreen plain surface");
        return hr;
      }

      m_format = bbDesc.Format;
      Logger::debug(str::format("DDraw4Surface::IntializeD3D9: Offscreen plain surface format set to ", m_format));
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.

    IDirectDrawSurface4* mipMap = m_proxy.ptr();

    m_mipCount = 1;
    while (mipMap != nullptr) {
      IDirectDrawSurface4* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces4Callback);
      if (mipMap != nullptr) {
        m_mipCount++;
      }
    }

    // Do not worry about maximum supported mip map levels validation,
    // because D3D9 will hanlde this for us and cap them appropriately
    if (m_mipCount > 1) {
      Logger::debug(str::format("DDraw4Surface::IntializeD3D9: Found ", m_mipCount, " mip levels"));

      if (unlikely(m_mipCount != m_desc.dwMipMapCount))
        Logger::debug(str::format("DDraw4Surface::IntializeD3D9: Mismatch with declared ", m_desc.dwMipMapCount, " mip levels"));
    }

    if (unlikely(m_parent->GetOptions()->autoGenMipMaps)) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Using auto mip map generation");
      m_mipCount = 0;
    }

    d3d9::D3DPOOL pool  = d3d9::D3DPOOL_DEFAULT;
    DWORD         usage = 0;

    // General surface/texture pool placement
    if (m_desc.ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)
      pool = d3d9::D3DPOOL_DEFAULT;
    // There's no explicit non-local video memory placement
    // per se, but D3DPOOL_MANAGED is close enough
    else if ((m_desc.ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM) || (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE))
      pool = d3d9::D3DPOOL_MANAGED;
    else if (m_desc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
      // We can't know beforehand if a texture is or isn't going to be
      // used in SetTexture() calls, and textures placed in D3DPOOL_SYSTEMMEM
      // will not work in that context in dxvk, so revert to D3DPOOL_MANAGED.
      pool = IsTexture() ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_SYSTEMMEM;

    // Place all possible render targets in DEFAULT
    //
    // Note: This is somewhat problematic for textures and cube maps
    // which will have D3DUSAGE_RENDERTARGET, but also need to have
    // D3DUSAGE_DYNAMIC for locking/uploads to work. The flag combination
    // isn't supported in D3D9, but we have a D3D7 exception in place.
    //
    if (IsRenderTarget() || initRT) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (IsDepthStencil()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (IsTexture()) {
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        Logger::debug("DDraw7Surface::IntializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_parent->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDraw7Surface::IntializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDraw4Surface::IntializeD3D9: Placing in: ", poolPlacement));

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_d3d6Device->GetMultiSampleType();

    Com<d3d9::IDirect3DSurface9> surf;

    // Front Buffer
    if (IsFrontBuffer()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing front buffer...");

      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to retrieve front buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Back Buffer
    } else if (IsBackBufferOrFlippable()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing back buffer...");

      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to retrieve back buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Textures
    } else if (IsTexture()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing texture...");

      Com<d3d9::IDirect3DTexture9> tex;

      hr = m_d3d6Device->GetD3D9()->CreateTexture(
        m_desc.dwWidth, m_desc.dwHeight, m_mipCount, usage,
        m_format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      if (unlikely(m_parent->GetOptions()->autoGenMipMaps))
        tex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Attach level 0 to this surface
      tex->GetSurfaceLevel(0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDraw4Surface::IntializeD3D9: Created texture");
      m_texture = std::move(tex);

    // Depth Stencil
    } else if (IsDepthStencil()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing depth stencil...");

      hr = m_d3d6Device->GetD3D9()->CreateDepthStencilSurface(
        m_desc.dwWidth, m_desc.dwHeight, m_format,
        multiSampleType, 0, FALSE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw4Surface::IntializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(surf);

    // Offscreen Plain Surfaces
    } else if (IsOffScreenPlainSurface()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing offscreen plain surface...");

      // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer,
      // either as existing RTs or during SetRenderTarget() calls, which are tracked with initRT
      if (unlikely(m_d3d6Device->GetRenderTarget() == this || initRT)) {
        Logger::debug("DDraw4Surface::IntializeD3D9: Offscreen plain surface is the RT");

        surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

        if (unlikely(surf == nullptr)) {
          Logger::err("DDraw4Surface::IntializeD3D9: Failed to retrieve offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      } else {
        hr = m_d3d6Device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          pool, &surf, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDraw4Surface::IntializeD3D9: Failed to create offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      }

      m_d3d9 = std::move(surf);

    // Overlays (haven't seen any actual use of overlays in the wild)
    } else if (IsOverlay()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing overlay...");

      // Always link overlays to the current render target
      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to retrieve overlay surface");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Generic render target
    } else if (IsRenderTarget()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing render target...");

      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = m_d3d6Device->GetD3D9()->CreateRenderTarget(
        m_desc.dwWidth, m_desc.dwHeight, m_format,
        multiSampleType, usage, TRUE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to create render target");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else if (!IsNotKnown()) {
      Logger::debug("DDraw4Surface::IntializeD3D9: Initializing generic surface...");

      hr = m_d3d6Device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          pool, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::IntializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw4Surface::IntializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    } else {
      Logger::warn("DDraw4Surface::IntializeD3D9: Skipping initialization of unknown surface");
    }

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDraw4Surface::UploadSurfaceData() {
    Logger::debug(str::format("DDraw4Surface::UploadSurfaceData: Uploading nr. [[4-", m_surfCount, "]]"));

    if (m_d3d6Device->HasDrawn() && (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable())) {
      Logger::debug("DDraw4Surface::UploadSurfaceData: Skipping upload");
      return DD_OK;
    }

    // Nothing to upload
    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw4Surface::UploadSurfaceData: No wrapped surface or texture");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw4Surface::UploadSurfaceData: Surface has 0 height or width");
      return DD_OK;
    }

    if (IsTexture()) {
      BlitToD3D9Texture(m_texture.ptr(), m_format, m_proxy.ptr(), m_mipCount);
    // Depth stencil do not need uploads (nor are they possible in D3D9)
    } else if (unlikely(IsDepthStencil())) {
      Logger::debug("DDraw4Surface::UploadSurfaceData: Skipping upload of depth stencil");
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface(m_d3d9.ptr(), m_format, m_proxy.ptr());
    }

    return DD_OK;
  }

}

#include "ddraw_surface.h"

#include "ddraw_interface.h"

#include "ddraw2/ddraw2_surface.h"
#include "ddraw2/ddraw3_surface.h"
#include "ddraw4/ddraw4_surface.h"
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
        Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(std::move(ppvProxyObject), this, nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface3");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(std::move(ppvProxyObject), this, nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface4");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(std::move(ppvProxyObject), m_parent->GetDDraw4Interface(), nullptr, nullptr, false));

      return S_OK;
    }
    // TODO: Check if this is even possible
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawSurface::QueryInterface: Query for IDirectDrawSurface7");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_texture5 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_parent->GetNextTextureHandle();
        m_texture5 = new D3D5Texture(std::move(ppvProxyObject), this, nextHandle);
        m_parent->EmplaceTexture(m_texture5.ptr(), nextHandle);

        m_dirtyMipMaps = true;
      }

      *ppvObject = m_texture5.ref();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
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

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      const bool exclusiveMode = (m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_parent->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d5Device->HasDrawn() &&
         (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()) &&
          m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d5Device->HasDrawn() && IsPrimarySurface()) {
        m_d3d5Device->ResetDrawTracking();
        m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        if (likely(!m_parent->GetOptions()->proxiedQueryInterface)) {
          Logger::warn("DDrawSurface::Blt: Received an unwrapped source surface");
        } else {
          Logger::debug("DDrawSurface::Blt: Received an unwrapped source surface");
        }
      }
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddrawSurface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDrawSurface::Blt: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
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

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      const bool exclusiveMode = (m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_parent->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d5Device->HasDrawn() &&
         (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable()) &&
          m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d5Device->HasDrawn() && IsPrimarySurface()) {
        m_d3d5Device->ResetDrawTracking();
        m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDrawSurface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddrawSurface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDrawSurface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
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
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("*** DDrawSurface::Flip: Ignoring");
      return DD_OK;
    }

    // Lost surfaces are not flippable
    HRESULT hr = m_proxy->IsLost();
    if (unlikely(FAILED(hr))) {
      Logger::debug("DDrawSurface::Flip: Lost surface");
      return hr;
    }

    if (unlikely(!(IsFrontBuffer() || IsBackBufferOrFlippable()))) {
      Logger::debug("DDrawSurface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDrawSurface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDrawSurface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDrawSurface> surf;
    if (m_parent->IsWrappedSurface(lpDDSurfaceTargetOverride)) {
      surf = static_cast<DDrawSurface*>(lpDDSurfaceTargetOverride);

      if (unlikely(!surf->IsBackBufferOrFlippable())) {
        Logger::debug("DDrawSurface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D5Device();
    if (likely(m_d3d5Device != nullptr)) {
      Logger::debug("*** DDrawSurface::Flip: Presenting");

      D3D5DeviceLock lock = m_d3d5Device->LockDevice();

      m_d3d5Device->ResetDrawTracking();

      if (unlikely(m_parent->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!IsInitialized()))
          IntializeD3D9(m_d3d5Device->GetRenderTarget() == this);

        BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_proxy.ptr(), m_d3d5Device->GetRenderTarget()->GetD3D9());

        if (unlikely(!m_parent->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
          if (unlikely(lpDDSurfaceTargetOverride != nullptr))
            Logger::debug("DDrawSurface::Flip: Received unwrapped surface");
          if (likely(m_d3d5Device->GetRenderTarget() == this))
            m_d3d5Device->SetFlipRTFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        } else {
          if (likely(m_d3d5Device->GetRenderTarget() == this))
            m_d3d5Device->SetFlipRTFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(surf->GetProxied(), dwFlags);
        }
      }

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_parent->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDrawSurface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = m_d3d5Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = m_d3d5Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDrawSurface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_parent->SetWaitForVBlank(false);
        }
      }

      m_d3d5Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
      Logger::debug("*** DDrawSurface::Flip: We have presented, allegedly");
    // If we don't have a valid D3D5 device, this means a D3D3 application
    // is trying to flip the surfaces, so allow that for compatibility reasons
    } else {
      Logger::warn("<<< DDrawSurface::Flip: Proxy");
      m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
    }

    return DD_OK;
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
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for an overlay");

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
    Logger::debug("<<< DDrawSurface::GetSurfaceDesc: Proxy");
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

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDrawSurface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_dirtyMipMaps = true;
      }
    }

    return hr;
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

  HRESULT DDrawSurface::InitializeD3D9RenderTarget() {
    HRESULT hr = DD_OK;

    RefreshD3D5Device();

    if (unlikely(!IsInitialized()))
      hr = IntializeD3D9(true);

    return hr;
  }

  HRESULT DDrawSurface::InitializeOrUploadD3D9() {
    HRESULT hr = DD_OK;

    RefreshD3D5Device();

    if (likely(IsInitialized())) {
      hr = UploadSurfaceData();
    } else {
      hr = IntializeD3D9(false);
    }

    return hr;
  }

  inline HRESULT DDrawSurface::IntializeD3D9(const bool initRT) {
    Logger::debug(str::format("DDrawSurface::IntializeD3D9: Initializing nr. [[1-", m_surfCount, "]]"));

    if (unlikely(m_d3d5Device == nullptr)) {
      Logger::debug("DDrawSurface::IntializeD3D9: Null device, can't initalize right now");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDrawSurface::IntializeD3D9: Surface has 0 height or width");
      return DD_OK;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(m_format == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDrawSurface::IntializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;

    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(m_format == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDrawSurface::IntializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    HRESULT hr;

    // In some cases we get passed offscreen plain surfaces with no data whatsoever in
    // ddpfPixelFormat, so we need to fall back to whatever the D3D9 back buffer is using.
    if (unlikely(m_format == d3d9::D3DFMT_UNKNOWN)) {
      Com<d3d9::IDirect3DSurface9> backBuffer;
      hr = m_d3d5Device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &backBuffer);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to get D3D9 back buffer");
        return hr;
      }

      d3d9::D3DSURFACE_DESC bbDesc;
      hr = backBuffer->GetDesc(&bbDesc);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to determine format for offscreen plain surface");
        return hr;
      }

      m_format = bbDesc.Format;
      Logger::debug(str::format("DDrawSurface::IntializeD3D9: Offscreen plain surface format set to ", m_format));
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.

    IDirectDrawSurface* mipMap = m_proxy.ptr();

    m_mipCount = 1;
    while (mipMap != nullptr) {
      IDirectDrawSurface* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
      if (mipMap != nullptr) {
        m_mipCount++;
      }
    }

    // Do not worry about maximum supported mip map levels validation,
    // because D3D9 will hanlde this for us and cap them appropriately
    if (m_mipCount > 1) {
      Logger::debug(str::format("DDrawSurface::IntializeD3D9: Found ", m_mipCount, " mip levels"));

      if (unlikely(m_mipCount != m_desc.dwMipMapCount))
        Logger::debug(str::format("DDrawSurface::IntializeD3D9: Mismatch with declared ", m_desc.dwMipMapCount, " mip levels"));
    }

    if (unlikely(m_parent->GetOptions()->autoGenMipMaps)) {
      Logger::debug("DDrawSurface::IntializeD3D9: Using auto mip map generation");
      m_mipCount = 0;
    }

    d3d9::D3DPOOL pool  = d3d9::D3DPOOL_DEFAULT;
    DWORD         usage = 0;

    // General surface/texture pool placement
    if (m_desc.ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)
      pool = d3d9::D3DPOOL_DEFAULT;
    // There's no explicit non-local video memory placement
    // per se, but D3DPOOL_MANAGED is close enough
    else if (m_desc.ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM)
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
      Logger::debug("DDrawSurface::IntializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (IsDepthStencil()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (IsTexture()) {
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        Logger::debug("DDrawSurface::IntializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_parent->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDrawSurface::IntializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDrawSurface::IntializeD3D9: Placing in: ", poolPlacement));

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_d3d5Device->GetMultiSampleType();

    Com<d3d9::IDirect3DSurface9> surf;

    // Front Buffer
    if (IsFrontBuffer()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing front buffer...");

      surf = m_d3d5Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to retrieve front buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Back Buffer
    } else if (IsBackBufferOrFlippable()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing back buffer...");

      surf = m_d3d5Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to retrieve back buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Textures
    } else if (IsTexture()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing texture...");

      Com<d3d9::IDirect3DTexture9> tex;

      hr = m_d3d5Device->GetD3D9()->CreateTexture(
        m_desc.dwWidth, m_desc.dwHeight, m_mipCount, usage,
        m_format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      if (unlikely(m_parent->GetOptions()->autoGenMipMaps))
        tex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Attach level 0 to this surface
      tex->GetSurfaceLevel(0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDrawSurface::IntializeD3D9: Created texture");
      m_texture = std::move(tex);

    // Depth Stencil
    } else if (IsDepthStencil()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing depth stencil...");

      hr = m_d3d5Device->GetD3D9()->CreateDepthStencilSurface(
        m_desc.dwWidth, m_desc.dwHeight, m_format,
        multiSampleType, 0, FALSE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDrawSurface::IntializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(surf);

    // Offscreen Plain Surfaces
    } else if (IsOffScreenPlainSurface()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing offscreen plain surface...");

      // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer,
      // either as existing RTs or during SetRenderTarget() calls, which are tracked with initRT
      if (unlikely(m_d3d5Device->GetRenderTarget() == this || initRT)) {
        Logger::debug("DDrawSurface::IntializeD3D9: Offscreen plain surface is the RT");

        surf = m_d3d5Device->GetD3D9BackBuffer(m_proxy.ptr());

        if (unlikely(surf == nullptr)) {
          Logger::err("DDrawSurface::IntializeD3D9: Failed to retrieve offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      } else {
        hr = m_d3d5Device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          pool, &surf, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDrawSurface::IntializeD3D9: Failed to create offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      }

      m_d3d9 = std::move(surf);

    // Overlays (haven't seen any actual use of overlays in the wild)
    } else if (IsOverlay()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing overlay...");

      // Always link overlays to the current render target
      surf = m_d3d5Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to retrieve overlay surface");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Generic render target
    } else if (IsRenderTarget()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing render target...");

      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = m_d3d5Device->GetD3D9()->CreateRenderTarget(
        m_desc.dwWidth, m_desc.dwHeight, m_format,
        multiSampleType, usage, TRUE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to create render target");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else if (!IsNotKnown()) {
      Logger::debug("DDrawSurface::IntializeD3D9: Initializing generic surface...");

      hr = m_d3d5Device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          pool, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::IntializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDrawSurface::IntializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    } else {
      Logger::warn("DDrawSurface::IntializeD3D9: Skipping initialization of unknown surface");
    }

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDrawSurface::UploadSurfaceData() {
    Logger::debug(str::format("DDrawSurface::UploadSurfaceData: Uploading nr. [[1-", m_surfCount, "]]"));

    if (m_d3d5Device->HasDrawn() && (IsPrimarySurface() || IsFrontBuffer() || IsBackBufferOrFlippable())) {
      Logger::debug("DDrawSurface::UploadSurfaceData: Skipping upload");
      return DD_OK;
    }

    // Nothing to upload
    if (unlikely(!IsInitialized())) {
      Logger::warn("DDrawSurface::UploadSurfaceData: No wrapped surface or texture");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDrawSurface::UploadSurfaceData: Surface has 0 height or width");
      return DD_OK;
    }

    if (IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface, DDSURFACEDESC>(m_texture.ptr(), m_format, m_proxy.ptr(), m_mipCount);
    // Depth stencil do not need uploads (nor are they possible in D3D9)
    } else if (unlikely(IsDepthStencil())) {
      Logger::debug("DDrawSurface::UploadSurfaceData: Skipping upload of depth stencil");
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface<IDirectDrawSurface, DDSURFACEDESC>(m_d3d9.ptr(), m_format, m_proxy.ptr());
    }

    return DD_OK;
  }

}

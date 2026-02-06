#include "ddraw4_surface.h"

#include "ddraw4_gamma.h"
#include "ddraw4_interface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d6/d3d6_texture.h"

namespace dxvk {

  uint32_t DDraw4Surface::s_surfCount = 0;

  DDraw4Surface::DDraw4Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw4Surface* pParentSurf,
        IUnknown* origin,
        bool isChildObject)
    : DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_isChildObject ( isChildObject )
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf )
    , m_origin ( origin ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve the common interface!");
    }

    m_commonIntf->AddWrappedSurface(this);

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (!m_commonSurf->IsDesc2Set()) {
      DDSURFACEDESC2 desc2;
      desc2.dwSize = sizeof(DDSURFACEDESC2);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc2);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc2(desc2);
      }
    }

    m_commonSurf->SetDD4Surface(this);

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    m_surfCount = ++s_surfCount;

    ListSurfaceDetails();
  }

  DDraw4Surface::~DDraw4Surface() {
    m_commonIntf->RemoveWrappedSurface(this);

    if (m_parent != nullptr && m_isChildObject)
      m_parent->Release();

    m_commonSurf->SetDD4Surface(nullptr);

    Logger::debug(str::format("DDraw4Surface: Surface nr. [[4-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface4))
      return this;

    throw DxvkError("DDraw4Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDraw4GammaControl(std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawColorControl");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::warn("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::warn("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::warn("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::warn("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");

      Com<IDirectDrawSurface7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD7Interface(), nullptr, this, false));

      return S_OK;
    }
    // Standard way of retrieving a texture for d3d6 SetTexture calls
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirect3DTexture2");

      Com<IDirect3DTexture2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D6Texture(std::move(ppvProxyObject), this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::warn("DDraw2Surface::QueryInterface: Query for IDirect3DTexture");

      Com<IDirect3DTexture> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Texture(std::move(ppvProxyObject), m_commonSurf->GetDDSurface()));

      return S_OK;
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
    Logger::debug("<<< DDraw4Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
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
    Logger::debug("<<< DDraw4Surface::Blt: Proxy");

    const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                            && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d6Device->HasDrawn() &&
          m_commonSurf->IsGuardableSurface() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d6Device->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_d3d6Device->ResetDrawTracking();
        m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw4Surface::Blt: Received an unwrapped source surface");
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);

      if (unlikely(ddraw4Surface->GetCommonSurface()->IsDepthStencil()))
        Logger::warn("DDraw4Surface::Blt: Source surface is a depth stencil");

      hr = m_proxy->Blt(lpDestRect, ddraw4Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
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
    Logger::debug("<<< DDraw4Surface::BltFast: Proxy");

    const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                            && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_d3d6Device->HasDrawn() &&
          m_commonSurf->IsGuardableSurface() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_d3d6Device->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_d3d6Device->ResetDrawTracking();
        m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw4Surface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);

      if (unlikely(ddraw4Surface->GetCommonSurface()->IsDepthStencil()))
        Logger::warn("DDraw4Surface::BltFast: Source surface is a depth stencil");

      hr = m_proxy->BltFast(dwX, dwY, ddraw4Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw4Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw4Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
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
    Logger::debug(">>> DDraw4Surface::EnumAttachedSurfaces");

    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface4> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfaces4Callback);

    HRESULT hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr != D3DENUMRET_CANCEL) {
      Com<IDirectDrawSurface4> surface4 = surfaceIt->surface4;

      auto attachedSurfaceIter = m_attachedSurfaces.find(surface4.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface4), m_commonIntf->GetDD4Interface(), this, nullptr, false);
        m_attachedSurfaces.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                   std::forward_as_tuple(ddraw4Surface.ptr()));
        // TODO: We should ref here, but somehow we're leaking an interface attached surface if we do...
        hr = lpEnumSurfacesCallback(ddraw4Surface.ptr(), &surfaceIt->surface4Desc, lpContext);
      } else {
        hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ptr(), &surfaceIt->surface4Desc, lpContext);
      }

      ++surfaceIt;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback) {
    Logger::debug("<<< DDraw4Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (m_parent == nullptr) {
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

    if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw4Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw4Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw4Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw4Surface> surf4;
    if (m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride)) {
      surf4 = static_cast<DDraw4Surface*>(lpDDSurfaceTargetOverride);

      if (unlikely(!surf4->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw4Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D6Device();
    if (likely(m_d3d6Device != nullptr)) {
      D3D6DeviceLock lock = m_d3d6Device->LockDevice();

      m_d3d6Device->ResetDrawTracking();

      if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!IsInitialized()))
          InitializeD3D9(m_d3d6Device->GetRenderTarget() == this);
        // Also ensure the D3D6 device render target is initialized
        DDraw4Surface* rt6 = m_d3d6Device->GetRenderTarget();
        if (unlikely(!rt6->IsInitialized()))
          rt6->InitializeD3D9RenderTarget();

        BlitToDDrawSurface<IDirectDrawSurface4, DDSURFACEDESC2>(m_proxy.ptr(), rt6->GetD3D9());

        if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
          if (unlikely(lpDDSurfaceTargetOverride != nullptr))
            Logger::debug("DDraw4Surface::Flip: Received unwrapped surface");
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
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw4Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = m_d3d6Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = m_d3d6Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      }

      m_d3d6Device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
    // If we don't have a valid D3D6 device, this means a D3D7 application
    // is trying to flip the surface. Allow that for compatibility reasons.
    } else {
      Logger::debug("<<< DDraw4Surface::Flip: Proxy");
      if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
        m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        m_proxy->Flip(surf4->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface) {
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
    else if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
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

    if (likely(!m_commonIntf->IsWrappedSurface(surface.ptr()))) {
      Logger::debug("DDraw4Surface::GetAttachedSurface: Got a new unwrapped surface");
      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface), m_commonIntf->GetDD4Interface(), this, nullptr, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw4Surface.ptr()));
          // TODO: We should ref here, but somehow we're leaking an interface attached surface if we do...
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
    Logger::debug(">>> DDraw4Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc2()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDraw4Surface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw4Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDC(HDC *lphDC) {
    Logger::debug(">>> DDraw4Surface::GetDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw4Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D6Device();
    if (m_d3d6Device != nullptr && !(m_d3d6Device->HasDrawn() &&
                                     m_commonSurf->IsGuardableSurface())) {
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
    Logger::debug(">>> DDraw4Surface::GetPalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug(">>> DDraw4Surface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc2()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw4Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC2)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc2();

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
    // TODO: Directly lock d3d9 surfaces and skip surface uploads on unlock,
    // but it can get very involved, especially when dealing with DXT formats.
    Logger::debug("<<< DDraw4Surface::Lock: Proxy");

    if (unlikely(m_commonSurf->IsDepthStencil()))
        Logger::warn("DDraw4Surface::Lock: Surface is a depth stencil");

    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw4Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (m_commonSurf->IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw4Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D6Device();
    if (m_d3d6Device != nullptr && !(m_d3d6Device->HasDrawn() &&
                                     m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw4Surface::ReleaseDC: Not yet drawn flippable surface");
      if (m_commonSurf->IsTexture())
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    HRESULT hr = m_d3d9->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw4Surface::ReleaseDC: Failed to release d3d9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Restore() {
    Logger::debug("<<< DDraw4Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw4Surface::SetClipper: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Unlock(LPRECT lpSurfaceData) {
    Logger::debug("<<< DDraw4Surface::Unlock: Proxy");

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw4Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlay: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
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
    Logger::debug("<<< DDraw4Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw4Surface::UpdateOverlayZOrder: Called with an unwrapped surface");
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw4Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(m_commonIntf->GetDD4Interface() == nullptr)) {
      Logger::warn("<<< DDraw4Surface::GetDDInterface: Proxy");
      return m_proxy->GetDDInterface(lplpDD);
    }

    Logger::debug(">>> DDraw4Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_commonIntf->GetDD4Interface());

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
    Logger::debug("<<< DDraw4Surface::SetSurfaceDesc: Proxy");

    // Can be used only to set the surface data and pixel format
    // used by an explicit system-memory surface (will be validated)
    HRESULT hr = m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    // Update the new surface description
    DDSURFACEDESC2 desc2;
    desc2.dwSize = sizeof(DDSURFACEDESC2);
    hr = m_proxy->GetSurfaceDesc(&desc2);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw4Surface::SetSurfaceDesc: Failed to get new surface desc");
    } else {
      m_commonSurf->SetDesc2(desc2);
    }

    // We may need to recreate the d3d9 object based on the new desc
    m_d3d9 = nullptr;

    if (!m_commonSurf->IsTexture()) {
      InitializeOrUploadD3D9();
    } else {
      m_commonSurf->DirtyMipMaps();
    }

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
      hr = InitializeD3D9(true);

    return hr;
  }

  HRESULT DDraw4Surface::InitializeOrUploadD3D9() {
    HRESULT hr = DD_OK;

    RefreshD3D6Device();

    if (likely(IsInitialized())) {
      hr = UploadSurfaceData();
    } else {
      hr = InitializeD3D9(false);
    }

    return hr;
  }

  inline HRESULT DDraw4Surface::InitializeD3D9(const bool initRT) {
    if (unlikely(m_d3d6Device == nullptr)) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Null device, can't initialize right now");
      return DD_OK;
    }

    Logger::debug(str::format("DDraw4Surface::InitializeD3D9: Initializing nr. [[4-", m_surfCount, "]]"));

    const DDSURFACEDESC2* desc2  = m_commonSurf->GetDesc2();
    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    if (unlikely(desc2->dwHeight == 0 || desc2->dwWidth == 0)) {
      Logger::err("DDraw4Surface::InitializeD3D9: Surface has 0 height or width");
      return DDERR_GENERIC;
    }

    if (unlikely(format == d3d9::D3DFMT_UNKNOWN)) {
      Logger::err("DDraw4Surface::InitializeD3D9: Surface has an unknown format");
      return DDERR_GENERIC;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(format == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDraw4Surface::InitializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;

    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(format == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDraw4Surface::InitializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.
    if (m_commonSurf->IsTexture()) {
      IDirectDrawSurface4* mipMap = m_proxy.ptr();

      uint8_t mipCount = 1;
      while (mipMap != nullptr) {
        IDirectDrawSurface4* parentSurface = mipMap;
        mipMap = nullptr;
        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces4Callback);
        if (mipMap != nullptr) {
          mipCount++;
        }
      }

      // Do not worry about maximum supported mip map levels validation,
      // because D3D9 will hanlde this for us and cap them appropriately
      if (mipCount > 1) {
        Logger::debug(str::format("DDraw4Surface::InitializeD3D9: Found ", mipCount, " mip levels"));

        if (unlikely(mipCount != desc2->dwMipMapCount))
          Logger::debug(str::format("DDraw4Surface::InitializeD3D9: Mismatch with declared ", desc2->dwMipMapCount, " mip levels"));
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDraw4Surface::InitializeD3D9: Using auto mip map generation");
        mipCount = 0;
      }

      m_commonSurf->SetMipCount(mipCount);
    }

    d3d9::D3DPOOL pool  = d3d9::D3DPOOL_DEFAULT;
    DWORD         usage = 0;

    // General surface/texture pool placement
    if (desc2->ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)
      pool = d3d9::D3DPOOL_DEFAULT;
    // There's no explicit non-local video memory placement
    // per se, but D3DPOOL_MANAGED is close enough
    else if ((desc2->ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM) || (desc2->ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE))
      pool = d3d9::D3DPOOL_MANAGED;
    else if (desc2->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
      // We can't know beforehand if a texture is or isn't going to be
      // used in SetTexture() calls, and textures placed in D3DPOOL_SYSTEMMEM
      // will not work in that context in dxvk, so revert to D3DPOOL_MANAGED.
      pool = m_commonSurf->IsTexture() ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_SYSTEMMEM;

    // Place all possible render targets in DEFAULT
    //
    // Note: This is somewhat problematic for textures and cube maps
    // which will have D3DUSAGE_RENDERTARGET, but also need to have
    // D3DUSAGE_DYNAMIC for locking/uploads to work. The flag combination
    // isn't supported in D3D9, but we have a D3D7 exception in place.
    //
    if (m_commonSurf->IsRenderTarget() || initRT) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (m_commonSurf->IsTexture()) {
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        Logger::debug("DDraw4Surface::InitializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDraw4Surface::InitializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDraw4Surface::InitializeD3D9: Placing in: ", poolPlacement));

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_d3d6Device->GetMultiSampleType();

    Com<d3d9::IDirect3DSurface9> surf;

    HRESULT hr = DDERR_GENERIC;

    // Front Buffer
    if (m_commonSurf->IsFrontBuffer()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing front buffer...");

      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to retrieve front buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Back Buffer
    } else if (m_commonSurf->IsBackBufferOrFlippable()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing back buffer...");

      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to retrieve back buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Textures
    } else if (m_commonSurf->IsTexture()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing texture...");

      Com<d3d9::IDirect3DTexture9> tex;

      hr = m_d3d6Device->GetD3D9()->CreateTexture(
        desc2->dwWidth, desc2->dwHeight, m_commonSurf->GetMipCount(), usage,
        format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps))
        tex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Attach level 0 to this surface
      tex->GetSurfaceLevel(0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDraw4Surface::InitializeD3D9: Created texture");
      m_texture = std::move(tex);

    // Depth Stencil
    } else if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing depth stencil...");

      hr = m_d3d6Device->GetD3D9()->CreateDepthStencilSurface(
        desc2->dwWidth, desc2->dwHeight, format,
        multiSampleType, 0, FALSE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw4Surface::InitializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(surf);

    // Offscreen Plain Surfaces
    } else if (m_commonSurf->IsOffScreenPlainSurface()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing offscreen plain surface...");

      // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer,
      // either as existing RTs or during SetRenderTarget() calls, which are tracked with initRT
      if (unlikely(m_d3d6Device->GetRenderTarget() == this || initRT)) {
        Logger::debug("DDraw4Surface::InitializeD3D9: Offscreen plain surface is the RT");

        surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

        if (unlikely(surf == nullptr)) {
          Logger::err("DDraw4Surface::InitializeD3D9: Failed to retrieve offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      } else {
        hr = m_d3d6Device->GetD3D9()->CreateOffscreenPlainSurface(
          desc2->dwWidth, desc2->dwHeight, format,
          pool, &surf, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDraw4Surface::InitializeD3D9: Failed to create offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      }

      m_d3d9 = std::move(surf);

    // Overlays (haven't seen any actual use of overlays in the wild)
    } else if (m_commonSurf->IsOverlay()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing overlay...");

      // Always link overlays to the current render target
      surf = m_d3d6Device->GetD3D9BackBuffer(m_proxy.ptr());

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to retrieve overlay surface");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Generic render target
    } else if (m_commonSurf->IsRenderTarget()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing render target...");

      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = m_d3d6Device->GetD3D9()->CreateRenderTarget(
        desc2->dwWidth, desc2->dwHeight, format,
        multiSampleType, usage, TRUE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to create render target");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else if (!m_commonSurf->IsNotKnown()) {
      Logger::debug("DDraw4Surface::InitializeD3D9: Initializing generic surface...");

      hr = m_d3d6Device->GetD3D9()->CreateOffscreenPlainSurface(
          desc2->dwWidth, desc2->dwHeight, format,
          pool, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Surface::InitializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw4Surface::InitializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    } else {
      Logger::warn("DDraw4Surface::InitializeD3D9: Skipping initialization of unknown surface");
    }

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDraw4Surface::UploadSurfaceData() {
    Logger::debug(str::format("DDraw4Surface::UploadSurfaceData: Uploading nr. [[4-", m_surfCount, "]]"));

    if (unlikely(m_d3d6Device->HasDrawn() && m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw4Surface::UploadSurfaceData: Skipping upload");
      return DD_OK;
    }

    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface4, DDSURFACEDESC2>(m_texture.ptr(), format,
                                                             m_proxy.ptr(), m_commonSurf->GetMipCount());
    // Depth stencil do not need uploads (nor are they possible in D3D9)
    } else if (unlikely(m_commonSurf->IsDepthStencil())) {
      Logger::debug("DDraw4Surface::UploadSurfaceData: Skipping upload of depth stencil");
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface<IDirectDrawSurface4, DDSURFACEDESC2>(m_d3d9.ptr(), format, m_proxy.ptr());
    }

    return DD_OK;
  }

}

#include "ddraw7_surface.h"

#include "ddraw7_gamma.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d6/d3d6_texture.h"

namespace dxvk {

  uint32_t DDraw7Surface::s_surfCount = 0;

  DDraw7Surface::DDraw7Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface7>&& surfProxy,
        DDraw7Interface* pParent,
        DDraw7Surface* pParentSurf,
        IUnknown* origin,
        bool isChildObject)
    : DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
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
      throw DxvkError("DDraw7Surface: ERROR! Failed to retrieve the common interface!");
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
        throw DxvkError("DDraw7Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc2(desc2);
      }
    }

    // Retrieve and cache the proxy surface color key
    if (m_commonSurf->HasColorKey() && !m_commonSurf->IsColorKeySet()) {
      DDCOLORKEY colorKey;

      HRESULT hr = m_proxy->GetColorKey(DDCKEY_SRCBLT, &colorKey);
      // Can return DDERR_NOCOLORKEY
      if (SUCCEEDED(hr))
        m_commonSurf->SetColorKey(&colorKey);
    }

    m_commonSurf->SetDD7Surface(this);

    if (m_parentSurf != nullptr && !m_commonSurf->IsFrontBuffer()
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()
     && !m_commonIntf->GetOptions()->forceSingleBackBuffer) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (likely(m_isChildObject))
      m_parent->AddRef();

    // Cube map face surfaces
    m_cubeMapSurfaces.fill(nullptr);

    m_surfCount = ++s_surfCount;

    ListSurfaceDetails();
  }

  DDraw7Surface::~DDraw7Surface() {
    m_commonIntf->RemoveWrappedSurface(this);

    if (likely(m_isChildObject))
      m_parent->Release();

    m_commonSurf->SetDD7Surface(nullptr);

    Logger::debug(str::format("DDraw7Surface: Surface nr. [[7-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface7))
      return this;

    throw DxvkError("DDraw7Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDraw7GammaControl(std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirectDrawColorControl");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw surfaces
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::warn("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, this, false));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::warn("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::warn("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::warn("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, this, false));

      return S_OK;
    }
    // Black & White queries for IDirect3DTexture2 for whatever reason...
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_texture6 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = 0;
        if (likely(m_commonIntf->GetDDInterface() != nullptr))
          nextHandle = m_commonIntf->GetDDInterface()->GetNextTextureHandle();
        m_texture6 = new D3D6Texture(std::move(ppvProxyObject), m_commonSurf->GetDD4Surface(), nextHandle);
        if (likely(m_commonIntf->GetDDInterface() != nullptr)) {
          D3DCommonTexture* commonTex = m_texture6->GetCommonTexture();
          m_commonIntf->GetDDInterface()->EmplaceTexture(commonTex, nextHandle);
        }
      }

      *ppvObject = m_texture6.ref();

      return S_OK;
    }
    // Shouldn't ever be called in practice
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::warn("DDraw7Surface::QueryInterface: Query for IDirect3DTexture");
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

  // This call will only attach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  // More than that, the attached surfaces do not need to be manageed by the object, the docs state:
  // "Unlike complex surfaces that you create with a single call to IDirectDraw7::CreateSurface, surfaces
  // attached with this method are not automatically released."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw7Surface::AddAttachedSurface: Attaching unwrapped surface");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(ddraw7Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddraw7Surface->SetParentSurface(this);
    m_depthStencil = ddraw7Surface;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw7Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw7Surface::Blt: Proxy");

    const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                            && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_commonIntf->HasDrawn() &&
          m_commonSurf->IsGuardableSurface() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_commonIntf->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_commonIntf->ResetDrawTracking();
        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    if (unlikely(m_commonSurf->IsDepthStencil())) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (lpDDSrcSurface == nullptr &&
          (dwFlags & DDBLT_DEPTHFILL) &&
           lpDDBltFx != nullptr &&
           m_d3d9Device != nullptr &&
           m_commonIntf->IsCurrentD3D9DepthStencil(m_d3d9.ptr())) {
        Logger::debug("DDraw7Surface::Blt: Clearing D3D9 depth stencil");

        const float zClear = m_commonSurf->GetNormalizedFloatDepth(lpDDBltFx->dwFillDepth);
        if (lpDestRect == nullptr) {
          m_d3d9Device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, zClear, 0);
        } else {
          D3DRECT rect9;
          memcpy(&rect9, lpDestRect, sizeof(D3DRECT));
          m_d3d9Device->Clear(1, &rect9, D3DCLEAR_ZBUFFER, 0, zClear, 0);
        }
      }
    }

    if (likely(m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* ddraw7Depth = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      if (unlikely(ddraw7Depth != nullptr && ddraw7Depth->GetCommonSurface()->IsDepthStencil())) {
        if (m_commonIntf->GetOptions()->depthWriteBack && m_commonIntf->IsCurrentDepthStencil(ddraw7Depth)) {
          if (unlikely(!ddraw7Depth->IsInitialized())) {
            HRESULT hrInit = ddraw7Depth->InitializeOrUploadD3D9();
            if (unlikely(FAILED(hrInit)))
              Logger::warn("DDraw7Surface::Blt: Failed to initialize d3d9 depth surface");
          }
          if (likely(IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(ddraw7Depth->GetProxied(), ddraw7Depth->GetD3D9());
        } else {
          static bool s_depthStencilWarningShown;

          if (!std::exchange(s_depthStencilWarningShown, true))
            Logger::warn("DDraw7Surface::Blt: Source surface is a depth stencil");
        }
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw7Surface::Blt: Received an unwrapped source surface");

      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw7Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap() || m_commonIntf->GetOptions()->apitraceMode) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  // The docs state: "The IDirectDrawSurface7::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw7Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw7Surface::BltFast: Proxy");

    const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                            && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      // Eclusive mode back buffer guard
      if (exclusiveMode && m_commonIntf->HasDrawn() &&
          m_commonSurf->IsGuardableSurface() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_commonIntf->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_commonIntf->ResetDrawTracking();
        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    if (likely(m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* ddraw7Depth = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      if (unlikely(ddraw7Depth != nullptr && ddraw7Depth->GetCommonSurface()->IsDepthStencil())) {
        if (m_commonIntf->GetOptions()->depthWriteBack && m_commonIntf->IsCurrentDepthStencil(ddraw7Depth)) {
          if (unlikely(!ddraw7Depth->IsInitialized())) {
            HRESULT hrInit = ddraw7Depth->InitializeOrUploadD3D9();
            if (unlikely(FAILED(hrInit)))
              Logger::warn("DDraw7Surface::BltFast: Failed to initialize d3d9 depth surface");
          }
          if (likely(IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(ddraw7Depth->GetProxied(), ddraw7Depth->GetD3D9());
        } else {
          static bool s_depthStencilWarningShown;

          if (!std::exchange(s_depthStencilWarningShown, true))
            Logger::warn("DDraw7Surface::BltFast: Source surface is a depth stencil");
        }
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw7Surface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw7Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap() || m_commonIntf->GetOptions()->apitraceMode) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr))
        Logger::warn("DDraw7Surface::DeleteAttachedSurface: Deleting unwrapped surface");

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, ddraw7Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(m_depthStencil == ddraw7Surface)) {
      ddraw7Surface->ClearParentSurface();
      m_depthStencil = nullptr;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    Logger::debug(">>> DDraw7Surface::EnumAttachedSurfaces");

    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface7> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfaces7Callback);

    HRESULT hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr != DDENUMRET_CANCEL) {
      Com<IDirectDrawSurface7> surface7 = surfaceIt->surface7;

      auto attachedSurfaceIter = m_attachedSurfaces.find(surface7.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface7), m_parent, this, nullptr, false);
        m_attachedSurfaces.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                   std::forward_as_tuple(ddraw7Surface.ptr()));
        // TODO: We should ref here, but somehow we're leaking an interface attached surface if we do...
        hr = lpEnumSurfacesCallback(ddraw7Surface.ptr(), &surfaceIt->surface7Desc, lpContext);
      } else {
        hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ptr(), &surfaceIt->surface7Desc, lpContext);
      }

      ++surfaceIt;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpfnCallback) {
    Logger::debug("<<< DDraw7Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Flip(LPDIRECTDRAWSURFACE7 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("*** DDraw7Surface::Flip: Presenting");

    // Lost surfaces are not flippable
    HRESULT hr = m_proxy->IsLost();
    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw7Surface::Flip: Lost surface");
      return hr;
    }

    if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw7Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw7Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw7Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw7Surface> surf7;
    if (m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride)) {
      surf7 = static_cast<DDraw7Surface*>(lpDDSurfaceTargetOverride);

      if (unlikely(!surf7->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw7Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      Logger::debug("*** DDraw7Surface::Flip: Presenting");

      m_commonIntf->ResetDrawTracking();

      if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!IsInitialized()))
          InitializeD3D9(m_commonIntf->IsCurrentRenderTarget(this));

        BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(m_proxy.ptr(), m_d3d9.ptr());

        if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
          if (unlikely(lpDDSurfaceTargetOverride != nullptr))
            Logger::debug("DDraw7Surface::Flip: Received unwrapped surface");
          if (likely(m_commonIntf->IsCurrentRenderTarget(this)))
            m_commonIntf->SetFlipRTSurfaceAndFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        } else {
          if (likely(m_commonIntf->IsCurrentRenderTarget(this)))
            m_commonIntf->SetFlipRTSurfaceAndFlags(lpDDSurfaceTargetOverride, dwFlags);
          return m_proxy->Flip(surf7->GetProxied(), dwFlags);
        }
      }

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = m_commonIntf->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = m_commonIntf->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      }

      m_d3d9Device->Present(NULL, NULL, NULL, NULL);
    // If we don't yet have a device, perhaps a D3D7 application is trying to
    // present exclusively on DDraw (such as a main menu), so allow the flip
    } else {
      Logger::debug("<<< DDraw7Surface::Flip: Proxy");
      if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
        m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        m_proxy->Flip(surf7->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for the primary surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for a flippable surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for an offscreen plain surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for a depth stencil");
    else if ((lpDDSCaps->dwCaps  & DDSCAPS_MIPMAP)
          || (lpDDSCaps->dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL))
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for a texture");
    else if (lpDDSCaps->dwCaps2 & DDSCAPS2_CUBEMAP)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for a cube map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for an overlay");

    Com<IDirectDrawSurface7> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDraw7Surface::GetAttachedSurface: Failed to find the requested surface");
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    if (likely(!m_commonIntf->IsWrappedSurface(surface.ptr()))) {
      Logger::debug("DDraw7Surface::GetAttachedSurface: Got a new unwrapped surface");
      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface), m_parent, this, nullptr, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw7Surface.ptr()));
          // TODO: We should ref here, but somehow we're leaking an interface attached surface if we do...
          *lplpDDAttachedSurface = ddraw7Surface.ptr();
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
      Logger::debug("DDraw7Surface::GetAttachedSurface: Got an existing wrapped surface");
      Com<DDraw7Surface> ddraw7Surface = static_cast<DDraw7Surface*>(surface.ptr());
      *lplpDDAttachedSurface = ddraw7Surface.ref();
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    Logger::debug(">>> DDraw7Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc2()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDraw7Surface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw7Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDC(HDC *lphDC) {
    Logger::debug(">>> DDraw7Surface::GetDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
      return m_proxy->GetDC(lphDC);

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    // Proxy GetDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D9Device();
    if (m_d3d9Device != nullptr && !(m_commonIntf->HasDrawn()
                                  && m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw7Surface::GetDC: Not yet drawn flippable surface");
      return m_proxy->GetDC(lphDC);
    }

    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lphDC);

    HRESULT hr = m_d3d9->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::GetDC: Failed to get D3D9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDraw7Surface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug(">>> DDraw7Surface::GetPalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug(">>> DDraw7Surface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc2()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw7Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC2)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc2();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw7Surface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::IsLost() {
    Logger::debug("<<< DDraw7Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw7Surface::Lock: Proxy");

    if (unlikely(m_commonSurf->IsDepthStencil())) {
      if (m_commonIntf->GetOptions()->depthWriteBack && m_commonIntf->IsCurrentDepthStencil(this)) {
        if (unlikely(!IsInitialized())) {
          HRESULT hrInit = InitializeOrUploadD3D9();
          if (unlikely(FAILED(hrInit)))
            Logger::warn("DDraw7Surface::Lock: Failed to initialize d3d9 depth surface");
        }
        if (likely(IsInitialized()))
          BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(m_proxy.ptr(), m_d3d9.ptr());
      } else {
        static bool s_depthStencilWarningShown;

        if (!std::exchange(s_depthStencilWarningShown, true))
          Logger::warn("DDraw7Surface::Lock: Surface is a depth stencil");
      }
    }

    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw7Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (m_commonSurf->IsTextureOrCubeMap() && !m_commonIntf->GetOptions()->apitraceMode)
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D9Device();
    if (m_d3d9Device != nullptr && !(m_commonIntf->HasDrawn()
                                  && m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw7Surface::ReleaseDC: Not yet drawn flippable surface");
      if (m_commonSurf->IsTextureOrCubeMap() && !m_commonIntf->GetOptions()->apitraceMode)
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    HRESULT hr = m_d3d9->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::ReleaseDC: Failed to release d3d9 DC");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Restore() {
    Logger::debug("<<< DDraw7Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw7Surface::SetClipper: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw7Surface::SetColorKey: Proxy");

    HRESULT hr = m_proxy->SetColorKey(dwFlags, lpDDColorKey);
    if (unlikely(FAILED(hr)))
      return hr;

    if (dwFlags == DDCKEY_SRCBLT) {
      Logger::debug("DDraw7Surface::SetColorKey: Updating DDCKEY_SRCBLT color key");

      if (lpDDColorKey != nullptr) {
        m_commonSurf->SetColorKey(lpDDColorKey);

        if (unlikely(lpDDColorKey->dwColorSpaceLowValue  != 0 ||
                    lpDDColorKey->dwColorSpaceHighValue != 0))
          Logger::debug("DDraw7Surface::SetColorKey: Use of non-black color key");
      } else {
        m_commonSurf->ClearColorKey();
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw7Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw7Surface::SetPalette: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Unlock(LPRECT lpSurfaceData) {
    Logger::debug("<<< DDraw7Surface::Unlock: Proxy");

    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap() || m_commonIntf->GetOptions()->apitraceMode) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlay: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw7Surface::UpdateOverlay: Called with an unwrapped surface");
      return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw7Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw7Surface::UpdateOverlayZOrder: Called with an unwrapped surface");
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw7Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDDInterface(LPVOID *lplpDD) {
    Logger::debug(">>> DDraw7Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = ref(m_parent);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::PageLock(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::PageLock: Proxy");
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::PageUnlock(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::PageUnlock: Proxy");
    return m_proxy->PageUnlock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::SetSurfaceDesc: Proxy");

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
      Logger::err("DDraw7Surface::SetSurfaceDesc: Failed to get new surface desc");
    } else {
      m_commonSurf->SetDesc2(desc2);
    }

    // We may need to recreate the d3d9 object based on the new desc
    m_d3d9 = nullptr;

    if (!m_commonSurf->IsTextureOrCubeMap() || m_commonIntf->GetOptions()->apitraceMode) {
      InitializeOrUploadD3D9();
    } else {
      m_commonSurf->DirtyMipMaps();
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::SetPrivateData: Proxy");
    return m_proxy->SetPrivateData(tag, pData, cbSize, dwFlags);
  }

  // Silent Hunter II uses GetPrivateData and relies on some sort of ddraw validation...
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    Logger::debug("<<< DDraw7Surface::GetPrivateData: Proxy");
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::FreePrivateData(REFGUID tag) {
    Logger::debug("<<< DDraw7Surface::FreePrivateData: Proxy");
    return m_proxy->FreePrivateData(tag);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetUniquenessValue(LPDWORD pValue) {
    Logger::debug("<<< DDraw7Surface::GetUniquenessValue: Proxy");
    return m_proxy->GetUniquenessValue(pValue);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ChangeUniquenessValue() {
    Logger::debug("<<< Called DDraw7Surface::ChangeUniquenessValue: Proxy");
    return m_proxy->ChangeUniquenessValue();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPriority(DWORD prio) {
    Logger::debug(">>> DDraw7Surface::SetPriority");

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::SetPriority: Not yet initialized");
      return m_proxy->SetPriority(prio);
    }

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    HRESULT hr = m_proxy->SetPriority(prio);
    if (unlikely(FAILED(hr)))
      return hr;

    m_d3d9->SetPriority(prio);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPriority(LPDWORD prio) {
    Logger::debug(">>> DDraw7Surface::GetPriority");

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::GetPriority: Not yet initialized");
      return m_proxy->GetPriority(prio);
    }

    if (unlikely(prio == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    *prio = m_d3d9->GetPriority();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetLOD(DWORD lod) {
    Logger::debug("<<< DDraw7Surface::SetLOD: Proxy");
    return m_proxy->SetLOD(lod);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetLOD(LPDWORD lod) {
    Logger::debug("<<< DDraw7Surface::GetLOD: Proxy");
    return m_proxy->GetLOD(lod);
  }

  HRESULT DDraw7Surface::InitializeD3D9RenderTarget() {
    HRESULT hr = DD_OK;

    RefreshD3D9Device();

    if (unlikely(!IsInitialized()))
      hr = InitializeD3D9(true);

    return hr;
  }

  HRESULT DDraw7Surface::InitializeOrUploadD3D9() {
    HRESULT hr = DD_OK;

    RefreshD3D9Device();

    if (likely(IsInitialized())) {
      hr = UploadSurfaceData();
    } else {
      hr = InitializeD3D9(false);
    }

    return hr;
  }

  inline void DDraw7Surface::InitializeAndAttachCubeFace(
        IDirectDrawSurface7* surf,
        d3d9::IDirect3DCubeTexture9* cubeTex9,
        d3d9::D3DCUBEMAP_FACES face) {
    Com<DDraw7Surface> face7;
    if (likely(!m_commonIntf->IsWrappedSurface(surf))) {
      Com<IDirectDrawSurface7> wrappedFace = surf;
      try {
        face7 = new DDraw7Surface(nullptr, std::move(wrappedFace), m_parent, this, nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err("InitializeAndAttachCubeFace: Failed to create wrapped cube face surface");
        Logger::err(e.message());
      }
    } else {
      face7 = static_cast<DDraw7Surface*>(surf);
    }

    if (likely(face7 != nullptr)) {
      Com<d3d9::IDirect3DSurface9> face9;
      cubeTex9->GetCubeMapSurface(face, 0, &face9);
      face7->SetD3D9(std::move(face9));
      m_attachedSurfaces.emplace(std::piecewise_construct,
                                std::forward_as_tuple(face7->GetProxied()),
                                std::forward_as_tuple(face7.ptr()));
    }
  }

  inline HRESULT DDraw7Surface::InitializeD3D9(const bool initRT) {
    if (unlikely(m_d3d9Device == nullptr)) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Null device, can't initialize right now");
      return DD_OK;
    }

    Logger::debug(str::format("DDraw7Surface::InitializeD3D9: Initializing nr. [[7-", m_surfCount, "]]"));

    const DDSURFACEDESC2* desc2  = m_commonSurf->GetDesc2();
    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    if (unlikely(desc2->dwHeight == 0 || desc2->dwWidth == 0)) {
      Logger::err("DDraw7Surface::InitializeD3D9: Surface has 0 height or width");
      return DDERR_GENERIC;
    }

    if (unlikely(format == d3d9::D3DFMT_UNKNOWN)) {
      Logger::err("DDraw7Surface::InitializeD3D9: Surface has an unknown format");
      return DDERR_GENERIC;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(format == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDraw7Surface::InitializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;

    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(format == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDraw7Surface::InitializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.
    if (m_commonSurf->IsTextureOrCubeMap()) {
      IDirectDrawSurface7* mipMap = m_proxy.ptr();

      uint8_t mipCount = 1;
      while (mipMap != nullptr) {
        IDirectDrawSurface7* parentSurface = mipMap;
        mipMap = nullptr;
        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
        if (mipMap != nullptr) {
          mipCount++;
        }
      }

      // Do not worry about maximum supported mip map levels validation,
      // because D3D9 will hanlde this for us and cap them appropriately
      if (mipCount > 1) {
        Logger::debug(str::format("DDraw7Surface::InitializeD3D9: Found ", mipCount, " mip levels"));

        if (unlikely(mipCount != desc2->dwMipMapCount))
          Logger::debug(str::format("DDraw7Surface::InitializeD3D9: Mismatch with declared ", desc2->dwMipMapCount, " mip levels"));
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Using auto mip map generation");
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
      pool = m_commonSurf->IsTextureOrCubeMap() ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_SYSTEMMEM;

    // Place all possible render targets in DEFAULT
    //
    // Note: This is somewhat problematic for textures and cube maps
    // which will have D3DUSAGE_RENDERTARGET, but also need to have
    // D3DUSAGE_DYNAMIC for locking/uploads to work. The flag combination
    // isn't supported in D3D9, but we have a D3D7 exception in place.
    //
    if (m_commonSurf->IsRenderTarget() || initRT) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (m_commonSurf->IsTextureOrCubeMap()) {
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDraw7Surface::InitializeD3D9: Placing in: ", poolPlacement));

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_commonIntf->GetMultiSampleType();
    const uint32_t index = m_commonSurf->GetBackBufferIndex();

    Com<d3d9::IDirect3DSurface9> surf;

    HRESULT hr = DDERR_GENERIC;

    // Front Buffer
    if (m_commonSurf->IsFrontBuffer()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing front buffer...");

      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to retrieve front buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Back Buffer
    } else if (m_commonSurf->IsBackBufferOrFlippable()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing back buffer...");

      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to retrieve back buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Cube maps
    } else if (m_commonSurf->IsCubeMap()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing cube map...");

      Com<d3d9::IDirect3DCubeTexture9> cubetex;

      hr = m_d3d9Device->CreateCubeTexture(
        desc2->dwWidth, m_commonSurf->GetMipCount(), usage,
        format, pool, &cubetex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create cube map");
        m_cubeMap = nullptr;
        return hr;
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps))
        cubetex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Always attach the positive X face to this surface
      cubetex->GetCubeMapSurface(d3d9::D3DCUBEMAP_FACE_POSITIVE_X, 0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDraw7Surface::InitializeD3D9: Retrieving cube map faces");

      CubeMapAttachedSurfaces cubeMapAttachedSurfaces;
      m_proxy->EnumAttachedSurfaces(&cubeMapAttachedSurfaces, EnumAndAttachCubeMapFacesCallback);

      // The positive X surfaces is this surface, so nothing should be retrieved
      if (unlikely(cubeMapAttachedSurfaces.positiveX != nullptr))
        Logger::warn("DDraw7Surface::InitializeD3D9: Non-null positive X cube map face");

      m_cubeMapSurfaces[0] = m_proxy.ptr();

      // We can't know in advance which faces have been generated,
      // so check them one by one, initialize and bind as needed
      if (cubeMapAttachedSurfaces.negativeX != nullptr) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Detected negative X cube map face");
        m_cubeMapSurfaces[1] = cubeMapAttachedSurfaces.negativeX;
        InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeX, cubetex.ptr(),
                                    d3d9::D3DCUBEMAP_FACE_NEGATIVE_X);
      }
      if (cubeMapAttachedSurfaces.positiveY != nullptr) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Detected positive Y cube map face");
        m_cubeMapSurfaces[2] = cubeMapAttachedSurfaces.positiveY;
        InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.positiveY, cubetex.ptr(),
                                    d3d9::D3DCUBEMAP_FACE_POSITIVE_Y);
      }
      if (cubeMapAttachedSurfaces.negativeY != nullptr) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Detected negative Y cube map face");
        m_cubeMapSurfaces[3] = cubeMapAttachedSurfaces.negativeY;
        InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeY, cubetex.ptr(),
                                    d3d9::D3DCUBEMAP_FACE_NEGATIVE_Y);
      }
      if (cubeMapAttachedSurfaces.positiveZ != nullptr) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Detected positive Z cube map face");
        m_cubeMapSurfaces[4] = cubeMapAttachedSurfaces.positiveZ;
        InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.positiveZ, cubetex.ptr(),
                                    d3d9::D3DCUBEMAP_FACE_POSITIVE_Z);
      }
      if (cubeMapAttachedSurfaces.negativeZ != nullptr) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Detected negative Z cube map face");
        m_cubeMapSurfaces[5] = cubeMapAttachedSurfaces.negativeZ;
        InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeZ, cubetex.ptr(),
                                    d3d9::D3DCUBEMAP_FACE_NEGATIVE_Z);
      }

      Logger::debug("DDraw7Surface::InitializeD3D9: Created cube map");
      m_cubeMap = std::move(cubetex);

    // Textures
    } else if (m_commonSurf->IsTexture()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing texture...");

      Com<d3d9::IDirect3DTexture9> tex;

      hr = m_d3d9Device->CreateTexture(
        desc2->dwWidth, desc2->dwHeight, m_commonSurf->GetMipCount(), usage,
        format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create texture");
        m_texture9 = nullptr;
        return hr;
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps))
        tex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Attach level 0 to this surface
      tex->GetSurfaceLevel(0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDraw7Surface::InitializeD3D9: Created texture");
      m_texture9 = std::move(tex);

    // Depth Stencil
    } else if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing depth stencil...");

      hr = m_d3d9Device->CreateDepthStencilSurface(
        desc2->dwWidth, desc2->dwHeight, format,
        multiSampleType, 0, FALSE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::InitializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(surf);

    // Offscreen Plain Surfaces
    } else if (m_commonSurf->IsOffScreenPlainSurface()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing offscreen plain surface...");

      // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer,
      // either as existing RTs or during SetRenderTarget() calls, which are tracked with initRT
      if (unlikely(m_commonIntf->IsCurrentRenderTarget(this) || initRT)) {
        Logger::debug("DDraw7Surface::InitializeD3D9: Offscreen plain surface is the RT");

        m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

        if (unlikely(surf == nullptr)) {
          Logger::err("DDraw7Surface::InitializeD3D9: Failed to retrieve offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      } else {
        hr = m_d3d9Device->CreateOffscreenPlainSurface(
          desc2->dwWidth, desc2->dwHeight, format,
          pool, &surf, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDraw7Surface::InitializeD3D9: Failed to create offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      }

      m_d3d9 = std::move(surf);

    // Overlays (haven't seen any actual use of overlays in the wild)
    } else if (m_commonSurf->IsOverlay()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing overlay...");

      // Always link overlays to the back buffer
      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to retrieve overlay surface");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // Generic render target
    } else if (m_commonSurf->IsRenderTarget()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing render target...");

      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = m_d3d9Device->CreateRenderTarget(
        desc2->dwWidth, desc2->dwHeight, format,
        multiSampleType, usage, TRUE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create render target");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else if (!m_commonSurf->IsNotKnown()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing generic surface...");

      hr = m_d3d9Device->CreateOffscreenPlainSurface(
          desc2->dwWidth, desc2->dwHeight, format,
          pool, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::InitializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    } else {
      Logger::warn("DDraw7Surface::InitializeD3D9: Skipping initialization of unknown surface");
    }

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDraw7Surface::UploadSurfaceData() {
    Logger::debug(str::format("DDraw7Surface::UploadSurfaceData: Uploading nr. [[7-", m_surfCount, "]]"));

    if (unlikely(m_commonIntf->HasDrawn() && m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw7Surface::UploadSurfaceData: Skipping upload");
      return DD_OK;
    }

    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    // Cube maps will also get marked as textures, so need to be handled first
    if (unlikely(m_commonSurf->IsCubeMap())) {
      // In theory we won't know which faces have been generated,
      // so check them one by one, and upload as needed
      const uint8_t mipCount = m_commonSurf->GetMipCount();
      if (likely(m_cubeMapSurfaces[0] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[0], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive X face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[1] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[1], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative X face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[2] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[2], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive Y face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[3] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[3], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative Y face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[4] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[4], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive Z face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[5] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap.ptr(), format, m_cubeMapSurfaces[5], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative Z face is null, skpping");
      }
    // Blit all the mips for textures
    } else if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface7, DDSURFACEDESC2>(m_texture9.ptr(), format,
                                                             m_proxy.ptr(), m_commonSurf->GetMipCount());
    // Depth stencil do not need uploads (nor are they possible in D3D9)
    } else if (unlikely(m_commonSurf->IsDepthStencil())) {
      Logger::debug("DDraw7Surface::UploadSurfaceData: Skipping upload of depth stencil");
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface<IDirectDrawSurface7, DDSURFACEDESC2>(m_d3d9.ptr(), format, m_proxy.ptr());
    }

    return DD_OK;
  }

}

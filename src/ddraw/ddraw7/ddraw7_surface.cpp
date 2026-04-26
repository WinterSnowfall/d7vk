#include "ddraw7_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"
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
        bool isChildObject)
    : DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_isChildObject ( isChildObject )
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw7Surface: ERROR! Failed to retrieve the common interface!");
    }

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

    m_commonIntf->AddWrappedSurface(this);

    m_commonSurf->SetDD7Surface(this);

    if (m_parentSurf != nullptr
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()
     && !m_commonIntf->GetOptions()->forceSingleBackBuffer
     && !m_commonIntf->GetOptions()->forceLegacyPresent) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    // Cube map face surfaces
    m_cubeMapSurfaces.fill(nullptr);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw7Surface: Created a new surface nr. [[7-", m_surfCount, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      m_commonSurf->ListSurfaceDetails();
    }
  }

  DDraw7Surface::~DDraw7Surface() {
    if (m_commonSurf->GetOrigin() == this)
      m_commonSurf->SetOrigin(nullptr);

    // Clear the cached depth stencil on the parent if matched
    if (unlikely(m_parentSurf != nullptr && m_commonSurf->IsDepthStencil()
      && m_parentSurf->GetAttachedDepthStencil() == this)) {
      m_parentSurf->ClearAttachedDepthStencil();
    }

    m_commonIntf->RemoveWrappedSurface(this);

    // Release all public references on all attached surfaces
    for (auto & attachedSurface : m_attachedSurfaces) {
      uint32_t attachedRef;
      do {
        attachedRef = attachedSurface.second->Release();
      } while (attachedRef > 0);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->Release();

    m_commonSurf->SetDD7Surface(nullptr);

    Logger::debug(str::format("DDraw7Surface: Surface nr. [[7-", m_surfCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Black & White queries for IDirect3DTexture2 for whatever reason...
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirect3DTexture2");
      return E_NOINTERFACE;
    }
    // Shouldn't ever be called in practice
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirect3DTexture");
      return E_NOINTERFACE;
    }
    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDraw7Surface::QueryInterface: Query for IDirectDrawColorControl");
      return E_NOINTERFACE;
    }
    // Some games query for legacy ddraw surfaces
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::debug("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, false));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::debug("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::debug("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::debug("DDraw7Surface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Surface::QueryInterface: Query for legacy IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, false));

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

  // On IDirectDrawSurface7, this call will only attach DDSCAPS_ZBUFFER
  // type surfaces and will fail if called with any other surface type.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw7Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(ddraw7Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddraw7Surface->SetParentSurface(this);
    m_depthStencil = ddraw7Surface;

    return hr;
  }

  // Docs: "The IDirectDrawSurface7::AddOverlayDirtyRect method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug(">>> DDraw7Surface::AddOverlayDirtyRect");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw7Surface::Blt: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      DownloadSurfaceData(surface7);
    }
    DownloadSurfaceData(this);

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (unlikely(lpDDSrcSurface == nullptr &&
                    (dwFlags & DDBLT_DEPTHFILL) &&
                    lpDDBltFx != nullptr &&
                    m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9DepthStencil(m_d3d9.ptr()))) {
        Logger::debug("DDraw7Surface::Blt: Clearing d3d9 depth stencil");

        HRESULT hrClear;
        const float zClear = m_commonSurf->GetNormalizedFloatDepth(lpDDBltFx->dwFillDepth);

        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, zClear, 0);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_ZBUFFER, 0, zClear, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw7Surface::Blt: Failed to clear d3d9 depth");
      }
      // Forward DDBLT_COLORFILL clears to D3D9 if done on the current render target
      if (unlikely(lpDDSrcSurface == nullptr &&
                    (dwFlags & DDBLT_COLORFILL) &&
                    lpDDBltFx != nullptr &&
                    m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9RenderTarget(m_d3d9.ptr()))) {
        Logger::debug("DDraw7Surface::Blt: Clearing d3d9 render target");

        HRESULT hrClear;
        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw7Surface::Blt: Failed to clear d3d9 render target");
      }

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_commonIntf->HasDrawn() &&
          m_commonSurf->IsD3D9BackBuffer() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_commonIntf->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_commonIntf->ResetDrawTracking();
        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw7Surface::Blt: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, ddraw7Surface->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }

      if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent)
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return hr;
  }

  // Docs: "The IDirectDrawSurface7::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw7Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw7Surface::BltFast: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      DownloadSurfaceData(surface7);
    }
    DownloadSurfaceData(this);

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Eclusive mode back buffer guard
      if (exclusiveMode && m_commonIntf->HasDrawn() &&
          m_commonSurf->IsD3D9BackBuffer() &&
          m_commonIntf->GetOptions()->backBufferGuard != D3DBackBufferGuard::Disabled) {
        return DD_OK;
      // Windowed mode presentation path
      } else if (!exclusiveMode && m_commonIntf->HasDrawn() && m_commonSurf->IsPrimarySurface()) {
        m_commonIntf->ResetDrawTracking();
        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw7Surface::BltFast: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, ddraw7Surface->GetShadowOrProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::BltFast: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }

      if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent)
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr)) {
        Logger::warn("DDraw7Surface::DeleteAttachedSurface: Received an unwrapped surface");
        return DDERR_UNSUPPORTED;
      }

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
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface7> surface7 = surfaceIt->surface7;

      auto attachedSurfaceIter = m_attachedSurfaces.find(surface7.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface7.ptr() == m_depthStencil->GetProxied())) {
          hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc2, lpContext);
        } else {
          Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface7), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                    std::forward_as_tuple(ddraw7Surface.ref()));
          hr = lpEnumSurfacesCallback(ddraw7Surface.ref(), &surfaceIt->desc2, lpContext);
        }
      } else {
        hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ref(), &surfaceIt->desc2, lpContext);
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

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = commonDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = commonDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      // If the interface is not waiting for VBlank and we stop getting DDFLIP_NOVSYNC
      // flags, reset the swapchain in order to return to VSync presentation using DEFAULT
      } else if (unlikely(!m_commonIntf->GetWaitForVBlank() && IsVSyncFlipFlag(dwFlags))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = commonDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = commonDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }

      m_d3d9Device->Present(NULL, NULL, NULL, NULL);
    // If we don't yet have a device, perhaps a D3D7 application is trying to
    // present exclusively on DDraw (such as a main menu), so allow the flip
    } else {
      Logger::debug("<<< DDraw7Surface::Flip: Proxy");

      // Update the VBlank wait status based on the flip flags
      m_commonIntf->SetWaitForVBlank(IsVSyncFlipFlag(dwFlags));

      if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(surf7->GetProxied(), dwFlags);
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

    try {
      auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
          *lplpDDAttachedSurface = m_depthStencil.ref();
        } else {
          Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw7Surface.ref()));
          *lplpDDAttachedSurface = ddraw7Surface.ref();
        }
      } else {
        *lplpDDAttachedSurface = attachedSurfaceIter->second.ref();
      }
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      *lplpDDAttachedSurface = nullptr;
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  // Blitting can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug(">>> DDraw7Surface::GetBltStatus");

    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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
    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lphDC);

    if (likely(!m_commonIntf->GetOptions()->forceLegacyPresent)) {
      // Foward GetDC calls if we have drawn and the surface is flippable
      RefreshD3D9Device();
      if (m_d3d9Device != nullptr && (m_commonIntf->HasDrawn() &&
                                      m_commonSurf->IsD3D9BackBuffer())) {
        Logger::debug(">>> DDraw7Surface::GetDC");

        if (unlikely(!IsInitialized())) {
          HRESULT hrUpload = InitializeOrUploadD3D9();
          if (unlikely(FAILED(hrUpload)))
            Logger::warn("DDraw7Surface::GetDC: Failed to initialize d3d9 surface");
        }

        HRESULT hr9 = m_d3d9->GetDC(lphDC);
        if (unlikely(FAILED(hr9)))
          Logger::warn("DDraw7Surface::GetDC: Failed D3D9 call");
        return hr9;
      }
    }

    Logger::debug("<<< DDraw7Surface::GetDC: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData(this);

    return GetShadowOrProxied()->GetDC(lphDC);
  }

  // Flipping can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug(">>> DDraw7Surface::GetFlipStatus");

    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData(this);

    return GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ReleaseDC(HDC hDC) {
    if (likely(!m_commonIntf->GetOptions()->forceLegacyPresent)) {
      // Foward ReleaseDC calls if we have drawn and the surface is flippable
      RefreshD3D9Device();
      if (m_d3d9Device != nullptr && (m_commonIntf->HasDrawn() &&
                                      m_commonSurf->IsD3D9BackBuffer())) {
        Logger::debug(">>> DDraw7Surface::ReleaseDC");

        if (unlikely(!IsInitialized())) {
          HRESULT hrUpload = InitializeOrUploadD3D9();
          if (unlikely(FAILED(hrUpload)))
            Logger::warn("DDraw7Surface::ReleaseDC: Failed to initialize d3d9 surface");
        }

        HRESULT hr9 = m_d3d9->ReleaseDC(hDC);
        if (unlikely(FAILED(hr9)))
          Logger::warn("DDraw7Surface::ReleaseDC: Failed D3D9 call");
        return hr9;
      }
    }

    Logger::debug("<<< DDraw7Surface::ReleaseDC: Proxy");

    HRESULT hr = GetShadowOrProxied()->ReleaseDC(hDC);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (m_commonSurf->IsTexture()) {
        m_commonSurf->DirtyMipMaps();
      } else if (unlikely(m_commonIntf->GetOptions()->forceLegacyPresent
                       || m_commonIntf->GetOptions()->apitraceMode)) {
        // We should ideally upload the surface contents here at all times,
        // however some games are amazing, and do hundreds of locks on the same
        // surface per frame, so this would absolutely tank performance
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::ReleaseDC: Failed upload to d3d9 surface");
      }

      if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent)
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
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

    hr = m_commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw7Surface::SetColorKey: Failed to retrieve updated surface desc");

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
      HRESULT hr = GetShadowOrProxied()->SetPalette(lpDDPalette);
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(nullptr);
    } else {
      DDrawPalette* ddrawPalette = static_cast<DDrawPalette*>(lpDDPalette);

      HRESULT hr = GetShadowOrProxied()->SetPalette(ddrawPalette->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(ddrawPalette);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Unlock(LPRECT lpSurfaceData) {
    Logger::debug("<<< DDraw7Surface::Unlock: Proxy");

    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Unlock: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }

      RefreshD3D9Device();
      if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent)
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlay: Proxy");

    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw7Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw7Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "The IDirectDrawSurface7::UpdateOverlayDisplay method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug(">>> DDraw7Surface::UpdateOverlayDisplay");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw7Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
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

    hr = m_commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw4Surface::SetSurfaceDesc: Failed to retrieve updated surface desc");

    // We may need to recreate the d3d9 object based on the new desc
    m_d3d9 = nullptr;

    if (!m_commonSurf->IsTextureOrCubeMap()) {
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

  // Silent Hunter II needs to get these from the proxy surface, as it
  // sets them otherwise... it doesn't call SetPrivateData at all
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    Logger::debug("<<< DDraw7Surface::GetPrivateData: Proxy");
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::FreePrivateData(REFGUID tag) {
    Logger::debug("<<< DDraw7Surface::FreePrivateData: Proxy");
    return m_proxy->FreePrivateData(tag);
  }

  // Docs: "The only defined uniqueness value is 0, indicating that the surface
  // is likely to be changing beyond the control of DirectDraw."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetUniquenessValue(LPDWORD pValue) {
    Logger::debug(">>> DDraw7Surface::GetUniquenessValue");

    if (unlikely(pValue == nullptr))
      return DDERR_INVALIDPARAMS;

    *pValue = 0;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ChangeUniquenessValue() {
    Logger::debug(">>> DDraw7Surface::ChangeUniquenessValue");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPriority(DWORD prio) {
    Logger::debug(">>> DDraw7Surface::SetPriority");

    RefreshD3D9Device();
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

    RefreshD3D9Device();
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

    RefreshD3D9Device();
    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::SetLOD: Not yet initialized");
      return m_proxy->SetLOD(lod);
    }

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    HRESULT hr = m_proxy->SetLOD(lod);
    if (unlikely(FAILED(hr)))
      return hr;

    if (m_texture9 != nullptr) {
      m_texture9->SetLOD(lod);
    } else if (m_cubeMap9 != nullptr) {
      m_cubeMap9->SetLOD(lod);
    } else {
      Logger::warn("DDraw7Surface::SetLOD: Failed to set D3D9 LOD");
      return DDERR_INVALIDOBJECT;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetLOD(LPDWORD lod) {
    Logger::debug(">>> DDraw7Surface::GetLOD");

    RefreshD3D9Device();
    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw7Surface::GetLOD: Not yet initialized");
      return m_proxy->GetLOD(lod);
    }

    if (unlikely(lod == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    if (likely(m_texture9 != nullptr)) {
      *lod = m_texture9->GetLOD();
    } else if (m_cubeMap9 != nullptr) {
      *lod = m_cubeMap9->GetLOD();
    } else {
      Logger::warn("DDraw7Surface::GetLOD: Failed to get D3D9 LOD");
      return DDERR_INVALIDOBJECT;
    }

    return DD_OK;
  }

  IDirectDrawSurface7* DDraw7Surface::GetShadowOrProxied() {
    RefreshD3D9Device();

    if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr))
      return m_shadowSurf->GetProxied();

    return m_proxy.ptr();
  }

  HRESULT DDraw7Surface::InitializeD3D9RenderTarget() {
    HRESULT hr = DD_OK;

    RefreshD3D9Device();

    if (unlikely(!IsInitialized()))
      hr = InitializeD3D9(true);

    m_commonSurf->MarkAsD3D9BackBuffer();

    return hr;
  }

  HRESULT DDraw7Surface::InitializeD3D9DepthStencil() {
    HRESULT hr = DD_OK;

    RefreshD3D9Device();

    if (unlikely(!IsInitialized()))
      hr = InitializeD3D9(false);

    m_commonSurf->MarkAsD3D9DepthStencil();

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
        face7 = new DDraw7Surface(nullptr, std::move(wrappedFace), m_parent, this, false);
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
                                std::forward_as_tuple(face7.ref()));
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
      DDSURFACEDESC2 mipDesc2;
      uint16_t mipCount = 1;

      while (mipMap != nullptr) {
        IDirectDrawSurface7* parentSurface = mipMap;
        mipMap = nullptr;
        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
        if (mipMap != nullptr) {
          mipCount++;

          mipDesc2 = { };
          mipDesc2.dwSize = sizeof(DDSURFACEDESC2);
          mipMap->GetSurfaceDesc(&mipDesc2);
          // Ignore multiple 1x1 mips, which apparently can get generated if the
          // application gets the dwMipMapCount wrong vs surface dimensions.
          if (unlikely(mipDesc2.dwWidth == 1 && mipDesc2.dwHeight == 1))
            break;
        }
      }

      // Do not worry about maximum supported mip map levels validation,
      // because D3D9 will handle this for us and cap them appropriately
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
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_commonIntf->GetCommonD3DDevice()->GetMultiSampleType();
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

      m_commonSurf->MarkAsD3D9BackBuffer();

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

      m_commonSurf->MarkAsD3D9BackBuffer();

    // Cube maps
    } else if (m_commonSurf->IsCubeMap()) {
      Logger::debug("DDraw7Surface::InitializeD3D9: Initializing cube map...");

      Com<d3d9::IDirect3DCubeTexture9> cubetex;

      hr = m_d3d9Device->CreateCubeTexture(
        desc2->dwWidth, m_commonSurf->GetMipCount(), usage,
        format, pool, &cubetex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::InitializeD3D9: Failed to create cube map");
        m_cubeMap9 = nullptr;
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
      m_cubeMap9 = std::move(cubetex);

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
      if (unlikely(m_commonIntf->GetCommonD3DDevice()->IsCurrentRenderTarget(this) || initRT)) {
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

      m_commonSurf->MarkAsD3D9BackBuffer();

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

      m_commonSurf->MarkAsD3D9BackBuffer();

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

    if (m_shadowSurf != nullptr)
      m_shadowSurf->SetD3D9(m_d3d9.ptr());

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDraw7Surface::UploadSurfaceData() {
    Logger::debug(str::format("DDraw7Surface::UploadSurfaceData: Uploading nr. [[7-", m_surfCount, "]]"));

    if (unlikely(m_commonIntf->HasDrawn() && m_commonSurf->IsD3D9BackBuffer())) {
      Logger::debug("DDraw7Surface::UploadSurfaceData: Skipping upload");
      return DD_OK;
    }

    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    // Cube maps will also get marked as textures, so need to be handled first
    if (unlikely(m_commonSurf->IsCubeMap())) {
      // In theory we won't know which faces have been generated,
      // so check them one by one, and upload as needed
      const uint16_t mipCount = m_commonSurf->GetMipCount();
      if (likely(m_cubeMapSurfaces[0] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[0], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive X face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[1] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[1], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative X face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[2] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[2], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive Y face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[3] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[3], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative Y face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[4] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[4], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Positive Z face is null, skpping");
      }
      if (likely(m_cubeMapSurfaces[5] != nullptr)) {
        BlitToD3D9CubeMap(m_cubeMap9.ptr(), format, m_cubeMapSurfaces[5], mipCount);
      } else {
        Logger::debug("DDraw7Surface::UploadSurfaceData: Negative Z face is null, skpping");
      }
    // Blit all the mips for textures
    } else if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface7, DDSURFACEDESC2>(m_texture9.ptr(), format,
                                                             m_proxy.ptr(), m_commonSurf->GetMipCount());
    // Blit surfaces directly
    } else {
      if (unlikely(m_commonSurf->IsDepthStencil())) {
        if (likely(m_commonIntf->GetOptions()->uploadDepthStencils)) {
          Logger::debug("DDraw7Surface::UploadSurfaceData: Uploading depth stencil");
        } else {
          Logger::debug("DDraw7Surface::UploadSurfaceData: Skipping upload of depth stencil");
          return DD_OK;
        }
      }

      BlitToD3D9Surface<IDirectDrawSurface7, DDSURFACEDESC2>(m_d3d9.ptr(), format, GetShadowOrProxied());
    }

    return DD_OK;
  }

  inline void DDraw7Surface::DownloadSurfaceData(DDraw7Surface* surface) {
    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (unlikely(surface->GetCommonSurface()->IsD3D9BackBuffer())) {
      if (d3dOptions->backBufferWriteBack || d3dOptions->forceLegacyPresent || d3dOptions->apitraceMode) {
        Logger::debug("DDraw7Surface::DownloadSurfaceData: Surface is a bound swapchain surface");

        if (unlikely(d3dOptions->apitraceMode && !surface->IsInitialized()))
          surface->InitializeOrUploadD3D9();

        if (surface->IsInitialized() && surface->GetCommonSurface()->IsD3D9SurfaceDirty()) {
          BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(surface->GetShadowOrProxied(), surface->GetD3D9());
          surface->GetCommonSurface()->UnDirtyD3D9Surface();
        }
      } else {
        static bool s_swapchainWarningShown;

        if (!std::exchange(s_swapchainWarningShown, true))
          Logger::warn("DDraw7Surface::DownloadSurfaceData: Surface is a swapchain surface");
      }
    } else if (unlikely(surface->GetCommonSurface()->IsD3D9DepthStencil())) {
      if (d3dOptions->depthWriteBack || d3dOptions->apitraceMode) {
        Logger::debug("DDraw7Surface::DownloadSurfaceData: Surface is a bound depth stencil");

        if (surface->IsInitialized() && surface->GetCommonSurface()->IsD3D9SurfaceDirty()) {
          BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(surface->GetProxied(), surface->GetD3D9());
          surface->GetCommonSurface()->UnDirtyD3D9Surface();
        }
      } else {
        static bool s_depthStencilWarningShown;

        if (!std::exchange(s_depthStencilWarningShown, true))
          Logger::warn("DDraw7Surface::DownloadSurfaceData: Surface is a depth stencil");
      }
    }
  }

  inline void DDraw7Surface::RefreshD3D9Device() {
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

    d3d9::IDirect3DDevice9* d3d9Device = commonDevice != nullptr ? commonDevice->GetD3D9Device() : nullptr;
    if (unlikely(m_d3d9Device != d3d9Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (m_d3d9Device != nullptr) {
        Logger::debug("DDraw7Surface: Device context has changed, clearing all D3D9 resources");
        m_cubeMap9 = nullptr;
        m_texture9 = nullptr;
        m_d3d9 = nullptr;
      }

      m_d3d9Device = d3d9Device;
    }
  }

}

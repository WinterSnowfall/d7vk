#include "ddraw4_surface.h"

#include "../d3d_common_device.h"

#include "ddraw4_interface.h"

#include "../ddraw_gamma.h"
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
        bool isChildObject)
    : DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4>(pParent, std::move(surfProxy))
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
      throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve the common interface!");
    }

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

    // Retrieve and cache the next surface in a flippable chain
    if (unlikely(m_commonSurf->IsFlippable() && !m_commonSurf->IsBackBuffer())) {
      IDirectDrawSurface4* nextFlippable = nullptr;
      EnumAttachedSurfaces(&nextFlippable, ListBackBufferSurfaces4Callback);
      m_nextFlippable = reinterpret_cast<DDraw4Surface*>(nextFlippable);
      if (likely(m_nextFlippable != nullptr))
        Logger::debug("DDraw4Surface: Retrieved the next swapchain surface");
    }

    m_commonIntf->AddWrappedSurface(this);

    m_commonSurf->SetDD4Surface(this);

    if (m_parentSurf != nullptr
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()
     && !m_commonIntf->GetOptions()->forceSingleBackBuffer
     && !m_commonIntf->GetOptions()->forceLegacyPresent) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", m_surfCount, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      m_commonSurf->ListSurfaceDetails();
    }
  }

  DDraw4Surface::~DDraw4Surface() {
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

    m_commonSurf->SetDD4Surface(nullptr);

    Logger::debug(str::format("DDraw4Surface: Surface nr. [[4-", m_surfCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a texture for d3d6 SetTexture calls
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_texture6 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_commonIntf->GetNextTextureHandle();
        m_texture6 = new D3D6Texture(std::move(ppvProxyObject), this, nextHandle);
        // D3D6Textures don't need handle lookups so skip storing them
      }

      *ppvObject = m_texture6.ref();

      return S_OK;
    }
    // Shouldn't ever be called in practice
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::warn("DDraw4Surface::QueryInterface: Query for IDirect3DTexture");
      return E_NOINTERFACE;
    }
    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawColorControl");
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");

      Com<IDirectDrawSurface7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD7Interface(), nullptr, false));

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
      Logger::warn("DDraw4Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    // Unsupported by Wine's DDraw implementation, so we'll do our own present
    if (unlikely(ddraw4Surface->GetCommonSurface()->IsBackBufferOrFlippable())) {
      Logger::debug("ddraw4Surface::AddAttachedSurface: Caching surface as DDraw RT");
      m_commonIntf->SetDDrawRenderTarget(ddraw4Surface->GetCommonSurface());
      return DD_OK;
    }

    HRESULT hr = m_proxy->AddAttachedSurface(ddraw4Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddraw4Surface->SetParentSurface(this);
    if (likely(ddraw4Surface->GetCommonSurface()->IsDepthStencil()))
      m_depthStencil = ddraw4Surface;

    return hr;
  }

  // Docs: "The IDirectDrawSurface4::AddOverlayDirtyRect method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug(">>> DDraw4Surface::AddOverlayDirtyRect");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw4Surface::Blt: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw4Surface* surface4 = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      surface4->DownloadSurfaceData();
    }
    DownloadSurfaceData();

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();
    if (likely(d3d9Device != nullptr)) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_DEPTHFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonSurf->GetCommonD3DDevice()->IsCurrentD3D9DepthStencil(m_commonSurf->GetD3D9Surface()))) {
        Logger::debug("DDraw4Surface::Blt: Clearing d3d9 depth stencil");

        HRESULT hrClear;
        const float zClear = m_commonSurf->GetNormalizedFloatDepth(lpDDBltFx->dwFillDepth);

        if (lpDestRect == nullptr) {
          hrClear = d3d9Device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, zClear, 0);
        } else {
          hrClear = d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_ZBUFFER, 0, zClear, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw4Surface::Blt: Failed to clear d3d9 depth");
      }
      // Forward DDBLT_COLORFILL clears to D3D9 if done on the current render target
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_COLORFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonSurf->GetCommonD3DDevice()->IsCurrentD3D9RenderTarget(m_commonSurf->GetD3D9Surface()))) {
        Logger::debug("DDraw4Surface::Blt: Clearing d3d9 render target");

        HRESULT hrClear;
        if (lpDestRect == nullptr) {
          hrClear = d3d9Device->Clear(0, NULL, D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        } else {
          hrClear = d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw4Surface::Blt: Failed to clear d3d9 render target");
      }

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
        DDraw4Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget4();

        if (ddraw4Surface == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw4Surface::Blt: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, ddraw4Surface->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  // Docs: "The IDirectDrawSurface4::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw4Surface::BltFast: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw4Surface* surface4 = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      surface4->DownloadSurfaceData();
    }
    DownloadSurfaceData();

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();
    if (likely(d3d9Device != nullptr)) {
      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
        DDraw4Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget4();

        if (ddraw4Surface == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw4Surface::BltFast: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, ddraw4Surface->GetShadowOrProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw4Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw4Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr)) {
        Logger::warn("DDraw4Surface::DeleteAttachedSurface: Received an unwrapped surface");
        return DDERR_UNSUPPORTED;
      }

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    if (unlikely(ddraw4Surface->GetCommonSurface()->IsBackBufferOrFlippable() &&
                 ddraw4Surface->GetCommonSurface() == m_commonIntf->GetDDrawRenderTarget())) {
      Logger::debug("DDraw4Surface::DeleteAttachedSurface: Removing cached DDraw RT surface");
      m_commonIntf->SetDDrawRenderTarget(nullptr);
      return DD_OK;
    }

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
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface4> surface4 = surfaceIt->surface4;

      auto attachedSurfaceIter = m_attachedSurfaces.find(surface4.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface4.ptr() == m_depthStencil->GetProxied())) {
          hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc2, lpContext);
        } else {
          Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface4), m_commonIntf->GetDD4Interface(), this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                    std::forward_as_tuple(ddraw4Surface.ref()));
          hr = lpEnumSurfacesCallback(ddraw4Surface.ref(), &surfaceIt->desc2, lpContext);
        }
      } else {
        hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ref(), &surfaceIt->desc2, lpContext);
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

    Com<DDraw4Surface> surf4;
    if (m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))
      surf4 = static_cast<DDraw4Surface*>(lpDDSurfaceTargetOverride);

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();
    if (likely(d3d9Device != nullptr)) {
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

      if (unlikely(surf4 != nullptr && !surf4->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw4Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw4Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      // If the interface is not waiting for VBlank and we stop getting DDFLIP_NOVSYNC
      // flags, reset the swapchain in order to return to VSync presentation using DEFAULT
      } else if (unlikely(!m_commonIntf->GetWaitForVBlank() && IsVSyncFlipFlag(dwFlags))) {
        Logger::info("DDraw4Surface::Flip: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }

      DDraw4Surface* rt = m_commonIntf->GetDDrawRenderTarget() != nullptr ?
                          m_commonIntf->GetDDrawRenderTarget()->GetDD4Surface() : nullptr;

      if (unlikely(rt != nullptr && m_commonSurf->IsPrimarySurface())) {
        Logger::debug("DDraw4Surface::Flip: Presenting from DDraw RT");
        rt->InitializeOrUploadD3D9();
        d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }

      if (likely(m_nextFlippable != nullptr)) {
        m_nextFlippable->InitializeOrUploadD3D9();
      } else {
        InitializeOrUploadD3D9();
      }

      d3d9Device->Present(NULL, NULL, NULL, NULL);

    } else {
      Logger::debug("<<< DDraw4Surface::Flip: Proxy");

      // Update the VBlank wait status based on the flip flags
      m_commonIntf->SetWaitForVBlank(IsVSyncFlipFlag(dwFlags));

      if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(surf4->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw4Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for the primary surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for a flippable surface");
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

    try {
      auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
          *lplpDDAttachedSurface = m_depthStencil.ref();
        } else {
          Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface), m_commonIntf->GetDD4Interface(), this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                      std::forward_as_tuple(ddraw4Surface.ref()));
          *lplpDDAttachedSurface = ddraw4Surface.ref();
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
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::GetBltStatus");

    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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
    Logger::debug("<<< DDraw4Surface::GetDC: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->GetDC(lphDC);
  }

  // Flipping can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::GetFlipStatus");

    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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
    Logger::debug("<<< DDraw4Surface::Lock: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ReleaseDC(HDC hDC) {
    Logger::debug("<<< DDraw4Surface::ReleaseDC: Proxy");

    HRESULT hr = GetShadowOrProxied()->ReleaseDC(hDC);

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();
      if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent)
          d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
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

      // Retrieve a hWnd, if needed, during clipper attachment
      HWND hWnd = nullptr;
      hr = ddrawClipper->GetProxied()->GetHWnd(&hWnd);
      if (unlikely(FAILED(hr))) {
        Logger::debug("DDraw4Surface::SetClipper: Failed to retrieve hWnd");
      } else {
        m_commonIntf->SetHWND(hWnd);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw4Surface::SetColorKey: Proxy");

    // The Combat Mission series of games set a color key which is
    // outside the color range of the surface they are setting it on...
    // clamp it to the surface color depth in that case. This doesn't
    // appear to work well universally, however, so only apply when needed.
    if (unlikely(m_commonIntf->GetOptions()->colorKeyMasking && lpDDColorKey != nullptr)) {
      const uint8_t colorBitCount = m_commonSurf->GetColorBitCount();
      lpDDColorKey->dwColorSpaceLowValue  &= (1 << colorBitCount) - 1;
      lpDDColorKey->dwColorSpaceHighValue &= (1 << colorBitCount) - 1;
    }

    HRESULT hr = m_proxy->SetColorKey(dwFlags, lpDDColorKey);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = m_commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw4Surface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDraw4Surface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton();
        if (unlikely(FAILED(hr)))
          Logger::warn("DDraw4Surface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw4Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw4Surface::SetPalette: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Unlock(LPRECT lpSurfaceData) {
    Logger::debug("<<< DDraw4Surface::Unlock: Proxy");

    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();
      if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlay: Proxy");

    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw4Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw4Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "The IDirectDrawSurface4::UpdateOverlayDisplay method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::UpdateOverlayDisplay");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw4Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
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

    hr = m_commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw4Surface::SetSurfaceDesc: Failed to retrieve updated surface desc");

    // We may need to recreate the d3d9 object based on the new desc
    m_commonSurf->SetD3D9Surface(nullptr);

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

  // Docs: "The only defined uniqueness value is 0, indicating that the surface
  // is likely to be changing beyond the control of DirectDraw."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetUniquenessValue(LPDWORD pValue) {
    Logger::debug(">>> DDraw4Surface::GetUniquenessValue");

    if (unlikely(pValue == nullptr))
      return DDERR_INVALIDPARAMS;

    *pValue = 0;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ChangeUniquenessValue() {
    Logger::debug(">>> DDraw4Surface::ChangeUniquenessValue");
    return DD_OK;
  }

  IDirectDrawSurface4* DDraw4Surface::GetShadowOrProxied() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();

    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr))
      return m_shadowSurf->GetProxied();

    return m_proxy.ptr();
  }

  HRESULT DDraw4Surface::InitializeD3D9RenderTarget() {
    m_commonSurf->RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9BackBuffer();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      if (m_commonSurf->IsTexture())
        UpdateMipMapCount();

      HRESULT hr = m_commonSurf->InitializeD3D9(true);
      if (unlikely(FAILED(hr)))
        return hr;

      Logger::debug(str::format("DDraw4Surface::InitializeD3D9RenderTarget: Initialized surface nr. [[4-", m_surfCount, "]]"));

      return UploadSurfaceData();
    }

    return D3D_OK;
  }

  HRESULT DDraw4Surface::InitializeD3D9DepthStencil() {
    m_commonSurf->RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9DepthStencil();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(false);
      if (unlikely(FAILED(hr)))
        return hr;

      Logger::debug(str::format("DDraw4Surface::InitializeD3D9DepthStencil: Initialized surface nr. [[4-", m_surfCount, "]]"));

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDraw4Surface::InitializeOrUploadD3D9() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->RefreshD3D9Device();

    // Fast skip
    if (unlikely(d3d9Device == nullptr)) {
      Logger::debug("DDraw4Surface::InitializeOrUploadD3D9: Null device, can't initialize right now");
      return D3D_OK;
    }

    if (unlikely(!m_commonSurf->IsInitialized())) {
      if (m_commonSurf->IsTexture())
        UpdateMipMapCount();

      const bool initRenderTarget = m_commonSurf->GetCommonD3DDevice()->IsCurrentRenderTarget(m_commonSurf.ptr());

      HRESULT hr = m_commonSurf->InitializeD3D9(initRenderTarget);
      if (unlikely(FAILED(hr)))
        return hr;

      Logger::debug(str::format("DDraw4Surface::InitializeOrUploadD3D9: Initialized surface nr. [[4-", m_surfCount, "]]"));
    }

    if (likely(m_commonSurf->IsInitialized()))
      return UploadSurfaceData();

    return DD_OK;
  }

  void DDraw4Surface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      m_commonSurf->RefreshD3D9Device();

    if (unlikely(m_commonSurf->IsD3D9BackBuffer())) {
      Logger::debug("DDraw4Surface::DownloadSurfaceData: Surface is a bound swapchain surface");

      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        Logger::debug(str::format("DDraw4Surface::DownloadSurfaceData: Downloading nr. [[4-", m_surfCount, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface4, DDSURFACEDESC2>(GetShadowOrProxied(), m_commonSurf->GetD3D9Surface(),
                                                                m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    } else if (unlikely(m_commonSurf->IsD3D9DepthStencil())) {
      Logger::debug("DDraw4Surface::DownloadSurfaceData: Surface is a bound depth stencil");

      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        Logger::debug(str::format("DDraw4Surface::DownloadSurfaceData: Downloading nr. [[4-", m_surfCount, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface4, DDSURFACEDESC2>(m_proxy.ptr(), m_commonSurf->GetD3D9Surface(),
                                                                m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    }
  }

  inline void DDraw4Surface::UpdateMipMapCount() {
    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.
    const DDSURFACEDESC2* desc2  = m_commonSurf->GetDesc2();

    IDirectDrawSurface4* mipMap = m_proxy.ptr();
    DDSURFACEDESC2 mipDesc2;
    uint16_t mipCount = 1;

    while (mipMap != nullptr) {
      IDirectDrawSurface4* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces4Callback);
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
      Logger::debug(str::format("DDraw4Surface::UpdateMipMapCount: Found ", mipCount, " mip levels"));

      if (unlikely(mipCount != desc2->dwMipMapCount))
        Logger::debug(str::format("DDraw4Surface::UpdateMipMapCount: Mismatch with declared ", desc2->dwMipMapCount, " mip levels"));
    }

    if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
      Logger::debug("DDraw4Surface::UpdateMipMapCount: Using auto mip map generation");
      mipCount = 0;
    }

    m_commonSurf->SetMipCount(mipCount);
  }

  inline HRESULT DDraw4Surface::UploadSurfaceData() {
    // Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    Logger::debug(str::format("DDraw4Surface::UploadSurfaceData: Uploading nr. [[4-", m_surfCount, "]]"));

    if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface4, DDSURFACEDESC2>(m_commonSurf->GetD3D9Texture(), m_proxy.ptr(),
                                                             m_commonSurf->GetMipCount(), m_commonSurf->IsDXTFormat());
    // Blit surfaces directly
    } else {
      if (unlikely(m_commonSurf->IsDepthStencil()))
        Logger::debug("DDrawSurface::UploadSurfaceData: Uploading depth stencil");

      BlitToD3D9Surface<IDirectDrawSurface4, DDSURFACEDESC2>(m_commonSurf->GetD3D9Surface(), GetShadowOrProxied(),
                                                             m_commonSurf->IsDXTFormat());
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

}

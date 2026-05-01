#include "ddraw_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"

#include "../d3d3/d3d3_device.h"
#include "../d3d3/d3d3_interface.h"
#include "../d3d3/d3d3_texture.h"

#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDrawSurface::s_surfCount = 0;

  DDrawSurface::DDrawSurface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface>&& surfProxy,
        DDrawInterface* pParent,
        DDrawSurface* pParentSurf,
        bool isChildObject)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawSurface, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
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
      throw DxvkError("DDrawSurface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (!m_commonSurf->IsDescSet()) {
      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDrawSurface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc(desc);
      }
    }

    // Retrieve and cache the next surface in a flippable chain
    if (unlikely(m_commonSurf->IsFlippable() && !m_commonSurf->IsBackBuffer())) {
      IDirectDrawSurface* nextFlippable = nullptr;
      EnumAttachedSurfaces(&nextFlippable, ListBackBufferSurfacesCallback);
      m_nextFlippable = reinterpret_cast<DDrawSurface*>(nextFlippable);
      if (likely(m_nextFlippable != nullptr))
        Logger::debug("DDraw4Surface: Retrieved the next swapchain surface");
    }

    m_commonIntf->AddWrappedSurface(this);

    m_commonSurf->SetDDSurface(this);

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

    Logger::debug(str::format("DDrawSurface: Created a new surface nr. [[1-", m_surfCount, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      m_commonSurf->ListSurfaceDetails();
    }
  }

  DDrawSurface::~DDrawSurface() {
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

    m_commonSurf->SetDDSurface(nullptr);

    Logger::debug(str::format("DDrawSurface: Surface nr. [[1-", m_surfCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawSurface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture");

      if (unlikely(m_texture3 == nullptr)) {
        Com<IDirect3DTexture> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_commonIntf->GetNextTextureHandle();
        m_texture3 = new D3D3Texture(std::move(ppvProxyObject), this, nextHandle);
        D3DCommonTexture* commonTex = m_texture3->GetCommonTexture();
        m_commonIntf->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = m_texture3.ref();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDrawSurface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_texture5 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_commonIntf->GetNextTextureHandle();
        m_texture5 = new D3D5Texture(std::move(ppvProxyObject), this, nextHandle);
        D3DCommonTexture* commonTex = m_texture5->GetCommonTexture();
        m_commonIntf->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = m_texture5.ref();

      return S_OK;
    }
    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawColorControl");
      return E_NOINTERFACE;
    }
    // The standard way of creating a new D3D3 device. Outside of RAMP, MMX, RGB and HAL,
    // some applications (e.g. Dark Rift) query for Wine's advertised custom device IID.
    if (riid == IID_IDirect3DHALDevice  || riid == IID_IDirect3DRGBDevice  ||
        riid == IID_IDirect3DMMXDevice  || riid == IID_IDirect3DRampDevice ||
        riid == IID_WineD3DDevice) {
      Logger::debug("DDrawSurface::QueryInterface: Query with an IDirect3DDevice IID");

      HRESULT hr = CreateDeviceInternal(riid, ppvObject);
      if (unlikely(FAILED(hr)))
        return E_NOINTERFACE;

      return S_OK;
    }
    // Some applications check the supported API level by querying the various newer surface GUIDs...
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr) {
        Logger::debug("DDrawSurface::QueryInterface: Query for existing IDirectDrawSurface2");
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface2");

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), this, nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::debug("DDrawSurface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), this, nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::debug("DDrawSurface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::debug("DDrawSurface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawSurface7");

      Com<IDirectDrawSurface7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD7Interface(), nullptr, false));

      return S_OK;
    }
    // Some games are known to query the clipper from the surface,
    // though that won't work and GetClipper exists anyway...
    if (unlikely(riid == __uuidof(IDirectDrawClipper))) {
      Logger::debug("DDrawSurface::QueryInterface: Query for IDirectDrawClipper");
      return E_NOINTERFACE;
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
    Logger::debug("<<< DDrawSurface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDrawSurface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

    // Unsupported by Wine's DDraw implementation, so we'll do our own present
    if (unlikely(ddrawSurface->GetCommonSurface()->IsBackBufferOrFlippable())) {
      Logger::debug("DDrawSurface::AddAttachedSurface: Caching surface as DDraw RT");
      m_commonIntf->SetDDrawRenderTarget(ddrawSurface->GetCommonSurface());
      return DD_OK;
    }

    HRESULT hr = m_proxy->AddAttachedSurface(ddrawSurface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddrawSurface->SetParentSurface(this);
    if (likely(ddrawSurface->GetCommonSurface()->IsDepthStencil()))
      m_depthStencil = ddrawSurface;

    return hr;
  }

  // Docs: "This method is used for the software implementation.
  // It is not needed if the overlay support is provided by the hardware."
  HRESULT STDMETHODCALLTYPE DDrawSurface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug(">>> DDrawSurface::AddOverlayDirtyRect");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDrawSurface::Blt: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDrawSurface* surface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      surface->DownloadSurfaceData();
    }
    DownloadSurfaceData();

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_DEPTHFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9DepthStencil(m_d3d9.ptr()))) {
        Logger::debug("DDrawSurface::Blt: Clearing d3d9 depth stencil");

        HRESULT hrClear;
        const float zClear = m_commonSurf->GetNormalizedFloatDepth(lpDDBltFx->dwFillDepth);

        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, zClear, 0);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_ZBUFFER, 0, zClear, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDrawSurface::Blt: Failed to clear d3d9 depth");
      }
      // Forward DDBLT_COLORFILL clears to D3D9 if done on the current render target
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_COLORFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9RenderTarget(m_d3d9.ptr()))) {
        Logger::debug("DDrawSurface::Blt: Clearing d3d9 render target");

        HRESULT hrClear;
        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDrawSurface::Blt: Failed to clear d3d9 render target");
      }

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

        DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
        DDrawSurface* renderTarget = commonDevice->GetCurrentRenderTarget();

        if (ddrawSurface == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDrawSurface::Blt: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, ddrawSurface->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      // Depth stencil uploads need to happen on the spot
      if (unlikely(m_commonSurf->IsDepthStencil())) {
        InitializeOrUploadD3D9();
      } else if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  // Docs: "The IDirectDrawSurface::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDrawSurface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug("<<< DDrawSurface::BltBatch: Proxy");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDrawSurface::BltFast: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDrawSurface* surface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      surface->DownloadSurfaceData();
    }
    DownloadSurfaceData();

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

        DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
        DDrawSurface* renderTarget = commonDevice->GetCurrentRenderTarget();

        if (ddrawSurface == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }

        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDrawSurface::BltFast: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, ddrawSurface->GetShadowOrProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      // Depth stencil uploads need to happen on the spot
      if (unlikely(m_commonSurf->IsDepthStencil())) {
        InitializeOrUploadD3D9();
      } else if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    Logger::debug("<<< DDrawSurface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr)) {
        Logger::warn("DDrawSurface::DeleteAttachedSurface: Received an unwrapped surface");
        return DDERR_UNSUPPORTED;
      }

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

    if (unlikely(ddrawSurface->GetCommonSurface()->IsBackBufferOrFlippable() &&
                 ddrawSurface->GetCommonSurface() == m_commonIntf->GetDDrawRenderTarget())) {
      Logger::debug("DDrawSurface::DeleteAttachedSurface: Removing cached DDraw RT surface");
      m_commonIntf->SetDDrawRenderTarget(nullptr);
      return DD_OK;
    }

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
    Logger::debug(">>> DDrawSurface::EnumAttachedSurfaces");

    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfacesCallback);

    HRESULT hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface> surface = surfaceIt->surface;

      auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
          hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc, lpContext);
        } else {
          Com<DDrawSurface> ddrawSurface = new DDrawSurface(nullptr, std::move(surface), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddrawSurface->GetProxied()),
                                     std::forward_as_tuple(ddrawSurface.ref()));
          hr = lpEnumSurfacesCallback(ddrawSurface.ref(), &surfaceIt->desc, lpContext);
        }
      } else {
        hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ref(), &surfaceIt->desc, lpContext);
      }

      ++surfaceIt;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDrawSurface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) {

    Com<DDrawSurface> surf;
    if (m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))
      surf = static_cast<DDrawSurface*>(lpDDSurfaceTargetOverride);

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      Logger::debug("*** DDrawSurface::Flip: Presenting");

      // Lost surfaces are not flippable
      HRESULT hr = m_proxy->IsLost();
      if (unlikely(FAILED(hr))) {
        Logger::debug("DDrawSurface::Flip: Lost surface");
        return hr;
      }

      if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable()))) {
        Logger::debug("DDrawSurface::Flip: Unflippable surface");
        return DDERR_NOTFLIPPABLE;
      }

      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Non-exclusive mode validations
      if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode)) {
        Logger::debug("DDrawSurface::Flip: Primary surface flip in non-exclusive mode");
        return DDERR_NOEXCLUSIVEMODE;
      }

      // Exclusive mode validations
      if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode)) {
        Logger::debug("DDrawSurface::Flip: Back buffer flip in exclusive mode");
        return DDERR_NOTFLIPPABLE;
      }

      if (unlikely(surf != nullptr && !surf->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDrawSurface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }

      DDrawSurface* rt = m_commonIntf->GetDDrawRenderTarget() != nullptr ?
                         m_commonIntf->GetDDrawRenderTarget()->GetDDSurface() : nullptr;

      if (unlikely(rt != nullptr && m_commonSurf->IsPrimarySurface())) {
        Logger::debug("DDrawSurface::Flip: Presenting from DDraw RT");
        rt->InitializeOrUploadD3D9();
        m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        return DD_OK;
      }

      if (likely(m_nextFlippable != nullptr)) {
        m_nextFlippable->InitializeOrUploadD3D9();
      } else {
        InitializeOrUploadD3D9();
      }

      m_d3d9Device->Present(NULL, NULL, NULL, NULL);
    // If we don't have a valid D3D5 device, this means a D3D3 application
    // is trying to flip the surface. Allow that for compatibility reasons.
    } else {
      Logger::debug("<<< DDrawSurface::Flip: Proxy");
      if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSurfaceTargetOverride))) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(surf->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE *lplpDDAttachedSurface) {
    Logger::debug("<<< DDrawSurface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for the primary surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
      Logger::debug("DDrawSurface::GetAttachedSurface: Querying for a flippable surface");
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

    try {
      auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
          *lplpDDAttachedSurface = m_depthStencil.ref();
        } else {
          Com<DDrawSurface> ddrawSurface = new DDrawSurface(nullptr, std::move(surface), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(ddrawSurface->GetProxied()),
                                      std::forward_as_tuple(ddrawSurface.ref()));
          *lplpDDAttachedSurface = ddrawSurface.ref();
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
  HRESULT STDMETHODCALLTYPE DDrawSurface::GetBltStatus(DWORD dwFlags) {
    Logger::debug(">>> DDrawSurface::GetBltStatus");

    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug(">>> DDrawSurface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug(">>> DDrawSurface::GetClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDrawSurface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetDC(HDC *lphDC) {
    Logger::debug("<<< DDrawSurface::GetDC: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->GetDC(lphDC);
  }

  // Flipping can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDrawSurface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug(">>> DDrawSurface::GetFlipStatus");

    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug(">>> DDrawSurface::GetPixelFormat");

    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDrawSurface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDrawSurface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDrawSurface::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::IsLost() {
    Logger::debug("<<< DDrawSurface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDrawSurface::Lock: Proxy");

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::ReleaseDC(HDC hDC) {
    Logger::debug("<<< DDrawSurface::ReleaseDC: Proxy");

    HRESULT hr = GetShadowOrProxied()->ReleaseDC(hDC);

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      // Depth stencil uploads need to happen on the spot
      if (unlikely(m_commonSurf->IsDepthStencil())) {
        InitializeOrUploadD3D9();
      } else if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Restore() {
    Logger::debug("<<< DDrawSurface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDrawSurface::SetClipper: Proxy");

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
        Logger::debug("DDrawSurface::SetClipper: Failed to retrieve hWnd");
      } else {
        m_commonIntf->SetHWND(hWnd);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDrawSurface::SetColorKey: Proxy");

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
      Logger::err("DDrawSurface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDrawSurface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton();
        if (unlikely(FAILED(hr)))
          Logger::warn("DDrawSurface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDrawSurface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDrawSurface::SetPalette: Proxy");

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

  HRESULT STDMETHODCALLTYPE DDrawSurface::Unlock(LPVOID lpSurfaceData) {
    Logger::debug("<<< DDrawSurface::Unlock: Proxy");

    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      m_commonSurf->DirtyDDrawSurface();

      RefreshD3D9Device();
      // Depth stencil uploads need to happen on the spot
      if (unlikely(m_commonSurf->IsDepthStencil())) {
        InitializeOrUploadD3D9();
      } else if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonIntf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          m_d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDrawSurface::UpdateOverlay: Proxy");

    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDrawSurface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddrawSurface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "This method is for software emulation only; it does nothing if the hardware supports overlays."
  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug(">>> DDrawSurface::UpdateOverlayDisplay");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference) {
    Logger::debug("<<< DDrawSurface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDrawSurface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddrawSurface->GetProxied());
  }

  IDirectDrawSurface* DDrawSurface::GetShadowOrProxied() {
    RefreshD3D9Device();

    if (unlikely(m_shadowSurf != nullptr && m_d3d9Device != nullptr))
      return m_shadowSurf->GetProxied();

    return m_proxy.ptr();
  }

  HRESULT DDrawSurface::InitializeD3D9RenderTarget() {
    RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9BackBuffer();

    if (unlikely(!IsInitialized()))
      return InitializeD3D9(true);

    return DD_OK;
  }

  HRESULT DDrawSurface::InitializeD3D9DepthStencil() {
    RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9DepthStencil();

    if (unlikely(!IsInitialized()))
      return InitializeD3D9(false);

    return DD_OK;
  }

  HRESULT DDrawSurface::InitializeOrUploadD3D9() {
    RefreshD3D9Device();

    if (unlikely(!IsInitialized()))
      return InitializeD3D9(false);

    return UploadSurfaceData();
  }

  void DDrawSurface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      RefreshD3D9Device();

    if (unlikely(m_commonSurf->IsD3D9BackBuffer())) {
      Logger::debug("DDrawSurface::DownloadSurfaceData: Surface is a bound swapchain surface");

      if (IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        Logger::debug(str::format("DDrawSurface::DownloadSurfaceData: Downloading nr. [[1-", m_surfCount, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(GetShadowOrProxied(), m_d3d9.ptr());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    } else if (unlikely(m_commonSurf->IsD3D9DepthStencil())) {
      Logger::debug("DDrawSurface::DownloadSurfaceData: Surface is a bound depth stencil");

      if (IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        Logger::debug(str::format("DDrawSurface::DownloadSurfaceData: Downloading nr. [[1-", m_surfCount, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_proxy.ptr(), m_d3d9.ptr());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    }
  }

  inline HRESULT DDrawSurface::InitializeD3D9(const bool initRT) {
    if (unlikely(m_d3d9Device == nullptr)) {
      Logger::debug("DDrawSurface::InitializeD3D9: Null device, can't initialize right now");
      return DD_OK;
    }

    Logger::debug(str::format("DDrawSurface::InitializeD3D9: Initializing nr. [[1-", m_surfCount, "]]"));

    const DDSURFACEDESC* desc    = m_commonSurf->GetDesc();
    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    if (unlikely(desc->dwHeight == 0 || desc->dwWidth == 0)) {
      Logger::err("DDrawSurface::InitializeD3D9: Surface has 0 height or width");
      return DDERR_GENERIC;
    }

    if (unlikely(format == d3d9::D3DFMT_UNKNOWN)) {
      Logger::err("DDrawSurface::InitializeD3D9: Surface has an unknown format");
      return DDERR_GENERIC;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(format == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDrawSurface::InitializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;

    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(format == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDrawSurface::InitializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.
    if (m_commonSurf->IsTexture()) {
      IDirectDrawSurface* mipMap = m_proxy.ptr();
      DDSURFACEDESC mipDesc;
      uint16_t mipCount = 1;

      while (mipMap != nullptr) {
        IDirectDrawSurface* parentSurface = mipMap;
        mipMap = nullptr;
        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
        if (mipMap != nullptr) {
          mipCount++;

          mipDesc = { };
          mipDesc.dwSize = sizeof(DDSURFACEDESC2);
          mipMap->GetSurfaceDesc(&mipDesc);
          // Ignore multiple 1x1 mips, which apparently can get generated if the
          // application gets the dwMipMapCount wrong vs surface dimensions.
          if (unlikely(mipDesc.dwWidth == 1 && mipDesc.dwHeight == 1))
            break;
        }
      }

      // Do not worry about maximum supported mip map levels validation,
      // because D3D9 will handle this for us and cap them appropriately
      if (mipCount > 1) {
        Logger::debug(str::format("DDrawSurface::InitializeD3D9: Found ", mipCount, " mip levels"));

        if (unlikely(mipCount != desc->dwMipMapCount))
          Logger::debug(str::format("DDrawSurface::InitializeD3D9: Mismatch with declared ", desc->dwMipMapCount, " mip levels"));
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDrawSurface::InitializeD3D9: Using auto mip map generation");
        mipCount = 0;
      }

      m_commonSurf->SetMipCount(mipCount);
    }

    d3d9::D3DPOOL pool  = d3d9::D3DPOOL_DEFAULT;
    DWORD         usage = 0;

    // General surface/texture pool placement
    if (desc->ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)
      pool = d3d9::D3DPOOL_DEFAULT;
    // There's no explicit non-local video memory placement
    // per se, but D3DPOOL_MANAGED is close enough
    else if (desc->ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM)
      pool = d3d9::D3DPOOL_MANAGED;
    else if (desc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
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
      Logger::debug("DDrawSurface::InitializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (m_commonSurf->IsTexture()) {
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        Logger::debug("DDrawSurface::InitializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        Logger::debug("DDrawSurface::InitializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDrawSurface::InitializeD3D9: Placing in: ", poolPlacement));

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_commonIntf->GetCommonD3DDevice()->GetMultiSampleType();
    const uint32_t index = m_commonSurf->GetBackBufferIndex();

    Com<d3d9::IDirect3DSurface9> surf;

    HRESULT hr = DDERR_GENERIC;

    // Front Buffer
    if (m_commonSurf->IsFrontBuffer()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing front buffer...");

      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to retrieve front buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

      m_commonSurf->MarkAsD3D9BackBuffer();

    // Back Buffer
    } else if (m_commonSurf->IsBackBufferOrFlippable()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing back buffer...");

      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to retrieve back buffer");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

      m_commonSurf->MarkAsD3D9BackBuffer();

    // Textures
    } else if (m_commonSurf->IsTexture()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing texture...");

      Com<d3d9::IDirect3DTexture9> tex;

      hr = m_d3d9Device->CreateTexture(
        desc->dwWidth, desc->dwHeight, m_commonSurf->GetMipCount(), usage,
        format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to create texture");
        m_texture9 = nullptr;
        return hr;
      }

      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps))
        tex->SetAutoGenFilterType(d3d9::D3DTEXF_ANISOTROPIC);

      // Attach level 0 to this surface
      tex->GetSurfaceLevel(0, &surf);
      m_d3d9 = (std::move(surf));

      Logger::debug("DDrawSurface::InitializeD3D9: Created texture");
      m_texture9 = std::move(tex);

    // Depth Stencil
    } else if (m_commonSurf->IsDepthStencil()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing depth stencil...");

      hr = m_d3d9Device->CreateDepthStencilSurface(
        desc->dwWidth, desc->dwHeight, format,
        multiSampleType, 0, FALSE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDrawSurface::InitializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(surf);

    // Offscreen Plain Surfaces
    } else if (m_commonSurf->IsOffScreenPlainSurface()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing offscreen plain surface...");

      // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer,
      // either as existing RTs or during SetRenderTarget() calls, which are tracked with initRT
      if (unlikely(m_commonIntf->GetCommonD3DDevice()->IsCurrentRenderTarget(this) || initRT)) {
        Logger::debug("DDrawSurface::InitializeD3D9: Offscreen plain surface is the RT");

        m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

        if (unlikely(surf == nullptr)) {
          Logger::err("DDrawSurface::InitializeD3D9: Failed to retrieve offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      } else {
        hr = m_d3d9Device->CreateOffscreenPlainSurface(
          desc->dwWidth, desc->dwHeight, format,
          pool, &surf, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDrawSurface::InitializeD3D9: Failed to create offscreen plain surface");
          m_d3d9 = nullptr;
          return hr;
        }
      }

      m_d3d9 = std::move(surf);

      m_commonSurf->MarkAsD3D9BackBuffer();

    // Overlays (haven't seen any actual use of overlays in the wild)
    } else if (m_commonSurf->IsOverlay()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing overlay...");

      // Always link overlays to the back buffer
      m_d3d9Device->GetBackBuffer(0, index, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf);

      if (unlikely(surf == nullptr)) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to retrieve overlay surface");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

      m_commonSurf->MarkAsD3D9BackBuffer();

    // Generic render target
    } else if (m_commonSurf->IsRenderTarget()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing render target...");

      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = m_d3d9Device->CreateRenderTarget(
        desc->dwWidth, desc->dwHeight, format,
        multiSampleType, usage, TRUE, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to create render target");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(surf);

    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else if (!m_commonSurf->IsNotKnown()) {
      Logger::debug("DDrawSurface::InitializeD3D9: Initializing generic surface...");

      hr = m_d3d9Device->CreateOffscreenPlainSurface(
          desc->dwWidth, desc->dwHeight, format,
          pool, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawSurface::InitializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDrawSurface::InitializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    } else {
      Logger::warn("DDrawSurface::InitializeD3D9: Skipping initialization of unknown surface");
    }

    if (m_shadowSurf != nullptr)
      m_shadowSurf->SetD3D9(m_d3d9.ptr());

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDrawSurface::UploadSurfaceData() {
    // Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    Logger::debug(str::format("DDrawSurface::UploadSurfaceData: Uploading nr. [[1-", m_surfCount, "]]"));

    const d3d9::D3DFORMAT format = m_commonSurf->GetD3D9Format();

    if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface, DDSURFACEDESC>(m_texture9.ptr(), format,
                                                           m_proxy.ptr(), m_commonSurf->GetMipCount());
    // Blit surfaces directly
    } else {
      if (unlikely(m_commonSurf->IsDepthStencil()))
        Logger::debug("DDrawSurface::UploadSurfaceData: Uploading depth stencil");

      BlitToD3D9Surface<IDirectDrawSurface, DDSURFACEDESC>(m_d3d9.ptr(), format, GetShadowOrProxied());
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

  inline HRESULT DDrawSurface::CreateDeviceInternal(REFIID riid, void** ppvObject) {
    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  rgbFallback          = false;

    if (likely(!d3dOptions->forceSWVP)) {
      if (riid == IID_IDirect3DHALDevice || riid == IID_WineD3DDevice) {
        Logger::info("DDrawSurface::CreateDeviceInternal: Creating an IID_IDirect3DHALDevice device");
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
      } else if (riid == IID_IDirect3DRGBDevice) {
        Logger::info("DDrawSurface::CreateDeviceInternal: Creating an IID_IDirect3DRGBDevice device");
      } else if (riid == IID_IDirect3DMMXDevice) {
        Logger::warn("DDrawSurface::CreateDeviceInternal: Unsupported MMX device, falling back to RGB");
        rgbFallback = true;
      } else if (riid == IID_IDirect3DRampDevice) {
        Logger::warn("DDrawSurface::CreateDeviceInternal: Unsupported Ramp device, falling back to RGB");
        rgbFallback = true;
      } else {
        Logger::warn("DDrawSurface::CreateDeviceInternal: Unknown device identifier, falling back to RGB");
        Logger::warn(str::format(riid));
        rgbFallback = true;
      }
    }

    const IID rclsidOverride = rgbFallback ? IID_IDirect3DRGBDevice : riid;

    HWND hWnd = m_commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hWnd == nullptr)) {
      Logger::debug("DDrawSurface::CreateDeviceInternal: HWND is NULL");
    }

    Com<IDirect3DDevice> ppvProxyObject;
    HRESULT hr = GetShadowOrProxied()->QueryInterface(rclsidOverride, reinterpret_cast<void**>(&ppvProxyObject));
    if (unlikely(FAILED(hr)))
      return hr;

    DWORD backBufferWidth  = m_commonSurf->GetDesc()->dwWidth;
    DWORD BackBufferHeight = m_commonSurf->GetDesc()->dwHeight;

    if (likely(d3dOptions->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        // Wayland apparently needs this for somewhat proper back buffer sizing
        if ((modeSize->width  && modeSize->width  < m_commonSurf->GetDesc()->dwWidth)
          || (modeSize->height && modeSize->height < m_commonSurf->GetDesc()->dwHeight)) {
          Logger::info("DDrawSurface::CreateDeviceInternal: Enforcing mode dimensions");

          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    Com<d3d9::IDirect3D9> d3d9Intf;
    // D3D3 is "special", so we might not have a valid D3D3 interface to work with
    // at this point. Create a temporary D3D9 interface should that ever happen.
    D3D3Interface* d3d3Intf = m_commonIntf->GetD3D3Interface();
    if (unlikely(d3d3Intf == nullptr)) {
      Logger::debug("DDrawSurface::CreateDeviceInternal: Creating a temporary D3D9 interface");
      d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);

      Com<IDxvkD3D8InterfaceBridge> d3d9Bridge;

      if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge))))) {
        Logger::err("DDrawSurface::CreateDeviceInternal: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
        return DDERR_GENERIC;
      }

      d3d9Bridge->EnableD3D3CompatibilityMode();
    } else {
      d3d9Intf = d3d3Intf->GetD3D9();
    }

    // Determine the supported AA sample count by querying the D3D9 interface
    d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3d9::D3DMULTISAMPLE_NONE;
    if (likely(d3dOptions->emulateFSAA != FSAAEmulation::Disabled)) {
      HRESULT hr4S = d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, m_commonSurf->GetD3D9Format(),
                                                          TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
      if (unlikely(FAILED(hr4S))) {
        HRESULT hr2S = d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, m_commonSurf->GetD3D9Format(),
                                                            TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
        if (unlikely(FAILED(hr2S))) {
          Logger::warn("DDrawSurface::CreateDeviceInternal: No MSAA support has been detected");
        } else {
          Logger::info("DDrawSurface::CreateDeviceInternal: Using 2x MSAA for FSAA emulation");
          multiSampleType = d3d9::D3DMULTISAMPLE_2_SAMPLES;
        }
      } else {
        Logger::info("DDrawSurface::CreateDeviceInternal: Using 4x MSAA for FSAA emulation");
        multiSampleType = d3d9::D3DMULTISAMPLE_4_SAMPLES;
      }
    } else {
      Logger::info("DDrawSurface::CreateDeviceInternal: FSAA emulation is disabled");
    }

    const DWORD cooperativeLevel = m_commonIntf->GetCooperativeLevel();

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || d3dOptions->forceMultiThreaded) {
      Logger::info("DDrawSurface::CreateDeviceInternal: Using thread safe runtime synchronization");
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    }
    // DDSCL_FPUPRESERVE does not exist prior to DDraw7,
    // and DDSCL_FPUSETUP is NOT the default state
    if (!(cooperativeLevel & DDSCL_FPUSETUP))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Logger::info(str::format("DDrawSurface::CreateDeviceInternal: Back buffer size: ", backBufferWidth, "x", BackBufferHeight));

    const DWORD backBufferCount = DetermineBackBufferCount(m_proxy.ptr());
    // Consider the front buffer as well when reporting the overall count
    Logger::info(str::format("DDrawSurface::CreateDeviceInternal: Back buffer count: ", backBufferCount + 1));

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = backBufferWidth;
    params.BackBufferHeight   = BackBufferHeight;
    params.BackBufferFormat   = m_commonSurf->GetD3D9Format();
    params.BackBufferCount    = backBufferCount;
    params.MultiSampleType    = multiSampleType;
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = hWnd;
    params.Windowed           = TRUE; // Always use windowed, so that we can delegate mode switching to ddraw
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // Needed for back buffer locks
    params.FullScreen_RefreshRateInHz = 0; // We'll get the right mode/refresh rate set by ddraw, just play along
    params.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT; // A D3D3 device always uses VSync

    Com<d3d9::IDirect3DDevice9> device9;
    hr = d3d9Intf->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hWnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("DDrawSurface::CreateDeviceInternal: Failed to create the D3D9 device");
      return hr;
    }

    Com<D3D3Device> device3 = new D3D3Device(nullptr, std::move(ppvProxyObject), this,
                                             GetD3D3Caps(d3dOptions), rclsidOverride, params,
                                             std::move(device9), deviceCreationFlags9);

    // Set the common device on the common interface
    m_commonIntf->SetCommonD3DDevice(device3->GetCommonD3DDevice());
    // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
    device3->InitializeDS();

    *ppvObject = device3.ref();

    return DD_OK;
  }

  inline void DDrawSurface::RefreshD3D9Device() {
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

    d3d9::IDirect3DDevice9* d3d9Device = commonDevice != nullptr ? commonDevice->GetD3D9Device() : nullptr;
    if (unlikely(m_d3d9Device != d3d9Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (m_d3d9Device != nullptr) {
        Logger::debug("DDrawSurface: Device context has changed, clearing all D3D9 resources");
        m_d3d9 = nullptr;
        if (m_shadowSurf != nullptr)
          m_shadowSurf->SetD3D9(nullptr);
      }

      m_d3d9Device = d3d9Device;
    }
  }

  inline DWORD DDrawSurface::DetermineBackBufferCount(IDirectDrawSurface* renderTarget) {
    DWORD backBufferCount = 0;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (likely(!d3dOptions->forceSingleBackBuffer && !d3dOptions->forceLegacyPresent)) {
      IDirectDrawSurface* backBuffer = renderTarget;
      HRESULT hr;

      while (backBuffer != nullptr) {
        IDirectDrawSurface* parentSurface = backBuffer;
        backBuffer = nullptr;

        hr = parentSurface->EnumAttachedSurfaces(&backBuffer, ListBackBufferSurfacesCallback);
        if (unlikely(FAILED(hr))) {
          Logger::warn("DDrawSurface::DetermineBackBufferCount: Unable to enumerate attached surfaces");
          break;
        }

        backBufferCount++;

        // the swapchain will eventually return to its origin
        if (backBuffer == renderTarget)
          break;
      }
    }

    return backBufferCount;
  }

}

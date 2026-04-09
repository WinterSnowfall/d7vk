#include "ddraw2_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d5/d3d5_device.h"
#include "../d3d5/d3d5_texture.h"

namespace dxvk {

  uint32_t DDraw2Surface::s_surfCount = 0;

  DDraw2Surface::DDraw2Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface2>&& surfProxy,
        DDrawSurface* pParent,
        DDraw2Surface* pParentSurf)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface2, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (unlikely(!m_commonSurf->IsDescSet())) {
      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc(desc);
      }
    }

    // We need to keep the IDirectDrawSurface parent around, since we're entirely dependent on it.
    // If one doesn't exist (e.g. attached surfaces), then use QueryInterface and cache it.
    if (m_parent == nullptr) {
      Com<IDirectDrawSurface> originSurf;
      HRESULT hr = m_proxy->QueryInterface(__uuidof(IDirectDrawSurface), reinterpret_cast<void**>(&originSurf));
      if (unlikely(FAILED(hr)))
        throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve an IDirectDrawSurface interface!");

      m_originSurf = new DDrawSurface(m_commonSurf.ptr(), std::move(originSurf), m_commonIntf->GetDDInterface(), nullptr, false);
      m_parent = m_originSurf.ptr();
    } else {
      m_originSurf = m_parent;
    }

    m_commonIntf->AddWrappedSurface(this);

    m_commonSurf->SetDD2Surface(this);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw2Surface: Created a new surface nr. [[2-", m_surfCount, "]]"));

    // Note: IDirectDrawSurface2 can't ever be the origin surface
  }

  DDraw2Surface::~DDraw2Surface() {
    // Clear the cached depth stencil on the parent if matched
    if (unlikely(m_parentSurf != nullptr && m_commonSurf->IsDepthStencil()
      && m_parentSurf->GetAttachedDepthStencil() == this)) {
      m_parentSurf->ClearAttachedDepthStencil();
    }

    m_commonIntf->RemoveWrappedSurface(this);

    m_commonSurf->SetDD2Surface(nullptr);

    Logger::debug(str::format("DDraw2Surface: Surface nr. [[2-", m_surfCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw2Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirect3DTexture2)) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_parent->GetD3D5Texture() == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_commonIntf->GetNextTextureHandle();
        Com<D3D5Texture> texture5 = new D3D5Texture(std::move(ppvProxyObject), m_parent, nextHandle);
        D3DCommonTexture* commonTex = texture5->GetCommonTexture();
        m_parent->SetD3D5Texture(texture5.ptr());
        m_commonIntf->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = ref(m_parent->GetD3D5Texture());

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirect3DTexture");

      if (unlikely(m_parent->GetD3D3Texture() == nullptr)) {
        Com<IDirect3DTexture> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_commonIntf->GetNextTextureHandle();
        Com<D3D3Texture> texture3 = new D3D3Texture(std::move(ppvProxyObject), m_parent, nextHandle);
        D3DCommonTexture* commonTex = texture3->GetCommonTexture();
        m_parent->SetD3D3Texture(texture3.ptr());
        m_commonIntf->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = ref(m_parent->GetD3D3Texture());

      return S_OK;
    }
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawGammaControl");
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), m_commonSurf->GetDDSurface()));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawColorControl");
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr) {
        Logger::debug("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface");
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface");

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr) {
        Logger::debug("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface3");
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface3");

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr) {
        Logger::debug("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface4");
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface4");

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr) {
        Logger::debug("DDraw2Surface::QueryInterface: Query for existing IDirectDrawSurface7");
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Surface::QueryInterface: Query for IDirectDrawSurface7");

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

  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw2Surface::AddAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw2Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSAttachedSurface);

    if (unlikely(ddraw2Surface->GetCommonSurface()->IsBackBufferOrFlippable())) {
      if (unlikely(m_commonIntf->GetOptions()->forceBlitOnFlip)) {
        Logger::debug("DDraw2Surface::AddAttachedSurface: Caching surface as RT");
        m_commonIntf->SetRenderTarget(ddraw2Surface->GetCommonSurface());
      } else {
        Logger::warn("DDraw2Surface::AddAttachedSurface: Trying to attach a flippable surface");
      }
    }

    HRESULT hr = m_proxy->AddAttachedSurface(ddraw2Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    ddraw2Surface->SetParentSurface(this);
    if (likely(ddraw2Surface->GetCommonSurface()->IsDepthStencil()))
      m_depthStencil = ddraw2Surface;

    return hr;
  }

  // Docs: "This method is used for the software implementation.
  // It is not needed if the overlay support is provided by the hardware."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug(">>> DDraw2Surface::AddOverlayDirtyRect");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw2Surface::Blt: Proxy");

    // Write back any flippable surfaces or depth stencils from D3D9
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw2Surface* sourceSurface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      if (unlikely(sourceSurface->GetCommonSurface()->IsGuardableSurface())) {
        if (m_commonIntf->GetOptions()->backBufferWriteBack || m_commonIntf->GetOptions()->apitraceMode) {
          Logger::debug("DDraw2Surface::Blt: Source surface is a swapchain surface");

          if (unlikely(m_commonIntf->GetOptions()->apitraceMode && !sourceSurface->IsInitialized()))
            sourceSurface->InitializeOrUploadD3D9();

          if (likely(sourceSurface->IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_parent->GetProxied(), m_parent->GetD3D9());
        } else {
          static bool s_swapchainWarningShown;

          if (!std::exchange(s_swapchainWarningShown, true))
            Logger::warn("DDraw2Surface::Blt: Source surface is a swapchain surface");
        }
      } else if (unlikely(sourceSurface->GetCommonSurface()->IsDepthStencil())) {
        if (m_commonIntf->GetOptions()->depthWriteBack || m_commonIntf->GetOptions()->apitraceMode) {
          Logger::debug("DDraw2Surface::Blt: Source surface is a depth stencil");

          if (likely(sourceSurface->IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_parent->GetProxied(), m_parent->GetD3D9());
        } else {
          static bool s_depthStencilWarningShown;

          if (!std::exchange(s_depthStencilWarningShown, true))
            Logger::warn("DDraw2Surface::Blt: Source surface is a depth stencil");
        }
      }
    }

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_DEPTHFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9DepthStencil(m_d3d9.ptr()))) {
        Logger::debug("DDraw2Surface::Blt: Clearing d3d9 depth stencil");

        HRESULT hrClear;
        const float zClear = m_commonSurf->GetNormalizedFloatDepth(lpDDBltFx->dwFillDepth);

        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, zClear, 256);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_ZBUFFER, 0, zClear, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw2Surface::Blt: Failed to clear d3d9 depth");
      }
      // Forward DDBLT_COLORFILL clears to D3D9 if done on the current render target
      if (unlikely(lpDDSrcSurface == nullptr &&
                  (dwFlags & DDBLT_COLORFILL) &&
                  lpDDBltFx != nullptr &&
                  m_commonIntf->GetCommonD3DDevice()->IsCurrentD3D9RenderTarget(m_d3d9.ptr()))) {
        Logger::debug("DDraw2Surface::Blt: Clearing d3d9 render target");

        HRESULT hrClear;
        if (lpDestRect == nullptr) {
          hrClear = m_d3d9Device->Clear(0, NULL, D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        } else {
          hrClear = m_d3d9Device->Clear(1, reinterpret_cast<D3DRECT*>(lpDestRect), D3DCLEAR_TARGET, lpDDBltFx->dwFillColor, 0.0f, 0);
        }
        if (unlikely(FAILED(hrClear)))
          Logger::warn("DDraw2Surface::Blt: Failed to clear d3d9 render target");
      }

      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

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

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw2Surface::Blt: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw2Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (m_commonSurf->IsTexture()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw2Surface::Blt: Failed upload to d3d9 surface");
      } else {
        m_commonSurf->DirtyMipMaps();
      }
    }

    return hr;
  }

  // Docs: "The IDirectDrawSurface2::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw2Surface::BltBatch");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw2Surface::BltFast: Proxy");

    // Write back any flippable surfaces or depth stencils from D3D9
    if (likely(lpDDSrcSurface != nullptr && m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw2Surface* sourceSurface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      if (unlikely(sourceSurface->GetCommonSurface()->IsGuardableSurface())) {
        if (m_commonIntf->GetOptions()->backBufferWriteBack || m_commonIntf->GetOptions()->apitraceMode) {
          Logger::debug("DDraw2Surface::BltFast: Source surface is a swapchain surface");

          if (unlikely(m_commonIntf->GetOptions()->apitraceMode && !sourceSurface->IsInitialized()))
            sourceSurface->InitializeOrUploadD3D9();

          if (likely(sourceSurface->IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_parent->GetProxied(), m_parent->GetD3D9());
        } else {
          static bool s_swapchainWarningShown;

          if (!std::exchange(s_swapchainWarningShown, true))
            Logger::warn("DDraw2Surface::BltFast: Source surface is a swapchain surface");
        }
      } else if (unlikely(sourceSurface->GetCommonSurface()->IsDepthStencil())) {
        if (m_commonIntf->GetOptions()->depthWriteBack || m_commonIntf->GetOptions()->apitraceMode) {
          Logger::debug("DDraw2Surface::BltFast: Source surface is a depth stencil");

          if (likely(sourceSurface->IsInitialized()))
            BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_parent->GetProxied(), m_parent->GetD3D9());
        } else {
          static bool s_depthStencilWarningShown;

          if (!std::exchange(s_depthStencilWarningShown, true))
            Logger::warn("DDraw2Surface::BltFast: Source surface is a depth stencil");
        }
      }
    }

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      const bool exclusiveMode = (m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE)
                              && !m_commonIntf->GetOptions()->ignoreExclusiveMode;

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

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw2Surface::BltFast: Received an unwrapped source surface");
        return DDERR_UNSUPPORTED;
      }
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw2Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (m_commonSurf->IsTexture()) {
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
    Logger::debug("<<< DDraw2Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr)) {
        Logger::warn("DDraw2Surface::DeleteAttachedSurface: Received an unwrapped surface");
        return DDERR_UNSUPPORTED;
      }

      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      if (lpDDSAttachedSurface == nullptr && likely(SUCCEEDED(hrProxy)))
        m_depthStencil = nullptr;

      return hrProxy;
    }

    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, ddraw2Surface->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(m_depthStencil == ddraw2Surface)) {
      ddraw2Surface->ClearParentSurface();
      m_depthStencil = nullptr;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::warn(">>> DDraw2Surface::EnumAttachedSurfaces");
    return m_parent->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
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

    if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw2Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw2Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw2Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw2Surface> surf2 = static_cast<DDraw2Surface*>(lpDDSurfaceTargetOverride);
    if (lpDDSurfaceTargetOverride != nullptr) {
      if (unlikely(!surf2->GetParent()->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw2Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      Logger::debug("*** DDraw2Surface::Flip: Presenting");

      m_commonIntf->ResetDrawTracking();

      if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
        D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

        if (unlikely(!IsInitialized()))
          m_parent->InitializeD3D9(commonDevice->IsCurrentRenderTarget(m_parent));

        BlitToDDrawSurface<IDirectDrawSurface2, DDSURFACEDESC>(m_proxy.ptr(), m_parent->GetD3D9());

        if (likely(commonDevice->IsCurrentRenderTarget(m_parent))) {
          if (lpDDSurfaceTargetOverride != nullptr) {
            m_commonIntf->SetFlipRTSurfaceAndFlags(surf2->GetParent()->GetProxied(), dwFlags);
          } else {
            m_commonIntf->SetFlipRTSurfaceAndFlags(nullptr, dwFlags);
          }
        }
        if (lpDDSurfaceTargetOverride != nullptr) {
          return m_proxy->Flip(surf2->GetProxied(), dwFlags);
        } else {
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        }
      }

      m_d3d9Device->Present(NULL, NULL, NULL, NULL);
    // If we don't have a valid D3D5 device, this means a D3D3 application
    // is trying to flip the surface. Allow that for compatibility reasons.
    } else {
      Logger::debug("<<< DDraw2Surface::Flip: Proxy");
      if (lpDDSurfaceTargetOverride == nullptr) {
        if (unlikely(m_commonIntf->GetOptions()->forceBlitOnFlip)) {
          DDraw2Surface* rt = m_commonIntf->GetRenderTarget()->GetDD2Surface();
          if (rt != nullptr && m_commonSurf->IsPrimarySurface()) {
            Logger::debug("DDraw2Surface::Flip: Blitting instead of flipping");
            m_proxy->Blt(nullptr, rt->GetProxied(), nullptr, DDBLT_WAIT, nullptr);
          } else {
            m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
          }
        }
      } else {
        m_proxy->Flip(surf2->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE2 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw2Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for the primary surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for a flippable surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for an offscreen plain surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for a depth stencil");
    else if (lpDDSCaps->dwCaps & DDSCAPS_MIPMAP)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for a texture");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
      Logger::debug("DDraw2Surface::GetAttachedSurface: Querying for an overlay");

    Com<IDirectDrawSurface2> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDraw2Surface::GetAttachedSurface: Failed to find the requested surface");
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
          Com<DDraw2Surface> ddraw2Surface = new DDraw2Surface(nullptr, std::move(surface), nullptr, this);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw2Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw2Surface.ref()));
          *lplpDDAttachedSurface = ddraw2Surface.ref();
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
  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetBltStatus(DWORD dwFlags) {
    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      Logger::debug("<<< DDraw2Surface::GetBltStatus: Proxy");
      m_proxy->GetBltStatus(dwFlags);
    }

    Logger::debug(">>> DDraw2Surface::GetBltStatus");

    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug(">>> DDraw2Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc()->ddsCaps;

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
    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (unlikely(lphDC == nullptr))
        return DDERR_INVALIDPARAMS;

      InitReturnPtr(lphDC);

      // Foward GetDC calls if we have drawn and the surface is flippable
      RefreshD3D9Device();
      if (m_d3d9Device != nullptr && (m_commonIntf->HasDrawn() &&
                                      m_commonSurf->IsGuardableSurface())) {
        Logger::debug(">>> DDraw2Surface::GetDC");

        if (unlikely(!IsInitialized())) {
          HRESULT hrUpload = InitializeOrUploadD3D9();
          if (unlikely(FAILED(hrUpload)))
            Logger::warn("DDraw2Surface::GetDC: Failed to initialize d3d9 surface");
        }

        HRESULT hr9 = m_parent->GetD3D9()->GetDC(lphDC);
        if (unlikely(FAILED(hr9)))
          Logger::warn("DDraw2Surface::GetDC: Failed D3D9 call");
        return hr9;
      }
    }

    Logger::debug("<<< DDraw2Surface::GetDC: Proxy");
    return m_proxy->GetDC(lphDC);
  }

  // Flipping can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetFlipStatus(DWORD dwFlags) {
    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      Logger::debug("<<< DDraw2Surface::GetFlipStatus: Proxy");
      m_proxy->GetFlipStatus(dwFlags);
    }

    Logger::debug(">>> DDraw2Surface::GetFlipStatus");

    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
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

    *lpDDPixelFormat = m_commonSurf->GetDesc()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw2Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc();

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

    // It's highly unlikely anyone would do depth locks with IDirectDrawSurface2
    if (unlikely(m_commonSurf->IsDepthStencil())) {
      static bool s_depthStencilWarningShown;

      if (!std::exchange(s_depthStencilWarningShown, true))
        Logger::warn("DDraw2Surface::Lock: Surface is a depth stencil");
    }

    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::ReleaseDC(HDC hDC) {
    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      // Foward ReleaseDC calls if we have drawn and the surface is flippable
      RefreshD3D9Device();
      if (m_d3d9Device != nullptr && (m_commonIntf->HasDrawn() &&
                                      m_commonSurf->IsGuardableSurface())) {
        Logger::debug(">>> DDraw2Surface::ReleaseDC");

        if (unlikely(!IsInitialized())) {
          HRESULT hrUpload = InitializeOrUploadD3D9();
          if (unlikely(FAILED(hrUpload)))
            Logger::warn("DDraw2Surface::ReleaseDC: Failed to initialize d3d9 surface");
        }

        HRESULT hr9 = m_parent->GetD3D9()->ReleaseDC(hDC);
        if (unlikely(FAILED(hr9)))
          Logger::warn("DDraw2Surface::ReleaseDC: Failed D3D9 call");
        return hr9;
      }
    }

    Logger::debug("<<< DDraw2Surface::ReleaseDC: Proxy");

    HRESULT hr = m_proxy->ReleaseDC(hDC);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (m_commonSurf->IsTexture()) {
        m_commonSurf->DirtyMipMaps();
      } else if (unlikely(m_commonIntf->GetOptions()->apitraceMode)) {
        // We should ideally upload the surface contents here at all times,
        // however some games are amazing, and do hundreds of locks on the same
        // surface per frame, so this would absolutely tank performance
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw2Surface::ReleaseDC: Failed upload to d3d9 surface");
      }
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

    HRESULT hr = m_proxy->SetColorKey(dwFlags, lpDDColorKey);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = m_commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw2Surface::SetColorKey: Failed to retrieve updated surface desc");

    return DD_OK;
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

    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture()) {
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

    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw2Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw2Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "This method is for software emulation only; it does nothing if the hardware supports overlays."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug(">>> DDraw2Surface::UpdateOverlayDisplay");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSReference) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw2Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw2Surface->GetProxied());
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

  inline void DDraw2Surface::RefreshD3D9Device() {
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

    d3d9::IDirect3DDevice9* d3d9Device = commonDevice != nullptr ? commonDevice->GetD3D9Device() : nullptr;
    if (unlikely(m_d3d9Device != d3d9Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (m_d3d9Device != nullptr) {
        Logger::debug("DDraw2Surface: Device context has changed, clearing all D3D9 resources");
        m_d3d9 = nullptr;
      }

      m_d3d9Device = d3d9Device;
    }
  }

}

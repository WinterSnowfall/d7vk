#include "ddraw3_surface.h"

#include "../ddraw/ddraw_gamma.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d5/d3d5_device.h"
#include "../d3d5/d3d5_texture.h"

namespace dxvk {

  uint32_t DDraw3Surface::s_surfCount = 0;

  DDraw3Surface::DDraw3Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface3>&& surfProxy,
        DDrawSurface* pParent,
        DDraw3Surface* pParentSurf)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf ) {
    // We need to keep the IDirectDrawSurface origin around, since we're entirely dependent on it
    if (m_parent != nullptr)
      m_originSurf = m_parent;

    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw3Surface: ERROR! Failed to retrieve the common interface!");
    }

    m_commonIntf->AddWrappedSurface(this);

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (unlikely(!m_commonSurf->IsDescSet())) {
      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw3Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc(desc);
      }
    }

    // Retrieve and cache the proxy surface color key
    if (unlikely(m_commonSurf->HasColorKey() && !m_commonSurf->IsColorKeySet())) {
      DDCOLORKEY colorKey;

      HRESULT hr = m_proxy->GetColorKey(DDCKEY_SRCBLT, &colorKey);
      // Can return DDERR_NOCOLORKEY
      if (SUCCEEDED(hr))
        m_commonSurf->SetColorKey(&colorKey);
    }

    if (m_commonSurf->GetOrigin() == nullptr)
      m_commonSurf->SetOrigin(this);

    m_commonSurf->SetDD3Surface(this);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw3Surface: Created a new surface nr. [[3-", m_surfCount, "]]"));
  }

  DDraw3Surface::~DDraw3Surface() {
    m_commonIntf->RemoveWrappedSurface(this);

    m_commonSurf->SetDD3Surface(nullptr);

    Logger::debug(str::format("DDraw3Surface: Surface nr. [[3-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface3))
      return this;

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

      *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDDInterface(), nullptr, false));

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

      *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonSurf->GetDDSurface(), nullptr));

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

      *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD4Interface(), nullptr, false));

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

      *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject), m_commonIntf->GetDD7Interface(), nullptr, false));

      return S_OK;
    }
    if (riid == __uuidof(IDirect3DTexture2)) {
      if (m_parent == nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirect3DTexture2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirect3DTexture2");

      if (unlikely(m_parent->GetD3D5Texture() == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_parent->GetParent()->GetNextTextureHandle();
        Com<D3D5Texture> texture5 = new D3D5Texture(std::move(ppvProxyObject), m_parent, nextHandle);
        D3DCommonTexture* commonTex = texture5->GetCommonTexture();
        m_parent->SetD3D5Texture(texture5.ptr());
        m_parent->GetParent()->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = ref(m_parent->GetD3D5Texture());

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      if (m_parent == nullptr) {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirect3DTexture");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirect3DTexture");

      if (unlikely(m_parent->GetD3D3Texture() == nullptr)) {
        Com<IDirect3DTexture> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        D3DTEXTUREHANDLE nextHandle = m_parent->GetParent()->GetNextTextureHandle();
        Com<D3D3Texture> texture3 = new D3D3Texture(std::move(ppvProxyObject), m_parent, nextHandle);
        D3DCommonTexture* commonTex = texture3->GetCommonTexture();
        m_parent->SetD3D3Texture(texture3.ptr());
        m_parent->GetParent()->EmplaceTexture(commonTex, nextHandle);
      }

      *ppvObject = ref(m_parent->GetD3D3Texture());

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

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw3Surface::AddAttachedSurface: Proxy");

    // Some games, like Nightmare Creatures, try to attach an IDirectDrawSurface depth stencil directly...
    if (unlikely(m_commonIntf->IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSAttachedSurface)))) {
      Logger::debug("DDraw3Surface::AddAttachedSurface: Attaching IDirectDrawSurface surface");
      DDrawSurface* ddrawSurface = reinterpret_cast<DDrawSurface*>(lpDDSAttachedSurface);
      Com<IDirectDrawSurface3> surface3;
      ddrawSurface->GetProxied()->QueryInterface(__uuidof(IDirectDrawSurface3), reinterpret_cast<void**>(&surface3));
      return m_proxy->AddAttachedSurface(surface3.ptr());
    }

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw3Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_GENERIC;
    }

    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSAttachedSurface);
    return m_proxy->AddAttachedSurface(ddraw3Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw3Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw3Surface::Blt: Proxy");

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

    if (unlikely(m_commonSurf->IsDepthStencil())) {
      // Forward DDBLT_DEPTHFILL clears to D3D9 if done on the current depth stencil
      if (lpDDSrcSurface == nullptr &&
          (dwFlags & DDBLT_DEPTHFILL) &&
           lpDDBltFx != nullptr &&
           m_d3d9Device != nullptr &&
           m_commonIntf->IsCurrentD3D9DepthStencil(m_parent->GetD3D9())) {
        Logger::debug("DDraw3Surface::Blt: Clearing D3D9 depth stencil");

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

    // It's highly unlikely anyone would do depth blits with IDirectDrawSurface3
    if (likely(m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw3Surface* ddraw3Depth = static_cast<DDraw3Surface*>(lpDDSrcSurface);
      if (unlikely(ddraw3Depth != nullptr && ddraw3Depth->GetCommonSurface()->IsDepthStencil())) {
        static bool s_depthStencilWarningShown;

        if (!std::exchange(s_depthStencilWarningShown, true))
          Logger::warn("DDraw3Surface::Blt: Source surface is a depth stencil");
      }
    }

    HRESULT hr;

    // Powerslide tries to blit here from a IDirectDrawSurface source
    if (unlikely(m_commonIntf->IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSrcSurface)))) {
      Logger::debug("DDraw3Surface::Blt: Received an IDirectDrawSurface source surface");
      DDrawSurface* ddrawSurface = reinterpret_cast<DDrawSurface*>(lpDDSrcSurface);
      Com<IDirectDrawSurface3> surface3;
      ddrawSurface->GetProxied()->QueryInterface(__uuidof(IDirectDrawSurface3), reinterpret_cast<void**>(&surface3));
      hr = m_proxy->Blt(lpDestRect, surface3.ptr(), lpSrcRect, dwFlags, lpDDBltFx);
    } else if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw3Surface::Blt: Received an unwrapped source surface");
        return DDERR_GENERIC;
      }
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw3Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture() || m_commonIntf->GetOptions()->apitraceMode) {
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

    // It's highly unlikely anyone would do depth blits with IDirectDrawSurface3
    if (likely(m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw3Surface* ddraw3Depth = static_cast<DDraw3Surface*>(lpDDSrcSurface);
      if (unlikely(ddraw3Depth != nullptr && ddraw3Depth->GetCommonSurface()->IsDepthStencil())) {
        static bool s_depthStencilWarningShown;

        if (!std::exchange(s_depthStencilWarningShown, true))
          Logger::warn("DDraw3Surface::BltFast: Source surface is a depth stencil");
      }
    }

    HRESULT hr;

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr)) {
        Logger::warn("DDraw3Surface::BltFast: Received an unwrapped source surface");
        return DDERR_GENERIC;
      }
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw3Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture() || m_commonIntf->GetOptions()->apitraceMode) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
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
    Logger::warn("<<< DDraw3Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDraw3Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Flip(LPDIRECTDRAWSURFACE3 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    // Lost surfaces are not flippable
    HRESULT hr = m_proxy->IsLost();
    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw3Surface::Flip: Lost surface");
      return hr;
    }

    if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable()))) {
      Logger::debug("DDraw3Surface::Flip: Unflippable surface");
      return DDERR_NOTFLIPPABLE;
    }

    const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

    // Non-exclusive mode validations
    if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode)) {
      Logger::debug("DDraw3Surface::Flip: Primary surface flip in non-exclusive mode");
      return DDERR_NOEXCLUSIVEMODE;
    }

    // Exclusive mode validations
    if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode)) {
      Logger::debug("DDraw3Surface::Flip: Back buffer flip in exclusive mode");
      return DDERR_NOTFLIPPABLE;
    }

    Com<DDraw3Surface> surf3 = static_cast<DDraw3Surface*>(lpDDSurfaceTargetOverride);
    if (lpDDSurfaceTargetOverride != nullptr) {
      if (unlikely(!surf3->GetParent()->GetCommonSurface()->IsBackBufferOrFlippable())) {
        Logger::debug("DDraw3Surface::Flip: Unflippable override surface");
        return DDERR_NOTFLIPPABLE;
      }
    }

    RefreshD3D9Device();
    if (likely(m_d3d9Device != nullptr)) {
      Logger::debug("*** DDraw3Surface::Flip: Presenting");

      m_commonIntf->ResetDrawTracking();

      if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
        if (unlikely(!IsInitialized()))
          m_parent->InitializeD3D9(m_commonIntf->IsCurrentRenderTarget(m_parent));

        BlitToDDrawSurface<IDirectDrawSurface3, DDSURFACEDESC>(m_proxy.ptr(), m_parent->GetD3D9());

        if (likely(m_commonIntf->IsCurrentRenderTarget(m_parent))) {
          if (lpDDSurfaceTargetOverride != nullptr) {
            m_commonIntf->SetFlipRTSurfaceAndFlags(surf3->GetParent()->GetProxied(), dwFlags);
          } else {
            m_commonIntf->SetFlipRTSurfaceAndFlags(nullptr, dwFlags);
          }
        }
        if (lpDDSurfaceTargetOverride != nullptr) {
          return m_proxy->Flip(surf3->GetProxied(), dwFlags);
        } else {
          return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
        }
      }

      m_d3d9Device->Present(NULL, NULL, NULL, NULL);
    // If we don't have a valid D3D5 device, this means a D3D3 application
    // is trying to flip the surface. Allow that for compatibility reasons.
    } else {
      Logger::debug("<<< DDraw3Surface::Flip: Proxy");
      if (lpDDSurfaceTargetOverride == nullptr) {
        m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        m_proxy->Flip(surf3->GetProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE3 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw3Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for the primary surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for the back buffer");
    else if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for a flippable surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for an offscreen plain surface");
    else if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for a depth stencil");
    else if (lpDDSCaps->dwCaps & DDSCAPS_MIPMAP)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for a texture");
    else if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
      Logger::debug("DDraw3Surface::GetAttachedSurface: Querying for an overlay");

    Com<IDirectDrawSurface3> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDraw3Surface::GetAttachedSurface: Failed to find the requested surface");
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    if (likely(!m_commonIntf->IsWrappedSurface(surface.ptr()))) {
      Logger::debug("DDraw3Surface::GetAttachedSurface: Got a new unwrapped surface");
      try {
        Com<DDraw3Surface> ddraw3Surface = new DDraw3Surface(nullptr, std::move(surface), nullptr, this);
        *lplpDDAttachedSurface = ddraw3Surface.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        *lplpDDAttachedSurface = nullptr;
        return DDERR_GENERIC;
      }
    // Can potentially happen with manually attached surfaces
    } else {
      Logger::debug("DDraw3Surface::GetAttachedSurface: Got an existing wrapped surface");
      Com<DDraw3Surface> ddraw3Surface = static_cast<DDraw3Surface*>(surface.ptr());
      *lplpDDAttachedSurface = ddraw3Surface.ref();
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug(">>> DDraw3Surface::GetCaps");

    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc()->ddsCaps;

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
    RefreshD3D9Device();
    if (m_d3d9Device != nullptr && !(m_commonIntf->HasDrawn() &&
                                     m_commonSurf->IsGuardableSurface())) {
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

    *lpDDPixelFormat = m_commonSurf->GetDesc()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug(">>> DDraw3Surface::GetSurfaceDesc");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc();

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

    // It's highly unlikely anyone would do depth locks with IDirectDrawSurface3
    if (unlikely(m_commonSurf->IsDepthStencil())) {
      static bool s_depthStencilWarningShown;

      if (!std::exchange(s_depthStencilWarningShown, true))
        Logger::warn("DDraw3Surface::Lock: Surface is a depth stencil");
    }

    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw3Surface::ReleaseDC");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      if (m_commonSurf->IsTexture() && !m_commonIntf->GetOptions()->apitraceMode)
        m_commonSurf->DirtyMipMaps();
      return m_proxy->ReleaseDC(hDC);
    }

    if (unlikely(!IsInitialized())) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet initialized");
      return m_proxy->ReleaseDC(hDC);
    }

    // Proxy ReleaseDC calls if we haven't yet drawn and the surface is flippable
    RefreshD3D9Device();
    if (m_d3d9Device != nullptr && !(m_commonIntf->HasDrawn() &&
                                     m_commonSurf->IsGuardableSurface())) {
      Logger::debug("DDraw3Surface::ReleaseDC: Not yet drawn flippable surface");
      if (m_commonSurf->IsTexture() && !m_commonIntf->GetOptions()->apitraceMode)
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

    HRESULT hr = m_proxy->SetColorKey(dwFlags, lpDDColorKey);
    if (unlikely(FAILED(hr)))
      return hr;

    if (dwFlags == DDCKEY_SRCBLT) {
      Logger::debug("DDraw3Surface::SetColorKey: Updating DDCKEY_SRCBLT color key");

      if (lpDDColorKey != nullptr) {
        m_commonSurf->SetColorKey(lpDDColorKey);

        if (unlikely(lpDDColorKey->dwColorSpaceLowValue  != 0 ||
                    lpDDColorKey->dwColorSpaceHighValue != 0))
          Logger::debug("DDraw3Surface::SetColorKey: Use of non-black color key");
      } else {
        m_commonSurf->ClearColorKey();
      }
    }

    return DD_OK;
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

    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!m_commonSurf->IsTexture() || m_commonIntf->GetOptions()->apitraceMode) {
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

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDDestSurface))) {
      Logger::warn("DDraw3Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_GENERIC;
    }

    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw3Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSReference) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayZOrder: Proxy");

    if (unlikely(!m_commonIntf->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw3Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_GENERIC;
    }

    DDraw3Surface* ddraw3Surface = static_cast<DDraw3Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw3Surface->GetProxied());
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
    DDSURFACEDESC desc;
    desc.dwSize = sizeof(DDSURFACEDESC);
    hr = m_proxy->GetSurfaceDesc(&desc);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw3Surface::SetSurfaceDesc: Failed to get new surface desc");
    } else {
      m_commonSurf->SetDesc(desc);
    }

    // We may need to recreate the d3d9 object based on the new desc
    m_d3d9 = nullptr;

    if (!m_commonSurf->IsTexture() || m_commonIntf->GetOptions()->apitraceMode) {
      InitializeOrUploadD3D9();
    } else {
      m_commonSurf->DirtyMipMaps();
    }

    return hr;
  }

}

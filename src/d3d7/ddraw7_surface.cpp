#include "ddraw7_surface.h"

#include "d3d7_caps.h"

#include <algorithm>

namespace dxvk {

  uint32_t DDraw7Surface::s_surfCount = 0;

  DDraw7Surface::DDraw7Surface(
        Com<IDirectDrawSurface7>&& surfProxy,
        DDraw7Interface* pParent,
        DDraw7Surface* pParentSurf,
        DDSURFACEDESC2 desc)
    : DDrawWrappedObject<d3d9::IDirect3DSurface9, IDirectDrawSurface7>(nullptr, std::move(surfProxy))
    , m_parent     ( pParent )
    , m_parentSurf ( pParentSurf )
    , m_desc       ( desc ) {
    m_parent->AddWrappedSurface(this);

    m_surfCount = ++s_surfCount;

    ListSurfaceDetails();
  }

  DDraw7Surface::~DDraw7Surface() {
    m_parent->RemoveWrappedSurface(this);

    Logger::info(str::format("DDraw7Surface: Surface nr. [[", m_surfCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr)) {
      Logger::err("DDraw7Surface::AddAttachedSurface: Called with NULL surface");
      return DDERR_INVALIDPARAMS;
    }

    if (likely(m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

      auto it = std::find(m_attachedSurfaces.begin(), m_attachedSurfaces.end(), ddraw7Surface);
      if (likely(it == m_attachedSurfaces.end())) {
          m_attachedSurfaces.push_back(ddraw7Surface);
          Logger::debug("DDraw7Surface::AddAttachedSurface: Attached new surface");
      } else {
          Logger::warn("DDraw7Surface::AddAttachedSurface: Surface is already attached");
      }

      return m_proxy->AddAttachedSurface(ddraw7Surface->GetProxied());
    } else {
      Logger::warn("DDraw7Surface::AddAttachedSurface: Attaching non-wrapped surface");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw7Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw7Surface::Blt: Proxy");

    if (unlikely(IsFrontBuffer() || IsBackBuffer()))
      return DD_OK;

    HRESULT hr;

    if (likely(m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw7Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      Logger::warn("DDraw7Surface::Blt: Received an unwrapped source surface");
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (SUCCEEDED(hr))
      InitializeOrUploadD3D9();

    return hr;
  }

  // Allegedly unimplemented, according to the official docs
  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::BltBatch: Proxy");
    return m_proxy->BltBatch(lpDDBltBatch, dwCount, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw7Surface::BltFast: Proxy");

    if (unlikely(IsFrontBuffer() || IsBackBuffer()))
      return DD_OK;

    HRESULT hr;

    if (likely(lpDDSrcSurface != nullptr && m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw7Surface->GetProxied(), lpSrcRect, dwTrans);
    } else {
      Logger::warn("DDraw7Surface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    }

    if (SUCCEEDED(hr))
      InitializeOrUploadD3D9();

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::DeleteAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr)) {
      Logger::err("DDraw7Surface::DeleteAttachedSurface: Called with NULL surface");
      return DDERR_INVALIDPARAMS;
    }

    if (likely(m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

      auto it = std::find(m_attachedSurfaces.begin(), m_attachedSurfaces.end(), ddraw7Surface);
      if (likely(it != m_attachedSurfaces.end())) {
          m_attachedSurfaces.erase(it);
          Logger::debug("DDraw7Surface::DeleteAttachedSurface: Removed attached surface");
      } else {
          Logger::warn("DDraw7Surface::DeleteAttachedSurface: Surface not found");
      }

      return m_proxy->DeleteAttachedSurface(dwFlags, ddraw7Surface->GetProxied());
    } else {
      Logger::warn("DDraw7Surface::DeleteAttachedSurface: Deleting non-wrapped surface");
      return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw7Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpfnCallback) {
    Logger::debug("<<< DDraw7Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Flip(LPDIRECTDRAWSURFACE7 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("*** DDraw7Surface::Flip: Presenting");

    if (unlikely(lpDDSurfaceTargetOverride != nullptr))
      Logger::warn("DDraw7Surface::Flip: use of non-NULL lpDDSurfaceTargetOverride");

    refreshD3D7Device();
    if (likely(m_d3d7device != nullptr)) {
      m_d3d7device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for the front buffer");
    if (lpDDSCaps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP))
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for the back buffer");
    if (lpDDSCaps->dwCaps & DDSCAPS_OFFSCREENPLAIN)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for an offscreen plain surface");
    if (lpDDSCaps->dwCaps & DDSCAPS_ZBUFFER)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for a depth stencil");
    if (lpDDSCaps->dwCaps & (DDSCAPS_TEXTURE | DDSCAPS_MIPMAP)
     || lpDDSCaps->dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for a texture mip map");
    else if (lpDDSCaps->dwCaps & DDSCAPS_TEXTURE)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for a texture");
    if (lpDDSCaps->dwCaps2 & DDSCAPS2_CUBEMAP)
      Logger::info("DDraw7Surface::GetAttachedSurface: Querying for a cube map");

    Com<IDirectDrawSurface7> surface = nullptr;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);

    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      Logger::debug("DDraw7Surface::GetAttachedSurface: Failed to find the requested surface");
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    if (likely(!m_parent->IsWrappedSurface(surface.ptr()))) {
      Logger::info("DDraw7Surface::GetAttachedSurface: Got a new unwrapped surface");
      DDSURFACEDESC2 desc;
      desc.dwSize = sizeof(DDSURFACEDESC2);
      surface->GetSurfaceDesc(&desc);
      Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(std::move(surface), m_parent, this, desc);
      m_attachedSurfaces.push_back(ddraw7Surface.ptr());
      *lplpDDAttachedSurface = ddraw7Surface.ref();
    // Can potentially happen with manually attached surfaces
    } else {
      Logger::info("DDraw7Surface::GetAttachedSurface: Got an existing wrapped surface");
      Com<DDraw7Surface> ddraw7Surface = static_cast<DDraw7Surface*>(surface.ptr());
      *lplpDDAttachedSurface = ddraw7Surface.ref();
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  // TODO: Maybe cache the caps during surface creation?
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    Logger::debug("<<< DDraw7Surface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetClipper(IDirectDrawClipper **lplpDDClipper) {
    Logger::debug("<<< DDraw7Surface::GetClipper: Proxy");
    return m_proxy->GetClipper(lplpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw7Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDC(HDC *lphDC) {
    Logger::debug(">>> DDraw7Surface::GetDC");

    InitializeOrUploadD3D9();

    HRESULT hr = GetD3D9()->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::GetDC: Failed to get d3d9 DC");
      return m_proxy->GetDC(lphDC);
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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPalette(IDirectDrawPalette **lplpDDPalette) {
    Logger::debug("<<< DDraw7Surface::GetPalette: Proxy");
    return m_proxy->GetPalette(lplpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug("<<< DDraw7Surface::GetPixelFormat: Proxy");
    return m_proxy->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw7Surface::GetSurfaceDesc: Proxy");
    // Don't return what we cache for now, since validatinons
    // need to be performed and some games actually depend on it
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw7Surface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::IsLost() {
    Logger::debug("<<< DDraw7Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw7Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw7Surface::ReleaseDC");

    HRESULT hr = GetD3D9()->ReleaseDC(hDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::ReleaseDC: Failed to release d3d9 DC");
      return m_proxy->ReleaseDC(hDC);
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Restore() {
    Logger::debug("<<< DDraw7Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw7Surface::SetClipper: Proxy");
    return m_proxy->SetClipper(lpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw7Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw7Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw7Surface::SetPalette: Proxy");
    return m_proxy->SetPalette(lpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Unlock(LPRECT lpSurfaceData) {
    Logger::debug("<<< DDraw7Surface::Unlock: Proxy");

    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (SUCCEEDED(hr))
      InitializeOrUploadD3D9();

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlay: Proxy");

    if (likely(m_parent->IsWrappedSurface(lpDDDestSurface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDDestSurface);
      return m_proxy->UpdateOverlay(lpSrcRect, ddraw7Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
    } else {
      return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlayZOrder: Proxy");

    if (likely(m_parent->IsWrappedSurface(lpDDSReference))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSReference);
      return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw7Surface->GetProxied());
    } else {
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDDInterface(void **lplpDD) {
    Logger::debug(">>> DDraw7Surface::GetDDInterface");

    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    // Was an easy footgun to return a proxied interface
    *lplpDD = m_parent;

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

    if (unlikely(lpDDSD == nullptr))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    // Make sure we don't store crap desc data
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::SetSurfaceDesc: Failed to set surface desc");
    } else {
      m_desc = *lpDDSD;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPrivateData(const GUID &tag, LPVOID pData, DWORD cbSize, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::SetPrivateData: Proxy");
    return m_proxy->SetPrivateData(tag, pData, cbSize, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPrivateData(const GUID &tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    Logger::debug("<<< DDraw7Surface::GetPrivateData: Proxy");
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::FreePrivateData(const GUID &tag) {
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
    Logger::debug("<<< DDraw7Surface::SetPriority: Proxy");
    return m_proxy->SetPriority(prio);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPriority(LPDWORD prio) {
    Logger::debug("<<< DDraw7Surface::GetPriority: Proxy");
    return m_proxy->GetPriority(prio);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetLOD(DWORD lod) {
    Logger::debug("<<< DDraw7Surface::SetLOD: Proxy");
    return m_proxy->SetLOD(lod);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetLOD(LPDWORD lod) {
    Logger::debug("<<< DDraw7Surface::GetLOD: Proxy");
    return m_proxy->GetLOD(lod);
  }

  // Callback function used in cube map face/surface initialization
  HRESULT STDMETHODCALLTYPE EnumAndAttachSurfacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    d3d9::IDirect3DCubeTexture9* cube = static_cast<d3d9::IDirect3DCubeTexture9*>(ctx);

    // Skip zbuffer. (Are we expecting a Z-buffer by default? Just for RT?)
    if (desc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
      return DDENUMRET_OK;

    Com<d3d9::IDirect3DSurface9> face = nullptr;
    cube->GetCubeMapSurface(GetCubemapFace(desc), 0, &face);

    DDraw7Surface* ddraw7surface = static_cast<DDraw7Surface*>(subsurf);
    ddraw7surface->SetSurface(std::move(face));

    return DDENUMRET_OK;
  }

  // Callback function used to navigate the linked mip map chain
  HRESULT STDMETHODCALLTYPE ListMipChainSurfacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextMip = static_cast<IDirectDrawSurface7**>(ctx);

    if (desc->ddsCaps.dwCaps & (DDSCAPS_TEXTURE | DDSCAPS_MIPMAP)
     || desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // Callback function used to return the depth stencil
  // The depth stencil is held as an attached surface of the render target
  HRESULT STDMETHODCALLTYPE DepthStencilSurfacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** depthStencil = static_cast<IDirectDrawSurface7**>(ctx);

    // This should typically hit on the first attached surface of an RT
    if (desc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER) {
      *depthStencil = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  HRESULT DDraw7Surface::InitializeOrUploadD3D9() {
    HRESULT hr = D3DERR_NOTAVAILABLE;

    refreshD3D7Device();

    if (!IsInitialized())
      hr = IntializeD3D9();
    else
      hr = UploadTextureData();

    return hr;
  }

  inline HRESULT DDraw7Surface::IntializeD3D9() {
    Logger::info(str::format("DDraw7Surface::IntializeD3D9: Initializing nr. [[", m_surfCount, "]]"));

    if (m_d3d7device == nullptr) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Null D3D7 device, can't initalize right now");
      return DD_OK;
    }

    HRESULT hr;

    // This is normal for front buffers apparently...
    if (unlikely(!IsFrontBuffer() && (m_desc.dwHeight == 0 || m_desc.dwWidth == 0))) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Surface has 0 height or width");
      return DD_OK;
    }

    // Textures should be fine in MANAGED as a rule of thumb
    d3d9::D3DPOOL pool  = d3d9::D3DPOOL_MANAGED;
    DWORD         usage = 0;
    // Place all possible render targets in DEFAULT,
    // as well as any overlays (typically used for video)
    if (IsRenderTarget() || IsOverlay()) {
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage = D3DUSAGE_RENDERTARGET;
    }
    if (m_isDXT) {
      pool  = d3d9::D3DPOOL_DEFAULT;
      // This is needed for us to be able to lock the texture
      usage = D3DUSAGE_DYNAMIC;
    }
    // Not sure if this is all that good for perf,
    // but let's respect what the application asks for
    if (pool == d3d9::D3DPOOL_MANAGED && (m_desc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
      pool = d3d9::D3DPOOL_SYSTEMMEM;

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guess it was intended more a hint, not neceesarily how many mips ended up on the GPU.

    IDirectDrawSurface7* mipMap = GetProxied();

    while (mipMap != nullptr) {
      IDirectDrawSurface7* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
      if (mipMap != nullptr) {
        m_mipCount++;
      }
    }

    Logger::debug(str::format("DDraw7Surface::UploadTextureData: Found ", m_mipCount, " mips"));

    uint32_t mipLevels = std::min(static_cast<uint32_t>(m_mipCount + 1), caps7::MaxMipLevels);
    if (mipLevels > 1)
      Logger::debug(str::format("DDraw7Surface::UploadTextureData: Found ", mipLevels, " mip levels"));

    // Render Target / various base surface types
    if (IsRenderTarget() || IsOverlay()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing render target (allegedly)...");

      Com<d3d9::IDirect3DSurface9> rt = nullptr;

      if (IsFrontBuffer()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created front buffer surface");
      }

      else if (IsBackBuffer()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created back buffer surface");
      }

      else if (IsOffScreenPlainSurface()) {
        hr = m_d3d7device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
          pool, &rt, nullptr);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created offscreen plain surface");
      }

      else if (IsOverlay()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created overlay surface");
      }

      else {
        // Must be lockable for blitting to work
        hr = m_d3d7device->GetD3D9()->CreateRenderTarget(
          m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
          d3d9::D3DMULTISAMPLE_NONE, usage, TRUE, &rt, nullptr);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created generic RT");
      }

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create RT");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(rt);
    // Depth Stencil
    } else if (IsDepthStencil()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing depth stencil...");

      Com<d3d9::IDirect3DSurface9> ds = nullptr;

      // Bind the auto depth stencil
      hr = m_d3d7device->GetD3D9()->GetDepthStencilSurface(&ds);

      /*hr = m_d3d7device->GetD3D9()->CreateDepthStencilSurface(
        m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
        d3d9::D3DMULTISAMPLE_NONE, 0, FALSE, &ds, nullptr);
      Logger::info("DDraw7Surface::IntializeD3D9: Created DS");*/

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(ds);
    // Cube maps
    } else if (IsCubeMap()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing cube map...");

      Com<d3d9::IDirect3DCubeTexture9> cubetex = nullptr;
      // TODO: Pool for non-RT cubemaps. Better check if cube maps
      // can even be render targets in d3d7...
      hr = m_d3d7device->GetD3D9()->CreateCubeTexture(
        m_desc.dwWidth, mipLevels, usage,
        ConvertFormat(m_desc.ddpfPixelFormat), pool, &cubetex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create cube map");
        m_cubeMap = nullptr;
        return hr;
      } else {
        Logger::info("DDraw7Surface::IntializeD3D9: Created cube map");
      }

      // Attach face 0 to this surface.
      Com<d3d9::IDirect3DSurface9> face = nullptr;
      cubetex->GetCubeMapSurface((d3d9::D3DCUBEMAP_FACES)0, 0, &face);
      m_d3d9 = (std::move(face));

      // Attach sides 1-5 to each attached surface.
      EnumAttachedSurfaces(cubetex.ptr(), EnumAndAttachSurfacesCallback);

      m_cubeMap = std::move(cubetex);
    // Textures
    } else if (IsTexture()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing a texture...");

      Com<d3d9::IDirect3DTexture9> tex = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateTexture(
        m_desc.dwWidth, m_desc.dwHeight, mipLevels, usage,
        ConvertFormat(m_desc.ddpfPixelFormat), pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::IntializeD3D9: Created d3d9 texture");
      m_texture = std::move(tex);

    } else {
      Logger::warn("DDraw7Surface::IntializeD3D9: Unknown surface type");

      Com<d3d9::IDirect3DSurface9> surf = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
          d3d9::D3DPOOL_MANAGED, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::info("DDraw7Surface::IntializeD3D9: Created offscreen plain surface");
      m_d3d9 = std::move(surf);
    }

    UploadTextureData();

    return DD_OK;
  }

  inline HRESULT DDraw7Surface::UploadTextureData() {
    Logger::info(str::format("DDraw7Surface::UploadTextureData: Uploading nr. [[", m_surfCount, "]]"));

    // Nothing to upload
    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw7Surface::UploadTextureData: No wrapped surface or texture");
      return DD_OK;
    }

    // TODO: In the case of uploading to front buffers, we need to get the back buffer
    // attached surface and upload data from there... this is needed for some cursed GDI
    // inter-op in Praetorians and most likely other games that use GetDC on the front buffer
    if (IsFrontBuffer())
      return DD_OK;

    if (m_desc.dwHeight == 0 || m_desc.dwWidth == 0) {
      Logger::warn("DDraw7Surface::UploadTextureData: Surface has 0 height or width");
      return DD_OK;
    }

    // Blit all the mips for textures
    if (m_texture != nullptr) {
      uint32_t mipLevels = std::min(static_cast<uint32_t>(m_mipCount + 1), caps7::MaxMipLevels);

      Logger::debug(str::format("DDraw7Surface::UploadTextureData: Blitting ", mipLevels, " mip map(s)"));

      IDirectDrawSurface7* mipMap = GetProxied();

      for (uint32_t i = 0; i < mipLevels; i++) {
        d3d9::D3DLOCKED_RECT rect9mip;
        HRESULT hr9mip = m_texture->LockRect(i, &rect9mip, 0, D3DLOCK_READONLY);
        if (SUCCEEDED(hr9mip)) {
          DDSURFACEDESC2 descMip;
          descMip.dwSize = sizeof(DDSURFACEDESC2);
          HRESULT hr7mip = mipMap->Lock(0, &descMip, DDLOCK_WRITEONLY, 0);
          if (SUCCEEDED(hr7mip)) {
            Logger::debug(str::format("descMip.dwWidth:  ", descMip.dwWidth));
            Logger::debug(str::format("descMip.dwHeight: ", descMip.dwHeight));
            Logger::debug(str::format("descMip.lPitch:   ", descMip.lPitch));
            Logger::debug(str::format("rect9mip.Pitch:   ", rect9mip.Pitch));
            if (unlikely(descMip.lPitch != rect9mip.Pitch)) {
              Logger::err(str::format("DDraw7Surface::UploadTextureData: Incompatible mip map ", i + 1, " pitch"));
            } else {
              size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
              memcpy(rect9mip.pBits, descMip.lpSurface, size);
            }
            mipMap->Unlock(0);
          }
          m_texture->UnlockRect(i);

          IDirectDrawSurface7* parentSurface = mipMap;
          mipMap = nullptr;
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
          if (mipMap == nullptr) {
            Logger::warn(str::format("DDraw7Surface::UploadTextureData: Last source mip nr. ", i + 1));
            break;
          }

          Logger::debug(str::format("DDraw7Surface::UploadTextureData: Done blitting mip ", i + 1));
        } else {
          Logger::warn(str::format("DDraw7Surface::UploadTextureData: Failed to lock d3d9 mip ", i + 1));
        }
      }

    } else if (m_cubeMap != nullptr) {
    // TODO: Handle uploading all cubemap faces
      Logger::warn("DDraw7Surface::UploadTextureData: Unhandled upload of cube map");

    // Blit surfaces directly
    // TODO: does this even work with depth stencils and other misc types?
    } else if (m_d3d9 != nullptr) {

      d3d9::D3DLOCKED_RECT rect9;
      HRESULT hr9 = m_d3d9->LockRect(&rect9, 0, D3DLOCK_READONLY);
      if (SUCCEEDED(hr9)) {
        DDSURFACEDESC2 desc;
        desc.dwSize = sizeof(DDSURFACEDESC2);
        HRESULT hr7 = m_proxy->Lock(0, &desc, DDLOCK_WRITEONLY, 0);
        if (SUCCEEDED(hr7)) {
          Logger::debug(str::format("desc.dwWidth:  ", desc.dwWidth));
          Logger::debug(str::format("desc.dwHeight: ", desc.dwHeight));
          Logger::debug(str::format("desc.lPitch:   ", desc.lPitch));
          Logger::debug(str::format("rect.Pitch:    ", rect9.Pitch));
          if (unlikely(desc.lPitch != rect9.Pitch)) {
            Logger::err("DDraw7Surface::UploadTextureData: Incompatible surface pitch");
          } else {
            size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
            memcpy(rect9.pBits, desc.lpSurface, size);
          }
          m_proxy->Unlock(0);
        }
        m_d3d9->UnlockRect();
      }
    }

    Logger::debug("DDraw7Surface::UploadTextureData: Upload complete");

    return DD_OK;
  }

  IDirectDrawSurface7* DDraw7Surface::GetAttachedDepthStencil() {
    IDirectDrawSurface7* depthStencil = nullptr;
    EnumAttachedSurfaces(&depthStencil, DepthStencilSurfacesCallback);
    return depthStencil;
  }

}

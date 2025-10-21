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

    if (SUCCEEDED(hr) && NeedsUpload())
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

    if (SUCCEEDED(hr) && NeedsUpload())
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

    if (unlikely(FAILED(hr))) {
      Logger::warn("DDraw7Surface::GetAttachedSurface: Failed to find the requested surface");
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
    Logger::debug("<<< DDraw7Surface::GetDC: Proxy");
    return m_proxy->GetDC(lphDC);
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
    Logger::debug("<<< DDraw7Surface::ReleaseDC: Proxy");
    return m_proxy->ReleaseDC(hDC);
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

    if (SUCCEEDED(hr) && NeedsUpload())
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
    Logger::info(str::format("DDraw7Surface::IntializeD3D9: Initializing nr. [[[", m_surfCount, "]]]"));

    if (m_d3d7device == nullptr) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Null D3D7 device, can't initalize right now");
      return DD_OK;
    }

    HRESULT hr;

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Surface has 0 height or width");
      return DD_OK;
    }

    d3d9::D3DPOOL pool = d3d9::D3DPOOL_MANAGED;
    // Place all possible render targets in DEFAULT
    if (IsRenderTarget())
      pool = d3d9::D3DPOOL_DEFAULT;
    // Not sure if this all that good for perf in our case...
    //else if (m_desc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
      //pool = d3d9::D3DPOOL_SYSTEMMEM;

    auto rawMips = m_desc.dwMipMapCount + 1;
    uint32_t mips = std::min(static_cast<uint32_t>(rawMips), caps7::MaxMipLevels);

    // Render Target / various base surface types
    if (IsRenderTarget()) {
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

      else {
        hr = m_d3d7device->GetD3D9()->CreateRenderTarget(
          m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
          d3d9::D3DMULTISAMPLE_NONE, 0, FALSE, &rt, nullptr);
        if (likely(SUCCEEDED(hr)))
          Logger::info("DDraw7Surface::IntializeD3D9: Created generic RT");
      }

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create RT");
        return hr;
      }

      m_d3d9 = std::move(rt);
    // Depth Stencil
    } else if (IsDepthStencil()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing depth stencil...");

      Com<d3d9::IDirect3DSurface9> ds = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateDepthStencilSurface(
        m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
        d3d9::D3DMULTISAMPLE_NONE, 0, FALSE, &ds, nullptr);
      Logger::info("DDraw7Surface::IntializeD3D9: Created DS");

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create DS");
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
        m_desc.dwWidth, mips, IsRenderTarget() ? D3DUSAGE_RENDERTARGET : 0,
        ConvertFormat(m_desc.ddpfPixelFormat), pool, &cubetex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create cube map");
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
        m_desc.dwWidth, m_desc.dwHeight, mips, 0,
        ConvertFormat(m_desc.ddpfPixelFormat), pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::IntializeD3D9: Created d3d9 texture");
      m_texture = std::move(tex);
    // Something... else?
    } else {
      Logger::warn("DDraw7Surface::IntializeD3D9: Initializing unknown/unhandled surface type");

      Com<d3d9::IDirect3DSurface9> rt = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateRenderTarget(
          m_desc.dwWidth, m_desc.dwHeight, ConvertFormat(m_desc.ddpfPixelFormat),
          d3d9::D3DMULTISAMPLE_NONE, 0, FALSE, &rt, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create RT");
        return hr;
      } else {
        Logger::info("DDraw7Surface::IntializeD3D9: Created a generic RT");
      }
    }

    UploadTextureData();

    return DD_OK;
  }

  // TODO: Blit more effectively, rather than with this silly GDI stuff
  inline HRESULT DDraw7Surface::UploadTextureData() {
    Logger::info(str::format("DDraw7Surface::UploadTextureData: Uploading nr. [[[", m_surfCount, "]]]"));

    // Nothing to upload
    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw7Surface::UploadTextureData: No wrapped surface or texture");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw7Surface::UploadTextureData: Surface has 0 height or width");
      return DD_OK;
    }

    HRESULT hr;

    // Get ddraw surface DC
    HDC dc7;
    hr = this->GetDC(&dc7);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::UploadTextureData: Failed GetDC for main surface");
      return hr;
    }

    Com<d3d9::IDirect3DSurface9> level = nullptr;

    // TODO: Handle uploading all cubemap faces

    // Blit all the mips for textures
    if (m_texture != nullptr) {
      auto rawMips = m_desc.dwMipMapCount + 1;
      uint32_t mips = std::min(static_cast<uint32_t>(rawMips), caps7::MaxMipLevels);

      for (uint32_t i = 0; i < mips; i++) {
        hr = m_texture->GetSurfaceLevel(i, &level);
        if (unlikely(FAILED(hr))) {
          Logger::warn(str::format("DDraw7Surface::UploadTextureData: Failed to get surface for level ", i));
          continue;
        }

        HDC levelDC;
        hr = level->GetDC(&levelDC);
        if (unlikely(FAILED(hr))) {
          Logger::warn(str::format("DDraw7Surface::UploadTextureData: Failed GetDC for mip level ", i));
          continue;
        }

        BitBlt(levelDC, 0, 0, m_desc.dwWidth, m_desc.dwHeight, dc7, 0, 0, SRCCOPY);

        level->ReleaseDC(levelDC);
      }
    // Blit surfaces directly
    // TODO: does this even work with depth stencils and other misc types?
    } else if (m_d3d9 != nullptr) {
      level = m_d3d9.ptr();

      HDC levelDC;
      hr = level->GetDC(&levelDC);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::UploadTextureData: Failed GetDC for surface");
        return hr;
      }

      BitBlt(levelDC, 0, 0, m_desc.dwWidth, m_desc.dwHeight, dc7, 0, 0, SRCCOPY);

      level->ReleaseDC(levelDC);
    } else {
      // Shouldn't ever happen, we've checked for it above
      Logger::err("DDraw7Surface::UploadTextureData: DDraw7Surface has neither a d3d9 surface or texture attached");
    }

    this->ReleaseDC(dc7);

    Logger::debug("DDraw7Surface::UploadTextureData: Upload complete");

    return DD_OK;
  }

}

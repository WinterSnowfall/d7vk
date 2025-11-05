#include "ddraw7_surface.h"

#include "d3d7_caps.h"

namespace dxvk {

  uint32_t DDraw7Surface::s_surfCount = 0;

  DDraw7Surface::DDraw7Surface(
        Com<IDirectDrawSurface7>&& surfProxy,
        DDraw7Interface* pParent,
        DDraw7Surface* pParentSurf,
        DDSURFACEDESC2 desc,
        bool isChildObject)
    : DDrawWrappedObject<d3d9::IDirect3DSurface9, IDirectDrawSurface7>(nullptr, std::move(surfProxy))
    , m_isChildObject ( isChildObject )
    , m_parent     ( pParent )
    , m_parentSurf ( pParentSurf )
    , m_desc       ( desc )
    , m_format     ( ConvertFormat(desc.ddpfPixelFormat) ) {
    if (likely(m_isChildObject))
      m_parent->AddRef();

    m_parent->AddWrappedSurface(this);

    m_surfCount = ++s_surfCount;

    ListSurfaceDetails();
  }

  DDraw7Surface::~DDraw7Surface() {
    m_parent->RemoveWrappedSurface(this);

    Logger::debug(str::format("DDraw7Surface: Surface nr. [[", m_surfCount, "]] bites the dust"));

    if (likely(m_isChildObject))
      m_parent->Release();
  }

  // This call will only attach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  // More than that, the attached surfaces do not need to be manageed by the object, the docs state:
  // "Unlike complex surfaces that you create with a single call to IDirectDraw7::CreateSurface, surfaces
  // attached with this method are not automatically released."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::AddAttachedSurface: Proxy");

    if (unlikely(lpDDSAttachedSurface == nullptr)) {
      Logger::err("DDraw7Surface::AddAttachedSurface: Called with NULL surface");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::warn("DDraw7Surface::AddAttachedSurface: Attaching unwrapped surface");
      return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);
    ddraw7Surface->SetParentSurface(this);
    m_depthStencil = ddraw7Surface;
    return m_proxy->AddAttachedSurface(ddraw7Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw7Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw7Surface::Blt: Proxy");

    RefreshD3D7Device();
    if (m_d3d7device != nullptr && m_d3d7device->HasDrawn() && (IsFrontBuffer() || IsBackBuffer()))
      return DD_OK;

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw7Surface::Blt: Received an unwrapped source surface");
      hr = m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->Blt(lpDestRect, ddraw7Surface->GetProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Blt: Failed upload to d3d9 surface");
      }
    }

    return hr;
  }

  // Allegedly unimplemented, according to the official docs
  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Surface::BltBatch: Proxy");
    return m_proxy->BltBatch(lpDDBltBatch, dwCount, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw7Surface::BltFast: Proxy");

    RefreshD3D7Device();
    if (m_d3d7device != nullptr && m_d3d7device->HasDrawn() && (IsFrontBuffer() || IsBackBuffer()))
      return DD_OK;

    HRESULT hr;

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSrcSurface))) {
      if (unlikely(lpDDSrcSurface != nullptr))
        Logger::warn("DDraw7Surface::BltFast: Received an unwrapped source surface");
      hr = m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = m_proxy->BltFast(dwX, dwY, ddraw7Surface->GetProxied(), lpSrcRect, dwTrans);
    }

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::BltFast: Failed upload to d3d9 surface");
      }
    }

    return hr;
  }

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::DeleteAttachedSurface: Proxy");

    // If lpDDSAttachedSurface is NULL, then all surfaces are detached.
    if (unlikely(!m_parent->IsWrappedSurface(lpDDSAttachedSurface))) {
      if (unlikely(lpDDSAttachedSurface != nullptr))
        Logger::warn("DDraw7Surface::DeleteAttachedSurface: Deleting unwrapped surface");
      return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);
    if (likely(m_depthStencil == ddraw7Surface)) {
      ddraw7Surface->ClearParentSurface();
      m_depthStencil = nullptr;
    }
    return m_proxy->DeleteAttachedSurface(dwFlags, ddraw7Surface->GetProxied());
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
      Logger::debug("DDraw7Surface::Flip: Use of non-NULL lpDDSurfaceTargetOverride");

    RefreshD3D7Device();
    if (likely(m_d3d7device != nullptr)) {
      m_d3d7device->ResetDrawTracking();
      m_d3d7device->GetD3D9()->Present(NULL, NULL, NULL, NULL);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw7Surface::GetAttachedSurface: Proxy");

    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for the front buffer");
    else if (lpDDSCaps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP))
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for the back buffer");
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
    else if (lpDDSCaps->dwCaps2 & DDSCAPS_OVERLAY)
      Logger::debug("DDraw7Surface::GetAttachedSurface: Querying for an overlay");

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
      Logger::debug("DDraw7Surface::GetAttachedSurface: Got a new unwrapped surface");
      DDSURFACEDESC2 desc;
      desc.dwSize = sizeof(DDSURFACEDESC2);
      surface->GetSurfaceDesc(&desc);
      Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(std::move(surface), m_parent, this, desc, false);
      m_attachedSurfaces.push_back(ddraw7Surface.ptr());
      // Do NOT ref here since we're managing the attached object lifecycle
      *lplpDDAttachedSurface = ddraw7Surface.ptr();
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

    *lpDDSCaps = m_desc.ddsCaps;

    return DD_OK;
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

    RefreshD3D7Device();
    if (unlikely(m_d3d7device != nullptr && m_d3d7device->GetOptions()->proxiedGetDC))
      return m_proxy->GetDC(lphDC);

    if (unlikely(lphDC == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw7Surface::GetDC: Not yet initialized");
      return m_proxy->GetDC(lphDC);
    }

    HRESULT hr = m_d3d9->GetDC(lphDC);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Surface::GetDC: Failed to get d3d9 DC");
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
    // This is NOT the desc that was passed during surface creation
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
    // TODO: Directly lock d3d9 surfaces and skip surface uploads on unlock,
    // but it can get very involved, especially when dealing with DXT formats.
    Logger::debug("<<< DDraw7Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw7Surface::ReleaseDC");

    RefreshD3D7Device();
    if (unlikely(m_d3d7device != nullptr && m_d3d7device->GetOptions()->proxiedGetDC))
      return m_proxy->ReleaseDC(hDC);

    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw7Surface::ReleaseDC: Not yet initialized");
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

    HRESULT hr = m_proxy->SetClipper(lpDDClipper);

    if (likely(SUCCEEDED(hr) && lpDDClipper != nullptr)) {
      // A few games apparently call SetCooperativeLevel AFTER creating the
      // d3d device, let alone the interface, which is technically illegal,
      // but hey, like that ever stopped anyone...
      if (unlikely(IsRenderTarget() && m_parent->GetHWND() == nullptr)) {
        Logger::debug("DDraw7Surface::SetClipper: Using hwnd from clipper");
        HWND hwnd;
        lpDDClipper->GetHWnd(&hwnd);
        m_parent->SetHWND(hwnd);
      }
    }

    return hr;
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

    // Note: Unfortunately, some applications write outside of locks too,
    // so we will always need to upload texture and mip map data on SetTexture
    HRESULT hr = m_proxy->Unlock(lpSurfaceData);

    if (likely(SUCCEEDED(hr))) {
      // Textures and cubemaps get uploaded during SetTexture calls
      if (!IsTextureOrCubeMap()) {
        HRESULT hrUpload = InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrUpload)))
          Logger::warn("DDraw7Surface::Unlock: Failed upload to d3d9 surface");
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw7Surface::UpdateOverlay: Proxy");

    if (unlikely(!m_parent->IsWrappedSurface(lpDDDestSurface))) {
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

    if (unlikely(!m_parent->IsWrappedSurface(lpDDSReference))) {
      Logger::warn("DDraw7Surface::UpdateOverlayZOrder: Called with an unwrapped surface");
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSReference);
    return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw7Surface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDDInterface(void **lplpDD) {
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
    return m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
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

  HRESULT DDraw7Surface::InitializeOrUploadD3D9() {
    HRESULT hr = DDERR_GENERIC;

    RefreshD3D7Device();

    if (likely(IsInitialized()))
      hr = UploadSurfaceData();
    else
      hr = IntializeD3D9();

    return hr;
  }

  // Callback function used in cube map face/surface initialization
  inline HRESULT STDMETHODCALLTYPE EnumAndAttachSurfacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    d3d9::IDirect3DCubeTexture9* cube = static_cast<d3d9::IDirect3DCubeTexture9*>(ctx);

    // Skip zbuffer. (Are we expecting a Z-buffer by default? Just for RT?)
    if (unlikely(desc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER))
      return DDENUMRET_OK;

    Com<d3d9::IDirect3DSurface9> face = nullptr;
    cube->GetCubeMapSurface(GetCubemapFace(desc), 0, &face);

    DDraw7Surface* ddraw7surface = static_cast<DDraw7Surface*>(subsurf);
    ddraw7surface->SetSurface(std::move(face));

    return DDENUMRET_OK;
  }

  inline HRESULT DDraw7Surface::IntializeD3D9() {
    Logger::debug(str::format("DDraw7Surface::IntializeD3D9: Initializing nr. [[", m_surfCount, "]]"));

    if (m_d3d7device == nullptr) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Null D3D7 device, can't initalize right now");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Surface has 0 height or width");
      return DD_OK;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(m_format == d3d9::D3DFMT_P8)) {
      Logger::warn("DDraw7Surface::IntializeD3D9: Unsupported format D3DFMT_P8");
      return DD_OK;
    }

    HRESULT hr;

    // In some cases we get passed offscreen plain surfaces with no data whatsoever in
    // ddpfPixelFormat, so we need to fall back to whatever the d3d9 back buffer is using.
    if (unlikely(m_format == d3d9::D3DFMT_UNKNOWN)) {
      Com<d3d9::IDirect3DSurface9> backBuffer;
      d3d9::D3DSURFACE_DESC bbDesc;

      hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &backBuffer);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to get d3d9 back buffer");
        return hr;
      }

      hr = backBuffer->GetDesc(&bbDesc);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to determine format for offscreen plain surface");
        return hr;
      }

      m_format = bbDesc.Format;
      Logger::debug(str::format("DDraw7Surface::IntializeD3D9: Offscreen plain surface format set to ", m_format));
    }

    d3d9::D3DPOOL   pool   = d3d9::D3DPOOL_DEFAULT;
    DWORD           usage  = 0;

    // General surface/texture pool placement
    if (m_desc.ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)
      pool = d3d9::D3DPOOL_DEFAULT;
    else if (m_desc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
      // We can't know beforehand if a texture is or isn't going to be
      // used in SetTexture() calls, and textures placed in D3DPOOL_SYSTEMMEM
      // will not work in that context in dxvk, so revert to D3DPOOL_MANAGED.
      pool = IsTexture() ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_SYSTEMMEM;
    // There's no explicit non-local video memory placement
    // per se, but D3DPOOL_MANAGED is close enough
    else if ((m_desc.ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM) || (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE))
      pool = d3d9::D3DPOOL_MANAGED;

    // Place all possible render targets in DEFAULT,
    // as well as any cube maps and overlays
    if (IsRenderTarget() || IsCubeMap() || IsOverlay()) {
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage = D3DUSAGE_RENDERTARGET;
    }
    // Should already be the case, but let's make doubly sure
    if (IsDXTFormat(m_format)) {
      pool  = d3d9::D3DPOOL_DEFAULT;
    }

    // General usage flags
    if (IsTexture() && pool == d3d9::D3DPOOL_DEFAULT)
      usage = D3DUSAGE_DYNAMIC;

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" :
                                pool == d3d9::D3DPOOL_SYSTEMMEM ? "D3DPOOL_SYSTEMMEM" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("DDraw7Surface::IntializeD3D9: Placing in: ", poolPlacement));

    d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3d9::D3DMULTISAMPLE_NONE;

    RefreshD3D7Device();
    if (likely(m_d3d7device != nullptr)) {
      const int32_t forceMSAA = m_d3d7device->GetOptions()->forceMSAA;

      if (unlikely(forceMSAA != -1)) {
        multiSampleType = d3d9::D3DMULTISAMPLE_TYPE(std::min<uint32_t>(8u, forceMSAA));
      }
    }

    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guess it was intended more a hint, not neceesarily how many mips ended up on the GPU.

    IDirectDrawSurface7* mipMap = m_proxy.ptr();

    while (mipMap != nullptr) {
      IDirectDrawSurface7* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
      if (mipMap != nullptr) {
        m_mipCount++;
      }
    }

    const uint32_t mipLevels = std::min(static_cast<uint32_t>(m_mipCount + 1), caps7::MaxMipLevels);
    if (mipLevels > 1) {
      Logger::debug(str::format("DDraw7Surface::IntializeD3D9: Found ", mipLevels, " mip levels"));

      if (unlikely(mipLevels != m_desc.dwMipMapCount))
        Logger::debug(str::format("DDraw7Surface::IntializeD3D9: Mismatch with declared ", m_desc.dwMipMapCount, "mip levels"));
    }

    // Render targets / various base surface types
    if (IsRenderTarget() || IsOverlay()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing render target...");

      Com<d3d9::IDirect3DSurface9> rt = nullptr;

      if (IsFrontBuffer()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::debug("DDraw7Surface::IntializeD3D9: Retrieved front buffer surface");
      }

      else if (IsBackBuffer()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::debug("DDraw7Surface::IntializeD3D9: Retrieved back buffer surface");
      }

      else if (IsOffScreenPlainSurface()) {
        // Sometimes we get passed offscreen plain surfaces which should be tied to the back buffer
        if (unlikely(m_d3d7device->GetRenderTarget() == this)) {
          Logger::debug("DDraw7Surface::IntializeD3D9: Unknown surface is the current RT");

          hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);

          if (unlikely(FAILED(hr))) {
            Logger::err("DDraw7Surface::IntializeD3D9: Failed to retrieve back buffer");
            m_d3d9 = nullptr;
            return hr;
          }

          Logger::debug("DDraw7Surface::IntializeD3D9: Retrieved back buffer surface");
        } else {
          hr = m_d3d7device->GetD3D9()->CreateOffscreenPlainSurface(
            m_desc.dwWidth, m_desc.dwHeight, m_format,
            pool, &rt, nullptr);

          if (likely(SUCCEEDED(hr)))
            Logger::debug("DDraw7Surface::IntializeD3D9: Created offscreen plain surface");
        }
      }

      else if (IsOverlay()) {
        hr = m_d3d7device->GetD3D9()->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &rt);
        if (likely(SUCCEEDED(hr)))
          Logger::debug("DDraw7Surface::IntializeD3D9: Retrieved overlay surface");
      }

      else {
        // Must be lockable for blitting to work
        hr = m_d3d7device->GetD3D9()->CreateRenderTarget(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          multiSampleType, usage, TRUE, &rt, nullptr);
        if (likely(SUCCEEDED(hr)))
          Logger::debug("DDraw7Surface::IntializeD3D9: Created generic RT");
      }

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create RT");
        m_d3d9 = nullptr;
        return hr;
      }

      m_d3d9 = std::move(rt);

    // Textures
    } else if (IsTexture()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing a texture...");

      Com<d3d9::IDirect3DTexture9> tex = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateTexture(
        m_desc.dwWidth, m_desc.dwHeight, mipLevels, usage,
        m_format, pool, &tex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create texture");
        m_texture = nullptr;
        return hr;
      }

      // Attach level 0 to this surface
      Com<d3d9::IDirect3DSurface9> level = nullptr;
      tex->GetSurfaceLevel(0, &level);
      m_d3d9 = (std::move(level));

      Logger::debug("DDraw7Surface::IntializeD3D9: Created d3d9 texture");
      m_texture = std::move(tex);

    // Depth Stencil
    } else if (IsDepthStencil()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing depth stencil...");

      Com<d3d9::IDirect3DSurface9> ds = nullptr;

      hr = m_d3d7device->GetD3D9()->CreateDepthStencilSurface(
        m_desc.dwWidth, m_desc.dwHeight, m_format,
        multiSampleType, 0, FALSE, &ds, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create DS");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::IntializeD3D9: Created depth stencil surface");

      m_d3d9 = std::move(ds);
    // Cube maps
    } else if (IsCubeMap()) {
      Logger::debug("DDraw7Surface::IntializeD3D9: Initializing cube map...");

      Com<d3d9::IDirect3DCubeTexture9> cubetex = nullptr;
      hr = m_d3d7device->GetD3D9()->CreateCubeTexture(
        m_desc.dwWidth, mipLevels, usage,
        m_format, pool, &cubetex, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create cube map");
        m_cubeMap = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::IntializeD3D9: Created cube map");

      // Attach face 0 to this surface
      Com<d3d9::IDirect3DSurface9> face = nullptr;
      cubetex->GetCubeMapSurface((d3d9::D3DCUBEMAP_FACES)0, 0, &face);
      m_d3d9 = (std::move(face));

      // Attach sides 1-5 to each attached surface
      m_proxy->EnumAttachedSurfaces(cubetex.ptr(), EnumAndAttachSurfacesCallback);

      m_cubeMap = std::move(cubetex);

    } else {
      Logger::err("DDraw7Surface::IntializeD3D9: Unknown surface type");

      Com<d3d9::IDirect3DSurface9> surf = nullptr;
      // D3DPOOL_SCRATCH allows the creation of surfaces with unsupported formats
      hr = m_d3d7device->GetD3D9()->CreateOffscreenPlainSurface(
          m_desc.dwWidth, m_desc.dwHeight, m_format,
          d3d9::D3DPOOL_SCRATCH, &surf, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Surface::IntializeD3D9: Failed to create offscreen plain surface");
        m_d3d9 = nullptr;
        return hr;
      }

      Logger::debug("DDraw7Surface::IntializeD3D9: Created offscreen plain surface");

      m_d3d9 = std::move(surf);
    }

    UploadSurfaceData();

    return DD_OK;
  }

  inline HRESULT DDraw7Surface::UploadSurfaceData() {
    Logger::debug(str::format("DDraw7Surface::UploadSurfaceData: Uploading nr. [[", m_surfCount, "]]"));

    if (m_d3d7device->HasDrawn() && (IsFrontBuffer() || IsBackBuffer()))
      return DD_OK;

    // Nothing to upload
    if (unlikely(!IsInitialized())) {
      Logger::warn("DDraw7Surface::UploadSurfaceData: No wrapped surface or texture");
      return DD_OK;
    }

    if (unlikely(m_desc.dwHeight == 0 || m_desc.dwWidth == 0)) {
      Logger::warn("DDraw7Surface::UploadSurfaceData: Surface has 0 height or width");
      return DD_OK;
    }

    // Blit all the mips for textures
    if (IsTexture()) {
      const uint32_t mipLevels = std::min(static_cast<uint32_t>(m_mipCount + 1), caps7::MaxMipLevels);
      BlitToD3D9Texture(m_texture.ptr(), m_proxy.ptr(), mipLevels, IsDXTFormat(m_format));
    // TODO: Handle uploading all cubemap faces
    } else if (unlikely(IsCubeMap())) {
      Logger::warn("DDraw7Surface::UploadSurfaceData: Unhandled upload of cube map");
    } else if (unlikely(IsDepthStencil())) {
      Logger::debug("DDraw7Surface::UploadSurfaceData: Skipping upload of depth stencil");
    // Blit surfaces directly
    } else if (likely(m_d3d9 != nullptr)) {
      BlitToD3D9Surface(m_d3d9.ptr(), m_proxy.ptr(), IsDXTFormat(m_format));
    }

    return DD_OK;
  }

}

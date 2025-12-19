#include "ddraw4_surface.h"

#include "ddraw4_interface.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDraw4Surface::s_surfCount = 0;

  DDraw4Surface::DDraw4Surface(
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw7Surface* origin)
    : DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_origin ( origin ) {
    if (unlikely(!IsLegacyInterface()))
      m_parent->AddWrappedSurface(this);

    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", m_surfCount, "]]"));
  }

  DDraw4Surface::~DDraw4Surface() {
    if (unlikely(!IsLegacyInterface()))
      m_parent->RemoveWrappedSurface(this);

    Logger::debug(str::format("DDraw4Surface: Surface nr. [[4-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface4)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw4Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw4Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (likely(IsLegacyInterface())) {
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface3");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw4Surface::QueryInterface: Query for IDirect3DTexture");
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw4Surface::AddAttachedSurface: Proxy");
    return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw4Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw4Surface::Blt: Proxy");
    return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw4Surface::BltBatch");
    return m_proxy->BltBatch(lpDDBltBatch, dwCount, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw4Surface::BltFast: Proxy");
    return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw4Surface::DeleteAttachedSurface: Proxy");
    return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw4Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback) {
    Logger::debug("<<< DDraw4Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("*** DDraw4Surface::Flip: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface) {
    if (likely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Surface::GetAttachedSurface: Proxy");
      return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
    }

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
    else if (lpDDSCaps->dwCaps2 & DDSCAPS2_CUBEMAP)
      Logger::debug("DDraw4Surface::GetAttachedSurface: Querying for a cube map");
    else if (lpDDSCaps->dwCaps2 & DDSCAPS_OVERLAY)
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

    Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(std::move(surface), m_parent, nullptr);
    *lplpDDAttachedSurface = ddraw4Surface.ref();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetCaps: Forwarded");
      return m_origin->GetCaps(lpDDSCaps);
    }

    Logger::debug("<<< DDraw4Surface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug("<<< DDraw4Surface::GetClipper: Proxy");
    return m_proxy->GetClipper(lplpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw4Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDC(HDC *lphDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetDC: Forwarded");
      return m_origin->GetDC(lphDC);
    }

    Logger::debug("<<< DDraw4Surface::GetDC: Proxy");
    return m_proxy->GetDC(lphDC);
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
    Logger::debug("<<< DDraw4Surface::GetPalette: Proxy");
    return m_proxy->GetPalette(lplpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetPixelFormat: Forwarded");
      return m_origin->GetPixelFormat(lpDDPixelFormat);
    }

    Logger::debug("<<< DDraw4Surface::GetPixelFormat: Proxy");
    return m_proxy->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::GetSurfaceDesc: Forwarded");
      return m_origin->GetSurfaceDesc(lpDDSurfaceDesc);
    }

    Logger::debug("<<< DDraw4Surface::GetSurfaceDesc: Proxy");
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw4Surface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::IsLost() {
    Logger::debug("<<< DDraw4Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::Lock: Forwarded");
      return m_origin->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    }

    Logger::debug("<<< DDraw4Surface::Lock: Proxy");
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ReleaseDC(HDC hDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::ReleaseDC: Forwarded");
      return m_origin->ReleaseDC(hDC);
    }

    Logger::debug("<<< DDraw4Surface::ReleaseDC: Proxy");
    return m_proxy->ReleaseDC(hDC);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Restore() {
    Logger::debug("<<< DDraw4Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw4Surface::SetClipper: Proxy");
    return m_proxy->SetClipper(lpDDClipper);
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
    return m_proxy->SetPalette(lpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Unlock(LPRECT lpSurfaceData) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Surface::Unlock: Forwarded");
      return m_origin->Unlock(lpSurfaceData);
    }

    Logger::debug("<<< DDraw4Surface::Unlock: Proxy");
    return m_proxy->Unlock(lpSurfaceData);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference) {
    Logger::debug("<<< DDraw4Surface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDDInterface(LPVOID *lplpDD) {
    Logger::warn("<<< DDraw4Surface::GetDDInterface: Proxy");
    return m_proxy->GetDDInterface(lplpDD);
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
    return m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
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

}

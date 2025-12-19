#include "ddraw3_surface.h"

#include "ddraw2_interface.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDraw3Surface::s_surfCount = 0;

  DDraw3Surface::DDraw3Surface(
        Com<IDirectDrawSurface3>&& surfProxy,
        DDrawInterface* pParent,
        DDraw7Surface* origin)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>(pParent, std::move(surfProxy), nullptr)
    , m_origin ( origin ) {
    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw3Surface: Created a new surface nr. [[3-", m_surfCount, "]]"));
  }

  DDraw3Surface::~DDraw3Surface() {
    Logger::debug(str::format("DDraw3Surface: Surface nr. [[3-", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDrawSurface3, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw3Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw3Surface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw3Surface::QueryInterface");

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
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface4");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface4");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw3Surface::QueryInterface: Query for IDirectDrawSurface7");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture)
              || riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("DDraw3Surface::QueryInterface: Query for IDirect3DTexture");
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

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw3Surface::AddAttachedSurface: Proxy");
    return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw3Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw3Surface::Blt: Proxy");
    return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw3Surface::BltBatch");
    return m_proxy->BltBatch(lpDDBltBatch, dwCount, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw3Surface::BltFast: Proxy");
    return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw3Surface::DeleteAttachedSurface: Proxy");
    return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw3Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDraw3Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Flip(LPDIRECTDRAWSURFACE3 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("*** DDraw3Surface::Flip: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE3 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw3Surface::GetAttachedSurface: Proxy");
    return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug("<<< DDraw3Surface::GetCaps: Proxy");
    // TODO: Convert LPDDSCAPS <-> LPDDSCAPS2
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug("<<< DDraw3Surface::GetClipper: Proxy");
    return m_proxy->GetClipper(lplpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw3Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetDC(HDC *lphDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::GetDC: Forwarded");
      return m_origin->GetDC(lphDC);
    }

    Logger::debug("<<< DDraw3Surface::GetDC: Proxy");
    return m_proxy->GetDC(lphDC);
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
    Logger::debug("<<< DDraw3Surface::GetPalette: Proxy");
    return m_proxy->GetPalette(lplpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::GetPixelFormat: Forwarded");
      return m_origin->GetPixelFormat(lpDDPixelFormat);
    }

    Logger::debug("<<< DDraw3Surface::GetPixelFormat: Proxy");
    return m_proxy->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw3Surface::GetSurfaceDesc: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw3Surface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::IsLost() {
    Logger::debug("<<< DDraw3Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw3Surface::Lock: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::ReleaseDC(HDC hDC) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw3Surface::ReleaseDC: Forwarded");
      return m_origin->ReleaseDC(hDC);
    }

    Logger::debug("<<< DDraw3Surface::ReleaseDC: Proxy");
    return m_proxy->ReleaseDC(hDC);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Restore() {
    Logger::debug("<<< DDraw3Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw3Surface::SetClipper: Proxy");
    return m_proxy->SetClipper(lpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw3Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw3Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw3Surface::SetPalette: Proxy");
    return m_proxy->SetPalette(lpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::Unlock(LPVOID lpSurfaceData) {
    Logger::debug("<<< DDraw3Surface::Unlock: Proxy");
    return m_proxy->Unlock(lpSurfaceData);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE3 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSReference) {
    Logger::debug("<<< DDraw3Surface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

  HRESULT STDMETHODCALLTYPE DDraw3Surface::GetDDInterface(LPVOID *lplpDD) {
    Logger::warn("<<< DDraw3Surface::GetDDInterface: Proxy");
    return m_proxy->GetDDInterface(lplpDD);
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
    return m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
  }

}

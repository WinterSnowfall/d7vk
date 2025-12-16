#include "ddraw2_surface.h"

#include "ddraw2_interface.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t DDraw2Surface::s_surfCount = 0;

  DDraw2Surface::DDraw2Surface(
        Com<IDirectDrawSurface2>&& surfProxy,
        DDraw7Surface* origin)
    : DDrawWrappedObject<DDraw2Interface, IDirectDrawSurface2, d3d9::IDirect3DSurface9>(nullptr, std::move(surfProxy), nullptr)
    , m_origin ( origin ) {
    m_surfCount = ++s_surfCount;

    Logger::debug(str::format("DDraw2Surface: Created a new surface nr. [[", m_surfCount, "]]:"));
  }

  DDraw2Surface::~DDraw2Surface() {
    Logger::debug(str::format("DDraw2Surface: Surface nr. [[", m_surfCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw2Interface, IDirectDrawSurface2, d3d9::IDirect3DSurface9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawSurface2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw2Surface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("DDraw2Surface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw2Surface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDrawGammaControl)) {
      return m_origin->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return m_origin->QueryInterface(riid, ppvObject);
    }

    // Some games query for legacy ddraw surfaces
    if (unlikely(riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for legacy IDirectDrawSurface");
      return m_origin->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2))) {
      Logger::debug("DDraw2Surface::QueryInterface: Query for legacy IDirectDraw");
      return m_origin->QueryInterface(riid, ppvObject);
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
    return m_proxy->AddAttachedSurface(lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    Logger::debug("<<< DDraw2Surface::AddOverlayDirtyRect: Proxy");
    return m_proxy->AddOverlayDirtyRect(lpRect);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    Logger::debug("<<< DDraw2Surface::Blt: Proxy");
    return m_proxy->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    Logger::debug(">>> DDraw2Surface::BltBatch");
    return m_proxy->BltBatch(lpDDBltBatch, dwCount, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    Logger::debug("<<< DDraw2Surface::BltFast: Proxy");
    return m_proxy->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    Logger::debug("<<< DDraw2Surface::DeleteAttachedSurface: Proxy");
    return m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw2Surface::EnumAttachedSurfaces: Proxy");
    return m_proxy->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    Logger::debug("<<< DDraw2Surface::EnumOverlayZOrders: Proxy");
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Flip(LPDIRECTDRAWSURFACE2 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    Logger::debug("*** DDraw2Surface::Flip: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE2 *lplpDDAttachedSurface) {
    Logger::debug("<<< DDraw2Surface::GetAttachedSurface: Proxy");
    return m_proxy->GetAttachedSurface(lpDDSCaps, lplpDDAttachedSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetBltStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::GetBltStatus: Proxy");
    return m_proxy->GetBltStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    Logger::debug("<<< DDraw2Surface::GetCaps: Proxy");
    // TODO: Convert LPDDSCAPS <-> LPDDSCAPS2
    return m_proxy->GetCaps(lpDDSCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    Logger::debug("<<< DDraw2Surface::GetClipper: Proxy");
    return m_proxy->GetClipper(lplpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw2Surface::GetColorKey: Proxy");
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDC(HDC *lphDC) {
    Logger::debug(">>> DDraw2Surface::GetDC: Forwarded");
    return m_origin->GetDC(lphDC);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetFlipStatus(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::GetFlipStatus: Proxy");
    return m_proxy->GetFlipStatus(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    Logger::debug("<<< DDraw2Surface::GetOverlayPosition: Proxy");
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    Logger::debug("<<< DDraw2Surface::GetPalette: Proxy");
    return m_proxy->GetPalette(lplpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    Logger::debug(">>> DDraw2Surface::GetPixelFormat: Forwarded");
    return m_origin->GetPixelFormat(lpDDPixelFormat);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw2Surface::GetSurfaceDesc: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->GetSurfaceDesc(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw2Surface::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::IsLost() {
    Logger::debug("<<< DDraw2Surface::IsLost: Proxy");
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw2Surface::Lock: Proxy");
    //TODO: Convert between LPDDSURFACEDESC <-> LPDDSURFACEDESC2
    return m_proxy->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::ReleaseDC(HDC hDC) {
    Logger::debug(">>> DDraw2Surface::ReleaseDC: Forwarded");
    return m_origin->ReleaseDC(hDC);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Restore() {
    Logger::debug("<<< DDraw2Surface::Restore: Proxy");
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    Logger::debug("<<< DDraw2Surface::SetClipper: Proxy");
    return m_proxy->SetClipper(lpDDClipper);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    Logger::debug("<<< DDraw2Surface::SetColorKey: Proxy");
    return m_proxy->SetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetOverlayPosition(LONG lX, LONG lY) {
    Logger::debug("<<< DDraw2Surface::SetOverlayPosition: Proxy");
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    Logger::debug("<<< DDraw2Surface::SetPalette: Proxy");
    return m_proxy->SetPalette(lpDDPalette);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Unlock(LPVOID lpSurfaceData) {
    Logger::debug("<<< DDraw2Surface::Unlock: Proxy");
    return m_proxy->Unlock(lpSurfaceData);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE2 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlay: Proxy");
    return m_proxy->UpdateOverlay(lpSrcRect, lpDDDestSurface, lpDestRect, dwFlags, lpDDOverlayFx);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlayDisplay: Proxy");
    return m_proxy->UpdateOverlayDisplay(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSReference) {
    Logger::debug("<<< DDraw2Surface::UpdateOverlayZOrder: Proxy");
    return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDDInterface(LPVOID *lplpDD) {
    Logger::warn("<<< DDraw2Surface::GetDDInterface: Proxy");
    return m_proxy->GetDDInterface(lplpDD);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageLock(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::PageLock: Proxy");
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageUnlock(DWORD dwFlags) {
    Logger::debug("<<< DDraw2Surface::PageUnlock: Proxy");
    return m_proxy->PageUnlock(dwFlags);
  }

}

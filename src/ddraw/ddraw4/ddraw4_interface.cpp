#include "ddraw4_interface.h"

#include "../ddraw7/ddraw7_interface.h"

namespace dxvk {

  uint32_t DDraw4Interface::s_intfCount = 0;

  DDraw4Interface::DDraw4Interface(Com<IDirectDraw4>&& proxyIntf, DDraw7Interface* origin)
    : DDrawWrappedObject<IUnknown, IDirectDraw4, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_origin ( origin ) {
    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw4Interface: Created a new interface nr. <<4-", m_intfCount, ">>"));
  }

  DDraw4Interface::~DDraw4Interface() {
    Logger::debug(str::format("DDraw4Interface: Interface nr. <<4-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw4, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw4)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw4Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw4Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D interface
    if (riid == __uuidof(IDirect3D3)) {
      Logger::warn("DDraw4Interface::QueryInterface: Query for IDirect3D3");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw4Interface::QueryInterface: Query for legacy IDirectDraw");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        Logger::warn("DDraw4Interface::QueryInterface: Query for legacy IDirectDraw");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
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

  // The documentation states: "The IDirectDraw4::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Interface::Compact() {
    Logger::debug(">>> DDraw4Interface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::CreateClipper: Forwarded");
      return m_origin->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
    }

    Logger::debug("<<< DDraw4Interface::CreateClipper: Proxy");
    return m_proxy->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::CreatePalette: Forwarded");
      return m_origin->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
    }

    Logger::debug("<<< DDraw4Interface::CreatePalette: Proxy");
    return m_proxy->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE4 *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::warn("<<< DDraw4Interface::CreateSurface: Proxy");
    return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::DuplicateSurface(LPDIRECTDRAWSURFACE4 lpDDSurface, LPDIRECTDRAWSURFACE4 *lplpDupDDSurface) {
    Logger::warn("<<< DDraw4Interface::DuplicateSurface: Proxy");
    return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback) {
    Logger::debug("<<< DDraw4Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw4Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::FlipToGDISurface() {
    Logger::debug("*** DDraw4Interface::FlipToGDISurface: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDraw4Interface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw4Interface::GetDisplayMode: Proxy");
    return m_proxy->GetDisplayMode(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::GetFourCCCodes: Forwarded");
      return m_origin->GetFourCCCodes(lpNumCodes, lpCodes);
    }

    Logger::debug("<<< DDraw4Interface::GetFourCCCodes: Proxy");
    return m_proxy->GetFourCCCodes(lpNumCodes, lpCodes);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetGDISurface(LPDIRECTDRAWSURFACE4 *lplpGDIDDSurface) {
    Logger::debug("<<< DDraw4Interface::GetGDISurface: Proxy");
    return m_proxy->GetGDISurface(lplpGDIDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    Logger::debug("<<< DDraw4Interface::GetMonitorFrequency: Proxy");
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetScanLine(LPDWORD lpdwScanLine) {
    Logger::debug("<<< DDraw4Interface::GetScanLine: Proxy");
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    Logger::debug("<<< DDraw4Interface::GetVerticalBlankStatus: Proxy");
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::Initialize(GUID* lpGUID) {
    Logger::debug("<<< DDraw4Interface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreDisplayMode() {
    Logger::debug("<<< DDraw4Interface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::SetCooperativeLevel: Forwarded");
      return m_origin->SetCooperativeLevel(hWnd, dwFlags);
    }

    Logger::debug("<<< DDraw4Interface::SetCooperativeLevel: Proxy");
    return m_proxy->SetCooperativeLevel(hWnd, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Interface::SetDisplayMode: Proxy");
    return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw4Interface::WaitForVerticalBlank: Proxy");
    return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::GetAvailableVidMem: Forwarded");
      return m_origin->GetAvailableVidMem(lpDDCaps, lpdwTotal, lpdwFree);
    }

    Logger::debug("<<< DDraw4Interface::GetAvailableVidMem: Proxy");
    return m_proxy->GetAvailableVidMem(lpDDCaps, lpdwTotal, lpdwFree);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE4 *pSurf) {
    Logger::debug("<<< DDraw4Interface::GetSurfaceFromDC: Proxy");
    return m_proxy->GetSurfaceFromDC(hdc, pSurf);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreAllSurfaces() {
    Logger::debug("<<< DDraw4Interface::RestoreAllSurfaces: Proxy");
    return m_proxy->RestoreAllSurfaces();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::TestCooperativeLevel() {
    Logger::debug("<<< DDraw4Interface::TestCooperativeLevel: Proxy");
    return m_proxy->TestCooperativeLevel();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Interface::GetDeviceIdentifier: Proxy");
    return m_proxy->GetDeviceIdentifier(pDDDI, dwFlags);
  }

}
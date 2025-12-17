#include "ddraw_interface.h"

#include "ddraw7/ddraw7_interface.h"

namespace dxvk {

  uint32_t DDrawInterface::s_intfCount = 0;

  DDrawInterface::DDrawInterface(Com<IDirectDraw>&& proxyIntf, DDraw7Interface* origin)
    : DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_origin ( origin ) {
    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDrawInterface: Created a new interface nr. <<1-", m_intfCount, ">>"));
  }

  DDrawInterface::~DDrawInterface() {
    Logger::debug(str::format("DDrawInterface: Interface nr. <<1-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDrawInterface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawInterface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D interface
    if (riid == __uuidof(IDirect3D3)) {
      Logger::warn("DDrawInterface::QueryInterface: Query for IDirect3D4");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Legacy way of retrieving a D3D7 interface
    if (riid == __uuidof(IDirect3D7)) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirect3D7");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        // TODO: Create the object, similar to how it's done in DDrawInterface7
        Logger::warn("DDrawInterface::QueryInterface: Query for IDirect3D7");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    // Legacy way of getting a DDraw7 interface
    if (riid == __uuidof(IDirectDraw7)) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirectDraw7");
        return m_origin->QueryInterface(riid, ppvObject);
      } else {
        // TODO: Create the object, and pass it on
        Logger::warn("DDrawInterface::QueryInterface: Query for IDirectDraw7");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
    }
    // Standard way of retrieving a D3D interface
    if (riid == __uuidof(IDirect3D)
      ||riid == __uuidof(IDirect3D2)) {
      Logger::warn("DDrawInterface::QueryInterface: Query for IDirect3D");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw2");

      if (unlikely(m_ddraw2Intf == nullptr)) {
        Com<IDirectDraw2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_ddraw2Intf = new DDraw2Interface(std::move(ppvProxyObject), nullptr);
      }

      *ppvObject = m_ddraw2Intf.ref();
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDraw4))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw4");

      if (unlikely(m_ddraw4Intf == nullptr)) {
        Com<IDirectDraw4> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_ddraw4Intf = new DDraw4Interface(std::move(ppvProxyObject), nullptr);
      }

      *ppvObject = m_ddraw4Intf.ref();
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

  // The documentation states: "The IDirectDraw::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDrawInterface::Compact() {
    Logger::debug(">>> DDrawInterface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawInterface::CreateClipper: Forwarded");
      return m_origin->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
    }

    Logger::debug("<<< DDrawInterface::CreateClipper: Proxy");
    return m_proxy->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawInterface::CreatePalette: Forwarded");
      return m_origin->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
    }

    Logger::debug("<<< DDrawInterface::CreatePalette: Proxy");
    return m_proxy->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::warn("<<< DDrawInterface::CreateSurface: Proxy");
    return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface) {
    Logger::warn("<<< DDrawInterface::DuplicateSurface: Proxy");
    return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    Logger::debug("<<< DDrawInterface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDrawInterface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::FlipToGDISurface() {
    Logger::debug("*** DDrawInterface::FlipToGDISurface: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDrawInterface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDrawInterface::GetDisplayMode: Proxy");
    return m_proxy->GetDisplayMode(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawInterface::GetFourCCCodes: Forwarded");
      return m_origin->GetFourCCCodes(lpNumCodes, lpCodes);
    }

    Logger::debug("<<< DDrawInterface::GetFourCCCodes: Proxy");
    return m_proxy->GetFourCCCodes(lpNumCodes, lpCodes);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetGDISurface(LPDIRECTDRAWSURFACE *lplpGDIDDSurface) {
    Logger::debug("<<< DDrawInterface::GetGDISurface: Proxy");
    return m_proxy->GetGDISurface(lplpGDIDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    Logger::debug("<<< DDrawInterface::GetMonitorFrequency: Proxy");
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetScanLine(LPDWORD lpdwScanLine) {
    Logger::debug("<<< DDrawInterface::GetScanLine: Proxy");
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    Logger::debug("<<< DDrawInterface::GetVerticalBlankStatus: Proxy");
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::Initialize(GUID* lpGUID) {
    Logger::debug("<<< DDrawInterface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::RestoreDisplayMode() {
    Logger::debug("<<< DDrawInterface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawInterface::SetCooperativeLevel: Forwarded");
      return m_origin->SetCooperativeLevel(hWnd, dwFlags);
    }

    Logger::debug("<<< DDrawInterface::SetCooperativeLevel: Proxy");
    return m_proxy->SetCooperativeLevel(hWnd, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) {
    Logger::debug("<<< DDrawInterface::SetDisplayMode: Proxy");
    return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDrawInterface::WaitForVerticalBlank: Proxy");
    return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
  }

}
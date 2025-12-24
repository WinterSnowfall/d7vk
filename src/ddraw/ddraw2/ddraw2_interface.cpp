#include "ddraw2_interface.h"

#include "../ddraw_surface.h"
#include "../ddraw_interface.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw4/ddraw4_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  uint32_t DDraw2Interface::s_intfCount = 0;

  DDraw2Interface::DDraw2Interface(Com<IDirectDraw2>&& proxyIntf, DDraw7Interface* origin)
    : DDrawWrappedObject<IUnknown, IDirectDraw2, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_origin ( origin ) {
    if (unlikely(!IsLegacyInterface())) {
      // Initialize the IDirect3D6 interlocked object
      void* d3d6IntfProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirect3D3), &d3d6IntfProxiedVoid);
      Com<IDirect3D3> d3d6IntfProxied = static_cast<IDirect3D3*>(d3d6IntfProxiedVoid);
      m_d3d6Intf = new D3D6Interface(std::move(d3d6IntfProxied), reinterpret_cast<DDraw4Interface*>(this));
    }

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw2Interface: Created a new interface nr. <<2-", m_intfCount, ">>"));
  }

  DDraw2Interface::~DDraw2Interface() {
    Logger::debug(str::format("DDraw2Interface: Interface nr. <<2-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw2Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw2Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw2Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D interface
    if (riid == __uuidof(IDirect3D3)) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDraw2Interface::QueryInterface: Query for IDirect3D3");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      *ppvObject = m_d3d6Intf.ref();
      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDraw2Interface::QueryInterface: Query for legacy IDirectDraw");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw2Interface::QueryInterface: Query for legacy IDirectDraw");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDraw4Interface::QueryInterface: Query for legacy IDirect3D");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::err("DDraw4Interface::QueryInterface: Unsupported IDirect3D interface");
      return E_NOINTERFACE;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDraw4Interface::QueryInterface: Query for legacy IDirect3D2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::err("DDraw4Interface::QueryInterface: Unsupported IDirect3D2 interface");
      return E_NOINTERFACE;
    }
    // Quite a lot of games query for this IID during intro playback
    if (unlikely(riid == GUID_IAMMediaStream)) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IAMMediaStream");
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

  // The documentation states: "The IDirectDraw2::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw2Interface::Compact() {
    Logger::debug(">>> DDraw2Interface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw2Interface::CreateClipper: Forwarded");
      return m_origin->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
    }

    Logger::debug("<<< DDraw2Interface::CreateClipper: Proxy");
    return m_proxy->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw2Interface::CreatePalette: Forwarded");
      return m_origin->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
    }

    Logger::debug("<<< DDraw2Interface::CreatePalette: Proxy");
    return m_proxy->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw2Interface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    if (unlikely(m_d3d6Intf->GetOptions()->proxiedLegacySurfaces)) {
      Logger::debug("<<< DDrawInterface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDraw2Interface::CreateSurface");

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        // TODO: Make away with the reinterpret_cast by having a different constructor, though it should be fine for now
        Com<DDrawSurface> surface = new DDrawSurface(std::move(ddrawSurfaceProxied), reinterpret_cast<DDrawInterface*>(this), nullptr);
        *lplpDDSurface = surface.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    } else {
      Logger::debug("DDraw2Interface::CreateSurface: Failed to create proxy surface");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface) {
    Logger::warn("<<< DDraw2Interface::DuplicateSurface: Proxy");
    return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    Logger::debug("<<< DDraw2Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw2Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::FlipToGDISurface() {
    Logger::debug("*** DDraw2Interface::FlipToGDISurface: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDraw2Interface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw2Interface::GetDisplayMode: Proxy");
    return m_proxy->GetDisplayMode(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw2Interface::GetFourCCCodes: Forwarded");
      return m_origin->GetFourCCCodes(lpNumCodes, lpCodes);
    }

    Logger::debug("<<< DDraw2Interface::GetFourCCCodes: Proxy");
    return m_proxy->GetFourCCCodes(lpNumCodes, lpCodes);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetGDISurface(LPDIRECTDRAWSURFACE *lplpGDIDDSurface) {
    Logger::debug("<<< DDraw2Interface::GetGDISurface: Proxy");
    return m_proxy->GetGDISurface(lplpGDIDDSurface);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    Logger::debug("<<< DDraw2Interface::GetMonitorFrequency: Proxy");
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetScanLine(LPDWORD lpdwScanLine) {
    Logger::debug("<<< DDraw2Interface::GetScanLine: Proxy");
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    Logger::debug("<<< DDraw2Interface::GetVerticalBlankStatus: Proxy");
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::Initialize(GUID* lpGUID) {
    Logger::debug("<<< DDraw2Interface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::RestoreDisplayMode() {
    Logger::debug("<<< DDraw2Interface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw2Interface::SetCooperativeLevel: Forwarded");
      return m_origin->SetCooperativeLevel(hWnd, dwFlags);
    }

    Logger::debug("<<< DDraw2Interface::SetCooperativeLevel: Proxy");
    return m_proxy->SetCooperativeLevel(hWnd, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw2Interface::SetDisplayMode: Proxy");
    return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw2Interface::WaitForVerticalBlank: Proxy");
    return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    Logger::debug("<<< DDraw2Interface::GetAvailableVidMem: Proxy");
    //TODO: Convert between LPDDSCAPS <-> LPDDSCAPS2
    return m_proxy->GetAvailableVidMem(lpDDCaps, lpdwTotal, lpdwFree);
  }

}
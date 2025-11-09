#include "ddraw7_interface.h"

#include "d3d7_device.h"
#include "ddraw7_surface.h"

#include <algorithm>

namespace dxvk {

  uint32_t DDraw7Interface::s_intfCount = 0;

  DDraw7Interface::DDraw7Interface(Com<IDirectDraw7>&& proxyIntf)
    : DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown>(nullptr, std::move(proxyIntf), nullptr) {
    // Initialize a dummy D3D9 interface to retrieve the config options
    m_d3d7ConfigIntf = new D3D7Interface(nullptr, this);

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw7Interface: Created a new interface nr. <<", m_intfCount, ">>"));
  }

  DDraw7Interface::~DDraw7Interface() {
    Logger::debug(str::format("DDraw7Interface: Interface nr. <<", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw7)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw7Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw7Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    *ppvObject = nullptr;

    // Standard way of creating a new D3D7 interface
    if (riid == __uuidof(IDirect3D7)) {
      void* d3d7IntfProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirect3D7), &d3d7IntfProxiedVoid);
      Com<IDirect3D7> d3d7IntfProxied = static_cast<IDirect3D7*>(d3d7IntfProxiedVoid);
      // Hold the address, not a reference, to newly created interfaces, otherwise
      // things may explode during an application's resource release cycle
      m_d3d7Intf = ref(new D3D7Interface(std::move(d3d7IntfProxied), this));
      *ppvObject = m_d3d7Intf;
      return S_OK;
    // Some games query the legacy ddraw interface
    } else if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::warn("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw");
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

  HRESULT STDMETHODCALLTYPE DDraw7Interface::Compact() {
    Logger::debug("<<< DDraw7Interface::Compact: Proxy");
    return m_proxy->Compact();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug("<<< DDraw7Interface::CreateClipper: Proxy");
    return m_proxy->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    Logger::debug("<<< DDraw7Interface::CreatePalette: Proxy");
    return m_proxy->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw7Interface::CreateSurface");

    if (unlikely(lpDDSurfaceDesc == nullptr || lplpDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface7> ddraw7SurfaceProxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddraw7SurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        Com<DDraw7Surface> surface7 = new DDraw7Surface(std::move(ddraw7SurfaceProxied), this, nullptr, true);

        if (unlikely(m_d3d7ConfigIntf->GetOptions()->proxiedQueryInterface)) {
          // Hack: Gothic / Gothic 2 and other games attach the depth stencil to an externally created
          // back buffer, so we need to re-attach the depth stencil to the back buffer on device creation
          if (unlikely(surface7->IsRenderTarget() || surface7->IsDepthStencil())) {
            if (unlikely(surface7->IsDepthStencil()))
              m_lastDepthStencil = surface7.ptr();
            surface7->SetForwardToProxy(true);
          }
        }

        *lplpDDSurface = surface7.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        *lplpDDSurface = nullptr;
        return DDERR_GENERIC;
      }
    } else {
      Logger::err("DDraw7Interface::CreateSurface: Failed to create proxy surface");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::DuplicateSurface(LPDIRECTDRAWSURFACE7 lpDDSurface, LPDIRECTDRAWSURFACE7 *lplpDupDDSurface) {
    Logger::debug("<<< DDraw7Interface::DuplicateSurface: Proxy");

    if (IsWrappedSurface(lpDDSurface)) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSurface);
      // TODO: Wrap the duplicate before returning
      return m_proxy->DuplicateSurface(ddraw7Surface->GetProxied(), lplpDupDDSurface);
    } else {
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback) {
    Logger::debug("<<< DDraw7Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw7Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::FlipToGDISurface() {
    // TODO: Think about how to handle this gracefully,
    // as quite a number of games rely on it for proper behavior
    Logger::debug("*** DDraw7Interface::FlipToGDISurface: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDraw7Interface::GetCaps: Proxy");

    HRESULT hr = m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);

    // Strip palletted surface support from reported caps
    if (likely(SUCCEEDED(hr))) {
      if (lpDDDriverCaps != nullptr) {
        lpDDDriverCaps->dwCaps &= ~(DDCAPS_PALETTE | DDCAPS_PALETTEVSYNC);
        lpDDDriverCaps->dwPalCaps = 0;
      }
      if (lpDDHELCaps != nullptr) {
        lpDDHELCaps->dwCaps &= ~(DDCAPS_PALETTE | DDCAPS_PALETTEVSYNC);
        lpDDHELCaps->dwPalCaps = 0;
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw7Interface::GetDisplayMode: Proxy");
    return m_proxy->GetDisplayMode(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    Logger::debug("<<< DDraw7Interface::GetFourCCCodes: Proxy");
    return m_proxy->GetFourCCCodes(lpNumCodes, lpCodes);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetGDISurface(IDirectDrawSurface7** lplpGDIDDSurface) {
    Logger::debug("<<< DDraw7Interface::GetGDISurface: Proxy");

    if(unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface7> gdiSurface = nullptr;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);

    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Interface::GetGDISurface: Failed to retrieve GDI surface");
      return hr;
    }

    if (unlikely(IsWrappedSurface(gdiSurface.ptr()))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(gdiSurface.ptr());
      *lplpGDIDDSurface = ddraw7Surface->GetProxied();
    } else {
      Logger::debug("DDraw7Interface::GetGDISurface: Received a non-wrapped GDI surface");
      // TODO: Maybe we want to keep it around... as a sort of GDI front buffer ref?
      try {
        *lplpGDIDDSurface = ref(new DDraw7Surface(std::move(gdiSurface), this, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        *lplpGDIDDSurface = nullptr;
        return DDERR_GENERIC;
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    Logger::debug("<<< DDraw7Interface::GetMonitorFrequency: Proxy");
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetScanLine(LPDWORD lpdwScanLine) {
    Logger::debug("<<< DDraw7Interface::GetScanLine: Proxy");
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    Logger::debug("<<< DDraw7Interface::GetVerticalBlankStatus: Proxy");
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::Initialize(GUID* lpGUID) {
    Logger::debug("<<< DDraw7Interface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::RestoreDisplayMode() {
    Logger::debug("<<< DDraw7Interface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::SetCooperativeLevel: Proxy");
    // This needs to be called on interface init, so is a reliable
    // way of getting the needed hWnd for d3d7 device creation
    m_hwnd = hWnd;
    return m_proxy->SetCooperativeLevel(hWnd, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::SetDisplayMode: Proxy");
    return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw7Interface::WaitForVerticalBlank: Proxy");
    return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    Logger::debug("<<< DDraw7Interface::GetAvailableVidMem: Proxy");
    // TODO: Implement memory limit reporting based
    // on returned data, as some games will need it
    return m_proxy->GetAvailableVidMem(lpDDCaps, lpdwTotal, lpdwFree);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetSurfaceFromDC(HDC hdc, IDirectDrawSurface7** pSurf) {
    Logger::debug(">>> DDraw7Interface::GetSurfaceFromDC");

    if (unlikely(pSurf == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface7> surface = nullptr;
    HRESULT hr = m_proxy->GetSurfaceFromDC(hdc, &surface);

    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw7Interface::GetSurfaceFromDC: Failed to get surface from DC");
      return hr;
    }

    try {
      *pSurf = ref(new DDraw7Surface(std::move(surface), this, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      *pSurf = nullptr;
      return DDERR_GENERIC;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::RestoreAllSurfaces() {
    Logger::debug("<<< DDraw7Interface::RestoreAllSurfaces: Proxy");
    return m_proxy->RestoreAllSurfaces();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::TestCooperativeLevel() {
    Logger::debug("<<< DDraw7Interface::TestCooperativeLevel: Proxy");
    return m_proxy->TestCooperativeLevel();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER2 pDDDI, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::GetDeviceIdentifier: Proxy");
    return m_proxy->GetDeviceIdentifier(pDDDI, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::StartModeTest(LPSIZE pModes, DWORD dwNumModes, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::StartModeTest: Proxy");
    return m_proxy->StartModeTest(pModes, dwNumModes, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EvaluateMode(DWORD dwFlags, DWORD* pTimeout) {
    Logger::debug("<<< DDraw7Interface::EvaluateMode: Proxy");
    return m_proxy->EvaluateMode(dwFlags, pTimeout);
  }

  bool DDraw7Interface::IsWrappedSurface(IDirectDrawSurface7* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(surface);
    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw7Surface);
    if (likely(it != m_surfaces.end()))
      return true;

    return false;
  }

  void DDraw7Interface::AddWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw7Surface);
      if (unlikely(it != m_surfaces.end())) {
          Logger::err("DDraw7Interface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(ddraw7Surface);
      }
    }
  }

  void DDraw7Interface::RemoveWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw7Surface);
      if (likely(it != m_surfaces.end())) {
          m_surfaces.erase(it);
      }
    }
  }
}
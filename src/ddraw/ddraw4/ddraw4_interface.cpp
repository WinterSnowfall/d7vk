#include "ddraw4_interface.h"

#include "../d3d6/d3d6_interface.h"

#include "../ddraw7/ddraw7_interface.h"

#include "ddraw4_surface.h"

#include <algorithm>

namespace dxvk {

  uint32_t DDraw4Interface::s_intfCount = 0;

  DDraw4Interface::DDraw4Interface(Com<IDirectDraw4>&& proxyIntf, DDraw7Interface* origin)
    : DDrawWrappedObject<IUnknown, IDirectDraw4, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_origin ( origin ) {
    if (likely(!IsLegacyInterface())) {
      // Initialize the IDirect3D6 interlocked object
      void* d3d6IntfProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirect3D3), &d3d6IntfProxiedVoid);
      Com<IDirect3D3> d3d6IntfProxied = static_cast<IDirect3D3*>(d3d6IntfProxiedVoid);
      m_d3d6Intf = new D3D6Interface(std::move(d3d6IntfProxied), this);
    }

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

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      if (unlikely(IsLegacyInterface())) {
        Logger::warn("DDraw4Interface::QueryInterface: Query for IDirect3D3");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      *ppvObject = m_d3d6Intf.ref();
      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::debug("DDraw4Interface::QueryInterface: Query for legacy IDirectDraw");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw4Interface::QueryInterface: Query for legacy IDirectDraw");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      if (unlikely(IsLegacyInterface())) {
        Logger::warn("DDraw4Interface::QueryInterface: Query for legacy IDirect3D");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::err("DDraw4Interface::QueryInterface: Unsupported IDirect3D interface");
      return E_NOINTERFACE;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (unlikely(IsLegacyInterface())) {
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

  // The documentation states: "The IDirectDraw4::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Interface::Compact() {
    Logger::debug(">>> DDraw4Interface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::CreateClipper: Forwarded");
      return m_origin->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
    }

    Logger::debug("<<< DDraw4Interface::CreateClipper: Proxy");
    // Note: Unfortunately, if we wrap clippers, WineD3D's ddraw will crash on an assert,
    // as it expects the vtable to correspond to its internal clipper implementation
    return m_proxy->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter);

    /*if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this));
    } else {
      Logger::warn("DDraw4Interface::CreateClipper: Failed to create proxy clipper");
      return hr;
    }

    return DD_OK;*/
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::CreatePalette: Forwarded");
      return m_origin->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
    }

    Logger::debug("<<< DDraw4Interface::CreatePalette: Proxy");
    // Note: Unfortunately, if we wrap palettes, WineD3D's ddraw will crash on an assert,
    // as it expects the vtable to correspond to its internal palette implementation
    return m_proxy->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);

    /*if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), this));
    } else {
      Logger::warn("DDraw4Interface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;*/
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE4 *lplpDDSurface, IUnknown *pUnkOuter) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Interface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    if (unlikely(m_d3d6Intf->GetOptions()->proxiedLegacySurfaces)) {
      Logger::debug("<<< DDrawInterface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDraw4Interface::CreateSurface");

    // The cooperative level is always checked first
    if (unlikely(!m_cooperativeLevel))
      return DDERR_NOCOOPERATIVELEVELSET;

    if (unlikely(lpDDSurfaceDesc == nullptr || lplpDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDSurface);

    // Because we are removing the DDSCAPS_WRITEONLY flag below, we need
    // to first validate the combinations that would otherwise cause issues
    HRESULT hr = ValidateSurfaceFlags(lpDDSurfaceDesc);
    if (unlikely(FAILED(hr)))
      return hr;

    // We need to ensure we can always read from surfaces for upload to
    // D3D9, so always strip the DDSCAPS_WRITEONLY flag on creation
    lpDDSurfaceDesc->ddsCaps.dwCaps &= ~DDSCAPS_WRITEONLY;
    // Similarly strip the DDSCAPS2_OPAQUE flag on texture creation
    if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE) {
      lpDDSurfaceDesc->ddsCaps.dwCaps2 &= ~DDSCAPS2_OPAQUE;
    }

    if (unlikely((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
              && (lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask == 0xFFFFFFFF))) {
      if (!m_d3d6Intf->GetOptions()->proxiedQueryInterface
        && m_d3d6Intf->GetOptions()->useD24X8forD32) {
        // In case of up-front unsupported and unadvertised D32 depth stencil use,
        // replace it with D24X8, as some games, such as Sacrifice, rely on it
        // to properly enable 32-bit display modes (and revert to 16-bit otherwise)
        Logger::info("DDraw7Interface::CreateSurface: Using D24X8 instead of D32");
        lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
      } else {
        Logger::warn("DDraw7Interface::CreateSurface: Use of unsupported D32");
      }
    }

    Com<IDirectDrawSurface4> ddrawSurface4Proxied;
    hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurface4Proxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        Com<DDraw4Surface> surface4 = new DDraw4Surface(std::move(ddrawSurface4Proxied), this, nullptr, nullptr, true);

        if (unlikely(m_d3d6Intf->GetOptions()->proxiedQueryInterface)) {
          // Hack: Gothic / Gothic 2 and other games attach the depth stencil to an externally created
          // back buffer, so we need to re-attach the depth stencil to the back buffer on device creation
          if (unlikely(surface4->IsForwardableSurface())) {
            if (unlikely(surface4->IsDepthStencil()))
              m_lastDepthStencil = surface4.ptr();
            surface4->SetForwardToProxy(true);
          }
        }

        *lplpDDSurface = surface4.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    // Some games simply try creating surfaces with various formats until something works...
    } else {
      Logger::debug("DDraw4Interface::CreateSurface: Failed to create proxy surface");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::DuplicateSurface(LPDIRECTDRAWSURFACE4 lpDDSurface, LPDIRECTDRAWSURFACE4 *lplpDupDDSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::warn("<<< DDraw4Interface::DuplicateSurface: Proxy");
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }

    Logger::debug("<<< DDraw4Interface::DuplicateSurface: Proxy");

    if (IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSurface);
      Com<IDirectDrawSurface4> dupSurface4;
      HRESULT hr = m_proxy->DuplicateSurface(ddraw4Surface->GetProxied(), &dupSurface4);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDraw4Surface(std::move(dupSurface4), this, nullptr, nullptr, false));
        } catch (const DxvkError& e) {
          Logger::err(e.message());
          return DDERR_GENERIC;
        }
      }
      return hr;
    } else {
      if (unlikely(lpDDSurface != nullptr))
        Logger::warn("DDraw7Interface::DuplicateSurface: Received an unwrapped source surface");
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }
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

    if (unlikely(m_d3d6Intf->GetOptions()->forceProxiedPresent))
      return m_proxy->FlipToGDISurface();

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
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::GetFourCCCodes: Forwarded");
      return m_origin->GetFourCCCodes(lpNumCodes, lpCodes);
    }

    Logger::debug(">>> DDraw4Interface::GetFourCCCodes");

    static const DWORD supportedFourCCs[] =
    {
        MAKEFOURCC('D', 'X', 'T', '1'),
        MAKEFOURCC('D', 'X', 'T', '2'),
        MAKEFOURCC('D', 'X', 'T', '3'),
        MAKEFOURCC('D', 'X', 'T', '4'),
        MAKEFOURCC('D', 'X', 'T', '5'),
    };

    const size_t returnNum = lpNumCodes != nullptr && *lpNumCodes < 5 ? *lpNumCodes : 5;

    // Only report DXT1-5 as supported FOURCCs
    if (lpNumCodes != nullptr && *lpNumCodes < 5)
      *lpNumCodes = 5;

    if (likely(lpCodes != nullptr)) {
      for (uint32_t i = 0; i < returnNum; i++) {
        lpCodes[returnNum] = supportedFourCCs[returnNum];
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetGDISurface(LPDIRECTDRAWSURFACE4 *lplpGDIDDSurface) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Interface::GetGDISurface: Proxy");
      return m_proxy->GetGDISurface(lplpGDIDDSurface);
    }

    Logger::debug("<<< DDraw4Interface::GetGDISurface: Proxy");

    if(unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpGDIDDSurface);

    Com<IDirectDrawSurface4> gdiSurface;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);

    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw4Interface::GetGDISurface: Failed to retrieve GDI surface");
      return hr;
    }

    if (unlikely(IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      Logger::debug("DDraw4Interface::GetGDISurface: Received a non-wrapped GDI surface");
      try {
        *lplpGDIDDSurface = ref(new DDraw4Surface(std::move(gdiSurface), this, nullptr, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    }

    return hr;
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
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::SetCooperativeLevel: Forwarded");
      return m_origin->SetCooperativeLevel(hWnd, dwFlags);
    }

    Logger::debug("<<< DDraw4Interface::SetCooperativeLevel: Proxy");

    HRESULT hr = m_proxy->SetCooperativeLevel(hWnd, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    const bool changed = m_cooperativeLevel != dwFlags;

    // This needs to be called on interface init, so is a reliable
    // way of getting the needed hWnd for d3d7 device creation
    if (likely((dwFlags & DDSCL_NORMAL) || (dwFlags & DDSCL_EXCLUSIVE)))
      m_hwnd = hWnd;

    if (changed)
      m_cooperativeLevel = dwFlags;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Interface::SetDisplayMode: Proxy");
      return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    }

    Logger::debug("<<< DDraw4Interface::SetDisplayMode: Proxy");

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_d3d6Intf->GetOptions()->forceProxiedPresent &&
                m_d3d6Intf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_cooperativeLevel & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw4Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        if (m_modeSize.width != dwWidth || m_modeSize.height != dwHeight) {
          m_modeSize.width  = dwWidth;
          m_modeSize.height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Interface::WaitForVerticalBlank: Proxy");
      return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    }

    Logger::debug("<<< DDraw7Interface::WaitForVerticalBlank: Proxy");

    HRESULT hr = m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_d3d6Intf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D6Device* d3d6Device = m_d3d6Intf->GetLastUsedDevice();
      if (unlikely(d3d6Device != nullptr && !m_waitForVBlank)) {
        Logger::info("DDraw4Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d6Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d6Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Interface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_waitForVBlank = true;
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug(">>> DDraw4Interface::GetAvailableVidMem: Forwarded");
      return m_origin->GetAvailableVidMem(lpDDCaps, lpdwTotal, lpdwFree);
    }

    Logger::debug(">>> DDraw4Interface::GetAvailableVidMem");

    if (unlikely(lpdwTotal == nullptr && lpdwFree == nullptr))
      return DD_OK;

    constexpr DWORD Megabytes = 1024 * 1024;

    D3D6Device* d3d6Device = m_d3d6Intf->GetLastUsedDevice();
    if (likely(d3d6Device != nullptr)) {
      Logger::debug("DDraw4Interface::GetAvailableVidMem: Getting memory stats from D3D9");

      const DWORD total9 = static_cast<DWORD>(m_d3d6Intf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD free9  = static_cast<DWORD>(d3d6Device->GetD3D9()->GetAvailableTextureMem());

      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;

    } else {
      Logger::debug("DDraw4Interface::GetAvailableVidMem: Getting memory stats from DDraw");

      DWORD total6 = 0;
      DWORD free6  = 0;

      HRESULT hr = m_proxy->GetAvailableVidMem(lpDDCaps, &total6, &free6);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Interface::GetAvailableVidMem: Failed proxied call");
        if (lpdwTotal != nullptr)
          *lpdwTotal = 0;
        if (lpdwFree != nullptr)
          *lpdwFree  = 0;
        return hr;
      }

      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: DDraw Total: ", total6));
      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: DDraw Free : ", free6));

      const DWORD total9 = static_cast<DWORD>(m_d3d6Intf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD delta  = total6 > total9 ? total6 - total9 : 0;
      const DWORD free9  = free6 > delta ? free6 - delta : 0;

      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE4 *pSurf) {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Interface::GetSurfaceFromDC: Proxy");
      return m_proxy->GetSurfaceFromDC(hdc, pSurf);
    }

    if (unlikely(pSurf == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(pSurf);

    Com<IDirectDrawSurface4> surface;
    HRESULT hr = m_proxy->GetSurfaceFromDC(hdc, &surface);

    if (unlikely(FAILED(hr))) {
      Logger::warn("DDraw4Interface::GetSurfaceFromDC: Failed to get surface from DC");
      return hr;
    }

    try {
      *pSurf = ref(new DDraw4Surface(std::move(surface), this, nullptr, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreAllSurfaces() {
    if (unlikely(IsLegacyInterface())) {
      Logger::debug("<<< DDraw4Interface::RestoreAllSurfaces: Proxy");
      return m_proxy->RestoreAllSurfaces();
    }

    Logger::debug("<<< DDraw4Interface::RestoreAllSurfaces: Proxy");

    HRESULT hr = m_proxy->RestoreAllSurfaces();
    if (unlikely(FAILED(hr)))
      return hr;

    for (auto* surface : m_surfaces) {
      if (!surface->IsTexture() || m_d3d6Intf->GetOptions()->forceTextureUploads) {
        surface->InitializeOrUploadD3D9();
      } else {
        surface->DirtyMipMaps();
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::TestCooperativeLevel() {
    Logger::debug("<<< DDraw4Interface::TestCooperativeLevel: Proxy");
    return m_proxy->TestCooperativeLevel();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Interface::GetDeviceIdentifier: Proxy");
    return m_proxy->GetDeviceIdentifier(pDDDI, dwFlags);
  }

  bool DDraw4Interface::IsWrappedSurface(IDirectDrawSurface4* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(surface);
    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw4Surface);
    if (likely(it != m_surfaces.end()))
      return true;

    return false;
  }

  void DDraw4Interface::AddWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw4Surface);
      if (unlikely(it != m_surfaces.end())) {
        Logger::warn("DDraw7Interface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(ddraw4Surface);

        if (likely(ddraw4Surface->IsChildObject()))
          this->AddRef();
      }
    }
  }

  void DDraw4Interface::RemoveWrappedSurface(IDirectDrawSurface4* surface) {
    if (likely(surface != nullptr)) {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw4Surface);
      if (likely(it != m_surfaces.end())) {
        m_surfaces.erase(it);

        if (likely(ddraw4Surface->IsChildObject()))
          this->Release();
      } else {
        Logger::warn("DDraw7Interface::RemoveWrappedSurface: Surface not found");
      }
    }
  }

}
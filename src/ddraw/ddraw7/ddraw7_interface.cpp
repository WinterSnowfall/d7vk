#include "ddraw7_interface.h"

#include "../d3d7/d3d7_device.h"

#include "../ddraw_interface.h"
#include "../ddraw2/ddraw2_interface.h"
#include "../ddraw4/ddraw4_interface.h"

#include "ddraw7_surface.h"

#include <algorithm>

namespace dxvk {

  uint32_t DDraw7Interface::s_intfCount = 0;

  DDraw7Interface::DDraw7Interface(Com<IDirectDraw7>&& proxyIntf)
    : DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown>(nullptr, std::move(proxyIntf), nullptr) {
    // Initialize the IDirect3D7 interlocked object
    void* d3d7IntfProxiedVoid = nullptr;
    // This can never reasonably fail
    m_proxy->QueryInterface(__uuidof(IDirect3D7), &d3d7IntfProxiedVoid);
    Com<IDirect3D7> d3d7IntfProxied = static_cast<IDirect3D7*>(d3d7IntfProxiedVoid);
    m_d3d7Intf = new D3D7Interface(std::move(d3d7IntfProxied), this);

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw7Interface: Created a new interface nr. <<7-", m_intfCount, ">>"));
  }

  DDraw7Interface::~DDraw7Interface() {
    Logger::debug(str::format("DDraw7Interface: Interface nr. <<7-", m_intfCount, ">> bites the dust"));
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

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D7 interface
    if (riid == __uuidof(IDirect3D7)) {
      *ppvObject = m_d3d7Intf.ref();
      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::debug("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw");

      Com<IDirectDraw> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawInterface(std::move(ppvProxyObject), this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      Logger::warn("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw2");

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Interface(std::move(ppvProxyObject), nullptr, this));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDraw4))) {
      Logger::debug("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw4");

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Interface(std::move(ppvProxyObject), nullptr, this));

      return S_OK;
    }
    // Quite a lot of games query for this IID during intro playback
    if (unlikely(riid == GUID_IAMMediaStream)) {
      Logger::debug("DDraw7Interface::QueryInterface: Query for IAMMediaStream");
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

  // The documentation states: "The IDirectDraw7::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Interface::Compact() {
    Logger::debug(">>> DDraw7Interface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw7Interface::CreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), reinterpret_cast<DDrawInterface*>(this)));
    } else {
      Logger::warn("DDraw7Interface::CreateClipper: Failed to create proxy clipper");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw7Interface::CreatePalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), reinterpret_cast<DDrawInterface*>(this)));
    } else {
      Logger::warn("DDraw7Interface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw7Interface::CreateSurface");

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
      if (!m_d3d7Intf->GetOptions()->proxiedQueryInterface
        && m_d3d7Intf->GetOptions()->useD24X8forD32) {
        // In case of up-front unsupported and unadvertised D32 depth stencil use,
        // replace it with D24X8, as some games, such as Sacrifice, rely on it
        // to properly enable 32-bit display modes (and revert to 16-bit otherwise)
        Logger::info("DDraw7Interface::CreateSurface: Using D24X8 instead of D32");
        lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
      } else {
        Logger::warn("DDraw7Interface::CreateSurface: Use of unsupported D32");
      }
    }

    Com<IDirectDrawSurface7> ddraw7SurfaceProxied;
    hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddraw7SurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        Com<DDraw7Surface> surface7 = new DDraw7Surface(std::move(ddraw7SurfaceProxied), this, nullptr, true);

        if (unlikely(m_d3d7Intf->GetOptions()->proxiedQueryInterface)) {
          // Hack: Gothic / Gothic 2 and other games attach the depth stencil to an externally created
          // back buffer, so we need to re-attach the depth stencil to the back buffer on device creation
          if (unlikely(surface7->IsForwardableSurface())) {
            if (unlikely(surface7->IsDepthStencil()))
              m_lastDepthStencil = surface7.ptr();
            surface7->SetForwardToProxy(true);
          }
        }

        *lplpDDSurface = surface7.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    // Some games simply try creating surfaces with various formats until something works...
    } else {
      Logger::debug("DDraw7Interface::CreateSurface: Failed to create proxy surface");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::DuplicateSurface(LPDIRECTDRAWSURFACE7 lpDDSurface, LPDIRECTDRAWSURFACE7 *lplpDupDDSurface) {
    Logger::debug("<<< DDraw7Interface::DuplicateSurface: Proxy");

    if (IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSurface);
      Com<IDirectDrawSurface7> dupSurface7;
      HRESULT hr = m_proxy->DuplicateSurface(ddraw7Surface->GetProxied(), &dupSurface7);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDraw7Surface(std::move(dupSurface7), this, nullptr, false));
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

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback) {
    Logger::debug("<<< DDraw7Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    Logger::debug("<<< DDraw7Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::FlipToGDISurface() {
    Logger::debug("*** DDraw7Interface::FlipToGDISurface: Ignoring");

    if (unlikely(m_d3d7Intf->GetOptions()->forceProxiedPresent))
      return m_proxy->FlipToGDISurface();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDraw7Interface::GetCaps: Proxy");
    return m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    Logger::debug("<<< DDraw7Interface::GetDisplayMode: Proxy");
    return m_proxy->GetDisplayMode(lpDDSurfaceDesc);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    Logger::debug(">>> DDraw7Interface::GetFourCCCodes");

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

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetGDISurface(LPDIRECTDRAWSURFACE7 *lplpGDIDDSurface) {
    Logger::debug("<<< DDraw7Interface::GetGDISurface: Proxy");

    if(unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpGDIDDSurface);

    Com<IDirectDrawSurface7> gdiSurface;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);

    if (unlikely(FAILED(hr))) {
      Logger::debug("DDraw7Interface::GetGDISurface: Failed to retrieve GDI surface");
      return hr;
    }

    if (unlikely(IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      Logger::debug("DDraw7Interface::GetGDISurface: Received a non-wrapped GDI surface");
      try {
        *lplpGDIDDSurface = ref(new DDraw7Surface(std::move(gdiSurface), this, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
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

  HRESULT STDMETHODCALLTYPE DDraw7Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::SetDisplayMode: Proxy");

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_d3d7Intf->GetOptions()->forceProxiedPresent &&
                m_d3d7Intf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_cooperativeLevel & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw7Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        if (m_modeSize.width != dwWidth || m_modeSize.height != dwHeight) {
          m_modeSize.width = dwWidth;
          m_modeSize.height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw7Interface::WaitForVerticalBlank: Proxy");

    HRESULT hr = m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_d3d7Intf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D7Device* d3d7Device = m_d3d7Intf->GetLastUsedDevice();
      if (unlikely(d3d7Device != nullptr && !m_waitForVBlank)) {
        Logger::info("DDraw7Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d7Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d7Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Interface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_waitForVBlank = true;
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    Logger::debug(">>> DDraw7Interface::GetAvailableVidMem");

    if (unlikely(lpdwTotal == nullptr && lpdwFree == nullptr))
      return DD_OK;

    constexpr DWORD Megabytes = 1024 * 1024;

    D3D7Device* d3d7Device = m_d3d7Intf->GetLastUsedDevice();
    if (likely(d3d7Device != nullptr)) {
      Logger::debug("DDraw7Interface::GetAvailableVidMem: Getting memory stats from D3D9");

      const DWORD total9 = static_cast<DWORD>(m_d3d7Intf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD free9  = static_cast<DWORD>(d3d7Device->GetD3D9()->GetAvailableTextureMem());

      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;

    } else {
      Logger::debug("DDraw7Interface::GetAvailableVidMem: Getting memory stats from DDraw");

      DWORD total7 = 0;
      DWORD free7  = 0;

      HRESULT hr = m_proxy->GetAvailableVidMem(lpDDCaps, &total7, &free7);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw7Interface::GetAvailableVidMem: Failed proxied call");
        if (lpdwTotal != nullptr)
          *lpdwTotal = 0;
        if (lpdwFree != nullptr)
          *lpdwFree  = 0;
        return hr;
      }

      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: DDraw Total: ", total7));
      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: DDraw Free : ", free7));

      const DWORD total9 = static_cast<DWORD>(m_d3d7Intf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD delta  = total7 > total9 ? total7 - total9 : 0;
      const DWORD free9  = free7 > delta ? free7 - delta : 0;

      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw7Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *pSurf) {
    Logger::debug(">>> DDraw7Interface::GetSurfaceFromDC");

    if (unlikely(pSurf == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(pSurf);

    Com<IDirectDrawSurface7> surface;
    HRESULT hr = m_proxy->GetSurfaceFromDC(hdc, &surface);

    if (unlikely(FAILED(hr))) {
      Logger::warn("DDraw7Interface::GetSurfaceFromDC: Failed to get surface from DC");
      return hr;
    }

    try {
      *pSurf = ref(new DDraw7Surface(std::move(surface), this, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::RestoreAllSurfaces() {
    Logger::debug("<<< DDraw7Interface::RestoreAllSurfaces: Proxy");

    HRESULT hr = m_proxy->RestoreAllSurfaces();
    if (unlikely(FAILED(hr)))
      return hr;

    for (auto* surface : m_surfaces) {
      if (!surface->IsTextureOrCubeMap()) {
        surface->InitializeOrUploadD3D9();
      } else {
        surface->DirtyMipMaps();
      }
    }

    return hr;
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
        Logger::warn("DDraw7Interface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(ddraw7Surface);

        if (likely(ddraw7Surface->IsChildObject()))
          this->AddRef();
      }
    }
  }

  void DDraw7Interface::RemoveWrappedSurface(IDirectDrawSurface7* surface) {
    if (likely(surface != nullptr)) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddraw7Surface);
      if (likely(it != m_surfaces.end())) {
        m_surfaces.erase(it);

        if (likely(ddraw7Surface->IsChildObject()))
          this->Release();
      } else {
        Logger::warn("DDraw7Interface::RemoveWrappedSurface: Surface not found");
      }
    }
  }
}
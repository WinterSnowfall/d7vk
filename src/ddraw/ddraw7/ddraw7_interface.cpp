#include "ddraw7_interface.h"

#include "ddraw7_surface.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw2/ddraw2_interface.h"
#include "../ddraw4/ddraw4_interface.h"

#include "../d3d7/d3d7_interface.h"
#include "../d3d7/d3d7_device.h"

namespace dxvk {

  uint32_t DDraw7Interface::s_intfCount = 0;

  DDraw7Interface::DDraw7Interface(
      DDrawCommonInterface* commonIntf,
      Com<IDirectDraw7>&& proxyIntf,
      IUnknown* origin,
      bool needsInitialization)
    : DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_needsInitialization ( needsInitialization )
    , m_commonIntf ( commonIntf )
    , m_origin ( origin ) {
    // We need a temporary D3D9 interface at this point to retrieve the
    // adapter identifier, as well as (potentially) the options through a bridge
    Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);

    d3d9::D3DADAPTER_IDENTIFIER9 adapterIdentifier9;
    HRESULT hr = d3d9Intf->GetAdapterIdentifier(0, 0, &adapterIdentifier9);
    if (unlikely(FAILED(hr))) {
      throw DxvkError("DDraw7Interface: ERROR! Failed to get D3D9 adapter identifier!");
    }

    if (m_commonIntf == nullptr) {
      Com<IDxvkD3D8InterfaceBridge> d3d9Bridge;
      // Can never fail while we statically link the d3d9 module
      d3d9Intf->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge));

      m_commonIntf = new DDrawCommonInterface(D3DOptions(*d3d9Bridge->GetConfig()));
    }

    m_commonIntf->SetAdapterIdentifier(adapterIdentifier9);

    m_commonIntf->SetDD7Interface(this);

    static bool s_apitraceModeWarningShown;

    if (unlikely(m_commonIntf->GetOptions()->apitraceMode &&
                 !std::exchange(s_apitraceModeWarningShown, true)))
      Logger::warn("DDraw7Interface: Apitrace mode is enabled. Performance will be suboptimal!");

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw7Interface: Created a new interface nr. <<7-", m_intfCount, ">>"));
  }

  DDraw7Interface::~DDraw7Interface() {
    m_commonIntf->SetDD7Interface(nullptr);

    Logger::debug(str::format("DDraw7Interface: Interface nr. <<7-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw7))
      return this;

    throw DxvkError("DDraw7Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D7 interface
    if (riid == __uuidof(IDirect3D7)) {
      Logger::debug("DDraw7Interface::QueryInterface: Query for IDirect3D7");

      // Initialize the IDirect3D7 interlocked object
      if (unlikely(m_d3d7Intf == nullptr)) {
        Com<IDirect3D7> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_d3d7Intf = new D3D7Interface(std::move(ppvProxyObject), this);
      }

      *ppvObject = m_d3d7Intf.ref();

      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw7Interface::QueryInterface: Query for existing IDirectDraw");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw");

      Com<IDirectDraw> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawInterface(m_commonIntf.ptr(), std::move(ppvProxyObject), this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      if (m_commonIntf->GetDD2Interface() != nullptr) {
        Logger::debug("DDraw7Interface::QueryInterface: Query for existing IDirectDraw2");
        return m_commonIntf->GetDD2Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw2");

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDDInterface(), this, false));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDraw4))) {
      if (m_commonIntf->GetDD4Interface() != nullptr) {
        Logger::debug("DDraw7Interface::QueryInterface: Query for existing IDirectDraw4");
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw7Interface::QueryInterface: Query for legacy IDirectDraw4");

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDDInterface(), this, false));

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
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this, false));
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
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), this));
    } else {
      Logger::warn("DDraw7Interface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw7Interface::CreateSurface");

    // The cooperative level is always checked first
    if (unlikely(!m_commonIntf->GetCooperativeLevel()))
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
      if (m_commonIntf->GetOptions()->useD24X8forD32) {
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
        Com<DDraw7Surface> surface7 = new DDraw7Surface(nullptr, std::move(ddraw7SurfaceProxied), this, nullptr, nullptr, true);

        if (unlikely(surface7->GetCommonSurface()->IsDepthStencil()))
          m_lastDepthStencil = surface7.ptr();

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

    if (m_commonIntf->IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSurface);
      Com<IDirectDrawSurface7> dupSurface7;
      HRESULT hr = m_proxy->DuplicateSurface(ddraw7Surface->GetProxied(), &dupSurface7);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDraw7Surface(nullptr, std::move(dupSurface7), this, nullptr, nullptr, false));
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

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback) {
    Logger::debug("<<< DDraw7Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    Logger::warn("<<< DDraw7Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::FlipToGDISurface() {
    Logger::debug("*** DDraw7Interface::FlipToGDISurface: Ignoring");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
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
        MAKEFOURCC('Y', 'U', 'Y', '2'),
    };

    // TODO: Check passed lpNumCodes size is larger than 6
    if (likely(lpNumCodes != nullptr && lpCodes != nullptr)) {
      for (uint8_t i = 0; i < 6; i++) {
        lpCodes[i] = supportedFourCCs[i];
      }
    }

    if (lpNumCodes != nullptr)
      *lpNumCodes = 6;

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

    if (unlikely(m_commonIntf->IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      Logger::debug("DDraw7Interface::GetGDISurface: Received a non-wrapped GDI surface");
      try {
        *lplpGDIDDSurface = ref(new DDraw7Surface(nullptr, std::move(gdiSurface), this, nullptr, nullptr, false));
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
    // Needed for interfaces crated via GetProxiedDDrawModule()
    if (unlikely(m_needsInitialization && !m_isInitialized)) {
      Logger::debug(">>> DDrawInterface::Initialize");
      m_isInitialized = true;
      return DD_OK;
    }

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

    m_commonIntf->SetCooperativeLevel(hWnd, dwFlags);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::SetDisplayMode: Proxy");

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent &&
                m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw7Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
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

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D7Device* d3d7Device = m_commonIntf->GetD3D7Device();
      if (unlikely(d3d7Device != nullptr && !m_commonIntf->GetWaitForVBlank())) {
        Logger::info("DDraw7Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d7Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d7Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Interface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
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

    D3D7Device* d3d7Device = m_commonIntf->GetD3D7Device();
    if (likely(d3d7Device != nullptr)) {
      Logger::debug("DDraw7Interface::GetAvailableVidMem: Getting memory stats from D3D9");

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
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

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
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
      *pSurf = ref(new DDraw7Surface(nullptr, std::move(surface), this, nullptr, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
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
    Logger::debug(">>> DDraw7Interface::GetDeviceIdentifier");

    if (unlikely(pDDDI == nullptr))
      return DDERR_INVALIDPARAMS;

    const d3d9::D3DADAPTER_IDENTIFIER9* adapterIdentifier9 = m_commonIntf->GetAdapterIdentifier();
    // This is typically the "2D accelerator" in the system
    if (unlikely(dwFlags & DDGDI_GETHOSTIDENTIFIER)) {
      Logger::debug("DDraw7Interface::GetDeviceIdentifier: Retrieving secondary adapter info");
      CopyToStringArray(pDDDI->szDriver, "vga.dll");
      CopyToStringArray(pDDDI->szDescription, "Standard VGA Adapter");
      pDDDI->liDriverVersion.QuadPart = 0;
      pDDDI->dwVendorId               = 0;
      pDDDI->dwDeviceId               = 0;
      pDDDI->dwSubSysId               = 0;
      pDDDI->dwRevision               = 0;
      pDDDI->guidDeviceIdentifier     = GUID_StandardVGAAdapter;
      pDDDI->dwWHQLLevel              = 0;
    } else {
      Logger::debug("DDraw7Interface::GetDeviceIdentifier: Retrieving primary adapter info");
      memcpy(&pDDDI->szDriver,      &adapterIdentifier9->Driver,      sizeof(adapterIdentifier9->Driver));
      memcpy(&pDDDI->szDescription, &adapterIdentifier9->Description, sizeof(adapterIdentifier9->Description));
      pDDDI->liDriverVersion.QuadPart = adapterIdentifier9->DriverVersion.QuadPart;
      pDDDI->dwVendorId               = adapterIdentifier9->VendorId;
      pDDDI->dwDeviceId               = adapterIdentifier9->DeviceId;
      pDDDI->dwSubSysId               = adapterIdentifier9->SubSysId;
      pDDDI->dwRevision               = adapterIdentifier9->Revision;
      pDDDI->guidDeviceIdentifier     = adapterIdentifier9->DeviceIdentifier;
      pDDDI->dwWHQLLevel              = adapterIdentifier9->WHQLLevel;
    }

    Logger::info(str::format("DDraw7Interface::GetDeviceIdentifier: Reporting: ", pDDDI->szDescription));

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::StartModeTest(LPSIZE pModes, DWORD dwNumModes, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Interface::StartModeTest: Proxy");
    return m_proxy->StartModeTest(pModes, dwNumModes, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Interface::EvaluateMode(DWORD dwFlags, DWORD* pTimeout) {
    Logger::debug("<<< DDraw7Interface::EvaluateMode: Proxy");
    return m_proxy->EvaluateMode(dwFlags, pTimeout);
  }

}
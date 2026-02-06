#include "ddraw4_interface.h"

#include "ddraw4_surface.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw2/ddraw2_interface.h"
#include "../ddraw7/ddraw7_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  uint32_t DDraw4Interface::s_intfCount = 0;

  DDraw4Interface::DDraw4Interface(
      DDrawCommonInterface* commonIntf,
      Com<IDirectDraw4>&& proxyIntf,
      DDrawInterface* pParent,
      IUnknown* origin,
      bool needsInitialization)
    : DDrawWrappedObject<DDrawInterface, IDirectDraw4, IUnknown>(pParent, std::move(proxyIntf), nullptr)
    , m_needsInitialization ( needsInitialization )
    , m_commonIntf ( commonIntf )
    , m_origin ( origin ) {
    // We need a temporary D3D9 interface at this point to retrieve the adapter identifier
    Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);

    d3d9::D3DADAPTER_IDENTIFIER9 adapterIdentifier9;
    HRESULT hr = d3d9Intf->GetAdapterIdentifier(0, 0, &adapterIdentifier9);
    if (unlikely(FAILED(hr))) {
      throw DxvkError("DDraw4Interface: ERROR! Failed to get D3D9 adapter identifier!");
    }

    m_commonIntf->SetAdapterIdentifier(adapterIdentifier9);

    m_commonIntf->SetDD4Interface(this);

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw4Interface: Created a new interface nr. <<4-", m_intfCount, ">>"));
  }

  DDraw4Interface::~DDraw4Interface() {
    m_commonIntf->SetDD4Interface(nullptr);

    Logger::debug(str::format("DDraw4Interface: Interface nr. <<4-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDraw4, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw4))
      return this;

    throw DxvkError("DDraw4Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirect3D3");

      // Initialize the IDirect3D3 interlocked object
      if (unlikely(m_d3d6Intf == nullptr)) {
        Com<IDirect3D3> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_d3d6Intf = new D3D6Interface(std::move(ppvProxyObject), this);
      }

      *ppvObject = m_d3d6Intf.ref();

      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw4Interface::QueryInterface: Query for existing IDirectDraw");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirectDraw");

      Com<IDirectDraw> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawInterface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                          nullptr, m_needsInitialization));

      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      if (m_commonIntf->GetDD2Interface() != nullptr) {
        Logger::debug("DDraw4Interface::QueryInterface: Query for existing IDirectDraw2");
        return m_commonIntf->GetDD2Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirectDraw2");

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDDInterface(), nullptr, m_needsInitialization));

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (m_commonIntf->GetDD7Interface() != nullptr) {
        Logger::debug("DDraw4Interface::QueryInterface: Query for existing IDirectDraw7");
        return m_commonIntf->GetDD7Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirectDraw7");

      Com<IDirectDraw7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           this, m_needsInitialization));

      return S_OK;
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw4Interface::QueryInterface: Forwarded query for IDirect3D");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirect3D");

      Com<IDirect3D> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Interface(std::move(ppvProxyObject), m_parent));

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw4Interface::QueryInterface: Forwarded query for IDirect3D2");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw4Interface::QueryInterface: Query for IDirect3D2");

      Com<IDirect3D2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D5Interface(std::move(ppvProxyObject), nullptr));

      return S_OK;
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
    Logger::debug(">>> DDraw4Interface::CreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this, false));
    } else {
      Logger::warn("DDraw4Interface::CreateClipper: Failed to create proxy clipper");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw4Interface::CreatePalette");

    if (unlikely(lplpDDPalette == nullptr))
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

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE4 *lplpDDSurface, IUnknown *pUnkOuter) {
    if (unlikely(m_commonIntf->GetOptions()->proxiedLegacySurfaces)) {
      Logger::debug("<<< DDraw4Interface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDraw4Interface::CreateSurface");

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

    Com<IDirectDrawSurface4> ddrawSurface4Proxied;
    hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurface4Proxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        Com<DDraw4Surface> surface4 = new DDraw4Surface(nullptr, std::move(ddrawSurface4Proxied), this, nullptr, nullptr, true);

        if (unlikely(surface4->GetCommonSurface()->IsDepthStencil()))
          m_lastDepthStencil = surface4.ptr();

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
    Logger::debug("<<< DDraw4Interface::DuplicateSurface: Proxy");

    if (m_commonIntf->IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSurface);
      Com<IDirectDrawSurface4> dupSurface4;
      HRESULT hr = m_proxy->DuplicateSurface(ddraw4Surface->GetProxied(), &dupSurface4);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDraw4Surface(nullptr, std::move(dupSurface4), this, nullptr, nullptr, false));
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
    Logger::warn("<<< DDraw4Interface::EnumSurfaces: Proxy");
    return m_proxy->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::FlipToGDISurface() {
    Logger::debug("*** DDraw4Interface::FlipToGDISurface: Ignoring");

    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent))
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
    Logger::debug(">>> DDraw4Interface::GetFourCCCodes");

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

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetGDISurface(LPDIRECTDRAWSURFACE4 *lplpGDIDDSurface) {
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

    if (unlikely(m_commonIntf->IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      Logger::debug("DDraw4Interface::GetGDISurface: Received a non-wrapped GDI surface");
      try {
        *lplpGDIDDSurface = ref(new DDraw4Surface(nullptr, std::move(gdiSurface), this, nullptr, nullptr, false));
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
    // Needed for interfaces crated via GetProxiedDDrawModule()
    if (unlikely(m_needsInitialization && !m_isInitialized)) {
      Logger::debug(">>> DDraw4Interface::Initialize");
      m_isInitialized = true;
      return DD_OK;
    }

    Logger::debug("<<< DDraw4Interface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreDisplayMode() {
    Logger::debug("<<< DDraw4Interface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Interface::SetCooperativeLevel: Proxy");

    HRESULT hr = m_proxy->SetCooperativeLevel(hWnd, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonIntf->SetCooperativeLevel(hWnd, dwFlags);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw4Interface::SetDisplayMode: Proxy");

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent &&
                m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw4Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw7Interface::WaitForVerticalBlank: Proxy");

    HRESULT hr = m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D6Device* d3d6Device = m_commonIntf->GetD3D6Device();
      if (unlikely(d3d6Device != nullptr && !m_commonIntf->GetWaitForVBlank())) {
        Logger::info("DDraw4Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d6Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d6Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Interface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    Logger::debug(">>> DDraw4Interface::GetAvailableVidMem");

    if (unlikely(lpdwTotal == nullptr && lpdwFree == nullptr))
      return DD_OK;

    constexpr DWORD Megabytes = 1024 * 1024;

    D3D6Device* d3d6Device = m_commonIntf->GetD3D6Device();
    if (likely(d3d6Device != nullptr)) {
      Logger::debug("DDraw4Interface::GetAvailableVidMem: Getting memory stats from D3D9");

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
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

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
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
      *pSurf = ref(new DDraw4Surface(nullptr, std::move(surface), this, nullptr, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return hr;
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
    Logger::debug(">>> DDraw4Interface::GetDeviceIdentifier");

    if (unlikely(pDDDI == nullptr))
      return DDERR_INVALIDPARAMS;

    const d3d9::D3DADAPTER_IDENTIFIER9* adapterIdentifier9 = m_commonIntf->GetAdapterIdentifier();
    // This is typically the "2D accelerator" in the system
    if (unlikely(dwFlags & DDGDI_GETHOSTIDENTIFIER)) {
      Logger::debug("DDraw4Interface::GetDeviceIdentifier: Retrieving secondary adapter info");
      CopyToStringArray(pDDDI->szDriver, "vga.dll");
      CopyToStringArray(pDDDI->szDescription, "Standard VGA Adapter");
      pDDDI->liDriverVersion.QuadPart = 0;
      pDDDI->dwVendorId               = 0;
      pDDDI->dwDeviceId               = 0;
      pDDDI->dwSubSysId               = 0;
      pDDDI->dwRevision               = 0;
      pDDDI->guidDeviceIdentifier     = GUID_StandardVGAAdapter;
    } else {
      Logger::debug("DDraw4Interface::GetDeviceIdentifier: Retrieving primary adapter info");
      memcpy(&pDDDI->szDriver,      &adapterIdentifier9->Driver,      sizeof(adapterIdentifier9->Driver));
      memcpy(&pDDDI->szDescription, &adapterIdentifier9->Description, sizeof(adapterIdentifier9->Description));
      pDDDI->liDriverVersion.QuadPart = adapterIdentifier9->DriverVersion.QuadPart;
      pDDDI->dwVendorId               = adapterIdentifier9->VendorId;
      pDDDI->dwDeviceId               = adapterIdentifier9->DeviceId;
      pDDDI->dwSubSysId               = adapterIdentifier9->SubSysId;
      pDDDI->dwRevision               = adapterIdentifier9->Revision;
      pDDDI->guidDeviceIdentifier     = adapterIdentifier9->DeviceIdentifier;
    }

    Logger::info(str::format("DDraw4Interface::GetDeviceIdentifier: Reporting: ", pDDDI->szDescription));

    return DD_OK;
  }

}
#include "ddraw2_interface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw/ddraw_interface.h"
#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw7/ddraw7_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_device.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  uint32_t DDraw2Interface::s_intfCount = 0;

  DDraw2Interface::DDraw2Interface(
      DDrawCommonInterface* commonIntf,
      Com<IDirectDraw2>&& proxyIntf,
      DDrawInterface* pParent,
      IUnknown* origin,
      bool needsInitialization)
    : DDrawWrappedObject<DDrawInterface, IDirectDraw2, IUnknown>(pParent, std::move(proxyIntf), nullptr)
    , m_needsInitialization ( needsInitialization )
    , m_commonIntf ( commonIntf )
    , m_parentIntf ( pParent )
    , m_origin ( origin ) {
    // m_commonIntf can never be null for IDirectDraw2
    m_commonIntf->SetDD2Interface(this);

    static bool s_apitraceModeWarningShown;

    if (unlikely(m_commonIntf->GetOptions()->apitraceMode &&
                 !std::exchange(s_apitraceModeWarningShown, true)))
      Logger::warn("DDraw2Interface: Apitrace mode is enabled. Performance will be suboptimal!");

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDraw2Interface: Created a new interface nr. <<2-", m_intfCount, ">>"));
  }

  DDraw2Interface::~DDraw2Interface() {
    m_commonIntf->SetDD2Interface(nullptr);

    Logger::debug(str::format("DDraw2Interface: Interface nr. <<2-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDraw2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw2))
      return this;

    throw DxvkError("DDraw2Interface::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw2Interface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D interface
    if (riid == __uuidof(IDirect3D2)) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw2Interface::QueryInterface: Query for IDirect3D2");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDraw2Interface::QueryInterface: Query for IDirect3D2");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      if (m_commonIntf->GetDDInterface() != nullptr) {
        Logger::debug("DDraw2Interface::QueryInterface: Query for existing IDirectDraw");
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Interface::QueryInterface: Query for IDirectDraw");

      Com<IDirectDraw> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawInterface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                          nullptr, m_needsInitialization));

      return S_OK;
    }
    // Legacy way of getting a DDraw4 interface
    if (riid == __uuidof(IDirectDraw4)) {
      if (m_commonIntf->GetDD4Interface() != nullptr) {
        Logger::debug("DDraw2Interface::QueryInterface: Query for existing IDirectDraw4");
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Interface::QueryInterface: Query for IDirectDraw4");

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDDInterface(), nullptr, m_needsInitialization));

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (m_commonIntf->GetDD7Interface() != nullptr) {
        Logger::debug("DDraw2Interface::QueryInterface: Query for existing IDirectDraw7");
        return m_commonIntf->GetDD7Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Interface::QueryInterface: Query for IDirectDraw7");

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
      if (m_commonIntf->GetDDInterface() == nullptr) {
        Logger::warn("DDraw2Interface::QueryInterface: Query for IDirect3D");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDraw2Interface::QueryInterface: Query for IDirect3D");

      Com<IDirect3D> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Interface(std::move(ppvProxyObject), m_commonIntf->GetDDInterface()));

      return S_OK;
    }
    // Standard way of retrieving a D3D6 interface
    if (unlikely(riid == __uuidof(IDirect3D3))) {
      Logger::debug("DDraw2Interface::QueryInterface: Query for IDirect3D3");

      Com<IDirect3D3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D6Interface(std::move(ppvProxyObject), m_commonIntf->GetDD4Interface()));

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

  // The documentation states: "The IDirectDraw2::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw2Interface::Compact() {
    Logger::debug(">>> DDraw2Interface::Compact");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw2Interface::CreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this, false));
    } else {
      Logger::warn("DDraw2Interface::CreateClipper: Failed to create proxy clipper");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDraw2Interface::CreatePalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      // Palettes created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), nullptr));
    } else {
      Logger::warn("DDraw2Interface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
    if (unlikely(m_commonIntf->GetOptions()->proxiedLegacySurfaces)) {
      Logger::debug("<<< DDraw2Interface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDraw2Interface::CreateSurface");

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

    if (unlikely((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
              && (lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask == 0xFFFFFFFF))) {
      if (m_commonIntf->GetOptions()->useD24X8forD32) {
        // In case of up-front unsupported and unadvertised D32 depth stencil use,
        // replace it with D24X8, as some games, such as Sacrifice, rely on it
        // to properly enable 32-bit display modes (and revert to 16-bit otherwise)
        Logger::info("DDraw2Interface::CreateSurface: Using D24X8 instead of D32");
        lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
      } else {
        Logger::warn("DDraw2Interface::CreateSurface: Use of unsupported D32");
      }
    }

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        // Surfaces created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
        Com<DDrawSurface> surface = new DDrawSurface(nullptr, std::move(ddrawSurfaceProxied),
                                                     m_commonIntf->GetDDInterface(), nullptr, nullptr, false);

        if (unlikely(surface->GetCommonSurface()->IsDepthStencil())) {
          m_lastDepthStencil = surface.ptr();
          // Also update the last set depth stencil on the base interface
          if (m_parent != nullptr)
            m_parent->SetLastDepthStencil(surface.ptr());
        }

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
    Logger::debug("<<< DDraw2Interface::DuplicateSurface: Proxy");

    if (m_commonIntf->IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSurface);
      Com<IDirectDrawSurface> dupSurface;
      HRESULT hr = m_proxy->DuplicateSurface(ddrawSurface->GetProxied(), &dupSurface);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDrawSurface(nullptr, std::move(dupSurface),
                                                   m_commonIntf->GetDDInterface(), nullptr, nullptr, false));
        } catch (const DxvkError& e) {
          Logger::err(e.message());
          return DDERR_GENERIC;
        }
      }
      return hr;
    } else {
      if (unlikely(lpDDSurface != nullptr))
        Logger::warn("DDraw2Interface::DuplicateSurface: Received an unwrapped source surface");
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    Logger::debug("<<< DDraw2Interface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::warn("<<< DDraw2Interface::EnumSurfaces: Proxy");
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
    Logger::debug(">>> DDraw2Interface::GetFourCCCodes");

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
    Logger::debug(">>> DDraw2Interface::Initialize");

    // Needed for interfaces crated via GetProxiedDDrawModule()
    if (unlikely(m_needsInitialization && !m_isInitialized)) {
      m_isInitialized = true;
      return DD_OK;
    }

    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::RestoreDisplayMode() {
    Logger::debug("<<< DDraw2Interface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    Logger::debug("<<< DDraw2Interface::SetCooperativeLevel: Proxy");

    HRESULT hr = m_proxy->SetCooperativeLevel(hWnd, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonIntf->SetCooperativeLevel(hWnd, dwFlags);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug("<<< DDraw2Interface::SetDisplayMode: Proxy");

    Logger::debug(str::format("DDraw2Interface::SetDisplayMode: ", dwWidth, "x", dwHeight, ":", dwBPP, "@", dwRefreshRate));

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent &&
                m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw2Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDraw2Interface::WaitForVerticalBlank: Proxy");

    HRESULT hr = m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D5Device* d3d5Device = m_commonIntf->GetD3D5Device();
      if (unlikely(d3d5Device != nullptr && !m_commonIntf->GetWaitForVBlank())) {
        Logger::info("DDraw2Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d5Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d5Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw2Interface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Interface::GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    Logger::debug(">>> DDraw2Interface::GetAvailableVidMem");

    if (unlikely(lpdwTotal == nullptr && lpdwFree == nullptr))
      return DD_OK;

    constexpr DWORD Megabytes = 1024 * 1024;

    D3D5Device* d3d5Device = m_commonIntf->GetD3D5Device();
    if (likely(d3d5Device != nullptr)) {
      Logger::debug("DDraw2Interface::GetAvailableVidMem: Getting memory stats from D3D9");

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD free9  = static_cast<DWORD>(d3d5Device->GetD3D9()->GetAvailableTextureMem());

      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;

    } else {
      Logger::debug("DDraw2Interface::GetAvailableVidMem: Getting memory stats from DDraw");

      DWORD total6 = 0;
      DWORD free6  = 0;

      HRESULT hr = m_proxy->GetAvailableVidMem(lpDDCaps, &total6, &free6);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw2Interface::GetAvailableVidMem: Failed proxied call");
        if (lpdwTotal != nullptr)
          *lpdwTotal = 0;
        if (lpdwFree != nullptr)
          *lpdwFree  = 0;
        return hr;
      }

      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: DDraw Total: ", total6));
      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: DDraw Free : ", free6));

      const DWORD total9 = static_cast<DWORD>(m_commonIntf->GetOptions()->maxAvailableMemory) * Megabytes;
      const DWORD delta  = total6 > total9 ? total6 - total9 : 0;
      const DWORD free9  = free6 > delta ? free6 - delta : 0;

      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: Total: ", total9));
      Logger::debug(str::format("DDraw2Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;
    }

    return DD_OK;
  }

}
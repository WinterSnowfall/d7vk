#include "ddraw_interface.h"

#include "ddraw_surface.h"

#include "../ddraw_clipper.h"
#include "../ddraw_palette.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw2/ddraw2_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  uint32_t DDrawInterface::s_intfCount = 0;

  DDrawInterface::DDrawInterface(
        DDrawCommonInterface* commonIntf,
        Com<IDirectDraw>&& proxyIntf)
    : DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_commonIntf ( commonIntf ) {

    if (m_commonIntf == nullptr) {
      // We need a temporary D3D9 interface at this point to retrieve the options,
      // even if we're only proxying and we don't yet have any child D3D interfaces
      Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);
      Com<IDxvkD3D8InterfaceBridge> d3d9Bridge;

      if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge))))) {
        throw DxvkError("DDrawInterface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
      }

      m_commonIntf = new DDrawCommonInterface(D3DOptions(*d3d9Bridge->GetConfig()));
    }

    if (m_commonIntf->GetOrigin() == nullptr)
      m_commonIntf->SetOrigin(this);

    m_commonIntf->SetDDInterface(this);

    static bool s_apitraceModeWarningShown;

    if (unlikely(m_commonIntf->GetOptions()->apitraceMode &&
                 !std::exchange(s_apitraceModeWarningShown, true)))
      Logger::warn("DDrawInterface: Apitrace mode is enabled. Performance will be suboptimal!");

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDrawInterface: Created a new interface nr. <<1-", m_intfCount, ">>"));
  }

  DDrawInterface::~DDrawInterface() {
    if (m_commonIntf->GetOrigin() == this)
      m_commonIntf->SetOrigin(nullptr);

    m_commonIntf->SetDDInterface(nullptr);

    Logger::debug(str::format("DDrawInterface: Interface nr. <<1-", m_intfCount, ">> bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawInterface::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      // GTA 2 queries for IDirect3D3 on IDirectDraw, after creating a
      // IDirectDraw4 interface and a ton of surfaces... so forward the call
      if (m_commonIntf->GetDD4Interface() != nullptr) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirect3D3");
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D3");

      Com<IDirect3D3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<D3D6Interface> d3d6Intf = new D3D6Interface(m_commonIntf.ptr(), nullptr, std::move(ppvProxyObject), this);
      *ppvObject = d3d6Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D2");

      // Initialize the IDirect3D2 interlocked object
      if (unlikely(m_d3d5Intf == nullptr)) {
        Com<IDirect3D2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_d3d5Intf = new D3D5Interface(m_commonIntf.ptr(), nullptr, std::move(ppvProxyObject), this);
      }

      *ppvObject = m_d3d5Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D");

      // Initialize the IDirect3D interlocked object
      if (unlikely(m_d3d3Intf == nullptr)) {
        Com<IDirect3D> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_d3d3Intf = new D3D3Interface(m_commonIntf.ptr(), nullptr, std::move(ppvProxyObject), this);
        m_commonIntf->SetD3D3Interface(m_d3d3Intf.ptr());
      }

      *ppvObject = m_d3d3Intf.ref();

      return S_OK;
    }
    // Standard way of getting a DDraw4 interface
    if (riid == __uuidof(IDirectDraw4)) {
      if (m_commonIntf->GetDD4Interface() != nullptr) {
        Logger::debug("DDrawInterface::QueryInterface: Query for existing IDirectDraw4");
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw4");

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Standard way of getting a DDraw2 interface
    if (riid == __uuidof(IDirectDraw2)) {
      if (m_commonIntf->GetDD2Interface() != nullptr) {
        Logger::debug("DDrawInterface::QueryInterface: Query for existing IDirectDraw2");
        return m_commonIntf->GetDD2Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw2");

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (m_commonIntf->GetDD7Interface() != nullptr) {
        Logger::debug("DDrawInterface::QueryInterface: Query for existing IDirectDraw7");
        return m_commonIntf->GetDD7Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw7");

      Com<IDirectDraw7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Quite a lot of games query for this IID during intro playback
    if (unlikely(riid == GUID_IAMMediaStream)) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IAMMediaStream");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Also seen queried by some games, such as V-Rally 2: Expert Edition
    if (unlikely(riid == GUID_IMediaStream)) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IMediaStream");
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

  // The documentation states: "The IDirectDraw::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDrawInterface::Compact() {
    Logger::debug(">>> DDrawInterface::Compact");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDrawInterface::CreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this));
    } else {
      Logger::warn("DDrawInterface::CreateClipper: Failed to create proxy clipper");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDrawInterface::CreatePalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      // Palettes created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), nullptr));
    } else {
      Logger::warn("DDrawInterface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDrawInterface::CreateSurface");

    // The cooperative level is always checked first
    if (unlikely(!m_commonIntf->IsCooperativeLevelSet()))
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
        Logger::info("DDrawInterface::CreateSurface: Using D24X8 instead of D32");
        lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
      } else {
        Logger::warn("DDrawInterface::CreateSurface: Use of unsupported D32");
      }
    }

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        // Surfaces created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
        Com<DDrawSurface> surface = new DDrawSurface(nullptr, std::move(ddrawSurfaceProxied), this, nullptr, false);
        *lplpDDSurface = surface.ref();
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    } else {
      Logger::debug("DDrawInterface::CreateSurface: Failed to create proxy surface");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface) {
    Logger::debug("<<< DDrawInterface::DuplicateSurface: Proxy");

    if (m_commonIntf->IsWrappedSurface(lpDDSurface)) {
      InitReturnPtr(lplpDupDDSurface);

      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSurface);
      Com<IDirectDrawSurface> dupSurface;
      HRESULT hr = m_proxy->DuplicateSurface(ddrawSurface->GetProxied(), &dupSurface);
      if (likely(SUCCEEDED(hr))) {
        try {
          *lplpDupDDSurface = ref(new DDrawSurface(nullptr, std::move(dupSurface), this, nullptr, false));
        } catch (const DxvkError& e) {
          Logger::err(e.message());
          return DDERR_GENERIC;
        }
      }
      return hr;
    } else {
      if (unlikely(lpDDSurface != nullptr)) {
        Logger::warn("DDrawInterface::DuplicateSurface: Received an unwrapped source surface");
        return DDERR_GENERIC;
      }
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    Logger::debug("<<< DDrawInterface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::debug(">>> DDrawInterface::EnumSurfaces: Proxy");

    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface> attachedSurfaces;
    // Enumerate all surfaces from the underlying DDraw implementation
    m_proxy->EnumSurfaces(dwFlags, lpDDSD, reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfacesCallback);

    HRESULT hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface> surface = surfaceIt->surface;

      Com<DDrawSurface> ddrawSurface = new DDrawSurface(nullptr, std::move(surface), this, nullptr, false);
      hr = lpEnumSurfacesCallback(ddrawSurface.ref(), &surfaceIt->surfaceDesc, lpContext);

      ++surfaceIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::FlipToGDISurface() {
    Logger::debug("*** DDrawInterface::FlipToGDISurface: Ignoring");
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    Logger::debug("<<< DDrawInterface::GetCaps: Proxy");

    HRESULT hr = m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
    if (unlikely(FAILED(hr)))
      return hr;

    static constexpr DWORD Megabytes = 1024 * 1024;
    static constexpr DWORD MaxMemory = ddrawCaps::MaxTextureMemory * Megabytes;
    static constexpr DWORD ReservedMemory = ddrawCaps::ReservedTextureMemory * Megabytes;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Properly fill in the dwVidMemTotal / dwVidMemFree fields
    DWORD total9 = 0;
    DWORD free9  = 0;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonIntf->GetD3D9Device();
    if (likely(d3d9Device != nullptr)) {
      Logger::debug("DDrawInterface::GetCaps: Getting memory stats from D3D9");

      total9 = static_cast<DWORD>(m_commonIntf->GetTotalTextureMemory());
      free9  = static_cast<DWORD>(d3d9Device->GetAvailableTextureMem());

      if (likely(total9 >= MaxMemory)) {
        const DWORD delta = total9 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free9 > delta + ReservedMemory ? free9 - (delta + ReservedMemory) : 0;
      }

      Logger::debug(str::format("DDrawInterface::GetCaps: Total: ", total9));
      Logger::debug(str::format("DDrawInterface::GetCaps: Free : ", free9));
    } else {
      Logger::debug("DDrawInterface::GetCaps: Getting memory stats from DDraw");

      const DWORD total3 = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemTotal : 0;
      const DWORD free3  = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemFree  : 0;

      Logger::debug(str::format("DDrawInterface::GetCaps: DDraw Total: ", total3));
      Logger::debug(str::format("DDrawInterface::GetCaps: DDraw Free : ", free3));

      if (unlikely(total3 < MaxMemory)) {
        total9 = total3;
        free9 = free3;
      } else {
        const DWORD delta = total3 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free3 > delta + ReservedMemory ? free3 - (delta + ReservedMemory) : 0;
      }

      Logger::debug(str::format("DDrawInterface::GetCaps: Total: ", total9));
      Logger::debug(str::format("DDrawInterface::GetCaps: Free : ", free9));
    }

    if (lpDDDriverCaps != nullptr) {
      lpDDDriverCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDDriverCaps->dwVidMemTotal = total9;
      lpDDDriverCaps->dwVidMemFree  = free9;
      lpDDDriverCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }
    if (lpDDHELCaps != nullptr) {
      lpDDHELCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDHELCaps->dwVidMemTotal = total9;
      lpDDHELCaps->dwVidMemFree  = free9;
      lpDDHELCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) {
    Logger::debug("<<< DDrawInterface::GetDisplayMode: Proxy");

    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_proxy->GetDisplayMode(lpDDSurfaceDesc);
    if (unlikely(FAILED(hr)))
      return hr;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (unlikely(d3dOptions->mask8BitModes && lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 8))
      lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 16;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    Logger::debug(">>> DDrawInterface::GetFourCCCodes");

    if (likely(lpNumCodes != nullptr && lpCodes != nullptr)) {
      const uint32_t copyNumCodes = std::min<uint32_t>(ddrawCaps::NumberOfFOURCCCodes, *lpNumCodes);
      for (uint32_t i = 0; i < copyNumCodes; i++) {
        lpCodes[i] = ddrawCaps::SupportedFourCCs[i];
      }
    }

    if (lpNumCodes != nullptr)
      *lpNumCodes = ddrawCaps::NumberOfFOURCCCodes;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetGDISurface(LPDIRECTDRAWSURFACE *lplpGDIDDSurface) {
    Logger::debug("<<< DDrawInterface::GetGDISurface: Proxy");

    if(unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpGDIDDSurface);

    Com<IDirectDrawSurface> gdiSurface;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);

    if (unlikely(FAILED(hr))) {
      Logger::debug("DDrawInterface::GetGDISurface: Failed to retrieve GDI surface");
      return hr;
    }

    if (unlikely(m_commonIntf->IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      Logger::debug("DDrawInterface::GetGDISurface: Received a non-wrapped GDI surface");
      try {
        *lplpGDIDDSurface = ref(new DDrawSurface(nullptr, std::move(gdiSurface), this, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    }

    return hr;
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

  // Should technically always return DDERR_ALREADYINITIALIZED, unless the
  // interface is created via IClassFactory, however Requiem: Avenging Angel
  // expects it to work on a regular interface too, after initially creating
  // and releasing an interface through IClassFactory (but never initializing it).
  // On native DDraw the initial interface most likely gets reused. In practice,
  // applications that don't use IClassFactory won't call this, so keep it simple.
  HRESULT STDMETHODCALLTYPE DDrawInterface::Initialize(GUID* lpGUID) {
    Logger::debug(">>> DDrawInterface::Initialize");

    if (unlikely(m_commonIntf->IsInitialized()))
      return DDERR_ALREADYINITIALIZED;

    m_commonIntf->MarkAsInitialized();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::RestoreDisplayMode() {
    Logger::debug("<<< DDrawInterface::RestoreDisplayMode: Proxy");
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    Logger::debug("<<< DDrawInterface::SetCooperativeLevel: Proxy");

    HRESULT hr = m_proxy->SetCooperativeLevel(hWnd, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonIntf->SetCooperativeLevel(hWnd, dwFlags);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) {
    Logger::debug("<<< DDrawInterface::SetDisplayMode: Proxy");

    Logger::debug(str::format("DDrawInterface::SetDisplayMode: ", dwWidth, "x", dwHeight, ":", dwBPP));

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent &&
                m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDrawInterface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    if (unlikely(m_commonIntf->GetOptions()->forceProxiedPresent)) {
      Logger::debug("<<< DDrawInterface::WaitForVerticalBlank: Proxy");
      m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    }

    Logger::debug(">>> DDrawInterface::WaitForVerticalBlank");

    if (unlikely(dwFlags & DDWAITVB_BLOCKBEGINEVENT))
      return DDERR_UNSUPPORTED;

    // Switch to a default presentation interval when an application
    // tries to wait for vertical blank, if we're not already doing so
    d3d9::IDirect3DDevice9* d3d9Device = m_commonIntf->GetD3D9Device();
    if (unlikely(d3d9Device != nullptr && !m_commonIntf->GetWaitForVBlank())) {
      Logger::info("DDrawInterface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

      d3d9::D3DPRESENT_PARAMETERS resetParams = m_commonIntf->GetPresentParameters();
      resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
      HRESULT hrReset = m_commonIntf->ResetD3D9Swapchain(&resetParams);
      if (likely(SUCCEEDED(hrReset)))
        m_commonIntf->SetWaitForVBlank(true);
    }

    return DD_OK;
  }

}
#include "ddraw_interface.h"

#include "ddraw_surface.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw2/ddraw2_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d5/d3d5_device.h"
#include "../d3d6/d3d6_interface.h"
#include "../d3d5/d3d5_texture.h"

namespace dxvk {

  uint32_t DDrawInterface::s_intfCount = 0;

  DDrawInterface::DDrawInterface(
      DDrawCommonInterface* commonIntf,
      Com<IDirectDraw>&& proxyIntf,
      IUnknown* origin,
      bool needsInitialization)
    : DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_needsInitialization ( needsInitialization )
    , m_commonIntf ( commonIntf )
    , m_origin ( origin ) {
    if (m_origin == nullptr) {
      // We need a temporary D3D9 interface at this point to retrieve the options,
      // even if we're only proxying and we don't yet have any child D3D interfaces
      Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);
      Com<IDxvkD3D8InterfaceBridge> d3d9Bridge;

      if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge))))) {
        throw DxvkError("DDrawInterface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
      }

      if (m_commonIntf == nullptr)
        m_commonIntf = new DDrawCommonInterface(D3DOptions(*d3d9Bridge->GetConfig()));
    }

    m_commonIntf->SetDDInterface(this);

    static bool s_apitraceModeWarningShown;

    if (unlikely(m_commonIntf->GetOptions()->apitraceMode &&
                 !std::exchange(s_apitraceModeWarningShown, true)))
      Logger::warn("DDrawInterface: Apitrace mode is enabled. Performance will be suboptimal!");

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("DDrawInterface: Created a new interface nr. <<1-", m_intfCount, ">>"));
  }

  DDrawInterface::~DDrawInterface() {
    m_commonIntf->SetDDInterface(nullptr);

    Logger::debug(str::format("DDrawInterface: Interface nr. <<1-", m_intfCount, ">> bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDraw))
      return this;

    throw DxvkError("DDrawInterface::QueryInterface: Unknown interface query");
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

      *ppvObject = ref(new D3D6Interface(std::move(ppvProxyObject), nullptr));

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D2");

      Com<IDirect3D2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      m_d3d5Intf = new D3D5Interface(std::move(ppvProxyObject), this);
      *ppvObject = m_d3d5Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D");

      Com<IDirect3D> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Interface(std::move(ppvProxyObject), this));

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

      *ppvObject = ref(new DDraw4Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           this, nullptr, m_needsInitialization));

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

      *ppvObject = ref(new DDraw2Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           this, nullptr, m_needsInitialization));

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (m_commonIntf->GetDD7Interface() != nullptr) {
        Logger::debug("DDrawInterface::QueryInterface: Query for existing IDirectDraw7");
        return m_commonIntf->GetDD7Interface()->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawInterface::QueryInterface: Query for IDirectDraw7");

      Com<IDirectDraw7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Interface(m_commonIntf.ptr(), std::move(ppvProxyObject),
                                           this, m_needsInitialization));

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
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    Logger::debug(">>> DDrawInterface::CreateClipper");

    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDClipper = ref(new DDrawClipper(std::move(lplpDDClipperProxy), this, false));
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
    if (unlikely(m_commonIntf->GetOptions()->proxiedLegacySurfaces)) {
      Logger::debug("<<< DDrawInterface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDrawInterface::CreateSurface");

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        // Surfaces created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
        Com<DDrawSurface> surface = new DDrawSurface(nullptr, std::move(ddrawSurfaceProxied),
                                                     this, nullptr, nullptr, false);

        if (unlikely(surface->GetCommonSurface()->IsDepthStencil()))
          m_lastDepthStencil = surface.ptr();

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
          *lplpDupDDSurface = ref(new DDrawSurface(nullptr, std::move(dupSurface),
                                                   this, nullptr, nullptr, false));
        } catch (const DxvkError& e) {
          Logger::err(e.message());
          return DDERR_GENERIC;
        }
      }
      return hr;
    } else {
      if (unlikely(lpDDSurface != nullptr))
        Logger::warn("DDrawInterface::DuplicateSurface: Received an unwrapped source surface");
      return m_proxy->DuplicateSurface(lpDDSurface, lplpDupDDSurface);
    }
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    Logger::debug("<<< DDrawInterface::EnumDisplayModes: Proxy");
    return m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    Logger::warn("<<< DDrawInterface::EnumSurfaces: Proxy");
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
    Logger::debug(">>> DDrawInterface::GetFourCCCodes");

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
    // Needed for interfaces crated via GetProxiedDDrawModule()
    if (unlikely(m_needsInitialization && !m_isInitialized)) {
      Logger::debug(">>> DDrawInterface::Initialize");
      m_isInitialized = true;
      return DD_OK;
    }

    Logger::debug("<<< DDrawInterface::Initialize: Proxy");
    return m_proxy->Initialize(lpGUID);
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
    if (m_d3d5Intf == nullptr) {
      Logger::debug("<<< DDraw2Interface::WaitForVerticalBlank: Proxy");
      return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    }

    Logger::debug("<<< DDrawInterface::WaitForVerticalBlank: Proxy");

    HRESULT hr = m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(!m_commonIntf->GetOptions()->forceProxiedPresent)) {
      // Switch to a default presentation interval when an application
      // tries to wait for vertical blank, if we're not already doing so
      D3D5Device* d3d5Device = m_commonIntf->GetD3D5Device();
      if (unlikely(d3d5Device != nullptr && !m_commonIntf->GetWaitForVBlank())) {
        Logger::info("DDrawInterface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        d3d9::D3DPRESENT_PARAMETERS resetParams = d3d5Device->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = d3d5Device->Reset(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDrawInterface::WaitForVerticalBlank: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }
    }

    return hr;
  }

  D3D5Texture* DDrawInterface::GetTextureFromHandle(D3DTEXTUREHANDLE handle) {
    auto texturesIter = m_textures.find(handle);

    if (unlikely(texturesIter == m_textures.end())) {
      Logger::warn(str::format("DDrawInterface::GetTextureFromHandle: Invalid handle: ", handle));
      return nullptr;
    }

    return texturesIter->second;
  }

}
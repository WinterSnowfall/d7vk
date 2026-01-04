#include "ddraw_interface.h"

#include "ddraw_surface.h"

#include "ddraw7/ddraw7_interface.h"
#include "ddraw4/ddraw4_interface.h"
#include "ddraw2/ddraw2_interface.h"
#include "d3d3/d3d3_interface.h"
#include "d3d5/d3d5_interface.h"
#include "d3d6/d3d6_interface.h"
#include "d3d5/d3d5_texture.h"

#include "../d3d9/d3d9_bridge.h"

#include <algorithm>

namespace dxvk {

  uint32_t DDrawInterface::s_intfCount = 0;

  DDrawInterface::DDrawInterface(Com<IDirectDraw>&& proxyIntf, DDraw7Interface* origin)
    : DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown>(nullptr, std::move(proxyIntf), nullptr)
    , m_origin ( origin ) {
    if (likely(!IsLegacyInterface())) {
      // We need a temporary D3D9 interface at this point to retrieve the options,
      // even if we're only proxying and we don't yet have any child D3D interfaces
      Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);
      Com<IDxvkD3D8InterfaceBridge> d3d9Bridge;

      if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge))))) {
        throw DxvkError("DDrawInterface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
      }

      m_options = D3DOptions(*d3d9Bridge->GetConfig());
    }

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

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDrawInterface::QueryInterface: Query for IDirect3D3");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D3");

      Com<IDirect3D3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D6Interface(std::move(ppvProxyObject), m_intf4));

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDrawInterface::QueryInterface: Query for legacy IDirect3D2");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

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
      if (likely(IsLegacyInterface())) {
        Logger::warn("DDrawInterface::QueryInterface: Query for legacy IDirect3D");
        return m_proxy->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirect3D");

      Com<IDirect3D> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Interface(std::move(ppvProxyObject), this));

      return S_OK;
    }
    // Legacy way of retrieving a D3D7 interface
    if (unlikely(riid == __uuidof(IDirect3D7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirect3D7");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawInterface::QueryInterface: Query for IDirect3D7");

      Com<IDirect3D7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D7Interface(std::move(ppvProxyObject), nullptr));

      return S_OK;
    }
    // Standard way of getting a DDraw4 interface
    if (riid == __uuidof(IDirectDraw4)) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirectDraw4");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw4");

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<DDraw4Interface> ddraw4Interface = new DDraw4Interface(std::move(ppvProxyObject), this, nullptr);
      ddraw4Interface->SetHWND(m_hwnd);
      ddraw4Interface->SetCooperativeLevel(m_cooperativeLevel);
      m_intf4 = ddraw4Interface.ptr();
      *ppvObject = ddraw4Interface.ref();

      return S_OK;
    }
    // Standard way of getting a DDraw2 interface
    if (riid == __uuidof(IDirectDraw2)) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirectDraw2");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::debug("DDrawInterface::QueryInterface: Query for IDirectDraw2");

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<DDraw2Interface> ddraw2Interface = new DDraw2Interface(std::move(ppvProxyObject), this, nullptr);
      ddraw2Interface->SetHWND(m_hwnd);
      ddraw2Interface->SetCooperativeLevel(m_cooperativeLevel);
      m_intf2 = ddraw2Interface.ptr();
      *ppvObject = ddraw2Interface.ref();

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (likely(IsLegacyInterface())) {
        Logger::debug("DDrawInterface::QueryInterface: Forwarded query for IDirectDraw7");
        return m_origin->QueryInterface(riid, ppvObject);
      }

      Logger::warn("DDrawInterface::QueryInterface: Query for IDirectDraw7");

      Com<IDirectDraw7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<DDraw7Interface> ddraw7Interface = new DDraw7Interface(std::move(ppvProxyObject));
      ddraw7Interface->SetHWND(m_hwnd);
      ddraw7Interface->SetCooperativeLevel(m_cooperativeLevel);
      *ppvObject = ddraw7Interface.ref();

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
    if (likely(IsLegacyInterface())) {
      Logger::debug(">>> DDrawInterface::CreatePalette: Forwarded");
      return m_origin->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter);
    }

    Logger::debug(">>> DDrawInterface::CreatePalette");

    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), this));
    } else {
      Logger::warn("DDrawInterface::CreatePalette: Failed to create proxy palette");
      return hr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
    if (likely(IsLegacyInterface())) {
      Logger::warn("<<< DDrawInterface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    if (unlikely(m_options.proxiedLegacySurfaces)) {
      Logger::debug("<<< DDrawInterface::CreateSurface: Proxy");
      return m_proxy->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    }

    Logger::debug(">>> DDrawInterface::CreateSurface");

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);

    if (likely(SUCCEEDED(hr))) {
      try{
        Com<DDrawSurface> surface = new DDrawSurface(std::move(ddrawSurfaceProxied), this, nullptr, nullptr, true);
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

    // Atempt to update any child interfaces, because some applications first
    // call QueryInterface, and only after that call SetCooperativeLevel
    if (m_intf2 != nullptr) {
      m_intf2->SetCooperativeLevel(dwFlags);
      m_intf2->SetHWND(m_hwnd);
    }
    if (m_intf4 != nullptr) {
      m_intf4->SetCooperativeLevel(dwFlags);
      m_intf4->SetHWND(m_hwnd);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) {
    Logger::debug("<<< DDrawInterface::SetDisplayMode: Proxy");
    return m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    Logger::debug("<<< DDrawInterface::WaitForVerticalBlank: Proxy");
    return m_proxy->WaitForVerticalBlank(dwFlags, hEvent);
  }

  bool DDrawInterface::IsWrappedSurface(IDirectDrawSurface* surface) const {
    if (unlikely(surface == nullptr))
      return false;

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(surface);
    auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddrawSurface);
    if (likely(it != m_surfaces.end()))
      return true;

    return false;
  }

  void DDrawInterface::AddWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddrawSurface);
      if (unlikely(it != m_surfaces.end())) {
        Logger::warn("DDrawInterface::AddWrappedSurface: Pre-existing wrapped surface found");
      } else {
        m_surfaces.push_back(ddrawSurface);

        if (likely(ddrawSurface->IsChildObject()))
          this->AddRef();
      }
    }
  }

  void DDrawInterface::RemoveWrappedSurface(IDirectDrawSurface* surface) {
    if (likely(surface != nullptr)) {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(surface);

      auto it = std::find(m_surfaces.begin(), m_surfaces.end(), ddrawSurface);
      if (likely(it != m_surfaces.end())) {
        m_surfaces.erase(it);

        if (likely(ddrawSurface->IsChildObject()))
          this->Release();
      } else {
        Logger::warn("DDrawInterface::RemoveWrappedSurface: Surface not found");
      }
    }
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
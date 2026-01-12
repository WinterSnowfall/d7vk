#include "d3d5_device.h"

#include "../ddraw_util.h"

#include "d3d5_texture.h"

#include "../d3d3/d3d3_device.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_interface.h"

#include <algorithm>

// Supress warnings about D3DRENDERSTATE_ALPHABLENDENABLE_OLD
// not being in the shipped D3D6 enum (thanks a lot, MS)
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wswitch"
#endif

namespace dxvk {

  uint32_t D3D5Device::s_deviceCount = 0;

  // Index buffer sizes of XS, S, M, L and XL, corresponding to 0.5 kb, 2 kb, 8 kb, 32 kb and 128 kb
  static constexpr UINT IndexCount[ddrawCaps::IndexBufferCount] = {256, 1024, 4096, 16384, D3DMAXNUMVERTICES};

  D3D5Device::D3D5Device(
      Com<IDirect3DDevice2>&& d3d5DeviceProxy,
      D3D5Interface* pParent,
      D3DDEVICEDESC2 Desc,
      GUID deviceGUID,
      d3d9::D3DPRESENT_PARAMETERS Params9,
      Com<d3d9::IDirect3DDevice9>&& pDevice9,
      DDrawSurface* pSurface,
      DWORD CreationFlags9)
    : DDrawWrappedObject<D3D5Interface, IDirect3DDevice2, d3d9::IDirect3DDevice9>(pParent, std::move(d3d5DeviceProxy), std::move(pDevice9))
    , m_DDIntfParent ( pParent->GetParent() )
    , m_commonIntf ( pParent->GetParent()->GetCommonInterface() )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_params9 ( Params9 )
    , m_desc ( Desc )
    , m_deviceGUID ( deviceGUID )
    , m_rt ( pSurface ) {
    // Get the bridge interface to D3D9
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D5Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_parent->AddRef();

    m_rtOrig = m_rt.ptr();

    HRESULT hr = EnumerateBackBuffers(m_rt->GetProxied());
    if(unlikely(FAILED(hr)))
      throw DxvkError("D3D5Device: ERROR! Failed to retrieve D3D9 back buffers!");

    if (unlikely(!m_parent->GetOptions()->disableAASupport
                &&m_parent->GetOptions()->forceEnableAA)) {
      Logger::warn("D3D5Device: Force enabling AA");
      m_d3d9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    }

    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D5Device: Created a new device nr. ((2-", m_deviceCount, "))"));
  }

  D3D5Device::~D3D5Device() {
    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->SetDevice(nullptr);
    }

    // Clear the common interface device pointer if it points to this device
    if (m_commonIntf->GetD3D5Device() == this)
      m_commonIntf->SetD3D5Device(nullptr);

    m_parent->Release();

    Logger::debug(str::format("D3D5Device: Device nr. ((2-", m_deviceCount, ")) bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D5Interface, IDirect3DDevice2, d3d9::IDirect3DDevice9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DDevice2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Device::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D5Device::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // O.D.T.: Escape... Or Die Trying queries for a D3D3 device in order to use execute buffers
    if (unlikely(riid == __uuidof(IDirect3DDevice))) {
      Logger::debug("D3D5Device::QueryInterface: Query for IDirect3DDevice");

      Com<IDirect3DDevice> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Device(std::move(ppvProxyObject), nullptr, this));

      return S_OK;
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

  HRESULT STDMETHODCALLTYPE D3D5Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    Logger::debug(">>> D3D5Device::GetCaps");

    if (unlikely(hal_desc == nullptr || hel_desc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidD3DDeviceDescSize(hal_desc->dwSize)
              || !IsValidD3DDeviceDescSize(hel_desc->dwSize)))
      return DDERR_INVALIDPARAMS;

    D3DDEVICEDESC2 desc_HAL = m_desc;
    D3DDEVICEDESC2 desc_HEL = m_desc;

    if (m_deviceGUID == IID_IDirect3DRGBDevice) {
      desc_HAL.dwFlags = 0;
      desc_HAL.dcmColorModel = 0;
      // Some applications apparently care about RGB texture caps
      desc_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
      desc_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;
    } else if (m_deviceGUID == IID_IDirect3DHALDevice) {
      desc_HEL.dcmColorModel = 0;
      desc_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    } else {
      Logger::warn("D3D5Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, sizeof(D3DDEVICEDESC2));
    memcpy(hel_desc, &desc_HEL, sizeof(D3DDEVICEDESC2));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SwapTextureHandles(IDirect3DTexture2 *tex1, IDirect3DTexture2 *tex2) {
    Logger::warn("!!! D3D5Device::SwapTextureHandles: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetStats(D3DSTATS *stats) {
    Logger::debug("<<< D3D5Device::GetStats: Proxy");
    return m_proxy->GetStats(stats);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::AddViewport(IDirect3DViewport2 *viewport) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::AddViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);
    HRESULT hr = m_proxy->AddViewport(d3d5Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    AddViewportInternal(viewport);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::DeleteViewport(IDirect3DViewport2 *viewport) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::DeleteViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);
    HRESULT hr = m_proxy->DeleteViewport(d3d5Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DeleteViewportInternal(viewport);

    // Clear the current viewport if it is deleted from the device
    if (m_currentViewport.ptr() == d3d5Viewport)
      m_currentViewport = nullptr;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::NextViewport(IDirect3DViewport2 *ref, IDirect3DViewport2 **viewport, DWORD flags) {
    Logger::warn("!!! D3D5Device::NextViewport: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx) {
    Logger::debug(">>> D3D5Device::EnumTextureFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // This is a DDSURFACEDESC in D3D5, because why not...
    DDSURFACEDESC textureFormat = { };
    textureFormat.dwSize  = sizeof(DDSURFACEDESC);
    textureFormat.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
    textureFormat.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Note: The list of formats exposed in D3D5 is restricted to the below

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D5
    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // Not supported in D3D9, but some games need
    // it to be advertised (for offscreen plain surfaces?)
    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // Not supported in D3D9, but some games may use it
    /*textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;*/

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::BeginScene() {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::BeginScene");

    RefreshLastUsedDevice();

    if (unlikely(m_inScene))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_d3d9->BeginScene();

    if (likely(SUCCEEDED(hr)))
      m_inScene = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::EndScene() {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::EndScene");

    RefreshLastUsedDevice();

    if (unlikely(!m_inScene))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_d3d9->EndScene();

    if (likely(SUCCEEDED(hr))) {
      if (m_parent->GetOptions()->forceProxiedPresent) {
        // If we have drawn anything, we need to make sure we blit back
        // the results onto the D3D6 render target before we flip it
        if (m_hasDrawn) {
          if (unlikely(!m_rt->IsInitialized()))
            m_rt->InitializeD3D9RenderTarget();
          BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_rt->GetProxied(), m_rt->GetD3D9());
        }
        m_rt->GetProxied()->Flip(m_flipRTFlags.surf, m_flipRTFlags.flags);

        if (likely(m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Strict))
          m_hasDrawn = false;
      }

      m_inScene = false;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetDirect3D(IDirect3D2 **d3d) {
    Logger::debug(">>> D3D5Device::GetDirect3D");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetCurrentViewport(IDirect3DViewport2 *viewport) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetCurrentViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D5Viewport> d3d5Viewport = static_cast<D3D5Viewport*>(viewport);
    HRESULT hr = m_proxy->SetCurrentViewport(d3d5Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    if (likely(m_currentViewport != nullptr))
      m_currentViewport->SetIsCurrentViewport(false);

    m_currentViewport = d3d5Viewport.ptr();

    m_currentViewport->SetIsCurrentViewport(true);
    m_currentViewport->ApplyViewport();
    m_currentViewport->ApplyAndActivateLights();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetCurrentViewport(IDirect3DViewport2 **viewport) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetCurrentViewport");

    if (unlikely(viewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    InitReturnPtr(viewport);

    if (unlikely(m_currentViewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    *viewport = m_currentViewport.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetRenderTarget(IDirectDrawSurface *surface, DWORD flags) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetRenderTarget");

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D5Device::SetRenderTarget: NULL render target");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D5Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_GENERIC;
    }

    DDrawSurface* rt5 = static_cast<DDrawSurface*>(surface);

    // We could technically allow unwrapped RTs when forcing proxied present,
    // however that doesn't get us anything, so simply don't bother with it
    if (unlikely(m_parent->GetOptions()->forceProxiedPresent)) {
      HRESULT hrRT = m_proxy->SetRenderTarget(rt5->GetProxied(), flags);
      if (unlikely(FAILED(hrRT))) {
        Logger::warn("D3D5Device::SetRenderTarget: Failed to set RT");
        return hrRT;
      }
    }

    // A render target surface needs to have the DDSCAPS_3DDEVICE cap
    if (unlikely(!rt5->GetCommonSurface()->Is3DSurface())) {
      Logger::err("D3D5Device::SetRenderTarget: Surface is missing DDSCAPS_3DDEVICE");
      return DDERR_INVALIDCAPS;
    }

    HRESULT hr = rt5->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    hr = m_d3d9->SetRenderTarget(0, rt5->GetD3D9());

    if (likely(SUCCEEDED(hr))) {
      Logger::debug("D3D5Device::SetRenderTarget: Set a new D3D9 RT");

      m_rt = rt5;
      m_ds = m_rt->GetAttachedDepthStencil();

      HRESULT hrDS;

      if (m_ds != nullptr) {
        Logger::debug("D3D5Device::SetRenderTarget: Found an attached DS");

        hrDS = m_ds->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D5Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
          return hrDS;
        }

        hrDS = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D5Device::SetRenderTarget: Failed to set D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D5Device::SetRenderTarget: Set a new D3D9 DS");
      } else {
        Logger::debug("D3D5Device::SetRenderTarget: RT has no depth stencil attached");

        hrDS = m_d3d9->SetDepthStencilSurface(nullptr);
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D5Device::SetRenderTarget: Failed to clear the D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D5Device::SetRenderTarget: Cleared the D3D9 DS");
      }
    } else {
      Logger::err("D3D5Device::SetRenderTarget: Failed to set D3D9 RT");
      return hr;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetRenderTarget(IDirectDrawSurface **surface) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetRenderTarget");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, D3DVERTEXTYPE dwVertexTypeDesc, DWORD dwFlags) {
    Logger::warn("!!! D3D5Device::Begin: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::BeginIndexed(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, DWORD flags) {
    Logger::warn("!!! D3D5Device::BeginIndexed: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Vertex(void *vertex) {
    Logger::warn("!!! D3D5Device::Vertex: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Index(WORD wVertexIndex) {
    Logger::warn("!!! D3D5Device::Index: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::End(DWORD dwFlags) {
    Logger::warn("!!! D3D5Device::End: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D5Device::GetRenderState: ", dwRenderStateType));

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    // As opposed to D3D7, D3D5 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D5RenderStateType(dwRenderStateType))) {
      *lpdwRenderState = 0;
      return D3D_OK;
    }

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // Replacement for later implemented GetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        if (unlikely(m_parent->GetOptions()->proxySetTexture)) {
          Logger::debug("<<< D3D5Device::GetRenderState: Proxy");
          return m_proxy->GetRenderState(dwRenderStateType, lpdwRenderState);
        }

        *lpdwRenderState = m_textureHandle;
        return D3D_OK;
      }

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_antialias;
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESS:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D6 docs state: "Note that many 3-D adapters apply texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_WRAPU:
      case D3DRENDERSTATE_WRAPV:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_linePattern);
        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        *lpdwRenderState = R2_COPYPEN;
        return D3D_OK;

      case D3DRENDERSTATE_PLANEMASK:
        *lpdwRenderState = 0;
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMAG:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMIN:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MINFILTER, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        *lpdwRenderState = m_textureMapBlend;
        return D3D_OK;

      // Replaced by D3DRENDERSTATE_ALPHABLENDENABLE
      case D3DRENDERSTATE_BLENDENABLE:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      case D3DRENDERSTATE_ZVISIBLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_SUBPIXEL:
      case D3DRENDERSTATE_SUBPIXELX:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEDALPHA:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO:
      case D3DRENDERSTATE_COLORKEYENABLE:
        *lpdwRenderState = m_commonIntf->GetColorKeyState();
        return D3D_OK;

      case D3DRENDERSTATE_ALPHABLENDENABLE_OLD:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      case D3DRENDERSTATE_BORDERCOLOR:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias  = 0;
        HRESULT res = m_d3d9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return res;
      } break;

      case D3DRENDERSTATE_ANISOTROPY:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, lpdwRenderState);
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEPATTERN00:
      case D3DRENDERSTATE_STIPPLEPATTERN01:
      case D3DRENDERSTATE_STIPPLEPATTERN02:
      case D3DRENDERSTATE_STIPPLEPATTERN03:
      case D3DRENDERSTATE_STIPPLEPATTERN04:
      case D3DRENDERSTATE_STIPPLEPATTERN05:
      case D3DRENDERSTATE_STIPPLEPATTERN06:
      case D3DRENDERSTATE_STIPPLEPATTERN07:
      case D3DRENDERSTATE_STIPPLEPATTERN08:
      case D3DRENDERSTATE_STIPPLEPATTERN09:
      case D3DRENDERSTATE_STIPPLEPATTERN10:
      case D3DRENDERSTATE_STIPPLEPATTERN11:
      case D3DRENDERSTATE_STIPPLEPATTERN12:
      case D3DRENDERSTATE_STIPPLEPATTERN13:
      case D3DRENDERSTATE_STIPPLEPATTERN14:
      case D3DRENDERSTATE_STIPPLEPATTERN15:
      case D3DRENDERSTATE_STIPPLEPATTERN16:
      case D3DRENDERSTATE_STIPPLEPATTERN17:
      case D3DRENDERSTATE_STIPPLEPATTERN18:
      case D3DRENDERSTATE_STIPPLEPATTERN19:
      case D3DRENDERSTATE_STIPPLEPATTERN20:
      case D3DRENDERSTATE_STIPPLEPATTERN21:
      case D3DRENDERSTATE_STIPPLEPATTERN22:
      case D3DRENDERSTATE_STIPPLEPATTERN23:
      case D3DRENDERSTATE_STIPPLEPATTERN24:
      case D3DRENDERSTATE_STIPPLEPATTERN25:
      case D3DRENDERSTATE_STIPPLEPATTERN26:
      case D3DRENDERSTATE_STIPPLEPATTERN27:
      case D3DRENDERSTATE_STIPPLEPATTERN28:
      case D3DRENDERSTATE_STIPPLEPATTERN29:
      case D3DRENDERSTATE_STIPPLEPATTERN30:
      case D3DRENDERSTATE_STIPPLEPATTERN31:
        *lpdwRenderState = 0;
        return D3D_OK;
    }

    // This call will never fail
    return m_d3d9->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D5Device::SetRenderState: ", dwRenderStateType));

    // As opposed to D3D7, D3D5 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D5RenderStateType(dwRenderStateType)))
      return D3D_OK;

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // Replacement for later implemented SetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        if (unlikely(m_parent->GetOptions()->proxySetTexture)) {
          Logger::debug("<<< D3D5Device::SetRenderState: Proxy");
          return m_proxy->SetRenderState(dwRenderStateType, dwRenderState);
        }

        D3D5Texture* texture5 = nullptr;

        if (likely(dwRenderState != 0)) {
          texture5 = m_DDIntfParent->GetTextureFromHandle(dwRenderState);
          if (unlikely(texture5 == nullptr))
            return DDERR_INVALIDPARAMS;
        }

        HRESULT hr = SetTextureInternal(texture5);
        if (unlikely(FAILED(hr)))
          return hr;

        break;
      }

      case D3DRENDERSTATE_ANTIALIAS:
        State9        = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        m_antialias   = dwRenderState;
        dwRenderState = m_antialias == D3DANTIALIAS_SORTDEPENDENT
                     || m_antialias == D3DANTIALIAS_SORTINDEPENDENT
                     || m_parent->GetOptions()->forceEnableAA ? TRUE : FALSE;
        break;

      case D3DRENDERSTATE_TEXTUREADDRESS:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D7 docs state: "Note that many 3-D adapters
      // apply texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        static bool s_texturePerspectiveErrorShown;

        if (!dwRenderState && !std::exchange(s_texturePerspectiveErrorShown, true))
          Logger::debug("D3D6Device::SetRenderState: Disabling of D3DRENDERSTATE_TEXTUREPERSPECTIVE is not supported");

        return D3D_OK;

      case D3DRENDERSTATE_WRAPU:
      case D3DRENDERSTATE_WRAPV:
        static bool s_wrapUVErrorShown;

        if (dwRenderState && !std::exchange(s_wrapUVErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_WRAPU/V");

        return D3D_OK;

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_linePattern = bit::cast<D3DLINEPATTERN>(dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        static bool s_monoEnableErrorShown;

        if (dwRenderState && !std::exchange(s_monoEnableErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_MONOENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        static bool s_ROP2ErrorShown;

        if (!std::exchange(s_ROP2ErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_ROP2");

        return D3D_OK;

      // "This render state is not supported by the software rasterizers, and is often ignored by hardware drivers."
      case D3DRENDERSTATE_PLANEMASK:
        return D3D_OK;

      // Docs: "[...]  only the first two (D3DFILTER_NEAREST and
      // D3DFILTER_LINEAR) are valid with D3DRENDERSTATE_TEXTUREMAG."
      case D3DRENDERSTATE_TEXTUREMAG: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, dwRenderState);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMIN: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, dwRenderState);
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE);
            break;
          // "The closest mipmap level is chosen and a point filter is applied."
          case D3DFILTER_MIPNEAREST:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The closest mipmap level is chosen and a bilinear filter is applied within it."
          case D3DFILTER_MIPLINEAR:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The two closest mipmap levels are chosen and then a linear
          //  blend is used between point filtered samples of each level."
          case D3DFILTER_LINEARMIPNEAREST:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          // "The two closest mipmap levels are chosen and then combined using a bilinear filter."
          case D3DFILTER_LINEARMIPLINEAR:
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        m_textureMapBlend = dwRenderState;

        switch (dwRenderState) {
          // "In this mode, the RGB and alpha values of the texture replace
          //  the colors that would have been used with no texturing."
          case D3DTBLEND_DECAL:
          case D3DTBLEND_COPY:
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values
          //  that would have been used with no texturing. Any alpha values in the texture
          //  replace the alpha values in the colors that would have been used with no texturing;
          //  if the texture does not contain an alpha component, alpha values at the vertices
          //  in the source are interpolated between vertices."
          case D3DTBLEND_MODULATE:
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            // TODO: Or D3DTOP_SELECTARG2 for no alpha? Patch it during SetTexture???
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "In this mode, the RGB and alpha values of the texture are blended with the colors
          //  that would have been used with no texturing, according to the following formulas [...]"
          case D3DTBLEND_DECALALPHA:
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_BLENDTEXTUREALPHA);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values that
          //  would have been used with no texturing, and the alpha values of the texture
          //  are multiplied with the alpha values that would have been used with no texturing."
          case D3DTBLEND_MODULATEALPHA:
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "Add the Gouraud interpolants to the texture lookup with saturation semantics
          //  (that is, if the color value overflows it is set to the maximum possible value)."
          case D3DTBLEND_ADD:
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_ADD);
            m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				    m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // Unsupported
          default:
          case D3DTBLEND_DECALMASK:
          case D3DTBLEND_MODULATEMASK:
            break;
        }

        return D3D_OK;

      // Replaced by D3DRENDERSTATE_ALPHABLENDENABLE
      case D3DRENDERSTATE_BLENDENABLE:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      // TODO:
      case D3DRENDERSTATE_ZVISIBLE:
        static bool s_zVisibleErrorShown;

        if (dwRenderState && !std::exchange(s_zVisibleErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_ZVISIBLE");

        return D3D_OK;

      // Docs state: "Most hardware either doesn't support it (always off) or
      // always supports it (always on).", and "All hardware should be subpixel correct.
      // Some software rasterizers are not subpixel correct because of the performance loss."
      case D3DRENDERSTATE_SUBPIXEL:
      case D3DRENDERSTATE_SUBPIXELX:
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEDALPHA:
        static bool s_stippledAlphaErrorShown;

        if (dwRenderState && !std::exchange(s_stippledAlphaErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEENABLE:
        static bool s_stippleEnableErrorShown;

        if (dwRenderState && !std::exchange(s_stippleEnableErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEENABLE");

        return D3D_OK;

      // TODO: Implement D3DRS_ANTIALIASEDLINEENABLE in D9VK.
      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO:
      case D3DRENDERSTATE_COLORKEYENABLE:
        static bool s_colorKeyEnableErrorShown;

        if (dwRenderState && !std::exchange(s_colorKeyEnableErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_COLORKEYENABLE");

        m_commonIntf->SetColorKeyState(dwRenderState);

        return D3D_OK;

      case D3DRENDERSTATE_ALPHABLENDENABLE_OLD:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      case D3DRENDERSTATE_BORDERCOLOR:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ddrawCaps::ZBIAS_SCALE);
        break;

      case D3DRENDERSTATE_ANISOTROPY:
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, dwRenderState);
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEPATTERN00:
      case D3DRENDERSTATE_STIPPLEPATTERN01:
      case D3DRENDERSTATE_STIPPLEPATTERN02:
      case D3DRENDERSTATE_STIPPLEPATTERN03:
      case D3DRENDERSTATE_STIPPLEPATTERN04:
      case D3DRENDERSTATE_STIPPLEPATTERN05:
      case D3DRENDERSTATE_STIPPLEPATTERN06:
      case D3DRENDERSTATE_STIPPLEPATTERN07:
      case D3DRENDERSTATE_STIPPLEPATTERN08:
      case D3DRENDERSTATE_STIPPLEPATTERN09:
      case D3DRENDERSTATE_STIPPLEPATTERN10:
      case D3DRENDERSTATE_STIPPLEPATTERN11:
      case D3DRENDERSTATE_STIPPLEPATTERN12:
      case D3DRENDERSTATE_STIPPLEPATTERN13:
      case D3DRENDERSTATE_STIPPLEPATTERN14:
      case D3DRENDERSTATE_STIPPLEPATTERN15:
      case D3DRENDERSTATE_STIPPLEPATTERN16:
      case D3DRENDERSTATE_STIPPLEPATTERN17:
      case D3DRENDERSTATE_STIPPLEPATTERN18:
      case D3DRENDERSTATE_STIPPLEPATTERN19:
      case D3DRENDERSTATE_STIPPLEPATTERN20:
      case D3DRENDERSTATE_STIPPLEPATTERN21:
      case D3DRENDERSTATE_STIPPLEPATTERN22:
      case D3DRENDERSTATE_STIPPLEPATTERN23:
      case D3DRENDERSTATE_STIPPLEPATTERN24:
      case D3DRENDERSTATE_STIPPLEPATTERN25:
      case D3DRENDERSTATE_STIPPLEPATTERN26:
      case D3DRENDERSTATE_STIPPLEPATTERN27:
      case D3DRENDERSTATE_STIPPLEPATTERN28:
      case D3DRENDERSTATE_STIPPLEPATTERN29:
      case D3DRENDERSTATE_STIPPLEPATTERN30:
      case D3DRENDERSTATE_STIPPLEPATTERN31:
        static bool s_stipplePatternErrorShown;

        if (!std::exchange(s_stipplePatternErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEPATTERNXX");

        return D3D_OK;
    }

    // This call will never fail
    return m_d3d9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetLightState");

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL:
        *lpdwLightState = m_materialHandle;
        break;
      case D3DLIGHTSTATE_AMBIENT:
        m_d3d9->GetRenderState(d3d9::D3DRS_AMBIENT, lpdwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        Logger::warn("D3D5Device::GetLightState: Unsupported D3DLIGHTSTATE_COLORMODEL");
        *lpdwLightState = D3DCOLOR_RGB;
        break;
      case D3DLIGHTSTATE_FOGMODE:
        m_d3d9->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        m_d3d9->GetRenderState(d3d9::D3DRS_FOGSTART, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        m_d3d9->GetRenderState(d3d9::D3DRS_FOGEND, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        m_d3d9->GetRenderState(d3d9::D3DRS_FOGDENSITY, lpdwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetLightState");

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL: {
        if (unlikely(!dwLightState))
          return D3D_OK;

        D3D5Material* material5 = m_parent->GetMaterialFromHandle(dwLightState);
        if (unlikely(material5 == nullptr))
          return DDERR_INVALIDPARAMS;

        m_materialHandle = dwLightState;
        Logger::debug(str::format("D3D5Device::SetLightState: Applying material nr. ", dwLightState, " to D3D9"));
        m_d3d9->SetMaterial(material5->GetD3D9Material());

        break;
      }
      case D3DLIGHTSTATE_AMBIENT:
        m_d3d9->SetRenderState(d3d9::D3DRS_AMBIENT, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        if (unlikely(dwLightState != D3DCOLOR_RGB))
          Logger::warn("D3D6Device::SetLightState: Unsupported D3DLIGHTSTATE_COLORMODEL");
        break;
      case D3DLIGHTSTATE_FOGMODE:
        m_d3d9->SetRenderState(d3d9::D3DRS_FOGVERTEXMODE, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        m_d3d9->SetRenderState(d3d9::D3DRS_FOGSTART, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        m_d3d9->SetRenderState(d3d9::D3DRS_FOGEND, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        m_d3d9->SetRenderState(d3d9::D3DRS_FOGDENSITY, dwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::SetTransform");
    return m_d3d9->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::GetTransform");
    return m_d3d9->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::MultiplyTransform");
    return m_d3d9->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::DrawPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE vertex_type, void *vertices, DWORD vertex_count, DWORD flags) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::DrawPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr))
      return DDERR_INVALIDPARAMS;

    DWORD vertex_type5 = ConvertVertexType(vertex_type);

    HandlePreDrawFlags(flags, vertex_type5);

    m_d3d9->SetFVF(vertex_type5);
    HRESULT hr = m_d3d9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(primitive_type),
                     GetPrimitiveCount(primitive_type, vertex_count),
                     vertices,
                     GetFVFSize(vertex_type5));

    HandlePostDrawFlags(flags, vertex_type5);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::DrawIndexedPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(primitive_type == D3DPT_POINTLIST))
      Logger::warn("D3D5Device::DrawIndexedPrimitive: D3DPT_POINTLIST primitive type");

    DWORD fvf5 = ConvertVertexType(fvf);

    HandlePreDrawFlags(flags, fvf5);

    m_d3d9->SetFVF(fvf5);
    HRESULT hr = m_d3d9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      vertices,
                      GetFVFSize(fvf5));

    HandlePostDrawFlags(flags, fvf5);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DCLIPSTATUS9 clipStatus9;
    // TODO: Split the union and intersection flags
    clipStatus9.ClipUnion        = clip_status->dwStatus;
    clipStatus9.ClipIntersection = clip_status->dwStatus;

    return m_d3d9->SetClipStatus(&clipStatus9);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
    D3D5DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DCLIPSTATUS9 clipStatus9;
    HRESULT hr = m_d3d9->GetClipStatus(&clipStatus9);

    if (FAILED(hr))
      return hr;

    D3DCLIPSTATUS clipStatus = { };
    clipStatus.dwFlags  = D3DCLIPSTATUS_STATUS;
    clipStatus.dwStatus = D3DSTATUS_DEFAULT | clipStatus9.ClipUnion | clipStatus9.ClipIntersection;

    *clip_status = clipStatus;

    return D3D_OK;
  }

  void D3D5Device::InitializeDS() {
    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      Logger::debug("D3D5Device::InitializeDS: Found an attached DS");

      HRESULT hrDS = m_ds->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D5Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        Logger::info("D3D5Device::InitializeDS: Got depth stencil from RT");

        DDSURFACEDESC descDS;
        descDS.dwSize = sizeof(DDSURFACEDESC);
        m_ds->GetProxied()->GetSurfaceDesc(&descDS);
        Logger::debug(str::format("D3D5Device::InitializeDS: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));

        HRESULT hrDS9 = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if(unlikely(FAILED(hrDS9))) {
          Logger::err("D3D5Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      Logger::info("D3D5Device::InitializeDS: RT has no depth stencil attached");
      m_d3d9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  d3d9::IDirect3DSurface9* D3D5Device::GetD3D9BackBuffer(IDirectDrawSurface* surface) const {
    if (unlikely(surface == nullptr))
      return nullptr;

    auto it = m_backBuffers.find(surface);
    if (likely(it != m_backBuffers.end()))
      return it->second.ptr();

    // Return the first back buffer if no other match is found
    return m_fallBackBuffer.ptr();
  }

  HRESULT D3D5Device::Reset(d3d9::D3DPRESENT_PARAMETERS* params) {
    Logger::info("D3D5Device::Reset: Resetting the D3D9 swapchain");
    HRESULT hr = m_bridge->ResetSwapChain(params);
    if (unlikely(FAILED(hr)))
      Logger::err("D3D5Device::Reset: Failed to reset the D3D9 swapchain");
    // Reset the current render target, so that it gets
    // re-associated with the regenerated back buffers
    m_rt->SetD3D9(nullptr);
    EnumerateBackBuffers(m_rtOrig->GetProxied());
    return hr;
  }

  inline HRESULT D3D5Device::EnumerateBackBuffers(IDirectDrawSurface* origin) {
    m_fallBackBuffer = nullptr;
    m_backBuffers.clear();

    Logger::debug("EnumerateBackBuffers: Enumerating back buffers");

    uint32_t backBufferCount = 0;
    IDirectDrawSurface* parentSurface = origin;

    // Always retrieve a fallback back buffer for use with
    // plain offscreen surfaces and overlays (even when no front buffer exists)
    HRESULT hrFallback = m_d3d9->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_fallBackBuffer);
    if (unlikely(FAILED(hrFallback)))
      return hrFallback;

    while (parentSurface != nullptr) {
      Com<d3d9::IDirect3DSurface9> surf9;

      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      parentSurface->GetSurfaceDesc(&desc);

      if (desc.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER) {
        Logger::debug("EnumerateBackBuffers: Added front buffer");
        // Always map the front buffer on top of the first back buffer to ensure that
        // direct front buffer blits some applications do are visible on screen
        HRESULT hr = m_d3d9->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf9);
        if (unlikely(FAILED(hr)))
          return hr;
      } else {
        Logger::debug(str::format("EnumerateBackBuffers: Added back buffer nr. ", backBufferCount + 1));
        const UINT backBuffer = !m_parent->GetOptions()->forceSingleBackBuffer ? backBufferCount : 0;
        HRESULT hr = m_d3d9->GetBackBuffer(0, backBuffer, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf9);
        if (unlikely(FAILED(hr)))
          return hr;
        backBufferCount++;
      }

      m_backBuffers.emplace(std::piecewise_construct,
                            std::forward_as_tuple(parentSurface),
                            std::forward_as_tuple(std::move(surf9)));

      IDirectDrawSurface* nextBackBuffer = nullptr;
      parentSurface->EnumAttachedSurfaces(&nextBackBuffer, ListBackBufferSurfaces5Callback);

      // the swapchain will eventually return to its origin
      if (nextBackBuffer == origin)
        break;

      parentSurface = nextBackBuffer;
    }

    return D3D_OK;
  }

  inline void D3D5Device::AddViewportInternal(IDirect3DViewport2* viewport) {
    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d5Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D5Device::AddViewportInternal: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d5Viewport);
      d3d5Viewport->SetDevice(this);
    }
  }

  inline void D3D5Device::DeleteViewportInternal(IDirect3DViewport2* viewport) {
    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d5Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
      d3d5Viewport->SetDevice(nullptr);
    } else {
      Logger::warn("D3D5Device::DeleteViewportInternal: Viewport not found");
    }
  }

  inline void D3D5Device::UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount) {
    Logger::debug(str::format("D3D5Device::UploadIndices: Uploading ", indexCount, " indices"));

    const size_t size = indexCount * sizeof(WORD);
    void* pData = nullptr;

    // Locking and unlocking are generally expected to work here
    ib9->Lock(0, size, &pData, D3DLOCK_DISCARD);
    memcpy(pData, static_cast<void*>(indices), size);
    ib9->Unlock();
  }

  inline HRESULT D3D5Device::SetTextureInternal(D3D5Texture* texture5) {
    Logger::debug(">>> D3D5Device::SetTextureInternal");

    HRESULT hr;

    // Unbinding texture stages
    if (texture5 == nullptr) {
      Logger::debug("D3D5Device::SetTextureInternal: Unbiding D3D9 texture");

      hr = m_d3d9->SetTexture(0, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_textureHandle != 0) {
          Logger::debug("D3D5Device::SetTextureInternal: Unbinding local texture");
          m_textureHandle = 0;
        }
      } else {
        Logger::err("D3D5Device::SetTextureInternal: Failed to unbind D3D9 texture");
      }

      return hr;
    }

    Logger::debug("D3D5Device::SetTextureInternal: Binding D3D9 texture");

    DDrawSurface* surface5 = texture5->GetParent();

    // Only upload textures if any sort of blit/lock operation
    // has been performed on them since the last SetTexture call
    if (surface5->GetCommonSurface()->HasDirtyMipMaps() || m_parent->GetOptions()->alwaysDirtyMipMaps) {
      hr = surface5->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D5Device::SetTextureInternal: Failed to initialize/upload D3D9 texture");
        return hr;
      }

      surface5->GetCommonSurface()->UnDirtyMipMaps();
    } else {
      Logger::debug("D3D5Device::SetTextureInternal: Skipping upload of texture and mip maps");
    }

    if (unlikely(m_textureHandle == texture5->GetHandle()))
      return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = surface5->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = m_d3d9->SetTexture(0, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D6Device::SetTextureInternal: Failed to bind D3D9 texture");
        return hr;
      }
    }

    m_textureHandle = texture5->GetHandle();

    return D3D_OK;
  }

}
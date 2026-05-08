#include "d3d5_device.h"

#include "../ddraw_util.h"

#include "../d3d_common_texture.h"
#include "../ddraw_common_interface.h"

#include "../d3d6/d3d6_device.h"
#include "../d3d3/d3d3_device.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_interface.h"

#include <algorithm>

// Supress warnings about D3DRENDERSTATE_ALPHABLENDENABLE_OLD
// not being in the shipped D3D5 enum (thanks a lot, MS)
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wswitch"
#endif

namespace dxvk {

  uint32_t D3D5Device::s_deviceCount = 0;

  D3D5Device::D3D5Device(
        D3DCommonDevice* commonD3DDevice,
        Com<IDirect3DDevice2>&& d3d5DeviceProxy,
        D3D5Interface* pParent,
        D3DDEVICEDESC2 Desc,
        GUID deviceGUID,
        d3d9::D3DPRESENT_PARAMETERS Params9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDrawSurface* pSurface,
        DWORD CreationFlags9)
    : DDrawWrappedObject<D3D5Interface, IDirect3DDevice2>(pParent, std::move(d3d5DeviceProxy))
    , m_commonD3DDevice ( commonD3DDevice )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_desc ( Desc )
    , m_rt ( pSurface ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonD3DDevice != nullptr) {
      m_commonIntf = m_commonD3DDevice->GetCommonInterface();
    } else {
      throw DxvkError("D3D5Device: ERROR! Failed to retrieve the common interface!");
    }

    d3d9::IDirect3DDevice9* device9;

    if (likely(m_commonD3DDevice == nullptr)) {
      m_commonD3DDevice = new D3DCommonDevice(m_commonIntf, deviceGUID, Params9, CreationFlags9);

      m_commonD3DDevice->SetD3D9Device(std::move(pDevice9));
      device9 = m_commonD3DDevice->GetD3D9Device();

      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
        Logger::warn("D3D5Device: Force enabling AA");
        device9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
      }

      // The default value of D3DRENDERSTATE_TEXTUREMAPBLEND in D3D5 is D3DTBLEND_MODULATE
      device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
      device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
      device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
      device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
      device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    } else {
      device9 = m_commonD3DDevice->GetD3D9Device();
      // Very important, otherwise the depth stencil isn't dirtied on draws
      m_ds = m_rt->GetAttachedDepthStencil();
    }

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(device9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D5Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (unlikely(!m_commonD3DDevice->GetTotalTextureMemory()))
      m_commonD3DDevice->SetTotalTextureMemory(m_bridge->DetermineInitialTextureMemory());

    // Update D3D9 legacy light state
    m_bridge->SetLegacyLightsState(true);

    if (m_commonD3DDevice->GetOrigin() == nullptr)
      m_commonD3DDevice->SetOrigin(this);

    m_commonD3DDevice->SetD3D5Device(this);

    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D5Device: Created a new device nr. ((2-", m_deviceCount, "))"));
  }

  D3D5Device::~D3D5Device() {
    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->GetCommonViewport()->SetD3D5Device(nullptr);
    }

    if (m_commonD3DDevice->GetD3D5Device() == this)
      m_commonD3DDevice->SetD3D5Device(nullptr);

    if (m_commonD3DDevice->GetOrigin() == this)
      m_commonD3DDevice->SetOrigin(nullptr);

    Logger::debug(str::format("D3D5Device: Device nr. ((2-", m_deviceCount, ")) bites the dust"));
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D5Device::AddRef() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D5Device::Release() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Device::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice))) {
      if (m_commonD3DDevice->GetD3D3Device() != nullptr) {
        Logger::debug("D3D3Device::QueryInterface: Query for existing IDirect3DDevice");
        return m_commonD3DDevice->GetD3D3Device()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D5Device::QueryInterface: Query for IDirect3DDevice");

      Com<IDirect3DDevice> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      // Reuse the existing D3D9 device in situations where games want
      // to get access only to D3D3 execute buffers on a D3D5 device
      m_device3 = new D3D3Device(m_commonD3DDevice.ptr(), std::move(ppvProxyObject),
                                 m_rt.ptr(), GetD3D3Caps(d3dOptions), m_commonD3DDevice->GetDeviceGUID(),
                                 m_commonD3DDevice->GetPresentParameters(), nullptr,
                                 m_commonD3DDevice->GetD3D9CreationFlags());
      m_commonD3DDevice->SetD3D3Device(m_device3.ptr());

      // On native this is the same object, so no need to ref
      *ppvObject = m_device3.ptr();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DDevice3))) {
      if (m_commonD3DDevice->GetD3D6Device() != nullptr) {
        Logger::debug("D3D5Device::QueryInterface: Query for existing IDirect3DDevice3");
        return m_commonD3DDevice->GetD3D6Device()->QueryInterface(riid, ppvObject);
      }

      // A D3D5 device shouldn't be able to create a D3D6 device
      // if it doesn't previously exist as a parent/origin device
      Logger::err("D3D5Device::QueryInterface: Query for IDirect3DDevice3");
      return E_NOINTERFACE;
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

    const GUID deviceGUID = m_commonD3DDevice->GetDeviceGUID();

    if (deviceGUID == IID_IDirect3DRGBDevice) {
      desc_HAL.dwFlags = 0;
      desc_HAL.dcmColorModel = 0;
      // Some applications apparently care about RGB texture caps
      desc_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
      desc_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;
    } else if (deviceGUID == IID_IDirect3DHALDevice) {
      desc_HEL.dcmColorModel = 0;
      desc_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    } else {
      Logger::warn("D3D5Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, hal_desc->dwSize);
    memcpy(hel_desc, &desc_HEL, hel_desc->dwSize);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SwapTextureHandles(IDirect3DTexture2 *tex1, IDirect3DTexture2 *tex2) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SwapTextureHandles");

    D3D5Texture* texture1 = static_cast<D3D5Texture*>(tex1);
    D3D5Texture* texture2 = static_cast<D3D5Texture*>(tex2);

    D3DCommonTexture* commonTex1 = texture1->GetCommonTexture();
    D3DCommonTexture* commonTex2 = texture2->GetCommonTexture();

    const D3DTEXTUREHANDLE handle1 = commonTex1->GetTextureHandle();
    const D3DTEXTUREHANDLE handle2 = commonTex2->GetTextureHandle();

    m_commonIntf->ReleaseTextureHandle(handle1);
    m_commonIntf->ReleaseTextureHandle(handle2);

    commonTex1->SetTextureHandle(handle2);
    commonTex2->SetTextureHandle(handle1);

    m_commonIntf->EmplaceTexture(commonTex1, handle2);
    m_commonIntf->EmplaceTexture(commonTex2, handle1);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetStats(D3DSTATS *stats) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetStats");

    if (unlikely(stats == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stats->dwSize != sizeof(D3DSTATS)))
      return DDERR_INVALIDPARAMS;

    D3DSTATS newStats = { };

    if (likely(m_commonD3DDevice->GetD3D3Device() != nullptr))
      newStats = m_commonD3DDevice->GetD3D3Device()->GetStatsInternal();

    const DWORD dwSize = stats->dwSize;

    *stats = newStats;
    stats->dwSize = dwSize;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::AddViewport(IDirect3DViewport2 *viewport) {
    D3DDeviceLock lock = LockDevice();

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
    D3DDeviceLock lock = LockDevice();

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

  HRESULT STDMETHODCALLTYPE D3D5Device::NextViewport(IDirect3DViewport2 *lpDirect3DViewport, IDirect3DViewport2 **lplpAnotherViewport, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::NextViewport");

    if (unlikely(lplpAnotherViewport == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpAnotherViewport);

    if (flags & D3DNEXT_HEAD) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DViewport == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(m_viewports.size() > 0))
        Logger::warn("D3D5Device::NextViewport: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx) {
    Logger::debug(">>> D3D5Device::EnumTextureFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // This is a DDSURFACEDESC in D3D5, because why not...
    DDSURFACEDESC textureFormat = { };
    textureFormat.dwSize  = sizeof(DDSURFACEDESC);
    textureFormat.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
    textureFormat.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Note: The list of formats exposed in D3D5 is restricted to the below

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D5
    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // Not supported in D3D9, but some games need
    // it to be advertised (for offscreen plain surfaces?)
    if (unlikely(d3dOptions->supportR3G3B2)) {
      textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
      hr = cb(&textureFormat, ctx);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    // Not supported in D3D9, but some games may use it
    /*textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;*/

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::BeginScene() {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::BeginScene");

    RefreshLastUsedDevice();

    if (unlikely(m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->BeginScene();

    if (likely(SUCCEEDED(hr)))
      m_commonD3DDevice->SetInScene(true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::EndScene() {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::EndScene");

    RefreshLastUsedDevice();

    if (unlikely(!m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->EndScene();

    if (likely(SUCCEEDED(hr)))
      m_commonD3DDevice->SetInScene(false);

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
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetCurrentViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D5Viewport> d3d5Viewport = static_cast<D3D5Viewport*>(viewport);
    HRESULT hr = m_proxy->SetCurrentViewport(d3d5Viewport->GetProxied());
    if (unlikely(FAILED(hr))) {
      Logger::debug("D3D5Device::SetCurrentViewport: Failed to set proxied viewport");
      return hr;
    }

    if (unlikely(m_currentViewport == d3d5Viewport))
      return D3D_OK;

    if (likely(m_currentViewport != nullptr)) {
      m_currentViewport->DeactivateLights();
      m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(false);
    }

    m_currentViewport = d3d5Viewport.ptr();

    m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(true);
    m_currentViewport->ApplyViewport();
    m_currentViewport->ApplyAndActivateLights();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetCurrentViewport(IDirect3DViewport2 **viewport) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetCurrentViewport");

    // This does indeed return D3DERR_NOCURRENTVIEWPORT...
    if (unlikely(viewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    // Current viewport is checked before initializing the return pointer
    if (unlikely(m_currentViewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    InitReturnPtr(viewport);

    *viewport = m_currentViewport.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetRenderTarget(IDirectDrawSurface *surface, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetRenderTarget");

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D5Device::SetRenderTarget: NULL render target");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D5Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_UNSUPPORTED;
    }

    DDrawSurface* rt5 = static_cast<DDrawSurface*>(surface);

    // Needed to ensure proxied Z/Stencil viewport clears will work
    HRESULT hrRT = m_proxy->SetRenderTarget(rt5->GetShadowOrProxied(), flags);
    if (unlikely(FAILED(hrRT)))
      Logger::debug("D3D5Device::SetRenderTarget: Failed to set RT");

    HRESULT hr = rt5->GetCommonSurface()->ValidateRTUsage();
    if (unlikely(FAILED(hr)))
      return hr;

    hr = rt5->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    hr = device9->SetRenderTarget(0, rt5->GetCommonSurface()->GetD3D9Surface());

    if (likely(SUCCEEDED(hr))) {
      Logger::debug("D3D5Device::SetRenderTarget: Set a new D3D9 RT");

      m_rt = rt5;
      m_ds = m_rt->GetAttachedDepthStencil();

      HRESULT hrDS;

      if (m_ds != nullptr) {
        Logger::debug("D3D5Device::SetRenderTarget: Found an attached DS");

        hrDS = m_ds->InitializeD3D9DepthStencil();
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D5Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
          return hrDS;
        }

        hrDS = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D5Device::SetRenderTarget: Failed to set D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D5Device::SetRenderTarget: Set a new D3D9 DS");
      } else {
        Logger::debug("D3D5Device::SetRenderTarget: RT has no depth stencil attached");

        hrDS = device9->SetDepthStencilSurface(nullptr);
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
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetRenderTarget");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, D3DVERTEXTYPE dwVertexTypeDesc, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::Begin");

    m_vertexStreamInfo.d3dpt = d3dptPrimitiveType;
    m_vertexStreamInfo.d3dvt = dwVertexTypeDesc;
    m_vertexStreamInfo.dwFlags = dwFlags;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::BeginIndexed(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, DWORD flags) {
    Logger::warn("!!! D3D5Device::BeginIndexed: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Vertex(void *vertex) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::Vertex");

    if (unlikely(vertex == nullptr))
      return DDERR_INVALIDPARAMS;

    if (m_vertexStreamInfo.d3dvt == D3DVT_VERTEX) {
      m_vertexStream.push_back(*reinterpret_cast<D3DVERTEX*>(vertex));
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_LVERTEX) {
      m_lvertexStream.push_back(*reinterpret_cast<D3DLVERTEX*>(vertex));
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_TLVERTEX) {
      m_tlvertexStream.push_back(*reinterpret_cast<D3DTLVERTEX*>(vertex));
    } else {
      Logger::warn(">>> D3D5Device::Vertex: Invalid vertex type");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::Index(WORD wVertexIndex) {
    Logger::warn("!!! D3D5Device::Index: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::End(DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::End");

    HRESULT hr;
    if (m_vertexStreamInfo.d3dvt == D3DVT_VERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_vertexStream.data(),
                         m_vertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_vertexStream.clear();
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_LVERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_lvertexStream.data(),
                         m_lvertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_lvertexStream.clear();
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_TLVERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_tlvertexStream.data(),
                         m_tlvertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_tlvertexStream.clear();
    } else {
      Logger::warn(">>> D3D5Device::End: Invalid vertex type");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(FAILED(hr)))
      Logger::warn(">>> D3D5Device::End: Failed call to DrawPrimitive");

    m_vertexStreamInfo = { };

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D5Device::GetRenderState: ", dwRenderStateType));

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    // As opposed to D3D7, D3D5 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D5RenderStateType(dwRenderStateType)
              && !m_commonIntf->GetOptions()->apitraceMode)) {
      Logger::debug(str::format("D3D5Device::GetRenderState: Invalid render state ", dwRenderStateType));
      *lpdwRenderState = 0;
      return D3D_OK;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // Replacement for later implemented GetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE:
        *lpdwRenderState = m_commonD3DDevice->GetCurrentTextureHandle();
        return D3D_OK;

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_commonD3DDevice->GetAntialias();
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESS:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, though default FALSE in D3D5
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        *lpdwRenderState = value9 & D3DWRAP_U;
        return D3D_OK;
      }

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        *lpdwRenderState = value9 & D3DWRAP_V;
        return D3D_OK;
      }

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_commonD3DDevice->GetLinePattern());
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
        device9->GetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMIN: {
        DWORD minFilter = 0;
        DWORD mipFilter = 0;
        device9->GetSamplerState(0, d3d9::D3DSAMP_MINFILTER, &minFilter);
        device9->GetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, &mipFilter);
        *lpdwRenderState = DecodeTextureMinValues(minFilter, mipFilter);
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        *lpdwRenderState = m_commonD3DDevice->GetTextureMapBlend();
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

      case D3DRENDERSTATE_STIPPLEDALPHA:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_STIPPLEENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE:
        *lpdwRenderState = m_commonD3DDevice->GetColorKeyEnable();
        return D3D_OK;

      case D3DRENDERSTATE_ALPHABLENDENABLE_OLD:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      case D3DRENDERSTATE_BORDERCOLOR:
        device9->GetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        device9->GetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias = 0;
        device9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return D3D_OK;
      }

      case D3DRENDERSTATE_ANISOTROPY:
        device9->GetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, lpdwRenderState);
        return D3D_OK;

      // Not mentioned in the D3D5 docs, but seen in the wild. D3D6 docs state:
      // "Flush any pending DrawPrimitive batches. When rendering with texture handles
      // (using the IDirect3DDevice2 interface) you must flush batched primitives
      // after modifying the current texture surface."
      case D3DRENDERSTATE_FLUSHBATCH:
        *lpdwRenderState = TRUE;
        return D3D_OK;

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
    return device9->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D5Device::SetRenderState: ", dwRenderStateType));

    // As opposed to D3D7, D3D5 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D5RenderStateType(dwRenderStateType))) {
      Logger::debug(str::format("D3D5Device::SetRenderState: Invalid render state ", dwRenderStateType));
      return D3D_OK;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // Replacement for later implemented SetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        DDrawSurface* surface = nullptr;

        if (likely(dwRenderState != 0)) {
          surface = m_commonIntf->GetSurfaceFromTextureHandle(dwRenderState);
          if (unlikely(surface == nullptr))
            return DDERR_INVALIDPARAMS;
        }

        HRESULT hr = SetTextureInternal(surface, dwRenderState);
        if (unlikely(FAILED(hr)))
          return hr;

        if (unlikely(surface == nullptr))
          m_bridge->SetColorKeyState(false);

        break;
      }

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                    || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT))
            Logger::warn("D3D5Device::SetRenderState: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9        = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        m_commonD3DDevice->SetAntialias(dwRenderState);
        dwRenderState = dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                     || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT
                     || d3dOptions->emulateFSAA == FSAAEmulation::Forced ? TRUE : FALSE;
        break;
      }

      case D3DRENDERSTATE_TEXTUREADDRESS:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, though default FALSE in D3D5
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        return D3D_OK;

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & D3DWRAP_U);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_U);
        }
        return D3D_OK;
      }

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & D3DWRAP_V);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_V);
        }
        return D3D_OK;
      }

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_commonD3DDevice->SetLinePattern(bit::cast<D3DLINEPATTERN>(dwRenderState));
        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        static bool s_monoEnableErrorShown;

        if (dwRenderState && !std::exchange(s_monoEnableErrorShown, true))
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_MONOENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        static bool s_ROP2ErrorShown;

        if (!std::exchange(s_ROP2ErrorShown, true))
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_ROP2");

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
            device9->SetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, dwRenderState);
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
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, dwRenderState);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE);
            break;
          // "The closest mipmap level is chosen and a point filter is applied."
          case D3DFILTER_MIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The closest mipmap level is chosen and a bilinear filter is applied within it."
          case D3DFILTER_MIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The two closest mipmap levels are chosen and then a linear
          //  blend is used between point filtered samples of each level."
          case D3DFILTER_LINEARMIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          // "The two closest mipmap levels are chosen and then combined using a bilinear filter."
          case D3DFILTER_LINEARMIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        m_commonD3DDevice->SetTextureMapBlend(dwRenderState);

        switch (dwRenderState) {
          // "In this mode, the RGB and alpha values of the texture replace
          //  the colors that would have been used with no texturing."
          case D3DTBLEND_DECAL:
          case D3DTBLEND_COPY:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values
          //  that would have been used with no texturing. Any alpha values in the texture
          //  replace the alpha values in the colors that would have been used with no texturing;
          //  if the texture does not contain an alpha component, alpha values at the vertices
          //  in the source are interpolated between vertices."
          case D3DTBLEND_MODULATE:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "In this mode, the RGB and alpha values of the texture are blended with the colors
          //  that would have been used with no texturing, according to the following formulas [...]"
          case D3DTBLEND_DECALALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_BLENDTEXTUREALPHA);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values that
          //  would have been used with no texturing, and the alpha values of the texture
          //  are multiplied with the alpha values that would have been used with no texturing."
          case D3DTBLEND_MODULATEALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            break;
          // "Add the Gouraud interpolants to the texture lookup with saturation semantics
          //  (that is, if the color value overflows it is set to the maximum possible value)."
          case D3DTBLEND_ADD:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_ADD);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
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

      // Safe to ignore. Docs state: "Direct3D's retained mode uses this operation as a
      // quick-reject test: it does the z-visible test on the bounding box of a set of
      // primitives and only renders them if it returns TRUE."
      case D3DRENDERSTATE_ZVISIBLE:
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
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEENABLE:
        static bool s_stippleEnableErrorShown;

        if (dwRenderState && !std::exchange(s_stippleEnableErrorShown, true))
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE: {
        m_commonD3DDevice->SetColorKeyEnable(dwRenderState);

        const D3DTEXTUREHANDLE currentTextureHandle = m_commonD3DDevice->GetCurrentTextureHandle();
        DDrawSurface* surface = currentTextureHandle != 0 ?
                                m_commonIntf->GetSurfaceFromTextureHandle(currentTextureHandle) : nullptr;
        const bool validColorKey = surface != nullptr ? surface->GetCommonSurface()->HasValidColorKey() : false;
        m_bridge->SetColorKeyState(dwRenderState && validColorKey);
        if (dwRenderState && validColorKey) {
          DDCOLORKEY normalizedColorKey = surface->GetCommonSurface()->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }

        return D3D_OK;
      }

      case D3DRENDERSTATE_ALPHABLENDENABLE_OLD:
        State9 = d3d9::D3DRS_ALPHABLENDENABLE;
        break;

      case D3DRENDERSTATE_BORDERCOLOR:
        device9->SetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        device9->SetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ddrawCaps::ZBIAS_SCALE);
        break;

      case D3DRENDERSTATE_ANISOTROPY:
        device9->SetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, dwRenderState);
        return D3D_OK;

      // Not mentioned in the D3D5 docs, but seen in the wild. D3D6 docs state:
      // "Flush any pending DrawPrimitive batches. When rendering with texture handles
      // (using the IDirect3DDevice2 interface) you must flush batched primitives
      // after modifying the current texture surface."
      case D3DRENDERSTATE_FLUSHBATCH:
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
          Logger::warn("D3D5Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEPATTERN");

        return D3D_OK;
    }

    // This call will never fail
    return device9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::GetLightState");

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL:
        *lpdwLightState = m_commonD3DDevice->GetCurrentMaterialHandle();
        break;
      case D3DLIGHTSTATE_AMBIENT:
        device9->GetRenderState(d3d9::D3DRS_AMBIENT, lpdwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        *lpdwLightState = D3DCOLOR_RGB;
        break;
      case D3DLIGHTSTATE_FOGMODE:
        device9->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        device9->GetRenderState(d3d9::D3DRS_FOGSTART, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        device9->GetRenderState(d3d9::D3DRS_FOGEND, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        device9->GetRenderState(d3d9::D3DRS_FOGDENSITY, lpdwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::SetLightState");

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL: {
        if (unlikely(!dwLightState)) {
          m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
          return D3D_OK;
        }

        d3d9::D3DMATERIAL9* material9 = m_parent->GetCommonD3DInterface()->GetD3D9MaterialFromHandle(dwLightState);
        if (unlikely(material9 == nullptr))
          return DDERR_INVALIDPARAMS;

        m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
        Logger::debug(str::format("D3D5Device::SetLightState: Applying material nr. ", dwLightState, " to D3D9"));
        device9->SetMaterial(material9);

        break;
      }
      case D3DLIGHTSTATE_AMBIENT:
        device9->SetRenderState(d3d9::D3DRS_AMBIENT, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        if (unlikely(dwLightState != D3DCOLOR_RGB))
          Logger::warn("D3D5Device::SetLightState: Unsupported D3DLIGHTSTATE_COLORMODEL");
        break;
      case D3DLIGHTSTATE_FOGMODE:
        device9->SetRenderState(d3d9::D3DRS_FOGVERTEXMODE, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        device9->SetRenderState(d3d9::D3DRS_FOGSTART, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        device9->SetRenderState(d3d9::D3DRS_FOGEND, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        device9->SetRenderState(d3d9::D3DRS_FOGDENSITY, dwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::SetTransform");
    return m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::GetTransform");
    return m_commonD3DDevice->GetD3D9Device()->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D5Device::MultiplyTransform");
    return m_commonD3DDevice->GetD3D9Device()->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::DrawPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE vertex_type, void *vertices, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::DrawPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeOrUploadD3D9();
    if (likely(m_ds != nullptr))
      m_ds->InitializeOrUploadD3D9();

    DWORD vertex_type5 = ConvertVertexType(vertex_type);

    HandlePreDrawFlags(flags, vertex_type5);
    HandlePreDrawLegacyProjection(flags);

    device9->SetFVF(vertex_type5);
    HRESULT hr = device9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(primitive_type),
                     GetPrimitiveCount(primitive_type, vertex_count),
                     vertices,
                     GetFVFSize(vertex_type5));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, vertex_type5);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D5Device::DrawIndexedPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeOrUploadD3D9();
    if (likely(m_ds != nullptr))
      m_ds->InitializeOrUploadD3D9();

    const DWORD fvf5 = ConvertVertexType(fvf);

    HandlePreDrawFlags(flags, fvf5);
    HandlePreDrawLegacyProjection(flags);

    device9->SetFVF(fvf5);
    HRESULT hr = device9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      vertices,
                      GetFVFSize(fvf5));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, fvf5);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    Logger::debug(">>> D3D5Device::SetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
    Logger::debug(">>> D3D5Device::GetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DVIEWPORT9 viewport9;
    if (SUCCEEDED(m_commonD3DDevice->GetD3D9Device()->GetViewport(&viewport9))) {
      clip_status->dwFlags = D3DCLIPSTATUS_EXTENTS2;
      clip_status->dwStatus = 0;
      clip_status->minx = viewport9.X;
      clip_status->maxx = viewport9.X + viewport9.Height;
      clip_status->miny = viewport9.Y;
      clip_status->maxy = viewport9.Y + viewport9.Width;
      clip_status->minz = 0;
      clip_status->maxz = 0;
    }

    return D3D_OK;
  }

  void D3D5Device::InitializeDS() {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      Logger::debug("D3D5Device::InitializeDS: Found an attached DS");

      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D5Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        Logger::info("D3D5Device::InitializeDS: Got depth stencil from RT");

        DDSURFACEDESC descDS;
        descDS.dwSize = sizeof(DDSURFACEDESC);
        m_ds->GetProxied()->GetSurfaceDesc(&descDS);
        Logger::debug(str::format("D3D5Device::InitializeDS: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));

        HRESULT hrDS9 = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
        if(unlikely(FAILED(hrDS9))) {
          Logger::err("D3D5Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      Logger::info("D3D5Device::InitializeDS: RT has no depth stencil attached");
      device9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  void D3D5Device::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if(likely(dirtyRenderTarget))
      m_rt->GetCommonSurface()->DirtyD3D9Surface();

    if (likely(dirtyPrimarySurface)) {
      DDrawCommonSurface* primarySurface = m_commonIntf->GetPrimarySurface();
      // The primary surface can be bound as RT, in which case it will
      // get dirtied twice, but we have no guarantees that will happen
      if (likely(primarySurface != nullptr))
        primarySurface->DirtyD3D9Surface();
    }

    if (likely(dirtyDepthStencil && m_ds != nullptr))
      m_ds->GetCommonSurface()->DirtyD3D9Surface();
  }

  inline void D3D5Device::AddViewportInternal(IDirect3DViewport2* viewport) {
    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d5Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D5Device::AddViewportInternal: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d5Viewport);
      d3d5Viewport->GetCommonViewport()->SetD3D5Device(this);
    }
  }

  inline void D3D5Device::DeleteViewportInternal(IDirect3DViewport2* viewport) {
    D3D5Viewport* d3d5Viewport = static_cast<D3D5Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d5Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
      d3d5Viewport->GetCommonViewport()->SetD3D5Device(nullptr);
    } else {
      Logger::warn("D3D5Device::DeleteViewportInternal: Viewport not found");
    }
  }

  inline HRESULT D3D5Device::SetTextureInternal(DDrawSurface* surface, DWORD textureHandle) {
    Logger::debug(">>> D3D5Device::SetTextureInternal");

    HRESULT hr;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // Unbinding texture stages
    if (surface == nullptr) {
      Logger::debug("D3D5Device::SetTextureInternal: Unbiding D3D9 texture");

      hr = device9->SetTexture(0, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_commonD3DDevice->GetCurrentTextureHandle() != 0) {
          Logger::debug("D3D5Device::SetTextureInternal: Unbinding local texture");
          m_commonD3DDevice->SetCurrentTextureHandle(0);
        }
      } else {
        Logger::err("D3D5Device::SetTextureInternal: Failed to unbind D3D9 texture");
      }

      return hr;
    }

    Logger::debug("D3D5Device::SetTextureInternal: Binding D3D9 texture");

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(surface->GetCommonSurface()->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
      surface->GetCommonSurface()->DirtyDDrawSurface();

    hr = surface->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::D3D5Device: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_commonD3DDevice->GetCurrentTextureHandle() == textureHandle))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = surface->GetCommonSurface()->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = device9->SetTexture(0, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D5Device::SetTextureInternal: Failed to bind D3D9 texture");
        return hr;
      }

      // "Any alpha values in the texture replace the alpha values in the colors that would
      //  have been used with no texturing; if the texture does not contain an alpha component,
      //  alpha values at the vertices in the source are interpolated between vertices."
      if (m_commonD3DDevice->GetTextureMapBlend() == D3DTBLEND_MODULATE) {
        const DWORD textureOp = surface->GetCommonSurface()->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE;
        device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
      }

      const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
      const bool validColorKey = surface->GetCommonSurface()->HasValidColorKey();
      m_bridge->SetColorKeyState(colorKeyEnable && validColorKey);
      if (colorKeyEnable && validColorKey) {
        DDCOLORKEY normalizedColorKey = surface->GetCommonSurface()->GetColorKeyNormalized();
        m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                              normalizedColorKey.dwColorSpaceHighValue);
      }
    }

    m_commonD3DDevice->SetCurrentTextureHandle(textureHandle);

    return D3D_OK;
  }

}
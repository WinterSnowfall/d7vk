#include "d3d6_device.h"

#include "../ddraw_common_interface.h"

#include "d3d6_buffer.h"
#include "d3d6_texture.h"

#include "../d3d5/d3d5_device.h"
#include "../d3d3/d3d3_device.h"

#include "../ddraw4/ddraw4_surface.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D6Device::s_deviceCount = 0;

  D3D6Device::D3D6Device(
        D3DCommonDevice* commonD3DDevice,
        Com<IDirect3DDevice3>&& d3d6DeviceProxy,
        D3D6Interface* pParent,
        D3DDEVICEDESC Desc,
        GUID deviceGUID,
        d3d9::D3DPRESENT_PARAMETERS Params9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDraw4Surface* pSurface,
        DWORD CreationFlags9)
    : DDrawWrappedObject<D3D6Interface, IDirect3DDevice3, d3d9::IDirect3DDevice9>(pParent, std::move(d3d6DeviceProxy), std::move(pDevice9))
    , m_commonD3DDevice ( commonD3DDevice )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_desc ( Desc )
    , m_rt ( pSurface ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonD3DDevice != nullptr) {
      m_commonIntf = m_commonD3DDevice->GetCommonInterface();
    } else {
      throw DxvkError("D3D6Device: ERROR! Failed to retrieve the common interface!");
    }

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    // Common D3D9 index buffers
    if (unlikely(FAILED(InitializeIndexBuffers()))) {
      throw DxvkError("D3D6Device: ERROR! Failed to initialize D3D9 index buffers.");
    }

    if (likely(m_commonD3DDevice == nullptr)) {
      m_commonD3DDevice = new D3DCommonDevice(m_commonIntf, deviceGUID, Params9, CreationFlags9,
                                              m_bridge->DetermineInitialTextureMemory());

      // Update D3D9 legacy light state
      m_bridge->SetLegacyLightsState(true);

      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
        Logger::warn("D3D6Device: Force enabling AA");
        m_d3d9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
      }

      // The default value of D3DRENDERSTATE_TEXTUREMAPBLEND in D3D6 is D3DTBLEND_MODULATE
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }

    if (m_commonD3DDevice->GetOrigin() == nullptr)
      m_commonD3DDevice->SetOrigin(this);

    m_commonD3DDevice->SetD3D6Device(this);

    m_textures.fill(nullptr);

    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D6Device: Created a new device nr. ((3-", m_deviceCount, "))"));
  }

  D3D6Device::~D3D6Device() {
    if (LogIndexBufferUsageStats()) {
      Logger::debug("D3D6Device: Index buffer upload statistics:");
      Logger::debug(str::format("   0.5 kb : ", m_ib9_uploads[0]));
      Logger::debug(str::format("   1   kb : ", m_ib9_uploads[1]));
      Logger::debug(str::format("   2   kb : ", m_ib9_uploads[2]));
      Logger::debug(str::format("   4   kb : ", m_ib9_uploads[3]));
      Logger::debug(str::format("   8   kb : ", m_ib9_uploads[4]));
      Logger::debug(str::format("   16  kb : ", m_ib9_uploads[5]));
      Logger::debug(str::format("   32  kb : ", m_ib9_uploads[6]));
      Logger::debug(str::format("   64  kb : ", m_ib9_uploads[7]));
      Logger::debug(str::format("   128 kb : ", m_ib9_uploads[8]));
    }

    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->GetCommonViewport()->SetD3D6Device(nullptr);
    }

    if (m_commonD3DDevice->GetD3D6Device() == this)
      m_commonD3DDevice->SetD3D6Device(nullptr);

    if (m_commonD3DDevice->GetOrigin() == this)
      m_commonD3DDevice->SetOrigin(nullptr);

    Logger::debug(str::format("D3D6Device: Device nr. ((3-", m_deviceCount, ")) bites the dust"));
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D6Device::AddRef() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D6Device::Release() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Device::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirect3DDevice)) {
      if (m_commonD3DDevice->GetD3D3Device() != nullptr) {
        Logger::debug("D3D6Device::QueryInterface: Query for existing IDirect3DDevice");
        return m_commonD3DDevice->GetD3D3Device()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D6Device::QueryInterface: Query for IDirect3DDevice");

      Com<IDirect3DDevice> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      // TODO: Make sure the RT has an existing DDrawSurface,
      // and QueryInterface for one if that's not the case

      // Reuse the existing D3D9 device in situations where games want
      // to get access only to D3D3 execute buffers on a D3D6 device
      Com<d3d9::IDirect3DDevice9> device9 = m_d3d9.ptr();
      m_device3 = new D3D3Device(m_commonD3DDevice.ptr(), std::move(ppvProxyObject),
                                 m_rt->GetCommonSurface()->GetDDSurface(), GetD3D3Caps(d3dOptions),
                                 m_commonD3DDevice->GetDeviceGUID(), m_commonD3DDevice->GetPresentParameters(),
                                 std::move(device9), m_commonD3DDevice->GetD3D9CreationFlags());
      m_commonD3DDevice->SetD3D3Device(m_device3.ptr());

      // On native this is the same object, so no need to ref
      *ppvObject = m_device3.ptr();

      return S_OK;
    }
    // Some games, such as Hype: The Time Quest, apparently query this
    // as well, albeit without actually using the returned object...
    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      if (m_commonD3DDevice->GetD3D5Device() != nullptr) {
        Logger::debug("D3D6Device::QueryInterface: Query for existing IDirect3DDevice2");
        return m_commonD3DDevice->GetD3D5Device()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D6Device::QueryInterface: Query for IDirect3DDevice2");

      Com<IDirect3DDevice2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      // TODO: Make sure the RT has an existing DDrawSurface,
      // and QueryInterface for one if that's not the case

      Com<d3d9::IDirect3DDevice9> device9 = m_d3d9.ptr();
      m_device5 = new D3D5Device(m_commonD3DDevice.ptr(), std::move(ppvProxyObject),
                                 m_commonD3DDevice->GetCommonD3DInterface()->GetD3D5Interface(),
                                 GetD3D5Caps(m_commonD3DDevice->GetDeviceGUID(), d3dOptions),
                                 m_commonD3DDevice->GetDeviceGUID(), m_commonD3DDevice->GetPresentParameters(),
                                 std::move(device9), m_rt->GetCommonSurface()->GetDDSurface(),
                                 m_commonD3DDevice->GetD3D9CreationFlags());
      m_commonD3DDevice->SetD3D5Device(m_device5.ptr());

      // On native this is the same object, so no need to ref
      *ppvObject = m_device5.ptr();

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

  HRESULT STDMETHODCALLTYPE D3D6Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    Logger::debug(">>> D3D6Device::GetCaps");

    if (unlikely(hal_desc == nullptr || hel_desc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidD3DDeviceDescSize(hal_desc->dwSize)
              || !IsValidD3DDeviceDescSize(hel_desc->dwSize)))
      return DDERR_INVALIDPARAMS;

    D3DDEVICEDESC desc_HAL = m_desc;
    D3DDEVICEDESC desc_HEL = m_desc;

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
      Logger::warn("D3D6Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, hal_desc->dwSize);
    memcpy(hel_desc, &desc_HEL, hel_desc->dwSize);

    return D3D_OK;
  }

  // Docs state: "The IDirect3DDevice3::GetStats method is obsolete,
  // and not implemented in the IDirect3DDevice3 interface."
  HRESULT STDMETHODCALLTYPE D3D6Device::GetStats(D3DSTATS *stats) {
    Logger::debug(">>> D3D6Device::GetStats");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::AddViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::AddViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);
    HRESULT hr = m_proxy->AddViewport(d3d6Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    AddViewportInternal(viewport);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DeleteViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DeleteViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);
    HRESULT hr = m_proxy->DeleteViewport(d3d6Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DeleteViewportInternal(viewport);

    // Clear the current viewport if it is deleted from the device
    if (m_currentViewport.ptr() == d3d6Viewport)
      m_currentViewport = nullptr;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::NextViewport(IDirect3DViewport3 *lpDirect3DViewport, IDirect3DViewport3 **lplpAnotherViewport, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::NextViewport");

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
        Logger::warn("D3D6Device::NextViewport: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    Logger::debug(">>> D3D6Device::EnumTextureFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // Note: The list of formats exposed in D3D6 is restricted to the below

    DDPIXELFORMAT textureFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D6
    textureFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // Not supported in D3D9, but some games need
    // it to be advertised (for offscreen plain surfaces?)
    if (unlikely(d3dOptions->supportR3G3B2)) {
      textureFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
      hr = cb(&textureFormat, ctx);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    // Not supported in D3D9, but some games may use it
    /*textureFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;*/

    textureFormat = GetTextureFormat(d3d9::D3DFMT_V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_L6V5U5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8L8V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT1);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT2);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT3);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::BeginScene() {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::BeginScene");

    RefreshLastUsedDevice();

    if (unlikely(m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_d3d9->BeginScene();

    if (likely(SUCCEEDED(hr)))
      m_commonD3DDevice->SetInScene(true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::EndScene() {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::EndScene");

    RefreshLastUsedDevice();

    if (unlikely(!m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_d3d9->EndScene();

    if (likely(SUCCEEDED(hr)))
      m_commonD3DDevice->SetInScene(false);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetDirect3D(IDirect3D3 **d3d) {
    Logger::debug(">>> D3D6Device::GetDirect3D");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetCurrentViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::SetCurrentViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6Viewport> d3d6Viewport = static_cast<D3D6Viewport*>(viewport);
    HRESULT hr = m_proxy->SetCurrentViewport(d3d6Viewport->GetProxied());
    if (unlikely(FAILED(hr))) {
      Logger::debug("D3D6Device::SetCurrentViewport: Failed to set proxied viewport");
      return hr;
    }

    if (unlikely(m_currentViewport == d3d6Viewport))
      return D3D_OK;

    if (likely(m_currentViewport != nullptr)) {
      m_currentViewport->DeactivateLights();
      m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(false);
    }

    m_currentViewport = d3d6Viewport.ptr();

    m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(true);
    m_currentViewport->ApplyViewport();
    m_currentViewport->ApplyAndActivateLights();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetCurrentViewport(IDirect3DViewport3 **viewport) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::GetCurrentViewport");

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

  HRESULT STDMETHODCALLTYPE D3D6Device::SetRenderTarget(IDirectDrawSurface4 *surface, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::SetRenderTarget");

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D6Device::SetRenderTarget: NULL render target");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D6Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_UNSUPPORTED;
    }

    DDraw4Surface* rt6 = static_cast<DDraw4Surface*>(surface);

    // Needed to ensure proxied Z/Stencil viewport clears will work
    HRESULT hrRT = m_proxy->SetRenderTarget(rt6->GetShadowOrProxied(), flags);
    if (unlikely(FAILED(hrRT)))
      Logger::debug("D3D6Device::SetRenderTarget: Failed to set RT");

    HRESULT hr = rt6->GetCommonSurface()->ValidateRTUsage();
    if (unlikely(FAILED(hr)))
      return hr;

    hr = rt6->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    hr = m_d3d9->SetRenderTarget(0, rt6->GetD3D9());

    if (likely(SUCCEEDED(hr))) {
      Logger::debug("D3D6Device::SetRenderTarget: Set a new D3D9 RT");

      m_rt = rt6;
      m_ds = m_rt->GetAttachedDepthStencil();

      HRESULT hrDS;

      if (m_ds != nullptr) {
        Logger::debug("D3D6Device::SetRenderTarget: Found an attached DS");

        hrDS = m_ds->InitializeD3D9DepthStencil();
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D6Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
          return hrDS;
        }

        hrDS = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D6Device::SetRenderTarget: Failed to set D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D6Device::SetRenderTarget: Set a new D3D9 DS");
      } else {
        Logger::debug("D3D6Device::SetRenderTarget: RT has no depth stencil attached");

        hrDS = m_d3d9->SetDepthStencilSurface(nullptr);
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D6Device::SetRenderTarget: Failed to clear the D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D6Device::SetRenderTarget: Cleared the D3D9 DS");
      }
    } else {
      Logger::err("D3D6Device::SetRenderTarget: Failed to set D3D9 RT");
      return hr;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetRenderTarget(IDirectDrawSurface4 **surface) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::GetRenderTarget");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::Begin");

    // All FVF combinations are technically supported,
    // but I doubt that is the case in practice
    if (dwVertexTypeDesc != D3DFVF_VERTEX &&
        dwVertexTypeDesc != D3DFVF_LVERTEX &&
        dwVertexTypeDesc != D3DFVF_TLVERTEX) {
      Logger::warn("D3D6Device::Begin: Unsupported FVF format");
      return DDERR_INVALIDPARAMS;
    }

    m_vertexStreamInfo.d3dpt = d3dptPrimitiveType;
    m_vertexStreamInfo.d3dvt = ConvertFVFType(dwVertexTypeDesc);
    m_vertexStreamInfo.dwFlags = dwFlags;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::BeginIndexed(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, DWORD flags) {
    Logger::warn("!!! D3D6Device::BeginIndexed: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Vertex(void *vertex) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::Vertex");

    if (unlikely(vertex == nullptr))
      return DDERR_INVALIDPARAMS;

    if (m_vertexStreamInfo.d3dvt == D3DVT_VERTEX) {
      m_vertexStream.push_back(*reinterpret_cast<D3DVERTEX*>(vertex));
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_LVERTEX) {
      m_lvertexStream.push_back(*reinterpret_cast<D3DLVERTEX*>(vertex));
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_TLVERTEX) {
      m_tlvertexStream.push_back(*reinterpret_cast<D3DTLVERTEX*>(vertex));
    } else {
      Logger::warn(">>> D3D6Device::Vertex: Invalid vertex type");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Index(WORD wVertexIndex) {
    Logger::warn("!!! D3D6Device::Index: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::End(DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::End");

    HRESULT hr;
    if (m_vertexStreamInfo.d3dvt == D3DVT_VERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, ConvertVertexType(m_vertexStreamInfo.d3dvt), m_vertexStream.data(),
                         m_vertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_vertexStream.clear();
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_LVERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, ConvertVertexType(m_vertexStreamInfo.d3dvt), m_lvertexStream.data(),
                         m_lvertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_lvertexStream.clear();
    } else if (m_vertexStreamInfo.d3dvt == D3DVT_TLVERTEX) {
      hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, ConvertVertexType(m_vertexStreamInfo.d3dvt), m_tlvertexStream.data(),
                         m_tlvertexStream.size(), m_vertexStreamInfo.dwFlags);
      m_tlvertexStream.clear();
    } else {
      Logger::warn(">>> D3D6Device::End: Invalid vertex type");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(FAILED(hr)))
      Logger::warn(">>> D3D6Device::End: Failed call to DrawPrimitive");

    m_vertexStreamInfo = { };

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D6Device::GetRenderState: ", dwRenderStateType));

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    // As opposed to D3D7, D3D6 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D6RenderStateType(dwRenderStateType))) {
      Logger::debug(str::format("D3D6Device::GetRenderState: Invalid render state ", dwRenderStateType));
      *lpdwRenderState = 0;
      return D3D_OK;
    }

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // "Texture handle for use when rendering with the IDirect3DDevice2 or earlier interfaces."
      case D3DRENDERSTATE_TEXTUREHANDLE:
        *lpdwRenderState = 0;
        return D3D_OK;

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_commonD3DDevice->GetAntialias();
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESS:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, default TRUE in D3D6
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        m_d3d9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        *lpdwRenderState = value9 & D3DWRAP_U;
        return D3D_OK;
      }

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        m_d3d9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
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
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMIN: {
        DWORD minFilter = 0;
        DWORD mipFilter = 0;
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MINFILTER, &minFilter);
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, &mipFilter);
        *lpdwRenderState = DecodeTextureMinValues(minFilter, mipFilter);
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        *lpdwRenderState = m_commonD3DDevice->GetTextureMapBlend();
        return D3D_OK;

      // Not supported by D3D6
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
        DWORD bias = 0;
        m_d3d9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return D3D_OK;
      }

      case D3DRENDERSTATE_ANISOTROPY:
        m_d3d9->GetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, lpdwRenderState);
        return D3D_OK;

      // "Batched primitives are implicitly flushed when rendering with the
      // IDirect3DDevice3 interface, as well as when rendering with execute buffers."
      case D3DRENDERSTATE_FLUSHBATCH:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
        *lpdwRenderState = FALSE;
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
    return m_d3d9->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D6Device::SetRenderState: ", dwRenderStateType));

    // As opposed to D3D7, D3D6 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D6RenderStateType(dwRenderStateType))) {
      Logger::debug(str::format("D3D6Device::SetRenderState: Invalid render state ", dwRenderStateType));
      return D3D_OK;
    }

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // "Texture handle for use when rendering with the IDirect3DDevice2 or earlier interfaces."
      case D3DRENDERSTATE_TEXTUREHANDLE:
        return D3D_OK;

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                    || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT))
            Logger::warn("D3D6Device::SetRenderState: Device does not expose FSAA emulation");
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
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        m_d3d9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, default TRUE in D3D6
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        return D3D_OK;

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        m_d3d9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          m_d3d9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & D3DWRAP_U);
        } else {
          m_d3d9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_U);
        }
        return D3D_OK;
      }

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        m_d3d9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          m_d3d9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & D3DWRAP_V);
        } else {
          m_d3d9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_V);
        }
        return D3D_OK;
      }

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_commonD3DDevice->SetLinePattern(bit::cast<D3DLINEPATTERN>(dwRenderState));
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

      // Docs state: "This render state is not supported by the software
      // rasterizers, and is often ignored by hardware drivers."
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
        m_commonD3DDevice->SetTextureMapBlend(dwRenderState);

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

      // Not supported by D3D6
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
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEENABLE:
        static bool s_stippleEnableErrorShown;

        if (dwRenderState && !std::exchange(s_stippleEnableErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE: {
        m_commonD3DDevice->SetColorKeyEnable(dwRenderState);

        const bool validColorKey = m_textures[0] != nullptr ? m_textures[0]->GetParent()->GetCommonSurface()->HasValidColorKey() : false;
        m_bridge->SetColorKeyState(dwRenderState && validColorKey);
        if (dwRenderState && validColorKey) {
          DDCOLORKEY normalizedColorKey = m_textures[0]->GetParent()->GetCommonSurface()->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }

        return D3D_OK;
      }

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

      // "Batched primitives are implicitly flushed when rendering with the
      // IDirect3DDevice3 interface, as well as when rendering with execute buffers."
      case D3DRENDERSTATE_FLUSHBATCH:
        return D3D_OK;

      // Unclear if this ever did much as far as the runtime was concered.
      // Most likely it was passed to the driver, though I don't expect
      // it was ever implemented, as it's a D3D6-only render state.
      case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
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
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEPATTERN");

        return D3D_OK;
    }

    // This call will never fail
    return m_d3d9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::GetLightState");

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL:
        *lpdwLightState = m_commonD3DDevice->GetCurrentMaterialHandle();
        break;
      case D3DLIGHTSTATE_AMBIENT:
        m_d3d9->GetRenderState(d3d9::D3DRS_AMBIENT, lpdwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
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
      case D3DLIGHTSTATE_COLORVERTEX:
        m_d3d9->GetRenderState(d3d9::D3DRS_COLORVERTEX, lpdwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::SetLightState");

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
        Logger::debug(str::format("D3D6Device::SetLightState: Applying material nr. ", dwLightState, " to D3D9"));
        m_d3d9->SetMaterial(material9);

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
      case D3DLIGHTSTATE_COLORVERTEX:
        m_d3d9->SetRenderState(d3d9::D3DRS_COLORVERTEX, dwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D6Device::SetTransform");
    return m_d3d9->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D6Device::GetTransform");
    return m_d3d9->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D6Device::MultiplyTransform");
    return m_d3d9->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD vertex_type, void *vertices, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr))
      return DDERR_INVALIDPARAMS;

    m_rt->InitializeOrUploadD3D9();

    HandlePreDrawFlags(flags, vertex_type);
    HandlePreDrawLegacyProjection(flags);

    m_d3d9->SetFVF(vertex_type);
    HRESULT hr = m_d3d9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(primitive_type),
                     GetPrimitiveCount(primitive_type, vertex_count),
                     vertices,
                     GetFVFSize(vertex_type));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, vertex_type);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawIndexedPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    m_rt->InitializeOrUploadD3D9();

    HandlePreDrawFlags(flags, fvf);
    HandlePreDrawLegacyProjection(flags);

    m_d3d9->SetFVF(fvf);
    HRESULT hr = m_d3d9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      vertices,
                      GetFVFSize(fvf));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, fvf);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    Logger::debug(">>> D3D6Device::SetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
    Logger::debug(">>> D3D6Device::GetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DVIEWPORT9 viewport9;
    if (SUCCEEDED(m_d3d9->GetViewport(&viewport9))) {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawPrimitiveStrided");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(strided_data == nullptr))
      return DDERR_INVALIDPARAMS;

    m_rt->InitializeOrUploadD3D9();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(fvf, strided_data, vertex_count);

    HandlePreDrawFlags(flags, fvf);
    HandlePreDrawLegacyProjection(flags);

    m_d3d9->SetFVF(fvf);
    HRESULT hr = m_d3d9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(primitive_type),
                     GetPrimitiveCount(primitive_type, vertex_count),
                     pvb.vertexData.data(),
                     pvb.stride);

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, fvf);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitiveStrided: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawIndexedPrimitiveStrided");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(strided_data == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    m_rt->InitializeOrUploadD3D9();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(fvf, strided_data, vertex_count);

    HandlePreDrawFlags(flags, fvf);
    HandlePreDrawLegacyProjection(flags);

    m_d3d9->SetFVF(fvf);
    HRESULT hr = m_d3d9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      pvb.vertexData.data(),
                      pvb.stride);

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, fvf);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveStrided: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, DWORD start_vertex, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawPrimitiveVB");

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vb == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6VertexBuffer> vb6 = static_cast<D3D6VertexBuffer*>(vb);

    if (unlikely(vb6->IsLocked())) {
      Logger::err("D3D6Device::DrawPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    m_rt->InitializeOrUploadD3D9();

    HandlePreDrawFlags(flags, vb6->GetFVF());
    HandlePreDrawLegacyProjection(flags);

    m_d3d9->SetFVF(vb6->GetFVF());
    m_d3d9->SetStreamSource(0, vb6->GetD3D9(), 0, vb6->GetStride());
    HRESULT hr = m_d3d9->DrawPrimitive(
                           d3d9::D3DPRIMITIVETYPE(primitive_type),
                           start_vertex,
                           GetPrimitiveCount(primitive_type, vertex_count));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, vb6->GetFVF());

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitiveVB: Failed D3D9 call to DrawPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::DrawIndexedPrimitiveVB");

    RefreshLastUsedDevice();

    if (unlikely(!index_count))
      return D3D_OK;

    if (unlikely(vb == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6VertexBuffer> vb6 = static_cast<D3D6VertexBuffer*>(vb);

    if (unlikely(vb6->IsLocked())) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    uint8_t ibIndex = 0;
    // Try to fit index buffer uploads into the smallest buffer size possible,
    // out of the five available: XS, S, M, L and XL (XL being the theoretical max)
    while (index_count > ddrawCaps::IndexCount[ibIndex]) {
      ibIndex++;
      if (unlikely(ibIndex > ddrawCaps::IndexBufferCount - 1)) {
        Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Exceeded size of largest index buffer");
        return DDERR_UNSUPPORTED;
      }
    }

    m_rt->InitializeOrUploadD3D9();

    HandlePreDrawFlags(flags, vb6->GetFVF());
    HandlePreDrawLegacyProjection(flags);

    d3d9::IDirect3DIndexBuffer9* ib9 = m_ib9[ibIndex].ptr();

    UploadIndices(ib9, indices, index_count);
    m_ib9_uploads[ibIndex]++;
    m_d3d9->SetIndices(ib9);
    m_d3d9->SetFVF(vb6->GetFVF());
    m_d3d9->SetStreamSource(0, vb6->GetD3D9(), 0, vb6->GetStride());
    HRESULT hr = m_d3d9->DrawIndexedPrimitive(
                    d3d9::D3DPRIMITIVETYPE(primitive_type),
                    0,
                    0,
                    vb6->GetNumVertices(),
                    0,
                    GetPrimitiveCount(primitive_type, index_count));

    HandlePostDrawLegacyProjection();
    HandlePostDrawFlags(flags, vb6->GetFVF());

    if(unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Failed D3D9 call to DrawIndexedPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::ComputeSphereVisibility(D3DVECTOR *lpCenters, D3DVALUE *lpRadii, DWORD dwNumSpheres, DWORD dwFlags, DWORD *lpdwReturnValues) {
    Logger::debug(">>> D3D6Device::ComputeSphereVisibility");

    if (unlikely(lpCenters == nullptr || lpRadii == nullptr || lpdwReturnValues == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(dwNumSpheres == 0))
      return D3D_OK;

    // Docs state: "The array need not be initialized, but it must be large enough to contain a DWORD for
    // each sphere being tested. When the method returns, each element in the array contains a combination
    // of flags that describe the visibility of that sphere within the current viewport for this device.
    // If a sphere is completely visible, the corresponding entry in lpdwReturnValues is 0."
    // Consider everything to be visible as a minimal implementation, which makes Space Empires V happy.
    for (DWORD i = 0; i < dwNumSpheres; i++)
      lpdwReturnValues[i] = 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTexture(DWORD stage, IDirect3DTexture2 **texture) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::GetTexture");

    if (unlikely(texture == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stage >= ddrawCaps::TextureStageCount)) {
      Logger::err(str::format("D3D6Device::GetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    *texture = m_textures[stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTexture(DWORD stage, IDirect3DTexture2 *texture) {
    D3DDeviceLock lock = LockDevice();

    Logger::debug(">>> D3D6Device::SetTexture");

    if (unlikely(stage >= ddrawCaps::TextureStageCount)) {
      Logger::err(str::format("D3D6Device::SetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    HRESULT hr;

    // Unbinding texture stages
    if (texture == nullptr) {
      Logger::debug("D3D6Device::SetTexture: Unbiding D3D9 texture");

      hr = m_d3d9->SetTexture(stage, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_textures[stage] != nullptr) {
          Logger::debug("D3D6Device::SetTexture: Unbinding local texture");
          m_textures[stage] = nullptr;

          if (likely(stage == 0))
            m_bridge->SetColorKeyState(false);
        }
      } else {
        Logger::err("D3D6Device::SetTexture: Failed to unbind D3D9 texture");
      }

      return hr;
    }

    D3D6Texture* texture6 = static_cast<D3D6Texture*>(texture);
    DDraw4Surface* surface6 = texture6->GetParent();

    Logger::debug("D3D6Device::SetTexture: Binding D3D9 texture");

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(surface6->GetD3D9Device() != m_d3d9.ptr()))
      surface6->GetCommonSurface()->DirtyDDrawSurface();

    hr = surface6->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetTexture: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_textures[stage] == texture6))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = surface6->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = m_d3d9->SetTexture(stage, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D6Device::SetTexture: Failed to bind D3D9 texture");
        return hr;
      }

      if (likely(stage == 0)) {
        // "Any alpha values in the texture replace the alpha values in the colors that would
        //  have been used with no texturing; if the texture does not contain an alpha component,
        //  alpha values at the vertices in the source are interpolated between vertices."
        if (m_commonD3DDevice->GetTextureMapBlend() == D3DTBLEND_MODULATE && !m_alphaOpSet) {
          const DWORD textureOp = surface6->GetCommonSurface()->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE;
          m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
        }

        const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
        const bool validColorKey = surface6->GetCommonSurface()->HasValidColorKey();
        m_bridge->SetColorKeyState(colorKeyEnable && validColorKey);
        if (colorKeyEnable && validColorKey) {
          DDCOLORKEY normalizedColorKey = surface6->GetCommonSurface()->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }
      }
    }

    m_textures[stage] = texture6;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState) {
    Logger::debug(">>> D3D6Device::GetTextureStageState");

    if (unlikely(lpdwState == nullptr))
      return DDERR_INVALIDPARAMS;

    // In the case of D3DTSS_ADDRESS, which is exclusive to D3D7
    // and D3D6, simply return based on D3DTSS_ADDRESSU
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      return m_d3d9->GetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, lpdwState);
    }

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    // If the type has been remapped to a sampler state type
    if (stateType != -1u) {
      // MAG/MIN/MIP filter enums are each different than the unified D3D9 D3DTEXTUREFILTERTYPE
      if (stateType == d3d9::D3DSAMP_MAGFILTER ||
          stateType == d3d9::D3DSAMP_MINFILTER || stateType == d3d9::D3DSAMP_MIPFILTER) {
        DWORD dwStateProxy9 = 0;
        HRESULT hr = m_d3d9->GetSamplerState(dwStage, stateType, &dwStateProxy9);
        *lpdwState = DecodeD3D9TexFilterValues(d3dTexStageStateType, dwStateProxy9);
        return hr;
      } else {
        return m_d3d9->GetSamplerState(dwStage, stateType, lpdwState);
      }
    } else {
      return m_d3d9->GetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), lpdwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState) {
    Logger::debug(">>> D3D6Device::SetTextureStageState");

    // In the case of D3DTSS_ADDRESS, which is exclusive to D3D7
    // and D3D6, we need to set up both D3DTSS_ADDRESSU and D3DTSS_ADDRESSV
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      m_d3d9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, dwState);
      return m_d3d9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSV, dwState);
    }

    if (!m_alphaOpSet && d3dTexStageStateType == D3DTSS_ALPHAOP)
      m_alphaOpSet = true;

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    // If the type has been remapped to a sampler state type
    if (stateType != -1u) {
      // MAG/MIN/MIP filter enums are each different than the unified D3D9 D3DTEXTUREFILTERTYPE
      if (stateType == d3d9::D3DSAMP_MAGFILTER ||
          stateType == d3d9::D3DSAMP_MINFILTER || stateType == d3d9::D3DSAMP_MIPFILTER) {
        const DWORD dwState9 = DecodeD3D7TexFilterValues(d3dTexStageStateType, dwState);
        return m_d3d9->SetSamplerState(dwStage, stateType, dwState9);
      } else {
        return m_d3d9->SetSamplerState(dwStage, stateType, dwState);
      }
    } else {
      return m_d3d9->SetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), dwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::ValidateDevice(LPDWORD lpdwPasses) {
    Logger::debug(">>> D3D6Device::ValidateDevice");

    HRESULT hr = m_d3d9->ValidateDevice(lpdwPasses);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  void D3D6Device::InitializeDS() {
    m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      Logger::debug("D3D6Device::InitializeDS: Found an attached DS");

      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D6Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        Logger::info("D3D6Device::InitializeDS: Got depth stencil from RT");

        DDSURFACEDESC2 descDS;
        descDS.dwSize = sizeof(DDSURFACEDESC2);
        m_ds->GetProxied()->GetSurfaceDesc(&descDS);
        Logger::debug(str::format("D3D6Device::InitializeDS: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));

        HRESULT hrDS9 = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if(unlikely(FAILED(hrDS9))) {
          Logger::err("D3D6Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      Logger::info("D3D6Device::InitializeDS: RT has no depth stencil attached");
      m_d3d9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  void D3D6Device::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
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

  HRESULT D3D6Device::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    Logger::info("D3D6Device::ResetD3D9Swapchain: Resetting the D3D9 swapchain");

    HRESULT hr = m_bridge->ResetSwapChain(params);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::ResetD3D9Swapchain: Failed to reset the D3D9 swapchain");
    } else {
      // TODO: Cache and reset all surfaces tied to the D3D9 backbuffers
      m_rt->SetD3D9(nullptr);
      // Note that the D3D9 depth stencil survives a swapchain reset,
      // so there's no need to worry about it in this case
    }

    return hr;
  }

  inline HRESULT D3D6Device::InitializeIndexBuffers() {
    static constexpr DWORD Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

    for (uint8_t ibIndex = 0; ibIndex < ddrawCaps::IndexBufferCount ; ibIndex++) {
      const UINT ibSize = ddrawCaps::IndexCount[ibIndex] * sizeof(WORD);

      Logger::debug(str::format("D3D6Device::InitializeIndexBuffer: Creating D3D9 index buffer, size: ", ibSize));

      HRESULT hr = m_d3d9->CreateIndexBuffer(ibSize, Usage, d3d9::D3DFMT_INDEX16,
                                             d3d9::D3DPOOL_DEFAULT, &m_ib9[ibIndex], nullptr);
      if (unlikely(FAILED(hr)))
        return hr;
    }

    return D3D_OK;
  }

  inline void D3D6Device::UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount) {
    Logger::debug(str::format("D3D6Device::UploadIndices: Uploading ", indexCount, " indices"));

    const size_t size = indexCount * sizeof(WORD);
    void* pData = nullptr;

    // Locking and unlocking are generally expected to work here
    ib9->Lock(0, size, &pData, D3DLOCK_DISCARD);
    memcpy(pData, static_cast<void*>(indices), size);
    ib9->Unlock();
  }

  inline void D3D6Device::AddViewportInternal(IDirect3DViewport3* viewport) {
    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d6Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D6Device::AddViewportInternal: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d6Viewport);
      d3d6Viewport->GetCommonViewport()->SetD3D6Device(this);
    }
  }

  inline void D3D6Device::DeleteViewportInternal(IDirect3DViewport3* viewport) {
    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d6Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
       d3d6Viewport->GetCommonViewport()->SetD3D6Device(nullptr);
    } else {
      Logger::warn("D3D6Device::DeleteViewportInternal: Viewport not found");
    }
  }

}
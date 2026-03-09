#include "d3d3_device.h"

#include "d3d3_execute_buffer.h"

#include "../ddraw/ddraw_surface.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D3Device::s_deviceCount = 0;

  D3D3Device::D3D3Device(
      Com<IDirect3DDevice>&& d3d3DeviceProxy,
      DDrawSurface* pParent,
      D3DDEVICEDESC Desc,
      GUID deviceGUID,
      d3d9::D3DPRESENT_PARAMETERS Params9,
      Com<d3d9::IDirect3DDevice9>&& pDevice9,
      DWORD CreationFlags9)
    : DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9>(pParent, std::move(d3d3DeviceProxy), std::move(pDevice9))
    , m_commonIntf ( pParent->GetCommonInterface() )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_params9 ( Params9 )
    , m_desc ( Desc )
    , m_deviceGUID ( deviceGUID )
    , m_rt ( pParent ) {
    // Get the bridge interface to D3D9
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D3Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (unlikely(m_parent->GetOptions()->emulateFSAA == FSAAEmulation::Forced)) {
      Logger::warn("D3D3Device: Force enabling AA");
      m_d3d9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    }

    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D3Device: Created a new device nr. ((1-", m_deviceCount, "))"));
  }

  D3D3Device::~D3D3Device() {
    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->SetDevice(nullptr);
    }

    // Clear the common interface device pointer if it points to this device
    if (m_commonIntf->GetD3D3Device() == this)
      m_commonIntf->SetD3D3Device(nullptr);

    Logger::debug(str::format("D3D3Device: Device nr. ((1-", m_deviceCount, ")) bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DDevice))
      return this;

    throw DxvkError("D3D3Device::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      if (likely(m_parent != nullptr)) {
        Logger::debug("D3D3Device::QueryInterface: Query for IDirect3DDevice2");
        m_parent->QueryInterface(riid, ppvObject);
      }

      Logger::warn("D3D3Device::QueryInterface: Query for IDirect3DDevice2");
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

  HRESULT STDMETHODCALLTYPE D3D3Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    Logger::debug(">>> D3D3Device::GetCaps");

    if (unlikely(hal_desc == nullptr || hel_desc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidD3DDeviceDescSize(hal_desc->dwSize)
              || !IsValidD3DDeviceDescSize(hel_desc->dwSize)))
      return DDERR_INVALIDPARAMS;

    D3DDEVICEDESC desc_HAL = m_desc;
    D3DDEVICEDESC desc_HEL = m_desc;

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
    } else {
      Logger::warn("D3D3Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, sizeof(D3DDEVICEDESC));
    memcpy(hel_desc, &desc_HEL, sizeof(D3DDEVICEDESC));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2) {
    Logger::warn("!!! D3D3Device::SwapTextureHandles: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetStats(D3DSTATS *stats) {
    Logger::debug("<<< D3D3Device::GetStats: Proxy");
    return m_proxy->GetStats(stats);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::AddViewport(IDirect3DViewport *viewport) {
    D3D3DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D3Device::AddViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    HRESULT hr = m_proxy->AddViewport(d3d3Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    AddViewportInternal(viewport);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteViewport(IDirect3DViewport *viewport) {
    D3D3DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D3Device::DeleteViewport");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    HRESULT hr = m_proxy->DeleteViewport(d3d3Viewport->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DeleteViewportInternal(viewport);

    // Clear the current viewport if it is deleted from the device
    if (m_currentViewport.ptr() == d3d3Viewport)
      m_currentViewport = nullptr;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::NextViewport(IDirect3DViewport *ref, IDirect3DViewport **viewport, DWORD flags) {
    Logger::warn("!!! D3D3Device::NextViewport: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx) {
    Logger::debug(">>> D3D3Device::EnumTextureFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    DDSURFACEDESC textureFormat = { };
    textureFormat.dwSize  = sizeof(DDSURFACEDESC);
    textureFormat.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
    textureFormat.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Note: The list of formats exposed in D3D3 is restricted to the below

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat.ddpfPixelFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D3
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

  HRESULT STDMETHODCALLTYPE D3D3Device::BeginScene() {
    D3D3DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D3Device::BeginScene");

    RefreshLastUsedDevice();

    if (unlikely(m_inScene))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_d3d9->BeginScene();

    if (likely(SUCCEEDED(hr)))
      m_inScene = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EndScene() {
    D3D3DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D3Device::EndScene");

    RefreshLastUsedDevice();

    if (unlikely(!m_inScene))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_d3d9->EndScene();

    if (likely(SUCCEEDED(hr))) {
      if (m_parent->GetOptions()->forceProxiedPresent) {
        // If we have drawn anything, we need to make sure we blit back
        // the results onto the D3D3 render target before we flip it
        if (m_commonIntf->HasDrawn())
          BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_rt->GetProxied(), m_rt->GetD3D9());

        m_rt->GetProxied()->Flip(static_cast<IDirectDrawSurface*>(m_commonIntf->GetFlipRTSurface()),
                                 m_commonIntf->GetFlipRTFlags());

        if (likely(m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Strict))
          m_commonIntf->ResetDrawTracking();
      }

      m_inScene = false;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetDirect3D(IDirect3D **d3d) {
    Logger::debug(">>> D3D3Device::GetDirect3D");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    *d3d = m_parent->GetCommonInterface()->GetD3D3Interface();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc) {
    Logger::debug("<<< D3D3Device::Initialize: Proxy");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Interface* d3d3Intf = static_cast<D3D3Interface*>(d3d);
    return m_proxy->Initialize(d3d3Intf->GetProxied(), lpGUID, desc);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter) {
    Logger::warn(">>> D3D3Device::CreateExecuteBuffer");

    Com<IDirect3DExecuteBuffer> bufferProxy;
    HRESULT hr = m_proxy->CreateExecuteBuffer(desc, &bufferProxy, pkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(buffer);

    *buffer = ref(new D3D3ExecuteBuffer(std::move(bufferProxy), this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags) {
    Logger::debug("<<< D3D3Device::Execute: Proxy");

    if (unlikely(buffer == nullptr || viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3ExecuteBuffer* d3d3ExecuteBuffer = static_cast<D3D3ExecuteBuffer*>(buffer);
    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    if (m_currentViewport != d3d3Viewport)
      m_currentViewport = d3d3Viewport;

    m_commonIntf->UpdateDrawTracking();

    return m_proxy->Execute(d3d3ExecuteBuffer->GetProxied(), d3d3Viewport->GetProxied(), flags);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect) {
    Logger::debug("<<< D3D3Device::Pick: Proxy");

    if (unlikely(buffer == nullptr || viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3ExecuteBuffer* d3d3ExecuteBuffer = static_cast<D3D3ExecuteBuffer*>(buffer);
    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    if (m_currentViewport != d3d3Viewport)
      m_currentViewport = d3d3Viewport;

    m_commonIntf->UpdateDrawTracking();

    return m_proxy->Pick(d3d3ExecuteBuffer->GetProxied(), d3d3Viewport->GetProxied(), flags, rect);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetPickRecords(DWORD *count, D3DPICKRECORD *records) {
    Logger::debug("<<< D3D3Device::GetPickRecords: Proxy");
    return m_proxy->GetPickRecords(count, records);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateMatrix(D3DMATRIXHANDLE *matrix) {
    Logger::debug("<<< D3D3Device::CreateMatrix: Proxy");
    return m_proxy->CreateMatrix(matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug("<<< D3D3Device::SetMatrix: Proxy");
    return m_proxy->SetMatrix(handle, matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug("<<< D3D3Device::GetMatrix: Proxy");
    return m_proxy->GetMatrix(handle, matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle) {
    Logger::debug("<<< D3D3Device::GetMatrix: Proxy");
    return m_proxy->DeleteMatrix(D3DMatHandle);
  }

  void D3D3Device::InitializeDS() {
    if (!m_rt->IsInitialized())
      m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      Logger::debug("D3D3Device::InitializeDS: Found an attached DS");

      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D3Device::InitializeDS: Failed to initialize D3D9 DS");
      } else if (m_commonIntf->GetD3D5Device() == nullptr) {
        Logger::info("D3D3Device::InitializeDS: Got depth stencil from RT");

        DDSURFACEDESC descDS;
        descDS.dwSize = sizeof(DDSURFACEDESC);
        m_ds->GetProxied()->GetSurfaceDesc(&descDS);
        Logger::debug(str::format("D3D3Device::InitializeDS: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));

        HRESULT hrDS9 = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if(unlikely(FAILED(hrDS9))) {
          Logger::err("D3D3Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else if (m_commonIntf->GetD3D5Device() == nullptr) {
      Logger::info("D3D3Device::InitializeDS: RT has no depth stencil attached");
      m_d3d9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  inline void D3D3Device::AddViewportInternal(IDirect3DViewport* viewport) {
    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d3Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D3Device::AddViewportInternal: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d3Viewport);
      d3d3Viewport->SetDevice(this);
    }
  }

  inline void D3D3Device::DeleteViewportInternal(IDirect3DViewport* viewport) {
    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d3Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
      d3d3Viewport->SetDevice(nullptr);
    } else {
      Logger::warn("D3D3Device::DeleteViewportInternal: Viewport not found");
    }
  }

}
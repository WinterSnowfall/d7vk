#include "d3d5_interface.h"

#include "d3d5_device.h"
#include "d3d5_light.h"
#include "d3d5_material.h"
#include "d3d5_viewport.h"

#include "ddraw_common_interface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_interface.h"

namespace dxvk {

  uint32_t D3D5Interface::s_intfCount = 0;

  D3D5Interface::D3D5Interface(Com<IDirect3D2>&& d3d5IntfProxy, DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirect3D2, d3d9::IDirect3D9>(pParent, std::move(d3d5IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION))) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D5Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->EnableD3D5CompatibilityMode();

    m_options = D3DOptions(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D5Interface: Created a new interface nr. ((2-", m_intfCount, "))"));
  }

  D3D5Interface::~D3D5Interface() {
    Logger::debug(str::format("D3D5Interface: Interface nr. ((2-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirect3D2, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D5Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy d3d interfaces
    if (unlikely(riid == __uuidof(IDirect3D))) {
      Logger::warn("D3D5Interface::QueryInterface: Query for legacy IDirect3D");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::debug("D3D5Interface::QueryInterface: Query for IDirectDraw");
      return m_parent->QueryInterface(riid, ppvObject);
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

  HRESULT STDMETHODCALLTYPE D3D5Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug("<<< D3D5Interface::EnumDevices: Proxy");
    return m_proxy->EnumDevices(lpEnumDevicesCallback, lpUserArg);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D5Interface::CreateLight");

    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    // We do not really need a proxy light object, it's a simple container
    *lplpDirect3DLight = ref(new D3D5Light(nullptr, this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateMaterial(LPDIRECT3DMATERIAL2 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D5Interface::CreateMaterial");

    if (unlikely(lplpDirect3DMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DMaterial);

    m_materialHandle++;
    auto materialIterPair = m_materials.emplace(std::piecewise_construct,
                                                std::forward_as_tuple(m_materialHandle),
                                                std::forward_as_tuple(nullptr, this, m_materialHandle));

    *lplpDirect3DMaterial = ref(&materialIterPair.first->second);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateViewport(LPDIRECT3DVIEWPORT2 *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D5Interface::CreateViewport");

    Com<IDirect3DViewport2> lplpD3DViewportProxy;
    HRESULT hr = m_proxy->CreateViewport(&lplpD3DViewportProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D5Viewport(std::move(lplpD3DViewportProxy), this));

    return D3D_OK;
  }

  // Minimal implementation which should suffice in most cases
  HRESULT STDMETHODCALLTYPE D3D5Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug(">>> D3D5Interface::FindDevice");

    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    // Software emulation, this is expected to be exposed (SWVP)
    D3DDEVICEDESC2 descRGB_HAL = GetD3D5Caps(IID_IDirect3DRGBDevice, m_options.disableAASupport);
    D3DDEVICEDESC2 descRGB_HEL = descRGB_HAL;
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE;
    descRGB_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    descRGB_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;

    // Hardware acceleration (SWVP)
    D3DDEVICEDESC2 descHAL_HAL = GetD3D5Caps(IID_IDirect3DHALDevice, m_options.disableAASupport);
    D3DDEVICEDESC2 descHAL_HEL = descHAL_HAL;
    descHAL_HEL.dcmColorModel = 0;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;

    D3DFINDDEVICERESULT2 lpD3DFRD2 = { };
    lpD3DFRD2.dwSize = sizeof(D3DFINDDEVICERESULT2);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      Logger::debug("D3D5Interface::FindDevice: Matching by device GUID");

      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice ||
          lpD3DFDS->guid == IID_IDirect3DMMXDevice ||
          lpD3DFDS->guid == IID_IDirect3DRampDevice) {
        Logger::debug("D3D5Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFRD2.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD2.ddHwDesc = descRGB_HAL;
        lpD3DFRD2.ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        Logger::debug("D3D5Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFRD2.guid = IID_IDirect3DHALDevice;
        lpD3DFRD2.ddHwDesc = descHAL_HAL;
        lpD3DFRD2.ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D5Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      Logger::debug("D3D5Interface::FindDevice: Matching by hardware flag");

      if (likely(lpD3DFDS->bHardware == TRUE)) {
        Logger::debug("D3D5Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFRD2.guid = IID_IDirect3DHALDevice;
        lpD3DFRD2.ddHwDesc = descHAL_HAL;
        lpD3DFRD2.ddSwDesc = descHAL_HEL;
      } else {
        Logger::debug("D3D5Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFRD2.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD2.ddHwDesc = descRGB_HAL;
        lpD3DFRD2.ddSwDesc = descRGB_HEL;
      }

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else {
      Logger::err("D3D5Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE lpDDS, LPDIRECT3DDEVICE2 *lplpD3DDevice) {
    Logger::debug(">>> D3D5Interface::CreateDevice");

    if (unlikely(lplpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpD3DDevice);

    if (unlikely(lpDDS == nullptr)) {
      Logger::err("D3D5Interface::CreateDevice: Null surface provided");
      return DDERR_INVALIDPARAMS;
    }

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  rgbFallback          = false;
    bool  halFallback          = false;

    if (likely(m_options.deviceTypeOverride == D3DDeviceTypeOverride::None)) {
      if (rclsid == IID_IDirect3DHALDevice) {
        Logger::info("D3D5Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
      } else if (rclsid == IID_IDirect3DRGBDevice) {
        Logger::info("D3D5Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
      } else if (rclsid == IID_IDirect3DMMXDevice) {
        Logger::warn("D3D5Interface::CreateDevice: Unsupported MMX device, falling back to RGB");
        rgbFallback = true;
      } else if (rclsid == IID_IDirect3DRampDevice) {
        Logger::warn("D3D5Interface::CreateDevice: Unsupported Ramp device, falling back to RGB");
        rgbFallback = true;
      } else {
        Logger::warn("D3D5Interface::CreateDevice: Unknown device identifier, falling back to HAL");
        Logger::warn(str::format(rclsid));
        halFallback = true;
      }
    } else {
      // Will default to SWVP, nothing to do in that case
      if (m_options.deviceTypeOverride == D3DDeviceTypeOverride::SWVPMixed) {
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
      } else if (m_options.deviceTypeOverride == D3DDeviceTypeOverride::HWVP) {
        deviceCreationFlags9 = D3DCREATE_HARDWARE_VERTEXPROCESSING;
      } else if (m_options.deviceTypeOverride == D3DDeviceTypeOverride::HWVPMixed) {
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
      }
    }

    const IID rclsidOverride = rgbFallback ? IID_IDirect3DRGBDevice :
                               halFallback ? IID_IDirect3DHALDevice : rclsid;

    DDrawCommonInterface* commonIntf = m_parent->GetCommonInterface();

    HWND hwnd = commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hwnd == nullptr)) {
      Logger::debug("D3D5Interface::CreateDevice: HWND is NULL");
    }

    Com<DDrawSurface> rt;
    if (unlikely(!m_parent->IsWrappedSurface(lpDDS))) {
      if (unlikely(m_options.proxiedQueryInterface)) {
        Logger::debug("D3D5Interface::CreateDevice: Unwrapped surface passed as RT");
        rt = new DDrawSurface(nullptr, std::move(lpDDS), m_parent, nullptr, nullptr, true);
        // Hack: attach the last created depth stencil to the unwrapped RT
        // We can not do this the usual way because the RT is not known to ddraw
        rt->SetAttachedDepthStencil(m_parent->GetLastDepthStencil());
      } else {
        Logger::err("D3D5Interface::CreateDevice: Unwrapped surface passed as RT");
        return DDERR_GENERIC;
      }
    } else {
      rt = static_cast<DDrawSurface*>(lpDDS);
    }

    Com<IDirect3DDevice2> d3d5DeviceProxy;
    HRESULT hr = m_proxy->CreateDevice(rclsidOverride, rt->GetProxied(), &d3d5DeviceProxy);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D5Interface::CreateDevice: Failed to create the proxy device");
      return hr;
    }

    DDSURFACEDESC desc;
    desc.dwSize = sizeof(DDSURFACEDESC);
    lpDDS->GetSurfaceDesc(&desc);

    DWORD backBufferWidth  = desc.dwWidth;
    DWORD BackBufferHeight = desc.dwHeight;

    if (likely(!m_options.forceProxiedPresent &&
                m_options.backBufferResize)) {
      const bool exclusiveMode = commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        DDrawModeSize* modeSize = commonIntf->GetModeSize();
        // Wayland apparently needs this for somewhat proper back buffer sizing
        if ((modeSize->width  && modeSize->width  < desc.dwWidth)
         || (modeSize->height && modeSize->height < desc.dwHeight)) {
          Logger::info("D3D5Interface::CreateDevice: Enforcing mode dimensions");

          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    // Determine the supported AA sample count by querying the D3D9 interface
    d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3d9::D3DMULTISAMPLE_NONE;
    if (likely(!m_options.disableAASupport)) {
      HRESULT hr4S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                        TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
      if (unlikely(FAILED(hr4S))) {
        HRESULT hr2S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                          TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
        if (unlikely(FAILED(hr2S))) {
          Logger::info("D3D5Interface::CreateDevice: Detected no AA support");
        } else {
          Logger::info("D3D6ID3D5Interfacenterface::CreateDevice: Detected support for 2x AA");
          multiSampleType = d3d9::D3DMULTISAMPLE_2_SAMPLES;
        }
      } else {
        Logger::info("D3D5Interface::CreateDevice: Detected support for 4x AA");
        multiSampleType = d3d9::D3DMULTISAMPLE_4_SAMPLES;
      }
    } else {
      Logger::warn("D3D5Interface::CreateDevice: AA support fully disabled");
    }

    Logger::info(str::format("D3D5Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    DWORD backBufferCount = 0;
    if (likely(!m_options.forceSingleBackBuffer)) {
      IDirectDrawSurface* backBuffer = rt->GetProxied();
      while (backBuffer != nullptr) {
        IDirectDrawSurface* parentSurface = backBuffer;
        backBuffer = nullptr;
        parentSurface->EnumAttachedSurfaces(&backBuffer, ListBackBufferSurfaces5Callback);
        backBufferCount++;
        // the swapchain will eventually return to its origin
        if (backBuffer == rt->GetProxied())
          break;
      }
    }
    // Consider the front buffer as well when reporting the overall count
    Logger::info(str::format("D3D5Interface::CreateDevice: Back buffer count: ", backBufferCount + 1));

    const DWORD cooperativeLevel = commonIntf->GetCooperativeLevel();
    // Always appears to be enabled when running in non-exclusive mode
    const bool vBlankStatus = commonIntf->GetWaitForVBlank();

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = backBufferWidth;
    params.BackBufferHeight   = BackBufferHeight;
    params.BackBufferFormat   = backBufferFormat;
    params.BackBufferCount    = backBufferCount;
    params.MultiSampleType    = multiSampleType; // Controlled through D3DRENDERSTATE_ANTIALIAS
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = hwnd;
    params.Windowed           = TRUE; // Always use windowed, so that we can delegate mode switching to ddraw
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // Needed for back buffer locks
    params.FullScreen_RefreshRateInHz = 0; // We'll get the right mode/refresh rate set by ddraw, just play along
    params.PresentationInterval       = vBlankStatus ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || m_options.forceMultiThreaded)
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    if (cooperativeLevel & DDSCL_FPUPRESERVE)
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Com<d3d9::IDirect3DDevice9> device9;
    hr = m_d3d9->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hwnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    D3DDEVICEDESC2 desc5 = GetD3D5Caps(rclsidOverride, m_options.disableAASupport);

    try{
      Com<D3D5Device> device = new D3D5Device(std::move(d3d5DeviceProxy), this, desc5,
                                              rclsidOverride, params, std::move(device9),
                                              rt.ptr(), deviceCreationFlags9);
      // Hold the address of the most recently created device, not a reference
      m_lastUsedDevice = device.ptr();
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      m_lastUsedDevice->InitializeDS();
      // Enable SWVP in case of mixed SWVP devices
      if (unlikely(m_options.deviceTypeOverride == D3DDeviceTypeOverride::SWVPMixed))
        m_lastUsedDevice->GetD3D9()->SetSoftwareVertexProcessing(TRUE);
      *lplpD3DDevice = device.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  D3D5Material* D3D5Interface::GetMaterialFromHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = m_materials.find(handle);

    if (unlikely(materialsIter == m_materials.end())) {
      Logger::warn(str::format("D3D5Interface::GetMaterialFromHandle: Invalid handle: ", handle));
      return nullptr;
    }

    return &materialsIter->second;
  }

}
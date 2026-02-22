#include "d3d6_interface.h"

#include "d3d6_device.h"
#include "d3d6_buffer.h"
#include "d3d6_light.h"
#include "d3d6_material.h"
#include "d3d6_multithread.h"
#include "d3d6_viewport.h"

#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw4/ddraw4_surface.h"

namespace dxvk {

  uint32_t D3D6Interface::s_intfCount = 0;

  D3D6Interface::D3D6Interface(Com<IDirect3D3>&& d3d6IntfProxy, DDraw4Interface* pParent)
    : DDrawWrappedObject<DDraw4Interface, IDirect3D3, d3d9::IDirect3D9>(pParent, std::move(d3d6IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION))) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->EnableD3D6CompatibilityMode();

    m_options = D3DOptions(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D6Interface: Created a new interface nr. ((3-", m_intfCount, "))"));
  }

  D3D6Interface::~D3D6Interface() {
    Logger::debug(str::format("D3D6Interface: Interface nr. ((3-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw4
  ULONG STDMETHODCALLTYPE D3D6Interface::AddRef() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->AddRef();
      else
        return m_parent->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the parent IDirectDraw4
  ULONG STDMETHODCALLTYPE D3D6Interface::Release() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->Release();
      else
        return m_parent->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Interface, IDirect3D3, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D3))
      return this;

    Logger::debug("D3D6Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy d3d interfaces
    if (unlikely(riid == __uuidof(IDirect3D)
              || riid == __uuidof(IDirect3D2))) {
      Logger::warn("D3D6Interface::QueryInterface: Query for legacy IDirect3D");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2)
              || riid == __uuidof(IDirectDraw4))) {
      Logger::debug("D3D6Interface::QueryInterface: Query for IDirectDraw");
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

  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug(">>> D3D6Interface::EnumDevices");

    if (unlikely(lpEnumDevicesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    // D3D6 reports both HAL and HEL caps for any time of device,
    // with minor differences between the two. Note that the
    // device listing order matters, so list RGB first, HAL second.

    // Software emulation, this is expected to be exposed (SWVP)
    GUID guidRGB = IID_IDirect3DRGBDevice;
    D3DDEVICEDESC descRGB_HAL = GetD3D6Caps(IID_IDirect3DRGBDevice, m_options.emulateFSAA != FSAAEmulation::Disabled);
    D3DDEVICEDESC descRGB_HEL = descRGB_HAL;
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    descRGB_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;
    char deviceDescRGB[100] = "D6VK RGB";
    char deviceNameRGB[100] = "D6VK RGB";

    HRESULT hr = lpEnumDevicesCallback(const_cast<GUID*>(&guidRGB), &deviceDescRGB[0],
                                       &deviceNameRGB[0], &descRGB_HAL, &descRGB_HEL, lpUserArg);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration (SWVP)
    GUID guidHAL = IID_IDirect3DHALDevice;
    D3DDEVICEDESC descHAL_HAL = GetD3D6Caps(IID_IDirect3DHALDevice, m_options.emulateFSAA != FSAAEmulation::Disabled);
    D3DDEVICEDESC descHAL_HEL = descHAL_HAL;
    descHAL_HEL.dcmColorModel = 0;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    char deviceDescHAL[100] = "D6VK HAL";
    char deviceNameHAL[100] = "D6VK HAL";

    hr = lpEnumDevicesCallback(const_cast<GUID*>(&guidHAL), &deviceDescHAL[0],
                               &deviceNameHAL[0], &descHAL_HAL, &descHAL_HEL, lpUserArg);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateLight");

    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    // We do not really need a proxy light object, it's a simple container
    *lplpDirect3DLight = ref(new D3D6Light(nullptr, this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateMaterial");

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

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateViewport");

    Com<IDirect3DViewport3> lplpD3DViewportProxy;
    HRESULT hr = m_proxy->CreateViewport(&lplpD3DViewportProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D6Viewport(nullptr, std::move(lplpD3DViewportProxy), this));

    return D3D_OK;
  }

  // Minimal implementation which should suffice in most cases
  HRESULT STDMETHODCALLTYPE D3D6Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug(">>> D3D6Interface::FindDevice");

    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    // Software emulation, this is expected to be exposed (SWVP)
    D3DDEVICEDESC descRGB_HAL = GetD3D6Caps(IID_IDirect3DRGBDevice, m_options.emulateFSAA != FSAAEmulation::Disabled);
    D3DDEVICEDESC descRGB_HEL = descRGB_HAL;
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HEL.dpcLineCaps.dwTextureCaps |= D3DPTEXTURECAPS_POW2;
    descRGB_HEL.dpcTriCaps.dwTextureCaps  |= D3DPTEXTURECAPS_POW2;

    // Hardware acceleration (SWVP)
    D3DDEVICEDESC descHAL_HAL = GetD3D6Caps(IID_IDirect3DHALDevice, m_options.emulateFSAA != FSAAEmulation::Disabled);
    D3DDEVICEDESC descHAL_HEL = descHAL_HAL;
    descHAL_HEL.dcmColorModel = 0;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;

    lpD3DFDR->dwSize = sizeof(D3DFINDDEVICERESULT);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      Logger::debug("D3D6Interface::FindDevice: Matching by device GUID");

      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice ||
          lpD3DFDS->guid == IID_IDirect3DMMXDevice ||
          lpD3DFDS->guid == IID_IDirect3DRampDevice) {
        Logger::debug("D3D6Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFDR->guid = IID_IDirect3DRGBDevice;
        lpD3DFDR->ddHwDesc = descRGB_HAL;
        lpD3DFDR->ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        Logger::debug("D3D6Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFDR->guid = IID_IDirect3DHALDevice;
        lpD3DFDR->ddHwDesc = descHAL_HAL;
        lpD3DFDR->ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D6Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      Logger::debug("D3D6Interface::FindDevice: Matching by hardware flag");

      if (likely(lpD3DFDS->bHardware == TRUE)) {
        Logger::debug("D3D6Interface::FindDevice: Matched IID_IDirect3DHALDevice");
        lpD3DFDR->guid = IID_IDirect3DHALDevice;
        lpD3DFDR->ddHwDesc = descHAL_HAL;
        lpD3DFDR->ddSwDesc = descHAL_HEL;
      } else {
        Logger::debug("D3D6Interface::FindDevice: Matched IID_IDirect3DRGBDevice");
        lpD3DFDR->guid = IID_IDirect3DRGBDevice;
        lpD3DFDR->ddHwDesc = descRGB_HAL;
        lpD3DFDR->ddSwDesc = descRGB_HEL;
      }
    } else {
      Logger::err("D3D6Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateDevice");

    if (unlikely(lplpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpD3DDevice);

    if (unlikely(lpDDS == nullptr)) {
      Logger::err("D3D6Interface::CreateDevice: Null surface provided");
      return DDERR_INVALIDPARAMS;
    }

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  rgbFallback          = false;

    if (likely(m_options.deviceTypeOverride == D3DDeviceTypeOverride::None)) {
      if (rclsid == IID_IDirect3DHALDevice) {
        Logger::info("D3D6Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
      } else if (rclsid == IID_IDirect3DRGBDevice) {
        Logger::info("D3D6Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
      } else if (rclsid == IID_IDirect3DMMXDevice) {
        Logger::warn("D3D6Interface::CreateDevice: Unsupported MMX device, falling back to RGB");
        rgbFallback = true;
      } else {
        // Revenant uses a rclsid of 7a31a548-0000-0007-26ed-780000000000...
        Logger::warn("D3D6Interface::CreateDevice: Unsupported device type, falling back to RGB");
        Logger::warn(str::format(rclsid));
        rgbFallback = true;
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

    const IID rclsidOverride = rgbFallback ? IID_IDirect3DRGBDevice : rclsid;

    DDrawCommonInterface* commonIntf = m_parent->GetCommonInterface();

    HWND hWnd = commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hWnd == nullptr)) {
      Logger::debug("D3D6Interface::CreateDevice: HWND is NULL");
    }

    Com<DDraw4Surface> rt4;
    if (unlikely(!m_parent->GetCommonInterface()->IsWrappedSurface(lpDDS))) {
      Logger::err("D3D6Interface::CreateDevice: Unwrapped surface passed as RT");
      return DDERR_GENERIC;
    } else {
      rt4 = static_cast<DDraw4Surface*>(lpDDS);
    }

    Com<IDirect3DDevice3> d3d6DeviceProxy;
    HRESULT hr = m_proxy->CreateDevice(rclsidOverride, rt4->GetProxied(), &d3d6DeviceProxy, nullptr);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D6Interface::CreateDevice: Failed to create the proxy device");
      return hr;
    }

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
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
          Logger::info("D3D6Interface::CreateDevice: Enforcing mode dimensions");

          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    // Determine the supported AA sample count by querying the D3D9 interface
    d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3d9::D3DMULTISAMPLE_NONE;
    if (likely(m_options.emulateFSAA != FSAAEmulation::Disabled)) {
      HRESULT hr4S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                        TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
      if (unlikely(FAILED(hr4S))) {
        HRESULT hr2S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                          TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
        if (unlikely(FAILED(hr2S))) {
          Logger::warn("D3D6Interface::CreateDevice: No MSAA support has been detected");
        } else {
          Logger::info("D3D6Interface::CreateDevice: Using 2x MSAA for FSAA emulation");
          multiSampleType = d3d9::D3DMULTISAMPLE_2_SAMPLES;
        }
      } else {
        Logger::info("D3D6Interface::CreateDevice: Using 4x MSAA for FSAA emulation");
        multiSampleType = d3d9::D3DMULTISAMPLE_4_SAMPLES;
      }
    } else {
      Logger::info("D3D6Interface::CreateDevice: FSAA emulation is disabled");
    }

    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    DWORD backBufferCount = 0;
    if (likely(!m_options.forceSingleBackBuffer)) {
      IDirectDrawSurface4* backBuffer = rt4->GetProxied();
      while (backBuffer != nullptr) {
        IDirectDrawSurface4* parentSurface = backBuffer;
        backBuffer = nullptr;
        parentSurface->EnumAttachedSurfaces(&backBuffer, ListBackBufferSurfaces4Callback);
        backBufferCount++;
        // the swapchain will eventually return to its origin
        if (backBuffer == rt4->GetProxied())
          break;
      }
    }
    // Consider the front buffer as well when reporting the overall count
    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer count: ", backBufferCount + 1));

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
    params.hDeviceWindow      = hWnd;
    params.Windowed           = TRUE; // Always use windowed, so that we can delegate mode switching to ddraw
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // Needed for back buffer locks
    params.FullScreen_RefreshRateInHz = 0; // We'll get the right mode/refresh rate set by ddraw, just play along
    params.PresentationInterval       = vBlankStatus ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || m_options.forceMultiThreaded)
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    // DDSCL_FPUPRESERVE does not exist prior to DDraw7,
    // and DDSCL_FPUSETUP is NOT the default state
    if (!(cooperativeLevel & DDSCL_FPUSETUP))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Com<d3d9::IDirect3DDevice9> device9;
    hr = m_d3d9->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hWnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    D3DDEVICEDESC desc6 = GetD3D6Caps(rclsidOverride, m_options.emulateFSAA != FSAAEmulation::Disabled);

    try{
      Com<D3D6Device> device6 = new D3D6Device(std::move(d3d6DeviceProxy), this, desc6,
                                               rclsidOverride, params, std::move(device9),
                                               rt4.ptr(), deviceCreationFlags9);
      // Set the newly created D3D6 device on the common interface
      commonIntf->SetD3D6Device(device6.ptr());
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      device6->InitializeDS();
      // Enable SWVP in case of mixed SWVP devices
      if (unlikely(m_options.deviceTypeOverride == D3DDeviceTypeOverride::SWVPMixed))
        device6->GetD3D9()->SetSoftwareVertexProcessing(TRUE);
      *lplpD3DDevice = device6.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateVertexBuffer(LPD3DVERTEXBUFFERDESC lpVBDesc, LPDIRECT3DVERTEXBUFFER *lpD3DVertexBuffer, DWORD dwFlags, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateVertexBuffer");

    if (unlikely(lpVBDesc == nullptr || lpD3DVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lpD3DVertexBuffer);

    // TODO: This, apparently, isn't validated by native D3D6 (to check)
    //if (unlikely(lpVBDesc->dwSize != sizeof(D3DVERTEXBUFFERDESC)))
      //return DDERR_INVALIDPARAMS;

    Com<IDirect3DVertexBuffer> vertexBuffer;
    // We don't really need a proxy buffer any longer
    /*HRESULT hr = m_proxy->CreateVertexBuffer(desc, &vertexBuffer, usage);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D6Interface::CreateVertexBuffer: Failed to create proxy vertex buffer");
      return hr;
    }*/

    // We need to delay the D3D9 vertex buffer creation as long as possible, to ensure
    // that (ideally) we actually have a valid D3D6 device in place when that happens
    *lpD3DVertexBuffer = ref(new D3D6VertexBuffer(std::move(vertexBuffer), nullptr, this, dwFlags, *lpVBDesc));

    return D3D_OK;
  }

  // Total Club Manager 2003 uses a D3D6 interface to query for supported Z buffer formats,
  // so report what we know is supported by D3D9, otherwise the game will error out on startup
  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext) {
    Logger::debug(">>> D3D6Interface::EnumZBufferFormats");

    if (unlikely(lpEnumCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    // There are just 3 supported depth stencil formats to worry about
    // in D3D9, so let's just enumerate them liniarly, for better clarity

    DDPIXELFORMAT depthFormat = GetZBufferFormat(d3d9::D3DFMT_D16);
    HRESULT hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24X8);
    hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24S8);
    hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::EvictManagedTextures() {
    Logger::debug(">>> D3D6Interface::EvictManagedTextures");

    HRESULT hr = m_proxy->EvictManagedTextures();
    if (unlikely(FAILED(hr)))
      return hr;

    D3D6Device* d3d6Device = m_parent->GetCommonInterface()->GetD3D6Device();
    if (likely(d3d6Device != nullptr)) {
      D3D6DeviceLock lock = d3d6Device->LockDevice();

      HRESULT hr9 = d3d6Device->GetD3D9()->EvictManagedResources();
      if (unlikely(FAILED(hr9))) {
        Logger::err("D3D6Interface::EvictManagedTextures: Failed D3D9 managed resource eviction");
        return hr9;
      }
    }

    return D3D_OK;
  }

  D3D6Material* D3D6Interface::GetMaterialFromHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = m_materials.find(handle);

    if (unlikely(materialsIter == m_materials.end())) {
      Logger::warn(str::format("D3D6Interface::GetMaterialFromHandle: Invalid handle: ", handle));
      return nullptr;
    }

    return &materialsIter->second;
  }

}
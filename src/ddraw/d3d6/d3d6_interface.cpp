#include "d3d6_interface.h"

#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw4/ddraw4_surface.h"

#include "d3d6_device.h"
#include "d3d6_buffer.h"
#include "d3d6_multithread.h"
#include "d3d6_util.h"

namespace dxvk {

  uint32_t D3D6Interface::s_intfCount = 0;

  D3D6Interface::D3D6Interface(Com<IDirect3D3>&& d3d6IntfProxy, DDraw4Interface* pParent)
    : DDrawWrappedObject<DDraw4Interface, IDirect3D3, d3d9::IDirect3D9>(pParent, std::move(d3d6IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION))) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    // TODO: Have a D3D6 equivalent
    m_bridge->EnableD3D6CompatibilityMode();

    m_d3d7Options = D3D7Options(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D6Interface: Created a new interface nr. ((3-", m_intfCount, "))"));
  }

  D3D6Interface::~D3D6Interface() {
    Logger::debug(str::format("D3D6Interface: Interface nr. ((3-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D6Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D6Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Interface, IDirect3D3, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

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
    Logger::debug("<<< D3D6Interface::EnumDevices: Proxy");
    return m_proxy->EnumDevices(lpEnumDevicesCallback, lpUserArg);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::warn("<<< D3D6Interface::CreateLight: Proxy");
    return m_proxy->CreateLight(lplpDirect3DLight, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::warn("<<< D3D6Interface::CreateMaterial: Proxy");
    return m_proxy->CreateMaterial(lplpDirect3DMaterial, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::warn("<<< D3D6Interface::CreateViewport: Proxy");
    return m_proxy->CreateViewport(lplpD3DViewport, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::warn("<<< D3D6Interface::FindDevice: Proxy");
    return m_proxy->FindDevice(lpD3DFDS, lpD3DFDR);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D6Interface::CreateDevice");

    if (unlikely(lplpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpD3DDevice);

    if (unlikely(lpDDS == nullptr)) {
      Logger::err("D3D7Interface::CreateDevice: Null surface provided");
      return DDERR_INVALIDPARAMS;
    }

    if (rclsid == IID_IDirect3DHALDevice) {
      Logger::info("D3D6Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
    } else if (rclsid == IID_IDirect3DMMXDevice) {
      Logger::info("D3D6Interface::CreateDevice: Created a IID_IDirect3DMMXDevice device");
    } else if (rclsid == IID_IDirect3DRGBDevice) {
      Logger::info("D3D6Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
    } else {
      Logger::err("D3D6Interface::CreateDevice: Unsupported device type");
      return DDERR_INVALIDPARAMS;
    }

    HWND hwnd = m_parent->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hwnd == nullptr)) {
      Logger::debug("D3D6Interface::CreateDevice: HWND is NULL");
    }

    Com<DDraw4Surface> rt4;
    if (unlikely(!m_parent->IsWrappedSurface(lpDDS))) {
      if (unlikely(m_d3d7Options.proxiedQueryInterface)) {
        Logger::debug("D3D6Interface::CreateDevice: Unwrapped surface passed as RT");
        rt4 = new DDraw4Surface(std::move(lpDDS), m_parent, nullptr, nullptr, true);
        // Hack: attach the last created depth stencil to the unwrapped RT
        // We can not do this the usual way because the RT is not known to ddraw
        rt4->SetAttachedDepthStencil(m_parent->GetLastDepthStencil());
      } else {
        Logger::err("D3D6Interface::CreateDevice: Unwrapped surface passed as RT");
        return DDERR_GENERIC;
      }
    } else {
      rt4 = static_cast<DDraw4Surface*>(lpDDS);
    }

    Com<IDirect3DDevice3> d3d6DeviceProxy;
    HRESULT hr = m_proxy->CreateDevice(rclsid, rt4->GetProxied(), &d3d6DeviceProxy, nullptr);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D6Interface::CreateDevice: Failed to create the proxy device");
      return hr;
    }

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    lpDDS->GetSurfaceDesc(&desc);

    DWORD backBufferWidth  = desc.dwWidth;
    DWORD BackBufferHeight = desc.dwHeight;

    if (likely(!m_d3d7Options.forceProxiedPresent &&
                m_d3d7Options.backBufferResize)) {
      const bool exclusiveMode = m_parent->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        DDrawModeSize modeSize = m_parent->GetModeSize();
        // Wayland apparently needs this for somewhat proper back buffer sizing
        if ((modeSize.width  && modeSize.width  < desc.dwWidth)
         || (modeSize.height && modeSize.height < desc.dwHeight)) {
          Logger::info("D3D6Interface::CreateDevice: Enforcing mode dimensions");

          backBufferWidth  = modeSize.width;
          BackBufferHeight = modeSize.height;
        }
      }
    }

    d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    // Determine the supported AA sample count by querying the D3D9 interface
    d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3d9::D3DMULTISAMPLE_NONE;
    if (likely(!m_d3d7Options.disableAASupport)) {
      HRESULT hr4S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                        TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
      if (unlikely(FAILED(hr4S))) {
        HRESULT hr2S = m_d3d9->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                          TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
        if (unlikely(FAILED(hr2S))) {
          Logger::info("D3D6Interface::CreateDevice: Detected no AA support");
        } else {
          Logger::info("D3D6Interface::CreateDevice: Detected support for 2x AA");
          multiSampleType = d3d9::D3DMULTISAMPLE_2_SAMPLES;
        }
      } else {
        Logger::info("D3D6Interface::CreateDevice: Detected support for 4x AA");
        multiSampleType = d3d9::D3DMULTISAMPLE_4_SAMPLES;
      }
    } else {
      Logger::warn("D3D6Interface::CreateDevice: AA support fully disabled");
    }

    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    DWORD backBufferCount = 0;
    if (likely(!m_d3d7Options.forceSingleBackBuffer)) {
      IDirectDrawSurface4* backBuffer = rt4->GetProxied();
      while (backBuffer != nullptr) {
        IDirectDrawSurface4* parentSurface = backBuffer;
        backBuffer = nullptr;
        parentSurface->EnumAttachedSurfaces(&backBuffer, ListBackBufferSurfaces6Callback);
        backBufferCount++;
        // the swapchain will eventually return to its origin
        if (backBuffer == rt4->GetProxied())
          break;
      }
    }
    // Consider the front buffer as well when reporting the overall count
    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer count: ", backBufferCount + 1));

    const DWORD cooperativeLevel = m_parent->GetCooperativeLevel();
    // Always appears to be enabled when running in non-exclusive mode
    const bool vBlankStatus = m_parent->GetWaitForVBlank();

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

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    if ((cooperativeLevel & DDSCL_MULTITHREADED) || m_d3d7Options.forceMultiThreaded)
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

    D3DDEVICEDESC desc6 = GetD3D6Caps(rclsid, m_d3d7Options.disableAASupport);

    try{
      Com<D3D6Device> device = new D3D6Device(std::move(d3d6DeviceProxy), this, desc6,
                                              params, std::move(device9), rt4.ptr(),
                                              deviceCreationFlags9);
      // Hold the address of the most recently created device, not a reference
      m_lastUsedDevice = device.ptr();
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      m_lastUsedDevice->InitializeDS();
      *lplpD3DDevice = device.ref();
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

    if (unlikely(lpVBDesc->dwSize != sizeof(D3DVERTEXBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    Com<IDirect3DVertexBuffer> vertexBuffer;
    // We don't really need a proxy buffer any longer
    /*HRESULT hr = m_proxy->CreateVertexBuffer(desc, &vertexBuffer, usage);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D6Interface::CreateVertexBuffer: Failed to create proxy vertex buffer");
      return hr;
    }*/

    // We need to delay the D3D9 vertex buffer creation as long as possible, to ensure
    // that (ideally) we actually have a valid d3d7 device in place when that happens
    *lpD3DVertexBuffer = ref(new D3D6VertexBuffer(std::move(vertexBuffer), nullptr, this, *lpVBDesc));

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
    Logger::debug("<<< D3D6Interface::EvictManagedTextures:  Proxy");
    return m_proxy->EvictManagedTextures();
  }

}
#include "d3d7_interface.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "d3d7_device.h"
#include "d3d7_buffer.h"
#include "d3d7_multithread.h"
#include "d3d7_util.h"

namespace dxvk {

  uint32_t D3D7Interface::s_intfCount = 0;

  D3D7Interface::D3D7Interface(Com<IDirect3D7>&& d3d7IntfProxy, DDraw7Interface* pParent)
    : DDrawWrappedObject<DDraw7Interface, IDirect3D7, d3d9::IDirect3D9>(pParent, std::move(d3d7IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION))) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->EnableD3D7CompatibilityMode();

    m_d3d7Options = D3D7Options(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D7Interface: Created a new interface nr. ((7-", m_intfCount, "))"));
  }

  D3D7Interface::~D3D7Interface() {
    Logger::debug(str::format("D3D7Interface: Interface nr. ((7-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw7
  ULONG STDMETHODCALLTYPE D3D7Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw7
  ULONG STDMETHODCALLTYPE D3D7Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw7Interface, IDirect3D7, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D7)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D7Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D7Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy d3d interfaces
    if (unlikely(riid == __uuidof(IDirect3D)
              || riid == __uuidof(IDirect3D2)
              || riid == __uuidof(IDirect3D3))) {
      Logger::warn("D3D7Interface::QueryInterface: Query for legacy IDirect3D");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2)
              || riid == __uuidof(IDirectDraw4))) {
      Logger::debug("D3D7Interface::QueryInterface: Query for legacy IDirectDraw");
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

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx) {
    Logger::debug(">>> D3D7Interface::EnumDevices");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ideally we should take all the adapters into account, however
    // D3D7 supports one RGB (software emulation) device, one HAL device,
    // and one HAL T&L device, all indentified via GUIDs

    // Note: The enumeration order seems to matter for some applications,
    // such as (The) Summoner, so always report RGB first, then HAL, then T&L HAL

    // Software emulation, this is expected to be exposed (SWVP)
    D3DDEVICEDESC7 desc7RGB = GetD3D7Caps(IID_IDirect3DRGBDevice, m_d3d7Options.disableAASupport);
    char deviceDescRGB[100] = "D7VK RGB";
    char deviceNameRGB[100] = "D7VK RGB";

    HRESULT hr = cb(&deviceDescRGB[0], &deviceNameRGB[0], &desc7RGB, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration (no T&L, SWVP)
    D3DDEVICEDESC7 desc7HAL = GetD3D7Caps(IID_IDirect3DHALDevice, m_d3d7Options.disableAASupport);
    char deviceDescHAL[100] = "D7VK HAL";
    char deviceNameHAL[100] = "D7VK HAL";

    hr = cb(&deviceDescHAL[0], &deviceNameHAL[0], &desc7HAL, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration with T&L (HWVP)
    D3DDEVICEDESC7 desc7TNL = GetD3D7Caps(IID_IDirect3DTnLHalDevice, m_d3d7Options.disableAASupport);
    char deviceDescTNL[100] = "D7VK T&L HAL";
    char deviceNameTNL[100] = "D7VK T&L HAL";

    hr = cb(&deviceDescTNL[0], &deviceNameTNL[0], &desc7TNL, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateDevice(REFCLSID rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice) {
    Logger::debug(">>> D3D7Interface::CreateDevice");

    if (unlikely(ppd3dDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(ppd3dDevice);

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D7Interface::CreateDevice: Null surface provided");
      return DDERR_INVALIDPARAMS;
    }

    DWORD vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      if (likely(!m_d3d7Options.forceSWVPDevice)) {
        Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DTnLHalDevice device");
        // Do not use exclusive HWVP, since some games call ProcessVertices
        // even in situations where they are expliclity using HW T&L
        vertexProcessing = D3DCREATE_MIXED_VERTEXPROCESSING;
      } else {
        Logger::info("D3D7Interface::CreateDevice: Using SWVP for a IID_IDirect3DTnLHalDevice device");
      }
    } else if (rclsid == IID_IDirect3DHALDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
    } else if (rclsid == IID_IDirect3DRGBDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
    } else {
      Logger::err("D3D7Interface::CreateDevice: Unknown device type");
      return DDERR_INVALIDPARAMS;
    }

    HWND hwnd = m_parent->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hwnd == nullptr)) {
      Logger::debug("D3D7Interface::CreateDevice: HWND is NULL");
    }

    Com<DDraw7Surface> rt7;
    if (unlikely(!m_parent->IsWrappedSurface(surface))) {
      if (unlikely(m_d3d7Options.proxiedQueryInterface)) {
        Logger::debug("D3D7Interface::CreateDevice: Unwrapped surface passed as RT");
        rt7 = new DDraw7Surface(std::move(surface), m_parent, nullptr, true);
        // Hack: attach the last created depth stencil to the unwrapped RT
        // We can not do this the usual way because the RT is not known to ddraw
        rt7->SetAttachedDepthStencil(m_parent->GetLastDepthStencil());
      } else {
        Logger::err("D3D7Interface::CreateDevice: Unwrapped surface passed as RT");
        return DDERR_GENERIC;
      }
    } else {
      rt7 = static_cast<DDraw7Surface*>(surface);
    }

    Com<IDirect3DDevice7> d3d7DeviceProxy;
    HRESULT hr = m_proxy->CreateDevice(rclsid, rt7->GetProxied(), &d3d7DeviceProxy);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Interface::CreateDevice: Failed to create the proxy device");
      return hr;
    }

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);

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
          Logger::info("D3D7Interface::CreateDevice: Enforcing mode dimensions");

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
          Logger::info("D3D7Interface::CreateDevice: Detected no AA support");
        } else {
          Logger::info("D3D7Interface::CreateDevice: Detected support for 2x AA");
          multiSampleType = d3d9::D3DMULTISAMPLE_2_SAMPLES;
        }
      } else {
        Logger::info("D3D7Interface::CreateDevice: Detected support for 4x AA");
        multiSampleType = d3d9::D3DMULTISAMPLE_4_SAMPLES;
      }
    } else {
      Logger::warn("D3D7Interface::CreateDevice: AA support fully disabled");
    }

    Logger::info(str::format("D3D7Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    DWORD backBufferCount = 0;
    if (likely(!m_d3d7Options.forceSingleBackBuffer)) {
      IDirectDrawSurface7* backBuffer = rt7->GetProxied();
      while (backBuffer != nullptr) {
        IDirectDrawSurface7* parentSurface = backBuffer;
        backBuffer = nullptr;
        parentSurface->EnumAttachedSurfaces(&backBuffer, ListBackBufferSurfacesCallback);
        backBufferCount++;
        // the swapchain will eventually return to its origin
        if (backBuffer == rt7->GetProxied())
          break;
      }
    }
    // Consider the front buffer as well when reporting the overall count
    Logger::info(str::format("D3D7Interface::CreateDevice: Back buffer count: ", backBufferCount + 1));

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

    DWORD deviceCreationFlags9 = vertexProcessing;
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
      Logger::err("D3D7Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    D3DDEVICEDESC7 desc7 = GetD3D7Caps(rclsid, m_d3d7Options.disableAASupport);

    try{
      Com<D3D7Device> device = new D3D7Device(std::move(d3d7DeviceProxy), this, desc7, params,
                                              vertexProcessing, std::move(device9), rt7.ptr(),
                                              deviceCreationFlags9);
      // Hold the address of the most recently created device, not a reference
      m_lastUsedDevice = device.ptr();
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      m_lastUsedDevice->InitializeDS();
      *ppd3dDevice = device.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage) {
    Logger::debug(">>> D3D7Interface::CreateVertexBuffer");

    if (unlikely(desc == nullptr || ppVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(ppVertexBuffer);

    if (unlikely(desc->dwSize != sizeof(D3DVERTEXBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    Com<IDirect3DVertexBuffer7> vertexBuffer7;
    // We don't really need a proxy buffer any longer
    /*HRESULT hr = m_proxy->CreateVertexBuffer(desc, &vertexBuffer7, usage);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Interface::CreateVertexBuffer: Failed to create proxy vertex buffer");
      return hr;
    }*/

    // We need to delay the D3D9 vertex buffer creation as long as possible, to ensure
    // that (ideally) we actually have a valid d3d7 device in place when that happens
    *ppVertexBuffer = ref(new D3D7VertexBuffer(std::move(vertexBuffer7), nullptr, this, *desc));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK cb, LPVOID ctx) {
    Logger::debug(">>> D3D7Interface::EnumZBufferFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // There are just 3 supported depth stencil formats to worry about
    // in D3D9, so let's just enumerate them liniarly, for better clarity

    DDPIXELFORMAT depthFormat = GetZBufferFormat(d3d9::D3DFMT_D16);
    HRESULT hr = cb(&depthFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24X8);
    hr = cb(&depthFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24S8);
    hr = cb(&depthFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EvictManagedTextures() {
    Logger::debug(">>> D3D7Interface::EvictManagedTextures");

    if (likely(m_lastUsedDevice != nullptr)) {
      D3D7DeviceLock lock = m_lastUsedDevice->LockDevice();

      HRESULT hr = m_lastUsedDevice->GetD3D9()->EvictManagedResources();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Interface::EvictManagedTextures: Failed D3D9 managed resource eviction");
        return hr;
      }
    }

    return m_proxy->EvictManagedTextures();
  }

}
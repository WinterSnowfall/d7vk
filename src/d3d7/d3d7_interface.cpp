#include "d3d7_interface.h"

#include "d3d7_device.h"
#include "d3d7_buffer.h"
#include "d3d7_util.h"
#include "ddraw7_interface.h"
#include "ddraw7_surface.h"

namespace dxvk {

  uint32_t D3D7Interface::s_intfCount = 0;

  // TODO: Figure out why these can't be used directly
  static constexpr IID IID_IDirect3DRGBDevice    = { 0xa4665c60, 0x2673, 0x11cf, {0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56} };
  static constexpr IID IID_IDirect3DHALDevice    = { 0x84e63de0, 0x46aa, 0x11cf, {0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e} };
  static constexpr IID IID_IDirect3DTnLHalDevice = { 0xf5049e78, 0x4861, 0x11d2, {0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8} };

  D3D7Interface::D3D7Interface(Com<IDirect3D7>&& d3d7IntfProxy, DDraw7Interface* pParent)
    : DDrawWrappedObject<d3d9::IDirect3D9, IDirect3D7>(std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION)), std::move(d3d7IntfProxy))
    , m_parent ( pParent )
    , m_desc ( GetBaseD3D7Caps() ) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->EnableD3D7CompatibilityMode();

    m_d3d7Options = D3D7Options(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D7Interface: Created a new interface nr. ((", m_intfCount, "))"));
  }

  D3D7Interface::~D3D7Interface() {
    // Clear up the d3d7 interface device pointer if it points to this interface
    if (m_parent->GetD3D7Interface() == this)
      m_parent->ClearD3D7Interface();

    Logger::debug(str::format("D3D7Interface: Interface nr. ((", m_intfCount, ")) bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx) {
    Logger::debug(">>> D3D7Interface::EnumDevices");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ideally we should take all the adapters into account, however
    // D3D7 supports one HAL T&L device, one HAL device, and one
    // RGB (software emulation) device, all indentified via GUIDs

    // Make a copy, because we need to alter stuff
    D3DDEVICEDESC7 desc7 = m_desc;

    // Hardware acceleration with T&L (HWVP)
    desc7.deviceGUID = IID_IDirect3DTnLHalDevice;
    char deviceNameTNL[100] = "D7VK T&L HAL";
    char deviceDescTNL[100] = "D7VK T&L HAL";

    HRESULT hr = cb(&deviceNameTNL[0], &deviceDescTNL[0], &desc7, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration (no T&L, SWVP)
    desc7.deviceGUID = IID_IDirect3DHALDevice;
    desc7.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT;
    char deviceNameHAL[100] = "D7VK HAL";
    char deviceDescHAL[100] = "D7VK HAL";

    hr = cb(&deviceNameHAL[0], &deviceDescHAL[0], &desc7, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Software emulation, this is expected to be exposed (SWVP)
    desc7.deviceGUID = IID_IDirect3DRGBDevice;
    desc7.dwDevCaps &= ~(D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION);
    char deviceNameRGB[100] = "D7VK RGB";
    char deviceDescRGB[100] = "D7VK RGB";

    cb(&deviceNameRGB[0], &deviceDescRGB[0], &desc7, ctx);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateDevice(const IID& rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice) {
    Logger::debug(">>> D3D7Interface::CreateDevice");

    if (unlikely(ppd3dDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D7Interface::CreateDevice: Null surface provided");
      return DDERR_INVALIDPARAMS;
    }

    DWORD vertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    bool  isRGBDevice      = false;

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DTnLHalDevice device");
    } else if (rclsid == IID_IDirect3DHALDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
      vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    } else if (rclsid == IID_IDirect3DRGBDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
      vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
      isRGBDevice = true;
    } else {
      Logger::err("D3D7Interface::CreateDevice: Unknown device type");
      return DDERR_INVALIDPARAMS;
    }

    HWND hwnd = m_parent->GetHWND();

    if (unlikely(hwnd == nullptr)) {
      Logger::err("D3D7Interface::CreateDevice: HWND is NULL");
      return DDERR_NOHWND;
    }

    if (unlikely(!m_parent->IsWrappedSurface(surface))) {
      Logger::err("D3D7Interface::CreateDevice: Unwrapped surface passed as RT");
      return DDERR_GENERIC;
    }

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);

    Logger::info(str::format("D3D7Interface::CreateDevice: RenderTarget: ", desc.dwWidth, "x", desc.dwHeight));

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = desc.dwWidth;
    params.BackBufferHeight   = desc.dwHeight;
    params.BackBufferFormat   = ConvertFormat(desc.ddpfPixelFormat);
    // TODO: Potentially cater for multiple back buffers, though in
    // practice their use isn't very common in d3d7
    params.BackBufferCount    = 1;
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = hwnd;
    params.Windowed           = TRUE; // TODO: Always windowed?
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

    if (unlikely(m_d3d7Options.forceMSAA != -1)) {
      params.MultiSampleType = d3d9::D3DMULTISAMPLE_TYPE(std::min<uint32_t>(8u, m_d3d7Options.forceMSAA));
    } else {
      params.MultiSampleType = d3d9::D3DMULTISAMPLE_NONE;
    }

    Com<d3d9::IDirect3DDevice9> device9 = nullptr;

    HRESULT hr = m_d3d9->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hwnd,
      vertexProcessing,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateDevice: Failed to created the nested D3D9 device");
      return hr;
    }

    Com<IDirect3DDevice7> d3d7DeviceProxy;
    DDraw7Surface* rt7 = static_cast<DDraw7Surface*>(surface);
    hr = m_proxy->CreateDevice(rclsid, rt7->GetProxied(), &d3d7DeviceProxy);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateDevice: Failed to created the proxy device");
      return hr;
    }

    // Store the GUID of the created device in m_desc
    m_desc.deviceGUID = rclsid;

    try{
      Com<D3D7Device> device = new D3D7Device(std::move(d3d7DeviceProxy), this, m_parent,
                                              std::move(device9), rt7, isRGBDevice);
      // Hold the address of the most recently created device, not a reference
      m_device = device.ptr();
      // Now that we have a valid d3d9 device pointer, we can initialize the depth stencil (if any)
      device->InitializeDS();
      *ppd3dDevice = device.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      *ppd3dDevice = nullptr;
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage) {
    Logger::debug(">>> D3D7Interface::CreateVertexBuffer");

    if (unlikely(desc == nullptr || ppVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(desc->dwSize != sizeof(D3DVERTEXBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(desc->dwNumVertices > D3DMAXNUMVERTICES))
      return DDERR_INVALIDPARAMS;

    // TODO: See if it actually matters to validate desc.dwFVF

    IDirect3DVertexBuffer7* vertexBuffer7 = nullptr;
    // We don't really need a proxy buffer any longer
    /*HRESULT hr = m_proxy->CreateVertexBuffer(desc, &vertexBuffer7, usage);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateVertexBuffer: Failed to create proxy vertex buffer");
      return hr;
    }*/

    // Can't create anything without a valid device
    if (unlikely(m_device == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DVertexBuffer9> buffer = nullptr;
    DWORD Usage = ConvertUsageFlags(desc->dwCaps, m_device->IsRGBDevice());
    DWORD Size  = GetFVFSize(desc->dwFVF) * desc->dwNumVertices;
    HRESULT hr = m_device->GetD3D9()->CreateVertexBuffer(Size, Usage, desc->dwFVF, d3d9::D3DPOOL_DEFAULT, &buffer, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateVertexBuffer: Failed to create vertex buffer");
      return hr;
    }

    *ppVertexBuffer = ref(new D3D7VertexBuffer(vertexBuffer7, std::move(buffer), this, *desc));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK cb, LPVOID ctx) {
    Logger::debug(">>> D3D7Interface::EnumZBufferFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // There are just 3 supported depth stencil formats to worry about
    // in d3d9, so let's just enumerate them liniarly, for better clarity

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

  // Typically gets called during tear-down, when it won't do much good anyway
  HRESULT STDMETHODCALLTYPE D3D7Interface::EvictManagedTextures() {
    Logger::debug(">>> D3D7Interface::EvictManagedTextures");
    if (m_device != nullptr) {
      m_device->LockDevice();
      m_device->GetD3D9()->EvictManagedResources();
    }
    return m_proxy->EvictManagedTextures();
  }

}
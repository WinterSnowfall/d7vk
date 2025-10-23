#include "d3d7_interface.h"

#include "d3d7_device.h"
#include "d3d7_buffer.h"
#include "d3d7_util.h"
#include "ddraw7_interface.h"
#include "ddraw7_surface.h"

namespace dxvk {

  // TODO: Figure out why these can't be used directly
  static constexpr IID IID_IDirect3DRGBDevice    = { 0xa4665c60, 0x2673, 0x11cf, {0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56} };
  static constexpr IID IID_IDirect3DHALDevice    = { 0x84e63de0, 0x46aa, 0x11cf, {0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e} };
  static constexpr IID IID_IDirect3DTnLHalDevice = { 0xf5049e78, 0x4861, 0x11d2, {0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8} };

  D3D7Interface::D3D7Interface(Com<IDirect3D7>&& d3d7IntfProxy, DDraw7Interface* pParent)
    : DDrawWrappedObject<d3d9::IDirect3D9, IDirect3D7>(std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION)), std::move(d3d7IntfProxy))
    , m_parent ( pParent ) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->EnableD3D7CompatibilityMode();
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx) {
    Logger::debug(">>> D3D7Interface::EnumDevices");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ideally we should take all the adapters into account, however
    // D3D7 supports one HAL device, one HAL T&L device, and one
    // RGB (software emulation) device, all indentified via GUIDs
    m_desc = GetBaseD3D7Caps();
    // Make a copy, because we need to alter stuff
    D3DDEVICEDESC7 desc7 = m_desc;

    // Hardware acceleration with T&L
    desc7.deviceGUID = IID_IDirect3DTnLHalDevice;
    char deviceNameTNL[50] = "D7VK T&L HAL";
    char deviceDescTNL[50] = "D7VK Direct3D7 T&L HAL";

    HRESULT hr = cb(&deviceNameTNL[0], &deviceDescTNL[0], &desc7, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Hardware acceleration (no T&L)
    desc7.deviceGUID = IID_IDirect3DHALDevice;
    desc7.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT;
    char deviceNameHAL[50] = "D7VK HAL";
    char deviceDescHAL[50] = "D7VK Direct3D7 HAL";

    hr = cb(&deviceNameHAL[0], &deviceDescHAL[0], &desc7, ctx);
    if (hr == D3DENUMRET_CANCEL)
      return D3D_OK;

    // Software emulation, this is expected to be exposed
    desc7.deviceGUID = IID_IDirect3DRGBDevice;
    desc7.dwDevCaps &= ~(D3DDEVCAPS_DRAWPRIMITIVES2EX | D3DDEVCAPS_HWRASTERIZATION);
    char deviceNameRGB[50] = "D7VK RGB";
    char deviceDescRGB[50] = "D7VK Direct3D7 RGB";

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

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);

    Logger::info(str::format("D3D7Interface::CreateDevice: RenderTarget: ", desc.dwWidth, "x", desc.dwHeight));

    if (unlikely(m_hwnd == nullptr)) {
      Logger::err("D3D7Interface::CreateDevice: HWND is NULL");
      return DDERR_GENERIC;
    }

    IDirectDrawSurface7* depthStencil = nullptr;
    DDSURFACEDESC2 descDS;
    descDS.dwSize = sizeof(DDSURFACEDESC2);

    if (likely(m_parent->IsWrappedSurface(surface))) {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(surface);
      depthStencil = ddraw7Surface->GetAttachedDepthStencil();
      if (depthStencil != nullptr) {
        Logger::debug("D3D7Interface::CreateDevice: Got depth stencil from RT");
        depthStencil->GetSurfaceDesc(&descDS);
        Logger::info(str::format("D3D7Interface::CreateDevice: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));
      } else {
        Logger::debug("D3D7Interface::CreateDevice: RT has no depth stencil attached");
      }
    }

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = desc.dwWidth;
    params.BackBufferHeight   = desc.dwHeight;
    params.BackBufferFormat   = ConvertFormat(desc.ddpfPixelFormat);
    params.BackBufferCount    = 1;
    params.MultiSampleType    = d3d9::D3DMULTISAMPLE_NONE;
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = m_hwnd;
    params.Windowed           = TRUE; // TODO: Always windowed?
    params.EnableAutoDepthStencil     = TRUE;
    params.AutoDepthStencilFormat     = depthStencil != nullptr ? ConvertFormat(descDS.ddpfPixelFormat) : d3d9::D3DFMT_D16;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

    Com<d3d9::IDirect3DDevice9> device9 = nullptr;

    HRESULT hr = m_d3d9->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      m_hwnd,
      D3DCREATE_HARDWARE_VERTEXPROCESSING,
      &params,
      &device9
    );
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateDevice: Failed to created the nested D3D9 device");
      return hr;
    }

    Com<IDirect3DDevice7> d3d7DeviceProxy;
    DDraw7Surface* ddraw7Surface;

    if (likely(m_parent->IsWrappedSurface(surface))) {
      ddraw7Surface = static_cast<DDraw7Surface*>(surface);
      hr = m_proxy->CreateDevice(rclsid, ddraw7Surface->GetProxied(), &d3d7DeviceProxy);
    } else {
      Logger::err("D3D7Interface::CreateDevice: Non wrapped surface passed as RT");
      return DDERR_GENERIC;
    }

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateDevice: Failed to created the proxy device");
      return hr;
    }

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DTnLHalDevice device");
    } else if (rclsid == IID_IDirect3DHALDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DHALDevice device");
    } else if (rclsid == IID_IDirect3DRGBDevice) {
      Logger::info("D3D7Interface::CreateDevice: Created a IID_IDirect3DRGBDevice device");
    } else {
      Logger::warn("D3D7Interface::CreateDevice: Created an unknown device type");
    }

    // Store the GUID of the created device in m_desc
    m_desc.deviceGUID = rclsid;

    try{
      Com<D3D7Device> device = new D3D7Device(std::move(d3d7DeviceProxy), this, m_parent, std::move(device9), ddraw7Surface);
      // Hold the address of the most recently created device, not a reference
      m_device = device.ptr();
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

    IDirect3DVertexBuffer7* vertexBuffer7 = nullptr;
    HRESULT hr = m_proxy->CreateVertexBuffer(desc, &vertexBuffer7, usage);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateVertexBuffer: Failed to create proxy vertex buffer");
      return hr;
    }

    // Can't create anything without a valid device
    if (unlikely(m_device == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DVertexBuffer9> buffer = nullptr;
    DWORD Flags = ConvertUsageFlags(desc->dwCaps);
    DWORD Size  = GetFVFSize(desc->dwFVF) * desc->dwNumVertices;
    hr = m_device->GetD3D9()->CreateVertexBuffer(Size, Flags | D3DUSAGE_DYNAMIC, desc->dwFVF, d3d9::D3DPOOL_DEFAULT, &buffer, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateVertexBuffer: Failed to create vertex buffer");
      return hr;
    }

    *ppVertexBuffer = ref(new D3D7VertexBuffer(vertexBuffer7, std::move(buffer), this, *desc));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumZBufferFormats(const IID& device_iid, LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    Logger::debug("<<< D3D7Interface::EnumZBufferFormats: Proxy");
    return m_proxy->EnumZBufferFormats(device_iid, cb, ctx);
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EvictManagedTextures() {
    Logger::debug(">>> D3D7Interface::EvictManagedTextures");
    if (m_device != nullptr) {
      m_device->LockDevice();
      m_device->GetD3D9()->EvictManagedResources();
    }
    // TODO: Check if this does more harm than good, because we keep hard
    // references to wrapped textures, which might blow up on eviction
    return m_proxy->EvictManagedTextures();
  }

}
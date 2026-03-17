#include "d3d3_device.h"

#include "d3d3_execute_buffer.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"

#include "../d3d5/d3d5_device.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D3Device::s_deviceCount = 0;

  D3D3Device::D3D3Device(
      Com<IDirect3DDevice>&& d3d3DeviceProxy,
      DDrawSurface* pParent,
      D3DDEVICEDESC3 Desc,
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

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
      Logger::warn("D3D3Device: Force enabling AA");
      m_d3d9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    }

    if (m_commonIntf->GetD3D5Device() == nullptr) {
      // The default value of D3DRENDERSTATE_TEXTUREMAPBLEND in D3D3 is D3DTBLEND_MODULATE
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_DIFFUSE);
      m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
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

    D3DDEVICEDESC3 desc_HAL = m_desc;
    D3DDEVICEDESC3 desc_HEL = m_desc;

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

    memcpy(hal_desc, &desc_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(hel_desc, &desc_HEL, sizeof(D3DDEVICEDESC3));

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
      const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

      if (d3dOptions->forceProxiedPresent) {
        // If we have drawn anything, we need to make sure we blit back
        // the results onto the D3D3 render target before we flip it
        if (m_commonIntf->HasDrawn())
          BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(m_rt->GetProxied(), m_rt->GetD3D9());

        m_rt->GetProxied()->Flip(static_cast<IDirectDrawSurface*>(m_commonIntf->GetFlipRTSurface()),
                                 m_commonIntf->GetFlipRTFlags());

        if (likely(d3dOptions->backBufferGuard != D3DBackBufferGuard::Strict))
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

    *d3d = m_commonIntf->GetD3D3Interface();

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
    Logger::debug(">>> D3D3Device::CreateExecuteBuffer");

    if (unlikely(desc == nullptr || buffer == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(buffer);

    Com<IDirect3DExecuteBuffer> bufferProxy;
    *buffer = ref(new D3D3ExecuteBuffer(std::move(bufferProxy), *desc, this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags) {
    Logger::debug(">>> D3D3Device::Execute");

    if (unlikely(buffer == nullptr || viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3ExecuteBuffer* d3d3ExecuteBuffer = static_cast<D3D3ExecuteBuffer*>(buffer);
    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);

    if (m_currentViewport != d3d3Viewport)
      m_currentViewport = d3d3Viewport;

    D3DEXECUTEDATA data = d3d3ExecuteBuffer->GetExecuteData();
    std::vector<uint8_t> executeBuffer = d3d3ExecuteBuffer->GetBuffer();
    if (unlikely(executeBuffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    uint8_t* buf = executeBuffer.data();
    const D3DTLVERTEX* vertexBuffer = reinterpret_cast<const D3DTLVERTEX*>(&buf[data.dwVertexOffset]);
    uint8_t* ptr = buf + data.dwInstructionOffset;
    uint8_t* end = ptr + data.dwInstructionLength;

    while (ptr < end - sizeof(D3DINSTRUCTION)) {
      D3DINSTRUCTION* instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
      const uint8_t  size  = instruction->bSize;
      const uint16_t count = instruction->wCount;
      const uint32_t instructionSize = sizeof(D3DINSTRUCTION) + (count * size);
      DWORD opcode = instruction->bOpcode;
      uint8_t* operation = ptr + sizeof(D3DINSTRUCTION);

      if (opcode == D3DOP_EXIT)
        break;

      if (unlikely(ptr + instructionSize > end)) {
        Logger::warn("D3D3Device::Execute: reached the end but there are no D3DOP_EXIT!");
        break;
      }

      switch (opcode) {
        case D3DOP_BRANCHFORWARD: {
          Logger::debug("D3D3Device::Execute: D3DOP_BRANCHFORWARD");

          D3DBRANCH* branch = reinterpret_cast<D3DBRANCH*>(operation);
          for (uint16_t i = 0; i < count; i++) {
            D3DBRANCH& b = branch[i];

            bool masked = (data.dsStatus.dwStatus & b.dwMask) == b.dwValue;
            if (b.bNegate) {
              masked = !masked;
            }

            if (masked && b.dwOffset) {
              ptr+= branch->dwOffset - instructionSize;
            }
          }

          break;
        }
        case D3DOP_LINE: {
          Logger::debug("D3D3Device::Execute: D3DOP_LINE");

          D3DLINE* line = reinterpret_cast<D3DLINE*>(operation);
          DrawLineInternal(line, count, data.dwVertexCount, vertexBuffer);

          break;
        }
        case D3DOP_POINT: {
          Logger::debug("D3D3Device::Execute: D3DOP_POINT");

          D3DPOINT* point = reinterpret_cast<D3DPOINT*>(operation);
          DrawPointInternal(point, count, data.dwVertexCount, vertexBuffer);

          break;
        }
        case D3DOP_TRIANGLE: {
          Logger::debug("D3D3Device::Execute: D3DOP_TRIANGLE");

          D3DTRIANGLE* triangle = reinterpret_cast<D3DTRIANGLE*>(operation);
          DrawTriangleInternal(triangle, count, data.dwVertexCount, vertexBuffer);

          break;
        }
        case D3DOP_MATRIXLOAD: {
          static bool s_matrixLoadErrorShown;
          if (!std::exchange(s_matrixLoadErrorShown, true))
            Logger::warn("D3D3Device::Execute: D3DOP_MATRIXLOAD is not implemented");
          break;
        }
        case D3DOP_MATRIXMULTIPLY: {
          static bool s_matrixMultiplyErrorShown;
          if (!std::exchange(s_matrixMultiplyErrorShown, true))
            Logger::warn("D3D3Device::Execute: D3DOP_MATRIXMULTIPLY is not implemented");
          break;
        }
        case D3DOP_PROCESSVERTICES: {
          D3DPROCESSVERTICES* processVertices = reinterpret_cast<D3DPROCESSVERTICES*>(operation);

          for (uint16_t i = 0; i < count; i++) {
            D3DPROCESSVERTICES& pv = processVertices[i];
            if (pv.dwFlags & D3DPROCESSVERTICES_COPY) {
              static bool s_pvCopyErrorShown;
              // Appears to be mostly harmless
              if (!std::exchange(s_pvCopyErrorShown, true))
                Logger::debug("D3D3Device::Execute: D3DOP_PROCESSVERTICES COPY is not implemented");
            }
            // D3DPROCESSVERTICES_NOCOLOR and D3DPROCESSVERTICES_UPDATEEXTENTS are additional flags for transforms
            if (pv.dwFlags & D3DPROCESSVERTICES_TRANSFORM) {
              static bool s_pvTransformErrorShown;
              if (!std::exchange(s_pvTransformErrorShown, true))
                Logger::warn("D3D3Device::Execute: D3DOP_PROCESSVERTICES TRANSFORM is not implemented");
            }
            if (pv.dwFlags & D3DPROCESSVERTICES_TRANSFORMLIGHT) {
              static bool s_pvTransformLightErrorShown;
              if (!std::exchange(s_pvTransformLightErrorShown, true))
                Logger::warn("D3D3Device::Execute: D3DOP_PROCESSVERTICES TRANSORMLIGHT is not implemented");
            }
          }

          break;
        }
        case D3DOP_SPAN: {
          Logger::warn("D3D3Device::Execute: D3DOP_SPAN");

          D3DSPAN* span = reinterpret_cast<D3DSPAN*>(operation);
          DrawSpanInternal(span, count, data.dwVertexCount, vertexBuffer);

          break;
        }
        case D3DOP_STATELIGHT: {
          Logger::debug("D3D3Device::Execute: D3DOP_STATELIGHT");

          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);
          for (uint16_t i = 0; i < count; i++) {
            D3DSTATE& s = state[i];
            SetLightStateInternal(s.dlstLightStateType, s.dwArg[0]);
          }
          break;
        }
        case D3DOP_STATERENDER: {
          Logger::debug("D3D3Device::Execute: D3DOP_STATERENDER");

          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);
          for (uint16_t i = 0; i < count; i++) {
            D3DSTATE& s = state[i];
            SetRenderStateInternal(s.drstRenderStateType, s.dwArg[0]);
          }

          break;
        }
        case D3DOP_STATETRANSFORM: {
          Logger::debug("D3D3Device::Execute: D3DOP_STATETRANSFORM");

          D3DSTATE* state = reinterpret_cast<D3DSTATE*>(operation);
          D3DMATRIX matrix;
          for (uint16_t i = 0; i < count; i++) {
            D3DSTATE& s = state[i];

            HRESULT hr = GetMatrix(s.dwArg[0], &matrix);
            if (unlikely(FAILED(hr)))
              Logger::warn(str::format("D3D3Device::Execute: Failed to retrieve matrix: ", s.dwArg[0]));

            hr = m_d3d9->SetTransform(ConvertTransformState(s.dtstTransformStateType), &matrix);
            if (unlikely(FAILED(hr)))
              Logger::warn("D3D3Device::Execute: Failed to set D3D9 transform");
          }

          break;
        }
        case D3DOP_SETSTATUS: {
          Logger::debug("D3D3Device::Execute: D3DOP_SETSTATUS");

          D3DSTATUS* status = reinterpret_cast<D3DSTATUS*>(operation);
          for (uint16_t i = 0; i < count; i++) {
            data.dsStatus = status[i];
          }

          break;
        }
        case D3DOP_TEXTURELOAD: {
          Logger::debug("D3D3Device::Execute: D3DOP_TEXTURELOAD");

          D3DTEXTURELOAD* textureLoad = reinterpret_cast<D3DTEXTURELOAD*>(operation);
          TextureLoadInternal(textureLoad, count);

          break;
        }
        default:
          Logger::err(str::format("D3D3Device::Execute: Unknown opcode encountered: ", opcode));
          break;
      }

      ptr += instructionSize;
    }

    m_commonIntf->UpdateDrawTracking();

    return D3D_OK;
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
    Logger::debug(">>> D3D3Device::CreateMatrix");

    m_matrixHandle++;
    m_matrices.emplace(std::piecewise_construct,
                       std::forward_as_tuple(m_matrixHandle),
                       std::forward_as_tuple(D3DMATRIX()));

    *matrix = m_matrixHandle;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D3Device::SetMatrix");

    if (unlikely(matrix == nullptr))
      return DDERR_INVALIDPARAMS;

    auto matrixIter = m_matrices.find(handle);

    if (likely(matrixIter != m_matrices.end())) {
      matrixIter->second = *matrix;
    } else {
      Logger::warn("D3D3Device::SetMatrix: Matrix not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D3Device::GetMatrix");

    if (unlikely(matrix == nullptr))
      return DDERR_INVALIDPARAMS;

    auto matrixIter = m_matrices.find(handle);

    if (likely(matrixIter != m_matrices.end())) {
      *matrix = matrixIter->second;
    } else {
      Logger::warn("D3D3Device::GetMatrix: Matrix not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle) {
    Logger::debug(">>> D3D3Device::DeleteMatrix");

    auto matrixIter = m_matrices.find(D3DMatHandle);

    if (likely(matrixIter != m_matrices.end())) {
      m_matrices.erase(matrixIter);
    } else {
      Logger::warn("D3D3Device::DeleteMatrix: Matrix not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
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

  inline HRESULT STDMETHODCALLTYPE D3D3Device::SetLightStateInternal(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    Logger::debug(">>> D3D3Device::SetLightStateInternal");

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL: {
        D3D5Device* device5 = m_commonIntf->GetD3D5Device();

        if (unlikely(!dwLightState)) {
          m_materialHandle = dwLightState;

          if (device5 != nullptr)
            device5->SetCurrentMaterialHandle(dwLightState);

          return D3D_OK;
        }

        Logger::debug(str::format("D3D3Device::SetLightStateInternal: Applying material nr. ", dwLightState, " to D3D9"));

        D3D3Interface* d3d3Intf = m_commonIntf->GetD3D3Interface();
        // consider pure D3D3 device use by default
        if (likely(d3d3Intf != nullptr)) {
          d3d9::D3DMATERIAL9* material9 = d3d3Intf->GetCommonD3DInterface()->GetD3D9MaterialFromHandle(dwLightState);
          if (unlikely(material9 == nullptr))
            return DDERR_INVALIDPARAMS;

          m_materialHandle = dwLightState;
          m_d3d9->SetMaterial(material9);
        // fall back to using a D3D5 device otherwise
        } else if (likely(device5 != nullptr)) {
          d3d9::D3DMATERIAL9* material9 = device5->GetParent()->GetCommonD3DInterface()->GetD3D9MaterialFromHandle(dwLightState);

          device5->SetCurrentMaterialHandle(dwLightState);
          device5->GetD3D9()->SetMaterial(material9);
        } else {
          Logger::warn("D3D3Device::SetLightStateInternal: Unable to set D3D9 material");
        }

        break;
      }
      case D3DLIGHTSTATE_AMBIENT:
        m_d3d9->SetRenderState(d3d9::D3DRS_AMBIENT, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        if (unlikely(dwLightState != D3DCOLOR_RGB))
          Logger::warn("D3D3Device::SetLightStateInternal: Unsupported D3DLIGHTSTATE_COLORMODEL");
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

  inline HRESULT STDMETHODCALLTYPE D3D3Device::SetRenderStateInternal(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    Logger::debug(str::format(">>> D3D3Device::SetRenderStateInternal: ", dwRenderStateType));

    // As opposed to D3D7, D3D3 does not error out on
    // unknown or invalid render states.
    if (unlikely(!IsValidD3D3RenderStateType(dwRenderStateType))) {
      Logger::debug(str::format("D3D3Device::SetRenderStateInternal: Invalid render state ", dwRenderStateType));
      return D3D_OK;
    }

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // Replacement for later implemented SetTexture calls
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        DDrawSurface* surface = nullptr;

        if (likely(dwRenderState != 0)) {
          surface = m_commonIntf->GetDDInterface()->GetSurfaceFromTextureHandle(dwRenderState);
          if (unlikely(surface == nullptr))
            return DDERR_INVALIDPARAMS;
        }

        HRESULT hr = SetTextureInternal(surface, dwRenderState);
        if (unlikely(FAILED(hr)))
          return hr;

        break;
      }

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState))
            Logger::warn("D3D3Device::SetRenderStateInternal: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9 = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        break;
      }

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
          Logger::debug("D3D3Device::SetRenderStateInternal: Disabling of D3DRENDERSTATE_TEXTUREPERSPECTIVE is not supported");

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
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRS_LINEPATTERN");

        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        static bool s_monoEnableErrorShown;

        if (dwRenderState && !std::exchange(s_monoEnableErrorShown, true))
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_MONOENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        static bool s_ROP2ErrorShown;

        if (!std::exchange(s_ROP2ErrorShown, true))
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_ROP2");

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
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_ZVISIBLE");

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
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEENABLE:
        static bool s_stippleEnableErrorShown;

        if (dwRenderState && !std::exchange(s_stippleEnableErrorShown, true))
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_STIPPLEENABLE");

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
          Logger::warn("D3D3Device::SetRenderStateInternal: Unimplemented render state D3DRENDERSTATE_STIPPLEPATTERNXX");

        return D3D_OK;
    }

    // This call will never fail
    return m_d3d9->SetRenderState(State9, dwRenderState);
  }

  inline void D3D3Device::DrawTriangleInternal(D3DTRIANGLE* triangle, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (DWORD i = 0; i < count; i++) {
      const D3DTRIANGLE& t = triangle[i];

      if (t.v1 >= vertexCount || t.v2 >= vertexCount || t.v3 >= vertexCount)
        continue;

      // TODO: Ignoring t.wFlags for now as they are relevant only for wireframe mode? (D3DTRIFLAG_START, D3DTRIFLAG_STARTFLAT(1-29), D3DTRIFLAG_ODD(strip), D3DTRIFLAG_EVEN(fan) and D3DTRIFLAG_EDGEENABLE)

      vertices.push_back(vertexBuffer[t.v1]);
      vertices.push_back(vertexBuffer[t.v2]);
      vertices.push_back(vertexBuffer[t.v3]);
    }

    if (!vertices.empty() && m_d3d9 != nullptr) {
      m_d3d9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = m_d3d9->DrawPrimitiveUP(
           d3d9::D3DPT_TRIANGLELIST,
           GetPrimitiveCount(D3DPT_TRIANGLELIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));
      if (SUCCEEDED(hr)) {
        Logger::debug(str::format("D3D3Device::Execute: D3DOP_TRIANGLE drawn triangles: ", vertices.size()));
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_TRIANGLE failed to draw triangles: ", vertices.size()));
      }
      vertices.clear();
    }
  }

  inline void D3D3Device::DrawLineInternal(D3DLINE* line, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (DWORD i = 0; i < count; i++) {
      const D3DLINE& l = line[i];

      if (l.v1 >= vertexCount || l.v2 >= vertexCount)
        continue;

      vertices.push_back(vertexBuffer[l.v1]);
      vertices.push_back(vertexBuffer[l.v2]);
    }

    if (!vertices.empty() && m_d3d9 != nullptr) {
      m_d3d9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = m_d3d9->DrawPrimitiveUP(
           d3d9::D3DPT_LINELIST,
           GetPrimitiveCount(D3DPT_LINELIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));
      if (SUCCEEDED(hr)) {
        Logger::debug(str::format("D3D3Device::Execute: D3DOP_LINE drawn triangles: ", vertices.size()));
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_LINE failed to draw triangles: ", vertices.size()));
      }
      vertices.clear();
    }
  }

  inline void D3D3Device::DrawPointInternal(D3DPOINT* point, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (DWORD i = 0; i < count; i++) {
      const D3DPOINT& p = point[i];

      if (p.wFirst >= vertexCount)
        continue;

      for (DWORD x = 0; x < std::min(static_cast<DWORD>(p.wCount), vertexCount - p.wFirst); x++) {
        vertices.push_back(vertexBuffer[p.wFirst + x]);
      }
    }

    if (!vertices.empty() && m_d3d9 != nullptr) {
      m_d3d9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = m_d3d9->DrawPrimitiveUP(
           d3d9::D3DPT_POINTLIST,
           GetPrimitiveCount(D3DPT_POINTLIST, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));
      if (SUCCEEDED(hr)) {
        Logger::debug(str::format("D3D3Device::Execute: D3DOP_POINT drawn triangles: ", vertices.size()));
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_POINT failed to draw triangles: ", vertices.size()));
      }
      vertices.clear();
    }
  }

  inline void D3D3Device::DrawSpanInternal(D3DSPAN* span, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer) {
    std::vector<D3DTLVERTEX> vertices;

    for (DWORD i = 0; i < count; i++) {
      const D3DSPAN& s = span[i];

      if (s.wFirst >= vertexCount)
        continue;

      for (DWORD x = 0; x < std::min(static_cast<DWORD>(s.wCount), vertexCount - s.wFirst); x++) {
        vertices.push_back(vertexBuffer[s.wFirst + x]);
      }
    }

    if (!vertices.empty() && m_d3d9 != nullptr) {
      m_d3d9->SetFVF(D3DFVF_TLVERTEX);
      HRESULT hr = m_d3d9->DrawPrimitiveUP(
           d3d9::D3DPT_LINESTRIP,
           GetPrimitiveCount(D3DPT_LINESTRIP, vertices.size()),
           vertices.data(),
           GetFVFSize(D3DFVF_TLVERTEX));
      if (SUCCEEDED(hr)) {
        Logger::debug(str::format("D3D3Device::Execute: D3DOP_SPAN drawn triangles: ", vertices.size()));
      } else {
        Logger::err(str::format("D3D3Device::Execute: D3DOP_SPAN failed to draw triangles: ", vertices.size()));
      }
      vertices.clear();
    }
  }

  inline void D3D3Device::TextureLoadInternal(D3DTEXTURELOAD* textureLoad, DWORD count) {
    for (DWORD i = 0; i < count; i++) {
      D3DTEXTURELOAD& tl = textureLoad[i];

      DDrawSurface* destSurf = m_commonIntf->GetDDInterface()->GetSurfaceFromTextureHandle(tl.hDestTexture);
      DDrawSurface* srcSurf = m_commonIntf->GetDDInterface()->GetSurfaceFromTextureHandle(tl.hSrcTexture);
      if (destSurf != nullptr && srcSurf != nullptr)
        destSurf->GetD3D3Texture()->Load(srcSurf->GetD3D3Texture());
      else
        Logger::warn("D3D3Device::Execute: D3DOP_TEXTURELOAD source or/and destination texture is null");
    }
  }

  inline HRESULT D3D3Device::SetTextureInternal(DDrawSurface* surface, DWORD textureHandle) {
    Logger::debug(">>> D3D3Device::SetTextureInternal");

    HRESULT hr;

    // Unbinding texture stages
    if (surface == nullptr) {
      Logger::debug("D3D3Device::SetTextureInternal: Unbiding D3D9 texture");

      hr = m_d3d9->SetTexture(0, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_textureHandle != 0) {
          Logger::debug("D3D3Device::SetTextureInternal: Unbinding local texture");
          m_textureHandle = 0;
        }
      } else {
        Logger::err("D3D3Device::SetTextureInternal: Failed to unbind D3D9 texture");
      }

      return hr;
    }

    Logger::debug("D3D3Device::SetTextureInternal: Binding D3D9 texture");

    // Only upload textures if any sort of blit/lock operation
    // has been performed on them since the last SetTexture call
    if (surface->GetCommonSurface()->HasDirtyMipMaps()) {
      hr = surface->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D3Device::SetTextureInternal: Failed to initialize/upload D3D9 texture");
        return hr;
      }

      surface->GetCommonSurface()->UnDirtyMipMaps();
    } else {
      Logger::debug("D3D3Device::SetTextureInternal: Skipping upload of texture and mip maps");
    }

    // Only fast skip on D3D9 side, since we want to ensure
    // color keying is applied properly even in the case
    // of the same texture being set again (color key may change)
    //if (unlikely(m_textureHandle == textureHandle))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = surface->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = m_d3d9->SetTexture(0, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D3Device::SetTextureInternal: Failed to bind D3D9 texture");
        return hr;
      }

      // "Any alpha values in the texture replace the alpha values in the colors that would
      //  have been used with no texturing; if the texture does not contain an alpha component,
      //  alpha values at the vertices in the source are interpolated between vertices."
      if (m_textureMapBlend == D3DTBLEND_MODULATE) {
        const DWORD textureOp = surface->GetCommonSurface()->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE;
        m_d3d9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
      }

      // D3D3 enables color key transparency globally
      const bool validColorKey = surface->GetCommonSurface()->HasValidColorKey();
      m_bridge->SetColorKeyState(validColorKey);
      if (validColorKey) {
        Logger::debug("D3D3Device::SetTextureInternal: Enabling color key transparency");
        DDCOLORKEY normalizedColorKey = surface->GetCommonSurface()->GetColorKeyNormalized();
        m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                              normalizedColorKey.dwColorSpaceHighValue);
      }
    }

    m_textureHandle = textureHandle;

    return D3D_OK;
  }

}
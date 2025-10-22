#include "d3d7_device.h"

#include "d3d7_interface.h"
#include "d3d7_buffer.h"
#include "d3d7_state_block.h"
#include "d3d7_util.h"
#include "ddraw7_surface.h"

namespace dxvk {

  D3D7Device::D3D7Device(
      Com<IDirect3DDevice7>&& d3d7DeviceProxy,
      D3D7Interface* pParent,
      DDraw7Interface* pDD7Parent,
      Com<d3d9::IDirect3DDevice9>&& pDevice,
      DDraw7Surface* pSurface)
    : DDrawWrappedObject<d3d9::IDirect3DDevice9, IDirect3DDevice7>(std::move(pDevice), std::move(d3d7DeviceProxy))
    , m_parent( pParent )
    , m_DD7Parent ( pDD7Parent )
    // Always enforce multi-threaded protection on a D3D7 device
    , m_singlethread( true )
    , m_rt ( pSurface ) {
    // Get the bridge interface to D3D9
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_rtOrig = m_rt.ptr();
    HRESULT hr = m_d3d9->GetRenderTarget(0, &m_rt9);
    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device: Failed to get d3d9 RT");

    hr = m_d3d9->GetDepthStencilSurface(&m_ds9);
    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device: Failed to get d3d9 depth stencil");

    // Textures
    m_textures.fill(nullptr);
  }

  D3D7Device::~D3D7Device() {
    // Clear up the interface device pointer if it points to this device
    if (m_parent->GetDevice() == this)
      m_parent->ClearDevice();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetCaps(D3DDEVICEDESC7 *desc) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetCaps");

    if (unlikely(desc == nullptr))
      return DDERR_INVALIDPARAMS;

    *desc = m_parent->GetDesc();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    Logger::debug("<<< D3D7Device::EnumTextureFormats: Proxy");
    return m_proxy->EnumTextureFormats(cb, ctx);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::BeginScene() {
    Logger::debug(">>> D3D7Device::BeginScene");
    return m_d3d9->BeginScene();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EndScene() {
    Logger::debug(">>> D3D7Device::EndScene");
    return m_d3d9->EndScene();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetDirect3D(IDirect3D7 **d3d) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetDirect3D");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetRenderTarget");

    if (surface == nullptr) {
      Logger::err("D3D7Device::SetRenderTarget: NULL render target");
      return DDERR_INVALIDPARAMS;
    }

    if (likely(m_DD7Parent->IsWrappedSurface(surface))) {
      DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

      // Will always be needed at this point
      HRESULT hr = surface7->InitializeOrUploadD3D9();

      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetRenderTarget: Failed to initialize/upload d3d9 RT");
        return hr;
      }

      if (surface7 == m_rtOrig) {
        hr = m_d3d9->SetRenderTarget(0, m_rt9.ptr());
        m_d3d9->SetDepthStencilSurface(m_ds9.ptr());
      } else {
        hr = m_d3d9->SetRenderTarget(0, m_rt->GetSurface());
        // TODO: Get the depth stencil from the RT as well and set it?
        // Not entirely sure how depth stencil use is set up in D3D7
      }

      if (likely(SUCCEEDED(hr))) {
        m_rt = surface7;
      } else {
        Logger::err("D3D7Device::SetRenderTarget: Failed to set RT");
        return hr;
      }
    } else {
      Logger::warn("D3D7Device::SetRenderTarget: Received an unwrapped RT");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetRenderTarget(IDirectDrawSurface7 **surface) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetRenderTarget");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::Clear(DWORD count, D3DRECT *rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil) {
    Logger::debug(">>> D3D7Device::Clear");
    return m_d3d9->Clear(count, rects, flags, color, z, stencil);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D7Device::SetTransform");
    return m_d3d9->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D7Device::GetTransform");
    return m_d3d9->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    Logger::debug(">>> D3D7Device::MultiplyTransform");
    return m_d3d9->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetViewport(D3DVIEWPORT7 *data) {
    Logger::debug(">>> D3D7Device::SetViewport");
    return m_d3d9->SetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetViewport(D3DVIEWPORT7 *data) {
    Logger::debug(">>> D3D7Device::GetViewport");
    return m_d3d9->GetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetMaterial(D3DMATERIAL7 *data) {
    Logger::debug(">>> D3D7Device::SetMaterial");
    return m_d3d9->SetMaterial(reinterpret_cast<d3d9::D3DMATERIAL9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetMaterial(D3DMATERIAL7 *data) {
    Logger::debug(">>> D3D7Device::GetMaterial");
    return m_d3d9->GetMaterial(reinterpret_cast<d3d9::D3DMATERIAL9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetLight(DWORD idx, D3DLIGHT7 *data) {
    Logger::debug(">>> D3D7Device::SetLight");
    return m_d3d9->SetLight(idx, reinterpret_cast<d3d9::D3DLIGHT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetLight(DWORD idx, D3DLIGHT7 *data) {
    Logger::debug(">>> D3D7Device::GetLight");
    return m_d3d9->GetLight(idx, reinterpret_cast<d3d9::D3DLIGHT9*>(data));
  }

  // ZBIAS can be an integer from 0 to 16 and needs to be remapped to float
  static constexpr float ZBIAS_SCALE     = -1.0 / (1 << 16); // Consider D16 precision
  static constexpr float ZBIAS_SCALE_INV = 1 / ZBIAS_SCALE;

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D7Device::SetRenderState: ", dwRenderStateType));

    d3d9::D3DRENDERSTATETYPE State9 = (d3d9::D3DRENDERSTATETYPE)dwRenderStateType;

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // TODO: How the heck will this ever work in d3d9?
      case D3DRENDERSTATE_ANTIALIAS:
        static bool s_antialiasErrorShown;

        if (!std::exchange(s_antialiasErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_ANTIALIAS");

        return D3D_OK;

      // TODO: The heck is this all about?
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        static bool s_texturePerspectiveErrorShown;

        if (!std::exchange(s_texturePerspectiveErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_TEXTUREPERSPECTIVE");

        return D3D_OK;

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_linePattern = bit::cast<D3DLINEPATTERN>(dwRenderState);
        return D3D_OK;

      // Not supported by D3D7 either, but its value is stored.
      case D3DRENDERSTATE_ZVISIBLE:
        m_zVisible = dwRenderState;
        return D3D_OK;

      // TODO: Oh I'll stipple your alpha alright...
      case D3DRENDERSTATE_STIPPLEDALPHA:
        static bool s_stippledAlphaErrorShown;

        if (!std::exchange(s_stippledAlphaErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO: Implement D3DRS_ANTIALIASEDLINEENABLE in D9VK.
      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO: Ugh... probably important
      case D3DRENDERSTATE_COLORKEYENABLE:
        static bool s_colorKeyEnableErrorShown;

        if (!std::exchange(s_colorKeyEnableErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_COLORKEYENABLE");

        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ZBIAS_SCALE);
        break;
    }

    // This call will never fail
    return m_d3d9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D7Device::GetRenderState: ", dwRenderStateType));

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DRENDERSTATETYPE State9 = (d3d9::D3DRENDERSTATETYPE)dwRenderStateType;

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // TODO: How the heck will this ever work in d3d9?
      case D3DRENDERSTATE_ANTIALIAS:
        static bool s_antialiasErrorShown;

        if (!std::exchange(s_antialiasErrorShown, true))
          Logger::warn("D3D7Device::GetRenderState: Unimplemented render state D3DRENDERSTATE_ANTIALIAS");

        *lpdwRenderState = D3DANTIALIAS_NONE;
        return D3D_OK;

      // TODO: The heck is this all about?
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        static bool s_texturePerspectiveErrorShown;

        if (!std::exchange(s_texturePerspectiveErrorShown, true))
          Logger::warn("D3D7Device::GetRenderState: Unimplemented render state D3DRENDERSTATE_TEXTUREPERSPECTIVE");

        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_linePattern);
        return D3D_OK;

      // Not supported by D3D7 either, but its value is stored.
      case D3DRENDERSTATE_ZVISIBLE:
        *lpdwRenderState = m_zVisible;
        return D3D_OK;

      // TODO: Oh I'll stipple your alpha alright...
      case D3DRENDERSTATE_STIPPLEDALPHA:
        static bool s_stippledAlphaErrorShown;

        if (!std::exchange(s_stippledAlphaErrorShown, true))
          Logger::warn("D3D7Device::GetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO: Ugh... probably important
      case D3DRENDERSTATE_COLORKEYENABLE:
        static bool s_colorKeyEnableErrorShown;

        if (!std::exchange(s_colorKeyEnableErrorShown, true))
          Logger::warn("D3D7Device::GetRenderState: Unimplemented render state D3DRENDERSTATE_COLORKEYENABLE");

        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias  = 0;
        HRESULT res = m_d3d9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ZBIAS_SCALE_INV);
        return res;
      } break;
    }

    // This call will never fail
    return m_d3d9->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::BeginStateBlock() {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::BeginStateBlock");

    if (unlikely(m_recorder != nullptr))
      return D3DERR_INBEGINSTATEBLOCK;

    HRESULT hr = m_d3d9->BeginStateBlock();

    if (likely(SUCCEEDED(hr))) {
      m_handle++;
      auto stateBlockIterPair = m_stateBlocks.emplace(std::piecewise_construct,
                                                      std::forward_as_tuple(m_handle),
                                                      std::forward_as_tuple(this));
      m_recorder = &stateBlockIterPair.first->second;
      m_recorderHandle = m_handle;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EndStateBlock(LPDWORD lpdwBlockHandle) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::EndStateBlock");

    if (unlikely(lpdwBlockHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_recorder == nullptr))
      return D3DERR_NOTINBEGINSTATEBLOCK;

    Com<d3d9::IDirect3DStateBlock9> pStateBlock;
    HRESULT hr = m_d3d9->EndStateBlock(&pStateBlock);

    if (likely(SUCCEEDED(hr))) {
      m_recorder->SetD3D9(std::move(pStateBlock));

      *lpdwBlockHandle = m_recorderHandle;

      m_recorder = nullptr;
      m_recorderHandle = 0;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ApplyStateBlock(DWORD dwBlockHandle) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::ApplyStateBlock");

    // Applications cannot apply a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::ApplyStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    return stateBlockIter->second.Apply();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::CaptureStateBlock(DWORD dwBlockHandle) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::CaptureStateBlock");

    // Applications cannot capture a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::CaptureStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    return stateBlockIter->second.Capture();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DeleteStateBlock(DWORD dwBlockHandle) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DeleteStateBlock");

    // Applications cannot delete a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::DeleteStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    m_stateBlocks.erase(stateBlockIter);

    // Native apparently does drop the handle counter in
    // situations where the handle being removed is the
    // last allocated handle, which allows some reuse
    if (m_handle == dwBlockHandle)
      m_handle--;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbType, LPDWORD lpdwBlockHandle) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::CreateStateBlock");

    if (unlikely(lpdwBlockHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    // Applications cannot create a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    D3D7StateBlockType stateBlockType = ConvertStateBlockType(d3dsbType);

    if (unlikely(stateBlockType == D3D7StateBlockType::Unknown)) {
      Logger::warn(str::format("D3D7Device::CreateStateBlock: Invalid state block type: ", d3dsbType));
      return DDERR_INVALIDPARAMS;
    }

    Com<d3d9::IDirect3DStateBlock9> pStateBlock9;
    HRESULT res = m_d3d9->CreateStateBlock(d3d9::D3DSTATEBLOCKTYPE(d3dsbType), &pStateBlock9);

    if (likely(SUCCEEDED(res))) {
      m_handle++;
      m_stateBlocks.emplace(std::piecewise_construct,
                            std::forward_as_tuple(m_handle),
                            std::forward_as_tuple(this, stateBlockType, pStateBlock9.ptr()));
      *lpdwBlockHandle = m_handle;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::PreLoad(IDirectDrawSurface7 *surface) {
    Logger::debug("<<< D3D7Device::PreLoad: Proxy");

    if (likely(m_DD7Parent->IsWrappedSurface(surface))) {
      DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);
      // This won't return anything
      surface7->GetSurface()->PreLoad();
      return D3D_OK;
    } else {
      Logger::warn("D3D7Device::PreLoad: Pre-loading a non-wrapped surface");
      return m_proxy->PreLoad(surface);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitive(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawPrimitive");

    // TODO: Many other validations to be done on primitive type, vertex type etc.
    if (unlikely(lpvVertices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!dwVertexCount))
      return D3D_OK;

    GetD3D9()->SetFVF(dwVertexTypeDesc);
    return GetD3D9()->DrawPrimitiveUP(
                          d3d9::D3DPRIMITIVETYPE(dptPrimitiveType),
                          GetPrimitiveCount(dptPrimitiveType, dwVertexCount),
                          lpvVertices,
                          GetFVFSize(dwVertexTypeDesc));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawIndexedPrimitive");

    // TODO: Many other validations to be done on primitive type, vertex type etc.
    if (unlikely(lpvVertices == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!dwVertexCount || !dwIndexCount))
      return D3D_OK;

    m_d3d9->SetFVF(dwVertexTypeDesc);
    return m_d3d9->DrawIndexedPrimitiveUP(
                          d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                          0,
                          dwVertexCount,
                          GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount),
                          lpwIndices,
                          d3d9::D3DFMT_INDEX16,
                          lpvVertices,
                          GetFVFSize(dwVertexTypeDesc));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DCLIPSTATUS9 clipStatus9;
    // TODO: Split the union and intersection flags
    clipStatus9.ClipUnion        = clip_status->dwStatus;
    clipStatus9.ClipIntersection = clip_status->dwStatus;

    return m_d3d9->SetClipStatus(&clipStatus9);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetClipStatus");

    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DCLIPSTATUS9 clipStatus9;
    HRESULT hr = m_d3d9->GetClipStatus(&clipStatus9);

    if (FAILED(hr))
      return hr;

    D3DCLIPSTATUS clipStatus7 = { };
    clipStatus7.dwFlags  = D3DCLIPSTATUS_STATUS;
    clipStatus7.dwStatus = D3DSTATUS_DEFAULT | clipStatus9.ClipUnion | clipStatus9.ClipIntersection;

    *clip_status = clipStatus7;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *pStridedData, DWORD stridedDataCount, DWORD flags) {
    Logger::warn("!!! D3D7Device::DrawPrimitiveStrided: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD  dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD  dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    Logger::warn("!!! D3D7Device::DrawIndexedPrimitiveStrided: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawPrimitiveVB");

    // TODO: Many other validations to be done on primitive type, vertex type etc.
    if (unlikely(lpd3dVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!dwNumVertices))
      return D3D_OK;

    D3D7VertexBuffer* vb = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb->IsLocked())) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    m_d3d9->SetFVF(vb->GetFVF());
    m_d3d9->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    return m_d3d9->DrawPrimitive(
                       d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                       dwStartVertex,
                       GetPrimitiveCount(d3dptPrimitiveType, dwNumVertices));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawIndexedPrimitiveVB");

    // TODO: Many other validations to be done on primitive type, vertex type etc.
    if (unlikely(lpd3dVertexBuffer == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!dwNumVertices || !dwIndexCount))
      return D3D_OK;

    D3D7VertexBuffer* vb = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb->IsLocked())) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    vb->UploadIndices(lpwIndices, dwIndexCount);
    HRESULT hr = m_d3d9->SetIndices(vb->GetIndexBuffer());
    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed to set d3d9 indices");

    hr = m_d3d9->SetFVF(vb->GetFVF());
    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed to set d3d9 FVF");

    hr = m_d3d9->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed to set d3d9 stream source");

    hr = m_d3d9->DrawIndexedPrimitive(
                  d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                  0,
                  0,
                  dwNumVertices,
                  dwStartVertex,
                  GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount));

    if(unlikely(FAILED(hr)))
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed d3d9 call to DrawIndexedPrimitive");

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ComputeSphereVisibility(D3DVECTOR *centers, D3DVALUE *radii, DWORD sphere_count, DWORD sphereCount, DWORD *visibility) {
    Logger::warn("!!! D3D7Device::ComputeSphereVisibility: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTexture(DWORD stage, IDirectDrawSurface7 **surface) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetTexture");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stage >= caps7::TextureStageCount)) {
      Logger::err(str::format("D3D7Device::GetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    *surface = m_textures[stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTexture(DWORD stage, IDirectDrawSurface7 *surface) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetTexture");

    if (unlikely(stage >= caps7::TextureStageCount)) {
      Logger::err(str::format("D3D7Device::SetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(ShouldRecord()))
      return m_recorder->SetTexture(stage, surface);

    HRESULT hr;

    // Unbinding texture stages
    if (surface == nullptr) {
      Logger::debug("D3D7Device::SetTexture: Unbiding d3d9 texture");

      hr = m_d3d9->SetTexture(stage, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_textures[stage] != nullptr) {
          m_textures[stage]->MarkAsUnbound();
          m_textures[stage] = nullptr;
        }
      } else {
        Logger::err("D3D7Device::SetTexture: Failed to unbind d3d9 texture");
      }

      return hr;
    }

    // Binding texture stages
    if (likely(m_DD7Parent->IsWrappedSurface(surface))) {
      Logger::debug("D3D7Device::SetTexture: Binding d3d9 texture");

      DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

      if (unlikely(m_textures[stage] == surface7))
        return D3D_OK;

      // Will always be needed at this point
      hr = surface7->InitializeOrUploadD3D9();

      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetTexture: Failed to initialize/upload d3d9 texture");
        return hr;
      }

      hr = m_d3d9->SetTexture(stage, surface7->GetTexture());
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D7Device::SetTexture: Failed to bind d3d9 texture");
        return hr;
      }

      m_textures[stage] = surface7;
      m_textures[stage]->MarkAsBound();
    } else {
      Logger::warn("D3D7Device::SetTexture: Received a non-wrapped texture");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetTextureStageState");

    if (unlikely(lpdwState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    if (stateType != -1u) {
      // If the type has been remapped to a sampler state type
      return GetD3D9()->GetSamplerState(dwStage, stateType, lpdwState);
    }
    else {
      return GetD3D9()->GetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), lpdwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetTextureStageState");

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    if (stateType != -1u) {
      // If the type has been remapped to a sampler state type
      return m_d3d9->SetSamplerState(dwStage, stateType, dwState);
    } else {
      return m_d3d9->SetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), dwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ValidateDevice(LPDWORD lpdwPasses) {
    Logger::debug(">>> D3D7Device::ValidateDevice");
    return m_d3d9->ValidateDevice(lpdwPasses);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags) {
    Logger::debug("<<< D3D7Device::Load: Proxy");

    if (dst_surface == nullptr || src_surface == nullptr) {
      Logger::warn("D3D7Device::Load: null source or destination");
      return DDERR_INVALIDPARAMS;
    }

    IDirectDrawSurface7* loadSource      = src_surface;
    IDirectDrawSurface7* loadDestination = dst_surface;

    if (likely(m_DD7Parent->IsWrappedSurface(src_surface))) {
      DDraw7Surface* ddraw7SurfaceSrc = static_cast<DDraw7Surface*>(src_surface);
      loadSource = ddraw7SurfaceSrc->GetProxied();
    } else {
      Logger::debug("D3D7Device::Load: Non-wrapped surface source");
      loadSource = src_surface;
    }

    if (likely(m_DD7Parent->IsWrappedSurface(dst_surface))) {
      DDraw7Surface* ddraw7SurfaceDst = static_cast<DDraw7Surface*>(dst_surface);
      loadDestination = ddraw7SurfaceDst->GetProxied();
    } else {
      Logger::debug("D3D7Device::Load: Non-wrapped surface destination");
      loadDestination = dst_surface;
    }

    // We need to copy to/from the proxied surfaces first, before we forward the actual call
    HRESULT hr = m_proxy->Load(loadDestination, dst_point, loadSource, src_rect, flags);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Device::Load: Failed to load surfaces");
    }

    if (likely(m_DD7Parent->IsWrappedSurface(src_surface))) {
      DDraw7Surface* ddraw7SurfaceSrc = static_cast<DDraw7Surface*>(src_surface);

      HRESULT hrInitSrc = ddraw7SurfaceSrc->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hrInitSrc))) {
        Logger::warn("D3D7Device::Load: Failed to upload d3d9 source surface data");
      }
    }

    if (likely(m_DD7Parent->IsWrappedSurface(dst_surface))) {
      DDraw7Surface* ddraw7SurfaceDst = static_cast<DDraw7Surface*>(dst_surface);

      HRESULT hrInitDst = ddraw7SurfaceDst->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hrInitDst))) {
        Logger::warn("D3D7Device::Load: Failed to upload d3d9 destination surface data");
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::LightEnable(DWORD dwLightIndex, BOOL bEnable) {
    Logger::debug(">>> D3D7Device::LightEnable");
    return m_d3d9->LightEnable(dwLightIndex, bEnable);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetLightEnable(DWORD dwLightIndex, BOOL *pbEnable) {
    Logger::debug(">>> D3D7Device::GetLightEnable");
    return m_d3d9->GetLightEnable(dwLightIndex, pbEnable);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation) {
    Logger::debug(">>> D3D7Device::SetClipPlane");
    return m_d3d9->SetClipPlane(dwIndex, pPlaneEquation);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation) {
    Logger::debug(">>> D3D7Device::GetClipPlane");
    return m_d3d9->GetClipPlane(dwIndex, pPlaneEquation);
  }

  // Docs state: "This method returns S_FALSE on retail builds of DirectX."
  HRESULT STDMETHODCALLTYPE D3D7Device::GetInfo(DWORD info_id, void *info, DWORD info_size) {
    Logger::debug(">>> D3D7Device::GetInfo");
    return S_FALSE;
  }

}
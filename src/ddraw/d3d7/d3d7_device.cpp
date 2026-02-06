#include "d3d7_device.h"

#include "d3d7_buffer.h"
#include "d3d7_state_block.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  uint32_t D3D7Device::s_deviceCount = 0;

  // Index buffer sizes of XS, S, M, L and XL, corresponding to 0.5 kb, 2 kb, 8 kb, 32 kb and 128 kb
  static constexpr UINT IndexCount[ddrawCaps::IndexBufferCount] = {256, 1024, 4096, 16384, D3DMAXNUMVERTICES};

  D3D7Device::D3D7Device(
      Com<IDirect3DDevice7>&& d3d7DeviceProxy,
      D3D7Interface* pParent,
      D3DDEVICEDESC7 Desc,
      d3d9::D3DPRESENT_PARAMETERS Params9,
      Com<d3d9::IDirect3DDevice9>&& pDevice9,
      DDraw7Surface* pSurface,
      DWORD CreationFlags9)
    : DDrawWrappedObject<D3D7Interface, IDirect3DDevice7, d3d9::IDirect3DDevice9>(pParent, std::move(d3d7DeviceProxy), std::move(pDevice9))
    , m_isMixedHWVP ( CreationFlags9 & D3DCREATE_MIXED_VERTEXPROCESSING )
    , m_commonIntf ( pParent->GetParent()->GetCommonInterface() )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_params9 ( Params9 )
    , m_desc ( Desc )
    , m_rt ( pSurface ) {
    // Get the bridge interface to D3D9
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8Bridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_rtOrig = m_rt.ptr();

    HRESULT hr = EnumerateBackBuffers(m_rt->GetProxied());
    if(unlikely(FAILED(hr)))
      throw DxvkError("D3D7Device: ERROR! Failed to retrieve D3D9 back buffers!");

    // Textures
    m_textures.fill(nullptr);
    // Common D3D9 index buffers
    m_ib9.fill(nullptr);

    if (unlikely(m_parent->GetOptions()->emulateFSAA == FSAAEmulation::Forced)) {
      Logger::warn("D3D7Device: Force enabling AA");
      m_d3d9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    }

    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D7Device: Created a new device nr. ((7-", m_deviceCount, "))"));
  }

  D3D7Device::~D3D7Device() {
    // If at least the smallest index buffer saw any use, then print the stats
    if (m_ib9_uploads[0] > 0) {
      Logger::info("D3D7Device: Common index buffer upload statistics:");
      Logger::info(str::format("   XS: ", m_ib9_uploads[0]));
      Logger::info(str::format("   S : ", m_ib9_uploads[1]));
      Logger::info(str::format("   M : ", m_ib9_uploads[2]));
      Logger::info(str::format("   L : ", m_ib9_uploads[3]));
      Logger::info(str::format("   XL: ", m_ib9_uploads[4]));
    }

    // Clear the common interface device pointer if it points to this device
    if (m_commonIntf->GetD3D7Device() == this)
      m_commonIntf->SetD3D7Device(nullptr);

    Logger::debug(str::format("D3D7Device: Device nr. ((7-", m_deviceCount, ")) bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D7Interface, IDirect3DDevice7, d3d9::IDirect3DDevice9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DDevice7))
      return this;

    throw DxvkError("D3D7Device::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D7Device::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // These aren't supported, but some games query for them anyway
    if (unlikely(riid == __uuidof(IDirect3DDevice)
              || riid == __uuidof(IDirect3DDevice2)
              || riid == __uuidof(IDirect3DDevice3))) {
      Logger::debug("D3D7Device::QueryInterface: Query for legacy IDirect3DDevice");
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

  HRESULT STDMETHODCALLTYPE D3D7Device::GetCaps(D3DDEVICEDESC7 *desc) {
    Logger::debug(">>> D3D7Device::GetCaps");

    if (unlikely(desc == nullptr))
      return DDERR_INVALIDPARAMS;

    *desc = m_desc;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    Logger::debug(">>> D3D7Device::EnumTextureFormats");

    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    // Note: The list of formats exposed in D3D7 is restricted to the below

    DDPIXELFORMAT textureFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D7
    textureFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // Not supported in D3D9, but some games need
    // it to be advertised (for offscreen plain surfaces?)
    textureFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    // Not supported in D3D9, but some games may use it
    // Note: Advertizing P8 support breaks Sacrifice
    /*textureFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;*/

    textureFormat = GetTextureFormat(d3d9::D3DFMT_V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_L6V5U5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8L8V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT1);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT2);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT3);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::BeginScene() {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::BeginScene");

    RefreshLastUsedDevice();

    if (unlikely(m_inScene))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_d3d9->BeginScene();

    if (likely(SUCCEEDED(hr)))
      m_inScene = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EndScene() {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::EndScene");

    RefreshLastUsedDevice();

    if (unlikely(!m_inScene))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_d3d9->EndScene();

    if (likely(SUCCEEDED(hr))) {
      if (m_parent->GetOptions()->forceProxiedPresent) {
        // If we have drawn anything, we need to make sure we blit back
        // the results onto the D3D7 render target before we flip it
        if (m_hasDrawn) {
          if (unlikely(!m_rt->IsInitialized()))
            m_rt->InitializeD3D9RenderTarget();
          BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(m_rt->GetProxied(), m_rt->GetD3D9());
        }
        m_rt->GetProxied()->Flip(m_flipRTFlags.surf, m_flipRTFlags.flags);

        if (likely(m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Strict))
          m_hasDrawn = false;
      }

      m_inScene = false;
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetDirect3D(IDirect3D7 **d3d) {
    Logger::debug(">>> D3D7Device::GetDirect3D");

    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetRenderTarget");

    if (unlikely(surface == nullptr)) {
      Logger::err("D3D7Device::SetRenderTarget: NULL render target");
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_GENERIC;
    }

    DDraw7Surface* rt7 = static_cast<DDraw7Surface*>(surface);

    // We could technically allow unwrapped RTs when forcing proxied present,
    // however that doesn't get us anything, so simply don't bother with it
    if (unlikely(m_parent->GetOptions()->forceProxiedPresent)) {
      HRESULT hrRT7 = m_proxy->SetRenderTarget(rt7->GetProxied(), flags);
      if (unlikely(FAILED(hrRT7))) {
        Logger::warn("D3D7Device::SetRenderTarget: Failed to set RT");
        return hrRT7;
      }
    }

    // A render target surface needs to have the DDSCAPS_3DDEVICE cap
    if (unlikely(!rt7->GetCommonSurface()->Is3DSurface())) {
      Logger::err("D3D7Device::SetRenderTarget: Surface is missing DDSCAPS_3DDEVICE");
      return DDERR_INVALIDCAPS;
    }

    HRESULT hr = rt7->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    hr = m_d3d9->SetRenderTarget(0, rt7->GetD3D9());

    if (likely(SUCCEEDED(hr))) {
      Logger::debug("D3D7Device::SetRenderTarget: Set a new D3D9 RT");

      m_rt = rt7;
      m_ds = m_rt->GetAttachedDepthStencil();

      HRESULT hrDS;

      if (m_ds != nullptr) {
        Logger::debug("D3D7Device::SetRenderTarget: Found an attached DS");

        hrDS = m_ds->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D7Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
          return hrDS;
        }

        hrDS = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D7Device::SetRenderTarget: Failed to set D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D7Device::SetRenderTarget: Set a new D3D9 DS");
      } else {
        Logger::debug("D3D7Device::SetRenderTarget: RT has no depth stencil attached");

        hrDS = m_d3d9->SetDepthStencilSurface(nullptr);
        if (unlikely(FAILED(hrDS))) {
          Logger::err("D3D7Device::SetRenderTarget: Failed to clear the D3D9 DS");
          return hrDS;
        }

        Logger::debug("D3D7Device::SetRenderTarget: Cleared the D3D9 DS");
      }
    } else {
      Logger::err("D3D7Device::SetRenderTarget: Failed to set D3D9 RT");
      return hr;
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
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::Clear");

    // We are now allowing proxy back buffer blits in certain cases, so
    // we must also ensure the back buffer clear calls are proxied
    HRESULT hr = m_proxy->Clear(count, rects, flags, color, z, stencil);
    if (unlikely(FAILED(hr)))
      Logger::debug("D3D7Device::Clear: Failed proxied clear call");

    // Fast skip
    if (unlikely(!count && rects))
      return D3D_OK;

    return m_d3d9->Clear(count, rects, flags, color, static_cast<float>(z), stencil);
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
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetViewport");

    if (unlikely(data == nullptr))
      return E_INVALIDARG;

    // Clear() calls are affected by the set viewport, so we
    // must ensure SetViewport() calls are also proxied
    HRESULT hr = m_proxy->SetViewport(data);
    // TODO: Implement RT size viewport validations and return E_INVALIDARG on error
    if (unlikely(FAILED(hr))) {
      Logger::debug("D3D7Device::SetViewport: Failed proxied set viewport call");
      return hr;
    }

    return m_d3d9->SetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetViewport(D3DVIEWPORT7 *data) {
    D3D7DeviceLock lock = LockDevice();

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

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D7Device::SetRenderState: ", dwRenderStateType));

    // As opposed to D3D8/9, D3D7 actually validates and
    // errors out in case of unknown/invalid render states
    if (unlikely(!IsValidD3D7RenderStateType(dwRenderStateType)))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      case D3DRENDERSTATE_ANTIALIAS: {
        if (likely(m_parent->GetOptions()->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                    || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT))
            Logger::warn("D3D7Device::SetRenderState: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9        = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        m_antialias   = dwRenderState;
        dwRenderState = m_antialias == D3DANTIALIAS_SORTDEPENDENT
                     || m_antialias == D3DANTIALIAS_SORTINDEPENDENT
                     || m_parent->GetOptions()->emulateFSAA == FSAAEmulation::Forced ? TRUE : FALSE;
        break;
      }

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D7 docs state: "Note that many 3-D adapters apply texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        static bool s_texturePerspectiveErrorShown;

        if (!dwRenderState && !std::exchange(s_texturePerspectiveErrorShown, true))
          Logger::debug("D3D7Device::SetRenderState: Disabling of D3DRENDERSTATE_TEXTUREPERSPECTIVE is not supported");

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

      // TODO:
      case D3DRENDERSTATE_STIPPLEDALPHA:
        static bool s_stippledAlphaErrorShown;

        if (dwRenderState && !std::exchange(s_stippledAlphaErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_STIPPLEDALPHA");

        return D3D_OK;

      // TODO: Implement D3DRS_ANTIALIASEDLINEENABLE in D9VK.
      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO:
      case D3DRENDERSTATE_COLORKEYENABLE:
        static bool s_colorKeyEnableErrorShown;

        if (dwRenderState && !std::exchange(s_colorKeyEnableErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_COLORKEYENABLE");

        m_commonIntf->SetColorKeyState(dwRenderState);

        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ddrawCaps::ZBIAS_SCALE);
        break;

      // TODO:
      case D3DRENDERSTATE_EXTENTS:
        static bool s_extentsErrorShown;

        if (dwRenderState && !std::exchange(s_extentsErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_EXTENTS");

        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_COLORKEYBLENDENABLE:
        static bool s_colorKeyBlendEnableErrorShown;

        if (dwRenderState && !std::exchange(s_colorKeyBlendEnableErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_COLORKEYBLENDENABLE");

        return D3D_OK;
    }

    // This call will never fail
    return m_d3d9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(str::format(">>> D3D7Device::GetRenderState: ", dwRenderStateType));

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    // As opposed to D3D8/9, D3D7 actually validates and
    // errors out in case of unknown/invalid render states
    if (unlikely(!IsValidD3D7RenderStateType(dwRenderStateType)))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_antialias;
        return D3D_OK;

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D7 docs state: "Note that many 3-D adapters apply texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_linePattern);
        return D3D_OK;

      // Not supported by D3D7 either, but its value is stored.
      case D3DRENDERSTATE_ZVISIBLE:
        *lpdwRenderState = m_zVisible;
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_STIPPLEDALPHA:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      // TODO:
      case D3DRENDERSTATE_COLORKEYENABLE:
        *lpdwRenderState = m_commonIntf->GetColorKeyState();
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias  = 0;
        HRESULT res = m_d3d9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return res;
      } break;

      // TODO:
      case D3DRENDERSTATE_EXTENTS:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      // TODO:
      case D3DRENDERSTATE_COLORKEYBLENDENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;
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
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::PreLoad");

    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::PreLoad: Received an unwrapped surface");
      return DDERR_GENERIC;
    }

    DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

    // Make sure the texture or surface is initialized and updated
    HRESULT hr = surface7->InitializeOrUploadD3D9();

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::PreLoad: Failed to initialize/upload D3D9 surface");
      return hr;
    }

    // Does not return an HRESULT
    surface7->GetD3D9()->PreLoad();

    hr = m_proxy->PreLoad(surface7->GetProxied());
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Device::PreLoad: Failed to preload proxied surface");
      return hr;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitive(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount))
      return D3D_OK;

    if (unlikely(lpvVertices == nullptr))
      return DDERR_INVALIDPARAMS;

    m_d3d9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = m_d3d9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                     GetPrimitiveCount(d3dptPrimitiveType, dwVertexCount),
                     lpvVertices,
                     GetFVFSize(dwVertexTypeDesc));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawIndexedPrimitive");

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpvVertices == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(d3dptPrimitiveType == D3DPT_POINTLIST))
      Logger::warn("D3D7Device::DrawIndexedPrimitiveVB: D3DPT_POINTLIST primitive type");

    m_d3d9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = m_d3d9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      0,
                      dwVertexCount,
                      GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount),
                      lpwIndices,
                      d3d9::D3DFMT_INDEX16,
                      lpvVertices,
                      GetFVFSize(dwVertexTypeDesc));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
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

    D3DCLIPSTATUS clipStatus = { };
    clipStatus.dwFlags  = D3DCLIPSTATUS_STATUS;
    clipStatus.dwStatus = D3DSTATUS_DEFAULT | clipStatus9.ClipUnion | clipStatus9.ClipIntersection;

    *clip_status = clipStatus;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawPrimitiveStrided");

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount))
      return D3D_OK;

    if (unlikely(lpVertexArray == nullptr))
      return DDERR_INVALIDPARAMS;

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(dwVertexTypeDesc, lpVertexArray, dwVertexCount);

    m_d3d9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = m_d3d9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                     GetPrimitiveCount(d3dptPrimitiveType, dwVertexCount),
                     pvb.vertexData.data(),
                     pvb.stride);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitiveStrided: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawIndexedPrimitiveStrided");

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpVertexArray == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(dwVertexTypeDesc, lpVertexArray, dwVertexCount);

    m_d3d9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = m_d3d9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      0,
                      dwVertexCount,
                      GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount),
                      lpwIndices,
                      d3d9::D3DFMT_INDEX16,
                      pvb.vertexData.data(),
                      pvb.stride);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveStrided: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawPrimitiveVB");

    RefreshLastUsedDevice();

    if (unlikely(!dwNumVertices))
      return D3D_OK;

    if (unlikely(lpd3dVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D7VertexBuffer> vb = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb->IsLocked())) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    m_d3d9->SetFVF(vb->GetFVF());
    m_d3d9->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    HRESULT hr = m_d3d9->DrawPrimitive(
                           d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                           dwStartVertex,
                           GetPrimitiveCount(d3dptPrimitiveType, dwNumVertices));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Failed D3D9 call to DrawPrimitive");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::DrawIndexedPrimitiveVB");

    RefreshLastUsedDevice();

    if (unlikely(!dwNumVertices || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpd3dVertexBuffer == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(d3dptPrimitiveType == D3DPT_POINTLIST))
      Logger::warn("D3D7Device::DrawIndexedPrimitiveVB: D3DPT_POINTLIST primitive type");

    Com<D3D7VertexBuffer> vb = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb->IsLocked())) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    // Initialize here, since not all application use indexed draws
    if (unlikely(!AreIndexBuffersInitialized()))
      InitializeIndexBuffers();

    uint8_t ibIndex = 0;
    // Try to fit index buffer uploads into the smallest buffer size possible,
    // out of the five available: XS, S, M, L and XL (XL being the theoretical max)
    while (dwIndexCount > IndexCount[ibIndex]) {
      ibIndex++;
      if (unlikely(ibIndex > ddrawCaps::IndexBufferCount - 1)) {
        Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Exceeded size of largest index buffer");
        return DDERR_GENERIC;
      }
    }

    d3d9::IDirect3DIndexBuffer9* ib9 = m_ib9[ibIndex].ptr();

    UploadIndices(ib9, lpwIndices, dwIndexCount);
    m_ib9_uploads[ibIndex]++;
    m_d3d9->SetIndices(ib9);
    m_d3d9->SetFVF(vb->GetFVF());
    m_d3d9->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    HRESULT hr = m_d3d9->DrawIndexedPrimitive(
                    d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                    dwStartVertex,
                    0,
                    dwNumVertices,
                    0,
                    GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount));

    if(unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed D3D9 call to DrawIndexedPrimitive");
      return hr;
    }

    if (unlikely(!m_hasDrawn))
      m_hasDrawn = true;

    return hr;
  }

  // No actual use of it seen in the wild yet
  HRESULT STDMETHODCALLTYPE D3D7Device::ComputeSphereVisibility(D3DVECTOR *centers, D3DVALUE *radii, DWORD sphere_count, DWORD sphereCount, DWORD *visibility) {
    Logger::warn("!!! D3D7Device::ComputeSphereVisibility: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTexture(DWORD stage, IDirectDrawSurface7 **surface) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::GetTexture");

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stage >= ddrawCaps::TextureStageCount)) {
      Logger::err(str::format("D3D7Device::GetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    *surface = m_textures[stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTexture(DWORD stage, IDirectDrawSurface7 *surface) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug(">>> D3D7Device::SetTexture");

    if (unlikely(stage >= ddrawCaps::TextureStageCount)) {
      Logger::err(str::format("D3D7Device::SetTexture: Invalid texture stage: ", stage));
      return DDERR_INVALIDPARAMS;
    }

    if (unlikely(ShouldRecord()))
      return m_recorder->SetTexture(stage, surface);

    HRESULT hr;

    // Unbinding texture stages
    if (surface == nullptr) {
      Logger::debug("D3D7Device::SetTexture: Unbiding D3D9 texture");

      hr = m_d3d9->SetTexture(stage, nullptr);

      if (likely(SUCCEEDED(hr))) {
        if (m_textures[stage] != nullptr) {
          Logger::debug("D3D7Device::SetTexture: Unbinding local texture");
          m_textures[stage] = nullptr;
        }
      } else {
        Logger::err("D3D7Device::SetTexture: Failed to unbind D3D9 texture");
      }

      return hr;
    }

    // Binding texture stages
    if (unlikely(!m_commonIntf->IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::SetTexture: Received an unwrapped texture");
      return DDERR_GENERIC;
    }

    Logger::debug("D3D7Device::SetTexture: Binding D3D9 texture");

    DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

    if (unlikely(m_parent->GetOptions()->proxiedSetTexture)) {
      HRESULT hrProxy = m_proxy->SetTexture(stage, surface7->GetProxied());
      if (unlikely(FAILED(hrProxy)))
        Logger::warn("D3D7Device::SetTexture: Failed proxied call");
    }

    // Only upload textures if any sort of blit/lock operation
    // has been performed on them since the last SetTexture call
    if (surface7->GetCommonSurface()->HasDirtyMipMaps() || m_parent->GetOptions()->alwaysDirtyMipMaps) {
      hr = surface7->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetTexture: Failed to initialize/upload D3D9 texture");
        return hr;
      }

      surface7->GetCommonSurface()->UnDirtyMipMaps();
    } else {
      Logger::debug("D3D7Device::SetTexture: Skipping upload of texture and mip maps");
    }

    if (unlikely(m_textures[stage] == surface7))
      return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = surface7->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = m_d3d9->SetTexture(stage, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D7Device::SetTexture: Failed to bind D3D9 texture");
        return hr;
      }
    } else {
      d3d9::IDirect3DCubeTexture9* cube9 = surface7->GetD3D9CubeTexture();

      hr = m_d3d9->SetTexture(stage, cube9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D7Device::SetTexture: Failed to bind D3D9 cube texture");
        return hr;
      }
    }

    m_textures[stage] = surface7;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState) {
    Logger::debug(">>> D3D7Device::GetTextureStageState");

    if (unlikely(lpdwState == nullptr))
      return DDERR_INVALIDPARAMS;

    // In the case of D3DTSS_ADDRESS, which is D3D7 specific,
    // simply return based on D3DTSS_ADDRESSU
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      return m_d3d9->GetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, lpdwState);
    }

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    if (stateType != -1u) {
      // If the type has been remapped to a sampler state type
      return m_d3d9->GetSamplerState(dwStage, stateType, lpdwState);
    } else {
      return m_d3d9->GetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), lpdwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState) {
    Logger::debug(">>> D3D7Device::SetTextureStageState");

    // In the case of D3DTSS_ADDRESS, which is D3D7 specific,
    // we need to set up both D3DTSS_ADDRESSU and D3DTSS_ADDRESSV
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      m_d3d9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, dwState);
      return m_d3d9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSV, dwState);
    }

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

  // This is a precursor of our ol' pal CopyRects
  HRESULT STDMETHODCALLTYPE D3D7Device::Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags) {
    D3D7DeviceLock lock = LockDevice();

    Logger::debug("<<< D3D7Device::Load: Proxy");

    if (dst_surface == nullptr || src_surface == nullptr) {
      Logger::warn("D3D7Device::Load: null source or destination");
      return DDERR_INVALIDPARAMS;
    }

    IDirectDrawSurface7* loadSource      = src_surface;
    IDirectDrawSurface7* loadDestination = dst_surface;

    if (likely(m_commonIntf->IsWrappedSurface(src_surface))) {
      DDraw7Surface* ddraw7SurfaceSrc = static_cast<DDraw7Surface*>(src_surface);
      loadSource = ddraw7SurfaceSrc->GetProxied();
    } else {
      Logger::warn("D3D7Device::Load: Unwrapped surface source");
    }

    if (likely(m_commonIntf->IsWrappedSurface(dst_surface))) {
      DDraw7Surface* ddraw7SurfaceDst = static_cast<DDraw7Surface*>(dst_surface);
      loadDestination = ddraw7SurfaceDst->GetProxied();
    } else {
      Logger::warn("D3D7Device::Load: Unwrapped surface destination");
    }

    HRESULT hr = m_proxy->Load(loadDestination, dst_point, loadSource, src_rect, flags);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Device::Load: Failed to load surfaces");
      return hr;
    }

    // Only upload the destination surface
    if (likely(m_commonIntf->IsWrappedSurface(dst_surface))) {
      DDraw7Surface* ddraw7SurfaceDst = static_cast<DDraw7Surface*>(dst_surface);

      // Textures and cubemaps get uploaded during SetTexture calls
      if (!ddraw7SurfaceDst->GetCommonSurface()->IsTextureOrCubeMap()) {
        HRESULT hrInitDst = ddraw7SurfaceDst->InitializeOrUploadD3D9();
        if (unlikely(FAILED(hrInitDst))) {
          Logger::err("D3D7Device::Load: Failed to upload D3D9 destination surface data");
        }
      } else {
        ddraw7SurfaceDst->GetCommonSurface()->DirtyMipMaps();
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

  void D3D7Device::InitializeDS() {
    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      Logger::debug("D3D7Device::InitializeDS: Found an attached DS");

      HRESULT hrDS = m_ds->InitializeOrUploadD3D9();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D7Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        Logger::info("D3D7Device::InitializeDS: Got depth stencil from RT");

        DDSURFACEDESC2 descDS;
        descDS.dwSize = sizeof(DDSURFACEDESC2);
        m_ds->GetProxied()->GetSurfaceDesc(&descDS);
        Logger::debug(str::format("D3D7Device::InitializeDS: DepthStencil: ", descDS.dwWidth, "x", descDS.dwHeight));

        HRESULT hrDS9 = m_d3d9->SetDepthStencilSurface(m_ds->GetD3D9());
        if(unlikely(FAILED(hrDS9))) {
          Logger::err("D3D7Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      Logger::info("D3D7Device::InitializeDS: RT has no depth stencil attached");
      m_d3d9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      m_d3d9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  d3d9::IDirect3DSurface9* D3D7Device::GetD3D9BackBuffer(IDirectDrawSurface7* surface) const {
    if (unlikely(surface == nullptr))
      return nullptr;

    auto it = m_backBuffers.find(surface);
    if (likely(it != m_backBuffers.end()))
      return it->second.ptr();

    // Return the first back buffer if no other match is found
    return m_fallBackBuffer.ptr();
  }

  HRESULT D3D7Device::Reset(d3d9::D3DPRESENT_PARAMETERS* params) {
    Logger::info("D3D7Device::Reset: Resetting the D3D9 swapchain");
    HRESULT hr = m_bridge->ResetSwapChain(params);
    if (unlikely(FAILED(hr)))
      Logger::err("D3D7Device::Reset: Failed to reset the D3D9 swapchain");
    // Reset the current render target, so that it gets
    // re-associated with the regenerated back buffers
    m_rt->SetD3D9(nullptr);
    EnumerateBackBuffers(m_rtOrig->GetProxied());
    return hr;
  }

  inline HRESULT D3D7Device::InitializeIndexBuffers() {
    static constexpr DWORD Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

    for (uint8_t ibIndex = 0; ibIndex < ddrawCaps::IndexBufferCount ; ibIndex++) {
      const UINT ibSize = IndexCount[ibIndex] * sizeof(WORD);

      Logger::debug(str::format("D3D7Device::InitializeIndexBuffer: Creating index buffer, size: ", ibSize));

      HRESULT hr = m_d3d9->CreateIndexBuffer(ibSize, Usage, d3d9::D3DFMT_INDEX16,
                                             d3d9::D3DPOOL_DEFAULT, &m_ib9[ibIndex], nullptr);
      if (FAILED(hr)) {
        Logger::err("D3D7Device::InitializeIndexBuffer: Failed to initialize D3D9 index buffer");
        return hr;
      }
    }

    return D3D_OK;
  }

  inline HRESULT D3D7Device::EnumerateBackBuffers(IDirectDrawSurface7* origin) {
    m_fallBackBuffer = nullptr;
    m_backBuffers.clear();

    Logger::debug("EnumerateBackBuffers: Enumerating back buffers");

    uint32_t backBufferCount = 0;
    IDirectDrawSurface7* parentSurface = origin;

    // Always retrieve a fallback back buffer for use with
    // plain offscreen surfaces and overlays (even when no front buffer exists)
    HRESULT hrFallback = m_d3d9->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_fallBackBuffer);
    if (unlikely(FAILED(hrFallback)))
      return hrFallback;

    while (parentSurface != nullptr) {
      Com<d3d9::IDirect3DSurface9> surf9;

      DDSURFACEDESC2 desc;
      desc.dwSize = sizeof(DDSURFACEDESC2);
      parentSurface->GetSurfaceDesc(&desc);

      if (desc.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER) {
        Logger::debug("EnumerateBackBuffers: Added front buffer");
        // Always map the front buffer on top of the first back buffer to ensure that
        // direct front buffer blits some applications do are visible on screen
        HRESULT hr = m_d3d9->GetBackBuffer(0, 0, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf9);
        if (unlikely(FAILED(hr)))
          return hr;
      } else {
        Logger::debug(str::format("EnumerateBackBuffers: Added back buffer nr. ", backBufferCount + 1));
        const UINT backBuffer = !m_parent->GetOptions()->forceSingleBackBuffer ? backBufferCount : 0;
        HRESULT hr = m_d3d9->GetBackBuffer(0, backBuffer, d3d9::D3DBACKBUFFER_TYPE_MONO, &surf9);
        if (unlikely(FAILED(hr)))
          return hr;
        backBufferCount++;
      }

      m_backBuffers.emplace(std::piecewise_construct,
                            std::forward_as_tuple(parentSurface),
                            std::forward_as_tuple(std::move(surf9)));

      IDirectDrawSurface7* nextBackBuffer = nullptr;
      parentSurface->EnumAttachedSurfaces(&nextBackBuffer, ListBackBufferSurfaces7Callback);

      // the swapchain will eventually return to its origin
      if (nextBackBuffer == origin)
        break;

      parentSurface = nextBackBuffer;
    }

    return D3D_OK;
  }

  inline void D3D7Device::UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount) {
    Logger::debug(str::format("D3D7Device::UploadIndices: Uploading ", indexCount, " indices"));

    const size_t size = indexCount * sizeof(WORD);
    void* pData = nullptr;

    // Locking and unlocking are generally expected to work here
    ib9->Lock(0, size, &pData, D3DLOCK_DISCARD);
    memcpy(pData, static_cast<void*>(indices), size);
    ib9->Unlock();
  }

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_caps.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d7_interface.h"
#include "d3d7_multithread.h"

#include <array>
#include <unordered_map>

namespace dxvk {

  struct FilpRT7Flags {
    IDirectDrawSurface7* surf  = nullptr;
    DWORD                flags = 0;
  };

  class D3D7StateBlock;
  class DDraw7Surface;
  class DDraw7Interface;

  /**
  * \brief D3D7 device implementation
  */
  class D3D7Device final : public DDrawWrappedObject<D3D7Interface, IDirect3DDevice7, d3d9::IDirect3DDevice9> {

  friend class D3D7StateBlock;

  public:
    D3D7Device(
        Com<IDirect3DDevice7>&& d3d7DeviceProxy,
        D3D7Interface* pParent,
        D3DDEVICEDESC7 Desc,
        d3d9::D3DPRESENT_PARAMETERS Params9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDraw7Surface* pRT,
        DWORD CreationFlags9);

    ~D3D7Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC7 *desc);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D7 **d3d);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT *rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE SetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE GetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState);

    HRESULT STDMETHODCALLTYPE BeginStateBlock();

    HRESULT STDMETHODCALLTYPE EndStateBlock(LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE PreLoad(IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD  dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE ComputeSphereVisibility(D3DVECTOR *centers, D3DVALUE *radii, DWORD sphere_count, DWORD sphereCount, DWORD *visibility);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState);

    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD lpdwPasses);

    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbType, LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags);

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD dwLightIndex, BOOL bEnable);

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD dwLightIndex, BOOL *pbEnable);

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetInfo(DWORD info_id, void *info, DWORD info_size);

    void InitializeDS();

    d3d9::IDirect3DSurface9* GetD3D9BackBuffer(IDirectDrawSurface7* surface) const;

    HRESULT Reset(d3d9::D3DPRESENT_PARAMETERS* params);

    D3D7DeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    const D3DOptions* GetOptions() const {
      return m_parent->GetOptions();
    }

    d3d9::D3DPRESENT_PARAMETERS GetPresentParameters() const {
      return m_params9;
    }

    d3d9::D3DMULTISAMPLE_TYPE GetMultiSampleType() const {
      return m_params9.MultiSampleType;
    }

    DDraw7Surface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    bool IsMixedHWVP() const {
      return m_isMixedHWVP && m_parent->GetOptions()->deviceTypeOverride != D3DDeviceTypeOverride::SWVPMixed;
    }

    bool HasDrawn() const {
      // Returning true here means we skip all proxied back buffer blits,
      // whereas returning false means we allow all proxied back buffer blits
      return m_parent->GetOptions()->backBufferGuard == D3DBackBufferGuard::Strict   ? true :
             m_parent->GetOptions()->backBufferGuard == D3DBackBufferGuard::Disabled ? false : m_hasDrawn;
    }

    void ResetDrawTracking() {
      if (likely(m_parent->GetOptions()->backBufferGuard != D3DBackBufferGuard::Strict))
        m_hasDrawn = false;
    }

    void SetFlipRTFlags(IDirectDrawSurface7* surf, DWORD flags) {
      m_flipRTFlags.surf  = surf;
      m_flipRTFlags.flags = flags;
    }

  private:

    inline HRESULT InitializeIndexBuffers();

    inline HRESULT EnumerateBackBuffers(IDirectDrawSurface7* surface);

    inline void UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount);

    inline float GetZBiasFactor();

    // If the last index buffer is initalized, then all are initialized
    inline bool AreIndexBuffersInitialized() const {
      return m_ib9[ddrawCaps::IndexBufferCount - 1] != nullptr;
    }

    inline bool ShouldRecord() const { return m_recorder != nullptr; }

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_parent->GetLastUsedDevice() != this))
        m_parent->SetLastUsedDevice(this);
    }

    bool                          m_isMixedHWVP   = false;
    bool                          m_hasDrawn      = false;
    bool                          m_inScene       = false;

    DDraw7Interface*              m_DD7IntfParent = nullptr;

    Com<DxvkD3D8Bridge>           m_bridge;

    D3D7Multithread               m_multithread;

    static uint32_t               s_deviceCount;
    uint32_t                      m_deviceCount = 0;

    d3d9::D3DPRESENT_PARAMETERS   m_params9;

    D3DDEVICEDESC7                m_desc;
    Com<DDraw7Surface>            m_rt;
    Com<DDraw7Surface, false>     m_rtOrig;
    DDraw7Surface*                m_ds = nullptr;

    Com<d3d9::IDirect3DSurface9>  m_fallBackBuffer;
    std::unordered_map<IDirectDrawSurface7*, Com<d3d9::IDirect3DSurface9>> m_backBuffers;

    std::array<Com<DDraw7Surface, false>, ddrawCaps::TextureStageCount> m_textures;

    D3D7StateBlock* m_recorder       = nullptr;
    DWORD           m_recorderHandle = 0;
    DWORD           m_handle         = 0;
    std::unordered_map<DWORD, D3D7StateBlock> m_stateBlocks;

    // Value of D3DRENDERSTATE_ANTIALIAS
    DWORD           m_antialias     = D3DANTIALIAS_NONE;
    // Value of D3DRENDERSTATE_LINEPATTERN
    D3DLINEPATTERN  m_linePattern   = { };
    // Value of D3DRENDERSTATE_ZVISIBLE (although the RS is not supported, its value is stored)
    DWORD           m_zVisible      = 0;

    FilpRT7Flags    m_flipRTFlags;

    // Common index buffers used for indexed draws, split up into five sizes:
    // XS, S, M, L and XL, corresponding to 0.5 kb, 2 kb, 8 kb, 32 kb and 128 kb
    std::array<Com<d3d9::IDirect3DIndexBuffer9>, ddrawCaps::IndexBufferCount> m_ib9;
    uint32_t m_ib9_uploads[ddrawCaps::IndexBufferCount] = {};

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_caps.h"

#include "../d3d9/d3d9_bridge.h"

#include "d3d6_interface.h"
#include "d3d6_material.h"
#include "d3d6_multithread.h"
#include "d3d6_viewport.h"

#include <array>
#include <vector>
#include <unordered_map>

namespace dxvk {

  struct FilpRT6Flags {
    IDirectDrawSurface4* surf  = nullptr;
    DWORD                flags = 0;
  };

  class DDraw4Surface;
  class DDraw4Interface;
  class D3D6Texture;

  /**
  * \brief D3D6 device implementation
  */
  class D3D6Device final : public DDrawWrappedObject<D3D6Interface, IDirect3DDevice3, d3d9::IDirect3DDevice9> {

  public:
    D3D6Device(
        Com<IDirect3DDevice3>&& d3d6DeviceProxy,
        D3D6Interface* pParent,
        D3DDEVICEDESC Desc,
        REFCLSID deviceGUID,
        d3d9::D3DPRESENT_PARAMETERS Params9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDraw4Surface* pRT,
        DWORD CreationFlags9);

    ~D3D6Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport3 *ref, IDirect3DViewport3 **viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D3 **d3d);

    HRESULT STDMETHODCALLTYPE SetCurrentViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE GetCurrentViewport(IDirect3DViewport3 **viewport);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface4 *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface4 **surface);

    HRESULT STDMETHODCALLTYPE Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BeginIndexed(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE Vertex(void *vertex);

    HRESULT STDMETHODCALLTYPE Index(WORD wVertexIndex);

    HRESULT STDMETHODCALLTYPE End(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    HRESULT STDMETHODCALLTYPE GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState);

    HRESULT STDMETHODCALLTYPE SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD vertex_type, void *vertices, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, DWORD start_vertex, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE ComputeSphereVisibility(D3DVECTOR *centers, D3DVALUE *radii, DWORD sphere_count, DWORD flags, DWORD *ret);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirect3DTexture2 **texture);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirect3DTexture2 *texture);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState);

    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD lpdwPasses);

    void InitializeDS();

    d3d9::IDirect3DSurface9* GetD3D9BackBuffer(IDirectDrawSurface4* surface) const;

    HRESULT Reset(d3d9::D3DPRESENT_PARAMETERS* params);

    D3D6DeviceLock LockDevice() {
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

    DDraw4Surface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    D3D6Viewport* GetCurrentViewportInternal() const {
      return m_currentViewport.ptr();
    }

    D3DMATERIALHANDLE GetCurrentMaterialHandle() const {
      return m_materialHandle;
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

    void SetFlipRTFlags(IDirectDrawSurface4* surf, DWORD flags) {
      m_flipRTFlags.surf  = surf;
      m_flipRTFlags.flags = flags;
    }

  private:

    inline HRESULT InitializeIndexBuffers();

    inline HRESULT EnumerateBackBuffers(IDirectDrawSurface4* surface);

    inline void AddViewportInternal(IDirect3DViewport3* viewport);

    inline void DeleteViewportInternal(IDirect3DViewport3* viewport);

    inline void UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount);

    // If the last index buffer is initalized, then all are initialized
    inline bool AreIndexBuffersInitialized() const {
      return m_ib9[ddrawCaps::IndexBufferCount - 1] != nullptr;
    }

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_parent->GetLastUsedDevice() != this))
        m_parent->SetLastUsedDevice(this);
    }

    inline void HandlePreDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      // Docs: "Direct3D normally performs lighting calculations
      // on any vertices that contain a vertex normal."
      if (!HasValidMaterial() ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) {
        m_d3d9->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
        if (m_lighting)
          Logger::debug("D3D6Device: Disabling lighting");
          m_d3d9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
      }
    }

    inline void HandlePostDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      if ((!HasValidMaterial() ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) && m_lighting) {
        Logger::debug("D3D6Device: Enabling lighting");
        m_d3d9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
      }
    }

    inline bool HasValidMaterial() {
      return m_materialHandle != 0
         || (m_currentViewport != nullptr && m_currentViewport->GetCurrentMaterialHandle() != 0);
    }

    bool                          m_hasDrawn      = false;
    bool                          m_inScene       = false;

    DWORD                         m_lighting      = FALSE;

    DDraw4Interface*              m_DD4IntfParent = nullptr;

    Com<DxvkD3D8Bridge>           m_bridge;

    D3D6Multithread               m_multithread;

    static uint32_t               s_deviceCount;
    uint32_t                      m_deviceCount = 0;

    d3d9::D3DPRESENT_PARAMETERS   m_params9;

    D3DMATERIALHANDLE             m_materialHandle = 0;

    D3DDEVICEDESC                 m_desc;
    REFCLSID                      m_deviceGUID;

    Com<DDraw4Surface>            m_rt;
    Com<DDraw4Surface, false>     m_rtOrig;
    DDraw4Surface*                m_ds = nullptr;

    Com<d3d9::IDirect3DSurface9>  m_fallBackBuffer;
    std::unordered_map<IDirectDrawSurface4*, Com<d3d9::IDirect3DSurface9>> m_backBuffers;

    std::array<Com<D3D6Texture, false>, ddrawCaps::TextureStageCount> m_textures;

    Com<D3D6Viewport>               m_currentViewport;
    std::vector<Com<D3D6Viewport>>  m_viewports;

    // Value of D3DRENDERSTATE_ANTIALIAS
    DWORD           m_antialias       = D3DANTIALIAS_NONE;
    // Value of D3DRENDERSTATE_LINEPATTERN
    D3DLINEPATTERN  m_linePattern     = {};
    // Value of D3DRENDERSTATE_ZVISIBLE (although the RS is not supported, its value is stored)
    DWORD           m_zVisible        = 0;
    // Value of D3DRENDERSTATE_TEXTUREMAPBLEND
    DWORD           m_textureMapBlend = D3DTBLEND_MODULATE;

    FilpRT6Flags    m_flipRTFlags;

    // Common index buffers used for indexed draws, split up into five sizes:
    // XS, S, M, L and XL, corresponding to 0.5 kb, 2 kb, 8 kb, 32 kb and 128 kb
    std::array<Com<d3d9::IDirect3DIndexBuffer9>, ddrawCaps::IndexBufferCount> m_ib9;
    uint32_t m_ib9_uploads[ddrawCaps::IndexBufferCount] = {};

  };

}
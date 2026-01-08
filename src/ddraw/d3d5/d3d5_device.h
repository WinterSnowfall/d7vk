#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_caps.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d5_interface.h"
#include "d3d5_material.h"
#include "d3d5_multithread.h"
#include "d3d5_viewport.h"

#include <vector>
#include <unordered_map>

namespace dxvk {

  struct FilpRT5Flags {
    IDirectDrawSurface*  surf  = nullptr;
    DWORD                flags = 0;
  };

  class DDrawCommonInterface;
  class DDrawSurface;
  class DDrawInterface;
  class D3D5Texture;

  /**
  * \brief D3D5 device implementation
  */
  class D3D5Device final : public DDrawWrappedObject<D3D5Interface, IDirect3DDevice2, d3d9::IDirect3DDevice9> {

  public:
    D3D5Device(
        Com<IDirect3DDevice2>&& d3d6DeviceProxy,
        D3D5Interface* pParent,
        D3DDEVICEDESC2 Desc,
        GUID deviceGUID,
        d3d9::D3DPRESENT_PARAMETERS Params9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDrawSurface* pRT,
        DWORD CreationFlags9);

    ~D3D5Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE SwapTextureHandles(IDirect3DTexture2 *tex1, IDirect3DTexture2 *tex2);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport2 *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport2 *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport2 *ref, IDirect3DViewport2 **viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D2 **d3d);

    HRESULT STDMETHODCALLTYPE SetCurrentViewport(IDirect3DViewport2 *viewport);

    HRESULT STDMETHODCALLTYPE GetCurrentViewport(IDirect3DViewport2 **viewport);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface **surface);

    HRESULT STDMETHODCALLTYPE Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, D3DVERTEXTYPE dwVertexTypeDesc, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BeginIndexed(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, DWORD flags);

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

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE vertex_type, void *vertices, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, D3DVERTEXTYPE fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    void InitializeDS();

    d3d9::IDirect3DSurface9* GetD3D9BackBuffer(IDirectDrawSurface* surface) const;

    HRESULT Reset(d3d9::D3DPRESENT_PARAMETERS* params);

    D3D5DeviceLock LockDevice() {
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

    DDrawSurface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    D3D5Viewport* GetCurrentViewportInternal() const {
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

    void SetFlipRTFlags(IDirectDrawSurface* surf, DWORD flags) {
      m_flipRTFlags.surf  = surf;
      m_flipRTFlags.flags = flags;
    }

  private:

    inline HRESULT InitializeIndexBuffers();

    inline HRESULT EnumerateBackBuffers(IDirectDrawSurface* surface);

    inline void AddViewportInternal(IDirect3DViewport2* viewport);

    inline void DeleteViewportInternal(IDirect3DViewport2* viewport);

    inline void UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount);

    inline float GetZBiasFactor();

    inline HRESULT SetTextureInternal(D3D5Texture* texture);

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_parent->GetLastUsedDevice() != this))
        m_parent->SetLastUsedDevice(this);
    }

    inline void HandlePreDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      // Docs: "Direct3D normally performs lighting calculations
      // on any vertices that contain a vertex normal."
      if (m_materialHandle == 0 ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) {
        m_d3d9->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
        if (m_lighting)
          Logger::debug("D3D5Device: Disabling lighting");
          m_d3d9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
      }
    }

    inline void HandlePostDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      if ((m_materialHandle == 0 ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) && m_lighting) {
        Logger::debug("D3D5Device: Enabling lighting");
        m_d3d9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
      }
    }

    bool                          m_hasDrawn   = false;
    bool                          m_inScene    = false;

    DWORD                         m_lighting   = FALSE;

    DDrawInterface*               m_DDIntfParent = nullptr;
    DDrawCommonInterface*         m_commonIntf = nullptr;

    Com<DxvkD3D8Bridge>           m_bridge;

    D3D5Multithread               m_multithread;

    static uint32_t               s_deviceCount;
    uint32_t                      m_deviceCount = 0;

    d3d9::D3DPRESENT_PARAMETERS   m_params9;

    D3DMATERIALHANDLE             m_materialHandle = 0;
    D3DTEXTUREHANDLE              m_textureHandle = 0;

    D3DDEVICEDESC2                m_desc;
    GUID                          m_deviceGUID;

    Com<DDrawSurface>             m_rt;
    Com<DDrawSurface, false>      m_rtOrig;
    DDrawSurface*                 m_ds = nullptr;

    Com<d3d9::IDirect3DSurface9>  m_fallBackBuffer;
    std::unordered_map<IDirectDrawSurface*, Com<d3d9::IDirect3DSurface9>> m_backBuffers;

    Com<D3D5Viewport>               m_currentViewport;
    std::vector<Com<D3D5Viewport>>  m_viewports;

    // Value of D3DRENDERSTATE_ANTIALIAS
    DWORD           m_antialias       = D3DANTIALIAS_NONE;
    // Value of D3DRENDERSTATE_LINEPATTERN
    D3DLINEPATTERN  m_linePattern     = {};
    // Value of D3DRENDERSTATE_TEXTUREMAPBLEND
    DWORD           m_textureMapBlend = D3DTBLEND_MODULATE;

    FilpRT5Flags    m_flipRTFlags;

  };

}
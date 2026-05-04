#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_caps.h"

#include "../d3d_common_device.h"

#include "../d3d_multithread.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d5_interface.h"
#include "d3d5_viewport.h"

#include <vector>

namespace dxvk {

  class D3DCommonDevice;
  class DDrawCommonInterface;
  class DDrawSurface;
  class DDrawInterface;
  class D3D3Device;

  /**
  * \brief D3D5 device implementation
  */
  class D3D5Device final : public DDrawWrappedObject<D3D5Interface, IDirect3DDevice2> {

  public:
    D3D5Device(
          D3DCommonDevice* commonD3DDevice,
          Com<IDirect3DDevice2>&& d3d5DeviceProxy,
          D3D5Interface* pParent,
          D3DDEVICEDESC2 Desc,
          GUID deviceGUID,
          d3d9::D3DPRESENT_PARAMETERS Params9,
          Com<d3d9::IDirect3DDevice9>&& pDevice9,
          DDrawSurface* pRT,
          DWORD CreationFlags9);

    ~D3D5Device();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE SwapTextureHandles(IDirect3DTexture2 *tex1, IDirect3DTexture2 *tex2);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport2 *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport2 *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport2 *lpDirect3DViewport, IDirect3DViewport2 **lplpAnotherViewport, DWORD flags);

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

    void UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface);

    D3DCommonDevice* GetCommonD3DDevice() {
      return m_commonD3DDevice.ptr();
    }

    D3DDeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    DDrawSurface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    DDrawSurface* GetDepthStencil() const {
      return m_ds.ptr();
    }

    D3D5Viewport* GetCurrentViewportInternal() const {
      return m_currentViewport.ptr();
    }

  private:

    inline void AddViewportInternal(IDirect3DViewport2* viewport);

    inline void DeleteViewportInternal(IDirect3DViewport2* viewport);

    inline HRESULT SetTextureInternal(DDrawSurface* surface, DWORD textureHandle);

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_commonIntf->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
        m_commonIntf->SetCommonD3DDevice(m_commonD3DDevice.ptr());
    }

    inline void HandlePreDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      // Docs: "Direct3D normally performs lighting calculations
      // on any vertices that contain a vertex normal."
      if (m_commonD3DDevice->GetCurrentMaterialHandle() == 0 ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) {
        d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

        device9->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
        if (m_lighting) {
          //Logger::debug("D3D5Device: Disabling lighting");
          device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
        }
      }
    }

    inline void HandlePostDrawFlags(DWORD drawFlags, DWORD vertexTypeDesc) {
      if ((m_commonD3DDevice->GetCurrentMaterialHandle() == 0 ||
          (drawFlags & D3DDP_DONOTLIGHT) ||
         !(vertexTypeDesc & D3DFVF_NORMAL)) && m_lighting) {
        d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

        //Logger::debug("D3D5Device: Enabling lighting");
        device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
      }
    }

    inline void HandlePreDrawLegacyProjection(DWORD drawFlags) {
      if (likely(m_currentViewport != nullptr)) {
        m_legacyProjection = m_currentViewport->GetCommonViewport()->GetLegacyProjectionMatrix(drawFlags);

        if (m_legacyProjection != nullptr) {
          d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

          //Logger::debug("D3D5Device: Applying legacy projection");
          device9->GetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
          device9->MultiplyTransform(d3d9::D3DTS_PROJECTION, m_legacyProjection);
        }
      }
    }

    inline void HandlePostDrawLegacyProjection() {
      if (m_legacyProjection != nullptr) {
        d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

        //Logger::debug("D3D5Device: Reverting legacy projection");
        device9->SetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
      }
    }

    static uint32_t                s_deviceCount;
    uint32_t                       m_deviceCount = 0;

    DWORD                          m_lighting    = FALSE;

    DDrawCommonInterface*          m_commonIntf  = nullptr;

    Com<D3DCommonDevice>           m_commonD3DDevice;

    Com<DxvkD3D8Bridge>            m_bridge;

    Com<D3D3Device>                m_device3;

    D3DMultithread                 m_multithread;

    D3DDEVICEDESC2                 m_desc;

    Com<DDrawSurface>              m_rt;
    Com<DDrawSurface, false>       m_ds;

    Com<D3D5Viewport>              m_currentViewport;
    std::vector<Com<D3D5Viewport>> m_viewports;

    VertexStreamInfo               m_vertexStreamInfo;
    std::vector<D3DVERTEX>         m_vertexStream;
    std::vector<D3DLVERTEX>        m_lvertexStream;
    std::vector<D3DTLVERTEX>       m_tlvertexStream;

    D3DMATRIX        m_projectionMatrix = { };
    const D3DMATRIX* m_legacyProjection = nullptr;

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"

#include "../d3d_multithread.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d3_interface.h"
#include "d3d3_viewport.h"

#include <vector>
#include <unordered_map>

namespace dxvk {

  class D3DCommonDevice;
  class DDrawCommonInterface;
  class DDrawSurface;

  /**
  * \brief D3D3 device implementation
  */
  class D3D3Device final : public DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9> {

  public:
    D3D3Device(
          D3DCommonDevice* commonD3DDevice,
          Com<IDirect3DDevice>&& d3d3DeviceProxy,
          DDrawSurface* pParent,
          D3DDEVICEDESC3 Desc,
          GUID deviceGUID,
          d3d9::D3DPRESENT_PARAMETERS Params9,
          Com<d3d9::IDirect3DDevice9>&& pDevice9,
          DWORD CreationFlags9);

    ~D3D3Device();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport *lpDirect3DViewport, IDirect3DViewport **lplpAnotherViewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D **d3d);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc);

    HRESULT STDMETHODCALLTYPE CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter);

    HRESULT STDMETHODCALLTYPE Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect);

    HRESULT STDMETHODCALLTYPE GetPickRecords(DWORD *count, D3DPICKRECORD *records);

    HRESULT STDMETHODCALLTYPE CreateMatrix(D3DMATRIXHANDLE *matrix);

    HRESULT STDMETHODCALLTYPE SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle);

    void InitializeDS();

    D3DCommonDevice* GetCommonD3DDevice() {
      return m_commonD3DDevice.ptr();
    }

    D3DDeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    void EnableLegacyLights(bool isD3DLight2) {
      m_bridge->SetLegacyLightsState(true, isD3DLight2);
    }

    D3DSTATS GetStatsInternal() const {
      return m_stats;
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

    DDrawSurface* GetDepthStencil() const {
      return m_ds.ptr();
    }

    D3D3Viewport* GetCurrentViewportInternal() const {
      return m_currentViewport.ptr();
    }

    D3DMATERIALHANDLE GetCurrentMaterialHandle() const {
      return m_materialHandle;
    }

  private:

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_commonIntf->GetD3D3Device() != this))
        m_commonIntf->SetD3D3Device(this);
    }

    inline void AddViewportInternal(IDirect3DViewport* viewport);

    inline void DeleteViewportInternal(IDirect3DViewport* viewport);

    inline HRESULT SetTextureInternal(DDrawSurface* surface, DWORD textureHandle);

    inline HRESULT STDMETHODCALLTYPE SetRenderStateInternal(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    inline HRESULT STDMETHODCALLTYPE SetLightStateInternal(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState);

    inline void DrawTriangleInternal(D3DTRIANGLE* triangle, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawLineInternal(D3DLINE* line, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawPointInternal(D3DPOINT* point, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawSpanInternal(D3DSPAN* span, DWORD count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void TextureLoadInternal(D3DTEXTURELOAD* textureLoad, DWORD count);

    inline void HandlePreDrawLegacyProjection() {
      if (likely(m_currentViewport != nullptr)) {
        m_legacyProjection = m_currentViewport->GetCommonViewport()->GetLegacyProjectionMatrix(0);

        if (m_legacyProjection != nullptr) {
          //Logger::debug("D3D3Device: Applying legacy projection");
          m_d3d9->GetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
          m_d3d9->MultiplyTransform(d3d9::D3DTS_PROJECTION, m_legacyProjection);
        }
      }
    }

    inline void HandlePostDrawLegacyProjection() {
      if (m_legacyProjection != nullptr) {
        //Logger::debug("D3D3Device: Reverting legacy projection");
        m_d3d9->SetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
      }
    }

    bool                           m_inScene     = false;

    static uint32_t                s_deviceCount;
    uint32_t                       m_deviceCount = 0;

    DDrawCommonInterface*          m_commonIntf  = nullptr;

    Com<D3DCommonDevice>           m_commonD3DDevice;

    Com<DxvkD3D8Bridge>            m_bridge;

    D3DMultithread                 m_multithread;

    d3d9::D3DPRESENT_PARAMETERS    m_params9;

    D3DMATERIALHANDLE              m_materialHandle = 0;
    D3DTEXTUREHANDLE               m_textureHandle  = 0;

    D3DDEVICEDESC3                 m_desc;
    GUID                           m_deviceGUID;

    Com<DDrawSurface>              m_rt;
    Com<DDrawSurface, false>       m_ds;

    Com<D3D3Viewport>              m_currentViewport;
    std::vector<Com<D3D3Viewport>> m_viewports;

    // Value of D3DRENDERSTATE_TEXTUREMAPBLEND
    DWORD                          m_textureMapBlend  = D3DTBLEND_MODULATE;

    D3DMATRIX                      m_projectionMatrix = { };
    const D3DMATRIX*               m_legacyProjection = nullptr;

    D3DSTATS                       m_stats            = { };

    D3DMATRIXHANDLE                m_worldHandle      = 0;
    D3DMATRIXHANDLE                m_viewHandle       = 0;
    D3DMATRIXHANDLE                m_projectionHandle = 0;

    std::atomic<D3DMATRIXHANDLE>   m_matrixHandle     = 0;
    std::unordered_map<D3DMATRIXHANDLE, D3DMATRIX> m_matrices;

  };

}
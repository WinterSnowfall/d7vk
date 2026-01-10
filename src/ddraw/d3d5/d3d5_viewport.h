#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_util.h"

#include "d3d5_interface.h"

#include "../d3d3/d3d3_viewport.h"

#include <vector>

namespace dxvk {

  class DDrawSurface;
  class D3D5Light;
  class D3D5Device;
  class D3D5Material;

  class D3D5Viewport final : public DDrawWrappedObject<D3D5Interface, IDirect3DViewport2, IUnknown> {

  public:

    D3D5Viewport(Com<IDirect3DViewport2>&& proxyViewport, D3D5Interface* pParent, IUnknown* origin);

    ~D3D5Viewport();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3D lpDirect3D);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT *data);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT *data);

    HRESULT STDMETHODCALLTYPE TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen);

    HRESULT STDMETHODCALLTYPE LightElements(DWORD element_count, D3DLIGHTDATA *data);

    HRESULT STDMETHODCALLTYPE SetBackground(D3DMATERIALHANDLE hMat);

    HRESULT STDMETHODCALLTYPE GetBackground(D3DMATERIALHANDLE *material, BOOL *valid);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth(IDirectDrawSurface *surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT *rects, DWORD flags);

    HRESULT STDMETHODCALLTYPE AddLight(IDirect3DLight *light);

    HRESULT STDMETHODCALLTYPE DeleteLight(IDirect3DLight *light);

    HRESULT STDMETHODCALLTYPE NextLight(IDirect3DLight *ref, IDirect3DLight **light, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetViewport2(D3DVIEWPORT2 *data);

    HRESULT STDMETHODCALLTYPE SetViewport2(D3DVIEWPORT2 *data);

    HRESULT ApplyViewport();

    HRESULT ApplyAndActivateLights();

    HRESULT ApplyAndActivateLight(DWORD index, D3D5Light* light5);

    d3d9::D3DVIEWPORT9 GetD3D9Viewport() const {
      return m_viewport9;
    }

    void SetDevice(D3D5Device* device) {
      m_device = device;
    }

    void SetIsCurrentViewport(bool isCurrentViewport) {
      m_isCurrentViewport = isCurrentViewport;
    }

    D3DMATERIALHANDLE GetCurrentMaterialHandle() const {
      return m_materialHandle;
    }

    bool IsCurrentViewport() const {
      return m_isCurrentViewport;
    }

  private:

    bool               m_isCurrentViewport = false;

    static uint32_t    s_viewportCount;
    uint32_t           m_viewportCount   = 0;

    IUnknown*          m_origin = nullptr;

    BOOL               m_materialIsSet   = FALSE;
    D3DMATERIALHANDLE  m_materialHandle  = 0;
    D3DCOLOR           m_backgroundColor = D3DCOLOR_RGBA(0, 0, 0, 0);

    BOOL               m_viewportIsSet = FALSE;
    D3DVIEWPORT        m_viewport      = { };
    D3DVIEWPORT2       m_viewport2     = { };

    d3d9::D3DVIEWPORT9 m_viewport9 = { };

    D3D5Device*        m_device = nullptr;

    std::vector<D3D5Light*> m_lights;

  };

}

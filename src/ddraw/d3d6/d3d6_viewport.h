#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_util.h"

#include "d3d6_interface.h"

#include <vector>

namespace dxvk {

  class DDraw4Surface;
  class D3D6Light;
  class D3D6Device;
  class D3D6Material;

  class D3D6Viewport final : public DDrawWrappedObject<D3D6Interface, IDirect3DViewport3, IUnknown> {

  public:

    D3D6Viewport(Com<IDirect3DViewport3>&& proxyViewport, D3D6Interface* pParent);

    ~D3D6Viewport();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

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

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth2(IDirectDrawSurface4 *surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid);

    HRESULT STDMETHODCALLTYPE Clear2(DWORD count, D3DRECT *rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil);

    HRESULT ApplyViewport();

    HRESULT ApplyMaterial();

    HRESULT ApplyAndActivateLights();

    HRESULT ApplyAndActivateLight(DWORD index, D3D6Light* light6);

    void SetDevice(D3D6Device* device) {
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
    uint32_t           m_viewportCount = 0;

    BOOL               m_materialIsSet  = FALSE;
    D3DMATERIALHANDLE  m_materialHandle = 0;

    BOOL               m_materialDepthIsSet = FALSE;
    DDraw4Surface*     m_materialDepth      = nullptr;

    BOOL               m_viewportIsSet = FALSE;
    D3DVIEWPORT        m_viewport      = { };
    D3DVIEWPORT2       m_viewport2     = { };

    d3d9::D3DVIEWPORT9 m_viewport9 = { };

    D3D6Device*        m_device = nullptr;

    std::vector<D3D6Light*> m_lights;

  };

}

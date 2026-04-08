#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_util.h"

#include "../d3d_common_viewport.h"

#include "d3d6_interface.h"

namespace dxvk {

  class D3DLight;

  class D3D5Viewport;
  class D3D3Viewport;

  class D3D6Viewport final : public DDrawWrappedObject<D3D6Interface, IDirect3DViewport3, IUnknown> {

  public:

    D3D6Viewport(
          D3DCommonViewport* commonViewport,
          Com<IDirect3DViewport3>&& proxyViewport,
          D3D6Interface* pParent);

    ~D3D6Viewport();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

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

    HRESULT STDMETHODCALLTYPE NextLight(IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetViewport2(D3DVIEWPORT2 *data);

    HRESULT STDMETHODCALLTYPE SetViewport2(D3DVIEWPORT2 *data);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth2(IDirectDrawSurface4 *surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid);

    HRESULT STDMETHODCALLTYPE Clear2(DWORD count, D3DRECT *rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil);

    HRESULT ApplyViewport();

    HRESULT ApplyAndActivateLights();

    HRESULT ApplyAndActivateLight(DWORD index, D3DLight* light);

    D3DCommonViewport* GetCommonViewport() const {
      return m_commonViewport.ptr();
    }

  private:

    static uint32_t        s_viewportCount;
    uint32_t               m_viewportCount = 0;

    Com<D3DCommonViewport> m_commonViewport;

    Com<D3D5Viewport>      m_viewport5;
    Com<D3D3Viewport>      m_viewport3;

  };

}

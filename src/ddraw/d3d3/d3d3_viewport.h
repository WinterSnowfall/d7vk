#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d3_interface.h"

namespace dxvk {

  class D3D3Viewport final : public DDrawWrappedObject<D3D3Interface, IDirect3DViewport, IUnknown> {

  public:

    D3D3Viewport(Com<IDirect3DViewport>&& proxyViewport, D3D3Interface* pParent, IUnknown* origin);

    ~D3D3Viewport();

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

  private:

    IUnknown*          m_origin;

    static uint32_t    s_viewportCount;
    uint32_t           m_viewportCount   = 0;

  };

}

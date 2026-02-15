#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d3_interface.h"

namespace dxvk {

  class D3D3Light final : public DDrawWrappedObject<D3D3Interface, IDirect3DLight, IUnknown> {

  public:

    D3D3Light(Com<IDirect3DLight>&& proxyLight, D3D3Interface* pParent);

    ~D3D3Light();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

    HRESULT STDMETHODCALLTYPE SetLight(D3DLIGHT *data);

    HRESULT STDMETHODCALLTYPE GetLight(D3DLIGHT *data);

  private:

    static uint32_t  s_lightCount;
    uint32_t         m_lightCount = 0;

  };

}

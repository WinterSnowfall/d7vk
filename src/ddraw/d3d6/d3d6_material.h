#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"

#include "d3d6_interface.h"

namespace dxvk {

  class D3D6Material final : public DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown> {

  public:

    D3D6Material(Com<IDirect3DMaterial3>&& proxyMaterial, D3D6Interface* pParent);

    ~D3D6Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE SetMaterial (D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial (D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle (IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle);

  private:

    static uint32_t  s_materialCount;
    uint32_t         m_materialCount = 0;

    D3DMATERIAL      m_material = { };

  };

}

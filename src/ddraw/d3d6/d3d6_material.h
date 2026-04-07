#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_material.h"

namespace dxvk {

  class D3D6Interface;

  class D3D6Material final : public DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown> {

  public:

    D3D6Material(
          Com<IDirect3DMaterial3>&& proxyMaterial,
          D3D6Interface* pParent,
          D3DMATERIALHANDLE handle);

    ~D3D6Material();

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle);

    D3DCommonMaterial* GetCommonMaterial() const {
      return m_commonMaterial.ptr();
    }

  private:

    static uint32_t        s_materialCount;
    uint32_t               m_materialCount = 0;

    Com<D3DCommonMaterial> m_commonMaterial;

  };

}

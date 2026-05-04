#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_material.h"

namespace dxvk {

  class D3D5Interface;

  class D3D5Material final : public DDrawWrappedObject<D3D5Interface, IDirect3DMaterial2> {

  public:

    D3D5Material(
          Com<IDirect3DMaterial2>&& proxyMaterial,
          D3D5Interface* pParent,
          D3DMATERIALHANDLE handle);

    ~D3D5Material();

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice2 *device, D3DMATERIALHANDLE *handle);

    D3DCommonMaterial* GetCommonMaterial() const {
      return m_commonMaterial.ptr();
    }

  private:

    static uint32_t        s_materialCount;
    uint32_t               m_materialCount = 0;

    Com<D3DCommonMaterial> m_commonMaterial;

  };

}

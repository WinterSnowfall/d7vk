#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_material.h"

namespace dxvk {

  class D3D3Interface;

  class D3D3Material final : public DDrawWrappedObject<D3D3Interface, IDirect3DMaterial> {

  public:

    D3D3Material(
          Com<IDirect3DMaterial>&& proxyMaterial,
          D3D3Interface* pParent,
          D3DMATERIALHANDLE handle);

    ~D3D3Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3D lpDirect3D);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice *device, D3DMATERIALHANDLE *handle);

    HRESULT STDMETHODCALLTYPE Reserve();

    HRESULT STDMETHODCALLTYPE Unreserve();

    D3DCommonMaterial* GetCommonMaterial() const {
      return m_commonMaterial.ptr();
    }

  private:

    static uint32_t        s_materialCount;
    uint32_t               m_materialCount = 0;

    Com<D3DCommonMaterial> m_commonMaterial;

  };

}

#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class D3D3Interface;

  class D3D3Material final : public DDrawWrappedObject<D3D3Interface, IDirect3DMaterial, IUnknown> {

  public:

    D3D3Material(Com<IDirect3DMaterial>&& proxyMaterial, D3D3Interface* pParent, D3DMATERIALHANDLE handle);

    ~D3D3Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3D lpDirect3D);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice *device, D3DMATERIALHANDLE *handle);

    HRESULT STDMETHODCALLTYPE Reserve();

    HRESULT STDMETHODCALLTYPE Unreserve();

    const d3d9::D3DMATERIAL9* GetD3D9Material() const {
      return &m_material9;
    }

    D3DCOLOR GetMaterialColor() const {
      return D3DCOLOR_RGBA(static_cast<BYTE>(m_material9.Diffuse.r * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.g * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.b * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.a * 255.0f));
    }

  private:

    static uint32_t    s_materialCount;
    uint32_t           m_materialCount = 0;

    D3DMATERIALHANDLE  m_materialHandle = 0;

    d3d9::D3DMATERIAL9 m_material9 = { };

  };

}

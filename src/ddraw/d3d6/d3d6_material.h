#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class D3D6Interface;

  class D3D6Material final : public DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown> {

  public:

    D3D6Material(Com<IDirect3DMaterial3>&& proxyMaterial, D3D6Interface* pParent, D3DMATERIALHANDLE handle);

    ~D3D6Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle);

    const d3d9::D3DMATERIAL9* GetD3D9Material() const {
      return &m_material9;
    }

    D3DCOLOR GetMaterialColor() const {
      return D3DCOLOR_RGBA(static_cast<BYTE>(m_material.dcvDiffuse.r * 255.0f),
                           static_cast<BYTE>(m_material.dcvDiffuse.g * 255.0f),
                           static_cast<BYTE>(m_material.dcvDiffuse.b * 255.0f),
                           static_cast<BYTE>(m_material.dcvDiffuse.a * 255.0f));
    }

  private:

    static uint32_t    s_materialCount;
    uint32_t           m_materialCount = 0;

    D3DMATERIALHANDLE  m_materialHandle = 0;

    D3DMATERIAL        m_material = { };

    d3d9::D3DMATERIAL9 m_material9 = { };

  };

}

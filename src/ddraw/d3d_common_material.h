#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class D3DCommonMaterial : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonMaterial(D3DMATERIALHANDLE materialHandle);

    ~D3DCommonMaterial();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    d3d9::D3DMATERIAL9* GetD3D9Material() {
      return &m_material9;
    }

    D3DMATERIALHANDLE GetMaterialHandle() const {
      return m_materialHandle;
    }

    D3DCOLOR GetMaterialColor() const {
      return D3DCOLOR_COLORVALUE(m_material9.Diffuse.r, m_material9.Diffuse.g,
                                 m_material9.Diffuse.b, m_material9.Diffuse.a);
    }

  private:

    D3DMATERIALHANDLE  m_materialHandle = 0;

    d3d9::D3DMATERIAL9 m_material9 = { };

  };

}
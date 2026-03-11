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
      return D3DCOLOR_RGBA(static_cast<BYTE>(m_material9.Diffuse.r * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.g * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.b * 255.0f),
                           static_cast<BYTE>(m_material9.Diffuse.a * 255.0f));
    }

  private:

    D3DMATERIALHANDLE  m_materialHandle = 0;

    d3d9::D3DMATERIAL9 m_material9 = { };

  };

}
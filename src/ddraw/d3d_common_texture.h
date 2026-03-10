#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_surface.h"

namespace dxvk {

  class DDrawSurface;
  class DDraw4Surface;

  class D3D3Texture;

  class D3DCommonTexture : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonTexture(DDrawCommonSurface* commonSurf, D3DTEXTUREHANDLE textureHandle);

    ~D3DCommonTexture();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    D3DTEXTUREHANDLE GetTextureHandle() const {
      return m_textureHandle;
    }

    DDrawSurface* GetDDSurface() const {
      return m_commonSurf->GetDDSurface();
    }

    void SetD3D3Texture(D3D3Texture* tex3) {
      m_tex3 = tex3;
    }

    D3D3Texture* GetD3D3Texture() const {
      return m_tex3;
    }

  private:

    DDrawCommonSurface*     m_commonSurf;

    D3DTEXTUREHANDLE        m_textureHandle = 0;

    D3D3Texture*            m_tex3 = nullptr;
  };

}
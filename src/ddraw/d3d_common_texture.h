#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_surface.h"

namespace dxvk {

  class DDrawSurface;
  class DDraw4Surface;

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

  private:

    DDrawCommonSurface*     m_commonSurf;

    D3DTEXTUREHANDLE        m_textureHandle = 0;

  };

}
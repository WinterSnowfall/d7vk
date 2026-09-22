#include "d3d_common_texture.h"

#include "ddraw_common_interface.h"

namespace dxvk {

  D3DCommonTexture::D3DCommonTexture(DDrawCommonSurface* commonSurf)
    : m_commonSurf ( commonSurf ) {
  }

  D3DCommonTexture::~D3DCommonTexture() {
    if (m_textureHandle)
      DDrawCommonInterface::ReleaseTextureHandle(m_textureHandle);
  }

  HRESULT STDMETHODCALLTYPE D3DCommonTexture::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = ref(this);
    return S_OK;
  }

  HRESULT D3DCommonTexture::GetHandleCommon(LPD3DTEXTUREHANDLE lpHandle) {
    if (unlikely(!m_commonSurf->IsTexture())) {
      // The Sims tries to get a handle from a surface which wasn't created with the DDSCAPS_TEXTURE
      // flag, so manually flag it as a texture before we initialize its D3D9 object
      if (likely(!m_commonSurf->IsInitialized())) {
        m_commonSurf->MarkWithTextureHandle();
      // If for some reason this happens after it's initialized, there's nothing we can do but log an error
      } else {
        Logger::err("D3DCommonTexture::GetHandleCommon: Parent surface isn't a texture");
      }
    }

    if (!m_textureHandle) {
      m_textureHandle = DDrawCommonInterface::GetNextTextureHandle();
      DDrawCommonInterface::EmplaceTexture(m_textureHandle, this);
    }

    *lpHandle = m_textureHandle;

    return D3D_OK;
  }

}
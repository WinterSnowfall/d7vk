#include "d3d_common_texture.h"

namespace dxvk {

  D3DCommonTexture::D3DCommonTexture(DDrawCommonSurface* commonSurf, D3DTEXTUREHANDLE textureHandle)
    : m_commonSurf ( commonSurf )
    , m_textureHandle ( textureHandle ) {
  }

  D3DCommonTexture::~D3DCommonTexture() {
  }

}
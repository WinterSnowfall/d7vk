#include "d3d_common_material.h"

namespace dxvk {

  D3DCommonMaterial::D3DCommonMaterial(D3DMATERIALHANDLE materialHandle)
    : m_materialHandle ( materialHandle ) {
  }

  D3DCommonMaterial::~D3DCommonMaterial() {
  }

}
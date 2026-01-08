#include "ddraw_common_surface.h"

namespace dxvk {

  DDrawCommonSurface::DDrawCommonSurface(DDrawCommonInterface* commonIntf)
    : m_commonIntf ( commonIntf ) {
  }

  DDrawCommonSurface::~DDrawCommonSurface() {
  }

}
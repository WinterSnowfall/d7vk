#include "d3d_common_viewport.h"

#include "d3d_common_interface.h"

#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonViewport::D3DCommonViewport(D3DCommonInterface* commonD3DIntf)
  : m_commonD3DIntf ( commonD3DIntf ) {
  }

  D3DCommonViewport::~D3DCommonViewport() {
  }

  D3D6Viewport* D3DCommonViewport::GetCurrentD3D6Viewport() {
    if (m_device6 != nullptr)
      return m_device6->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D5Viewport* D3DCommonViewport::GetCurrentD3D5Viewport() {
    if (m_device5 != nullptr)
      return m_device5->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D3Viewport* D3DCommonViewport::GetCurrentD3D3Viewport() {
    if (m_device3 != nullptr)
      return m_device3->GetCurrentViewportInternal();

    return nullptr;
  }

  void D3DCommonViewport::EnableLegacyLights(bool isD3DLight2) {
    if (m_device6 != nullptr) {
      return m_device6->EnableLegacyLights(isD3DLight2);
    } else if (m_device5 != nullptr) {
      return m_device5->EnableLegacyLights(isD3DLight2);
    } else if (m_device3 != nullptr) {
      return m_device3->EnableLegacyLights(isD3DLight2);
    }
  }

  d3d9::IDirect3DDevice9* D3DCommonViewport::GetD3D9Device() {
    if (m_device6 != nullptr) {
      return m_device6->GetD3D9();
    } else if (m_device5 != nullptr) {
      return m_device5->GetD3D9();
    } else if (m_device3 != nullptr) {
      return m_device3->GetD3D9();
    }

    return nullptr;
  }

}
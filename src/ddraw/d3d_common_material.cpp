#include "d3d_common_material.h"

#include "d3d6/d3d6_material.h"
#include "d3d5/d3d5_material.h"
#include "d3d3/d3d3_material.h"

#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonMaterial::D3DCommonMaterial(D3DMATERIALHANDLE materialHandle)
    : m_materialHandle ( materialHandle ) {
  }

  D3DCommonMaterial::~D3DCommonMaterial() {
  }

  D3DMATERIALHANDLE D3DCommonMaterial::GetProxiedMaterialHandle(IUnknown* d3dDevice) const {
    D3DMATERIALHANDLE proxiedHandle = D3DMATERIALHANDLE(0);
    HRESULT hr = DDERR_GENERIC;

    if (m_material6 != nullptr) {
      hr = m_material6->GetProxied()->GetHandle(reinterpret_cast<IDirect3DDevice3*>(d3dDevice), &proxiedHandle);
    } else if (m_material5 != nullptr) {
      hr = m_material5->GetProxied()->GetHandle(reinterpret_cast<IDirect3DDevice2*>(d3dDevice), &proxiedHandle);
    } else if (m_material3 != nullptr) {
      hr = m_material3->GetProxied()->GetHandle(reinterpret_cast<IDirect3DDevice*>(d3dDevice), &proxiedHandle);
    }

    if (unlikely(FAILED(hr)))
      Logger::warn("D3DCommonMaterial::GetProxiedMaterialHandle: Failed to retrieve proxied handle");

    return proxiedHandle;
  }

}
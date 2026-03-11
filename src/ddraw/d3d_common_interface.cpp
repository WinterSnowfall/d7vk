#include "d3d_common_interface.h"

#include "d3d_common_material.h"

#include "d3d3/d3d3_interface.h"
#include "d3d3/d3d3_material.h"
#include "d3d5/d3d5_interface.h"
#include "d3d5/d3d5_material.h"
#include "d3d6/d3d6_interface.h"
#include "d3d6/d3d6_material.h"

namespace dxvk {

  D3DCommonInterface::D3DCommonInterface() {
  }

  D3DCommonInterface::~D3DCommonInterface() {
  }

  d3d9::D3DMATERIAL9* D3DCommonInterface::GetD3D9MaterialFromHandle(D3DMATERIALHANDLE handle) const {
    if (unlikely(handle == 0))
      return nullptr;

    if (m_d3d6Intf != nullptr) {
      D3D6Material* material6 = m_d3d6Intf->GetMaterialFromHandle(handle);
      if (material6 != nullptr)
        return material6->GetCommonMaterial()->GetD3D9Material();
    }
    if (m_d3d5Intf != nullptr) {
      D3D5Material* material5 = m_d3d5Intf->GetMaterialFromHandle(handle);
      if (material5 != nullptr)
        return material5->GetCommonMaterial()->GetD3D9Material();
    }
    if (m_d3d3Intf != nullptr) {
      D3D3Material* material3 = m_d3d3Intf->GetMaterialFromHandle(handle);
      if (material3 != nullptr)
        return material3->GetCommonMaterial()->GetD3D9Material();
    }

    Logger::warn(str::format("D3DCommonInterface::GetD3D9MaterialFromHandle: Unknown handle: ", handle));
    return nullptr;
  }

  D3DCommonMaterial* D3DCommonInterface::GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle) const {
    if (unlikely(handle == 0))
      return nullptr;

    if (m_d3d6Intf != nullptr) {
      D3D6Material* material6 = m_d3d6Intf->GetMaterialFromHandle(handle);
      if (material6 != nullptr)
        return material6->GetCommonMaterial();
    }
    if (m_d3d5Intf != nullptr) {
      D3D5Material* material5 = m_d3d5Intf->GetMaterialFromHandle(handle);
      if (material5 != nullptr)
        return material5->GetCommonMaterial();
    }
    if (m_d3d3Intf != nullptr) {
      D3D3Material* material3 = m_d3d3Intf->GetMaterialFromHandle(handle);
      if (material3 != nullptr)
        return material3->GetCommonMaterial();
    }

    Logger::warn(str::format("D3DCommonInterface::GetCommonMaterialFromHandle: Unknown handle: ", handle));
    return nullptr;
  }

}
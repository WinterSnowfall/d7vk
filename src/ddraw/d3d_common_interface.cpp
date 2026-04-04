#include "d3d_common_interface.h"

#include "d3d_common_material.h"

namespace dxvk {

  D3DCommonInterface::D3DCommonInterface() {
  }

  D3DCommonInterface::~D3DCommonInterface() {
  }

  d3d9::D3DMATERIAL9* D3DCommonInterface::GetD3D9MaterialFromHandle(D3DMATERIALHANDLE handle) const {
    if (unlikely(handle == 0))
      return nullptr;

    auto materialsIter = m_materials.find(handle);

    if (unlikely(materialsIter == m_materials.end())) {
      Logger::warn(str::format("D3DCommonInterface::GetD3D9MaterialFromHandle: Unknown handle: ", handle));
      return nullptr;
    }

    return materialsIter->second->GetD3D9Material();
  }

  D3DCommonMaterial* D3DCommonInterface::GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle) const {
    if (unlikely(handle == 0))
      return nullptr;

    auto materialsIter = m_materials.find(handle);

    if (unlikely(materialsIter == m_materials.end())) {
      Logger::warn(str::format("D3DCommonInterface::GetCommonMaterialFromHandle: Unknown handle: ", handle));
      return nullptr;
    }

    return materialsIter->second;
  }

  void D3DCommonInterface::EmplaceMaterial(D3DCommonMaterial* commonMaterial, D3DMATERIALHANDLE handle) {
    m_materials.emplace(std::piecewise_construct,
                        std::forward_as_tuple(handle),
                        std::forward_as_tuple(commonMaterial));
  }

  void D3DCommonInterface::ReleaseMaterialHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = m_materials.find(handle);

    if (likely(materialsIter != m_materials.end()))
      m_materials.erase(materialsIter);
  }

}
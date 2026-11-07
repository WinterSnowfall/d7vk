#include "d3d_common_interface.h"

#include "ddraw_common_interface.h"

#include "d3d_common_material.h"

#include "ddraw7/ddraw7_interface.h"
#include "ddraw4/ddraw4_interface.h"

namespace dxvk {

  std::atomic<D3DMATERIALHANDLE> D3DCommonInterface::s_materialHandle = { 0u };

  D3DCommonInterface::D3DCommonInterface(
        Com<d3d9::IDirect3D9>&& d3d9Intf,
        Com<IDxvkLegacyD3DInterfaceBridge>&& intfBridge)
    : m_d3d9Intf ( std::move(d3d9Intf) )
    , m_bridge   ( std::move(intfBridge) ) {
  }

  D3DCommonInterface::~D3DCommonInterface() {
  }

  HRESULT STDMETHODCALLTYPE D3DCommonInterface::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = ref(this);
    return S_OK;
  }

  // Calling EvictManagedTextures can be done with the same end effect
  // on any proxied IDirect3D7/3 object, so simply query for one on demand
  HRESULT D3DCommonInterface::EvictManagedTextures(DDrawCommonInterface* commonIntf) {
    DDraw7Interface* intf7 = commonIntf->GetDD7Interface();
    if (intf7 != nullptr) {
      Com<IDirect3D7> d3d7IntfProxied;
      intf7->GetProxied()->QueryInterface(__uuidof(IDirect3D7), reinterpret_cast<void**>(&d3d7IntfProxied));

      if (likely(d3d7IntfProxied != nullptr)) {
        HRESULT hr = d3d7IntfProxied->EvictManagedTextures();
        if (unlikely(FAILED(hr)))
          return hr;
      }
    } else {
      DDraw4Interface* intf4 = commonIntf->GetDD4Interface();
      if (intf4 != nullptr) {
        Com<IDirect3D3> d3d6IntfProxied;
        intf4->GetProxied()->QueryInterface(__uuidof(IDirect3D3), reinterpret_cast<void**>(&d3d6IntfProxied));

        if (likely(d3d6IntfProxied != nullptr)) {
          HRESULT hr = d3d6IntfProxied->EvictManagedTextures();
          if (unlikely(FAILED(hr)))
            return hr;
        }
      }
    }

    return D3D_OK;
  }

  D3DCommonMaterial* D3DCommonInterface::GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle) {
    if (unlikely(handle == 0))
      return nullptr;

    auto materialsIter = s_materials.find(handle);
    if (unlikely(materialsIter == s_materials.end())) {
      Logger::warn(str::format("D3DCommonInterface::GetCommonMaterialFromHandle: Unknown handle: ", handle));
      return nullptr;
    }

    return materialsIter->second;
  }

  void D3DCommonInterface::EmplaceMaterial(D3DMATERIALHANDLE handle, D3DCommonMaterial* commonMaterial) {
    s_materials.emplace(std::piecewise_construct,
                        std::forward_as_tuple(handle),
                        std::forward_as_tuple(commonMaterial));
  }

  void D3DCommonInterface::ReleaseMaterialHandle(D3DMATERIALHANDLE handle) {
    auto materialsIter = s_materials.find(handle);

    if (likely(materialsIter != s_materials.end()))
      s_materials.erase(materialsIter);
  }

  d3d9::D3DMULTISAMPLE_TYPE D3DCommonInterface::GetMultiSampleType(d3d9::D3DFORMAT backBufferFormat) const {
    HRESULT hr = m_d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                        TRUE, d3d9::D3DMULTISAMPLE_4_SAMPLES, NULL);
    if (likely(SUCCEEDED(hr))) {
      Logger::info("D3DCommonInterface::GetMultiSampleType: Using 4x MSAA for FSAA emulation");
      return d3d9::D3DMULTISAMPLE_4_SAMPLES;
    }

    hr = m_d3d9Intf->CheckDeviceMultiSampleType(0, d3d9::D3DDEVTYPE_HAL, backBufferFormat,
                                                TRUE, d3d9::D3DMULTISAMPLE_2_SAMPLES, NULL);
    if (SUCCEEDED(hr)) {
      Logger::info("D3DCommonInterface::GetMultiSampleType: Using 2x MSAA for FSAA emulation");
      return d3d9::D3DMULTISAMPLE_2_SAMPLES;
    }

    return d3d9::D3DMULTISAMPLE_NONE;
  }

}

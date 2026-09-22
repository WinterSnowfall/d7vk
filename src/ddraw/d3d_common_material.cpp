#include "d3d_common_material.h"

#include "d3d_common_interface.h"
#include "d3d_common_device.h"

namespace dxvk {

  D3DCommonMaterial::D3DCommonMaterial() {
  }

  D3DCommonMaterial::~D3DCommonMaterial() {
    if (m_materialHandle)
      D3DCommonInterface::ReleaseMaterialHandle(m_materialHandle);
  }

  HRESULT STDMETHODCALLTYPE D3DCommonMaterial::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = ref(this);
    return S_OK;
  }

  HRESULT D3DCommonMaterial::SetMaterialCommon(D3DMATERIAL* data, D3DCommonDevice* commonDevice) {
    m_material9.Diffuse  = data->dcvDiffuse;
    m_material9.Ambient  = data->dcvAmbient;
    m_material9.Specular = data->dcvSpecular;
    m_material9.Emissive = data->dcvEmissive;
    m_material9.Power    = data->dvPower;

    m_dirtyColor = true;

    // Update the D3D9 material directly if it's actively being used
    if (m_materialHandle && commonDevice != nullptr) {
      const D3DMATERIALHANDLE currentHandle = commonDevice->GetCurrentMaterialHandle();
      if (currentHandle == m_materialHandle)
        commonDevice->GetD3D9Device()->SetMaterial(&m_material9);
    }

    return D3D_OK;
  }

  HRESULT D3DCommonMaterial::GetMaterialCommon(D3DMATERIAL* data) {
    data->dcvDiffuse  = m_material9.Diffuse;
    data->dcvAmbient  = m_material9.Ambient;
    data->dcvSpecular = m_material9.Specular;
    data->dcvEmissive = m_material9.Emissive;
    data->dvPower     = m_material9.Power;
    data->hTexture    = 0u; // Relevant only for IID_IDirect3DRampDevice
    data->dwRampSize  = 0u; // Relevant only for IID_IDirect3DRampDevice

    return D3D_OK;
  }

  HRESULT D3DCommonMaterial::GetHandleCommon(D3DMATERIALHANDLE* handle) {
    if (!m_materialHandle) {
      m_materialHandle = D3DCommonInterface::GetNextMaterialHandle();
      D3DCommonInterface::EmplaceMaterial(m_materialHandle, this);
    }

    *handle = m_materialHandle;

    return D3D_OK;
  }

}
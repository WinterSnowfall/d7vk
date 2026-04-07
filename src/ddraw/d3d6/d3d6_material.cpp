#include "d3d6_material.h"

#include "d3d6_device.h"
#include "d3d6_interface.h"
#include "d3d6_viewport.h"

#include "../ddraw4/ddraw4_interface.h"

namespace dxvk {

  uint32_t D3D6Material::s_materialCount = 0;

  D3D6Material::D3D6Material(
        Com<IDirect3DMaterial3>&& proxyMaterial,
        D3D6Interface* pParent,
        D3DMATERIALHANDLE handle)
    : DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown>(pParent, std::move(proxyMaterial), nullptr) {
    m_commonMaterial = new D3DCommonMaterial(handle);

    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D6Material: Created a new material nr. [[3-", m_materialCount, "]]"));
  }

  D3D6Material::~D3D6Material() {
    m_parent->GetCommonD3DInterface()->ReleaseMaterialHandle(m_commonMaterial->GetMaterialHandle());

    Logger::debug(str::format("D3D6Material: Material nr. [[3-", m_materialCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::SetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D6Material::SetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    material9->Diffuse  = data->dcvDiffuse;
    material9->Ambient  = data->dcvAmbient;
    material9->Specular = data->dcvSpecular;
    material9->Emissive = data->dcvEmissive;
    material9->Power    = data->dvPower;

    D3DMATERIALHANDLE handle = m_commonMaterial->GetMaterialHandle();

    Logger::debug(str::format(">>> D3D6Material::SetMaterial: Updated material nr. ", handle));
    Logger::debug(str::format("   Diffuse:  ", material9->Diffuse.r,  " ", material9->Diffuse.g, " ", material9->Diffuse.b));
    Logger::debug(str::format("   Ambient:  ", material9->Ambient.r,  " ", material9->Ambient.g, " ", material9->Ambient.b));
    Logger::debug(str::format("   Specular: ", material9->Specular.r, " ", material9->Specular.g, " ", material9->Specular.b));
    Logger::debug(str::format("   Emissive: ", material9->Emissive.r, " ", material9->Emissive.g, " ", material9->Emissive.b));
    Logger::debug(str::format("   Power:    ", material9->Power));

    // Update the D3D9 material directly if it's actively being used
    D3D6Device* device6 = m_parent->GetCommonInterface()->GetD3D6Device();
    if (likely(device6 != nullptr)) {
      D3DMATERIALHANDLE currentHandle = device6->GetCurrentMaterialHandle();
      if (currentHandle == handle) {
        Logger::debug(str::format("D3D6Material::SetMaterial: Applying material nr. ", handle, " to D3D9"));
        device6->GetD3D9()->SetMaterial(material9);
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D6Material::GetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    data->dcvDiffuse  = material9->Diffuse;
    data->dcvAmbient  = material9->Ambient;
    data->dcvSpecular = material9->Specular;
    data->dcvEmissive = material9->Emissive;
    data->dvPower     = material9->Power;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetHandle(IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle) {
    Logger::debug(">>> D3D6Material::GetHandle");

    if(unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    *handle = m_commonMaterial->GetMaterialHandle();

    return D3D_OK;
  }

}

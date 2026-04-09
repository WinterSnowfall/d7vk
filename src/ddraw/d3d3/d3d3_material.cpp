#include "d3d3_material.h"

#include "d3d3_device.h"
#include "d3d3_interface.h"
#include "d3d3_viewport.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  uint32_t D3D3Material::s_materialCount = 0;

  D3D3Material::D3D3Material(
        Com<IDirect3DMaterial>&& proxyMaterial,
        D3D3Interface* pParent,
        D3DMATERIALHANDLE handle)
    : DDrawWrappedObject<D3D3Interface, IDirect3DMaterial, IUnknown>(pParent, std::move(proxyMaterial), nullptr) {
    m_commonMaterial = new D3DCommonMaterial(handle);

    m_commonMaterial->SetD3D3Material(this);

    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D3Material: Created a new material nr. [[1-", m_materialCount, "]]"));
  }

  D3D3Material::~D3D3Material() {
    m_parent->GetCommonD3DInterface()->ReleaseMaterialHandle(m_commonMaterial->GetMaterialHandle());

    m_commonMaterial->SetD3D3Material(nullptr);

    Logger::debug(str::format("D3D3Material: Material nr. [[1-", m_materialCount, "]] bites the dust"));
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DMaterial object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Material::Initialize(LPDIRECT3D lpDirect3D) {
    Logger::debug(">>> D3D3Material::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::SetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D3Material::SetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    // This call needs to be forwarded to the proxied material
    // too, in order to have a proper color used during proxied clears
    HRESULT hr = m_proxy->SetMaterial(data);
    if (unlikely(FAILED(hr)))
      Logger::warn("D3D3Material::SetMaterial: Failed to set the proxied material");

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    material9->Diffuse  = data->dcvDiffuse;
    material9->Ambient  = data->dcvAmbient;
    material9->Specular = data->dcvSpecular;
    material9->Emissive = data->dcvEmissive;
    material9->Power    = data->dvPower;

    D3DMATERIALHANDLE handle = m_commonMaterial->GetMaterialHandle();

    Logger::debug(str::format(">>> D3D3Material::SetMaterial: Updated material nr. ", handle));
    Logger::debug(str::format("   Diffuse:  ", material9->Diffuse.r,  " ", material9->Diffuse.g, " ", material9->Diffuse.b));
    Logger::debug(str::format("   Ambient:  ", material9->Ambient.r,  " ", material9->Ambient.g, " ", material9->Ambient.b));
    Logger::debug(str::format("   Specular: ", material9->Specular.r, " ", material9->Specular.g, " ", material9->Specular.b));
    Logger::debug(str::format("   Emissive: ", material9->Emissive.r, " ", material9->Emissive.g, " ", material9->Emissive.b));
    Logger::debug(str::format("   Power:    ", material9->Power));

    // Update the D3D9 material directly if it's actively being used
    D3D3Device* device3 = m_parent->GetCommonInterface()->GetCommonD3DDevice()->GetD3D3Device();
    if (likely(device3 != nullptr)) {
      D3DMATERIALHANDLE currentHandle = device3->GetCurrentMaterialHandle();
      if (currentHandle == handle) {
        Logger::debug(str::format("D3D3Material::SetMaterial: Applying material nr. ", handle, " to D3D9"));
        device3->GetD3D9()->SetMaterial(material9);
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D3Material::GetMaterial");

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

  HRESULT STDMETHODCALLTYPE D3D3Material::GetHandle(IDirect3DDevice *device, D3DMATERIALHANDLE *handle) {
    Logger::debug(">>> D3D3Material::GetHandle");

    if(unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    *handle = m_commonMaterial->GetMaterialHandle();

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Material::Reserve() {
    return DDERR_UNSUPPORTED;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Material::Unreserve() {
    return DDERR_UNSUPPORTED;
  }

}

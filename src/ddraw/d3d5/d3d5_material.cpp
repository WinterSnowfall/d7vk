#include "d3d5_material.h"

#include "d3d5_device.h"
#include "d3d5_interface.h"
#include "d3d5_viewport.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  uint32_t D3D5Material::s_materialCount = 0;

  D3D5Material::D3D5Material(
      Com<IDirect3DMaterial2>&& proxyMaterial,
      D3D5Interface* pParent,
      D3DMATERIALHANDLE handle)
    : DDrawWrappedObject<D3D5Interface, IDirect3DMaterial2, IUnknown>(pParent, std::move(proxyMaterial), nullptr) {
    m_commonMaterial = new D3DCommonMaterial(handle);

    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D5Material: Created a new material nr. [[2-", m_materialCount, "]]"));
  }

  D3D5Material::~D3D5Material() {
    Logger::debug(str::format("D3D5Material: Material nr. [[2-", m_materialCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D5Interface, IDirect3DMaterial2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    // Materials are managed through handles, so nobody will query this,
    // nor should we care in particular about older interfaces here
    if (riid == __uuidof(IDirect3DMaterial2))
      return this;

    throw DxvkError("D3D5Material::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Material::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Material::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  HRESULT STDMETHODCALLTYPE D3D5Material::SetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D5Material::SetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DMATERIAL9* material9 = m_commonMaterial->GetD3D9Material();

    material9->Diffuse  = data->dcvDiffuse;
    material9->Ambient  = data->dcvAmbient;
    material9->Specular = data->dcvSpecular;
    material9->Emissive = data->dcvEmissive;
    material9->Power    = data->dvPower;

    D3DMATERIALHANDLE handle = m_commonMaterial->GetMaterialHandle();

    Logger::debug(str::format(">>> D3D5Material::SetMaterial: Updated material nr. ", handle));
    Logger::debug(str::format("   Diffuse:  ", material9->Diffuse.r,  " ", material9->Diffuse.g, " ", material9->Diffuse.b));
    Logger::debug(str::format("   Ambient:  ", material9->Ambient.r,  " ", material9->Ambient.g, " ", material9->Ambient.b));
    Logger::debug(str::format("   Specular: ", material9->Specular.r, " ", material9->Specular.g, " ", material9->Specular.b));
    Logger::debug(str::format("   Emissive: ", material9->Emissive.r, " ", material9->Emissive.g, " ", material9->Emissive.b));
    Logger::debug(str::format("   Power:    ", material9->Power));

    // Update the D3D9 material directly if it's actively being used
    D3D5Device* device5 = m_parent->GetParent()->GetCommonInterface()->GetD3D5Device();
    if (likely(device5 != nullptr)) {
      D3DMATERIALHANDLE currentHandle = device5->GetCurrentMaterialHandle();
      if (currentHandle == handle) {
        Logger::debug(str::format("D3D5Material::SetMaterial: Applying material nr. ", handle, " to D3D9"));
        device5->GetD3D9()->SetMaterial(material9);
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Material::GetMaterial(D3DMATERIAL *data) {
    Logger::debug(">>> D3D5Material::GetMaterial");

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

  HRESULT STDMETHODCALLTYPE D3D5Material::GetHandle(IDirect3DDevice2 *device, D3DMATERIALHANDLE *handle) {
    Logger::debug(">>> D3D5Material::GetHandle");

    if(unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    *handle = m_commonMaterial->GetMaterialHandle();

    return D3D_OK;
  }

}

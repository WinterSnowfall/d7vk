#include "d3d6_material.h"

#include "d3d6_interface.h"

#include "../d3d_common_device.h"

#include "../ddraw_common_interface.h"

#include "../ddraw4/ddraw4_interface.h"

namespace dxvk {

  D3D6Material::D3D6Material(
        D3DCommonMaterial* commonMaterial,
        D3D6Interface* pParent)
    : DDrawChildObject<D3D6Interface, IDirect3DMaterial3>(pParent)
    , m_commonMaterial ( commonMaterial ) {
    if (m_commonMaterial == nullptr)
      m_commonMaterial = new D3DCommonMaterial();

    m_commonMaterial->SetD3D6Material(this);
  }

  D3D6Material::~D3D6Material() {
    m_commonMaterial->SetD3D6Material(nullptr);
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown)
            || riid == __uuidof(IDirect3DMaterial3))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D6Material::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::SetMaterial(D3DMATERIAL* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    D3DCommonDevice* commonDevice = m_parent->GetCommonInterface()->GetCommonD3DDevice();

    return m_commonMaterial->SetMaterialCommon(data, commonDevice);
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetMaterial(D3DMATERIAL* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonMaterial->GetMaterialCommon(data);
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetHandle(IDirect3DDevice3* device, D3DMATERIALHANDLE* handle) {
    if (unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonMaterial->GetHandleCommon(handle);
  }

}

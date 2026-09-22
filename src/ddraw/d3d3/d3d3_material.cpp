#include "d3d3_material.h"

#include "d3d3_interface.h"

#include "../d3d_common_device.h"

#include "../ddraw_common_interface.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  D3D3Material::D3D3Material(
        D3DCommonMaterial* commonMaterial,
        D3D3Interface* pParent)
    : DDrawChildObject<D3D3Interface, IDirect3DMaterial>(pParent)
    , m_commonMaterial ( commonMaterial ) {
    if (m_commonMaterial == nullptr)
      m_commonMaterial = new D3DCommonMaterial();

    m_commonMaterial->SetD3D3Material(this);
  }

  D3D3Material::~D3D3Material() {
    m_commonMaterial->SetD3D3Material(nullptr);
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DMaterial))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Material::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DMaterial object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Material::Initialize(LPDIRECT3D lpDirect3D) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::SetMaterial(D3DMATERIAL* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    D3DCommonDevice* commonDevice = m_parent->GetCommonInterface()->GetCommonD3DDevice();

    return m_commonMaterial->SetMaterialCommon(data, commonDevice);
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetMaterial(D3DMATERIAL* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonMaterial->GetMaterialCommon(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetHandle(IDirect3DDevice* device, D3DMATERIALHANDLE* handle) {
    if (unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonMaterial->GetHandleCommon(handle);
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

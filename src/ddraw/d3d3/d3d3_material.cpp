#include "d3d3_material.h"

#include "d3d3_device.h"
#include "d3d3_interface.h"

namespace dxvk {

  uint32_t D3D3Material::s_materialCount = 0;

  D3D3Material::D3D3Material(Com<IDirect3DMaterial>&& proxyMaterial, D3D3Interface* pParent)
    : DDrawWrappedObject<D3D3Interface, IDirect3DMaterial, IUnknown>(pParent, std::move(proxyMaterial), nullptr) {
    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D3Material: Created a new material nr. [[1-", m_materialCount, "]]"));
  }

  D3D3Material::~D3D3Material() {
    Logger::debug(str::format("D3D3Material: Material nr. [[1-", m_materialCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D3Interface, IDirect3DMaterial, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    // Materials are managed through handles, so nobody will query this,
    // nor should we care in particular about older interfaces here
    if (riid == __uuidof(IDirect3DMaterial))
      return this;

    throw DxvkError("D3D3Material::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D3Material::QueryInterface");

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

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DMaterial object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Material::Initialize(LPDIRECT3D lpDirect3D) {
    Logger::debug(">>> D3D3Material::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::SetMaterial(D3DMATERIAL *data) {
    Logger::debug("<<< D3D3Material::SetMaterial: Proxy");
    return m_proxy->SetMaterial(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetMaterial(D3DMATERIAL *data) {
    Logger::debug("<<< D3D3Material::GetMaterial: Proxy");
    return m_proxy->GetMaterial(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Material::GetHandle(IDirect3DDevice *device, D3DMATERIALHANDLE *handle) {
    Logger::debug("<<< D3D3Material::GetHandle: Proxy");

    D3D3Device* d3d3Device = static_cast<D3D3Device*>(device);
    return m_proxy->GetHandle(d3d3Device->GetProxied(), handle);
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

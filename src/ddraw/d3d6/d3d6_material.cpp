#include "d3d6_material.h"

namespace dxvk {

  uint32_t D3D6Material::s_materialCount = 0;

  D3D6Material::D3D6Material(Com<IDirect3DMaterial3>&& proxyMaterial, D3D6Interface* pParent)
    : DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown>(pParent, std::move(proxyMaterial), nullptr) {
    m_materialCount = ++s_materialCount;

    Logger::debug(str::format("D3D6Material: Created a new material nr. [[3-", m_materialCount, "]]"));
  }

  D3D6Material::~D3D6Material() {
    Logger::debug(str::format("D3D6Material: Material nr. [[3-", m_materialCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D6Interface, IDirect3DMaterial3, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    // This appears to be largely unhandled for IDirect3DMaterial3...
    /*if (riid == __uuidof(IDirect3DMaterial3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Material::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }*/

    throw DxvkError("D3D6Material::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Material::QueryInterface");

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

  HRESULT STDMETHODCALLTYPE D3D6Material::SetMaterial (D3DMATERIAL *data) {
    Logger::debug(">>> D3D6Viewport::SetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    m_material = *data;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetMaterial (D3DMATERIAL *data) {
    Logger::debug(">>> D3D6Viewport::GetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    *data = m_material;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Material::GetHandle (IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle) {
    Logger::debug(">>> D3D6Viewport::GetHandle");

    if(unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    // Use the material count as a token, ignore the device
    *handle = static_cast<D3DMATERIALHANDLE>(m_materialCount);

    return D3D_OK;
  }

}

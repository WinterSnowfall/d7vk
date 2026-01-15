#include "d3d5_material.h"

#include "d3d5_device.h"
#include "d3d5_interface.h"
#include "d3d5_viewport.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  uint32_t D3D5Material::s_materialCount = 0;

  D3D5Material::D3D5Material(Com<IDirect3DMaterial2>&& proxyMaterial, D3D5Interface* pParent, D3DMATERIALHANDLE handle)
    : DDrawWrappedObject<D3D5Interface, IDirect3DMaterial2, IUnknown>(pParent, std::move(proxyMaterial), nullptr)
    , m_materialHandle ( handle ) {
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
    if (riid == __uuidof(IDirect3DMaterial2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Material::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

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
    if (unlikely(m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Material::SetMaterial: Proxy");
      return m_proxy->SetMaterial(data);
    }

    Logger::debug(">>> D3D5Material::SetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    m_material = *data;

    m_material9.Diffuse  = m_material.dcvDiffuse;
    m_material9.Ambient  = m_material.dcvAmbient;
    m_material9.Specular = m_material.dcvSpecular;
    m_material9.Emissive = m_material.dcvEmissive;
    m_material9.Power    = m_material.dvPower;

    Logger::debug(str::format(">>> D3D5Material::SetMaterial: Updated material nr. ", m_materialHandle));
    Logger::debug(str::format("   Diffuse:  ", m_material9.Diffuse.r, " ", m_material9.Diffuse.g, " ", m_material9.Diffuse.b));
    Logger::debug(str::format("   Ambient:  ", m_material9.Ambient.r, " ", m_material9.Ambient.g, " ", m_material9.Ambient.b));
    Logger::debug(str::format("   Specular: ", m_material9.Specular.r, " ", m_material9.Specular.g, " ", m_material9.Specular.b));
    Logger::debug(str::format("   Emissive: ", m_material9.Emissive.r, " ", m_material9.Emissive.g, " ", m_material9.Emissive.b));
    Logger::debug(str::format("   Power:    ", m_material9.Power));

    // Update the D3D9 material directly if it's actively being used
    D3D5Device* device5 = m_parent->GetParent()->GetCommonInterface()->GetD3D5Device();
    if (likely(device5 != nullptr)) {
      D3DMATERIALHANDLE material = device5->GetCurrentMaterialHandle();
      if (material == m_materialHandle) {
        Logger::debug(str::format("D3D5Material::SetMaterial: Applying material nr. ", m_materialHandle, " to D3D9"));
        device5->GetD3D9()->SetMaterial(&m_material9);
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Material::GetMaterial(D3DMATERIAL *data) {
    if (unlikely(m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Material::GetMaterial: Proxy");
      return m_proxy->GetMaterial(data);
    }

    Logger::debug(">>> D3D5Material::GetMaterial");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    *data = m_material;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Material::GetHandle(IDirect3DDevice2 *device, D3DMATERIALHANDLE *handle) {
    if (unlikely(m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Material::GetHandle: Proxy");
      D3D5Device* d3d5Device = static_cast<D3D5Device*>(device);
      return m_proxy->GetHandle(d3d5Device->GetProxied(), handle);
    }

    Logger::debug(">>> D3D5Material::GetHandle");

    if(unlikely(device == nullptr || handle == nullptr))
      return DDERR_INVALIDPARAMS;

    *handle = m_materialHandle;

    return D3D_OK;
  }

}

#include "d3d3_light.h"

namespace dxvk {

  uint32_t D3D3Light::s_lightCount = 0;

  D3D3Light::D3D3Light(Com<IDirect3DLight>&& proxyLight, D3D3Interface* pParent)
    : DDrawWrappedObject<D3D3Interface, IDirect3DLight, IUnknown>(pParent, std::move(proxyLight), nullptr) {
    m_lightCount = ++s_lightCount;

    Logger::debug(str::format("D3D3Light: Created a new light nr. [[1-", m_lightCount, "]]"));
  }

  D3D3Light::~D3D3Light() {
    Logger::debug(str::format("D3D3Light: Light nr. [[1-", m_lightCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D3Interface, IDirect3DLight, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DLight))
      return this;

    throw DxvkError("D3D3Light::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Light::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D3Light::QueryInterface");

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

  // Docs state: "The method returns DDERR_ALREADYINITIALIZED because
  // the Direct3DLight object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Light::Initialize(IDirect3D *d3d) {
    Logger::debug(">>> D3D3Light::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Light::SetLight(D3DLIGHT *data) {
    Logger::debug("<<< D3D3Light::SetLight: Proxy");
    return m_proxy->SetLight(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Light::GetLight(D3DLIGHT *data) {
    Logger::debug("<<< D3D3Light::GetLight: Proxy");
    return m_proxy->GetLight(data);
  }

}

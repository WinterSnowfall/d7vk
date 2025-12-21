#include "d3d6_light.h"

namespace dxvk {

  uint32_t D3D6Light::s_lightCount = 0;

  D3D6Light::D3D6Light(Com<IDirect3DLight>&& proxyLight, D3D6Interface* pParent)
    : DDrawWrappedObject<D3D6Interface, IDirect3DLight, IUnknown>(pParent, std::move(proxyLight), nullptr) {
    m_lightCount = ++s_lightCount;

    Logger::debug(str::format("D3D6Light: Created a new light nr. [[1-", m_lightCount, "]]"));
  }

  D3D6Light::~D3D6Light() {
    Logger::debug(str::format("D3D6Light: Light nr. [[1-", m_lightCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D6Interface, IDirect3DLight, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    // This appears to be largely unhandled for IDirect3DLight...
    /*if (riid == __uuidof(IDirect3DLight)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Light::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }*/

    throw DxvkError("D3D6Light::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D6Light::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Light::QueryInterface");

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
  HRESULT STDMETHODCALLTYPE D3D6Light::Initialize(IDirect3D *d3d) {
    Logger::debug(">>> D3D6Viewport::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Light::SetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3D6Viewport::SetLight");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    m_light = *data;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Light::GetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3D6Viewport::GetLight");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    *data = m_light;

    return D3D_OK;
  }

}

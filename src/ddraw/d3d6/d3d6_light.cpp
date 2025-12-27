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
    Logger::debug(">>> D3D6Light::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Light::SetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3D6Light::SetLight");

    static constexpr D3DCOLORVALUE zeroValue = {{0.0f}, {0.0f}, {0.0f}, {0.0f}};

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dltType == D3DLIGHT_PARALLELPOINT))
      Logger::warn("D3D6Light::SetLight: Unsupported light type D3DLIGHT_PARALLELPOINT");

    // Docs: "Although this method's declaration specifies the lpLight parameter as being
    // the address of a D3DLIGHT structure, that structure is not normally used. Rather,
    // the D3DLIGHT2 structure is recommended to achieve the best lighting effects."
    memcpy(&m_light, data, sizeof(D3DLIGHT2));

    m_light9.Type         = d3d9::D3DLIGHTTYPE(m_light.dltType);
    m_light9.Diffuse      = m_light.dcvColor;
    m_light9.Specular     = HasSpecular() ? m_light.dcvColor : zeroValue;
    m_light9.Ambient      = m_light.dcvColor;
    m_light9.Position     = m_light.dvPosition;
    m_light9.Direction    = m_light.dvDirection;
    m_light9.Range        = m_light.dvRange;
    m_light9.Falloff      = m_light.dvFalloff;
    m_light9.Attenuation0 = m_light.dvAttenuation0;
    m_light9.Attenuation1 = m_light.dvAttenuation1;
    m_light9.Attenuation2 = m_light.dvAttenuation2;
    m_light9.Theta        = m_light.dvTheta;
    m_light9.Phi          = m_light.dvPhi;

    Logger::debug(str::format(">>> D3D6Light::SetLight: Updated light nr. ", m_lightCount));
    Logger::debug(str::format("   Type:         ", m_light9.Type));
    Logger::debug(str::format("   Diffuse:      ", m_light9.Diffuse.r, " ", m_light9.Diffuse.g, " ", m_light9.Diffuse.b));
    Logger::debug(str::format("   Specular:     ", m_light9.Specular.r, " ", m_light9.Specular.g, " ", m_light9.Specular.b));
    Logger::debug(str::format("   Ambient:      ", m_light9.Ambient.r, " ", m_light9.Ambient.g, " ", m_light9.Ambient.b));
    Logger::debug(str::format("   Position:     ", m_light9.Position.x, " ", m_light9.Position.y, " ", m_light9.Position.z));
    Logger::debug(str::format("   Direction:    ", m_light9.Direction.x, " ", m_light9.Direction.y, " ", m_light9.Direction.z));
    Logger::debug(str::format("   Range:        ", m_light9.Range));
    Logger::debug(str::format("   Falloff:      ", m_light9.Falloff));
    Logger::debug(str::format("   Attenuation0: ", m_light9.Attenuation0));
    Logger::debug(str::format("   Attenuation1: ", m_light9.Attenuation1));
    Logger::debug(str::format("   Attenuation2: ", m_light9.Attenuation2));
    Logger::debug(str::format("   Theta:        ", m_light9.Theta));
    Logger::debug(str::format("   Phi:          ", m_light9.Phi));

    // Update the D3D9 light directly if it's actively being used
    if (m_viewport != nullptr && m_viewport->IsCurrentViewport())
      m_viewport->ApplyAndActivateLight(m_lightCount, this);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Light::GetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3D6Light::GetLight");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    // Technically should also accept D3DLIGHT, but prior to D3D5
    if (unlikely(data->dwSize != sizeof(D3DLIGHT2)))
      return DDERR_INVALIDPARAMS;

    memcpy(data, &m_light, sizeof(D3DLIGHT2));

    return D3D_OK;
  }

}

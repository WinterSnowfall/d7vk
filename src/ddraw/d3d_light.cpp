#include "d3d_light.h"

#include "d3d3/d3d3_viewport.h"
#include "d3d5/d3d5_viewport.h"
#include "d3d6/d3d6_viewport.h"

namespace dxvk {

  uint32_t D3DLight::s_lightCount = 0;

  D3DLight::D3DLight() {
    m_lightCount = ++s_lightCount;

    Logger::debug(str::format("D3DLight: Created a new light nr. [[1-", m_lightCount, "]]"));
  }

  D3DLight::~D3DLight() {
    Logger::debug(str::format("D3DLight: Light nr. [[1-", m_lightCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3DLight::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">> D3DLight::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DLight))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3DLight::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "The method returns DDERR_ALREADYINITIALIZED because
  // the Direct3DLight object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3DLight::Initialize(IDirect3D *d3d) {
    Logger::debug(">>> D3DLight::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3DLight::SetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3DLight::SetLight");

    static constexpr D3DCOLORVALUE noLight = {{0.0f}, {0.0f}, {0.0f}, {0.0f}};

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!data->dwSize))
      return DDERR_INVALIDPARAMS;

    // Hidden & Dangeous spams a lot of parallel point lights
    if (unlikely(data->dltType == D3DLIGHT_PARALLELPOINT)) {
      static bool s_parallelPointErrorShown;

      if (!std::exchange(s_parallelPointErrorShown, true))
        Logger::warn("D3DLight::SetLight: Unsupported light type D3DLIGHT_PARALLELPOINT");

      return DDERR_INVALIDPARAMS;
    }
    // Specific to D3D3
    if (unlikely(data->dltType == D3DLIGHT_GLSPOT)) {
      static bool s_glSpotErrorShown;

      if (!std::exchange(s_glSpotErrorShown, true))
        Logger::warn("D3DLight::SetLight: Unsupported light type D3DLIGHT_GLSPOT");

      return DDERR_INVALIDPARAMS;
    }

    m_isD3DLight2 = data->dwSize == sizeof(D3DLIGHT2);

    const bool hasSpecular = m_isD3DLight2 ? !(reinterpret_cast<D3DLIGHT2*>(data)->dwFlags & D3DLIGHT_NO_SPECULAR) : true;

    // Docs: "Although this method's declaration specifies the lpLight parameter as being
    // the address of a D3DLIGHT structure, that structure is not normally used. Rather,
    // the D3DLIGHT2 structure is recommended to achieve the best lighting effects."
    m_light9.Type         = d3d9::D3DLIGHTTYPE(data->dltType);
    m_light9.Diffuse      = data->dcvColor;
    m_light9.Specular     = hasSpecular ? data->dcvColor : noLight;
    // Ambient light comes from the material
    m_light9.Ambient      = noLight;
    m_light9.Position     = data->dvPosition;
    m_light9.Direction    = data->dvDirection;
    m_light9.Range        = data->dvRange;
    m_light9.Falloff      = data->dvFalloff;
    m_light9.Attenuation0 = data->dvAttenuation0;
    m_light9.Attenuation1 = data->dvAttenuation1;
    m_light9.Attenuation2 = data->dvAttenuation2;
    m_light9.Theta        = data->dvTheta;
    m_light9.Phi          = data->dvPhi;
    // D3DLIGHT structure lights are, apparently, considered to be active by default
    m_isActive            = m_isD3DLight2 ? (reinterpret_cast<D3DLIGHT2*>(data)->dwFlags & D3DLIGHT_ACTIVE) : true;

    Logger::debug(str::format(">>> D3DLight::SetLight: Updated light nr. ", m_lightCount));
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
    if (m_viewport6 != nullptr && m_viewport6->GetCommonViewport()->IsCurrentViewport())
      m_viewport6->ApplyAndActivateLight(m_lightCount, this);
    else if (m_viewport5 != nullptr && m_viewport5->GetCommonViewport()->IsCurrentViewport())
      m_viewport5->ApplyAndActivateLight(m_lightCount, this);
    else if (m_viewport3 != nullptr && m_viewport3->GetCommonViewport()->IsCurrentViewport())
      m_viewport3->ApplyAndActivateLight(m_lightCount, this);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DLight::GetLight(D3DLIGHT *data) {
    Logger::debug(">>> D3DLight::GetLight");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    data->dltType         = D3DLIGHTTYPE(m_light9.Type);
    data->dcvColor        = m_light9.Diffuse;
    //data->dcvColor      = m_light9.Specular;
    data->dvPosition      = m_light9.Position;
    data->dvDirection     = m_light9.Direction;
    data->dvRange         = m_light9.Range;
    data->dvFalloff       = m_light9.Falloff;
    data->dvAttenuation0  = m_light9.Attenuation0;
    data->dvAttenuation1  = m_light9.Attenuation1;
    data->dvAttenuation2  = m_light9.Attenuation2;
    data->dvTheta         = m_light9.Theta;
    data->dvPhi           = m_light9.Phi;

    if (m_isD3DLight2)
      reinterpret_cast<D3DLIGHT2*>(data)->dwFlags = m_isActive;

    return D3D_OK;
  }

}

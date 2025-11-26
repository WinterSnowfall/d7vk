#include "ddraw7_gamma.h"

#include "d3d7_device.h"

namespace dxvk {

  DDraw7GammaControl::DDraw7GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw7Surface* pParent)
    : DDrawWrappedObject<DDraw7Surface, IDirectDrawGammaControl, IUnknown>(pParent, std::move(proxyGamma), nullptr) {
    Logger::debug("DDraw7GammaControl: Created a new gamma control interface");
  }

  DDraw7GammaControl::~DDraw7GammaControl() {
    Logger::debug("DDraw7GammaControl: A gamma control interface bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw7Surface, IDirectDrawGammaControl, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("IDirectDrawGammaControl::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("IDirectDrawGammaControl::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDraw7GammaControl::GetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    RefreshD3D7Device();
    // For proxied pesentation we need to rely on ddraw to handle gamma
    if (likely(m_d3d7Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
      Logger::debug("DDraw7GammaControl::GetGammaRamp: Getting gamma ramp via D3D9");
      d3d9::D3DGAMMARAMP rampData = { };
      m_d3d7Device->GetD3D9()->GetGammaRamp(0, &rampData);
      // Both gamma structs are identical in content/size
      memcpy(static_cast<void*>(lpRampData), static_cast<const void*>(&rampData), sizeof(DDGAMMARAMP));
    } else {
      Logger::debug("DDraw7GammaControl::GetGammaRamp: Getting gamma ramp via DDraw");
      return m_proxy->GetGammaRamp(dwFlags, lpRampData);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDraw7GammaControl::SetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(!m_parent->GetOptions()->ignoreGammaRamp)) {
      RefreshD3D7Device();
      // For proxied pesentation we need to rely on ddraw to handle gamma
      if (likely(m_d3d7Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
        Logger::debug("DDraw7GammaControl::SetGammaRamp: Setting gamma ramp via D3D9");
        m_d3d7Device->GetD3D9()->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
                                              reinterpret_cast<const d3d9::D3DGAMMARAMP*>(lpRampData));
      } else {
        Logger::debug("DDraw7GammaControl::SetGammaRamp: Setting gamma ramp via DDraw");
        return m_proxy->SetGammaRamp(dwFlags, lpRampData);
      }
    }

    return DD_OK;
  }

}

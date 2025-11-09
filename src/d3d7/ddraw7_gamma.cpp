#include "ddraw7_gamma.h"

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
    Logger::debug("<<< DDraw7GammaControl::GetGammaRamp: Proxy");
    return m_proxy->GetGammaRamp(dwFlags, lpRampData);
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug("<<< DDraw7GammaControl::SetGammaRamp: Proxy");

    if (likely(!m_parent->GetOptions()->ignoreGammaRamp))
      return m_proxy->SetGammaRamp(dwFlags, lpRampData);

    return DD_OK;
  }

}

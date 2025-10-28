#include "ddraw7_gamma.h"

namespace dxvk {

  DDraw7GammaControl::DDraw7GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma)
    : m_proxy  ( std::move(proxyGamma) ) {
    Logger::debug("DDraw7GammaControl: Created a new gamma control interface");
  }

  DDraw7GammaControl::~DDraw7GammaControl() {
    Logger::debug("DDraw7GammaControl: A gamma control interface bites the dust");
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7GammaControl::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirectDrawGammaControl)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDraw7GammaControl::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug("<< DDraw7GammaControl::GetGammaRamp: Proxy");
    return m_proxy->GetGammaRamp(dwFlags, lpRampData);
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug("<< DDraw7GammaControl::SetGammaRamp: Proxy");
    // TODO: Add a config option to enable or disable
    // application-set gamma ramps. Ignore them for now.
    //return m_proxy->GetGammaRamp(dwFlags, lpRampData);
    return DD_OK;
  }

}

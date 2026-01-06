#include "ddraw4_gamma.h"

namespace dxvk {

  DDraw4GammaControl::DDraw4GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw4Surface* pParent)
    : DDrawWrappedObject<DDraw4Surface, IDirectDrawGammaControl, IUnknown>(pParent, std::move(proxyGamma), nullptr) {
    Logger::debug("DDraw4GammaControl: Created a new gamma control interface");
  }

  DDraw4GammaControl::~DDraw4GammaControl() {
    Logger::debug("DDraw4GammaControl: A gamma control interface bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Surface, IDirectDrawGammaControl, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw4GammaControl::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDraw4GammaControl::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw4GammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw4GammaControl::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("DDraw4GammaControl::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("DDraw4GammaControl::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("DDraw4GammaControl::QueryInterface: Query for IDirectDrawSurface3");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug("DDraw4GammaControl::QueryInterface: Query for IDirectDrawSurface4");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug("DDraw4GammaControl::QueryInterface: Query for IDirectDrawSurface7");
      return m_parent->QueryInterface(riid, ppvObject);
    }

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw4GammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDraw4GammaControl::GetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    RefreshD3D6Device();
    // For proxied pesentation we need to rely on ddraw to handle gamma
    if (likely(m_d3d6Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
      Logger::debug("DDraw4GammaControl::GetGammaRamp: Getting gamma ramp via D3D9");
      d3d9::D3DGAMMARAMP rampData = { };
      m_d3d6Device->GetD3D9()->GetGammaRamp(0, &rampData);
      // Both gamma structs are identical in content/size
      memcpy(static_cast<void*>(lpRampData), static_cast<const void*>(&rampData), sizeof(DDGAMMARAMP));
    } else {
      Logger::debug("DDraw4GammaControl::GetGammaRamp: Getting gamma ramp via DDraw");
      return m_proxy->GetGammaRamp(dwFlags, lpRampData);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4GammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDraw4GammaControl::SetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(!m_parent->GetOptions()->ignoreGammaRamp)) {
      RefreshD3D6Device();
      // For proxied pesentation we need to rely on ddraw to handle gamma
      if (likely(m_d3d6Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
        Logger::debug("DDraw4GammaControl::SetGammaRamp: Setting gamma ramp via D3D9");
        m_d3d6Device->GetD3D9()->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
                                              reinterpret_cast<const d3d9::D3DGAMMARAMP*>(lpRampData));
      } else {
        Logger::debug("DDraw4GammaControl::SetGammaRamp: Setting gamma ramp via DDraw");
        return m_proxy->SetGammaRamp(dwFlags, lpRampData);
      }
    } else {
      Logger::info("DDraw4GammaControl::SetGammaRamp: Ignoring application set gamma ramp");
    }

    return DD_OK;
  }

}

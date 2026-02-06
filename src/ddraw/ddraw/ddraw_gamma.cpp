#include "ddraw_gamma.h"

namespace dxvk {

  DDrawGammaControl::DDrawGammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDrawSurface* pParent)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawGammaControl, IUnknown>(pParent, std::move(proxyGamma), nullptr) {
    Logger::debug("DDrawGammaControl: Created a new gamma control interface");
  }

  DDrawGammaControl::~DDrawGammaControl() {
    Logger::debug("DDrawGammaControl: A gamma control interface bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirectDrawGammaControl, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawGammaControl))
      return this;

    throw DxvkError("DDrawGammaControl::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawGammaControl::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("DDrawGammaControl::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("DDrawGammaControl::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("DDrawGammaControl::QueryInterface: Query for IDirectDrawSurface3");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug("DDrawGammaControl::QueryInterface: Query for IDirectDrawSurface4");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug("DDrawGammaControl::QueryInterface: Query for IDirectDrawSurface7");
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

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDrawGammaControl::GetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    RefreshD3D5Device();
    // For proxied pesentation we need to rely on ddraw to handle gamma
    if (likely(m_d3d5Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
      Logger::debug("DDrawGammaControl::GetGammaRamp: Getting gamma ramp via D3D9");
      d3d9::D3DGAMMARAMP rampData = { };
      m_d3d5Device->GetD3D9()->GetGammaRamp(0, &rampData);
      // Both gamma structs are identical in content/size
      memcpy(static_cast<void*>(lpRampData), static_cast<const void*>(&rampData), sizeof(DDGAMMARAMP));
    } else {
      Logger::debug("DDrawGammaControl::GetGammaRamp: Getting gamma ramp via DDraw");
      return m_proxy->GetGammaRamp(dwFlags, lpRampData);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDrawGammaControl::SetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(!m_parent->GetOptions()->ignoreGammaRamp)) {
      RefreshD3D5Device();
      // For proxied pesentation we need to rely on ddraw to handle gamma
      if (likely(m_d3d5Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
        Logger::debug("DDrawGammaControl::SetGammaRamp: Setting gamma ramp via D3D9");
        m_d3d5Device->GetD3D9()->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
                                              reinterpret_cast<const d3d9::D3DGAMMARAMP*>(lpRampData));
      } else {
        Logger::debug("DDrawGammaControl::SetGammaRamp: Setting gamma ramp via DDraw");
        return m_proxy->SetGammaRamp(dwFlags, lpRampData);
      }
    } else {
      Logger::info("DDrawGammaControl::SetGammaRamp: Ignoring application set gamma ramp");
    }

    return DD_OK;
  }

}

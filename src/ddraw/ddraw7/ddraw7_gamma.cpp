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
    if (riid == __uuidof(IDirectDrawGammaControl))
      return this;

    throw DxvkError("DDraw7GammaControl::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDraw7GammaControl::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("DDraw7GammaControl::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("DDraw7GammaControl::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("DDraw7GammaControl::QueryInterface: Query for IDirectDrawSurface3");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug("DDraw7GammaControl::QueryInterface: Query for IDirectDrawSurface4");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug("DDraw7GammaControl::QueryInterface: Query for IDirectDrawSurface7");
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

  HRESULT STDMETHODCALLTYPE DDraw7GammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    Logger::debug(">>> DDraw7GammaControl::GetGammaRamp");

    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* d3d9Device = m_parent->GetCommonInterface()->GetD3D9Device();
    // For proxied pesentation we need to rely on ddraw to handle gamma
    if (likely(d3d9Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
      Logger::debug("DDraw7GammaControl::GetGammaRamp: Getting gamma ramp via D3D9");
      d3d9::D3DGAMMARAMP rampData = { };
      d3d9Device->GetGammaRamp(0, &rampData);
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
      d3d9::IDirect3DDevice9* d3d9Device = m_parent->GetCommonInterface()->GetD3D9Device();
      // For proxied pesentation we need to rely on ddraw to handle gamma
      if (likely(d3d9Device != nullptr && !m_parent->GetOptions()->forceProxiedPresent)) {
        Logger::debug("DDraw7GammaControl::SetGammaRamp: Setting gamma ramp via D3D9");
        d3d9Device->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
                                 reinterpret_cast<const d3d9::D3DGAMMARAMP*>(lpRampData));
      } else {
        Logger::debug("DDraw7GammaControl::SetGammaRamp: Setting gamma ramp via DDraw");
        return m_proxy->SetGammaRamp(dwFlags, lpRampData);
      }
    } else {
      Logger::info("DDraw7GammaControl::SetGammaRamp: Ignoring application set gamma ramp");
    }

    return DD_OK;
  }

}

#include "ddraw_gamma.h"

#include "d3d_common_device.h"

namespace dxvk {

  DDrawGammaControl::DDrawGammaControl(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawGammaControl>&& proxyGamma,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawGammaControl, IUnknown>(pParent, std::move(proxyGamma), nullptr)
    , m_commonSurf ( commonSurf ) {
    Logger::debug("DDrawGammaControl: Created a new gamma control interface");
  }

  DDrawGammaControl::~DDrawGammaControl() {
    Logger::debug("DDrawGammaControl: A gamma control interface bites the dust");
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawGammaControl::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
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

    DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

    D3DCommonDevice* commonDevice = commonIntf->GetCommonD3DDevice();
    // For proxied pesentation we need to rely on ddraw to handle gamma
    if (likely(commonDevice != nullptr && !commonIntf->GetOptions()->forceProxiedPresent)) {
      Logger::debug("DDrawGammaControl::GetGammaRamp: Getting gamma ramp via D3D9");

      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      d3d9::D3DGAMMARAMP rampData = { };
      d3d9Device->GetGammaRamp(0, &rampData);

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

    DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

    if (likely(!commonIntf->GetOptions()->ignoreGammaRamp)) {
      D3DCommonDevice* commonDevice = commonIntf->GetCommonD3DDevice();
      // For proxied pesentation we need to rely on ddraw to handle gamma
      if (likely(commonDevice != nullptr && !commonIntf->GetOptions()->forceProxiedPresent)) {
        Logger::debug("DDrawGammaControl::SetGammaRamp: Setting gamma ramp via D3D9");

        d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

        d3d9Device->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
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

#include "ddraw_gamma.h"

#include "d3d_common_device.h"

namespace dxvk {

  DDrawGammaControl::DDrawGammaControl(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawGammaControl>&& proxyGamma,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawGammaControl>(pParent, std::move(proxyGamma))
    , m_commonSurf ( commonSurf ) {
  }

  DDrawGammaControl::~DDrawGammaControl() {
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IDirectDrawGammaControl))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawGammaControl::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_isGammaRampSet)) {
      DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

      D3DCommonDevice* commonD3DDevice = commonIntf->GetCommonD3DDevice();

      if (likely(!commonIntf->GetOptions()->forceProxiedGammaRamp && commonD3DDevice != nullptr)) {
        d3d9::IDirect3DDevice9* d3d9Device = commonD3DDevice->GetD3D9Device();

        d3d9Device->GetGammaRamp(0, reinterpret_cast<d3d9::D3DGAMMARAMP*>(&m_gammaRamp));
      } else {
        HRESULT hr = m_proxy->GetGammaRamp(dwFlags, &m_gammaRamp);
        if (unlikely(FAILED(hr)))
          return hr;
      }
    }

    *lpRampData = m_gammaRamp;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

    const D3DOptions* d3dOptions = commonIntf->GetOptions();

    if (likely(!d3dOptions->ignoreGammaRamp)) {
      D3DCommonDevice* commonD3DDevice = commonIntf->GetCommonD3DDevice();

      if (likely(!d3dOptions->forceProxiedGammaRamp && commonD3DDevice != nullptr)) {
        // Skip setting the gamma ramp on a non-primary surface or if we aren't in a full screen mode
        if (likely(m_commonSurf->IsPrimarySurface()
               && (commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE))) {
          d3d9::IDirect3DDevice9* d3d9Device = commonD3DDevice->GetD3D9Device();

          d3d9Device->SetGammaRamp(0, ConvertGammaRampFlags(dwFlags),
                                   reinterpret_cast<d3d9::D3DGAMMARAMP*>(lpRampData));
        }
      } else {
        HRESULT hr = m_proxy->SetGammaRamp(dwFlags, lpRampData);
        if (unlikely(FAILED(hr)))
          return hr;
      }
    }

    m_gammaRamp = *lpRampData;
    m_isGammaRampSet = true;

    return DD_OK;
  }

}

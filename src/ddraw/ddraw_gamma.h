#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"
#include "ddraw_util.h"

namespace dxvk {

  class DDrawCommonSurface;

  class DDrawGammaControl final : public DDrawWrappedObject<IUnknown, IDirectDrawGammaControl> {

  public:

    DDrawGammaControl(
         DDrawCommonSurface* commonSurf,
         Com<IDirectDrawGammaControl>&& proxyGamma,
         IUnknown* pParent);

    ~DDrawGammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    bool                m_isGammaRampSet = false;

    DDGAMMARAMP         m_gammaRamp      = { };

    DDrawCommonSurface* m_commonSurf     = nullptr;

  };

}

#pragma once

#include "d3d7_include.h"

namespace dxvk {

  class DDraw7GammaControl final : public ComObjectClamp<IDirectDrawGammaControl> {

  public:
    DDraw7GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma);

    ~DDraw7GammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    Com<IDirectDrawGammaControl> m_proxy;

  };

}

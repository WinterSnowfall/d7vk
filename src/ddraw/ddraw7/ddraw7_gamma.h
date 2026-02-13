#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw7_surface.h"

namespace dxvk {

  class DDraw7GammaControl final : public DDrawWrappedObject<DDraw7Surface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDraw7GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw7Surface* pParent);

    ~DDraw7GammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  };

}

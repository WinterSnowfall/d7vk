#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw4_surface.h"

namespace dxvk {

  class DDraw4GammaControl final : public DDrawWrappedObject<DDraw4Surface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDraw4GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw4Surface* pParent);

    ~DDraw4GammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  };

}

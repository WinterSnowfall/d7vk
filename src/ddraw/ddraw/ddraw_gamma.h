#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw_surface.h"

namespace dxvk {

  class DDrawGammaControl final : public DDrawWrappedObject<DDrawSurface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDrawGammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDrawSurface* pParent);

    ~DDrawGammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  };

}

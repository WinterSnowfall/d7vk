#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw_surface.h"

#include "../d3d5/d3d5_device.h"

namespace dxvk {

  class DDrawGammaControl final : public DDrawWrappedObject<DDrawSurface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDrawGammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDrawSurface* pParent);

    ~DDrawGammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    inline void RefreshD3D5Device() {
      D3D5Device* d3d5Device = m_parent->GetD3D5Device();
      if (unlikely(m_d3d5Device != d3d5Device))
        m_d3d5Device = d3d5Device;
    }

    D3D5Device* m_d3d5Device = nullptr;

  };

}

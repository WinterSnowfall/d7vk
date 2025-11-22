#pragma once

#include "d3d7_include.h"
#include "ddraw7_surface.h"

namespace dxvk {

  class D3D7Device;

  class DDraw7GammaControl final : public DDrawWrappedObject<DDraw7Surface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDraw7GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw7Surface* pParent);

    ~DDraw7GammaControl();

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    inline void RefreshD3D7Device() {
      m_d3d7Device = m_parent->GetD3D7Device();
    }

    D3D7Device* m_d3d7Device = nullptr;

  };

}

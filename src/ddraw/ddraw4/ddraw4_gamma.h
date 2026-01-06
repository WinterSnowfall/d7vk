#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw4_surface.h"

#include "../d3d6/d3d6_device.h"

namespace dxvk {

  class DDraw4GammaControl final : public DDrawWrappedObject<DDraw4Surface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDraw4GammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw4Surface* pParent);

    ~DDraw4GammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    inline void RefreshD3D6Device() {
      D3D6Device* d3d6Device = m_parent->GetD3D6Device();
      if (unlikely(m_d3d6Device != d3d6Device))
        m_d3d6Device = d3d6Device;
    }

    D3D6Device* m_d3d6Device = nullptr;

  };

}

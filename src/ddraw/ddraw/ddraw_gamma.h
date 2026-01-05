#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw7/ddraw7_surface.h"

#include "../d3d7/d3d7_device.h"

namespace dxvk {

  class DDrawGammaControl final : public DDrawWrappedObject<DDraw7Surface, IDirectDrawGammaControl, IUnknown> {

  public:

    DDrawGammaControl(Com<IDirectDrawGammaControl>&& proxyGamma, DDraw7Surface* pParent);

    ~DDrawGammaControl();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

    HRESULT STDMETHODCALLTYPE SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData);

  private:

    inline void RefreshD3D7Device() {
      D3D7Device* d3d7Device = m_parent->GetD3D7Device();
      if (unlikely(m_d3d7Device != d3d7Device))
        m_d3d7Device = d3d7Device;
    }

    D3D7Device* m_d3d7Device = nullptr;

  };

}

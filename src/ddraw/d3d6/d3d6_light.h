#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d6_interface.h"
#include "d3d6_viewport.h"

namespace dxvk {

  class D3D6Light final : public DDrawWrappedObject<D3D6Interface, IDirect3DLight, IUnknown> {

  public:

    D3D6Light(Com<IDirect3DLight>&& proxyLight, D3D6Interface* pParent);

    ~D3D6Light();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

    HRESULT STDMETHODCALLTYPE SetLight(D3DLIGHT *data);

    HRESULT STDMETHODCALLTYPE GetLight(D3DLIGHT *data);

    const d3d9::D3DLIGHT9* GetD3D9Light() const {
      return &m_light9;
    }

    void SetViewport(D3D6Viewport* viewport) {
      m_viewport = viewport;
    }

    bool HasViewport() const {
      return m_viewport != nullptr;
    }

    DWORD GetIndex() const {
      return m_lightCount;
    }

    bool IsActive() const {
      return m_light.dwFlags & D3DLIGHT_ACTIVE;
    }

  private:

    inline bool HasSpecular() const {
      return !(m_light.dwFlags & D3DLIGHT_NO_SPECULAR);
    }

    static uint32_t  s_lightCount;
    uint32_t         m_lightCount = 0;

    D3D6Viewport*    m_viewport = nullptr;

    D3DLIGHT2        m_light = { };

    DWORD            m_index9 = 0;

    d3d9::D3DLIGHT9  m_light9 = { };

  };

}

#pragma once

#include "ddraw_include.h"
#include "ddraw_child_object.h"

namespace dxvk {

  class D3DViewport;

  class D3DLight final : public DDrawChildObject<IUnknown, IDirect3DLight> {

  public:

    D3DLight(IUnknown* pParent);

    ~D3DLight();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

    HRESULT STDMETHODCALLTYPE SetLight(D3DLIGHT *data);

    HRESULT STDMETHODCALLTYPE GetLight(D3DLIGHT *data);

    const d3d9::D3DLIGHT9* GetD3D9Light() const {
      return &m_light9;
    }

    void SetViewport(D3DViewport* viewport) {
      m_viewport = viewport;
    }

    bool HasViewport() const {
      return m_viewport != nullptr;
    }

    bool IsActive() const {
      return m_isActive;
    }

    DWORD GetIndex() {
      if (unlikely(!m_light9Index))
        m_light9Index = ++s_light9Index;

      return m_light9Index;
    }

  private:

    bool             m_isActive        = false;
    bool             m_isParallelPoint = false;

    DWORD            m_flags           = 0;

    D3DViewport*     m_viewport        = nullptr;

    d3d9::D3DLIGHT9  m_light9          = { };

    uint32_t         m_light9Index     = 0;
    static std::atomic<uint32_t> s_light9Index;

  };

}

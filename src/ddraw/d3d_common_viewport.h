#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class D3D6Viewport;
  class D3D5Viewport;
  class D3D3Viewport;

  class D3DCommonViewport : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonViewport();

    ~D3DCommonViewport();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    d3d9::D3DVIEWPORT9* GetD3D9Viewport() {
      return &m_viewport9;
    }

    void MarkViewportAsSet() {
      m_isViewportSet = true;
    }

    bool IsViewportSet() const {
      return m_isViewportSet;
    }

    void MarkMaterialAsSet() {
      m_isMaterialSet = true;
    }

    bool IsMaterialSet() const {
      return m_isMaterialSet;
    }

    void SetMaterialHandle(D3DMATERIALHANDLE materialHandle) {
      m_materialHandle = materialHandle;
    }

    D3DMATERIALHANDLE GetMaterialHandle() const {
      return m_materialHandle;
    }

    void SetBackgroundColor(D3DCOLOR backgroundColor) {
      m_backgroundColor = backgroundColor;
    }

    D3DCOLOR GetBackgroundColor() const {
      return m_backgroundColor;
    }

    void SetIsCurrentViewport(bool isCurrentViewport) {
      m_isCurrentViewport = isCurrentViewport;
    }

    bool IsCurrentViewport() const {
      return m_isCurrentViewport;
    }

    void SetD3D6Viewport(D3D6Viewport* d3d6Viewport) {
      m_d3d6Viewport = d3d6Viewport;
    }

    D3D6Viewport* GetD3D6Viewport() const {
      return m_d3d6Viewport;
    }

    void SetD3D5Viewport(D3D5Viewport* d3d5Viewport) {
      m_d3d5Viewport = d3d5Viewport;
    }

    D3D5Viewport* GetD3D5Viewport() const {
      return m_d3d5Viewport;
    }

    void SetD3D3Viewport(D3D3Viewport* d3d3Viewport) {
      m_d3d3Viewport = d3d3Viewport;
    }

    D3D3Viewport* GetD3D3Viewport() const {
      return m_d3d3Viewport;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

    bool IsOrigin(IUnknown* origin) const {
      return m_origin == origin;
    }

  private:

    bool               m_isViewportSet     = false;
    bool               m_isCurrentViewport = false;
    bool               m_isMaterialSet     = false;

    D3DMATERIALHANDLE  m_materialHandle    = 0;
    D3DCOLOR           m_backgroundColor   = D3DCOLOR_RGBA(0, 0, 0, 0);

    d3d9::D3DVIEWPORT9 m_viewport9 = { };

    // Track all possible viewport versions of the same object
    D3D6Viewport*      m_d3d6Viewport      = nullptr;
    D3D5Viewport*      m_d3d5Viewport      = nullptr;
    D3D3Viewport*      m_d3d3Viewport      = nullptr;

    IUnknown*          m_origin            = nullptr;

  };

}
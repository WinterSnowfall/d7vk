#pragma once

#include "ddraw_include.h"

#include "d3d_common_material.h"

namespace dxvk {

  class D3D7Interface;
  class D3D6Interface;
  class D3D5Interface;
  class D3D3Interface;

  class D3DCommonMaterial;

  class D3DCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonInterface();

    ~D3DCommonInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    d3d9::D3DMATERIAL9* GetD3D9MaterialFromHandle(D3DMATERIALHANDLE handle) const;

    D3DCommonMaterial* GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle) const;

    D3DMATERIALHANDLE GetNextMaterialHandle() {
      return ++m_materialHandle;
    }

    void SetD3D7Interface(D3D7Interface* d3d7Intf) {
      m_d3d7Intf = d3d7Intf;
    }

    D3D7Interface* GetD3D7Interface() const {
      return m_d3d7Intf;
    }

    void SetD3D6Interface(D3D6Interface* d3d6Intf) {
      m_d3d6Intf = d3d6Intf;
    }

    D3D6Interface* GetD3D6Interface() const {
      return m_d3d6Intf;
    }

    void SetD3D5Interface(D3D5Interface* d3d5Intf) {
      m_d3d5Intf = d3d5Intf;
    }

    D3D5Interface* GetD3D5Interface() const {
      return m_d3d5Intf;
    }

    void SetD3D3Interface(D3D3Interface* d3d3Intf) {
      m_d3d3Intf = d3d3Intf;
    }

    D3D3Interface* GetD3D3Interface() const {
      return m_d3d3Intf;
    }

  private:

    // Track all possible last used D3D interfaces
    D3D7Interface*                    m_d3d7Intf           = nullptr;
    D3D6Interface*                    m_d3d6Intf           = nullptr;
    D3D5Interface*                    m_d3d5Intf           = nullptr;
    D3D3Interface*                    m_d3d3Intf           = nullptr;

    D3DMATERIALHANDLE                 m_materialHandle = 0;

  };

}
#pragma once

#include "ddraw_include.h"

#include <unordered_map>

namespace dxvk {

  class DDrawCommonInterface;

  class D3D7Device;
  class D3D6Device;
  class D3D5Device;
  class D3D3Device;

  class D3DCommonDevice : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonDevice(
          DDrawCommonInterface* commonIntf,
          DWORD creationFlags9,
          uint32_t totalMemory);

    ~D3DCommonDevice();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    DWORD GetD3D9CreationFlags() const {
      return m_creationFlags9;
    }

    void SetD3D7Device(D3D7Device* d3d7Device) {
      m_d3d7Device = d3d7Device;
    }

    D3D7Device* GetD3D7Device() const {
      return m_d3d7Device;
    }

    void SetD3D6Device(D3D6Device* d3d6Device) {
      m_d3d6Device = d3d6Device;
    }

    D3D6Device* GetD3D6Device() const {
      return m_d3d6Device;
    }

    void SetD3D5Device(D3D5Device* d3d5Device) {
      m_d3d5Device = d3d5Device;
    }

    D3D5Device* GetD3D5Device() const {
      return m_d3d5Device;
    }

    void SetD3D3Device(D3D3Device* d3d3Device) {
      m_d3d3Device = d3d3Device;
    }

    D3D3Device* GetD3D3Device() const {
      return m_d3d3Device;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

    uint32_t GetTotalTextureMemory() const {
      return m_totalMemory;
    }

  private:

    uint32_t              m_totalMemory    = 0;

    DDrawCommonInterface* m_commonIntf     = nullptr;

    DWORD                 m_creationFlags9 = 0;

    // Track all possible last used D3D devices
    D3D7Device*           m_d3d7Device     = nullptr;
    D3D6Device*           m_d3d6Device     = nullptr;
    D3D5Device*           m_d3d5Device     = nullptr;
    D3D3Device*           m_d3d3Device     = nullptr;

    // Track the origin device, as in the device
    // that gets created through a CreateDevice call
    IUnknown*             m_origin         = nullptr;

  };

}
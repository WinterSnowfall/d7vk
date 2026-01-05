#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"

#include "../../d3d9/d3d9_bridge.h"

#include <vector>

namespace dxvk {

  class DDraw7Interface;
  class D3D7Device;

  /**
  * \brief D3D7 interface implementation
  */
  class D3D7Interface final : public DDrawWrappedObject<DDraw7Interface, IDirect3D7, d3d9::IDirect3D9> {

  public:
    D3D7Interface(Com<IDirect3D7>&& d3d7Intf, DDraw7Interface* pParent);

    ~D3D7Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx);

    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage);

    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID device_iid, LPD3DENUMPIXELFORMATSCALLBACK cb, LPVOID ctx);

    HRESULT STDMETHODCALLTYPE EvictManagedTextures();

    D3D7Device* GetLastUsedDevice() const {
      return m_lastUsedDevice;
    }

    void SetLastUsedDevice(D3D7Device* device) {
      m_lastUsedDevice = device;
    }

    const D3DOptions* GetOptions() const {
      return &m_options;
    }

  private:

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    D3DOptions                    m_options;

    D3D7Device*                   m_lastUsedDevice = nullptr;

  };

}
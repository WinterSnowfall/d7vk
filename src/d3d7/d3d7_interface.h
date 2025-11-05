#pragma once

#include "d3d7_include.h"
#include "d3d7_options.h"
#include "ddraw7_wrapped_object.h"
#include "../d3d9/d3d9_bridge.h"

#include <vector>

namespace dxvk {

  class DDraw7Interface;
  class D3D7Device;

  /**
  * \brief D3D7 interface implementation
  *
  * Implements the IDirect3D7 interfaces
  */
  class D3D7Interface final : public DDrawWrappedObject<d3d9::IDirect3D9, IDirect3D7> {

  public:
    D3D7Interface(Com<IDirect3D7>&& d3d7Intf, DDraw7Interface* pParent);

    ~D3D7Interface();

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx);

    HRESULT STDMETHODCALLTYPE CreateDevice(const IID& rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage);

    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID device_iid, LPD3DENUMPIXELFORMATSCALLBACK cb, LPVOID ctx);

    HRESULT STDMETHODCALLTYPE EvictManagedTextures();

    D3D7Device* GetDevice() const {
      return m_device;
    }

    void ClearDevice() {
      m_device = nullptr;
    }

    const D3D7Options* GetOptions() const {
      return &m_d3d7Options;
    }

  private:
    DDraw7Interface*              m_parent = nullptr;

    D3D7Options                   m_d3d7Options;

    Com<IDirect3D7>               m_d3d7IntfProxy;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    D3D7Device*                   m_device = nullptr;

  };

}
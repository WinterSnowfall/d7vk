#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"

#include "../d3d9/d3d9_bridge.h"

namespace dxvk {

  class DDraw2Interface;

  /**
  * \brief D3D5 interface implementation
  */
  class D3D5Interface final : public DDrawWrappedObject<DDraw2Interface, IDirect3D2, d3d9::IDirect3D9> {

  public:
    D3D5Interface(Com<IDirect3D2>&& d3d5Intf, DDraw2Interface* pParent);

    ~D3D5Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg);

    HRESULT STDMETHODCALLTYPE CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateMaterial(LPDIRECT3DMATERIAL2 *lplpDirect3DMaterial, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateViewport(LPDIRECT3DVIEWPORT2 *lplpD3DViewport, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR);

    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE lpDDS, LPDIRECT3DDEVICE2 *lplpD3DDevice);

    const D3DOptions* GetOptions() const {
      return &m_options;
    }

  private:

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    D3DOptions                    m_options;

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_format.h"

#include "../d3d9/d3d9_bridge.h"

#include "../d3d7/d3d7_options.h"

namespace dxvk {

  class DDrawInterface;

  /**
  * \brief D3D6 interface implementation
  */
  class D3D6Interface final : public DDrawWrappedObject<DDrawInterface, IDirect3D3, d3d9::IDirect3D9> {

  public:
    D3D6Interface(Com<IDirect3D3>&& d3d6Intf, DDrawInterface* pParent);

    ~D3D6Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg);

    HRESULT STDMETHODCALLTYPE CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter);
    
    HRESULT STDMETHODCALLTYPE CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter);
    
    HRESULT STDMETHODCALLTYPE CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter);
    
    HRESULT STDMETHODCALLTYPE FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR);

    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(LPD3DVERTEXBUFFERDESC lpVBDesc, LPDIRECT3DVERTEXBUFFER *lpD3DVertexBuffer, DWORD dwFlags, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext);

    HRESULT STDMETHODCALLTYPE EvictManagedTextures();

    const D3D7Options* GetOptions() const {
      return &m_d3d7Options;
    }

  private:

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    D3D7Options                   m_d3d7Options;

  };

}
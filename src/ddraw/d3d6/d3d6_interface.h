#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_format.h"

#include "ddraw_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

#include <unordered_map>

namespace dxvk {

  class DDraw4Interface;
  class D3D6Device;
  class D3D6Material;

  /**
  * \brief D3D6 interface implementation
  */
  class D3D6Interface final : public DDrawWrappedObject<DDraw4Interface, IDirect3D3, d3d9::IDirect3D9> {

  public:
    D3D6Interface(Com<IDirect3D3>&& d3d6Intf, DDraw4Interface* pParent);

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

    D3D6Material* GetMaterialFromHandle(D3DMATERIALHANDLE handle);

    const D3DOptions* GetOptions() const {
      return &m_options;
    }

  private:

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    D3DOptions                    m_options;

    D3DMATERIALHANDLE             m_materialHandle = 0;
    std::unordered_map<D3DMATERIALHANDLE, D3D6Material> m_materials;

  };

}
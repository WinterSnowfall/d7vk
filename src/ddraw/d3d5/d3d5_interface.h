#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_format.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

#include <unordered_map>

namespace dxvk {

  class DDrawInterface;
  class D3D5Material;

  /**
  * \brief D3D5 interface implementation
  */
  class D3D5Interface final : public DDrawWrappedObject<DDrawInterface, IDirect3D2, d3d9::IDirect3D9> {

  public:
    D3D5Interface(D3DCommonInterface* commonD3DIntf, Com<IDirect3D2>&& d3d5Intf, DDrawInterface* pParent);

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

    D3D5Material* GetMaterialFromHandle(D3DMATERIALHANDLE handle);

    void ReleaseMaterialHandle(D3DMATERIALHANDLE handle);

    D3DCommonInterface* GetCommonD3DInterface() const {
      return m_commonD3DIntf.ptr();
    }

    const D3DOptions* GetOptions() const {
      return m_commonD3DIntf->GetOptions();
    }

  private:

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    Com<D3DCommonInterface>       m_commonD3DIntf;

    std::unordered_map<D3DMATERIALHANDLE, D3D5Material*> m_materials;

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_format.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

namespace dxvk {

  /**
  * \brief D3D5 interface implementation
  */
  class D3D5Interface final : public DDrawWrappedObject<IUnknown, IDirect3D2> {

  public:
    D3D5Interface(
          DDrawCommonInterface* commonIntf,
          D3DCommonInterface* commonD3DIntf,
          Com<IDirect3D2>&& d3d5Intf,
          IUnknown* pParent);

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

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    D3DCommonInterface* GetCommonD3DInterface() const {
      return m_commonD3DIntf.ptr();
    }

  private:

    inline DWORD DetermineBackBufferCount(IDirectDrawSurface* renderTarget);

    static uint32_t               s_intfCount;
    uint32_t                      m_intfCount  = 0;

    Com<IDxvkD3D8InterfaceBridge> m_bridge;

    DDrawCommonInterface*         m_commonIntf = nullptr;

    Com<D3DCommonInterface>       m_commonD3DIntf;

  };

}
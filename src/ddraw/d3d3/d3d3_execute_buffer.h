#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d3_device.h"

namespace dxvk {

  class D3D3ExecuteBuffer final : public DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown> {

  public:

    D3D3ExecuteBuffer(Com<IDirect3DExecuteBuffer>&& buffProxy,
                      D3D3Device* pParent);

    ~D3D3ExecuteBuffer();

    HRESULT STDMETHODCALLTYPE GetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Lock(LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Optimize(DWORD dwUnknown);

    HRESULT STDMETHODCALLTYPE SetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved);


  private:

    static uint32_t       s_buffCount;
    uint32_t              m_buffCount  = 0;

  };

}

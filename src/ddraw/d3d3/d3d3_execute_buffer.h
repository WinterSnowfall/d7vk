#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d3_device.h"

#include <vector>

namespace dxvk {

  class D3D3ExecuteBuffer final : public DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown> {

  public:

    D3D3ExecuteBuffer(
          Com<IDirect3DExecuteBuffer>&& buffProxy,
          D3DEXECUTEBUFFERDESC desc,
          D3D3Device* pParent);

    ~D3D3ExecuteBuffer();

    HRESULT STDMETHODCALLTYPE GetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Lock(LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Optimize(DWORD dwUnknown);

    HRESULT STDMETHODCALLTYPE SetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved);

    std::vector<uint8_t> GetBuffer() const {
      return m_buffer;
    }

    D3DEXECUTEDATA GetExecuteData() const {
      return m_data;
    }

  private:

    bool                  m_locked = false;

    static uint32_t       s_buffCount;
    uint32_t              m_buffCount  = 0;

    D3DEXECUTEBUFFERDESC  m_desc;
    D3DEXECUTEDATA        m_data;
    D3D3Device*           m_D3D3Device = nullptr;

    std::vector<uint8_t>  m_buffer;
  };

}

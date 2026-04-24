#pragma once

#include "../ddraw_include.h"

#include <vector>

namespace dxvk {

  class D3D3ExecuteBuffer final : public ComObjectClamp<IDirect3DExecuteBuffer> {

  public:

    D3D3ExecuteBuffer(D3DEXECUTEBUFFERDESC desc);

    ~D3D3ExecuteBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

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

    void SetExecuteData(D3DEXECUTEDATA data) {
      m_data = data;
    }

  private:

    bool                  m_locked = false;

    static uint32_t       s_buffCount;
    uint32_t              m_buffCount  = 0;

    D3DEXECUTEBUFFERDESC  m_desc;
    D3DEXECUTEDATA        m_data;

    std::vector<uint8_t>  m_buffer;

  };

}

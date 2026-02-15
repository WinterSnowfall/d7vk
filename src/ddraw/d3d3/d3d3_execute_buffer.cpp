#include "d3d3_execute_buffer.h"

namespace dxvk {

  uint32_t D3D3ExecuteBuffer::s_buffCount = 0;

  D3D3ExecuteBuffer::D3D3ExecuteBuffer(
            Com<IDirect3DExecuteBuffer>&& buffProxy,
            D3D3Device* pParent)
    : DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown>(pParent, std::move(buffProxy), nullptr) {
    m_buffCount = ++s_buffCount;

    Logger::debug(str::format("D3D3ExecuteBuffer: Created a new execute buffer nr. {{1-", m_buffCount, "}}:"));
  }

  D3D3ExecuteBuffer::~D3D3ExecuteBuffer() {
    Logger::debug(str::format("D3D3ExecuteBuffer: Execute buffer nr. {{1-", m_buffCount, "}} bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DExecuteBuffer))
      return this;

    Logger::debug("D3D3ExecuteBuffer::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::GetExecuteData(LPD3DEXECUTEDATA lpData) {
    Logger::debug("<<< D3D3ExecuteBuffer::GetExecuteData: Proxy");
    return m_proxy->GetExecuteData(lpData);
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug("<<< D3D3ExecuteBuffer::Initialize: Proxy");
    D3D3Device* d3d3Device = static_cast<D3D3Device*>(lpDirect3DDevice);
    return m_proxy->Initialize(d3d3Device->GetProxied(), lpDesc);
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Lock(LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug("<<< D3D3ExecuteBuffer::Lock: Proxy");
    return m_proxy->Lock(lpDesc);
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Optimize(DWORD dwUnknown) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::SetExecuteData(LPD3DEXECUTEDATA lpData) {
    Logger::debug("<<< D3D3ExecuteBuffer::SetExecuteData: Proxy");
    return m_proxy->SetExecuteData(lpData);
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Unlock() {
    Logger::debug("<<< D3D3ExecuteBuffer::Unlock: Proxy");
    return m_proxy->Unlock();
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved) {
    Logger::debug("<<< D3D3ExecuteBuffer::Validate: Proxy");
    return m_proxy->Validate(lpdwOffset, lpFunc, lpUserArg, dwReserved);
  }

}

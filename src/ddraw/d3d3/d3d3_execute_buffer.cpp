#include "d3d3_execute_buffer.h"

namespace dxvk {

  uint32_t D3D3ExecuteBuffer::s_buffCount = 0;

  D3D3ExecuteBuffer::D3D3ExecuteBuffer(
            Com<IDirect3DExecuteBuffer>&& buffProxy,
            D3DEXECUTEBUFFERDESC desc,
            D3D3Device* pParent)
    : DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown>(pParent, std::move(buffProxy), nullptr)
    , m_desc (desc) {
    m_buffCount = ++s_buffCount;

    Initialize(pParent, &desc);

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
    Logger::debug("<<< D3D3ExecuteBuffer::GetExecuteData");

    if (unlikely(lpData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_buffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    *lpData = m_data;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug("<<< D3D3ExecuteBuffer::Initialize");

    if (unlikely(lpDirect3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDesc == nullptr || lpDesc->dwSize != sizeof(D3DEXECUTEBUFFERDESC)))
      return DDERR_INVALIDPARAMS;

    m_D3D3Device = static_cast<D3D3Device*>(lpDirect3DDevice);

    if (m_buffer.size() == 0 && lpDesc->dwFlags & D3DDEB_BUFSIZE) {
        m_desc = *lpDesc;
        m_buffer.resize(lpDesc->dwBufferSize);

        Logger::debug(str::format("<<< D3D3ExecuteBuffer::Initialize: Buffer is initialized with size ", lpDesc->dwBufferSize));
        return D3D_OK;
    }

    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Lock(LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug("<<< D3D3ExecuteBuffer::Lock");

    if (unlikely(lpDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (m_locked)
      return D3DERR_EXECUTE_LOCKED;

    m_locked = true;
    lpDesc->dwFlags = D3DDEB_BUFSIZE|D3DDEB_LPDATA;
    lpDesc->dwBufferSize = m_buffer.size();
    lpDesc->lpData = m_buffer.data();

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Optimize(DWORD dwUnknown) {
    Logger::warn("<<< D3D3ExecuteBuffer::Optimize: Unsupported");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::SetExecuteData(LPD3DEXECUTEDATA lpData) {
    Logger::debug("<<< D3D3ExecuteBuffer::SetExecuteData");

    if (unlikely(lpData == nullptr || m_buffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    if (lpData->dwInstructionOffset + lpData->dwInstructionLength > m_buffer.size())
      return DDERR_INVALIDPARAMS;

    if (lpData->dwVertexOffset + lpData->dwVertexCount > m_buffer.size())
      return DDERR_INVALIDPARAMS;

    m_data = *lpData;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Unlock() {
    Logger::debug("<<< D3D3ExecuteBuffer::Unlock");

    if (!m_locked)
      return D3DERR_EXECUTE_NOT_LOCKED;

    m_locked = false;

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved) {
    Logger::warn("<<< D3D3ExecuteBuffer::Validate: Unsupported");
    return DDERR_UNSUPPORTED;
  }

}

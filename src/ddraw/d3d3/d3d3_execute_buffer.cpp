#include "d3d3_execute_buffer.h"

namespace dxvk {

  uint32_t D3D3ExecuteBuffer::s_buffCount = 0;

  D3D3ExecuteBuffer::D3D3ExecuteBuffer(
        Com<IDirect3DExecuteBuffer>&& buffProxy,
        D3DEXECUTEBUFFERDESC desc,
        D3D3Device* pParent)
    : DDrawWrappedObject<D3D3Device, IDirect3DExecuteBuffer, IUnknown>(pParent, std::move(buffProxy), nullptr)
    , m_desc (desc) {
    if (likely(m_buffer.size() == 0 && (m_desc.dwFlags & D3DDEB_BUFSIZE))) {
      m_buffer.resize(m_desc.dwBufferSize);
      Logger::debug(str::format("D3D3ExecuteBuffer: Buffer is initialized with size ", m_desc.dwBufferSize));
    }

    m_buffCount = ++s_buffCount;

    Logger::debug(str::format("D3D3ExecuteBuffer: Created a new execute buffer nr. {{1-", m_buffCount, "}}:"));
  }

  D3D3ExecuteBuffer::~D3D3ExecuteBuffer() {
    Logger::debug(str::format("D3D3ExecuteBuffer: Execute buffer nr. {{1-", m_buffCount, "}} bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::GetExecuteData(LPD3DEXECUTEDATA lpData) {
    Logger::debug(">>> D3D3ExecuteBuffer::GetExecuteData");

    if (unlikely(lpData == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_buffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    *lpData = m_data;

    return D3D_OK;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the
  // Direct3DExecuteBuffer object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug(">>> D3D3ExecuteBuffer::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Lock(LPD3DEXECUTEBUFFERDESC lpDesc) {
    Logger::debug(">>> D3D3ExecuteBuffer::Lock");

    if (unlikely(lpDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_locked))
      return D3DERR_EXECUTE_LOCKED;

    m_locked = true;
    lpDesc->dwFlags = D3DDEB_BUFSIZE|D3DDEB_LPDATA;
    lpDesc->dwBufferSize = m_buffer.size();
    lpDesc->lpData = m_buffer.data();

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Optimize(DWORD dwUnknown) {
    Logger::debug(">>> D3D3ExecuteBuffer::Optimize");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::SetExecuteData(LPD3DEXECUTEDATA lpData) {
    Logger::debug(">>> D3D3ExecuteBuffer::SetExecuteData");

    if (unlikely(lpData == nullptr || m_buffer.size() == 0))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwInstructionOffset + lpData->dwInstructionLength > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpData->dwVertexOffset + lpData->dwVertexCount > m_buffer.size()))
      return DDERR_INVALIDPARAMS;

    m_data = *lpData;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Unlock() {
    Logger::debug(">>> D3D3ExecuteBuffer::Unlock");

    if (unlikely(!m_locked))
      return D3DERR_EXECUTE_NOT_LOCKED;

    m_locked = false;

    return D3D_OK;
  }

  // Docs state: "Not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3ExecuteBuffer::Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved) {
    Logger::debug(">>> D3D3ExecuteBuffer::Validate");
    return DDERR_UNSUPPORTED;
  }

}

#include "d3d7_buffer.h"

#include "d3d7_device.h"
#include "d3d7_util.h"

namespace dxvk {

  uint32_t D3D7VertexBuffer::s_buffCount = 0;

  D3D7VertexBuffer::D3D7VertexBuffer(
            IDirect3DVertexBuffer7* buffProxy,
            Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer,
            D3D7Interface* pIntf,
            D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<d3d9::IDirect3DVertexBuffer9, IDirect3DVertexBuffer7>(std::move(pBuffer), buffProxy)
    , m_parent ( pIntf )
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_buffCount = ++s_buffCount;

    ListBufferDetails();

    InitializeIndexBuffer();
  }

  D3D7VertexBuffer::~D3D7VertexBuffer() {
    Logger::debug(str::format("D3D7VertexBuffer: Buffer nr. {{", m_buffCount, "}} bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    Logger::debug(">>> D3D7VertexBuffer::GetVertexBufferDesc");

    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpVBDesc->dwSize != sizeof(LPD3DVERTEXBUFFERDESC)))
      return DDERR_INVALIDOBJECT;

    *lpVBDesc = m_desc;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    Logger::debug(">>> D3D7VertexBuffer::Lock");

    if (data_size != nullptr)
      *data_size = m_size;

    HRESULT hr = m_d3d9->Lock(0, 0, data, ConvertLockFlags(flags, false));

    if (likely(SUCCEEDED(hr)))
      m_locked = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Unlock() {
    Logger::debug(">>> D3D7VertexBuffer::Unlock");

    HRESULT hr = m_d3d9->Unlock();

    if (likely(SUCCEEDED(hr)))
      m_locked = false;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D7VertexBuffer::ProcessVertices");

    if(unlikely(lpD3DDevice == nullptr || lpSrcBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7VertexBuffer* vb = static_cast<D3D7VertexBuffer*>(lpSrcBuffer);
    D3D7Device* actualDevice = vb->GetDevice();

    if(unlikely(actualDevice == nullptr || device != actualDevice)) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Incompatible or null device");
      return DDERR_GENERIC;
    }

    if (likely(device != nullptr))
      device->LockDevice();

    HRESULT hr = device->GetD3D9()->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Failed to set d3d9 stream source");
      return hr;
    }

    hr = device->GetD3D9()->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_d3d9.ptr(), nullptr, dwFlags);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Failed call to d3d9 ProcessVertices");
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::warn("!!! D3D7VertexBuffer::ProcessVerticesStrided: Stub");

    if(unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7Device* actualDevice = GetDevice();

    if(unlikely(actualDevice == nullptr || device != actualDevice)) {
      Logger::err(">>> D3D7VertexBuffer::ProcessVerticesStrided: Incompatible or null device");
      return DDERR_GENERIC;
    }

    //if (likely(device != nullptr))
      //device->LockDevice();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::warn("!!! D3D7VertexBuffer::Optimize: Stub");

    if(unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    //D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    //if (likely(device != nullptr))
      //device->LockDevice();

    //TODO: Sets the D3DVBCAPS_OPTIMIZED flag in dwCaps and the buffer can no longer be locked
    return D3D_OK;
  };

  HRESULT D3D7VertexBuffer::UploadIndices(WORD* indices, DWORD indexCount) {
    HRESULT hr;

    if (unlikely(m_ib9 == nullptr)) {
      hr = InitializeIndexBuffer();

      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::UploadIndices: Failed to initialize index buffer");
        return DDERR_GENERIC;
      }
    }

    const size_t size = indexCount * sizeof(WORD);

    Logger::debug(str::format("D3D7VertexBuffer::UploadIndices: Uploading ", size, " indices"));

    if (unlikely(size > m_size)) {
      Logger::err("D3D7VertexBuffer::UploadIndices: Index buffer capacity exceeded");
    }

    void* pData = nullptr;
    hr = m_ib9->Lock(0, size, &pData, D3DLOCK_DISCARD);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::UploadIndices: Failed to lock destination buffer");
      return DDERR_GENERIC;
    }

    memcpy(pData, static_cast<void*>(indices), size);

    hr = m_ib9->Unlock();

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::UploadIndices: Failed to unlock destination buffer");
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  inline HRESULT D3D7VertexBuffer::InitializeIndexBuffer() {
    D3D7Device* device = GetDevice();

    static constexpr DWORD Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

    if (device != nullptr) {
      Logger::info(str::format("D3D7VertexBuffer::InitializeIndexBuffer: Creating index buffer, size: ", m_size));

      HRESULT hr = device->GetD3D9()->CreateIndexBuffer(m_size, Usage, d3d9::D3DFMT_INDEX16,
                                                        d3d9::D3DPOOL_DEFAULT, &m_ib9, nullptr);

      if (FAILED(hr)) {
        Logger::err("D3D7VertexBuffer::InitializeIndexBuffer: Failed to create attached index buffer");
        return hr;
      }
    } else {
      Logger::warn("D3D7VertexBuffer::InitializeIndexBuffer: Can not initialize index buffer with a NULL device");
    }

    return D3D_OK;
  }

}

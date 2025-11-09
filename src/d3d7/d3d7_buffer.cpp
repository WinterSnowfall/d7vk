#include "d3d7_buffer.h"

#include "d3d7_device.h"
#include "d3d7_util.h"
#include "ddraw7_interface.h"

namespace dxvk {

  uint32_t D3D7VertexBuffer::s_buffCount = 0;

  D3D7VertexBuffer::D3D7VertexBuffer(
            Com<IDirect3DVertexBuffer7>&& buffProxy,
            Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer9,
            D3D7Interface* pParent,
            D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7, d3d9::IDirect3DVertexBuffer9>(pParent, std::move(buffProxy), std::move(pBuffer9))
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_buffCount = ++s_buffCount;

    ListBufferDetails();
  }

  D3D7VertexBuffer::~D3D7VertexBuffer() {
    Logger::debug(str::format("D3D7VertexBuffer: Buffer nr. {{", m_buffCount, "}} bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7, d3d9::IDirect3DVertexBuffer9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DVertexBuffer7)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D7VertexBuffer::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwarpped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D7VertexBuffer::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
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

    return D3D_OK;
  }

  // TODO: Sets the D3DVBCAPS_OPTIMIZED flag in dwCaps and the buffer can no longer be locked
  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D7VertexBuffer::Optimize");

    if(unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    //D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);

    return D3D_OK;
  };

}

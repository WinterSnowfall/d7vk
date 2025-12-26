#include "d3d6_buffer.h"

#include "../ddraw_util.h"
#include "../ddraw4/ddraw4_interface.h"

#include "d3d6_multithread.h"

namespace dxvk {

  uint32_t D3D6VertexBuffer::s_buffCount = 0;

  D3D6VertexBuffer::D3D6VertexBuffer(
            Com<IDirect3DVertexBuffer>&& buffProxy,
            Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer9,
            D3D6Interface* pParent,
            D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<D3D6Interface, IDirect3DVertexBuffer, d3d9::IDirect3DVertexBuffer9>(pParent, std::move(buffProxy), std::move(pBuffer9))
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_parent->AddRef();

    m_buffCount = ++s_buffCount;

    // 0 the size to cater for future GetVertexBufferDesc calls
    m_desc.dwSize = 0;

    ListBufferDetails();
  }

  D3D6VertexBuffer::~D3D6VertexBuffer() {
    m_parent->Release();

    Logger::debug(str::format("D3D7VertexBuffer: Buffer nr. {{7-", m_buffCount, "}} bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D6Interface, IDirect3DVertexBuffer, d3d9::IDirect3DVertexBuffer9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DVertexBuffer)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6VertexBuffer::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D6VertexBuffer::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    Logger::debug(">>> D3D6VertexBuffer::GetVertexBufferDesc");

    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    // Will be passed as 0, and is expected to be returned as 0
    //if (unlikely(lpVBDesc->dwSize != sizeof(LPD3DVERTEXBUFFERDESC)))
      //return DDERR_INVALIDOBJECT;

    *lpVBDesc = m_desc;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    Logger::debug(">>> D3D6VertexBuffer::Lock");

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    RefreshD3D6Device();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    if (data_size != nullptr)
      *data_size = m_size;

    HRESULT hr = m_d3d9->Lock(0, 0, data, ConvertLockFlags(flags, false));

    if (likely(SUCCEEDED(hr)))
      m_locked = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Unlock() {
    Logger::debug(">>> D3D6VertexBuffer::Unlock");

    RefreshD3D6Device();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    HRESULT hr = m_d3d9->Unlock();

    if (likely(SUCCEEDED(hr)))
      m_locked = false;
    else
      return D3DERR_VERTEXBUFFERUNLOCKFAILED;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D6VertexBuffer::ProcessVertices");

    if (unlikely(!dwCount))
      return D3D_OK;

    if (unlikely(lpD3DDevice == nullptr || lpSrcBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!(dwVertexOp & D3DVOP_TRANSFORM)))
      return DDERR_INVALIDPARAMS;

    D3D6Device* device = static_cast<D3D6Device*>(lpD3DDevice);
    D3D6VertexBuffer* vb = static_cast<D3D6VertexBuffer*>(lpSrcBuffer);

    vb->RefreshD3D6Device();
    if (unlikely(vb->GetDevice() == nullptr || device != vb->GetDevice())) {
      Logger::err("D3D6VertexBuffer::ProcessVertices: Incompatible or null device");
      return DDERR_GENERIC;
    }

    HRESULT hrInit;

    // Check and initialize the source buffer
    if (unlikely(!vb->IsInitialized())) {
      hrInit = vb->InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    // Check and initialize the destination buffer (this buffer)
    RefreshD3D6Device();
    if (unlikely(!IsInitialized())) {
      hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    D3D6DeviceLock lock = device->LockDevice();

    HandlePreProcessVerticesFlags(dwVertexOp);

    device->GetD3D9()->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    HRESULT hr = device->GetD3D9()->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_d3d9.ptr(), nullptr, dwFlags);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
    }

    HandlePostProcessVerticesFlags(dwVertexOp);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D6VertexBuffer::Optimize");

    if (unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(IsLocked()))
      return D3DERR_VERTEXBUFFERLOCKED;

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    m_desc.dwCaps &= D3DVBCAPS_OPTIMIZED;

    return D3D_OK;
  };

  HRESULT D3D6VertexBuffer::InitializeD3D9() {
    // Can't create anything without a valid device
    if (unlikely(m_d3d6Device == nullptr)) {
      Logger::err("D3D6VertexBuffer::IntializeD3D9: Null D3D6 device, can't initalize right now");
      return DDERR_GENERIC;
    }

    d3d9::D3DPOOL pool = d3d9::D3DPOOL_DEFAULT;

    if (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY)
      pool = d3d9::D3DPOOL_SYSTEMMEM;

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" : "D3DPOOL_SYSTEMMEM";

    Logger::debug(str::format("D3D6VertexBuffer::IntializeD3D9: Placing in: ", poolPlacement));

    const DWORD usage = ConvertUsageFlags(m_desc.dwCaps, pool);
    HRESULT hr = m_d3d6Device->GetD3D9()->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_d3d9, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6VertexBuffer::IntializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    Logger::debug("D3D6VertexBuffer::IntializeD3D9: Created D3D9 vertex buffer");

    return DD_OK;
  }

}

#include "d3d7_buffer.h"

#include "d3d7_device.h"
#include "d3d7_multithread.h"
#include "d3d7_util.h"
#include "ddraw7_interface.h"

namespace dxvk {

  uint32_t D3D7VertexBuffer::s_buffCount = 0;

  static constexpr IID IID_IDirect3DTnLHalDevice = { 0xf5049e78, 0x4861, 0x11d2, {0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8} };

  D3D7VertexBuffer::D3D7VertexBuffer(
            Com<IDirect3DVertexBuffer7>&& buffProxy,
            Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer9,
            D3D7Interface* pParent,
            D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7, d3d9::IDirect3DVertexBuffer9>(pParent, std::move(buffProxy), std::move(pBuffer9))
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_parent->AddRef();

    m_buffCount = ++s_buffCount;

    // 0 the size to cater for future GetVertexBufferDesc calls
    m_desc.dwSize = 0;

    ListBufferDetails();
  }

  D3D7VertexBuffer::~D3D7VertexBuffer() {
    Logger::debug(str::format("D3D7VertexBuffer: Buffer nr. {{", m_buffCount, "}} bites the dust"));

    m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7, d3d9::IDirect3DVertexBuffer9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DVertexBuffer7)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D7VertexBuffer::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
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

    // Will be passed as 0, and is expected to be returned as 0
    //if (unlikely(lpVBDesc->dwSize != sizeof(D3DVERTEXBUFFERDESC)))
      //return DDERR_INVALIDOBJECT;

    *lpVBDesc = m_desc;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    Logger::debug(">>> D3D7VertexBuffer::Lock");

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

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

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Unlock() {
    Logger::debug(">>> D3D7VertexBuffer::Unlock");

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

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D7VertexBuffer::ProcessVertices");

    if (unlikely(!dwCount))
      return D3D_OK;

    if (unlikely(lpD3DDevice == nullptr || lpSrcBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!(dwVertexOp & D3DVOP_TRANSFORM)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(dwVertexOp & D3DVOP_CLIP)) {
      static bool s_clipErrorShown;

      if (!std::exchange(s_clipErrorShown, true))
        Logger::warn("D3D7VertexBuffer::ProcessVertices: Unsupported vertex operation: D3DVOP_CLIP");
    }
    if (unlikely(dwVertexOp & D3DVOP_EXTENTS)) {
      static bool s_extentsErrorShown;

      if (!std::exchange(s_extentsErrorShown, true))
        Logger::warn("D3D7VertexBuffer::ProcessVertices: Unsupported vertex operation: D3DVOP_EXTENTS");
    }
    if (unlikely(dwVertexOp & D3DVOP_LIGHT)) {
      static bool s_lightErrorShown;

      if (!std::exchange(s_lightErrorShown, true))
        Logger::warn("D3D7VertexBuffer::ProcessVertices: Unsupported vertex operation: D3DVOP_LIGHT");
    }

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7VertexBuffer* vb = static_cast<D3D7VertexBuffer*>(lpSrcBuffer);
    D3D7Device* actualDevice = vb->GetDevice();

    if (unlikely(actualDevice == nullptr || device != actualDevice)) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Incompatible or null device");
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
    if (unlikely(!IsInitialized())) {
      hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    D3D7DeviceLock lock = device->LockDevice();

    if (device->IsMixedVPDevice())
      device->GetD3D9()->SetSoftwareVertexProcessing(TRUE);

    device->GetD3D9()->SetStreamSource(0, vb->GetD3D9(), 0, vb->GetStride());
    HRESULT hr = device->GetD3D9()->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_d3d9.ptr(), nullptr, dwFlags);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
    }

    if (device->IsMixedVPDevice())
      device->GetD3D9()->SetSoftwareVertexProcessing(FALSE);

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::warn("!!! D3D7VertexBuffer::ProcessVerticesStrided: Stub");

    if (unlikely(!dwCount))
      return D3D_OK;

    if(unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7Device* actualDevice = m_parent->GetDevice();

    if(unlikely(actualDevice == nullptr || device != actualDevice)) {
      Logger::err(">>> D3D7VertexBuffer::ProcessVerticesStrided: Incompatible or null device");
      return DDERR_GENERIC;
    }

    D3D7DeviceLock lock = device->LockDevice();

    if (device->IsMixedVPDevice())
      device->GetD3D9()->SetSoftwareVertexProcessing(TRUE);

    // TODO: lpVertexArray needs to be transformed into a non-strided vertex buffer stream

    if (device->IsMixedVPDevice())
      device->GetD3D9()->SetSoftwareVertexProcessing(FALSE);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::debug(">>> D3D7VertexBuffer::Optimize");

    if (unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(IsLocked()))
      return D3DERR_VERTEXBUFFERLOCKED;

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    m_desc.dwCaps &= D3DVBCAPS_OPTIMIZED;

    return D3D_OK;
  };

  HRESULT D3D7VertexBuffer::InitializeD3D9() {
    D3D7Device* device7 = m_parent->GetDevice();

    // Can't create anything without a valid device
    if (unlikely(device7 == nullptr)) {
      Logger::err("D3D7VertexBuffer::IntializeD3D9: Null D3D7 device, can't initalize right now");
      return DDERR_GENERIC;
    }

    D3DDEVICEDESC7 deviceDesc;
    device7->GetCaps(&deviceDesc);
    const d3d9::D3DPOOL pool = (m_parent->GetOptions()->managedTNLBuffers
                             && deviceDesc.deviceGUID == IID_IDirect3DTnLHalDevice) ?
                                d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_DEFAULT;

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" : "D3DPOOL_MANAGED";

    Logger::debug(str::format("D3D7VertexBuffer::IntializeD3D9: Placing in: ", poolPlacement));

    const DWORD usage = ConvertUsageFlags(m_desc.dwCaps, pool);
    HRESULT hr = device7->GetD3D9()->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_d3d9, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::IntializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    Logger::debug("D3D7VertexBuffer::IntializeD3D9: Created D3D9 vertex buffer");

    return DD_OK;
  }

}

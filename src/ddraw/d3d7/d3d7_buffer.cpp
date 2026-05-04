#include "d3d7_buffer.h"

#include "../d3d_common_device.h"

#include "../ddraw_util.h"

#include "../d3d_process_vertices.h"
#include "../d3d_multithread.h"

#include "../ddraw7/ddraw7_interface.h"

namespace dxvk {

  uint32_t D3D7VertexBuffer::s_buffCount = 0;

  D3D7VertexBuffer::D3D7VertexBuffer(
        D3D7Interface* pParent,
        D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7>(pParent, nullptr)
    , m_commonIntf ( pParent->GetCommonInterface() )
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_parent->AddRef();

    m_buffCount = ++s_buffCount;

    ListBufferDetails();
  }

  D3D7VertexBuffer::~D3D7VertexBuffer() {
    m_parent->Release();

    Logger::debug(str::format("D3D7VertexBuffer: Buffer nr. {{7-", m_buffCount, "}} bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    Logger::debug(">>> D3D7VertexBuffer::GetVertexBufferDesc");

    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    const DWORD dwSize = lpVBDesc->dwSize;

    *lpVBDesc = m_desc;
    // The value passed in dwSize during the query is expected to be
    // preserved, even if it is not equal to sizeof(D3DVERTEXBUFFERDESC)
    lpVBDesc->dwSize = dwSize;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    Logger::debug(">>> D3D7VertexBuffer::Lock");

    if (unlikely(IsOptimized()))
      return D3DERR_VERTEXBUFFEROPTIMIZED;

    RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    if (data_size != nullptr)
      *data_size = m_size;

    HRESULT hr = m_vb9->Lock(0, 0, data, ConvertD3D7LockFlags(flags, m_legacyDiscard, false));

    if (likely(SUCCEEDED(hr)))
      m_locked = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::Unlock() {
    Logger::debug(">>> D3D7VertexBuffer::Unlock");

    RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    HRESULT hr = m_vb9->Unlock();

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

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);
    D3D7VertexBuffer* vb = static_cast<D3D7VertexBuffer*>(lpSrcBuffer);

    vb->RefreshD3DDevice();
    if (unlikely(vb->GetDevice() == nullptr || device != vb->GetDevice())) {
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
    d3d9::IDirect3DDevice9* device9 = RefreshD3DDevice();
    if (unlikely(!IsInitialized())) {
      hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    D3DDeviceLock lock = device->LockDevice();

    HRESULT hr = D3D_OK;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (likely(d3dOptions->cpuProcessVertices)) {
      uint8_t *inData = nullptr;
      uint8_t *outData = nullptr;

      hr = vb->GetD3D9VertexBuffer()->Lock(dwSrcIndex * vb->GetStride(), dwCount * vb->GetStride(),
                                           reinterpret_cast<void**>(&inData), D3DLOCK_READONLY|D3DLOCK_NOSYSLOCK);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed to lock source buffer");
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      hr = m_vb9->Lock(dwDestIndex * m_stride, dwCount * m_stride, reinterpret_cast<void**>(&outData), D3DLOCK_NOSYSLOCK);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed to lock destination buffer");
        vb->Unlock();
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      ProcessVerticesData pvData;
      pvData.inData = inData;
      pvData.inFVF = vb->GetFVF();
      pvData.inStride = vb->GetStride();
      pvData.outData = outData;
      pvData.outFVF = m_desc.dwFVF;
      pvData.outStride = m_stride;
      pvData.vertexCount = dwCount;
      pvData.correction = nullptr;
      pvData.dsStatus = nullptr;
      pvData.doLighting = dwVertexOp & D3DVOP_LIGHT;
      pvData.doClipping = dwVertexOp & D3DVOP_CLIP;
      pvData.doNotCopyData = dwFlags & D3DPV_DONOTCOPYDATA;
      pvData.doExtents = true;
      pvData.isLegacy = false;

      std::vector<d3d9::D3DLIGHT9> lights9 = device->GetLights();
      pvData.lights = &lights9;

      ProcessVerticesSW(device9, m_commonIntf->GetOptions(), &pvData);

      m_vb9->Unlock();
      vb->Unlock();

    } else {
      HandlePreProcessVerticesFlags(dwVertexOp);

      device9->SetFVF(vb->GetFVF());
      device9->SetStreamSource(0, vb->GetD3D9VertexBuffer(), 0, vb->GetStride());
      hr = device9->ProcessVertices(dwSrcIndex, dwDestIndex, dwCount, m_vb9.ptr(), nullptr, dwFlags);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
      }

      HandlePostProcessVerticesFlags(dwVertexOp);
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D7VertexBuffer::ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags) {
    Logger::warn("!!! D3D7VertexBuffer::ProcessVerticesStrided: Stub");

    if (unlikely(!dwCount))
      return D3D_OK;

    if(unlikely(lpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D7Device* device = static_cast<D3D7Device*>(lpD3DDevice);

    RefreshD3DDevice();
    if(unlikely(m_d3d7Device == nullptr || device != m_d3d7Device)) {
      Logger::err(">>> D3D7VertexBuffer::ProcessVerticesStrided: Incompatible or null device");
      return DDERR_GENERIC;
    }

    // Check and initialize the destination buffer (this buffer)
    if (unlikely(!IsInitialized())) {
      HRESULT hrInit = InitializeD3D9();
      if (unlikely(FAILED(hrInit)))
        return hrInit;
    }

    D3DDeviceLock lock = device->LockDevice();

    HandlePreProcessVerticesFlags(dwVertexOp);

    // TODO: lpVertexArray needs to be transformed into a non-strided vertex buffer stream

    HandlePostProcessVerticesFlags(dwVertexOp);

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
    // Can't create anything without a valid device
    if (unlikely(m_d3d7Device == nullptr)) {
      Logger::warn("D3D7VertexBuffer::InitializeD3D9: Null D3D7 device, can't initialize right now");
      return DDERR_GENERIC;
    }

    d3d9::IDirect3DDevice9* device9 = m_d3d7Device->GetCommonD3DDevice()->GetD3D9Device();

    d3d9::D3DPOOL pool = d3d9::D3DPOOL_DEFAULT;

    if (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY)
      pool = d3d9::D3DPOOL_SYSTEMMEM;

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" : "D3DPOOL_SYSTEMMEM";

    Logger::debug(str::format("D3D7VertexBuffer::InitializeD3D9: Placing in: ", poolPlacement));

    const DWORD usage = ConvertD3D7UsageFlags(m_desc.dwCaps);
    m_legacyDiscard = m_commonIntf->GetOptions()->forceLegacyDiscard &&
                      (usage & D3DUSAGE_DYNAMIC) && (usage & D3DUSAGE_WRITEONLY);
    HRESULT hr = device9->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_vb9, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7VertexBuffer::InitializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    Logger::debug("D3D7VertexBuffer::InitializeD3D9: Created D3D9 vertex buffer");

    return DD_OK;
  }

  d3d9::IDirect3DDevice9* D3D7VertexBuffer::RefreshD3DDevice() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    D3D7Device* d3d7Device = commonD3DDevice != nullptr ? commonD3DDevice->GetD3D7Device() : nullptr;
    if (unlikely(m_d3d7Device != d3d7Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (unlikely(m_d3d7Device != nullptr)) {
        Logger::debug("D3D7VertexBuffer::RefreshD3DDevice: Device context has changed, clearing D3D9 buffers");
        m_vb9 = nullptr;
      }
      m_d3d7Device = d3d7Device;
    }

    return commonD3DDevice != nullptr ? commonD3DDevice->GetD3D9Device() : nullptr;
  }

  inline void D3D7VertexBuffer::HandlePreProcessVerticesFlags(DWORD pvFlags) {
    // Disable lighting if the D3DVOP_LIGHT isn't specified
    if (!(pvFlags & D3DVOP_LIGHT)) {
      d3d9::IDirect3DDevice9* device9 = m_d3d7Device->GetCommonD3DDevice()->GetD3D9Device();

      device9->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
      if (m_lighting)
        device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    }
  }

  inline void D3D7VertexBuffer::HandlePostProcessVerticesFlags(DWORD pvFlags) {
    if (!(pvFlags & D3DVOP_LIGHT) && m_lighting) {
      d3d9::IDirect3DDevice9* device9 = m_d3d7Device->GetCommonD3DDevice()->GetD3D9Device();

      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    }
  }

}

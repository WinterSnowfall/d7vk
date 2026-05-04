#include "d3d6_buffer.h"

#include "../d3d_common_device.h"

#include "../ddraw_util.h"

#include "../d3d_process_vertices.h"
#include "../d3d_multithread.h"

#include "../ddraw4/ddraw4_interface.h"

namespace dxvk {

  uint32_t D3D6VertexBuffer::s_buffCount = 0;

  D3D6VertexBuffer::D3D6VertexBuffer(
        D3D6Interface* pParent,
        DWORD creationFlags,
        D3DVERTEXBUFFERDESC desc)
    : DDrawWrappedObject<D3D6Interface, IDirect3DVertexBuffer>(pParent, nullptr)
    , m_commonIntf ( pParent->GetCommonInterface() )
    , m_creationFlags ( creationFlags )
    , m_desc ( desc )
    , m_stride ( GetFVFSize(desc.dwFVF) )
    , m_size ( m_stride * desc.dwNumVertices ) {
    m_buffCount = ++s_buffCount;

    ListBufferDetails();
  }

  D3D6VertexBuffer::~D3D6VertexBuffer() {
    Logger::debug(str::format("D3D6VertexBuffer: Buffer nr. {{1-", m_buffCount, "}} bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc) {
    Logger::debug(">>> D3D6VertexBuffer::GetVertexBufferDesc");

    if (unlikely(lpVBDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    const DWORD dwSize = lpVBDesc->dwSize;

    *lpVBDesc = m_desc;
    // The value passed in dwSize during the query is expected to be
    // preserved, even if it is not equal to sizeof(D3DVERTEXBUFFERDESC)
    lpVBDesc->dwSize = dwSize;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Lock(DWORD flags, void **data, DWORD *data_size) {
    Logger::debug(">>> D3D6VertexBuffer::Lock");

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

    HRESULT hr = m_vb9->Lock(0, 0, data, ConvertD3D6LockFlags(flags, false));

    if (likely(SUCCEEDED(hr)))
      m_locked = true;

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6VertexBuffer::Unlock() {
    Logger::debug(">>> D3D6VertexBuffer::Unlock");

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

    vb->RefreshD3DDevice();
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

      HRESULT hr = vb->GetD3D9VertexBuffer()->Lock(dwSrcIndex * vb->GetStride(), dwCount * vb->GetStride(),
                                                   reinterpret_cast<void**>(&inData), D3DLOCK_READONLY|D3DLOCK_NOSYSLOCK);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed to lock source buffer");
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      hr = m_vb9->Lock(dwDestIndex * m_stride, dwCount * m_stride, reinterpret_cast<void**>(&outData), D3DLOCK_NOSYSLOCK);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed to lock destination buffer");
        vb->Unlock();
        return D3DERR_VERTEXBUFFERLOCKED;
      }

      D3DCommonViewport* commonViewport = device->GetCurrentViewportInternal()->GetCommonViewport();

      ProcessVerticesData pvData;
      pvData.inData = inData;
      pvData.inFVF = vb->GetFVF();
      pvData.inStride = vb->GetStride();
      pvData.outData = outData;
      pvData.outFVF = m_desc.dwFVF;
      pvData.outStride = m_stride;
      pvData.vertexCount = dwCount;
      pvData.correction = commonViewport->GetLegacyProjectionMatrix(0);
      pvData.dsStatus = nullptr;
      pvData.doLighting = dwVertexOp & D3DVOP_LIGHT;
      pvData.doClipping = dwVertexOp & D3DVOP_CLIP;
      pvData.doNotCopyData = dwFlags & D3DPV_DONOTCOPYDATA;
      pvData.doExtents = true;
      pvData.isLegacy = true;

      std::vector<Com<D3DLight>> lights = commonViewport->GetLights();
      std::vector<d3d9::D3DLIGHT9> lights9;

      for (auto light: lights) {
        if (!light->IsActive())
          continue;

        const d3d9::D3DLIGHT9* light9 = light->GetD3D9Light();
        if (light9 != nullptr)
          lights9.push_back(*light9);
      }

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
        Logger::err("D3D6VertexBuffer::ProcessVertices: Failed call to D3D9 ProcessVertices");
      }

      HandlePostProcessVerticesFlags(dwVertexOp);
    }

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
      Logger::warn("D3D6VertexBuffer::InitializeD3D9: Null D3D6 device, can't initialize right now");
      return DDERR_GENERIC;
    }

    d3d9::IDirect3DDevice9* device9 = m_d3d6Device->GetCommonD3DDevice()->GetD3D9Device();

    d3d9::D3DPOOL pool = d3d9::D3DPOOL_DEFAULT;

    if (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY)
      pool = d3d9::D3DPOOL_SYSTEMMEM;

    const char* poolPlacement = pool == d3d9::D3DPOOL_DEFAULT ? "D3DPOOL_DEFAULT" : "D3DPOOL_SYSTEMMEM";

    Logger::debug(str::format("D3D6VertexBuffer::InitializeD3D9: Placing in: ", poolPlacement));

    const DWORD usage = ConvertD3D6UsageFlags(m_desc.dwCaps, m_creationFlags);
    HRESULT hr = device9->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_vb9, nullptr);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6VertexBuffer::InitializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    Logger::debug("D3D6VertexBuffer::InitializeD3D9: Created D3D9 vertex buffer");

    return DD_OK;
  }

  d3d9::IDirect3DDevice9* D3D6VertexBuffer::RefreshD3DDevice() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    D3D6Device* d3d6Device = commonD3DDevice != nullptr ? commonD3DDevice->GetD3D6Device() : nullptr;
    if (unlikely(m_d3d6Device != d3d6Device)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (unlikely(m_d3d6Device != nullptr)) {
        Logger::debug("D3D6VertexBuffer::RefreshD3DDevice: Device context has changed, clearing D3D9 buffers");
        m_vb9 = nullptr;
      }
      m_d3d6Device = d3d6Device;
    }

    return commonD3DDevice != nullptr ? commonD3DDevice->GetD3D9Device() : nullptr;
  }

  inline void D3D6VertexBuffer::HandlePreProcessVerticesFlags(DWORD pvFlags) {
    // Disable lighting if the D3DVOP_LIGHT isn't specified
    if (!(pvFlags & D3DVOP_LIGHT)) {
      d3d9::IDirect3DDevice9* device9 = m_d3d6Device->GetCommonD3DDevice()->GetD3D9Device();

      device9->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
      if (m_lighting) {
        //Logger::debug("D3D6VertexBuffer: Disabling lighting");
        device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
      }
    }
    m_d3d6Device->HandlePreDrawLegacyProjection(0);
  }

  inline void D3D6VertexBuffer::HandlePostProcessVerticesFlags(DWORD pvFlags) {
    if (!(pvFlags & D3DVOP_LIGHT) && m_lighting) {
      d3d9::IDirect3DDevice9* device9 = m_d3d6Device->GetCommonD3DDevice()->GetD3D9Device();

      //Logger::debug("D3D6VertexBuffer: Enabling lighting");
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    }
    m_d3d6Device->HandlePostDrawLegacyProjection();
  }

}

#include "d3d_common_buffer.h"

#include "d3d_common_device.h"

namespace dxvk {

  D3DCommonBuffer::D3DCommonBuffer(
        DDrawCommonInterface* commonIntf,
        const D3DVERTEXBUFFERDESC* pDesc,
        DWORD creationFlags)
    : m_commonIntf ( commonIntf )
    , m_desc ( *pDesc )
    , m_creationFlags ( creationFlags )
    , m_stride ( GetFVFSize(pDesc->dwFVF) )
    , m_size ( m_stride * pDesc->dwNumVertices ) {
  }

  D3DCommonBuffer::~D3DCommonBuffer() {
  }

  HRESULT D3DCommonBuffer::InitializeD3D9() {
    // Can't create anything without a valid device
    if (unlikely(m_commonD3DDevice == nullptr)) {
      Logger::warn("D3DCommonBuffer::InitializeD3D9: Null device, can't initialize right now");
      return DDERR_GENERIC;
    }

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    const d3d9::D3DPOOL pool = (m_desc.dwCaps & D3DVBCAPS_SYSTEMMEMORY) ? d3d9::D3DPOOL_SYSTEMMEM :
                               d3dOptions->managedVertexBuffers ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_DEFAULT;
    const DWORD usage = ConvertD3DUsageFlags(m_desc.dwCaps, m_creationFlags, pool);

    // Note: IDirect3DVertexBuffer (D3D6) doesn't have a DISCARD flag
    m_legacyDiscard = d3dOptions->forceLegacyDiscard &&
                      (usage & D3DUSAGE_DYNAMIC) && (usage & D3DUSAGE_WRITEONLY);

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    HRESULT hr = device9->CreateVertexBuffer(m_size, usage, m_desc.dwFVF, pool, &m_vb9, nullptr);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3DCommonBuffer::InitializeD3D9: Failed to create D3D9 vertex buffer");
      return hr;
    }

    return D3D_OK;
  }

  void D3DCommonBuffer::RefreshD3DDevice() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    if (unlikely(m_commonD3DDevice != commonD3DDevice)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (unlikely(m_commonD3DDevice != nullptr)) {
        Logger::debug("D3DCommonBuffer::RefreshD3DDevice: Device context has changed, clearing D3D9 buffers");
        m_vb9 = nullptr;
      }
      m_commonD3DDevice = commonD3DDevice;
    }
  }

}
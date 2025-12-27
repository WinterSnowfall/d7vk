#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "d3d6_interface.h"
#include "d3d6_device.h"

namespace dxvk {

  class D3D6VertexBuffer final : public DDrawWrappedObject<D3D6Interface, IDirect3DVertexBuffer, d3d9::IDirect3DVertexBuffer9> {

  public:

    D3D6VertexBuffer(Com<IDirect3DVertexBuffer>&& buffProxy,
                    Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer9,
                    D3D6Interface* pParent,
                    D3DVERTEXBUFFERDESC desc);

    ~D3D6VertexBuffer();

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD dwFlags, LPVOID* lplpData, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    DWORD GetFVF() const {
      return m_desc.dwFVF;
    }

    DWORD GetStride() const {
      return m_stride;
    }

    DWORD GetNumVertices() const {
      return m_size / m_stride;
    }

    bool IsLocked() const {
      return m_locked;
    }

    D3D6Device* GetDevice() const {
      return m_d3d6Device;
    }

    void RefreshD3D6Device() {
      D3D6Device* d3d6Device = m_parent->GetLastUsedDevice();
      if (unlikely(m_d3d6Device != d3d6Device)) {
        // Check if the device has been recreated and reset all D3D9 resources
        if (unlikely(m_d3d6Device != nullptr)) {
          Logger::debug("D3D6VertexBuffer::RefreshD3D6Device: Device context has changed, clearing D3D9 buffers");
          m_d3d9 = nullptr;
        }
        m_d3d6Device = d3d6Device;
      }
    }

    HRESULT InitializeD3D9();

  private:

    inline bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

    inline void HandlePreProcessVerticesFlags(DWORD pvFlags) {
      // Disable lighting if the D3DVOP_LIGHT isn't specified
      if (!(pvFlags & D3DVOP_LIGHT)) {
        m_d3d6Device->GetD3D9()->GetRenderState(d3d9::D3DRS_LIGHTING, &m_lighting);
        if (m_lighting) {
          Logger::debug("D3D6VertexBuffer: Disabling lighting");
          m_d3d6Device->GetD3D9()->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
        }
      }
      if (pvFlags & D3DVOP_CLIP) {
        Logger::debug("D3D6VertexBuffer: Unsupported D3DVOP_CLIP");
      }
      if (pvFlags & D3DVOP_EXTENTS) {
        Logger::debug("D3D6VertexBuffer: Unsupported D3DVOP_EXTENTS");
      }
    }

    inline void HandlePostProcessVerticesFlags(DWORD pvFlags) {
      if (!(pvFlags & D3DVOP_LIGHT) && m_lighting) {
        Logger::debug("D3D6VertexBuffer: Enabling lighting");
        m_d3d6Device->GetD3D9()->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
      }
    }

    inline void ListBufferDetails() const {
      Logger::debug(str::format("D3D6VertexBuffer: Created a new buffer nr. {{1-", m_buffCount, "}}:"));
      Logger::debug(str::format("   Size:     ", m_size));
      Logger::debug(str::format("   FVF:      ", m_desc.dwFVF));
      Logger::debug(str::format("   Vertices: ", m_size / m_stride));
    }

    static uint32_t     s_buffCount;
    uint32_t            m_buffCount  = 0;

    D3D6Device*         m_d3d6Device = nullptr;

    DWORD               m_lighting   = FALSE;

    D3DVERTEXBUFFERDESC m_desc;

    UINT                m_stride = 0;
    UINT                m_size   = 0;

    bool                m_locked  = false;

  };

}

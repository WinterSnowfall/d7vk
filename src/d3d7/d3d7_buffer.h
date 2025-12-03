#pragma once

#include "d3d7_include.h"
#include "d3d7_interface.h"
#include "d3d7_device.h"
#include "ddraw7_wrapped_object.h"

namespace dxvk {

  class D3D7VertexBuffer final : public DDrawWrappedObject<D3D7Interface, IDirect3DVertexBuffer7, d3d9::IDirect3DVertexBuffer9> {

  public:

    D3D7VertexBuffer(Com<IDirect3DVertexBuffer7>&& buffProxy,
                     Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer9,
                     D3D7Interface* pParent,
                     D3DVERTEXBUFFERDESC desc);

    ~D3D7VertexBuffer();

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD flags, void **data, DWORD *data_size);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    DWORD GetFVF() const {
      return m_desc.dwFVF;
    }

    DWORD GetStride() const {
      return m_stride;
    }

    bool IsLocked() const {
      return m_locked;
    }

    D3D7Device* GetDevice() const {
      return m_d3d7Device;
    }

    void RefreshD3D7Device() {
      D3D7Device* d3d7Device = m_parent->GetLastUsedDevice();
      if (unlikely(m_d3d7Device != d3d7Device)) {
        // Check if the device has been recreated and reset all D3D9 resources
        if (unlikely(m_d3d7Device != nullptr)) {
          Logger::debug("D3D7VertexBuffer::RefreshD3D7Device: Device context has changed, clearing D3D9 buffers");
          m_d3d9 = nullptr;
        }
        m_d3d7Device = d3d7Device;
      }
    }

    HRESULT InitializeD3D9();

  private:

    inline bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

    inline void ListBufferDetails() const {
      Logger::debug(str::format("D3D7VertexBuffer: Created a new buffer nr. {{", m_buffCount, "}}:"));
      Logger::debug(str::format("   Size:     ", m_size));
      Logger::debug(str::format("   FVF:      ", m_desc.dwFVF));
      Logger::debug(str::format("   Vertices: ", m_size / m_stride));
    }

    static uint32_t     s_buffCount;
    uint32_t            m_buffCount  = 0;

    D3D7Device*         m_d3d7Device = nullptr;

    D3DVERTEXBUFFERDESC m_desc;

    UINT                m_stride = 0;
    UINT                m_size   = 0;

    bool                m_locked  = false;

  };

}

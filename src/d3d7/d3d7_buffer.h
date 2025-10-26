#pragma once

#include "d3d7_include.h"
#include "d3d7_interface.h"
#include "ddraw7_wrapped_object.h"

namespace dxvk {

  class D3D7Interface;

  class D3D7VertexBuffer final : public DDrawWrappedObject<d3d9::IDirect3DVertexBuffer9, IDirect3DVertexBuffer7> {

  public:

    D3D7VertexBuffer(IDirect3DVertexBuffer7* buffProxy,
                     Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer,
                     D3D7Interface* pIntf,
                     D3DVERTEXBUFFERDESC desc);

    ~D3D7VertexBuffer();

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD flags, void **data, DWORD *data_size);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT UploadIndices(WORD* indices, DWORD indexCount);

    d3d9::IDirect3DIndexBuffer9* GetIndexBuffer() const {
      return m_ib9.ptr();
    }

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
      return m_parent->GetDevice();
    }

  private:

    inline void InitializeIndexBuffer();

    inline void ListBufferDetails() {
      Logger::debug(str::format("D3D7VertexBuffer: Created a new buffer nr. {{", m_buffCount, "}}:"));
      Logger::debug(str::format("   Size:     ", m_size));
      Logger::debug(str::format("   FVF:      ", m_desc.dwFVF));
      Logger::debug(str::format("   Vertices: ", m_size / m_stride));
    }

    static uint32_t     s_buffCount;
    uint32_t            m_buffCount  = 0;

    D3D7Interface*      m_parent = nullptr;

    D3DVERTEXBUFFERDESC m_desc;

    DWORD               m_stride  = 0;
    DWORD               m_size    = 0;

    bool                m_locked  = false;

    // Attached index buffer, same size as the vertex buffer
    Com<d3d9::IDirect3DIndexBuffer9> m_ib9;

  };

}

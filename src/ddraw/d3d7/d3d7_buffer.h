#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_buffer.h"

#include "d3d7_interface.h"
#include "d3d7_device.h"

namespace dxvk {

  class D3D7VertexBuffer final : public DDrawChildObject<D3D7Interface, IDirect3DVertexBuffer7> {

  public:

    D3D7VertexBuffer(
          D3DCommonBuffer* commonBuffer,
          D3D7Interface* pParent,
          D3DVERTEXBUFFERDESC* pDesc);

    ~D3D7VertexBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD flags, void **data, DWORD *data_size);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    D3DCommonBuffer* GetCommonBuffer() const {
      return m_commonBuffer.ptr();
    }

    bool IsLocked() const {
      return m_locked;
    }

  private:

    std::atomic<bool>    m_locked = false;

    Com<D3DCommonBuffer> m_commonBuffer;

  };

}

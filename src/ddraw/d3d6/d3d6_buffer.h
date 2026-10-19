#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_buffer.h"

#include "../../util/sync/sync_scoped.h"

#include "d3d6_interface.h"
#include "d3d6_device.h"

namespace dxvk {

  using D3DDeviceLock = sync::ScopedDeviceGuard;

  class D3D6VertexBuffer final : public DDrawChildObject<D3D6Interface, IDirect3DVertexBuffer> {

  public:

    D3D6VertexBuffer(
          D3DCommonBuffer* commonBuffer,
          D3D6Interface* pParent,
          D3DVERTEXBUFFERDESC* pDesc,
          DWORD creationFlags);

    ~D3D6VertexBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD dwFlags, LPVOID* lplpData, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    D3DCommonBuffer* GetCommonBuffer() const {
      return m_commonBuffer.ptr();
    }

    bool IsLocked() const {
      return m_locked;
    }

  private:

    inline void IncrementLockCounter() {
      m_locked++;
    }

    inline void DecrementLockCounter() {
      uint32_t lockedOld, locked;

      do {
        lockedOld = m_locked.load();
        // Clamp to 0
        if (unlikely(!lockedOld))
          return;
        locked = lockedOld - 1u;
      } while (!m_locked.compare_exchange_weak(lockedOld, locked));
    }

    // There can be multiple simultaneous locks on a vertex buffer
    std::atomic<uint32_t> m_locked = { 0u };

    Com<D3DCommonBuffer>  m_commonBuffer;

  };

}

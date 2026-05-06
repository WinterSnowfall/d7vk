#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"

#include "d3d6_interface.h"
#include "d3d6_device.h"

namespace dxvk {

  class D3D6VertexBuffer final : public DDrawWrappedObject<D3D6Interface, IDirect3DVertexBuffer> {

  public:

    D3D6VertexBuffer(
          D3D6Interface* pParent,
          DWORD creationFlags,
          D3DVERTEXBUFFERDESC desc);

    ~D3D6VertexBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD dwFlags, LPVOID* lplpData, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    HRESULT InitializeD3D9();

    d3d9::IDirect3DDevice9* RefreshD3DDevice();

    bool IsInitialized() const {
      return m_vb9 != nullptr;
    }

    d3d9::IDirect3DVertexBuffer9* GetD3D9VertexBuffer() const {
      return m_vb9.ptr();
    }

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

  private:

    inline void HandlePreProcessVerticesFlags(DWORD pvFlags);

    inline void HandlePostProcessVerticesFlags(DWORD pvFlags);

    inline bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

    inline void ListBufferDetails() const {
      Logger::debug(str::format("D3D6VertexBuffer: Created a new buffer nr. {{1-", m_buffCount, "}}:"));
      Logger::debug(str::format("   Size:     ", m_size));
      Logger::debug(str::format("   FVF:      ", m_desc.dwFVF));
      Logger::debug(str::format("   Vertices: ", m_size / m_stride));
    }

    bool                              m_locked  = false;

    static uint32_t                   s_buffCount;
    uint32_t                          m_buffCount  = 0;

    DDrawCommonInterface*             m_commonIntf = nullptr;

    D3D6Device*                       m_d3d6Device = nullptr;

    Com<d3d9::IDirect3DVertexBuffer9> m_vb9;

    DWORD                             m_lighting   = FALSE;

    DWORD                             m_creationFlags = 0;
    D3DVERTEXBUFFERDESC               m_desc;

    UINT                              m_stride = 0;
    UINT                              m_size   = 0;

  };

}

#pragma once

#include "ddraw_include.h"
#include "ddraw_util.h"

#include "ddraw_common_interface.h"

namespace dxvk {

  class DDrawCommonInterface;
  class D3DCommonDevice;

  class D3DCommonBuffer : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonBuffer(
          DDrawCommonInterface* commonIntf,
          const D3DVERTEXBUFFERDESC* pDesc,
          DWORD creationFlags
    );

    ~D3DCommonBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    HRESULT InitializeD3D9();

    void RefreshD3DDevice();

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    bool IsInitialized() const {
      return m_vb9 != nullptr;
    }

    d3d9::IDirect3DVertexBuffer9* GetD3D9VertexBuffer() const {
      return m_vb9.ptr();
    }

    D3DVERTEXBUFFERDESC GetDesc() const {
      return m_desc;
    }

    DWORD GetFVF() const {
      return m_desc.dwFVF;
    }

    DWORD GetStride() const {
      return m_stride;
    }

    DWORD GetSize() const {
      return m_size;
    }

    DWORD GetNumVertices() const {
      return m_desc.dwNumVertices;
    }

    // Cops 2170: The Power of Law relies on us not discarding on any write only lock
    // to render geometry, and does not mark the affected buffers with D3DVBCAPS_WRITEONLY
    bool GetLegacyDiscard(DWORD flags) const {
      return m_legacyDiscard || (m_commonIntf->GetOptions()->forceLegacyDiscard && (flags & DDLOCK_WRITEONLY));
    }

    void MarkAsOptimized() {
      m_desc.dwCaps |= D3DVBCAPS_OPTIMIZED;
    }

    bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

  private:

    bool                              m_legacyDiscard   = false;

    DDrawCommonInterface*             m_commonIntf      = nullptr;

    D3DCommonDevice*                  m_commonD3DDevice = nullptr;

    D3DVERTEXBUFFERDESC               m_desc;
    DWORD                             m_creationFlags   = 0;

    DWORD                             m_stride          = 0;
    DWORD                             m_size            = 0;

    Com<d3d9::IDirect3DVertexBuffer9> m_vb9;

  };

}
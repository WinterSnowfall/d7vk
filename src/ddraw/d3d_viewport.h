#pragma once

#include "ddraw_include.h"
#include "ddraw_child_object.h"
#include "ddraw_util.h"

#include "d3d_light.h"

#include <vector>

namespace dxvk {

  class D3DCommonDevice;

  class DDrawSurface;
  class DDraw4Surface;

  class D3DViewport final : public DDrawChildObject<IUnknown, IDirect3DViewport3> {

  public:

    D3DViewport(IUnknown* pParent);

    ~D3DViewport();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D* d3d);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT* data);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT* data);

    HRESULT STDMETHODCALLTYPE TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA* data, DWORD flags, DWORD* offscreen);

    HRESULT STDMETHODCALLTYPE LightElements(DWORD element_count, D3DLIGHTDATA* data);

    HRESULT STDMETHODCALLTYPE SetBackground(D3DMATERIALHANDLE hMat);

    HRESULT STDMETHODCALLTYPE GetBackground(D3DMATERIALHANDLE* material, BOOL* valid);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth(IDirectDrawSurface* surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth(IDirectDrawSurface** surface, BOOL* valid);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT* rects, DWORD flags);

    HRESULT STDMETHODCALLTYPE AddLight(IDirect3DLight* light);

    HRESULT STDMETHODCALLTYPE DeleteLight(IDirect3DLight* light);

    HRESULT STDMETHODCALLTYPE NextLight(IDirect3DLight* lpDirect3DLight, IDirect3DLight** lplpDirect3DLight, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetViewport2(D3DVIEWPORT2* data);

    HRESULT STDMETHODCALLTYPE SetViewport2(D3DVIEWPORT2* data);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth2(IDirectDrawSurface4* surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth2(IDirectDrawSurface4** surface, BOOL* valid);

    HRESULT STDMETHODCALLTYPE Clear2(DWORD count, D3DRECT* rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil);

    void ApplyViewport();

    void DeactivateLights();

    void DeactivateLight(D3DLight* light);

    void ApplyAndActivateLights();

    void ApplyAndActivateLight(D3DLight* light);

    void SetCommonD3DDevice(D3DCommonDevice* commonD3DDevice) {
      m_commonD3DDevice = commonD3DDevice;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    d3d9::D3DVIEWPORT9* GetD3D9Viewport() {
      return &m_viewport9;
    }

    bool IsViewportSet() const {
      return m_isViewportSet;
    }

    void SetIsCurrentViewport(bool isCurrentViewport) {
      m_isCurrentViewport = isCurrentViewport;
    }

    bool IsCurrentViewport() const {
      return m_isCurrentViewport;
    }

    const D3DMATRIX* GetLegacyProjectionMatrix(DWORD drawFlags) {
      // Fast skip if viewport values haven't been set
      if (unlikely(!m_isViewportSet))
        return nullptr;

      const bool needsClipping = !(drawFlags & D3DDP_DONOTCLIP);
      if (unlikely(m_needsClipping != needsClipping)) {
        m_dirtyProjection = true;
        m_needsClipping = needsClipping;
      }

      // Recalculate legacy projection matrix only when needed
      if (unlikely(m_dirtyProjection)) {
        m_legacyProjection._11 = m_legacyScale.x;
        m_legacyProjection._22 = m_legacyScale.y;
        m_legacyProjection._33 = m_legacyScale.z;
        m_legacyProjection._41 = m_needsClipping ? m_legacyClip.x : 0.0f;
        m_legacyProjection._42 = m_needsClipping ? m_legacyClip.y : 0.0f;
        m_legacyProjection._43 = m_needsClipping ? m_legacyClip.z : 0.0f;
        m_legacyProjection._44 = 1.0f;
        // Determine if the projection matrix is an identity matrix
        m_isIdentityMatrix = m_legacyProjection._11 == 1.0f &&
                             m_legacyProjection._22 == 1.0f &&
                             m_legacyProjection._33 == 1.0f &&
                             m_legacyProjection._41 == 0.0f &&
                             m_legacyProjection._42 == 0.0f &&
                             m_legacyProjection._43 == 0.0f;
        m_dirtyProjection = false;
      }

      return m_isIdentityMatrix ? nullptr : &m_legacyProjection;
    }

    bool HasLights() const {
      return m_lights.size() > 0;
    }

    void GetD3D9ActiveLights(std::vector<d3d9::D3DLIGHT9>* lights9) {
      for (auto& light : m_lights) {
        if (light->IsActive())
          lights9->push_back(*light->GetD3D9Light());
      }
    }

  private:

    bool               m_isCurrentViewport     = false;

    bool               m_isViewportSet         = false;
    bool               m_isMaterialSet         = false;
    bool               m_isBackgroundDepthSet  = false;
    bool               m_isBackgroundDepth4Set = false;

    // Legacy projection state
    bool               m_isIdentityMatrix      = false;
    bool               m_needsClipping         = false;
    bool               m_dirtyProjection       = false;

    D3DMATERIALHANDLE  m_materialHandle        = 0u;

    D3DVECTOR          m_legacyScale           = { };
    D3DVECTOR          m_legacyClip            = { };
    D3DMATRIX          m_legacyProjection      = { };

    Com<DDrawSurface>  m_backgroundDepth;
    Com<DDraw4Surface> m_backgroundDepth4;

    D3DCommonDevice*   m_commonD3DDevice       = nullptr;

    d3d9::D3DVIEWPORT9 m_viewport9             = { };

    std::vector<Com<D3DLight>> m_lights;

  };

}

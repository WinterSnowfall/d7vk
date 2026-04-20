#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_surface.h"

#include "../ddraw/ddraw_surface.h"

#include <unordered_map>

namespace dxvk {

  class DDrawSurface;
  class DDraw7Surface;

  /**
  * \brief IDirectDrawSurface3 interface implementation
  */
  class DDraw3Surface final : public DDrawWrappedObject<DDrawSurface, IDirectDrawSurface3, d3d9::IDirect3DSurface9> {

  public:

    DDraw3Surface(
          DDrawCommonSurface* commonSurf,
          Com<IDirectDrawSurface3>&& surfProxy,
          DDrawSurface* pParent,
          DDraw3Surface* pParentSurf);

    ~DDraw3Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE3 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE3 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE3 *lplpDDAttachedSurface);

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS lpDDSCaps);

    HRESULT STDMETHODCALLTYPE GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette);

    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE IsLost();

    HRESULT STDMETHODCALLTYPE Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    HRESULT STDMETHODCALLTYPE Restore();

    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG lX, LONG lY);

    HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);

    HRESULT STDMETHODCALLTYPE Unlock(LPVOID lpSurfaceData);

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE3 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE3 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC lpDDSD, DWORD dwFlags);

    IDirectDrawSurface3* GetShadowOrProxied();

    void SetShadowSurface(DDraw3Surface* shadowSurf) {
      m_shadowSurf = shadowSurf;
    }

    DDraw3Surface* GetShadowSurface() const {
      return m_shadowSurf.ptr();
    }

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    d3d9::IDirect3DDevice9* GetD3D9Device() const {
      return m_d3d9Device;
    }

    DDraw3Surface* GetAttachedDepthStencil() {
      // Fast path, since in most cases we already store the required surface
      if (likely(m_depthStencil.ptr() != nullptr))
        return m_depthStencil.ptr();

      DDSCAPS caps;
      caps.dwCaps = DDSCAPS_ZBUFFER;
      IDirectDrawSurface3* surface = nullptr;
      HRESULT hr = GetAttachedSurface(&caps, &surface);
      if (unlikely(FAILED(hr)))
        return nullptr;

      m_depthStencil = reinterpret_cast<DDraw3Surface*>(surface);

      return m_depthStencil.ptr();
    }

    void ClearAttachedDepthStencil() {
      m_depthStencil = nullptr;
    }

    void SetParentSurface(DDraw3Surface* surface) {
      m_parentSurf = surface;
      m_commonSurf->SetIsAttached(true);
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
      m_commonSurf->SetIsAttached(false);
    }

    HRESULT InitializeOrUploadD3D9() {
      return m_parent->InitializeOrUploadD3D9();
    }

    bool IsInitialized() {
      return m_parent->IsInitialized();
    }

  private:

    inline void RefreshD3D9Device();

    static uint32_t  s_surfCount;
    uint32_t         m_surfCount = 0;

    Com<DDrawCommonSurface>  m_commonSurf;
    DDrawCommonInterface*    m_commonIntf = nullptr;

    Com<DDrawSurface, false> m_originSurf;

    DDraw3Surface*           m_parentSurf = nullptr;

    d3d9::IDirect3DDevice9*  m_d3d9Device = nullptr;

    // Offscreen plain surface we use to mask unwanted DDraw interactions, such
    // as forced swapchain presents caused by blits/locks on primary surfaces
    Com<DDraw3Surface>       m_shadowSurf;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw3Surface>       m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface3*, Com<DDraw3Surface, false>> m_attachedSurfaces;

  };

}

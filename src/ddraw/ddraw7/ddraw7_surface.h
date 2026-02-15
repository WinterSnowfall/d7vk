#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "ddraw7_interface.h"

#include <array>
#include <unordered_map>

namespace dxvk {

  /**
  * \brief IDirectDrawSurface7 interface implementation
  */
  class DDraw7Surface final : public DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7, d3d9::IDirect3DSurface9> {

  public:

    DDraw7Surface(
          DDrawCommonSurface* commonSurf,
          Com<IDirectDrawSurface7>&& surfProxy,
          DDraw7Interface* pParent,
          DDraw7Surface* pParentSurf,
          IUnknown* origin,
          bool isChildObject);

    ~DDraw7Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE7 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface);

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS2 lpDDSCaps);

    HRESULT STDMETHODCALLTYPE GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette);

    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE IsLost();

    HRESULT STDMETHODCALLTYPE Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    HRESULT STDMETHODCALLTYPE Restore();

    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG lX, LONG lY);

    HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);

    HRESULT STDMETHODCALLTYPE Unlock(LPRECT lpSurfaceData);

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    HRESULT STDMETHODCALLTYPE SetPriority(DWORD prio);

    HRESULT STDMETHODCALLTYPE GetPriority(LPDWORD prio);

    HRESULT STDMETHODCALLTYPE SetLOD(DWORD lod);

    HRESULT STDMETHODCALLTYPE GetLOD(LPDWORD lod);

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    const D3DOptions* GetOptions() const {
      return m_commonIntf->GetOptions();
    }

    d3d9::IDirect3DTexture9* GetD3D9Texture() const {
      return m_texture.ptr();
    }

    d3d9::IDirect3DCubeTexture9* GetD3D9CubeTexture() const {
      return m_cubeMap.ptr();
    }

    DDraw7Surface* GetAttachedDepthStencil() {
      // Fast path, since in most cases we already store the required surface
      if (likely(m_depthStencil.ptr() != nullptr))
        return m_depthStencil.ptr();

      DDSCAPS2 caps2;
      caps2.dwCaps = DDSCAPS_ZBUFFER;
      IDirectDrawSurface7* surface = nullptr;
      HRESULT hr = GetAttachedSurface(&caps2, &surface);
      if (unlikely(FAILED(hr)))
        return nullptr;

      m_depthStencil = reinterpret_cast<DDraw7Surface*>(surface);

      return m_depthStencil.ptr();
    }

    void SetParentSurface(DDraw7Surface* surface) {
      m_parentSurf = surface;
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
    }

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeOrUploadD3D9();

  private:

    inline void InitializeAndAttachCubeFace(
        IDirectDrawSurface7* surf,
        d3d9::IDirect3DCubeTexture9* cubeTex9,
        d3d9::D3DCUBEMAP_FACES face);

    inline HRESULT InitializeD3D9(const bool initRT);

    inline HRESULT UploadSurfaceData();

    inline void RefreshD3D9Device() {
      d3d9::IDirect3DDevice9* d3d9Device = m_commonIntf->GetD3D9Device();
      if (unlikely(m_d3d9Device != d3d9Device)) {
        // Check if the device has been recreated and reset all D3D9 resources
        if (m_d3d9Device != nullptr) {
          Logger::debug("DDraw7Surface: Device context has changed, clearing all D3D9 resources");
          m_cubeMap = nullptr;
          m_texture = nullptr;
          m_d3d9 = nullptr;
        }
        m_d3d9Device = d3d9Device;
      }
    }

    inline void ListSurfaceDetails() const {
      const char* type = "generic surface";

      if (m_commonSurf->IsFrontBuffer())                type = "front buffer";
      else if (m_commonSurf->IsBackBuffer())            type = "back buffer";
      else if (m_commonSurf->IsTextureMip())            type = "texture mipmap";
      else if (m_commonSurf->IsCubeMap())               type = "cube map";
      else if (m_commonSurf->IsTexture())               type = "texture";
      else if (m_commonSurf->IsDepthStencil())          type = "depth stencil";
      else if (m_commonSurf->IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (m_commonSurf->IsOverlay())               type = "overlay";
      else if (m_commonSurf->Is3DSurface())             type = "render target";
      else if (m_commonSurf->IsPrimarySurface())        type = "primary surface";
      else if (m_commonSurf->IsNotKnown())              type = "unknown";

      const DDSURFACEDESC2* desc2 = m_commonSurf->GetDesc2();

      Logger::debug(str::format("DDraw7Surface: Created a new surface nr. [[7-", m_surfCount, "]]:"));
      Logger::debug(str::format("   Type:        ", type));
      Logger::debug(str::format("   Dimensions:  ", desc2->dwWidth, "x", desc2->dwHeight));
      Logger::debug(str::format("   Format:      ", m_commonSurf->GetD3D9Format()));
      Logger::debug(str::format("   IsComplex:   ", m_commonSurf->IsComplex() ? "yes" : "no"));
      Logger::debug(str::format("   HasMipMaps:  ", desc2->dwMipMapCount ? "yes" : "no"));
      Logger::debug(str::format("   IsAttached:  ", m_parentSurf != nullptr ? "yes" : "no"));
      if (m_commonSurf->IsFrontBuffer())
        Logger::debug(str::format("   BackBuffers: ", desc2->dwBackBufferCount));
      if (m_commonSurf->HasColorKey())
        Logger::debug(str::format("   ColorKey:    ", m_commonSurf->GetColorKey()->dwColorSpaceLowValue));
    }

    bool             m_isChildObject = false;

    static uint32_t  s_surfCount;
    uint32_t         m_surfCount     = 0;

    Com<DDrawCommonSurface>             m_commonSurf;
    DDrawCommonInterface*               m_commonIntf = nullptr;

    DDraw7Surface*                      m_parentSurf = nullptr;

    IUnknown*                           m_origin     = nullptr;

    d3d9::IDirect3DDevice9*             m_d3d9Device = nullptr;

    Com<d3d9::IDirect3DCubeTexture9>    m_cubeMap;
    std::array<IDirectDrawSurface7*, 6> m_cubeMapSurfaces;

    Com<d3d9::IDirect3DTexture9>        m_texture;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw7Surface>                  m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface7*, Com<DDraw7Surface, false>> m_attachedSurfaces;

  };

}

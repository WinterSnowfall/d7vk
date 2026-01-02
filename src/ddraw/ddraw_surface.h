#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"
#include "ddraw_format.h"
#include "ddraw_clipper.h"
#include "ddraw_palette.h"

#include <unordered_map>

namespace dxvk {

  class DDrawInterface;
  class DDraw7Surface;

  /**
  * \brief Minimal IDirectDrawSurface interface implementation for IDirectDrawSurface7 QueryInterface calls
  */
  class DDrawSurface final : public DDrawWrappedObject<DDrawInterface, IDirectDrawSurface, d3d9::IDirect3DSurface9> {

  public:

    DDrawSurface(
        Com<IDirectDrawSurface>&& surfProxy,
        DDrawInterface* pParent,
        DDrawSurface* pParentSurf,
        DDraw7Surface* origin,
        bool isChildObject);

    ~DDrawSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE *lplpDDAttachedSurface);

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

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference);

    bool IsChildObject() const {
      return m_isChildObject;
    }

    bool IsDepthStencil() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER;
    }

    bool Is3DSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE;
    }

    DDrawSurface* GetAttachedDepthStencil() const {
      return m_depthStencil.ptr();
    }

    void SetAttachedDepthStencil(DDrawSurface* ds) {
      ds->SetParentSurface(this);
      m_depthStencil = ds;
    }

    void SetParentSurface(DDrawSurface* surface) {
      m_parentSurf = surface;
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
    }

  private:

    inline bool IsLegacyInterface() {
      return m_origin != nullptr;
    }

    inline bool IsAttached() const {
      return m_parentSurf != nullptr;
    }

    inline bool IsComplex() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_COMPLEX;
    }

    inline bool IsNotKnown() const {
      return !(m_desc.dwFlags & DDSD_CAPS);
    }

    inline bool IsPrimarySurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE;
    }

    inline bool IsFrontBuffer() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER;
    }

    inline bool IsBackBuffer() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER;
    }

    inline bool IsOffScreenPlainSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN;
    }

    inline bool IsTexture() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_TEXTURE;
    }

    inline bool IsTextureMip() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_MIPMAP;
    }

    inline bool IsOverlay() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OVERLAY;
    }

    inline void ListSurfaceDetails() const {
      const char* type = "generic surface";

      if (IsFrontBuffer())                type = "front buffer";
      else if (IsBackBuffer())            type = "back buffer";
      else if (IsTextureMip())            type = "texture mipmap";
      else if (IsTexture())               type = "texture";
      else if (IsDepthStencil())          type = "depth stencil";
      else if (IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (IsOverlay())               type = "overlay";
      else if (Is3DSurface())             type = "render target";
      else if (IsNotKnown())              type = "unknown";

      const char* attached = IsAttached() ? "yes" : "no";

      Logger::debug(str::format("DDrawSurface: Created a new surface nr. [[1-", m_surfCount, "]]:"));
      Logger::debug(str::format("   Type:       ", type));
      Logger::debug(str::format("   Dimensions: ", m_desc.dwWidth, "x", m_desc.dwHeight));
      Logger::debug(str::format("   Format:     ", m_format));
      Logger::debug(str::format("   IsComplex:  ", IsComplex() ? "yes" : "no"));
      Logger::debug(str::format("   HasMips:    ", m_desc.dwMipMapCount ? "yes" : "no"));
      Logger::debug(str::format("   IsAttached: ", attached));
      if (IsFrontBuffer())
        Logger::debug(str::format("   BackBuffer: ", m_desc.dwBackBufferCount));
    }

    bool             m_isChildObject = true;

    static uint32_t  s_surfCount;
    uint32_t         m_surfCount = 0;

    DDrawSurface*    m_parentSurf = nullptr;

    DDraw7Surface*   m_origin    = nullptr;

    DDSURFACEDESC                       m_desc;
    d3d9::D3DFORMAT                     m_format;

    Com<DDrawClipper>                   m_clipper;
    Com<DDrawPalette>                   m_palette;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDrawSurface>                   m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface*, Com<DDrawSurface, false>> m_attachedSurfaces;

  };

}

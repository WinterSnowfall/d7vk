#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_format.h"

#include "../ddraw_common_surface.h"

#include "ddraw4_interface.h"

#include "../d3d6/d3d6_device.h"

#include <unordered_map>

namespace dxvk {

  class DDraw7Surface;

  /**
  * \brief Minimal IDirectDrawSurface4 interface implementation for IDirectDrawSurface7 QueryInterface calls
  */
  class DDraw4Surface final : public DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9> {

  public:

    DDraw4Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw4Surface* pParentSurf,
        DDraw7Surface* origin,
        bool isChildObject);

    ~DDraw4Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface);

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

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    const D3DOptions* GetOptions() const {
      return m_parent->GetOptions();
    }

    D3D6Device* GetD3D6Device() {
      RefreshD3D6Device();
      return m_d3d6Device;
    }

    d3d9::IDirect3DTexture9* GetD3D9Texture() const {
      return m_texture.ptr();
    }

    bool IsChildObject() const {
      return m_isChildObject;
    }

    bool IsTexture() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_TEXTURE;
    }

    bool IsRenderTarget() const {
      return IsFrontBuffer() || IsBackBufferOrFlippable() || Is3DSurface();
    }

    bool IsForwardableSurface() const {
      return IsFrontBuffer() || IsBackBufferOrFlippable() || IsDepthStencil() || IsOffScreenPlainSurface();
    }

    bool IsBackBufferOrFlippable() const {
      return !IsFrontBuffer() && (m_desc.ddsCaps.dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP));
    }

    bool IsDepthStencil() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER;
    }

    bool Is3DSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE;
    }

    DDraw4Surface* GetAttachedDepthStencil() const {
      return m_depthStencil.ptr();
    }

    void SetAttachedDepthStencil(DDraw4Surface* ds) {
      ds->SetParentSurface(this);
      m_depthStencil = ds;
    }

    void SetParentSurface(DDraw4Surface* surface) {
      m_parentSurf = surface;
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
    }

    bool HasDirtyMipMaps() const {
      return m_commonSurf->HasDirtyMipMaps();
    }

    void DirtyMipMaps() {
      m_commonSurf->DirtyMipMaps();
    }

    void UnDirtyMipMaps() {
      m_commonSurf->UnDirtyMipMaps();
    }

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeOrUploadD3D9();

  private:

    inline bool IsLegacyInterface() const {
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

    inline bool IsTextureMip() const {
      return (m_desc.ddsCaps.dwCaps  & DDSCAPS_MIPMAP) ||
             (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL);
    }

    inline bool IsOverlay() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OVERLAY;
    }

    inline bool IsManaged() const {
      return m_desc.ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE;
    }

    inline HRESULT IntializeD3D9(const bool initRT);

    inline HRESULT UploadSurfaceData();

    inline void RefreshD3D6Device() {
      D3D6Device* d3d6Device = m_parent->GetD3D6Device();
      if (unlikely(m_d3d6Device != d3d6Device)) {
        // Check if the device has been recreated and reset all D3D9 resources
        if (unlikely(m_d3d6Device != nullptr)) {
          Logger::debug("DDraw4Surface: Device context has changed, clearing all D3D9 resources");
          m_texture = nullptr;
          m_d3d9 = nullptr;
        }
        m_d3d6Device = d3d6Device;
      }
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

      Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", m_surfCount, "]]:"));
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

    Com<DDrawCommonSurface>             m_commonSurf;

    DDraw4Surface*                      m_parentSurf = nullptr;

    DDraw7Surface*                      m_origin     = nullptr;

    D3D6Device*                         m_d3d6Device = nullptr;

    DDSURFACEDESC2                      m_desc;
    d3d9::D3DFORMAT                     m_format;

    Com<D3D6Texture, false>             m_texture6;

    Com<d3d9::IDirect3DTexture9>        m_texture;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw4Surface>                  m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface4*, Com<DDraw4Surface, false>> m_attachedSurfaces;

  };

}

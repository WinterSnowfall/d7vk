#pragma once

#include "d3d7_include.h"
#include "d3d7_device.h"
#include "d3d7_util.h"
#include "ddraw7_interface.h"
#include "ddraw7_wrapped_object.h"

#include <vector>

namespace dxvk {

  class DDraw7Surface final : public DDrawWrappedObject<d3d9::IDirect3DSurface9, IDirectDrawSurface7> {

  public:

    DDraw7Surface(
          Com<IDirectDrawSurface7>&& surfProxy,
          DDraw7Interface* pParent,
          DDraw7Surface* pParentSurf,
          DDSURFACEDESC2 desc);

    ~DDraw7Surface();

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

    HRESULT STDMETHODCALLTYPE GetClipper(IDirectDrawClipper **lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(IDirectDrawPalette **lplpDDPalette);

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

    HRESULT STDMETHODCALLTYPE GetDDInterface(void **lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(const GUID &tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(const GUID &tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(const GUID &tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    HRESULT STDMETHODCALLTYPE SetPriority(DWORD prio);

    HRESULT STDMETHODCALLTYPE GetPriority(LPDWORD prio);

    HRESULT STDMETHODCALLTYPE SetLOD(DWORD lod);

    HRESULT STDMETHODCALLTYPE GetLOD(LPDWORD lod);

    d3d9::IDirect3DBaseTexture9* GetTexture() const {
      return m_texture.ptr();
    }

    d3d9::IDirect3DSurface9* GetSurface() const {
      return m_d3d9.ptr();
    }

    void SetSurface(Com<d3d9::IDirect3DSurface9>&& surface) {
      m_d3d9 = std::move(surface);
    }

    bool IsInitialized() const {
      return m_d3d9 != nullptr || m_texture != nullptr || m_cubeMap != nullptr;
    }

    IDirectDrawSurface7* GetAttachedDepthStencil();

    HRESULT InitializeOrUploadD3D9();

  private:

    inline bool IsAttached() const {
      return m_parentSurf != nullptr;
    }

    inline bool IsNotKnown() const {
      return !(m_desc.dwFlags & DDSD_CAPS);
    }

    inline bool IsFrontBuffer() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE;
    }

    inline bool IsBackBuffer() const {
      return m_desc.ddsCaps.dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP);
    }

    inline bool IsOffScreenPlainSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN;
    }

    inline bool IsDepthStencil() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER;
    }

    inline bool IsTexture() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_TEXTURE;
    }

    inline bool IsTextureMip() const {
      return (m_desc.ddsCaps.dwCaps  & DDSCAPS_MIPMAP) ||
             (m_desc.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL);
    }

    inline bool IsCubeMap() const {
      return m_desc.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP;
    }

    inline bool IsOverlay() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OVERLAY;
    }

    // TODO: Probably wrong, need a more accurate way
    inline bool IsRenderTarget() const {
      return IsFrontBuffer() || IsBackBuffer() || IsOffScreenPlainSurface();
    }

    inline HRESULT IntializeD3D9();

    inline HRESULT UploadTextureData();

    // TODO: Need to do this on every device use
    // and refresh derp out if the device is lost
    inline void refreshD3D7Device() {
      D3D7Device* d3d7device = m_parent->GetD3D7Device();
      // Check if the device has been lost
      if (unlikely(m_d3d7device != nullptr && m_d3d7device != d3d7device)) {
        Logger::warn("D3D9 device has been recreated, clearing all d3d9 resources");
        m_d3d9 = nullptr;
        m_texture = nullptr;
        m_cubeMap = nullptr;
      }
      m_d3d7device = d3d7device;
    }

    inline void ListSurfaceDetails() {
      const char* type = "oopsie, unhandled";

      if (IsFrontBuffer())                type = "front buffer";
      else if (IsBackBuffer())            type = "back buffer";
      else if (IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (IsDepthStencil())          type = "depth stencil";
      else if (IsTextureMip())            type = "texture mipmap";
      else if (IsTexture())               type = "texture";
      else if (IsCubeMap())               type = "cube map";
      else if (IsNotKnown())              type = "unknown";

      // Front buffer surfaces apparently don't specify any pixel format info
      // Offscreen plain surfaces can also be created with unsupported formats,
      // but let's at least try to determine the format in those cases
      auto format = !IsFrontBuffer() ? ConvertFormat(m_desc.ddpfPixelFormat) : d3d9::D3DFMT_UNKNOWN;

      if (IsDXTFormat(format))
        m_isDXT = true;

      const char* attached = IsAttached() ? "yes" : "no";

      Logger::info(str::format("DDraw7Surface: Created a new surface nr. [[", m_surfCount, "]]:"));
      Logger::info(str::format("   Type:       ", type));
      Logger::info(str::format("   Dimensions: ", m_desc.dwWidth, "x", m_desc.dwHeight));
      Logger::info(str::format("   Format:     ", format));
      Logger::info(str::format("   HasMips:    ", m_desc.dwMipMapCount ? "yes" : "no"));
      Logger::info(str::format("   IsAttached: ", attached));
    }

    bool              m_isDXT      = false;
    uint32_t          m_mipCount   = 0;

    static uint32_t   s_surfCount;
    uint32_t          m_surfCount  = 0;

    DDraw7Interface*  m_parent     = nullptr;

    DDraw7Surface*    m_parentSurf = nullptr;

    D3D7Device*       m_d3d7device = nullptr;

    DDSURFACEDESC2    m_desc;

    // TODO: Might be worth making this a single generic type at some point
    Com<d3d9::IDirect3DTexture9>           m_texture;
    Com<d3d9::IDirect3DCubeTexture9>       m_cubeMap;

    // These are attached surfaces, which can be mips, back buffers, depth buffers, etc.
    // They are implemented with linked list, so for example only one mip level will be
    // held in a parent texture, and the next mip level will be held in the previous mip
    std::vector<Com<DDraw7Surface, false>> m_attachedSurfaces;

  };

}

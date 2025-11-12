#pragma once

#include "d3d7_include.h"
#include "d3d7_device.h"
#include "ddraw7_format.h"
#include "ddraw7_interface.h"
#include "ddraw7_wrapped_object.h"

#include <unordered_map>

namespace dxvk {

  class DDraw7Surface final : public DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7, d3d9::IDirect3DSurface9> {

  public:

    DDraw7Surface(
          Com<IDirectDrawSurface7>&& surfProxy,
          DDraw7Interface* pParent,
          DDraw7Surface* pParentSurf,
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

    const D3D7Options* GetOptions() const {
      return m_parent->GetOptions();
    }

    d3d9::IDirect3DTexture9* GetTexture() const {
      return m_texture.ptr();
    }

    d3d9::IDirect3DSurface9* GetSurface() const {
      return m_d3d9.ptr();
    }

    void SetSurface(Com<d3d9::IDirect3DSurface9>&& surface) {
      m_d3d9 = std::move(surface);
    }

    bool IsInitialized() const {
      return m_d3d9 != nullptr;
    }

    bool IsTextureOrCubeMap() const {
      return IsTexture() || IsCubeMap();
    }

    bool IsRenderTarget() const {
      return IsFrontBuffer() || IsBackBuffer() || Is3DSurface();
    }

    bool IsForwardableSurface() const {
      return IsFrontBuffer() || IsBackBuffer() || IsDepthStencil() || IsOffScreenPlainSurface();
    }

    bool IsDepthStencil() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER;
    }

    DDraw7Surface* GetAttachedDepthStencil() const {
      return m_depthStencil.ptr();
    }

    void ClearedAttachedDepthStencil() {
      m_depthStencil = nullptr;
    }

    void SetParentSurface(DDraw7Surface* surface) {
      m_parentSurf = surface;
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
    }

    HRESULT InitializeOrUploadD3D9();

  private:

    inline bool IsAttached() const {
      return m_parentSurf != nullptr;
    }

    inline bool IsComplex() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_COMPLEX;
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

    inline bool Is3DSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE;
    }

    inline bool IsOffScreenPlainSurface() const {
      return m_desc.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN;
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

    inline HRESULT IntializeD3D9();

    inline HRESULT UploadSurfaceData();

    // TODO: Need to do this on every device use
    // and refresh derp out if the device is lost
    inline void RefreshD3D7Device() {
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
      else if (IsTextureMip())            type = "texture mipmap";
      else if (IsTexture())               type = "texture";
      else if (IsDepthStencil())          type = "depth stencil";
      else if (IsCubeMap())               type = "cube map";
      else if (IsOverlay())               type = "overlay";
      else if (IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (Is3DSurface())             type = "render target";
      else if (IsNotKnown())              type = "unknown";

      const char* attached = IsAttached() ? "yes" : "no";

      Logger::debug(str::format("DDraw7Surface: Created a new surface nr. [[", m_surfCount, "]]:"));
      Logger::debug(str::format("   Type:       ", type));
      Logger::debug(str::format("   Dimensions: ", m_desc.dwWidth, "x", m_desc.dwHeight));
      Logger::debug(str::format("   Format:     ", m_format));
      Logger::debug(str::format("   IsComplex:  ", IsComplex() ? "yes" : "no"));
      Logger::debug(str::format("   HasMips:    ", m_desc.dwMipMapCount ? "yes" : "no"));
      Logger::debug(str::format("   IsAttached: ", attached));
      if (IsFrontBuffer()) {
        Logger::debug(str::format("   BackBuffer: ", m_desc.dwBackBufferCount));

        if(unlikely(m_desc.dwBackBufferCount > 1))
          Logger::warn("DDraw7Surface: Unhandled use of multiple back buffers");
      }
    }

    bool             m_isChildObject = false;
    uint32_t         m_mipCount      = 0;

    static uint32_t  s_surfCount;
    uint32_t         m_surfCount     = 0;

    DDraw7Surface*   m_parentSurf    = nullptr;

    D3D7Device*      m_d3d7device    = nullptr;

    DDSURFACEDESC2   m_desc;
    d3d9::D3DFORMAT  m_format;

    Com<d3d9::IDirect3DTexture9>     m_texture;
    Com<d3d9::IDirect3DCubeTexture9> m_cubeMap;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw7Surface, false>        m_depthStencil;
    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface7*, Com<DDraw7Surface, false>> m_attachedSurfaces;

  };

}

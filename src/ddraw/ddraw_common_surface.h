#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_interface.h"

#include "ddraw/ddraw_clipper.h"
#include "ddraw/ddraw_palette.h"

namespace dxvk {

  class DDraw7Surface;
  class DDraw4Surface;
  class DDraw3Surface;
  class DDraw2Surface;
  class DDrawSurface;

  class DDrawCommonSurface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonSurface(DDrawCommonInterface* commonIntf);

    ~DDrawCommonSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf.ptr();
    }

    bool IsDesc2Set() const {
      return m_isDesc2Set;
    }

    void SetDesc2(DDSURFACEDESC2& desc2) {
      m_desc2 = desc2;
      m_isDesc2Set = true;
      m_format9 = ConvertFormat(m_desc2.ddpfPixelFormat);
      // determine and cache various frequently used flag combinations
      m_isTextureOrCubeMap      = IsTexture() || IsCubeMap();
      m_isBackBufferOrFlippable = !IsFrontBuffer() && (IsBackBuffer() || IsFlippableSurface());
      m_isRenderTarget          = IsFrontBuffer() || IsBackBuffer() || IsFlippableSurface() || Is3DSurface();
      m_isForwardableSurface    = IsFrontBuffer()  || IsBackBuffer() || IsFlippableSurface()
                               || IsDepthStencil() || IsOffScreenPlainSurface();
      m_isGuardableSurface      = IsPrimarySurface() || IsFrontBuffer()
                               || IsBackBuffer() || IsFlippableSurface();
    }

    const DDSURFACEDESC2* GetDesc2() const {
      return &m_desc2;
    }

    bool IsDescSet() const {
      return m_isDescSet;
    }

    void SetDesc(DDSURFACEDESC& desc) {
      m_desc = desc;
      m_isDescSet = true;
      m_format9 = ConvertFormat(m_desc.ddpfPixelFormat);
      // determine and cache various frequently used flag combinations
      m_isBackBufferOrFlippable = !IsFrontBuffer() && (IsBackBuffer() || IsFlippableSurface());
      m_isRenderTarget          = IsFrontBuffer() || IsBackBuffer() || IsFlippableSurface() || Is3DSurface();
      m_isForwardableSurface    = IsFrontBuffer()  || IsBackBuffer() || IsFlippableSurface()
                               || IsDepthStencil() || IsOffScreenPlainSurface();
      m_isGuardableSurface      = IsPrimarySurface() || IsFrontBuffer()
                               || IsBackBuffer() || IsFlippableSurface();
    }

    const DDSURFACEDESC* GetDesc() const {
      return &m_desc;
    }

    bool IsAlphaFormat() const {
      return ((m_desc2.dwFlags & DDSD_ALPHABITDEPTH) && m_desc2.dwAlphaBitDepth != 0)
          || ((m_desc.dwFlags  & DDSD_ALPHABITDEPTH) && m_desc.dwAlphaBitDepth  != 0);
    }

    bool IsColorKeySet() const {
      return m_isColorKeySet;
    }

    bool HasValidColorKey() const {
      return m_isColorKeySet && m_colorKey.dwColorSpaceLowValue == m_colorKey.dwColorSpaceHighValue;
    }

    void SetColorKey(DDCOLORKEY* colorKey) {
      m_colorKey.dwColorSpaceLowValue  = colorKey->dwColorSpaceLowValue;
      m_colorKey.dwColorSpaceHighValue = colorKey->dwColorSpaceHighValue;
      m_isColorKeySet = true;
    }

    void ClearColorKey() {
      m_colorKey.dwColorSpaceLowValue  = 0;
      m_colorKey.dwColorSpaceHighValue = 0;
      m_isColorKeySet = false;
    }

    const DDCOLORKEY* GetColorKey() const {
      return &m_colorKey;
    }

    DWORD GetColorKeyNormalized() const {
      const DDPIXELFORMAT* pixelFormat = (m_desc2.dwFlags & DDSD_PIXELFORMAT) ? &m_desc2.ddpfPixelFormat : &m_desc.ddpfPixelFormat;

      return ColorKeyToRGB(pixelFormat, m_colorKey.dwColorSpaceHighValue);
    }

    d3d9::D3DFORMAT GetD3D9Format() const {
      const D3DOptions* options = m_commonIntf->GetOptions();

      // Use D16_LOCKABLE instead of D16 if depth write backs are needed
      if (unlikely(options->depthWriteBack && m_format9 == d3d9::D3DFMT_D16))
        return d3d9::D3DFMT_D16_LOCKABLE;

      return m_format9;
    }

    uint8_t GetMipCount() const {
      return m_mipCount;
    }

    void SetMipCount(uint8_t mipCount) {
      m_mipCount = mipCount;
    }

    uint32_t GetBackBufferIndex() const {
      return m_backBufferIndex;
    }

    void IncrementBackBufferIndex(uint32_t index) {
      m_backBufferIndex = index + 1;
    }

    bool HasDirtyMipMaps() const {
      return m_dirtyMipMaps;
    }

    void DirtyMipMaps() {
      m_dirtyMipMaps = true;
    }

    void UnDirtyMipMaps() {
      m_dirtyMipMaps = false;
    }

    void SetClipper(DDrawClipper* clipper) {
      m_clipper = clipper;
    }

    DDrawClipper* GetClipper() const {
      return m_clipper.ptr();
    }

    void SetPalette(DDrawPalette* palette) {
      m_palette = palette;
    }

    DDrawPalette* GetPalette() const {
      return m_palette.ptr();
    }

    void SetDD7Surface(DDraw7Surface* surf7) {
      m_surf7 = surf7;
    }

    DDraw7Surface* GetDD7Surface() const {
      return m_surf7;
    }

    void SetDD4Surface(DDraw4Surface* surf4) {
      m_surf4 = surf4;
    }

    DDraw4Surface* GetDD4Surface() const {
      return m_surf4;
    }

    void SetDD3Surface(DDraw3Surface* surf3) {
      m_surf3 = surf3;
    }

    DDraw3Surface* GetDD3Surface() const {
      return m_surf3;
    }

    void SetDD2Surface(DDraw2Surface* surf2) {
      m_surf2 = surf2;
    }

    DDraw2Surface* GetDD2Surface() const {
      return m_surf2;
    }

    void SetDDSurface(DDrawSurface* surf) {
      m_surf = surf;
    }

    DDrawSurface* GetDDSurface() const {
      return m_surf;
    }

    bool IsComplex() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_COMPLEX
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_COMPLEX;
    }

    bool IsPrimarySurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_PRIMARYSURFACE;
    }

    bool IsFrontBuffer() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_FRONTBUFFER;
    }

    bool IsBackBuffer() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_BACKBUFFER;
    }

    bool IsDepthStencil() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_ZBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_ZBUFFER;
    }

    bool IsOffScreenPlainSurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_OFFSCREENPLAIN;
    }

    bool IsTexture() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_TEXTURE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_TEXTURE;
    }

    bool IsOverlay() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_OVERLAY
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_OVERLAY;
    }

    bool Is3DSurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_3DDEVICE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_3DDEVICE;
    }

    bool IsTextureMip() const {
      return m_desc2.ddsCaps.dwCaps  & DDSCAPS_MIPMAP
          || m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL
          || m_desc.ddsCaps.dwCaps   & DDSCAPS_MIPMAP;
    }

    bool IsCubeMap() const {
      return m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP;
    }

    bool IsFlippableSurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_FLIP
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_FLIP;
    }

    bool IsNotKnown() const {
      return !(m_desc2.dwFlags & DDSD_CAPS)
          && !(m_desc.dwFlags  & DDSD_CAPS);
    }

    bool IsManaged() const {
      return m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE;
    }

    bool HasColorKey() const {
      return (m_desc2.dwFlags & DDSD_CKSRCBLT ||
              m_desc.dwFlags  & DDSD_CKSRCBLT);
    }

    bool IsTextureOrCubeMap() const {
      return m_isTextureOrCubeMap;
    }

    bool IsBackBufferOrFlippable() const {
      return m_isBackBufferOrFlippable;
    }

    bool IsRenderTarget() const {
      return m_isRenderTarget;
    }

    bool IsForwardableSurface() const {
      return m_isForwardableSurface;
    }

    bool IsGuardableSurface() const {
      return m_isGuardableSurface;
    }

  private:

    bool                      m_dirtyMipMaps  = false;
    bool                      m_isDesc2Set    = false;
    bool                      m_isDescSet     = false;
    bool                      m_isColorKeySet = false;

    bool                      m_isTextureOrCubeMap      = false;
    bool                      m_isBackBufferOrFlippable = false;
    bool                      m_isRenderTarget          = false;
    bool                      m_isForwardableSurface    = false;
    bool                      m_isGuardableSurface      = false;

    uint8_t                   m_mipCount = 1;
    uint32_t                  m_backBufferIndex = 0;

    DDSURFACEDESC             m_desc  = { };
    DDSURFACEDESC2            m_desc2 = { };
    DDCOLORKEY                m_colorKey = { };
    d3d9::D3DFORMAT           m_format9 = d3d9::D3DFMT_UNKNOWN;

    Com<DDrawClipper>         m_clipper;
    Com<DDrawPalette>         m_palette;

    Com<DDrawCommonInterface> m_commonIntf;

    // Track all possible surface versions of the same object
    DDraw7Surface*            m_surf7        = nullptr;
    DDraw4Surface*            m_surf4        = nullptr;
    DDraw3Surface*            m_surf3        = nullptr;
    DDraw2Surface*            m_surf2        = nullptr;
    DDrawSurface*             m_surf         = nullptr;

  };

}
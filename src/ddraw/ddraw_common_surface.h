#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_interface.h"

#include "ddraw_clipper.h"
#include "ddraw_palette.h"

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

    HRESULT RefreshSurfaceDescripton();

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf.ptr();
    }

    bool IsDesc2Set() const {
      return m_isDesc2Set;
    }

    void SetDesc2(const DDSURFACEDESC2& desc2) {
      m_desc2 = desc2;
      m_isDesc2Set = true;
      m_format9 = ConvertFormat(m_desc2.ddpfPixelFormat);
      // determine and cache various frequently used flag combinations
      m_isTextureOrCubeMap      = IsTexture() || IsCubeMap();
      m_isBackBufferOrFlippable = !IsFrontBuffer() && (IsBackBuffer() || IsFlippable());
      m_isRenderTarget          = IsFrontBuffer() || IsBackBuffer() || IsFlippable() || Is3DSurface();
      m_isForwardableSurface    = IsFrontBuffer()  || IsBackBuffer() || IsFlippable()
                               || IsDepthStencil() || IsOffScreenPlainSurface();
      m_isGuardableSurface      = IsPrimarySurface() || IsFrontBuffer()
                               || IsBackBuffer() || IsFlippable();
    }

    const DDSURFACEDESC2* GetDesc2() const {
      return &m_desc2;
    }

    bool IsDescSet() const {
      return m_isDescSet;
    }

    void SetDesc(const DDSURFACEDESC& desc) {
      m_desc = desc;
      m_isDescSet = true;
      m_format9 = ConvertFormat(m_desc.ddpfPixelFormat);
      // determine and cache various frequently used flag combinations
      m_isBackBufferOrFlippable = !IsFrontBuffer() && (IsBackBuffer() || IsFlippable());
      m_isRenderTarget          = IsFrontBuffer() || IsBackBuffer() || IsFlippable() || Is3DSurface();
      m_isForwardableSurface    = IsFrontBuffer()  || IsBackBuffer() || IsFlippable()
                               || IsDepthStencil() || IsOffScreenPlainSurface();
      m_isGuardableSurface      = IsPrimarySurface() || IsFrontBuffer()
                               || IsBackBuffer() || IsFlippable();
    }

    const DDSURFACEDESC* GetDesc() const {
      return &m_desc;
    }

    bool IsAlphaFormat() const {
      return ((m_desc2.dwFlags & DDSD_PIXELFORMAT) && (m_desc2.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS))
          || ((m_desc.dwFlags  & DDSD_PIXELFORMAT) && (m_desc.ddpfPixelFormat.dwFlags  & DDPF_ALPHAPIXELS));
    }

    bool HasValidColorKey() const {
      return (m_desc2.dwFlags & DDSD_CKSRCBLT) || (m_desc.dwFlags & DDSD_CKSRCBLT);
    }

    const DDCOLORKEY* GetColorKey() const {
      return (m_desc2.dwFlags & DDSD_CKSRCBLT) ? &m_desc2.ddckCKSrcBlt : &m_desc.ddckCKSrcBlt;
    }

    DDCOLORKEY GetColorKeyNormalized() const {
      const DDPIXELFORMAT* pixelFormat = (m_desc2.dwFlags & DDSD_PIXELFORMAT) ? &m_desc2.ddpfPixelFormat : &m_desc.ddpfPixelFormat;
      const DDCOLORKEY*    colorKey    = (m_desc2.dwFlags & DDSD_CKSRCBLT) ? &m_desc2.ddckCKSrcBlt : &m_desc.ddckCKSrcBlt;

      // Empire of the Ants relies on us using the "Low" color space DWORD
      return ColorKeyToRGB(pixelFormat, colorKey->dwColorSpaceLowValue);
    }

    d3d9::D3DFORMAT GetD3D9Format() const {
      return m_format9;
    }

    float GetNormalizedFloatDepth(DWORD input) const {
      DWORD max = m_format9 != d3d9::D3DFMT_D16 ? std::numeric_limits<uint32_t>::max()
                                                : std::numeric_limits<uint16_t>::max();
      return static_cast<float>(input) / static_cast<float>(max);
    }

    uint16_t GetMipCount() const {
      return m_mipCount;
    }

    void SetMipCount(uint16_t mipCount) {
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

    bool IsAttached() const {
      return m_isAttached;
    }

    void SetIsAttached(bool isAttached) {
      m_isAttached = isAttached;
    }

    void SetClipper(DDrawClipper* clipper) {
      m_clipper = clipper;
    }

    DDrawClipper* GetClipper() const {
      return m_clipper.ptr();
    }

    void SetPalette(DDrawPalette* palette) {
      if (likely(m_palette != palette)) {
        if (palette == nullptr)
          m_palette->SetCommonSurface(nullptr);

        m_palette = palette;

        if (m_palette != nullptr)
          m_palette->SetCommonSurface(this);
      }
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

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
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

    bool IsFlippable() const {
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

    bool IsInSystemMemory() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_SYSTEMMEMORY;
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

    HRESULT ValidateRTUsage() const {
      // Render targets require the DDSCAPS_3DDEVICE flag
      if (unlikely(!Is3DSurface())) {
        Logger::err("DDrawCommonInterface::ValidateRTUsage: Missing DDSCAPS_3DDEVICE");
        return DDERR_INVALIDCAPS;
      }

      // Depth stencil surfaces can't be set as render targets
      if (unlikely(IsDepthStencil())) {
        Logger::err("DDrawCommonInterface::ValidateRTUsage: Invalid DDSCAPS_ZBUFFER");
        return DDERR_INVALIDCAPS;
      }

      // TODO: Render targets must not be created in system memory on HAL/HAL T&L devices
      /*if (unlikely(IsInSystemMemory())) {
        Logger::err("DDrawCommonInterface::ValidateRTUsage: Invalid DDSCAPS_SYSTEMMEMORY");
        return D3DERR_SURFACENOTINVIDMEM;
      }*/

      return DD_OK;
    }

    void ListSurfaceDetails() const {
      const char* type = "generic surface";

      if (IsFrontBuffer())                type = "front buffer";
      else if (IsBackBuffer())            type = "back buffer";
      else if (IsTextureMip())            type = "texture mipmap";
      else if (IsTexture())               type = "texture";
      else if (IsDepthStencil())          type = "depth stencil";
      else if (IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (IsOverlay())               type = "overlay";
      else if (Is3DSurface())             type = "render target";
      else if (IsPrimarySurface())        type = "primary surface";
      else if (IsNotKnown())              type = "unknown";

      Logger::debug(str::format("   Type:        ", type));
      Logger::debug(str::format("   Dimensions:  ", m_desc.dwWidth, "x", m_desc.dwHeight));
      Logger::debug(str::format("   Format:      ", GetD3D9Format()));
      Logger::debug(str::format("   IsComplex:   ", IsComplex() ? "yes" : "no"));
      Logger::debug(str::format("   HasMipMaps:  ", m_desc.dwMipMapCount ? "yes" : "no"));
      Logger::debug(str::format("   IsAttached:  ", IsAttached() ? "yes" : "no"));
      if (IsFrontBuffer())
        Logger::debug(str::format("   BackBuffers: ", m_desc.dwBackBufferCount));
      if (HasColorKey())
        Logger::debug(str::format("   ColorKey:    ", GetColorKey()->dwColorSpaceLowValue));
    }

  private:

    bool                      m_dirtyMipMaps  = false;
    bool                      m_isAttached    = false;
    bool                      m_isDesc2Set    = false;
    bool                      m_isDescSet     = false;

    bool                      m_isTextureOrCubeMap      = false;
    bool                      m_isBackBufferOrFlippable = false;
    bool                      m_isRenderTarget          = false;
    bool                      m_isForwardableSurface    = false;
    bool                      m_isGuardableSurface      = false;

    uint16_t                  m_mipCount = 1;
    uint32_t                  m_backBufferIndex = 0;

    DDSURFACEDESC             m_desc  = { };
    DDSURFACEDESC2            m_desc2 = { };
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

    // Track the origin surface, as in the DDraw surface
    // that gets created through a CreateSurface call
    IUnknown*                 m_origin       = nullptr;

  };

}
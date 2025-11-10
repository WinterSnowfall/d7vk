#pragma once

#include "d3d7_include.h"

namespace dxvk {

  inline d3d9::D3DCUBEMAP_FACES GetCubemapFace(DDSURFACEDESC2* desc) {
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) return d3d9::D3DCUBEMAP_FACE_POSITIVE_X;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_X;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) return d3d9::D3DCUBEMAP_FACE_POSITIVE_Y;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Y;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) return d3d9::D3DCUBEMAP_FACE_POSITIVE_Z;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Z;
    return d3d9::D3DCUBEMAP_FACE_POSITIVE_X;
  }

  inline d3d9::D3DFORMAT ConvertFormat(DDPIXELFORMAT& fmt) {
    if (fmt.dwFlags & DDPF_RGB) {
      Logger::debug(str::format("ConvertFormat: fmt.dwRGBBitCount:     ", fmt.dwRGBBitCount));
      Logger::debug(str::format("ConvertFormat: fmt.dwRGBAlphaBitMask: ", fmt.dwRGBAlphaBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwRBitMask:        ", fmt.dwRBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwGBitMask:        ", fmt.dwGBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwBBitMask:        ", fmt.dwBBitMask));

      switch (fmt.dwRGBBitCount) {
        case 8:
          // R: 1110 0000
          return (fmt.dwFlags & DDPF_PALETTEINDEXED8) ? d3d9::D3DFMT_P8 :d3d9::D3DFMT_R3G3B2;
        case 16: {
          switch (fmt.dwRBitMask) {
            case (0xF << 8):
              // A: 1111 0000 0000 0000
              // R: 0000 1111 0000 0000
              return d3d9::D3DFMT_A4R4G4B4;
            case (0x1F << 10):
              // A: 1000 0000 0000 0000
              // R: 0111 1100 0000 0000
              return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A1R5G5B5 : d3d9::D3DFMT_X1R5G5B5;
            case (0x1F << 11):
              // R: 1111 1000 0000 0000
              return d3d9::D3DFMT_R5G6B5;
          }
          Logger::warn("ConvertFormat: Unhandled dwRGBBitCount 16 format");
          return d3d9::D3DFMT_UNKNOWN;
        }
        case 32: {
          // A: 1111 1111 0000 0000 0000 0000 0000 0000
          // R: 0000 0000 1111 1111 0000 0000 0000 0000
          return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A8R8G8B8 : d3d9::D3DFMT_X8R8G8B8;
        }
      }
      Logger::warn("ConvertFormat: Unhandled dwRGBBitCount format");
      return d3d9::D3DFMT_UNKNOWN;

    } else if ((fmt.dwFlags & DDPF_ZBUFFER)) {
      Logger::debug(str::format("ConvertFormat: fmt.dwZBufferBitDepth: ", fmt.dwZBufferBitDepth));
      Logger::debug(str::format("ConvertFormat: fmt.dwZBitMask:        ",        fmt.dwZBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwStencilBitMask:  ",  fmt.dwStencilBitMask));

      switch (fmt.dwZBufferBitDepth) {
        case 16:
          // D: 1111 1111 1111 1111
          // S: 0000 0000 0000 0000
          return d3d9::D3DFMT_D16;
        case 24:
          // We don't support or expose a 24-bit depth stencil per se,
          // but some applications request one anyway. Use D3DFMT_D24X8.
          return d3d9::D3DFMT_D24X8;
        case 32: {
          switch (fmt.dwStencilBitMask) {
            case 0:
              // D: 1111 1111 1111 1111 1111 1111 0000 0000
              // S: 0000 0000 0000 0000 0000 0000 0000 0000
              return d3d9::D3DFMT_D24X8;
            case (0xFF):
              // D: 1111 1111 1111 1111 1111 1111 0000 0000
              // S: 0000 0000 0000 0000 0000 0000 1111 1111
              return d3d9::D3DFMT_D24S8;
            // Sometimes we get queries with reversed bit mask ordering
            // TODO: Check if depth has a different bit order than regular RGB
            case (DWORD(0xFF << 24)):
              // D: 0000 0000 1111 1111 1111 1111 1111 1111
              // S: 1111 1111 0000 0000 0000 0000 0000 0000
              return d3d9::D3DFMT_D24S8;
          }
          Logger::warn("ConvertFormat: Unhandled dwStencilBitMask 32 format");
          return d3d9::D3DFMT_UNKNOWN;
        }
      }
      Logger::warn("ConvertFormat: Unhandled dwZBufferBitDepth format");
      return d3d9::D3DFMT_UNKNOWN;

    } else if ((fmt.dwFlags & DDPF_LUMINANCE)) {
      Logger::debug(str::format("ConvertFormat: fmt.dwLuminanceBitCount:     ", fmt.dwLuminanceBitCount));
      Logger::debug(str::format("ConvertFormat: fmt.dwLuminanceAlphaBitMask: ", fmt.dwLuminanceAlphaBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwLuminanceBitMask:      ", fmt.dwLuminanceBitMask));

      switch (fmt.dwLuminanceBitCount) {
        case 8: {
          switch (fmt.dwLuminanceBitMask) {
            case (0xF):
              // L: 1111 1111
              return d3d9::D3DFMT_L8;
            case (0x8):
              // A: 1111 0000
              // L: 0000 1111
              return d3d9::D3DFMT_A4L4;
          }
          Logger::warn("ConvertFormat: Unhandled dwLuminanceBitCount 8 format");
          return d3d9::D3DFMT_UNKNOWN;
        }
        case 16:
          // A: 1111 1111 0000 0000
          // L: 0000 0000 1111 1111
          return d3d9::D3DFMT_A8L8;
      }
      Logger::warn("ConvertFormat: Unhandled dwLuminanceBitCount format");
      return d3d9::D3DFMT_UNKNOWN;

    } else if ((fmt.dwFlags & DDPF_BUMPDUDV)) {
      Logger::debug(str::format("ConvertFormat: fmt.dwBumpBitCount:         ", fmt.dwBumpBitCount));
      Logger::debug(str::format("ConvertFormat: fmt.dwBumpLuminanceBitMask: ", fmt.dwBumpLuminanceBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwBumpDvBitMask:        ", fmt.dwBumpDvBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwBumpDuBitMask:        ", fmt.dwBumpDuBitMask));

      switch (fmt.dwBumpBitCount) {
        case 16: {
          // L: 1111 1100 0000 0000
          // V: 0000 0011 1110 0000
          // U: 0000 0000 0001 1111
          //
          // L: 0000 0000 0000 0000
          // V: 1111 1111 0000 0000
          // U: 0000 0000 1111 1111
          return fmt.dwBumpLuminanceBitMask ? d3d9::D3DFMT_L6V5U5 : d3d9::D3DFMT_V8U8;
        }
        case 32: {
          // A: 0000 0000 0000 0000 0000 0000 0000 0000
          // L: 0000 0000 1111 1111 0000 0000 0000 0000
          // V: 0000 0000 0000 0000 1111 1111 0000 0000
          // U: 0000 0000 0000 0000 0000 0000 1111 1111
          return d3d9::D3DFMT_X8L8V8U8;
        }
      }
      Logger::warn("ConvertFormat: Unhandled dwBumpBitCount format");
      return d3d9::D3DFMT_UNKNOWN;

    } else if ((fmt.dwFlags & DDPF_FOURCC)) {
      switch (fmt.dwFourCC) {
        case MAKEFOURCC('D', 'X', 'T', '1'):
          Logger::debug("ConvertFormat: Detected a FOURCC payload: DXT1");
          return d3d9::D3DFMT_DXT1;
        case MAKEFOURCC('D', 'X', 'T', '2'):
          Logger::debug("ConvertFormat: Detected a FOURCC payload: DXT2");
          return d3d9::D3DFMT_DXT2;
        case MAKEFOURCC('D', 'X', 'T', '3'):
          Logger::debug("ConvertFormat: Detected a FOURCC payload: DXT3");
          return d3d9::D3DFMT_DXT3;
        case MAKEFOURCC('D', 'X', 'T', '4'):
          Logger::debug("ConvertFormat: Detected a FOURCC payload: DXT4");
          return d3d9::D3DFMT_DXT4;
        case MAKEFOURCC('D', 'X', 'T', '5'):
          Logger::debug("ConvertFormat: Detected a FOURCC payload: DXT5");
          return d3d9::D3DFMT_DXT5;
      }
      Logger::warn("ConvertFormat: Unhandled FOURCC payload");
      return d3d9::D3DFMT_UNKNOWN;

    } else {
      Logger::warn("ConvertFormat: Unhandled bit payload");
    }

    return d3d9::D3DFMT_UNKNOWN;
  }

  inline DDPIXELFORMAT GetTextureFormat (d3d9::D3DFORMAT format) {
    DDPIXELFORMAT tformat = { };
    tformat.dwSize = sizeof(DDPIXELFORMAT);

    switch (format) {
      case d3d9::D3DFMT_A8R8G8B8:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 32;
        tformat.dwRGBAlphaBitMask = 0xff000000;
        tformat.dwRBitMask = 0x00ff0000;
        tformat.dwGBitMask = 0x0000ff00;
        tformat.dwBBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_X8R8G8B8:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 32;
        tformat.dwRGBAlphaBitMask = 0x00000000;
        tformat.dwRBitMask = 0x00ff0000;
        tformat.dwGBitMask = 0x0000ff00;
        tformat.dwBBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_R5G6B5:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x0000;
        tformat.dwRBitMask = 0xf800;
        tformat.dwGBitMask = 0x07e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_X1R5G5B5:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x0000;
        tformat.dwRBitMask = 0x7c00;
        tformat.dwGBitMask = 0x03e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_A1R5G5B5:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x8000;
        tformat.dwRBitMask = 0x7c00;
        tformat.dwGBitMask = 0x03e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_A4R4G4B4:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0xf000;
        tformat.dwRBitMask = 0x0f00;
        tformat.dwGBitMask = 0x00f0;
        tformat.dwBBitMask = 0x000f;
        break;

      case d3d9::D3DFMT_R3G3B2:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 8;
        tformat.dwLuminanceAlphaBitMask = 0x00;
        tformat.dwRBitMask = 0xe0;
        tformat.dwGBitMask = 0x1c;
        tformat.dwBBitMask = 0x03;
        break;

      // TODO: Not entirely sure if this is correct...
      case d3d9::D3DFMT_P8:
        tformat.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
        tformat.dwRGBBitCount = 8;
        break;

      case d3d9::D3DFMT_L8:
        tformat.dwFlags = DDPF_LUMINANCE;
        tformat.dwLuminanceBitCount = 8;
        tformat.dwLuminanceBitMask = 0xff;
        break;

      case d3d9::D3DFMT_A8L8:
        tformat.dwFlags = DDPF_ALPHAPIXELS | DDPF_LUMINANCE;
        tformat.dwLuminanceBitCount = 16;
        tformat.dwLuminanceAlphaBitMask = 0xff00;
        tformat.dwLuminanceBitMask = 0x00ff;
        break;

      case d3d9::D3DFMT_A4L4:
        tformat.dwFlags = DDPF_ALPHAPIXELS | DDPF_LUMINANCE;
        tformat.dwLuminanceAlphaBitMask = 0xf0;
        tformat.dwLuminanceBitCount = 8;
        tformat.dwLuminanceBitMask = 0x0f;
        break;

      case d3d9::D3DFMT_V8U8:
        tformat.dwFlags = DDPF_BUMPDUDV;
        tformat.dwBumpBitCount = 16;
        tformat.dwBumpDvBitMask = 0xff00;
        tformat.dwBumpDuBitMask = 0x00ff;
        break;

      case d3d9::D3DFMT_L6V5U5:
        tformat.dwFlags = DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE;
        tformat.dwBumpBitCount = 16;
        tformat.dwBumpLuminanceBitMask = 0xfc00;
        tformat.dwBumpDvBitMask = 0x03e0;
        tformat.dwBumpDuBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_X8L8V8U8:
        tformat.dwFlags = DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE;
        tformat.dwBumpBitCount = 32;
        tformat.dwLuminanceAlphaBitMask = 0x00000000;
        tformat.dwBumpLuminanceBitMask = 0x00ff0000;
        tformat.dwBumpDvBitMask = 0x0000ff00;
        tformat.dwBumpDuBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_DXT1:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '1');
        break;

      case d3d9::D3DFMT_DXT2:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '2');
        break;

      case d3d9::D3DFMT_DXT3:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '3');
        break;

      case d3d9::D3DFMT_DXT4:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '4');
        break;

      case d3d9::D3DFMT_DXT5:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
        break;

      default:
      case d3d9::D3DFMT_UNKNOWN:
        Logger::err("GetTextureFormat: Unhandled format");
        break;
    }

    return tformat;
  }

  inline DDPIXELFORMAT GetZBufferFormat (d3d9::D3DFORMAT format) {
    DDPIXELFORMAT zformat = { };
    zformat.dwSize = sizeof(DDPIXELFORMAT);

    switch (format) {
      case d3d9::D3DFMT_D15S1:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 16;
        zformat.dwZBitMask = 0xfff7;
        zformat.dwStencilBitDepth = 1;
        zformat.dwStencilBitMask = 0x0008;
        break;

      case d3d9::D3DFMT_D16:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 16;
        zformat.dwZBitMask = 0xffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x0000;
        break;

      case d3d9::D3DFMT_D24X4S4:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0xffffff00;
        zformat.dwStencilBitDepth = 4;
        zformat.dwStencilBitMask = 0x0000000f;
        break;

      case d3d9::D3DFMT_D24X8:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0xffffff00;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x00000000;
        break;

      case d3d9::D3DFMT_D24S8:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0xffffff00;
        zformat.dwStencilBitDepth = 8;
        zformat.dwStencilBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_D32:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0xffffffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x00000000;
        break;

      default:
      case d3d9::D3DFMT_UNKNOWN:
        Logger::err("GetZBufferFormat: Unhandled format");
        break;
    }

    return zformat;
  }

  inline bool IsDXTFormat(d3d9::D3DFORMAT fmt) {
    return fmt == d3d9::D3DFMT_DXT1
        || fmt == d3d9::D3DFMT_DXT2
        || fmt == d3d9::D3DFMT_DXT3
        || fmt == d3d9::D3DFMT_DXT4
        || fmt == d3d9::D3DFMT_DXT5;
  }

  // Callback function used to navigate the linked mip map chain
  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextMip = static_cast<IDirectDrawSurface7**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline void BlitToD3D9Texture(
      d3d9::IDirect3DTexture9* texture9,
      IDirectDrawSurface7* surface7,
      uint32_t mipLevels,
      bool isDXT) {
    IDirectDrawSurface7* mipMap = surface7;

    Logger::debug(str::format("BlitToD3D9Texture: Blitting ", mipLevels, " mip map(s)"));

    for (uint32_t i = 0; i < mipLevels; i++) {
      // Should never occur normally, but acts as a last ditch safety check
      if (unlikely(mipMap == nullptr)) {
        Logger::warn(str::format("BlitToD3D9Texture: Last found source mip ", i - 1));
        break;
      }

      d3d9::D3DLOCKED_RECT rect9mip;
      HRESULT hr9mip = texture9->LockRect(i, &rect9mip, 0, 0);
      if (likely(SUCCEEDED(hr9mip))) {
        DDSURFACEDESC2 descMip;
        descMip.dwSize = sizeof(DDSURFACEDESC2);
        HRESULT hr7mip = mipMap->Lock(0, &descMip, DDLOCK_READONLY, 0);
        if (likely(SUCCEEDED(hr7mip))) {
          Logger::debug(str::format("descMip.dwWidth:  ", descMip.dwWidth));
          Logger::debug(str::format("descMip.dwHeight: ", descMip.dwHeight));
          Logger::debug(str::format("descMip.lPitch:   ", descMip.lPitch));
          Logger::debug(str::format("rect9mip.Pitch:   ", rect9mip.Pitch));
          // The lock pitch of a DXT surface represents its entire size, apparently
          if (isDXT) {
            // TODO: Determine the minimum between the destination size and
            // the source lPitch (which is the size for compressed formats)
            // to potentially fix some texture edge artifacts with DXT
            size_t size = static_cast<size_t>(descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9Texture: Done blitting DXT mip ", i));
          } else if (unlikely(!isDXT && descMip.lPitch != rect9mip.Pitch)) {
            if (unlikely(i == 0))
              Logger::warn(str::format("BlitToD3D9Texture: Incompatible mip map ", i, " pitch"));
            else
              Logger::debug(str::format("BlitToD3D9Texture: Incompatible mip map ", i, " pitch"));

            uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9mip.pBits);
            uint8_t* data7 = reinterpret_cast<uint8_t*>(descMip.lpSurface);

            size_t copyPitch = std::min<size_t>(descMip.lPitch, rect9mip.Pitch);
            for (uint32_t h = 0; h < descMip.dwHeight; h++)
              memcpy(&data9[h * rect9mip.Pitch], &data7[h * descMip.lPitch], copyPitch);

            Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", i, " row by row"));
          } else {
            size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", i));
          }
          mipMap->Unlock(0);
        } else {
          Logger::warn(str::format("BlitToD3D9Texture: Failed to lock d3d7 mip ", i));
        }
        texture9->UnlockRect(i);

        IDirectDrawSurface7* parentSurface = mipMap;
        mipMap = nullptr;
        // TODO: This does not work reliably on UE1, need to explore why
        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
      } else {
        Logger::warn(str::format("BlitToD3D9Texture: Failed to lock d3d9 mip ", i));
      }
    }
  }

  inline void BlitToD3D9Surface(
      d3d9::IDirect3DSurface9* surface9,
      IDirectDrawSurface7* surface7,
      bool isDXT) {
    d3d9::D3DLOCKED_RECT rect9;
    HRESULT hr9 = surface9->LockRect(&rect9, 0, 0);
    if (SUCCEEDED(hr9)) {
      DDSURFACEDESC2 desc;
      desc.dwSize = sizeof(DDSURFACEDESC2);
      HRESULT hr7 = surface7->Lock(0, &desc, DDLOCK_READONLY, 0);
      if (SUCCEEDED(hr7)) {
        Logger::debug(str::format("desc.dwWidth:  ", desc.dwWidth));
        Logger::debug(str::format("desc.dwHeight: ", desc.dwHeight));
        Logger::debug(str::format("desc.lPitch:   ", desc.lPitch));
        Logger::debug(str::format("rect.Pitch:    ", rect9.Pitch));
        // The lock pitch of a DXT surface represents its entire size, apparently
        if (isDXT) {
          size_t size = static_cast<size_t>(desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          Logger::debug("BlitToD3D9Texture: Done blitting DXT surface");
        } else if (unlikely(desc.lPitch != rect9.Pitch)) {
          Logger::debug("BlitToD3D9Surface: Incompatible surface pitch");

          uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9.pBits);
          uint8_t* data7 = reinterpret_cast<uint8_t*>(desc.lpSurface);

          size_t copyPitch = std::min<size_t>(desc.lPitch, rect9.Pitch);
          for (uint32_t h = 0; h < desc.dwHeight; h++)
            memcpy(&data9[h * rect9.Pitch], &data7[h * desc.lPitch], copyPitch);

          Logger::debug("BlitToD3D9Surface: Done blitting surface row by row");
        } else {
          size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          Logger::debug("BlitToD3D9Surface: Done blitting surface");
        }
        surface7->Unlock(0);
      } else {
        Logger::warn("BlitToD3D9Surface: Failed to lock d3d7 surface");
      }
      surface9->UnlockRect();
    } else {
      Logger::warn("BlitToD3D9Surface: Failed to lock d3d9 surface");
    }
  }

}
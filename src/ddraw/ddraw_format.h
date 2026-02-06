#pragma once

#include "ddraw_include.h"

#include <vector>
#include <type_traits>

namespace dxvk {

  template <size_t N>
  static void CopyToStringArray(char (&dst)[N], const char* src) {
    str::strlcpy(dst, src, N);
  }

  struct CubeMapAttachedSurfaces {
    IDirectDrawSurface7* positiveX = nullptr;
    IDirectDrawSurface7* negativeX = nullptr;
    IDirectDrawSurface7* positiveY = nullptr;
    IDirectDrawSurface7* negativeY = nullptr;
    IDirectDrawSurface7* positiveZ = nullptr;
    IDirectDrawSurface7* negativeZ = nullptr;
  };

  struct AttachedSurface {
    IDirectDrawSurface*  surface      = nullptr;
    DDSURFACEDESC        surfaceDesc  = { };
  };

  struct AttachedSurface4 {
    IDirectDrawSurface4* surface4     = nullptr;
    DDSURFACEDESC2       surface4Desc = { };
  };

  struct AttachedSurface7 {
    IDirectDrawSurface7* surface7     = nullptr;
    DDSURFACEDESC2       surface7Desc = { };
  };

  inline bool IsCubeMapFace(DDSURFACEDESC2* desc) {
    return desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX
        || desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX
        || desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY
        || desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY
        || desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ
        || desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ;
  }

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
          return (fmt.dwFlags & DDPF_PALETTEINDEXED8) ? d3d9::D3DFMT_P8 : d3d9::D3DFMT_R3G3B2;
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
        // Drakan: Order of the Flame uses a dwRGBBitCount of 24
        // to request for D3DFMT_X8R8G8B8, based on provided bitmasks
        case 24:
          return d3d9::D3DFMT_X8R8G8B8;
        case 32: {
          // A: 1111 1111 0000 0000 0000 0000 0000 0000
          // R: 0000 0000 1111 1111 0000 0000 0000 0000
          return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A8R8G8B8 : d3d9::D3DFMT_X8R8G8B8;
        }
      }
      Logger::warn("ConvertFormat: Unhandled dwRGBBitCount format");
      return d3d9::D3DFMT_UNKNOWN;

    // Depth formats will traditionally store stencil info in the MSB, however
    // some games will apparently try to use the LSB, so handle both cases
    } else if (fmt.dwFlags & DDPF_ZBUFFER) {
      Logger::debug(str::format("ConvertFormat: fmt.dwZBufferBitDepth: ", fmt.dwZBufferBitDepth));
      Logger::debug(str::format("ConvertFormat: fmt.dwZBitMask:        ",        fmt.dwZBitMask));
      Logger::debug(str::format("ConvertFormat: fmt.dwStencilBitMask:  ",  fmt.dwStencilBitMask));

      switch (fmt.dwZBufferBitDepth) {
        case 16: {
          switch (fmt.dwStencilBitMask) {
            case 0:
              // D: 1111 1111 1111 1111
              // S: 0000 0000 0000 0000
              return d3d9::D3DFMT_D16;
            case 0x1:
            case (0x1 << 15):
              // D: 0111 1111 1111 1111
              // S: 1000 0000 0000 0000
              Logger::warn("ConvertFormat: Unsupported format D3DFMT_D15S1");
              return d3d9::D3DFMT_D16;
          }
          Logger::warn("ConvertFormat: Unhandled dwStencilBitMask 16 format");
          return d3d9::D3DFMT_UNKNOWN;
        }
        // We don't support or expose a 24-bit depth stencil per se,
        // but some applications request one anyway. Use 32-bit depth.
        case 24:
        case 32: {
          switch (fmt.dwStencilBitMask) {
            case 0: {
              switch (fmt.dwZBitMask) {
                case 0xFFFFFFFF:
                  // D: 1111 1111 1111 1111 1111 1111 1111 1111
                  // S: 0000 0000 0000 0000 0000 0000 0000 0000
                  Logger::warn("ConvertFormat: Unsupported format D3DFMT_D32");
                  return d3d9::D3DFMT_D24X8;
                case 0xFFFFFF:
                case (DWORD(0xFFFFFF << 8)):
                  // D: 0000 0000 1111 1111 1111 1111 1111 1111
                  // S: 0000 0000 0000 0000 0000 0000 0000 0000
                  return d3d9::D3DFMT_D24X8;
              }
              Logger::warn("ConvertFormat: Unhandled dwZBitMask 24/32 format");
              return d3d9::D3DFMT_UNKNOWN;
            }
            case 0xFF:
            case (DWORD(0xFF << 24)):
              // D: 0000 0000 1111 1111 1111 1111 1111 1111
              // S: 1111 1111 0000 0000 0000 0000 0000 0000
              return d3d9::D3DFMT_D24S8;
            case 0xF:
            case (DWORD(0xF << 24)):
              // D: 0000 0000 1111 1111 1111 1111 1111 1111
              // S: 1111 0000 0000 0000 0000 0000 0000 0000
              Logger::warn("ConvertFormat: Unsupported format D3DFMT_D24X4S4");
              return d3d9::D3DFMT_D24S8;
          }
          Logger::warn("ConvertFormat: Unhandled dwStencilBitMask 24/32 format");
          return d3d9::D3DFMT_UNKNOWN;
        }
      }
      Logger::warn("ConvertFormat: Unhandled dwZBufferBitDepth format");
      return d3d9::D3DFMT_UNKNOWN;

    } else if (fmt.dwFlags & DDPF_LUMINANCE) {
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

    } else if (fmt.dwFlags & DDPF_BUMPDUDV) {
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

    } else if (fmt.dwFlags & DDPF_FOURCC) {
      switch (fmt.dwFourCC) {
        case MAKEFOURCC('D', 'X', 'T', '1'):
          return d3d9::D3DFMT_DXT1;
        case MAKEFOURCC('D', 'X', 'T', '2'):
          return d3d9::D3DFMT_DXT2;
        case MAKEFOURCC('D', 'X', 'T', '3'):
          return d3d9::D3DFMT_DXT3;
        case MAKEFOURCC('D', 'X', 'T', '4'):
          return d3d9::D3DFMT_DXT4;
        case MAKEFOURCC('D', 'X', 'T', '5'):
          return d3d9::D3DFMT_DXT5;
        case MAKEFOURCC('Y', 'U', 'Y', '2'):
          return d3d9::D3DFMT_YUY2;
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
        tformat.dwRGBAlphaBitMask = 0x00;
        tformat.dwRBitMask = 0xe0;
        tformat.dwGBitMask = 0x1c;
        tformat.dwBBitMask = 0x03;
        break;

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
        zformat.dwZBitMask = 0x7fff;
        zformat.dwStencilBitDepth = 1;
        zformat.dwStencilBitMask = 0x8000;
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
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 4;
        zformat.dwStencilBitMask = 0xf0000000;
        break;

      case d3d9::D3DFMT_D24X8:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x00000000;
        break;

      case d3d9::D3DFMT_D24S8:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 8;
        zformat.dwStencilBitMask = 0xff000000;
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

  inline HRESULT ValidateSurfaceFlags(DDSURFACEDESC2* desc) {
    const bool isOffScreenPlainSurface = desc->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN;
    const bool isTexture               = desc->ddsCaps.dwCaps & DDSCAPS_TEXTURE;
    const bool isInVideoMemory         = desc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY;
    const bool isInSystemMemory        = desc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY;
    const bool isWriteOnly             = desc->ddsCaps.dwCaps & DDSCAPS_WRITEONLY;

    if (unlikely(isOffScreenPlainSurface && (isInVideoMemory || isInSystemMemory))) {
      return isWriteOnly ? DDERR_INVALIDCAPS : DD_OK;
    }

    if (unlikely(isTexture && (isInVideoMemory || isInSystemMemory))) {
      return isWriteOnly ? DDERR_INVALIDCAPS : DD_OK;
    }

    return DD_OK;
  }

  // D3D5 Callback function used to navigate a flipable surface swapchain
  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfacesCallback(IDirectDrawSurface* subsurf, DDSURFACEDESC* desc, void* ctx) {
    IDirectDrawSurface** nextBackBuffer = static_cast<IDirectDrawSurface**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D6 Callback function used to navigate a flipable surface swapchain
  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfaces4Callback(IDirectDrawSurface4* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface4** nextBackBuffer = static_cast<IDirectDrawSurface4**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D7 callback function used to navigate a flipable surface swapchain
  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfaces7Callback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextBackBuffer = static_cast<IDirectDrawSurface7**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D5 callback function used to navigate the linked mip map chain
  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfacesCallback(IDirectDrawSurface* subsurf, DDSURFACEDESC* desc, void* ctx) {
    IDirectDrawSurface** nextMip = static_cast<IDirectDrawSurface**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_MIPMAP) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D6 callback function used to navigate the linked mip map chain
  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfaces4Callback(IDirectDrawSurface4* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface4** nextMip = static_cast<IDirectDrawSurface4**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D7 callback function used to navigate the linked mip map chain
  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfaces7Callback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextMip = static_cast<IDirectDrawSurface7**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  // D3D5 callback function used to enumerate attached surfaces
  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfacesCallback(IDirectDrawSurface* surface, DDSURFACEDESC* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface>*>(ctx);

    AttachedSurface attachedSurface = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface);

    return DDENUMRET_OK;
  }

  // D3D6 callback function used to enumerate attached surfaces
  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces4Callback(IDirectDrawSurface4* surface, DDSURFACEDESC2* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface4>*>(ctx);

    AttachedSurface4 attachedSurface4 = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface4);

    return DDENUMRET_OK;
  }

  // D3D7 callback function used to enumerate attached surfaces
  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces7Callback(IDirectDrawSurface7* surface, DDSURFACEDESC2* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface7>*>(ctx);

    AttachedSurface7 attachedSurface7 = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface7);

    return DDENUMRET_OK;
  }

  // D3D7 callback function used in cube map face/surface initialization
  inline HRESULT STDMETHODCALLTYPE EnumAndAttachCubeMapFacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    CubeMapAttachedSurfaces* cubeMapAttachedSurfaces = static_cast<CubeMapAttachedSurfaces*>(ctx);

    // Skip any surface which isn't a cube map face
    if (unlikely(!IsCubeMapFace(desc)))
      return DDENUMRET_OK;

    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) {
      cubeMapAttachedSurfaces->positiveX = subsurf;
      return DDENUMRET_OK;
    } else if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) {
      cubeMapAttachedSurfaces->negativeX = subsurf;
      return DDENUMRET_OK;
    } else if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) {
      cubeMapAttachedSurfaces->positiveY = subsurf;
      return DDENUMRET_OK;
    } else if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) {
      cubeMapAttachedSurfaces->negativeY = subsurf;
      return DDENUMRET_OK;
    } else if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) {
      cubeMapAttachedSurfaces->positiveZ = subsurf;
      return DDENUMRET_OK;
    } else if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) {
      cubeMapAttachedSurfaces->negativeZ = subsurf;
      return DDENUMRET_OK;
    }

    return DDENUMRET_OK;
  }

  inline void BlitToD3D9CubeMap(
      d3d9::IDirect3DCubeTexture9* cubeTex9,
      d3d9::D3DFORMAT format9,
      IDirectDrawSurface7* surface,
      uint8_t mipLevels) {
    // Properly handle cube textures with auto-generated mip maps
    const uint8_t actualMipLevels = std::max<uint8_t>(1u, mipLevels);

    DDSURFACEDESC2 desc = { };
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);
    const d3d9::D3DCUBEMAP_FACES face = GetCubemapFace(&desc);

    Logger::debug(str::format("BlitToD3D9CubeMap: Blitting face ", face));

    IDirectDrawSurface7* mipMap = surface;

    Logger::debug(str::format("BlitToD3D9CubeMap: Blitting ", static_cast<uint32_t>(actualMipLevels), " mip map(s)"));

    for (uint8_t i = 0; i < actualMipLevels; i++) {
      // Should never occur normally, but acts as a last ditch safety check
      if (unlikely(mipMap == nullptr)) {
        Logger::warn(str::format("BlitToD3D9CubeMap: Last found source mip ", static_cast<uint32_t>(i - 1)));
        break;
      }

      d3d9::D3DLOCKED_RECT rect9mip;
      // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
      HRESULT hr9 = cubeTex9->LockRect(face, i, &rect9mip, 0, D3DLOCK_DISCARD);
      if (likely(SUCCEEDED(hr9))) {
        DDSURFACEDESC2 descMip;
        descMip.dwSize = sizeof(DDSURFACEDESC2);
        HRESULT hr = mipMap->Lock(0, &descMip, DDLOCK_READONLY, 0);
        if (likely(SUCCEEDED(hr))) {
          Logger::debug(str::format("descMip.dwWidth:  ", descMip.dwWidth));
          Logger::debug(str::format("descMip.dwHeight: ", descMip.dwHeight));
          Logger::debug(str::format("descMip.lPitch:   ", descMip.lPitch));
          Logger::debug(str::format("rect9mip.Pitch:   ", rect9mip.Pitch));
          // The lock pitch of a DXT surface represents its entire size, apparently
          if (IsDXTFormat(format9)) {
            const size_t size = static_cast<size_t>(descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting DXT mip ", static_cast<uint32_t>(i)));
          } else if (unlikely(descMip.lPitch != rect9mip.Pitch)) {
            Logger::debug(str::format("BlitToD3D9CubeMap: Incompatible mip map ", static_cast<uint32_t>(i), " pitch"));

            uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9mip.pBits);
            uint8_t* data7 = reinterpret_cast<uint8_t*>(descMip.lpSurface);

            const size_t copyPitch = std::min<size_t>(descMip.lPitch, rect9mip.Pitch);
            for (uint32_t h = 0; h < descMip.dwHeight; h++)
              memcpy(&data9[h * rect9mip.Pitch], &data7[h * descMip.lPitch], copyPitch);

            Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting mip ", i, " row by row"));
          } else {
            const size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting mip ", static_cast<uint32_t>(i)));
          }
          mipMap->Unlock(0);
        } else {
          Logger::warn(str::format("BlitToD3D9CubeMap: Failed to lock mip ", static_cast<uint32_t>(i)));
        }
        cubeTex9->UnlockRect(face, i);

        IDirectDrawSurface7* parentSurface = mipMap;
        mipMap = nullptr;

        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
      } else {
        Logger::warn(str::format("BlitToD3D9CubeMap: Failed to lock D3D9 mip ", static_cast<uint32_t>(i)));
      }
    }
  }

  template <typename SurfaceType, typename DescType>
  inline void BlitToD3D9Texture(
      d3d9::IDirect3DTexture9* texture9,
      d3d9::D3DFORMAT format9,
      SurfaceType* surface,
      uint8_t mipLevels) {
    // Properly handle textures with auto-generated mip maps
    const uint8_t actualMipLevels = std::max<uint8_t>(1u, mipLevels);

    SurfaceType* mipMap = surface;

    Logger::debug(str::format("BlitToD3D9Texture: Blitting ", static_cast<uint32_t>(actualMipLevels), " mip map(s)"));

    for (uint8_t i = 0; i < actualMipLevels; i++) {
      // Should never occur normally, but acts as a last ditch safety check
      if (unlikely(mipMap == nullptr)) {
        Logger::warn(str::format("BlitToD3D9Texture: Last found source mip ", static_cast<uint32_t>(i - 1)));
        break;
      }

      d3d9::D3DLOCKED_RECT rect9mip;
      // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
      HRESULT hr9 = texture9->LockRect(i, &rect9mip, 0, D3DLOCK_DISCARD);
      if (likely(SUCCEEDED(hr9))) {
        DescType descMip;
        descMip.dwSize = sizeof(DescType);
        HRESULT hr = mipMap->Lock(0, &descMip, DDLOCK_READONLY, 0);
        if (likely(SUCCEEDED(hr))) {
          Logger::debug(str::format("descMip.dwWidth:  ", descMip.dwWidth));
          Logger::debug(str::format("descMip.dwHeight: ", descMip.dwHeight));
          Logger::debug(str::format("descMip.lPitch:   ", descMip.lPitch));
          Logger::debug(str::format("rect9mip.Pitch:   ", rect9mip.Pitch));
          // The lock pitch of a DXT surface represents its entire size, apparently
          if (IsDXTFormat(format9)) {
            const size_t size = static_cast<size_t>(descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9Texture: Done blitting DXT mip ", static_cast<uint32_t>(i)));
          } else if (unlikely(descMip.lPitch != rect9mip.Pitch)) {
            Logger::debug(str::format("BlitToD3D9Texture: Incompatible mip map ", static_cast<uint32_t>(i), " pitch"));

            uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9mip.pBits);
            uint8_t* data7 = reinterpret_cast<uint8_t*>(descMip.lpSurface);

            const size_t copyPitch = std::min<size_t>(descMip.lPitch, rect9mip.Pitch);
            for (uint32_t h = 0; h < descMip.dwHeight; h++)
              memcpy(&data9[h * rect9mip.Pitch], &data7[h * descMip.lPitch], copyPitch);

            Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", static_cast<uint32_t>(i), " row by row"));
          } else {
            const size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", static_cast<uint32_t>(i)));
          }
          mipMap->Unlock(0);
        } else {
          Logger::warn(str::format("BlitToD3D9Texture: Failed to lock mip ", static_cast<uint32_t>(i)));
        }
        texture9->UnlockRect(i);

        SurfaceType* parentSurface = mipMap;
        mipMap = nullptr;

        if constexpr (std::is_same<SurfaceType, IDirectDrawSurface7>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
        } else if constexpr (std::is_same<SurfaceType, IDirectDrawSurface4>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces4Callback);
        } else if constexpr (std::is_same<SurfaceType, IDirectDrawSurface>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
        } else {
          Logger::err("BlitToD3D9Texture: Unsupported surface type");
        }
      } else {
        Logger::warn(str::format("BlitToD3D9Texture: Failed to lock D3D9 mip ", i));
      }
    }
  }

  template <typename SurfaceType, typename DescType>
  inline void BlitToD3D9Surface(
      d3d9::IDirect3DSurface9* surface9,
      d3d9::D3DFORMAT format9,
      SurfaceType* surface) {
    d3d9::D3DLOCKED_RECT rect9;
    // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
    HRESULT hr9 = surface9->LockRect(&rect9, 0, D3DLOCK_DISCARD);
    if (likely(SUCCEEDED(hr9))) {
      DescType desc;
      desc.dwSize = sizeof(DescType);
      HRESULT hr = surface->Lock(0, &desc, DDLOCK_READONLY, 0);
      if (likely(SUCCEEDED(hr))) {
        Logger::debug(str::format("desc.dwWidth:  ", desc.dwWidth));
        Logger::debug(str::format("desc.dwHeight: ", desc.dwHeight));
        Logger::debug(str::format("desc.lPitch:   ", desc.lPitch));
        Logger::debug(str::format("rect.Pitch:    ", rect9.Pitch));
        // The lock pitch of a DXT surface represents its entire size, apparently
        if (IsDXTFormat(format9)) {
          const size_t size = static_cast<size_t>(desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          Logger::debug("BlitToD3D9Texture: Done blitting DXT surface");
        } else if (unlikely(desc.lPitch != rect9.Pitch)) {
          Logger::debug("BlitToD3D9Surface: Incompatible surface pitch");

          uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9.pBits);
          uint8_t* data7 = reinterpret_cast<uint8_t*>(desc.lpSurface);

          const size_t copyPitch = std::min<size_t>(desc.lPitch, rect9.Pitch);
          for (uint32_t h = 0; h < desc.dwHeight; h++)
            memcpy(&data9[h * rect9.Pitch], &data7[h * desc.lPitch], copyPitch);

          Logger::debug("BlitToD3D9Surface: Done blitting surface row by row");
        } else {
          const size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          Logger::debug("BlitToD3D9Surface: Done blitting surface");
        }
        surface->Unlock(0);
      } else {
        Logger::warn("BlitToD3D9Surface: Failed to lock surface");
      }
      surface9->UnlockRect();
    } else {
      Logger::warn("BlitToD3D9Surface: Failed to lock D3D9 surface");
    }
  }

  // reverse blitter, used in the forceProxiedPresent logic
  template <typename SurfaceType, typename DescType>
  inline void BlitToDDrawSurface(
      SurfaceType* surface,
      d3d9::IDirect3DSurface9* surface9) {
    DescType desc;
    desc.dwSize = sizeof(DescType);
    HRESULT hr = surface->Lock(0, &desc, DDLOCK_WRITEONLY, 0);
    if (likely(SUCCEEDED(hr))) {
      d3d9::D3DLOCKED_RECT rect9;
      HRESULT hr9 = surface9->LockRect(&rect9, 0, D3DLOCK_READONLY);
      if (likely(SUCCEEDED(hr9))) {
        Logger::debug(str::format("desc.dwWidth:  ", desc.dwWidth));
        Logger::debug(str::format("desc.dwHeight: ", desc.dwHeight));
        Logger::debug(str::format("desc.lPitch:   ", desc.lPitch));
        Logger::debug(str::format("rect.Pitch:    ", rect9.Pitch));
        if (unlikely(desc.lPitch != rect9.Pitch)) {
          Logger::debug("BlitToDDrawSurface: Incompatible surface pitch");

          uint8_t* data7 = reinterpret_cast<uint8_t*>(desc.lpSurface);
          uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9.pBits);

          const size_t copyPitch = std::min<size_t>(desc.lPitch, rect9.Pitch);
          for (uint32_t h = 0; h < desc.dwHeight; h++)
            memcpy(&data7[h * desc.lPitch], &data9[h * rect9.Pitch], copyPitch);

          Logger::debug("BlitToDDrawSurface: Done blitting surface row by row");
        } else {
          const size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
          memcpy(desc.lpSurface, rect9.pBits, size);
          Logger::debug("BlitToDDrawSurface: Done blitting surface");
        }
        surface9->UnlockRect();
      } else {
        Logger::warn("BlitToDDrawSurface: Failed to lock D3D9 surface");
      }
      surface->Unlock(0);
    } else {
      Logger::warn("BlitToDDrawSurface: Failed to lock surface");
    }
  }

}
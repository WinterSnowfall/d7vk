#pragma once

#include "d3d7_include.h"
#include "d3d7_caps.h"

#define GET_BIT_FIELD(token, field) ((token & field ## _MASK) >> field ## _SHIFT)

namespace dxvk {

  // TODO: Make this static and comment the loggers once everything is sorted (probably never)
  inline d3d9::D3DFORMAT ConvertFormat(DDPIXELFORMAT& fmt) {
    if (fmt.dwFlags & DDPF_RGB) {
      Logger::info(str::format("ConvertFormat: fmt.dwRGBBitCount: ",     fmt.dwRGBBitCount));
      Logger::info(str::format("ConvertFormat: fmt.dwRBitMask: ",        fmt.dwRBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwGBitMask: ",        fmt.dwRBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwBBitMask: ",        fmt.dwRBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwRGBAlphaBitMask: ", fmt.dwRGBAlphaBitMask));

      switch (fmt.dwRGBBitCount) {
        case 16: {
          switch (fmt.dwRBitMask) {
            // R: 0000 1111 0000 0000
            // A: 1111 0000 0000 0000
            case (0xF << 8):
              return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A4R4G4B4 : d3d9::D3DFMT_X4R4G4B4;
            // R: 0111 1100 0000 0000
            // A: 1000 0000 0000 0000
            case (0x1F << 10):
              return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A1R5G5B5 : d3d9::D3DFMT_X1R5G5B5;
            // R: 1111 1000 0000 0000
            case (0x1F << 11):
              return d3d9::D3DFMT_R5G6B5;
            // TODO: Check if we've missed anything. Maybe A8?
          }
          return d3d9::D3DFMT_X1R5G5B5;
        }
        case 24:
          // TODO: Anything else here?
          return d3d9::D3DFMT_R8G8B8;
        case 32: {
          switch (fmt.dwRBitMask) {
            // R: 0000 0000 1111 1111 0000 0000 0000 0000
            // A: 1111 1111 0000 0000 0000 0000 0000 0000
            case (0xFF << 16):
              return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A8R8G8B8 : d3d9::D3DFMT_X8R8G8B8;
            // R: 0011 1111 1111 0000 0000 0000 0000 0000
            // A: 1100 0000 0000 0000 0000 0000 0000 0000
            case (0x3FF << 20):
              return d3d9::D3DFMT_A2R10G10B10;
            case (0xFF):
            // R: 0000 0000 0000 0000 0000 0000 1111 1111
            // A: 1111 1111 0000 0000 0000 0000 0000 0000
              return fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A8B8G8R8 : d3d9::D3DFMT_X8B8G8R8;
          }
          return d3d9::D3DFMT_X8B8G8R8;
        }
      }
    // TODO: Check if these are actually correct and work
    } else if ((fmt.dwFlags & DDPF_ZBUFFER)) {
      Logger::info(str::format("ConvertFormat: fmt.dwZBufferBitDepth: ", fmt.dwZBufferBitDepth));
      Logger::info(str::format("ConvertFormat: fmt.dwZBitMask: ",        fmt.dwRBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwStencilBitMask: ",  fmt.dwStencilBitMask));

      switch (fmt.dwZBufferBitDepth) {
        // D: 1111 1111 1111 1110
        // S: 0000 0000 0000 0001
        case 16:
          return fmt.dwStencilBitMask ? d3d9::D3DFMT_D15S1 : d3d9::D3DFMT_D16;
        case 32: {
          switch (fmt.dwStencilBitMask) {
            case 0:
              return d3d9::D3DFMT_D24X8;
            case (0xFF):
              // D: 1111 1111 1111 1111 1111 1111 0000 0000
              // S: 0000 0000 0000 0000 0000 0000 1111 1111
              return d3d9::D3DFMT_D24S8;
            case (0xF):
              // D: 1111 1111 1111 1111 1111 1111 0000 0000
              // S: 0000 0000 0000 0000 0000 0000 0000 1111
              return d3d9::D3DFMT_D24X4S4;
          }
        }
        return d3d9::D3DFMT_D24S8;
      }
    } else if ((fmt.dwFlags & DDPF_ALPHA)) {
      // Alpha only surfaces. I sincerely hope d3d7 doesn't need anything else
      return d3d9::D3DFMT_A8;
    } else if ((fmt.dwFlags & DDPF_LUMINANCE)) {
      switch (fmt.dwLuminanceBitCount) {
        case 8: {
          switch (fmt.dwLuminanceBitMask) {
            // L: 1111 1111
            case (0xF):
              return d3d9::D3DFMT_L8;
            // L: 0000 1111
            // A: 1111 0000
            case (0x8):
              return d3d9::D3DFMT_A4L4;
          }
          return d3d9::D3DFMT_A4L4;
        }
        // L: 0000 0000 1111 1111
        // A: 1111 1111 0000 0000
        case 16:
          return d3d9::D3DFMT_A8L8;
      }
      return d3d9::D3DFMT_A8L8;
    // Hopefully there will be none of this
    } else if ((fmt.dwFlags & DDPF_YUV)) {
      Logger::info(str::format("ConvertFormat: fmt.dwYUVBitCount: ", fmt.dwYUVBitCount));
      Logger::info(str::format("ConvertFormat: fmt.dwYBitMask: ",    fmt.dwYBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwUBitMask: ",    fmt.dwUBitMask));
      Logger::info(str::format("ConvertFormat: fmt.dwVBitMask: ",    fmt.dwVBitMask));

      Logger::warn("ConvertFormat: Unhandled YUV payload");
    // I guess there are some thing here we can convert...
    } else if ((fmt.dwFlags & DDPF_FOURCC)) {
      switch (fmt.dwFourCC) {
        case MAKEFOURCC('U', 'Y', 'V', 'Y'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: UYVY");
          return d3d9::D3DFMT_UYVY;
        case MAKEFOURCC('D', 'X', 'T', '1'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: DXT1");
          return d3d9::D3DFMT_DXT1;
        case MAKEFOURCC('D', 'X', 'T', '2'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: DXT2");
          return d3d9::D3DFMT_DXT2;
        case MAKEFOURCC('D', 'X', 'T', '3'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: DXT3");
          return d3d9::D3DFMT_DXT3;
        case MAKEFOURCC('D', 'X', 'T', '4'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: DXT4");
          return d3d9::D3DFMT_DXT4;
        case MAKEFOURCC('D', 'X', 'T', '5'):
          Logger::warn("ConvertFormat: Detected a FOURCC payload: DXT5");
          return d3d9::D3DFMT_DXT5;
      }
      Logger::warn("ConvertFormat: Unhandled FOURCC payload");
    } else {
      Logger::warn("ConvertFormat: Unhandled bit payload");
    }

    // TODO: Not sure if this is a good idea or we need a "catch-all"
    return d3d9::D3DFMT_UNKNOWN;
  }

  static inline d3d9::D3DCUBEMAP_FACES GetCubemapFace(DDSURFACEDESC2* desc) {
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) return d3d9::D3DCUBEMAP_FACE_POSITIVE_X;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_X;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) return d3d9::D3DCUBEMAP_FACE_POSITIVE_Y;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Y;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) return d3d9::D3DCUBEMAP_FACE_POSITIVE_Z;
    if (desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Z;
    return d3d9::D3DCUBEMAP_FACE_POSITIVE_X;
  }

  static inline d3d9::D3DTRANSFORMSTATETYPE ConvertTransformState(D3DTRANSFORMSTATETYPE tst) {
    switch (tst) {
      case D3DTRANSFORMSTATE_WORLD:  return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD);
      case D3DTRANSFORMSTATE_WORLD1: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD1);
      case D3DTRANSFORMSTATE_WORLD2: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD2);
      case D3DTRANSFORMSTATE_WORLD3: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD3);
      default: return d3d9::D3DTRANSFORMSTATETYPE(tst);
    }
  }

  static inline DWORD ConvertLockFlags(DWORD lockFlags) {
    DWORD lockFlagsD3D9 = 0;

    if (lockFlags & DDLOCK_NOSYSLOCK) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOSYSLOCK;
    }
    // Not sure if both can happen at the same time, but play it safe
    if ((lockFlags & DDLOCK_READONLY) && !(lockFlags & DDLOCK_WRITEONLY)) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_READONLY;
    }
    // Flipped logic between d3d7 and d3d9
    if (!(lockFlags & DDLOCK_WAIT)) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DONOTWAIT;
    }
    if (lockFlags & DDLOCK_DISCARDCONTENTS) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DISCARD;
    }
    if (lockFlags & DDLOCK_NOOVERWRITE) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOOVERWRITE;
    }

    return lockFlagsD3D9;
  }

  static inline DWORD ConvertUsageFlags(DWORD usageFlags) {
    DWORD usageFlagsD3D9 = 0;

    if (usageFlags & D3DVBCAPS_DONOTCLIP) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_DONOTCLIP;
    }
    // Technically for SWVP, but let's ignore it
    /*if (usageFlags & D3DVBCAPS_SYSTEMMEMORY) {
      lockFlagsD3D9 |= (DWORD)D3DUSAGE_SOFTWAREPROCESSING;
    }*/
    if (usageFlags & D3DVBCAPS_WRITEONLY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_WRITEONLY;
    }

    return usageFlagsD3D9;
  }

  static inline size_t GetFVFSize(DWORD fvf) {
    size_t size = 0;

    switch (fvf & D3DFVF_POSITION_MASK) {
      case D3DFVF_XYZ:
          size += 3 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZRHW:
          size += 4 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZB1:
          size += 4 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZB2:
          size += 5 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZB3:
          size += 6 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZB4:
          size += 7 * sizeof(FLOAT);
          break;
      case D3DFVF_XYZB5:
          size += 8 * sizeof(FLOAT);
          break;
    }

    if (fvf & D3DFVF_NORMAL) {
        size += 3 * sizeof(FLOAT);
    }
    if (fvf & D3DFVF_RESERVED1) {
        size += sizeof(DWORD);
    }
    if (fvf & D3DFVF_DIFFUSE) {
        size += sizeof(D3DCOLOR);
    }
    if (fvf & D3DFVF_SPECULAR) {
        size += sizeof(D3DCOLOR);
    }

    DWORD textureCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (DWORD coord = 0; coord < textureCount; ++coord) {

      DWORD texCoordSize = (fvf >> (coord * 2 + 16)) & 3;

      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
            size += 1 * sizeof(FLOAT);
            break;
        case D3DFVF_TEXTUREFORMAT2:
            size += 2 * sizeof(FLOAT);
            break;
        case D3DFVF_TEXTUREFORMAT3:
            size += 3 * sizeof(FLOAT);
            break;
        case D3DFVF_TEXTUREFORMAT4:
            size += 4 * sizeof(FLOAT);
            break;
      }
    }

    return size;
  }


  static inline UINT GetPrimitiveCount(D3DPRIMITIVETYPE PrimitiveType, DWORD VertexCount) {
    switch (PrimitiveType) {
      default:
      case D3DPT_TRIANGLELIST:  return static_cast<UINT>(VertexCount / 3);
      case D3DPT_POINTLIST:     return static_cast<UINT>(VertexCount);
      case D3DPT_LINELIST:      return static_cast<UINT>(VertexCount / 2);
      case D3DPT_LINESTRIP:     return static_cast<UINT>(VertexCount - 1);
      case D3DPT_TRIANGLESTRIP: return static_cast<UINT>(VertexCount - 2);
      case D3DPT_TRIANGLEFAN:   return static_cast<UINT>(VertexCount - 2);
    }
  }

  // If this D3DTEXTURESTAGESTATETYPE has been remapped to a d3d9::D3DSAMPLERSTATETYPE
  // it will be returned, otherwise returns -1
  static inline d3d9::D3DSAMPLERSTATETYPE ConvertSamplerStateType(const D3DTEXTURESTAGESTATETYPE StageType) {
    switch (StageType) {
      // 13-21:
      case D3DTSS_ADDRESSU:       return d3d9::D3DSAMP_ADDRESSU;
      case D3DTSS_ADDRESSV:       return d3d9::D3DSAMP_ADDRESSV;
      case D3DTSS_BORDERCOLOR:    return d3d9::D3DSAMP_BORDERCOLOR;
      case D3DTSS_MAGFILTER:      return d3d9::D3DSAMP_MAGFILTER;
      case D3DTSS_MINFILTER:      return d3d9::D3DSAMP_MINFILTER;
      case D3DTSS_MIPFILTER:      return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MIPMAPLODBIAS:  return d3d9::D3DSAMP_MIPMAPLODBIAS;
      case D3DTSS_MAXMIPLEVEL:    return d3d9::D3DSAMP_MAXMIPLEVEL;
      case D3DTSS_MAXANISOTROPY:  return d3d9::D3DSAMP_MAXANISOTROPY;
      default:                    return d3d9::D3DSAMPLERSTATETYPE(-1);
    }
  }

  static inline D3DDEVICEDESC7 GetBaseD3D7Caps() {
    D3DDEVICEDESC7 desc7;

    desc7.dwDevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL
                    | D3DDEVCAPS_CANRENDERAFTERFLIP
                    | D3DDEVCAPS_DRAWPRIMTLVERTEX
                    | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                    | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                    | D3DDEVCAPS_FLOATTLVERTEX
                    | D3DDEVCAPS_HWRASTERIZATION
                    | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                 // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                    | D3DDEVCAPS_SORTDECREASINGZ
                    | D3DDEVCAPS_SORTEXACT
                    | D3DDEVCAPS_SORTINCREASINGZ
                 // | D3DDEVCAPS_STRIDEDVERTICES // Mentioned in the docs, but apparently is a ghost
                    | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
                 // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                    | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                    | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                    | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    D3DPRIMCAPS prim;
    prim.dwSize = sizeof(D3DPRIMCAPS);

    prim.dwMiscCaps           = D3DPMISCCAPS_CONFORMANT
                              | D3DPMISCCAPS_CULLCCW
                              | D3DPMISCCAPS_CULLCW
                              | D3DPMISCCAPS_CULLNONE
                           // | D3DPMISCCAPS_LINEPATTERNREP // Not implemented in D3D9
                              | D3DPMISCCAPS_MASKPLANES
                              | D3DPMISCCAPS_MASKZ;

    prim.dwRasterCaps         = D3DPRASTERCAPS_ANISOTROPY
                              | D3DPRASTERCAPS_ANTIALIASEDGES // Technically not implemented in D3D9
                              | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                              | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT // Not implemented in D3D9
                              | D3DPRASTERCAPS_ROP2
                              | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL // I guess...
                           // | D3DPRASTERCAPS_SUBPIXELX
                              | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                              | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                              | D3DPRASTERCAPS_ZBUFFERLESSHSR
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    prim.dwZCmpCaps           = D3DPCMPCAPS_ALWAYS
                              | D3DPCMPCAPS_EQUAL
                              | D3DPCMPCAPS_GREATER
                              | D3DPCMPCAPS_GREATEREQUAL
                              | D3DPCMPCAPS_LESS
                              | D3DPCMPCAPS_LESSEQUAL
                              | D3DPCMPCAPS_NEVER
                              | D3DPCMPCAPS_NOTEQUAL;

    prim.dwSrcBlendCaps       = D3DPBLENDCAPS_BOTHINVSRCALPHA
                              | D3DPBLENDCAPS_BOTHSRCALPHA
                              | D3DPBLENDCAPS_DESTALPHA
                              | D3DPBLENDCAPS_DESTCOLOR
                              | D3DPBLENDCAPS_INVDESTALPHA
                              | D3DPBLENDCAPS_INVDESTCOLOR
                              | D3DPBLENDCAPS_INVSRCALPHA
                              | D3DPBLENDCAPS_INVSRCCOLOR
                              | D3DPBLENDCAPS_ONE
                              | D3DPBLENDCAPS_SRCALPHA
                              | D3DPBLENDCAPS_SRCALPHASAT
                              | D3DPBLENDCAPS_SRCCOLOR
                              | D3DPBLENDCAPS_ZERO;

    prim.dwDestBlendCaps      = prim.dwSrcBlendCaps;

    prim.dwAlphaCmpCaps       = prim.dwZCmpCaps;

    prim.dwShadeCaps          = D3DPSHADECAPS_ALPHAFLATBLEND
                              | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                              | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                              | D3DPSHADECAPS_ALPHAPHONGBLEND
                              | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                              | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                              | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                              | D3DPSHADECAPS_COLORPHONGMONO
                              | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                              | D3DPSHADECAPS_FOGPHONG
                              | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                              | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB
                              | D3DPSHADECAPS_SPECULARPHONGMONO
                              | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_ALPHAPALETTE
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_COLORKEYBLEND // Technically not implemented/present in D3D8/D3D9
                              | D3DPTEXTURECAPS_CUBEMAP
                           // | D3DPTEXTURECAPS_NONPOW2CONDITIONAL
                              | D3DPTEXTURECAPS_PERSPECTIVE
                           // | D3DPTEXTURECAPS_POW2
                              | D3DPTEXTURECAPS_PROJECTED
                           // | D3DPTEXTURECAPS_SQUAREONLY
                              | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE
                              | D3DPTEXTURECAPS_TRANSPARENCY;

    prim.dwTextureFilterCaps  = D3DPTFILTERCAPS_LINEAR
                              | D3DPTFILTERCAPS_LINEARMIPLINEAR
                              | D3DPTFILTERCAPS_LINEARMIPNEAREST
                              | D3DPTFILTERCAPS_MIPLINEAR
                              | D3DPTFILTERCAPS_MIPNEAREST
                              | D3DPTFILTERCAPS_NEAREST
                           // | D3DPTFILTERCAPS_MAGFAFLATCUBIC
                              | D3DPTFILTERCAPS_MAGFANISOTROPIC
                           // | D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC
                              | D3DPTFILTERCAPS_MAGFLINEAR
                              | D3DPTFILTERCAPS_MAGFPOINT
                              | D3DPTFILTERCAPS_MINFANISOTROPIC
                              | D3DPTFILTERCAPS_MINFLINEAR
                              | D3DPTFILTERCAPS_MINFPOINT
                              | D3DPTFILTERCAPS_MIPFLINEAR
                              | D3DPTFILTERCAPS_MIPFPOINT;

    prim.dwTextureBlendCaps   = 0;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 32;
    prim.dwStippleHeight      = 32;

    desc7.dpcLineCaps         = prim;
    desc7.dpcTriCaps          = prim;

    desc7.dwDeviceRenderBitDepth   = DDBD_16 | DDBD_24 | DDBD_32;
    desc7.dwDeviceZBufferBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc7.dwMinTextureWidth        = 0;
    desc7.dwMinTextureHeight       = 0;
    desc7.dwMaxTextureWidth        = caps7::MaxTextureDimension;
    desc7.dwMaxTextureHeight       = caps7::MaxTextureDimension;
    desc7.dwMaxTextureRepeat       = 8192;
    desc7.dwMaxTextureAspectRatio  = 8192;
    desc7.dwMaxAnisotropy          = 16;
    desc7.dvGuardBandLeft          = -32768.0f;
    desc7.dvGuardBandTop           = -32768.0f;
    desc7.dvGuardBandRight         = 32768.0f;
    desc7.dvGuardBandBottom        = 32768.0f;
    desc7.dvExtentsAdjust          = 0.0f;

    desc7.dwStencilCaps        = D3DSTENCILCAPS_DECR
                               | D3DSTENCILCAPS_DECRSAT
                               | D3DSTENCILCAPS_INCR
                               | D3DSTENCILCAPS_INCRSAT
                               | D3DSTENCILCAPS_INVERT
                               | D3DSTENCILCAPS_KEEP
                               | D3DSTENCILCAPS_REPLACE
                               | D3DSTENCILCAPS_ZERO;

    desc7.dwFVFCaps                = (caps7::MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK);
                                /* | D3DFVFCAPS_DONOTSTRIPELEMENTS; */

    desc7.dwTextureOpCaps          = D3DTEXOPCAPS_ADD
                                   | D3DTEXOPCAPS_ADDSIGNED
                                   | D3DTEXOPCAPS_ADDSIGNED2X
                                   | D3DTEXOPCAPS_ADDSMOOTH
                                   | D3DTEXOPCAPS_BLENDCURRENTALPHA
                                   | D3DTEXOPCAPS_BLENDDIFFUSEALPHA
                                   | D3DTEXOPCAPS_BLENDFACTORALPHA
                                   | D3DTEXOPCAPS_BLENDTEXTUREALPHA
                                   | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
                                   | D3DTEXOPCAPS_BUMPENVMAP
                                   | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
                                   | D3DTEXOPCAPS_DISABLE
                                   | D3DTEXOPCAPS_DOTPRODUCT3
                                   | D3DTEXOPCAPS_MODULATE
                                   | D3DTEXOPCAPS_MODULATE2X
                                   | D3DTEXOPCAPS_MODULATE4X
                                   | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR
                                   | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
                                   | D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR
                                   | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
                                   | D3DTEXOPCAPS_PREMODULATE
                                   | D3DTEXOPCAPS_SELECTARG1
                                   | D3DTEXOPCAPS_SELECTARG2
                                   | D3DTEXOPCAPS_SUBTRACT;

    desc7.wMaxTextureBlendStages   = caps7::MaxTextureBlendStages;
    desc7.wMaxSimultaneousTextures = caps7::MaxSimultaneousTextures;
    desc7.dwMaxActiveLights        = caps7::MaxEnabledLights;
    desc7.dvMaxVertexW             = 1e10f;
    desc7.wMaxUserClipPlanes       = caps7::MaxClipPlanes;
    desc7.wMaxVertexBlendMatrices  = 4;

    desc7.dwVertexProcessingCaps   = D3DVTXPCAPS_TEXGEN
                                   | D3DVTXPCAPS_MATERIALSOURCE7
                                   | D3DVTXPCAPS_VERTEXFOG
                                   | D3DVTXPCAPS_DIRECTIONALLIGHTS
                                   | D3DVTXPCAPS_POSITIONALLIGHTS;
                                // | D3DVTXPCAPS_NONLOCALVIEWER; // Described in the official docs, otherwise a ghost

    return desc7;
  }

  static inline bool IsDXTFormat(d3d9::D3DFORMAT fmt) {
    return fmt == d3d9::D3DFMT_DXT1
        || fmt == d3d9::D3DFMT_DXT2
        || fmt == d3d9::D3DFMT_DXT3
        || fmt == d3d9::D3DFMT_DXT4
        || fmt == d3d9::D3DFMT_DXT5;
  }

}
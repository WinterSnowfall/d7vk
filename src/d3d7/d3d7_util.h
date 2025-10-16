#pragma once

#include "d3d7_include.h"
#include "d3d7_caps.h"

#define GET_BIT_FIELD(token, field) ((token & field ## _MASK) >> field ## _SHIFT)

namespace dxvk {

  inline d3d9::D3DFORMAT ConvertFormat(DDPIXELFORMAT& fmt) {
    if (fmt.dwFlags & DDPF_RGB) {
      // R bitmask: 0111 1100 0000 0000
      // G bitmask: 0000 0011 1110 0000
      // B bitmask: 0000 0000 0001 1111
      switch (fmt.dwRGBBitCount) {
        case 16:
          return d3d9::D3DFMT_X1R5G5B5;
        case 32:
          return d3d9::D3DFMT_A8R8G8B8;
      }
    }
    return d3d9::D3DFMT_A8B8G8R8;
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

  inline d3d9::D3DTRANSFORMSTATETYPE ConvertTransformState(D3DTRANSFORMSTATETYPE tst) {
    switch (tst) {
      case D3DTRANSFORMSTATE_WORLD:  return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD);
      case D3DTRANSFORMSTATE_WORLD1: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD1);
      case D3DTRANSFORMSTATE_WORLD2: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD2);
      case D3DTRANSFORMSTATE_WORLD3: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD3);
      default: return d3d9::D3DTRANSFORMSTATETYPE(tst);
    }
  }

  inline DWORD ConvertLockFlags(DWORD lockFlags) {
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

  inline DWORD ConvertUsageFlags(DWORD usageFlags) {
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

  inline size_t GetFVFSize(DWORD FVF) {
    size_t stride = 3;
    switch (FVF & D3DFVF_POSITION_MASK) {
      case D3DFVF_XYZRHW: stride += 1; break;
      case D3DFVF_XYZB1:  stride += 1; break;
      case D3DFVF_XYZB2:  stride += 2; break;
      case D3DFVF_XYZB3:  stride += 3; break;
      case D3DFVF_XYZB4:  stride += 4; break;
      case D3DFVF_XYZB5:  stride += 5; break;
    }
    if (FVF & D3DFVF_NORMAL)    stride += 3;
    if (FVF & D3DFVF_RESERVED1) stride += 1; // TODO: What is D3DFVF_RESERVED1? Point size?
    if (FVF & D3DFVF_DIFFUSE)   stride += 1;
    if (FVF & D3DFVF_SPECULAR)  stride += 1;

    if (FVF & D3DFVF_TEXCOUNT_MASK) {
      DWORD texCount = GET_BIT_FIELD(FVF, D3DFVF_TEXCOUNT);
      for (DWORD i = 0; i < texCount; i++) {
        if      ((FVF & D3DFVF_TEXCOORDSIZE1(i)) == D3DFVF_TEXCOORDSIZE1(i)) stride += 1;  // & 3
        else if ((FVF & D3DFVF_TEXCOORDSIZE3(i)) == D3DFVF_TEXCOORDSIZE3(i)) stride += 3;  // & 1
        else if ((FVF & D3DFVF_TEXCOORDSIZE4(i)) == D3DFVF_TEXCOORDSIZE4(i)) stride += 4;  // & 2
        else if ((FVF & D3DFVF_TEXCOORDSIZE2(i)) == D3DFVF_TEXCOORDSIZE2(i)) stride += 2;  // & 0 (default)
      }
    }
    return stride * sizeof(float);
  }

  inline UINT GetPrimitiveCount(D3DPRIMITIVETYPE PrimitiveType, DWORD VertexCount) {
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
  // TODO: Verify.
  inline d3d9::D3DSAMPLERSTATETYPE ConvertSamplerStateType(const D3DTEXTURESTAGESTATETYPE StageType) {
    switch (StageType) {
      // 13-21:
      case D3DTSS_ADDRESSU:       return d3d9::D3DSAMP_ADDRESSU;
      case D3DTSS_ADDRESSV:       return d3d9::D3DSAMP_ADDRESSV;
      case D3DTSS_BORDERCOLOR:    return d3d9::D3DSAMP_BORDERCOLOR;
      case D3DTSS_MAGFILTER:      return d3d9::D3DSAMP_MAGFILTER;
      case D3DTSS_MINFILTER:      return d3d9::D3DSAMP_MINFILTER;
      case D3DTSS_MIPFILTER:      return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MIPMAPLODBIAS:  return d3d9::D3DSAMP_MIPMAPLODBIAS;
      case D3DTSS_MAXMIPLEVEL:    return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MAXANISOTROPY:  return d3d9::D3DSAMP_MAXANISOTROPY;
      default:                    return d3d9::D3DSAMPLERSTATETYPE(-1);
    }
  }

  inline D3DDEVICEDESC7 GetBaseD3D7Caps() {
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
                              | D3DPMISCCAPS_MASKPLANES // ?????
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
                              | D3DPRASTERCAPS_ROP2 // ?????
                              | D3DPRASTERCAPS_STIPPLE // Not entirely sure if we should expose this
                              | D3DPRASTERCAPS_SUBPIXEL // I guess...
                           // | D3DPRASTERCAPS_SUBPIXELX
                              | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                              | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                              | D3DPRASTERCAPS_ZBUFFERLESSHSR // Sure, sure
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
                              | D3DPSHADECAPS_ALPHAFLATSTIPPLED // "To stipple or not to stipple?", that is the question
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
                           // | D3DPTEXTURECAPS_COLORKEYBLEND // Let's say we don't support this (not present in d3d8/9)
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

}
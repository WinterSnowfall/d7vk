#pragma once

#include "ddraw_include.h"

#include "ddraw_caps.h"

#include <vector>

namespace dxvk {

  struct PackedVertexBuffer {
    std::vector<uint8_t> vertexData;
    UINT stride;
  };

  inline d3d9::D3DTRANSFORMSTATETYPE ConvertTransformState(D3DTRANSFORMSTATETYPE tst) {
    switch (tst) {
      case D3DTRANSFORMSTATE_WORLD:  return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD);
      case D3DTRANSFORMSTATE_WORLD1: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD1);
      case D3DTRANSFORMSTATE_WORLD2: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD2);
      case D3DTRANSFORMSTATE_WORLD3: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD3);
      default: return d3d9::D3DTRANSFORMSTATETYPE(tst);
    }
  }

  inline DWORD ConvertLockFlags(DWORD lockFlags, bool isSurface) {
    DWORD lockFlagsD3D9 = 0;

    // DDLOCK_WAIT is default for ddraw7 surfaces, so ignore it
    // and only factor in DDLOCK_DONOTWAIT. Buffers locks have
    // a flipped logic compared to d3d9.
    if ((!isSurface && !(lockFlags & DDLOCK_WAIT))
     || (isSurface  &&  (lockFlags & DDLOCK_DONOTWAIT))) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DONOTWAIT;
    }
    if (lockFlags & DDLOCK_NOSYSLOCK) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOSYSLOCK;
    }
    // Not sure if both can happen at the same time, but play it safe
    if ((lockFlags & DDLOCK_READONLY) && !(lockFlags & DDLOCK_WRITEONLY)) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_READONLY;
    }
    // D3D7 only lock flags
    if (lockFlags & DDLOCK_DISCARDCONTENTS) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DISCARD;
    }
    if (lockFlags & DDLOCK_NOOVERWRITE) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOOVERWRITE;
    }

    return lockFlagsD3D9;
  }

  inline DWORD ConvertD3D6UsageFlags(DWORD usageFlags, DWORD creationFlags, d3d9::D3DPOOL pool) {
    DWORD usageFlagsD3D9 = 0;

    // The D3D6 docs do not mention the presence of a D3DVBCAPS_DONOTCLIP flag,
    // and only the creation flag D3DDP_DONOTCLIP is touted as being usable
    if ((usageFlags & D3DVBCAPS_DONOTCLIP) || (creationFlags & D3DDP_DONOTCLIP)) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_DONOTCLIP;
    }
    if (usageFlags & D3DVBCAPS_SYSTEMMEMORY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_SOFTWAREPROCESSING;
    }
    if (usageFlags & D3DVBCAPS_WRITEONLY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_WRITEONLY;
    }

    // D3D6 does not use DDLOCK_DISCARDCONTENTS or
    // DDLOCK_NOOVERWRITE, however, still mark DEFAULT buffers
    // as DYNAMIC to handle any potential CPU read-backs
    return pool == d3d9::D3DPOOL_DEFAULT ? usageFlagsD3D9 | D3DUSAGE_DYNAMIC : usageFlagsD3D9;
  }

  inline DWORD ConvertD3D7UsageFlags(DWORD usageFlags, d3d9::D3DPOOL pool) {
    DWORD usageFlagsD3D9 = 0;

    if (usageFlags & D3DVBCAPS_DONOTCLIP) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_DONOTCLIP;
    }
    if (usageFlags & D3DVBCAPS_SYSTEMMEMORY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_SOFTWAREPROCESSING;
    }
    if (usageFlags & D3DVBCAPS_WRITEONLY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_WRITEONLY;
    }

    // Though D3D7 does not specify it, all D3DPOOL_DEFAULT buffers need
    // to be DYNAMIC, otherwise some lock flags will not work properly
    return pool == d3d9::D3DPOOL_DEFAULT ? usageFlagsD3D9 | D3DUSAGE_DYNAMIC : usageFlagsD3D9;
  }

  inline size_t GetFVFPositionSize(DWORD fvf) {
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

    return size;
  }

  inline size_t GetFVFTexCoordSize(DWORD fvf, DWORD coord) {
    size_t size = 0;

    DWORD texCoordSize = (fvf >> (coord * 2 + 16)) & 0x3;
    switch (texCoordSize) {
      // D3DFVF_TEXTUREFORMAT2 0
      case D3DFVF_TEXTUREFORMAT2:
        size += 2 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT3 1
      case D3DFVF_TEXTUREFORMAT3:
         size += 3 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT4 2
      case D3DFVF_TEXTUREFORMAT4:
        size += 4 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT1 3
      case D3DFVF_TEXTUREFORMAT1:
        size += sizeof(FLOAT);
        break;
    }

    return size;
  }

  inline size_t GetFVFSize(DWORD fvf) {
    size_t size = 0;

    size += GetFVFPositionSize(fvf);

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
      size += GetFVFTexCoordSize(fvf, coord);
    }

    return size;
  }

  inline UINT GetPrimitiveCount(D3DPRIMITIVETYPE PrimitiveType, DWORD VertexCount) {
    switch (PrimitiveType) {
      case D3DPT_POINTLIST:     return static_cast<UINT>(VertexCount);
      case D3DPT_LINELIST:      return static_cast<UINT>(VertexCount / 2);
      case D3DPT_LINESTRIP:     return static_cast<UINT>(VertexCount - 1);
      default:
      case D3DPT_TRIANGLELIST:  return static_cast<UINT>(VertexCount / 3);
      case D3DPT_TRIANGLESTRIP: return static_cast<UINT>(VertexCount - 2);
      case D3DPT_TRIANGLEFAN:   return static_cast<UINT>(VertexCount - 2);
    }
  }

  inline PackedVertexBuffer TransformStridedtoUP(
        DWORD dwFVF,
        LPD3DDRAWPRIMITIVESTRIDEDDATA lpVBStrided,
        DWORD dwNumVertices) {
    PackedVertexBuffer pvb;
    pvb.stride = GetFVFSize(dwFVF);
    pvb.vertexData.resize(pvb.stride * dwNumVertices);

    DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    for (DWORD i = 0; i < dwNumVertices; i++) {
      uint8_t* ptr = pvb.vertexData.data() + i * pvb.stride;

      if ((dwFVF & D3DFVF_POSITION_MASK) && lpVBStrided->position.lpvData) {
        size_t size = GetFVFPositionSize(dwFVF);
        memcpy(ptr, (uint8_t*)lpVBStrided->position.lpvData + i * lpVBStrided->position.dwStride, size);
        ptr += size;
      }

      if ((dwFVF & D3DFVF_NORMAL) && lpVBStrided->normal.lpvData) {
        size_t size = 3 * sizeof(FLOAT);
        memcpy(ptr, (uint8_t*)lpVBStrided->normal.lpvData + i * lpVBStrided->normal.dwStride, size);
        ptr += size;
      }

      if (dwFVF & D3DFVF_RESERVED1) {
        // D3DLVERTEX
        ptr += sizeof(DWORD);
      }

      if ((dwFVF & D3DFVF_DIFFUSE) && lpVBStrided->diffuse.lpvData) {
        size_t size = sizeof(D3DCOLOR);
        memcpy(ptr, (uint8_t*)lpVBStrided->diffuse.lpvData + i * lpVBStrided->diffuse.dwStride, size);
        ptr += size;
      }

      if ((dwFVF & D3DFVF_SPECULAR) && lpVBStrided->specular.lpvData) {
        size_t size = sizeof(D3DCOLOR);
        memcpy(ptr, (uint8_t*)lpVBStrided->specular.lpvData + i * lpVBStrided->specular.dwStride, size);
        ptr += size;
      }

      for (DWORD t = 0; t < dwNumTextures; t++) {
        if (lpVBStrided->textureCoords[t].lpvData) {
          size_t size = GetFVFTexCoordSize(dwFVF, t);
          memcpy(ptr, (uint8_t*)lpVBStrided->textureCoords[t].lpvData + i * lpVBStrided->textureCoords[t].dwStride, size);
          ptr += size;
        }
      }
    }

    return pvb;
  }

  // If this D3DTEXTURESTAGESTATETYPE has been remapped to a d3d9::D3DSAMPLERSTATETYPE
  // it will be returned, otherwise returns -1u
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
      case D3DTSS_MAXMIPLEVEL:    return d3d9::D3DSAMP_MAXMIPLEVEL;
      case D3DTSS_MAXANISOTROPY:  return d3d9::D3DSAMP_MAXANISOTROPY;
      default:                    return d3d9::D3DSAMPLERSTATETYPE(-1u);
    }
  }

  inline D3DDEVICEDESC GetD3D6Caps(const IID rclsid, bool disableAASupport) {
    D3DDEVICEDESC desc;

    desc.dwSize    = sizeof(D3DDEVICEDESC);

    desc.dwFlags   = D3DDD_BCLIPPING
                   | D3DDD_COLORMODEL
                   | D3DDD_DEVCAPS
                   | D3DDD_DEVICERENDERBITDEPTH
                   | D3DDD_DEVICEZBUFFERBITDEPTH
                   | D3DDD_LIGHTINGCAPS
                   | D3DDD_LINECAPS
                   | D3DDD_MAXBUFFERSIZE
                   | D3DDD_MAXVERTEXCOUNT
                   | D3DDD_TRANSFORMCAPS
                   | D3DDD_TRICAPS;

    desc.dcmColorModel = D3DCOLOR_RGB;

    desc.dwDevCaps = D3DDEVCAPS_CANRENDERAFTERFLIP
                   | D3DDEVCAPS_DRAWPRIMTLVERTEX
                   | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                   | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                   | D3DDEVCAPS_FLOATTLVERTEX
                // | D3DDEVCAPS_HWRASTERIZATION
                // | D3DDEVCAPS_DRAWPRIMITIVES2
                // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                // | D3DDEVCAPS_DRAWPRIMITIVES2EX
                // | D3DDEVCAPS_SORTDECREASINGZ
                // | D3DDEVCAPS_SORTEXACT
                   | D3DDEVCAPS_SORTINCREASINGZ // TODO: Check native
                // | D3DDEVCAPS_TEXREPEATNOTSCALEDBYSIZE
                   | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
                // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                   | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                   | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                   | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    if (rclsid == IID_IDirect3DHALDevice) {
      desc.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                      | D3DDEVCAPS_DRAWPRIMITIVES2
                      | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

    D3DTRANSFORMCAPS transformCaps;
    transformCaps.dwSize = sizeof(D3DTRANSFORMCAPS);
    transformCaps.dwCaps = D3DTRANSFORMCAPS_CLIP;

    desc.dtcTransformCaps = transformCaps;

    desc.bClipping = TRUE;

    D3DLIGHTINGCAPS lightingCaps;
    lightingCaps.dwSize  = sizeof(D3DLIGHTINGCAPS);
    lightingCaps.dwCaps  = D3DLIGHTCAPS_DIRECTIONAL
                      // | D3DLIGHTCAPS_GLSPOT
                      // | D3DLIGHTCAPS_PARALLELPOINT // Not supported by D3D9
                         | D3DLIGHTCAPS_POINT
                         | D3DLIGHTCAPS_SPOT;
    lightingCaps.dwLightingModel = D3DLIGHTINGMODEL_RGB;
    lightingCaps.dwNumLights = ddrawCaps::MaxEnabledLights;

    desc.dlcLightingCaps = lightingCaps;

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
                           // | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                           // | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT // Not implemented in D3D9
                              | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                              | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                              | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                           // | D3DPRASTERCAPS_ZBUFFERLESSHSR // Easy footgun to not get a z-buffer
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    if (likely(!disableAASupport)) {
      prim.dwRasterCaps |= D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                        |  D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT;
    }

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
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                              | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                              | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                              | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                              | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_ALPHAPALETTE
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_COLORKEYBLEND // Hard required, but not implemented in D3D9
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

    // Allegedly a deprecated item, but some d3d7 games,
    // like Summoner, still expect it to contain legacy values
    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_ADD
                              | D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 32;
    prim.dwStippleHeight      = 32;

    desc.dpcLineCaps         = prim;
    desc.dpcTriCaps          = prim;

    desc.dwDeviceRenderBitDepth   = DDBD_16 | DDBD_32;
    desc.dwDeviceZBufferBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc.dwMaxBufferSize          = 0;
    desc.dwMaxVertexCount         = D3DDD_MAXVERTEXCOUNT;
    desc.dwMinTextureWidth        = 0;
    desc.dwMinTextureHeight       = 0;
    desc.dwMaxTextureWidth        = ddrawCaps::MaxTextureDimension;
    desc.dwMaxTextureHeight       = ddrawCaps::MaxTextureDimension;
    desc.dwMaxTextureRepeat       = 8192;
    desc.dwMaxTextureAspectRatio  = 8192;
    desc.dwMaxAnisotropy          = 16;
    desc.dvGuardBandLeft          = -32768.0f;
    desc.dvGuardBandTop           = -32768.0f;
    desc.dvGuardBandRight         = 32768.0f;
    desc.dvGuardBandBottom        = 32768.0f;
    desc.dvExtentsAdjust          = 0.0f;

    desc.dwStencilCaps        = D3DSTENCILCAPS_DECR
                              | D3DSTENCILCAPS_DECRSAT
                              | D3DSTENCILCAPS_INCR
                              | D3DSTENCILCAPS_INCRSAT
                              | D3DSTENCILCAPS_INVERT
                              | D3DSTENCILCAPS_KEEP
                              | D3DSTENCILCAPS_REPLACE
                              | D3DSTENCILCAPS_ZERO;

    desc.dwFVFCaps                = (ddrawCaps::MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK);
                                /* | D3DFVFCAPS_DONOTSTRIPELEMENTS; */

    desc.dwTextureOpCaps          = D3DTEXOPCAPS_ADD
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

    desc.wMaxTextureBlendStages   = ddrawCaps::MaxTextureBlendStages;
    desc.wMaxSimultaneousTextures = ddrawCaps::MaxSimultaneousTextures;

    return desc;
  }

  inline D3DDEVICEDESC7 GetD3D7Caps(const IID rclsid, bool disableAASupport) {
    D3DDEVICEDESC7 desc7;

    desc7.dwDevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL
                    | D3DDEVCAPS_CANRENDERAFTERFLIP
                    | D3DDEVCAPS_DRAWPRIMTLVERTEX
                    | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                    | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                    | D3DDEVCAPS_FLOATTLVERTEX
                 // | D3DDEVCAPS_HWRASTERIZATION
                 // | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                 // | D3DDEVCAPS_DRAWPRIMITIVES2
                 // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                 // | D3DDEVCAPS_DRAWPRIMITIVES2EX
                 // | D3DDEVCAPS_SORTDECREASINGZ
                 // | D3DDEVCAPS_SORTEXACT
                    | D3DDEVCAPS_SORTINCREASINGZ // TODO: Check native
                 // | D3DDEVCAPS_STRIDEDVERTICES // Mentioned in the docs, but apparently is a ghost
                    | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
                 // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                    | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                    | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                    | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      desc7.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                       | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                       | D3DDEVCAPS_DRAWPRIMITIVES2
                       | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }
    else if (rclsid == IID_IDirect3DHALDevice) {
      desc7.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                       | D3DDEVCAPS_DRAWPRIMITIVES2
                       | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

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
                           // | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                           // | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT // Not implemented in D3D9
                              | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                              | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                              | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                           // | D3DPRASTERCAPS_ZBUFFERLESSHSR // Easy footgun to not get a z-buffer
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    if (likely(!disableAASupport)) {
      prim.dwRasterCaps |= D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                        |  D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT;
    }

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
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                              | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                              | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                              | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                              | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_ALPHAPALETTE
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_COLORKEYBLEND // Hard required, but not implemented in D3D9
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

    // Allegedly a deprecated item, but some d3d7 games,
    // like Summoner, still expect it to contain legacy values
    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_ADD
                              | D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 32;
    prim.dwStippleHeight      = 32;

    desc7.dpcLineCaps         = prim;
    desc7.dpcTriCaps          = prim;

    desc7.dwDeviceRenderBitDepth   = DDBD_16 | DDBD_32;
    desc7.dwDeviceZBufferBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc7.dwMinTextureWidth        = 0;
    desc7.dwMinTextureHeight       = 0;
    desc7.dwMaxTextureWidth        = ddrawCaps::MaxTextureDimension;
    desc7.dwMaxTextureHeight       = ddrawCaps::MaxTextureDimension;
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

    desc7.dwFVFCaps                = (ddrawCaps::MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK);
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

    desc7.wMaxTextureBlendStages   = ddrawCaps::MaxTextureBlendStages;
    desc7.wMaxSimultaneousTextures = ddrawCaps::MaxSimultaneousTextures;
    desc7.dwMaxActiveLights        = ddrawCaps::MaxEnabledLights;
    desc7.dvMaxVertexW             = 1e10f;

    desc7.deviceGUID               = rclsid;

    desc7.wMaxUserClipPlanes       = ddrawCaps::MaxClipPlanes;
    desc7.wMaxVertexBlendMatrices  = 4;

    desc7.dwVertexProcessingCaps   = D3DVTXPCAPS_TEXGEN
                                   | D3DVTXPCAPS_MATERIALSOURCE7
                                   | D3DVTXPCAPS_VERTEXFOG
                                   | D3DVTXPCAPS_DIRECTIONALLIGHTS
                                   | D3DVTXPCAPS_POSITIONALLIGHTS;
                                // | D3DVTXPCAPS_NONLOCALVIEWER; // Described in the official docs, otherwise a ghost

    return desc7;
  }

  inline bool IsValidD3D6RenderStateType(D3DRENDERSTATETYPE rs) {
    return rs == D3DRENDERSTATE_TEXTUREHANDLE
        || rs == D3DRENDERSTATE_ANTIALIAS
        || rs == D3DRENDERSTATE_TEXTUREADDRESS
        || rs == D3DRENDERSTATE_TEXTUREPERSPECTIVE
        || rs == D3DRENDERSTATE_WRAPU
        || rs == D3DRENDERSTATE_WRAPV
        || rs == D3DRENDERSTATE_ZENABLE
        || rs == D3DRENDERSTATE_FILLMODE
        || rs == D3DRENDERSTATE_SHADEMODE
        || rs == D3DRENDERSTATE_LINEPATTERN
        || rs == D3DRENDERSTATE_MONOENABLE
        || rs == D3DRENDERSTATE_ROP2
        || rs == D3DRENDERSTATE_PLANEMASK
        || rs == D3DRENDERSTATE_ZWRITEENABLE
        || rs == D3DRENDERSTATE_ALPHATESTENABLE
        || rs == D3DRENDERSTATE_LASTPIXEL
        || rs == D3DRENDERSTATE_TEXTUREMAG
        || rs == D3DRENDERSTATE_TEXTUREMIN
        || rs == D3DRENDERSTATE_SRCBLEND
        || rs == D3DRENDERSTATE_DESTBLEND
        || rs == D3DRENDERSTATE_TEXTUREMAPBLEND
        || rs == D3DRENDERSTATE_CULLMODE
        || rs == D3DRENDERSTATE_ZFUNC
        || rs == D3DRENDERSTATE_ALPHAREF
        || rs == D3DRENDERSTATE_ALPHAFUNC
        || rs == D3DRENDERSTATE_DITHERENABLE
        || rs == D3DRENDERSTATE_ALPHABLENDENABLE
        || rs == D3DRENDERSTATE_FOGENABLE
        || rs == D3DRENDERSTATE_SPECULARENABLE
        || rs == D3DRENDERSTATE_ZVISIBLE
        || rs == D3DRENDERSTATE_SUBPIXEL
        || rs == D3DRENDERSTATE_SUBPIXELX
        || rs == D3DRENDERSTATE_STIPPLEDALPHA
        || rs == D3DRENDERSTATE_FOGCOLOR
        || rs == D3DRENDERSTATE_FOGTABLEMODE
        || rs == D3DRENDERSTATE_FOGTABLESTART
        || rs == D3DRENDERSTATE_FOGTABLEEND
        || rs == D3DRENDERSTATE_FOGTABLEDENSITY
        || rs == D3DRENDERSTATE_STIPPLEENABLE
        || rs == D3DRENDERSTATE_EDGEANTIALIAS
        || rs == D3DRENDERSTATE_COLORKEYENABLE
        || rs == D3DRENDERSTATE_BORDERCOLOR
        || rs == D3DRENDERSTATE_TEXTUREADDRESSU
        || rs == D3DRENDERSTATE_TEXTUREADDRESSV
        || rs == D3DRENDERSTATE_MIPMAPLODBIAS
        || rs == D3DRENDERSTATE_ZBIAS
        || rs == D3DRENDERSTATE_RANGEFOGENABLE
        || rs == D3DRENDERSTATE_ANISOTROPY
        || rs == D3DRENDERSTATE_FLUSHBATCH
        || rs == D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT
        || rs == D3DRENDERSTATE_STENCILENABLE
        || rs == D3DRENDERSTATE_STENCILFAIL
        || rs == D3DRENDERSTATE_STENCILZFAIL
        || rs == D3DRENDERSTATE_STENCILPASS
        || rs == D3DRENDERSTATE_STENCILFUNC
        || rs == D3DRENDERSTATE_STENCILREF
        || rs == D3DRENDERSTATE_STENCILMASK
        || rs == D3DRENDERSTATE_STENCILWRITEMASK
        || rs == D3DRENDERSTATE_TEXTUREFACTOR
        || rs == D3DRENDERSTATE_STIPPLEPATTERN00
        || rs == D3DRENDERSTATE_STIPPLEPATTERN01
        || rs == D3DRENDERSTATE_STIPPLEPATTERN02
        || rs == D3DRENDERSTATE_STIPPLEPATTERN03
        || rs == D3DRENDERSTATE_STIPPLEPATTERN04
        || rs == D3DRENDERSTATE_STIPPLEPATTERN05
        || rs == D3DRENDERSTATE_STIPPLEPATTERN06
        || rs == D3DRENDERSTATE_STIPPLEPATTERN07
        || rs == D3DRENDERSTATE_STIPPLEPATTERN08
        || rs == D3DRENDERSTATE_STIPPLEPATTERN09
        || rs == D3DRENDERSTATE_STIPPLEPATTERN10
        || rs == D3DRENDERSTATE_STIPPLEPATTERN11
        || rs == D3DRENDERSTATE_STIPPLEPATTERN12
        || rs == D3DRENDERSTATE_STIPPLEPATTERN13
        || rs == D3DRENDERSTATE_STIPPLEPATTERN14
        || rs == D3DRENDERSTATE_STIPPLEPATTERN15
        || rs == D3DRENDERSTATE_STIPPLEPATTERN16
        || rs == D3DRENDERSTATE_STIPPLEPATTERN17
        || rs == D3DRENDERSTATE_STIPPLEPATTERN18
        || rs == D3DRENDERSTATE_STIPPLEPATTERN19
        || rs == D3DRENDERSTATE_STIPPLEPATTERN20
        || rs == D3DRENDERSTATE_STIPPLEPATTERN21
        || rs == D3DRENDERSTATE_STIPPLEPATTERN22
        || rs == D3DRENDERSTATE_STIPPLEPATTERN23
        || rs == D3DRENDERSTATE_STIPPLEPATTERN24
        || rs == D3DRENDERSTATE_STIPPLEPATTERN25
        || rs == D3DRENDERSTATE_STIPPLEPATTERN26
        || rs == D3DRENDERSTATE_STIPPLEPATTERN27
        || rs == D3DRENDERSTATE_STIPPLEPATTERN28
        || rs == D3DRENDERSTATE_STIPPLEPATTERN29
        || rs == D3DRENDERSTATE_STIPPLEPATTERN30
        || rs == D3DRENDERSTATE_STIPPLEPATTERN31
        || rs == D3DRENDERSTATE_WRAP0
        || rs == D3DRENDERSTATE_WRAP1
        || rs == D3DRENDERSTATE_WRAP2
        || rs == D3DRENDERSTATE_WRAP3
        || rs == D3DRENDERSTATE_WRAP4
        || rs == D3DRENDERSTATE_WRAP5
        || rs == D3DRENDERSTATE_WRAP6
        || rs == D3DRENDERSTATE_WRAP7;
  }

  inline bool IsValidD3D7RenderStateType(D3DRENDERSTATETYPE rs) {
    return rs == D3DRENDERSTATE_ANTIALIAS
        || rs == D3DRENDERSTATE_TEXTUREPERSPECTIVE
        || rs == D3DRENDERSTATE_ZENABLE
        || rs == D3DRENDERSTATE_FILLMODE
        || rs == D3DRENDERSTATE_SHADEMODE
        || rs == D3DRENDERSTATE_LINEPATTERN
        || rs == D3DRENDERSTATE_ZWRITEENABLE
        || rs == D3DRENDERSTATE_ALPHATESTENABLE
        || rs == D3DRENDERSTATE_LASTPIXEL
        || rs == D3DRENDERSTATE_SRCBLEND
        || rs == D3DRENDERSTATE_DESTBLEND
        || rs == D3DRENDERSTATE_CULLMODE
        || rs == D3DRENDERSTATE_ZFUNC
        || rs == D3DRENDERSTATE_ALPHAREF
        || rs == D3DRENDERSTATE_ALPHAFUNC
        || rs == D3DRENDERSTATE_DITHERENABLE
        || rs == D3DRENDERSTATE_ALPHABLENDENABLE
        || rs == D3DRENDERSTATE_FOGENABLE
        || rs == D3DRENDERSTATE_SPECULARENABLE
        || rs == D3DRENDERSTATE_ZVISIBLE
        || rs == D3DRENDERSTATE_STIPPLEDALPHA
        || rs == D3DRENDERSTATE_FOGCOLOR
        || rs == D3DRENDERSTATE_FOGTABLEMODE
        || rs == D3DRENDERSTATE_FOGTABLESTART
        || rs == D3DRENDERSTATE_FOGTABLEEND
        || rs == D3DRENDERSTATE_FOGTABLEDENSITY
        || rs == D3DRENDERSTATE_FOGSTART
        || rs == D3DRENDERSTATE_FOGEND
        || rs == D3DRENDERSTATE_FOGDENSITY
        || rs == D3DRENDERSTATE_EDGEANTIALIAS
        || rs == D3DRENDERSTATE_COLORKEYENABLE
        || rs == D3DRENDERSTATE_ZBIAS
        || rs == D3DRENDERSTATE_RANGEFOGENABLE
        || rs == D3DRENDERSTATE_STENCILENABLE
        || rs == D3DRENDERSTATE_STENCILFAIL
        || rs == D3DRENDERSTATE_STENCILZFAIL
        || rs == D3DRENDERSTATE_STENCILPASS
        || rs == D3DRENDERSTATE_STENCILFUNC
        || rs == D3DRENDERSTATE_STENCILREF
        || rs == D3DRENDERSTATE_STENCILMASK
        || rs == D3DRENDERSTATE_STENCILWRITEMASK
        || rs == D3DRENDERSTATE_TEXTUREFACTOR
        || rs == D3DRENDERSTATE_WRAP0
        || rs == D3DRENDERSTATE_WRAP1
        || rs == D3DRENDERSTATE_WRAP2
        || rs == D3DRENDERSTATE_WRAP3
        || rs == D3DRENDERSTATE_WRAP4
        || rs == D3DRENDERSTATE_WRAP5
        || rs == D3DRENDERSTATE_WRAP6
        || rs == D3DRENDERSTATE_WRAP7
        || rs == D3DRENDERSTATE_CLIPPING
        || rs == D3DRENDERSTATE_LIGHTING
        || rs == D3DRENDERSTATE_EXTENTS
        || rs == D3DRENDERSTATE_AMBIENT
        || rs == D3DRENDERSTATE_FOGVERTEXMODE
        || rs == D3DRENDERSTATE_COLORVERTEX
        || rs == D3DRENDERSTATE_LOCALVIEWER
        || rs == D3DRENDERSTATE_NORMALIZENORMALS
        || rs == D3DRENDERSTATE_COLORKEYBLENDENABLE
        || rs == D3DRENDERSTATE_DIFFUSEMATERIALSOURCE
        || rs == D3DRENDERSTATE_SPECULARMATERIALSOURCE
        || rs == D3DRENDERSTATE_AMBIENTMATERIALSOURCE
        || rs == D3DRENDERSTATE_EMISSIVEMATERIALSOURCE
        || rs == D3DRENDERSTATE_VERTEXBLEND
        || rs == D3DRENDERSTATE_CLIPPLANEENABLE;
  }

}
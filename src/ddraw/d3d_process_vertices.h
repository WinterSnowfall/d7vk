#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"
#include "ddraw_caps.h"

#include "d3d_common_viewport.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace dxvk {

  struct ProcessVerticesData {
    bool doLighting;
    bool doClipping;
    bool doExtents;
    bool doNotCopyData;
    bool isLegacy;
    uint16_t vertexCount;
    size_t inStride;
    size_t outStride;
    DWORD inFVF;
    DWORD outFVF;
    uint8_t* inData;
    uint8_t* outData;
    const D3DMATRIX* correction;
    D3DSTATUS* dsStatus;
    std::vector<d3d9::D3DLIGHT9>* lights;
  };

  // D3DCOLOR is DWORD packed ARGB, uint8_t per component
  // D3DCOLORVALUE is struct with float per component normalized to 0.0f - 1.0f
  inline D3DCOLOR ColorVToColor(const D3DCOLORVALUE& c) {
    return D3DCOLOR_ARGB(
      static_cast<int>(c.a * 255.0f),
      static_cast<int>(c.r * 255.0f),
      static_cast<int>(c.g * 255.0f),
      static_cast<int>(c.b * 255.0f)
    );
  }

  inline D3DCOLORVALUE ColorToColorV(D3DCOLOR c) {
    D3DCOLORVALUE result;

    result.a = ((c >> 24) & 0xFF) / 255.0f;
    result.r = ((c >> 16) & 0xFF) / 255.0f;
    result.g = ((c >> 8)  & 0xFF) / 255.0f;
    result.b = ((c >> 0)  & 0xFF) / 255.0f;

    return result;
  }

  inline D3DCOLORVALUE ColorVClamp(D3DCOLORVALUE color, float min, float max) {
    D3DCOLORVALUE result;

    result.r = std::clamp(color.r, min, max);
    result.g = std::clamp(color.g, min, max);
    result.b = std::clamp(color.b, min, max);
    result.a = std::clamp(color.a, min, max);

    return result;
  }

  inline void ColorVMultiplyAdd(D3DCOLORVALUE &out, D3DCOLORVALUE color, float value, bool alpha = false) {
    out.r += color.r * value;
    out.g += color.g * value;
    out.b += color.b * value;
    if (alpha)
      out.a += color.r * value;
  }

  inline Vector4 NormalizeVec3(Vector4 v) {
    Vector4 result = normalize(Vector4(v.x, v.y, v.z, 0.0f));
    result.w = 0.0;

    return result;
  }

  inline void MaterialColorSource(
        d3d9::IDirect3DDevice9* d3d9Device, DWORD* diffuse,
        DWORD* specular, DWORD* ambient, DWORD* emissive) {
    DWORD state;

    d3d9Device->GetRenderState(d3d9::D3DRS_LIGHTING, &state);
    if (state) {
      *diffuse = d3d9::D3DMCS_COLOR1;
      *specular = d3d9::D3DMCS_COLOR2;
      *ambient = *emissive = d3d9::D3DMCS_MATERIAL;
      return;
    }

    d3d9Device->GetRenderState(d3d9::D3DRS_COLORVERTEX, &state);
    if (state) {
      *ambient = *diffuse = *emissive = *specular = d3d9::D3DMCS_MATERIAL;
      return;
    }

    d3d9Device->GetRenderState(d3d9::D3DRS_DIFFUSEMATERIALSOURCE, diffuse);
    d3d9Device->GetRenderState(d3d9::D3DRS_SPECULARMATERIALSOURCE, specular);
    d3d9Device->GetRenderState(d3d9::D3DRS_AMBIENTMATERIALSOURCE, ambient);
    d3d9Device->GetRenderState(d3d9::D3DRS_EMISSIVEMATERIALSOURCE, emissive);
  }

  inline D3DCOLORVALUE ColorFromMaterialSource(
        D3DCOLOR* diffuse, D3DCOLOR* specular, DWORD colorSource, D3DCOLORVALUE materialColor) {
    switch (colorSource) {
      case d3d9::D3DMCS_COLOR1:
        if (diffuse != nullptr)
          return ColorToColorV(*diffuse);
        else
          return D3DCOLORVALUE{1.0, 1.0, 1.0, 1.0};
      case d3d9::D3DMCS_COLOR2:
        if (specular != nullptr)
          return ColorToColorV(*specular);
        break;
      case d3d9::D3DMCS_MATERIAL:
        return materialColor;
    }

    return D3DCOLORVALUE{0.0, 0.0, 0.0, 0.0};
  }

  inline void ApplyLight(
        const d3d9::D3DLIGHT9* light9, bool localViewer, D3DCOLORVALUE& diffuse, D3DCOLORVALUE& specular,
        Vector4 normals, Vector4 hitDirection, Vector4 position, float attenuation, float materialPower, bool isLegacy) {
    const float direction_dot = dot(hitDirection, normals);
    const float direction_clamped_dot = std::clamp(direction_dot, 0.0f, 1.0f);
    ColorVMultiplyAdd(diffuse, light9->Diffuse, direction_clamped_dot * attenuation);

    Vector4 mid = hitDirection;
    if (localViewer) {
      mid.x -= position.x;
      mid.y -= position.y;
      mid.z -= position.z;
    } else {
      mid.z -= 1.0f;
    }

    mid = normalize(mid);
    const float direction_transformed_dot = dot(Vector4(normals.x, normals.y, normals.z, 0.0f),
                                                Vector4(mid.x, mid.y, mid.z, 0.0f));
    if (direction_transformed_dot > 0.0f && (!isLegacy || materialPower > 0.0f) && direction_dot > 0.0f)
      ColorVMultiplyAdd(specular, light9->Specular, powf(direction_transformed_dot, materialPower) * attenuation);
  }

  inline void ApplyFog(
        float *specularAlpha, D3DFOGMODE vertexMode, float density,
        float start, float end, bool range, Vector4 position) {
    if (vertexMode == D3DFOG_NONE)
      return;

    const float coord = range ? sqrt(dot(position, position)) : fabsf(position.z);

    switch (vertexMode) {
      case D3DFOG_EXP:
        *specularAlpha = expf(-1.0f * coord * density);
        break;
      case D3DFOG_EXP2:
        *specularAlpha = expf(-1.0f * coord * density * coord * density);
        break;
      case D3DFOG_LINEAR:
        *specularAlpha = (end - coord) / (end - start);
        break;
      default:
        break;
    }
  }

  inline void ProcessVerticesInput(
        DWORD dwFVF, uint8_t *ptr, Vector4& position, Vector4& normals,
        Vector4& texCoords, D3DCOLOR& diffuse, D3DCOLOR& specular) {
    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    if (uint8_t type = (dwFVF & D3DFVF_POSITION_MASK)) {
      switch (type) {
        case D3DFVF_XYZ:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          break;
        case D3DFVF_XYZRHW:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB1:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB2:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          ptr += 2 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB3:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          ptr += 3 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB4:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          ptr += 4 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB5:
          position.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          position.w = 1.0f;
          ptr += 5 * sizeof(FLOAT);
          break;
      }
    }

    if (dwFVF & D3DFVF_NORMAL) {
      normals.x = *reinterpret_cast<FLOAT*>(ptr);
      ptr += sizeof(FLOAT);
      normals.y = *reinterpret_cast<FLOAT*>(ptr);
      ptr += sizeof(FLOAT);
      normals.z = *reinterpret_cast<FLOAT*>(ptr);
      ptr += sizeof(FLOAT);
      normals.w = 0.0f;
    }

    if (dwFVF & D3DFVF_RESERVED1) {
      ptr += sizeof(DWORD);
    }

    if (dwFVF & D3DFVF_DIFFUSE) {
      diffuse = *reinterpret_cast<D3DCOLOR*>(ptr);
      ptr += sizeof(D3DCOLOR);
    }

    if (dwFVF & D3DFVF_SPECULAR) {
      specular = *reinterpret_cast<D3DCOLOR*>(ptr);
      ptr += sizeof(D3DCOLOR);
    }

    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          texCoords.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          texCoords.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT3:
          texCoords.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT4:
          texCoords.x = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.y = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.z = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          texCoords.w = *reinterpret_cast<FLOAT*>(ptr);
          ptr += sizeof(FLOAT);
          break;
      }
    }
  }

  inline void ProcessVerticesOutput(
        DWORD dwFVF, uint8_t *ptr, Vector4* position, Vector4* normals,
        Vector4* texCoords, D3DCOLOR* diffuse, D3DCOLOR* specular) {
    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    if (uint8_t type = (dwFVF & D3DFVF_POSITION_MASK)) {
      switch (type) {
        case D3DFVF_XYZ:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZRHW:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->w, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB1:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB2:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 2 * sizeof(FLOAT));
          ptr += 2 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB3:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 3 * sizeof(FLOAT));
          ptr += 3 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB4:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 4 * sizeof(FLOAT));
          ptr += 4 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB5:
          memcpy(ptr, &position->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 5 * sizeof(FLOAT));
          ptr += 5 * sizeof(FLOAT);
          break;
      }
    }

    if (dwFVF & D3DFVF_NORMAL) {
      memcpy(ptr, &normals->x, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
      memcpy(ptr, &normals->y, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
      memcpy(ptr, &normals->z, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
    }

    if (dwFVF & D3DFVF_RESERVED1) {
      ptr += sizeof(DWORD);
    }

    if (dwFVF & D3DFVF_DIFFUSE) {
      if (diffuse != nullptr)
        memcpy(ptr, diffuse, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    if (dwFVF & D3DFVF_SPECULAR) {
      if (specular != nullptr)
        memcpy(ptr, specular, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT3:
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT4:
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          if (texCoords != nullptr)
            memcpy(ptr, &texCoords->w, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
      }
    }
  }

  inline void ProcessVerticesSW(
        d3d9::IDirect3DDevice9* d3d9Device, const D3DOptions* options, ProcessVerticesData *pvData) {
    Logger::debug(">>> ProcessVerticesSW");
    if (unlikely(pvData == nullptr)) {
      Logger::err("ProcessVerticesSW: Missing processing data");
      return;
    }

    D3DMATRIX world{}, view{}, projection{};
    d3d9::D3DVIEWPORT9 viewport9;
    HRESULT hr = d3d9Device->GetViewport(&viewport9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get viewport");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get world transform");
      return;
    }
    hr = d3d9Device->GetTransform(d3d9::D3DTS_VIEW, &view);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get view transform");
      return;
    }
    hr = d3d9Device->GetTransform(d3d9::D3DTS_PROJECTION, &projection);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get projection transform");
      return;
    }

    d3d9::D3DMATERIAL9 material{};
    d3d9Device->GetMaterial(&material);

    Matrix4 wv = MatrixD3DTo4(&view) * MatrixD3DTo4(&world);
    Matrix4 wvp = MatrixD3DTo4(&projection) * wv;
    if (pvData->isLegacy && pvData->correction != nullptr)
      wvp = MatrixD3DTo4(pvData->correction) * wvp;

    BOOL isEnabledLighting, isEnabledFogRange, isEnabledNormalizeNormals, isEnabledSpecular, isEnabledLocalViewer;
    D3DCOLOR stateAmbient;
    D3DFOGMODE fogVertexMode;
    float fogStart, fogEnd, fogDensity;

    d3d9Device->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, reinterpret_cast<DWORD*>(&fogVertexMode));
    d3d9Device->GetRenderState(d3d9::D3DRS_FOGSTART, reinterpret_cast<DWORD*>(&fogStart));
    d3d9Device->GetRenderState(d3d9::D3DRS_FOGEND, reinterpret_cast<DWORD*>(&fogEnd));
    d3d9Device->GetRenderState(d3d9::D3DRS_FOGDENSITY, reinterpret_cast<DWORD*>(&fogDensity));
    d3d9Device->GetRenderState(d3d9::D3DRS_RANGEFOGENABLE, reinterpret_cast<DWORD*>(&isEnabledFogRange));
    d3d9Device->GetRenderState(d3d9::D3DRS_LIGHTING, reinterpret_cast<DWORD*>(&isEnabledLighting));
    if (isEnabledLighting) {
      d3d9Device->GetRenderState(d3d9::D3DRS_AMBIENT, reinterpret_cast<DWORD*>(&stateAmbient));
      d3d9Device->GetRenderState(d3d9::D3DRS_SPECULARENABLE, reinterpret_cast<DWORD*>(&isEnabledSpecular));
      d3d9Device->GetRenderState(d3d9::D3DRS_NORMALIZENORMALS, reinterpret_cast<DWORD*>(&isEnabledNormalizeNormals));
      d3d9Device->GetRenderState(d3d9::D3DRS_LOCALVIEWER, reinterpret_cast<DWORD*>(&isEnabledLocalViewer));
    }

    DWORD sourceDiffuse, sourceSpecular, sourceAmbient, sourceEmissive;
    MaterialColorSource(d3d9Device, &sourceDiffuse, &sourceSpecular, &sourceAmbient, &sourceEmissive);
    for (uint16_t t = 0; t < pvData->vertexCount; t++) {
      uint8_t* inPtr = pvData->inData + t * pvData->inStride;
      uint8_t* outPtr = pvData->outData + t * pvData->outStride;

      Vector4 inPosition{};
      Vector4 inNormals{};
      D3DCOLOR inDiffuse = 0, inSpecular = 0;
      Vector4 inTexCoords{};
      const bool hasDiffUse  = pvData->inFVF & D3DFVF_DIFFUSE;
      const bool hasSpecular = pvData->inFVF & D3DFVF_SPECULAR;
      ProcessVerticesInput(pvData->inFVF, inPtr, inPosition, inNormals, inTexCoords, inDiffuse, inSpecular);

      // Transform vertices
      Vector4 h = wvp * inPosition;
      Vector4 outPosition;

      // Hidden & Dangerous (D3D6) relies on division by zero and NAN/INF output
      outPosition.w = 1.0f / h.w;
      outPosition.x = viewport9.X + static_cast<float>(viewport9.Width) * 0.5 * (h.x * outPosition.w + 1.0f);
      outPosition.y = viewport9.Y + static_cast<float>(viewport9.Height) * 0.5 * (1.0f - h.y * outPosition.w);
      outPosition.z = viewport9.MinZ + h.z * outPosition.w * (viewport9.MaxZ - viewport9.MinZ);

      // Hack for Resident Evil quad alignment problem
      if (unlikely(options->vertexOffset != 0.0f)) {
        outPosition.x += options->vertexOffset;
        outPosition.y += options->vertexOffset;
      }

      if (unlikely(pvData->doClipping)) {
        if (pvData->dsStatus != nullptr && (pvData->dsStatus->dwFlags & D3DSETSTATUS_STATUS)) {
          pvData->dsStatus->dwFlags = 0;
          pvData->dsStatus->dwStatus = 0;
          if (pvData->doExtents) {
            pvData->dsStatus->dwFlags |= D3DSETSTATUS_EXTENTS;
            pvData->dsStatus->drExtent.x1 = viewport9.X;
            pvData->dsStatus->drExtent.y1 = viewport9.Y;
            pvData->dsStatus->drExtent.x2 = viewport9.X + viewport9.Width;
            pvData->dsStatus->drExtent.y2 = viewport9.Y + viewport9.Height;
          }
        }
      }

      D3DCOLORVALUE diffuse{0.0f, 0.0f, 0.0f, 0.0f};
      D3DCOLORVALUE specular{0.0f, 0.0f, 0.0f, 0.0f};

      Vector4 VWPosition = wv * inPosition;
      const float positionScale = 1.0f / VWPosition.w;
      VWPosition.x *= positionScale;
      VWPosition.y *= positionScale;
      VWPosition.z *= positionScale;

      if (pvData->doLighting && isEnabledLighting && pvData->lights != nullptr
       && pvData->outFVF & (D3DFVF_DIFFUSE | D3DFVF_SPECULAR)) {
        const float materialPower = isEnabledSpecular ? material.Power : 0.0f;
        Vector4 NVWPosition = NormalizeVec3(VWPosition);
        Vector4 normals = wv * inNormals;
        if (isEnabledNormalizeNormals)
          normals = NormalizeVec3(normals);

        D3DCOLORVALUE ambient = ColorToColorV(stateAmbient);

        for (d3d9::D3DLIGHT9 light : *pvData->lights) {
          Vector4 hitDirection;
          float attenuation;
          D3DLIGHTTYPE lightType = D3DLIGHTTYPE(light.Type);
          switch (lightType) {
            case D3DLIGHT_DIRECTIONAL: {
              Vector4 lightDirection = NormalizeVec3(MatrixD3DTo4(&view) * Vector4(-light.Direction.x, -light.Direction.y,
                                                                                   -light.Direction.z, 0.0f));
              hitDirection = Vector4(lightDirection.x, lightDirection.y, lightDirection.z, 0.0f);
              attenuation = 1.0f;
              break;
            }
            case D3DLIGHT_POINT:
            case D3DLIGHT_SPOT: {
              Vector4 light_position = MatrixD3DTo4(&view) * Vector4(light.Position.x, light.Position.y,
                                                                     light.Position.z, 1.0f);

              hitDirection = Vector4(light_position.x - VWPosition.x, light_position.y - VWPosition.y,
                                     light_position.z - VWPosition.z, 0.0f);
              Vector4 destination(1.0f, 0.0f, dot(hitDirection, hitDirection), 0.0f);
              destination.y = sqrtf(destination.z);
              if (pvData->isLegacy) {
                destination.y = (light.Range - destination.y) / light.Range;
                if (destination.y <= 0.0f)
                  continue;
                destination.z = destination.y * destination.y;
              } else {
                if (destination.y > 0.0f)
                  continue;
              }

              hitDirection = NormalizeVec3(hitDirection);
              attenuation = destination.x * light.Attenuation0 + destination.y *
                            light.Attenuation1 + destination.z * light.Attenuation2;
              if (!pvData->isLegacy)
                attenuation = 1.0f / attenuation;

              if (lightType == D3DLIGHT_SPOT) {
                Vector4 lightDirection = NormalizeVec3(MatrixD3DTo4(&view) * Vector4(light.Direction.x, light.Direction.y,
                                                                                     light.Direction.z, 0.0f));
                const float rho = dot(-hitDirection, lightDirection);
                const float cosHalfPhi = cosf(light.Phi / 2.0f);
                const float cosHalfTheta = cosf(light.Theta / 2.0f);
                if (rho <= cosHalfPhi)
                  attenuation = 0.0f;
                else if (rho <= cosHalfTheta)
                  attenuation *= powf((rho - cosHalfPhi) / (cosHalfTheta - cosHalfPhi), light.Falloff);
              }
              break;
            }
            // TODO: parallel point lights
            case D3DLIGHT_PARALLELPOINT:
            default: {
              static bool s_warnShown;
              if (!std::exchange(s_warnShown, true))
                Logger::warn(str::format("ProcessVerticesSW: Unsupported light type: ", light.Type));
              continue;
            }
          }

          ColorVMultiplyAdd(ambient, light.Ambient, attenuation);
          ApplyLight(&light, isEnabledLocalViewer, diffuse, specular, normals,
                     hitDirection, NVWPosition, attenuation, materialPower, pvData->isLegacy);
        }

        D3DCOLORVALUE materialDiffuse = ColorFromMaterialSource(hasDiffUse ? &inDiffuse : nullptr,
                                                                hasSpecular ? &inSpecular : nullptr,
                                                                sourceDiffuse, material.Diffuse);
        D3DCOLORVALUE materialSpecular = ColorFromMaterialSource(hasDiffUse ? &inDiffuse : nullptr,
                                                                 hasSpecular ? &inSpecular : nullptr,
                                                                 sourceSpecular, material.Specular);
        D3DCOLORVALUE materialAmbient = ColorFromMaterialSource(hasDiffUse ? &inDiffuse : nullptr,
                                                                hasSpecular ? &inSpecular : nullptr,
                                                                sourceAmbient, material.Ambient);
        D3DCOLORVALUE materialEmissive = ColorFromMaterialSource(hasDiffUse ? &inDiffuse : nullptr,
                                                                 hasSpecular ? &inSpecular : nullptr,
                                                                 sourceEmissive, material.Emissive);

        diffuse.r = ambient.r * materialAmbient.r + diffuse.r * materialDiffuse.r + materialEmissive.r;
        diffuse.g = ambient.g * materialAmbient.g + diffuse.g * materialDiffuse.g + materialEmissive.g;
        diffuse.b = ambient.b * materialAmbient.b + diffuse.b * materialDiffuse.b + materialEmissive.b;
        diffuse.a = material.Diffuse.a;

        specular.r *= materialSpecular.r;
        specular.g *= materialSpecular.g;
        specular.b *= materialSpecular.b;
        specular.a *= pvData->isLegacy ? 0.0f : materialSpecular.a;
      } else {
        diffuse = hasDiffUse ? ColorToColorV(inDiffuse) : D3DCOLORVALUE{1.0, 1.0, 1.0, 1.0};
        specular = hasSpecular ? ColorToColorV(inSpecular) : D3DCOLORVALUE{0.0, 0.0, 0.0, 0.0};
      }

      ApplyFog(&specular.a, fogVertexMode, fogDensity, fogStart, fogEnd, isEnabledFogRange, VWPosition);
      diffuse = ColorVClamp(diffuse, 0.0f, 1.0f);
      specular = ColorVClamp(specular, 0.0f, 1.0f);

      D3DCOLOR outDiffuse = ColorVToColor(diffuse);
      D3DCOLOR outSpecular = ColorVToColor(specular);

      ProcessVerticesOutput(pvData->outFVF, outPtr, &outPosition, &inNormals,
         pvData->doNotCopyData ? nullptr : &inTexCoords,
         pvData->doNotCopyData ? nullptr : &outDiffuse,
         pvData->doNotCopyData ? nullptr : &outSpecular);
    }
  }

}

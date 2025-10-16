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

  using TexCoordArray = std::array<std::array<FLOAT, 4>, ddrawCaps::MaxSimultaneousTextures>;

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

  inline D3DCOLORVALUE ColorToColorV(const D3DCOLOR& c) {
    D3DCOLORVALUE result;

    result.a = ((c >> 24) & 0xFF) / 255.0f;
    result.r = ((c >> 16) & 0xFF) / 255.0f;
    result.g = ((c >> 8)  & 0xFF) / 255.0f;
    result.b = ((c >> 0)  & 0xFF) / 255.0f;

    return result;
  }

  inline D3DCOLORVALUE ColorVClamp(const D3DCOLORVALUE& color, float min, float max) {
    D3DCOLORVALUE result;

    result.r = std::clamp(color.r, min, max);
    result.g = std::clamp(color.g, min, max);
    result.b = std::clamp(color.b, min, max);
    result.a = std::clamp(color.a, min, max);

    return result;
  }

  inline void ColorVMultiplyAdd(D3DCOLORVALUE& out, const D3DCOLORVALUE& color, float value, bool alpha = false) {
    out.r += color.r * value;
    out.g += color.g * value;
    out.b += color.b * value;
    if (unlikely(alpha))
      out.a += color.a * value;
  }

  inline Vector4 NormalizeVec3(const Vector4& v) {
    Vector4 result = normalize(Vector4(v.x, v.y, v.z, 0.0f));
    result.w = 0.0f;

    return result;
  }

  inline void ApplyLight(
        const d3d9::D3DLIGHT9* light9, bool localViewer, D3DCOLORVALUE& diffuse, D3DCOLORVALUE& specular,
        const Vector4& normals, const Vector4& hitDirection, const Vector4& position,
        float attenuation, float materialPower, bool isLegacy) {
    const float direction_dot = std::clamp(dot(hitDirection, normals), 0.0f, 1.0f);
    ColorVMultiplyAdd(diffuse, light9->Diffuse, direction_dot * attenuation);

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
        float* specularAlpha, D3DFOGMODE vertexMode, float density,
        float start, float end, bool useRange, const Vector4& position) {
    const float coord = useRange ? sqrtf(dot(position, position)) : fabsf(position.z);

    switch (vertexMode) {
      // (end - coord) / (end - start)
      case D3DFOG_LINEAR: {
        const float fogFactor = end - coord;
        *specularAlpha = fogFactor / (end - start);
        break;
      }
      // 1 / (e^[coord * density])
      case D3DFOG_EXP: {
        const float fogFactor = coord * density;
        *specularAlpha = expf(-1.0f * fogFactor);
        break;
      }
      // 1 / (e^[coord * density])^2
      case D3DFOG_EXP2: {
        const float fogFactor = coord * density;
        *specularAlpha = expf(-1.0f * fogFactor * fogFactor);
        break;
      }
      default:
        break;
    }
  }

  inline void ProcessVerticesInput(
        bool doNotCopyData, DWORD dwFVF, uint8_t *ptr, Vector4& position, Vector4& normals,
        TexCoordArray& texCoords, D3DCOLOR& diffuse, D3DCOLOR& specular) {
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

    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          if (likely(!doNotCopyData))
            memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          if (likely(!doNotCopyData))
            memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 2);
          ptr += sizeof(FLOAT) * 2;
          break;
        case D3DFVF_TEXTUREFORMAT3:
          if (likely(!doNotCopyData))
            memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_TEXTUREFORMAT4:
          if (likely(!doNotCopyData))
            memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
      }
    }
  }

  inline void ProcessVerticesOutput(
        bool doNotCopyData, DWORD dwFVF, uint8_t* ptr, const Vector4& position, const Vector4& normals,
        const TexCoordArray& texCoords, const D3DCOLOR& diffuse, const D3DCOLOR& specular) {
    if (uint8_t type = (dwFVF & D3DFVF_POSITION_MASK)) {
      switch (type) {
        case D3DFVF_XYZ:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZRHW:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.w, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB1:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_XYZB2:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 2 * sizeof(FLOAT));
          ptr += 2 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB3:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 3 * sizeof(FLOAT));
          ptr += 3 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB4:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 4 * sizeof(FLOAT));
          ptr += 4 * sizeof(FLOAT);
          break;
        case D3DFVF_XYZB5:
          memcpy(ptr, &position.x, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.y, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memcpy(ptr, &position.z, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          memset(ptr, 0x00, 5 * sizeof(FLOAT));
          ptr += 5 * sizeof(FLOAT);
          break;
      }
    }

    if (dwFVF & D3DFVF_NORMAL) {
      memcpy(ptr, &normals.x, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
      memcpy(ptr, &normals.y, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
      memcpy(ptr, &normals.z, sizeof(FLOAT));
      ptr += sizeof(FLOAT);
    }

    if (dwFVF & D3DFVF_RESERVED1) {
      ptr += sizeof(DWORD);
    }

    if (dwFVF & D3DFVF_DIFFUSE) {
      if (likely(!doNotCopyData))
        memcpy(ptr, &diffuse, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    if (dwFVF & D3DFVF_SPECULAR) {
      if (likely(!doNotCopyData))
        memcpy(ptr, &specular, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          if (likely(!doNotCopyData))
            memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          if (likely(!doNotCopyData))
            memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 2);
          ptr += sizeof(FLOAT) * 2;
          break;
        case D3DFVF_TEXTUREFORMAT3:
          if (likely(!doNotCopyData))
            memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_TEXTUREFORMAT4:
          if (likely(!doNotCopyData))
            memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
      }
    }
  }

  inline void ProcessVerticesSW(
        d3d9::IDirect3DDevice9* d3d9Device, const D3DOptions* options, ProcessVerticesData* pvData) {
    Logger::debug(">>> ProcessVerticesSW");
    if (unlikely(pvData == nullptr)) {
      Logger::err("ProcessVerticesSW: Missing processing data");
      return;
    }

    d3d9::D3DVIEWPORT9 viewport9;
    D3DMATRIX world9, view9, projection9;

    HRESULT hr = d3d9Device->GetViewport(&viewport9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 viewport");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 world transform");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_VIEW), &view9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 view transform");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_PROJECTION), &projection9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 projection transform");
      return;
    }

    // Precalculate a few static viewport factors, to save on per-vertex cycles
    const float viewport9HalfWidth  = static_cast<float>(viewport9.Width)  * 0.5f;
    const float viewport9HalfHeight = static_cast<float>(viewport9.Height) * 0.5f;
    const float viewport9ZDelta     = viewport9.MaxZ - viewport9.MinZ;
    const DWORD viewport9Right      = viewport9.X + viewport9.Width;
    const DWORD viewport9Bottom     = viewport9.Y + viewport9.Height;

    const bool needsCorrection = pvData->isLegacy && pvData->correction != nullptr;
    const Matrix4 view = MatrixD3DTo4(&view9);
    const Matrix4 wv   = view * MatrixD3DTo4(&world9);
    const Matrix4 wvp  = !needsCorrection ? MatrixD3DTo4(&projection9) * wv
                                          : MatrixD3DTo4(pvData->correction) * MatrixD3DTo4(&projection9) * wv;

    D3DFOGMODE fogVertexMode;
    float fogStart, fogEnd, fogDensity;
    BOOL isEnabledFogRange, isEnabledLighting, isEnabledSpecular, isEnabledNormalizeNormals, isEnabledLocalViewer;
    D3DCOLOR ambientStateColor;
    // In D3D7 it's quite possible no material is set, even though lighting is enabled
    d3d9::D3DMATERIAL9 material9 = { };

    d3d9Device->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, reinterpret_cast<DWORD*>(&fogVertexMode));
    const bool isEnabledFog = fogVertexMode != D3DFOG_NONE;
    if (isEnabledFog) {
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGSTART, reinterpret_cast<DWORD*>(&fogStart));
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGEND, reinterpret_cast<DWORD*>(&fogEnd));
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGDENSITY, reinterpret_cast<DWORD*>(&fogDensity));
      d3d9Device->GetRenderState(d3d9::D3DRS_RANGEFOGENABLE, reinterpret_cast<DWORD*>(&isEnabledFogRange));
    }
    if (!pvData->isLegacy)
      d3d9Device->GetRenderState(d3d9::D3DRS_LIGHTING, reinterpret_cast<DWORD*>(&isEnabledLighting));
    // We factor in global lighting state into pvData->doLighting for D3D6 and
    // earlier, since D3DRENDERSTATE_/D3DRS_LIGHTING doesn't exist before D3D7
    const bool useLighting = pvData->isLegacy ? pvData->doLighting : pvData->doLighting && isEnabledLighting;
    if (useLighting) {
      d3d9Device->GetRenderState(d3d9::D3DRS_AMBIENT, reinterpret_cast<DWORD*>(&ambientStateColor));
      d3d9Device->GetRenderState(d3d9::D3DRS_SPECULARENABLE, reinterpret_cast<DWORD*>(&isEnabledSpecular));
      d3d9Device->GetRenderState(d3d9::D3DRS_NORMALIZENORMALS, reinterpret_cast<DWORD*>(&isEnabledNormalizeNormals));
      d3d9Device->GetRenderState(d3d9::D3DRS_LOCALVIEWER, reinterpret_cast<DWORD*>(&isEnabledLocalViewer));
      d3d9Device->GetMaterial(&material9);
    }

    static constexpr D3DCOLORVALUE defaultAmbientColorV  = {0.0f, 0.0f, 0.0f, 0.0f};
    static constexpr D3DCOLORVALUE defaultDiffuseColorV  = {1.0f, 1.0f, 1.0f, 1.0f};
    static constexpr D3DCOLORVALUE defaultSpecularColorV = {0.0f, 0.0f, 0.0f, 0.0f};

    const float materialPower = useLighting && isEnabledSpecular ? material9.Power : 0.0f;
    const D3DCOLORVALUE ambientStateColorV = useLighting ? ColorToColorV(ambientStateColor) : defaultAmbientColorV;

    for (uint16_t t = 0; t < pvData->vertexCount; t++) {
      uint8_t* inPtr = pvData->inData + t * pvData->inStride;
      uint8_t* outPtr = pvData->outData + t * pvData->outStride;

      Vector4 inPosition  = { };
      Vector4 inNormals   = { };
      D3DCOLOR inDiffuse  = 0;
      D3DCOLOR inSpecular = 0;
      TexCoordArray inTexCoords = { };

      const bool hasDiffUse  = pvData->inFVF & D3DFVF_DIFFUSE;
      const bool hasSpecular = pvData->inFVF & D3DFVF_SPECULAR;

      ProcessVerticesInput(pvData->doNotCopyData, pvData->inFVF, inPtr, inPosition,
                           inNormals, inTexCoords, inDiffuse, inSpecular);

      // Transform vertices
      Vector4 h = wvp * inPosition;
      Vector4 outPosition;

      // Hidden & Dangerous (D3D6) relies on division by zero and NAN/INF output
      outPosition.w = 1.0f / h.w;
      outPosition.x = viewport9.X + viewport9HalfWidth  * (h.x * outPosition.w + 1.0f);
      outPosition.y = viewport9.Y + viewport9HalfHeight * (1.0f - h.y * outPosition.w);
      outPosition.z = viewport9.MinZ + h.z * outPosition.w * viewport9ZDelta;

      // Hack for Resident Evil quad alignment problem
      if (unlikely(options->vertexOffset != 0.0f)) {
        outPosition.x += options->vertexOffset;
        outPosition.y += options->vertexOffset;
      }

      if (pvData->doClipping) {
        if (pvData->dsStatus != nullptr && (pvData->dsStatus->dwFlags & D3DSETSTATUS_STATUS)) {
          pvData->dsStatus->dwFlags = 0;
          pvData->dsStatus->dwStatus = 0;
          if (pvData->doExtents) {
            pvData->dsStatus->dwFlags |= D3DSETSTATUS_EXTENTS;
            pvData->dsStatus->drExtent.x1 = viewport9.X;
            pvData->dsStatus->drExtent.y1 = viewport9.Y;
            pvData->dsStatus->drExtent.x2 = viewport9Right;
            pvData->dsStatus->drExtent.y2 = viewport9Bottom;
          }
        }
      }

      D3DCOLORVALUE diffuse  = {0.0f, 0.0f, 0.0f, 0.0f};
      D3DCOLORVALUE specular = {0.0f, 0.0f, 0.0f, 0.0f};

      Vector4 VWPosition = wv * inPosition;
      const float positionScale = 1.0f / VWPosition.w;
      VWPosition.x *= positionScale;
      VWPosition.y *= positionScale;
      VWPosition.z *= positionScale;

      if (useLighting && (pvData->outFVF & (D3DFVF_DIFFUSE | D3DFVF_SPECULAR))) {
        const Vector4 NVWPosition = NormalizeVec3(VWPosition);
        const Vector4 normals = !isEnabledNormalizeNormals ? wv * inNormals : NormalizeVec3(wv * inNormals);

        D3DCOLORVALUE ambient = ambientStateColorV;

        for (const d3d9::D3DLIGHT9& light : *pvData->lights) {
          Vector4 hitDirection;
          float attenuation;
          const D3DLIGHTTYPE lightType = D3DLIGHTTYPE(light.Type);
          switch (lightType) {
            case D3DLIGHT_DIRECTIONAL: {
              const Vector4 lightDirection = NormalizeVec3(view * Vector4(-light.Direction.x, -light.Direction.y,
                                                                          -light.Direction.z, 0.0f));

              hitDirection = Vector4(lightDirection.x, lightDirection.y, lightDirection.z, 0.0f);
              attenuation = 1.0f;
              break;
            }
            case D3DLIGHT_POINT:
            case D3DLIGHT_SPOT: {
              const Vector4 lightPosition = view * Vector4(light.Position.x, light.Position.y,
                                                           light.Position.z, 1.0f);

              hitDirection = Vector4(lightPosition.x - VWPosition.x, lightPosition.y - VWPosition.y,
                                     lightPosition.z - VWPosition.z, 0.0f);
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
                const Vector4 lightDirection = NormalizeVec3(view * Vector4(light.Direction.x, light.Direction.y,
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
            // D3DLIGHT_PARALLELPOINT ligts are emulated with maximum range
            // and no attentuation regular point lights, so this shouldn't be hit
            default:
              Logger::warn(str::format("ProcessVerticesSW: Invalid light type: ", lightType));
              continue;
          }

          ColorVMultiplyAdd(ambient, light.Ambient, attenuation);
          ApplyLight(&light, isEnabledLocalViewer, diffuse, specular, normals,
                     hitDirection, NVWPosition, attenuation, materialPower, pvData->isLegacy);
        }

        const D3DCOLORVALUE materialDiffuse  = hasDiffUse  ? ColorToColorV(inDiffuse)  : defaultDiffuseColorV;
        const D3DCOLORVALUE materialSpecular = hasSpecular ? ColorToColorV(inSpecular) : defaultSpecularColorV;

        diffuse.r = ambient.r * material9.Ambient.r + diffuse.r * materialDiffuse.r + material9.Emissive.r;
        diffuse.g = ambient.g * material9.Ambient.g + diffuse.g * materialDiffuse.g + material9.Emissive.g;
        diffuse.b = ambient.b * material9.Ambient.b + diffuse.b * materialDiffuse.b + material9.Emissive.b;
        diffuse.a = material9.Diffuse.a;

        specular.r *= materialSpecular.r;
        specular.g *= materialSpecular.g;
        specular.b *= materialSpecular.b;
        specular.a *= pvData->isLegacy ? 0.0f : materialSpecular.a;
      } else {
        diffuse  = hasDiffUse  ? ColorToColorV(inDiffuse)  : defaultDiffuseColorV;
        specular = hasSpecular ? ColorToColorV(inSpecular) : defaultSpecularColorV;
      }

      if (isEnabledFog)
        ApplyFog(&specular.a, fogVertexMode, fogDensity, fogStart, fogEnd, isEnabledFogRange, VWPosition);

      diffuse  = ColorVClamp(diffuse,  0.0f, 1.0f);
      specular = ColorVClamp(specular, 0.0f, 1.0f);

      static constexpr D3DCOLOR defaultColor = D3DCOLOR_ARGB(0, 0, 0, 0);
      const D3DCOLOR outDiffuse  = !pvData->doNotCopyData ? ColorVToColor(diffuse)  : defaultColor;
      const D3DCOLOR outSpecular = !pvData->doNotCopyData ? ColorVToColor(specular) : defaultColor;

      ProcessVerticesOutput(pvData->doNotCopyData, pvData->outFVF, outPtr, outPosition,
                            inNormals, inTexCoords, outDiffuse, outSpecular);
    }
  }

}

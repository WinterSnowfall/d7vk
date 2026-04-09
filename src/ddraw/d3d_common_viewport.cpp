#include "d3d_common_viewport.h"

#include "d3d_common_interface.h"

#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DCommonViewport::D3DCommonViewport(D3DCommonInterface* commonD3DIntf)
  : m_commonD3DIntf ( commonD3DIntf ) {
  }

  D3DCommonViewport::~D3DCommonViewport() {
  }

  D3D6Viewport* D3DCommonViewport::GetCurrentD3D6Viewport() {
    if (m_device6 != nullptr)
      return m_device6->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D5Viewport* D3DCommonViewport::GetCurrentD3D5Viewport() {
    if (m_device5 != nullptr)
      return m_device5->GetCurrentViewportInternal();

    return nullptr;
  }

  D3D3Viewport* D3DCommonViewport::GetCurrentD3D3Viewport() {
    if (m_device3 != nullptr)
      return m_device3->GetCurrentViewportInternal();

    return nullptr;
  }

  void D3DCommonViewport::EnableLegacyLights(bool isD3DLight2) {
    if (m_device6 != nullptr) {
      return m_device6->EnableLegacyLights(isD3DLight2);
    } else if (m_device5 != nullptr) {
      return m_device5->EnableLegacyLights(isD3DLight2);
    } else if (m_device3 != nullptr) {
      return m_device3->EnableLegacyLights(isD3DLight2);
    }
  }

  d3d9::IDirect3DDevice9* D3DCommonViewport::GetD3D9Device() {
    if (m_device6 != nullptr) {
      return m_device6->GetD3D9();
    } else if (m_device5 != nullptr) {
      return m_device5->GetD3D9();
    } else if (m_device3 != nullptr) {
      return m_device3->GetD3D9();
    }

    return nullptr;
  }

  HRESULT D3DCommonViewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    if (data == nullptr || data->dwSize != sizeof(D3DTRANSFORMDATA))
      return DDERR_INVALIDPARAMS;

    if (data->lpIn == nullptr || data->lpOut == nullptr || data->dwInSize == 0|| data->dwOutSize == 0)
      return DDERR_INVALIDPARAMS;

    if ((flags & (D3DTRANSFORM_CLIPPED | D3DTRANSFORM_UNCLIPPED)) == 0)
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9 *m_device9 = GetD3D9Device();
    if (m_device9 == nullptr) {
      Logger::warn("D3DCommonViewport::TransformVertices: No D3D9 device available!");
      return DDERR_GENERIC;
    }

    // Ensure transform states aren't modified in flight
    if (m_device6 != nullptr)
      D3DDeviceLock lock6 = m_device6->LockDevice();
    if (m_device5 != nullptr)
      D3DDeviceLock lock5 = m_device5->LockDevice();
    if (m_device3 != nullptr)
      D3DDeviceLock lock3 = m_device3->LockDevice();

    bool clipped = (flags & D3DTRANSFORM_CLIPPED) && !(flags & D3DTRANSFORM_UNCLIPPED);

    D3DMATRIX world{}, view{}, projection{};
    HRESULT hr;
    hr = m_device9->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get world transform");
      return DDERR_GENERIC;
    }
    hr = m_device9->GetTransform(d3d9::D3DTS_VIEW, &view);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get view transform");
      return DDERR_GENERIC;
    }
    hr = m_device9->GetTransform(d3d9::D3DTS_PROJECTION, &projection);
    if (FAILED(hr)) {
      Logger::err("D3DCommonViewport::TransformVertices: failed to get projection transform");
      return DDERR_GENERIC;
    }

    Matrix4 wv = MatrixD3DTo4(&view) * MatrixD3DTo4(&world);
    Matrix4 wvp = MatrixD3DTo4(&projection) * wv;

    if (clipped)
      *offscreen = UINT_MAX;
    else
      *offscreen = 0;

    for (DWORD t = 0; t < vertex_count; t++) {
      // Docs says input is always D3DLVERTEX and output D3DTLVERTEX.
      // But they can have arbitrary stride set by application and defined via dwInSize/dwOutSize.
      D3DLVERTEX& in = *(reinterpret_cast<D3DLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpIn) + data->dwInSize * t));
      D3DTLVERTEX& out = *(reinterpret_cast<D3DTLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpIn) + data->dwOutSize * t));

      // Logger::debug(str::format("D3DCommonViewport::TransformVertices: INPUT: ", t, " in: ", in.x, ", ", in.y, ", ", in.z));

      Vector4 h = wvp * Vector4({in.x, in.y, in.z, 1.0f});

      auto outH = data->lpHOut;
      if (clipped) {
        outH[t].dwFlags = 0;
        if (h.x > h.w)
          outH[t].dwFlags |= D3DCLIP_RIGHT;
        if (h.x < -h.w)
          outH[t].dwFlags |= D3DCLIP_LEFT;
        if (h.y > h.w)
          outH[t].dwFlags |= D3DCLIP_TOP;
        if (h.y < -h.w)
          outH[t].dwFlags |= D3DCLIP_BOTTOM;
        if (h.z < 0.0f)
          outH[t].dwFlags |= D3DCLIP_FRONT;
        if (h.z > h.w)
          outH[t].dwFlags |= D3DCLIP_BACK;

        *offscreen &= outH[t].dwFlags;

        outH[t].hx = (h.x - m_legacyClip.x * h.w) / m_legacyScale.x;
        outH[t].hy = (h.y - m_legacyClip.y * h.w) / m_legacyScale.y;
        outH[t].hz = (h.z - m_legacyClip.z * h.w) / m_legacyScale.z;

        if (outH[t].dwFlags) {
          out.sx = h.x;
          out.sy = h.y;
          out.sz = h.z;
          out.rhw = h.w;
          continue;
        }
      }

      out.rhw = (h.w != 0.0f) ? (1.0f / h.w) : 0.0f;
      out.sx = (m_viewport9.X + static_cast<float>(m_viewport9.Width) * 0.5) + (h.x * out.rhw);
      out.sy = (m_viewport9.Y + static_cast<float>(m_viewport9.Height) * 0.5) - (h.y * out.rhw);
      out.sz = m_viewport9.MinZ + (h.z * out.rhw) * (m_viewport9.MaxZ - m_viewport9.MinZ);

      // Logger::debug(str::format("D3DCommonViewport::TransformVertices: OUTPUT: out: ", out.sx, ", ", out.sy, ", ", out.sz, ", rhw: ", out.rhw));
    }

    return D3D_OK;
  }

}
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

  d3d9::IDirect3DDevice9* D3DCommonViewport::GetD3D9Device() {
    if (m_device6 != nullptr) {
      return m_device6->GetCommonD3DDevice()->GetD3D9Device();
    } else if (m_device5 != nullptr) {
      return m_device5->GetCommonD3DDevice()->GetD3D9Device();
    } else if (m_device3 != nullptr) {
      return m_device3->GetCommonD3DDevice()->GetD3D9Device();
    }

    return nullptr;
  }

  void D3DCommonViewport::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if (m_device6 != nullptr) {
      m_device6->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device5 != nullptr) {
      m_device5->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    } else if (m_device3 != nullptr) {
      m_device3->UpdateSurfaceDirtyTracking(dirtyRenderTarget, dirtyDepthStencil, dirtyPrimarySurface);
    }
  }

  HRESULT D3DCommonViewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    if (data == nullptr || data->dwSize != sizeof(D3DTRANSFORMDATA) || offscreen == nullptr)
      return DDERR_INVALIDPARAMS;

    if ((flags & (D3DTRANSFORM_CLIPPED | D3DTRANSFORM_UNCLIPPED)) == 0)
      return DDERR_INVALIDPARAMS;

    bool clipped = (flags & D3DTRANSFORM_CLIPPED) && !(flags & D3DTRANSFORM_UNCLIPPED);
    if (clipped)
      *offscreen = UINT_MAX;
    else
      *offscreen = 0;

    // When vertex_count = 0 native apparently returns success even when data->lpIn/data->lpOut are null, otherwise crash
    if (vertex_count == 0)
      return D3D_OK;

    if (data->dwInSize < sizeof(D3DLVERTEX) || data->dwOutSize < sizeof(D3DTLVERTEX))
      return DDERR_INVALIDPARAMS;

    if (data->lpIn == nullptr || data->lpOut == nullptr)
      return DDERR_INVALIDPARAMS;

    // Ensure transform states aren't modified in flight
    if (m_device6 != nullptr)
      D3DDeviceLock lock6 = m_device6->LockDevice();
    if (m_device5 != nullptr)
      D3DDeviceLock lock5 = m_device5->LockDevice();
    if (m_device3 != nullptr)
      D3DDeviceLock lock3 = m_device3->LockDevice();

    d3d9::IDirect3DDevice9* m_device9 = GetD3D9Device();

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
    const D3DMATRIX* correction = GetLegacyProjectionMatrix(0);
    if (correction != nullptr)
      wvp = MatrixD3DTo4(correction) * wvp;

    for (DWORD t = 0; t < vertex_count; t++) {
      // Docs says input is always D3DLVERTEX and output D3DTLVERTEX.
      // But they can have arbitrary stride set by application and defined via dwInSize/dwOutSize.
      D3DLVERTEX& in = *(reinterpret_cast<D3DLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpIn) + data->dwInSize * t));
      D3DTLVERTEX& out = *(reinterpret_cast<D3DTLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpOut) + data->dwOutSize * t));

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

      // Hidden & Dangerous (D3D6) relies on NAN/INF output
      // in ProcessVertices, so do the same here just in case
      out.rhw = 1.0f / h.w;
      out.sx = m_viewport9.X + static_cast<float>(m_viewport9.Width) * 0.5 * (h.x * out.rhw + 1.0f);
      out.sy = m_viewport9.Y + static_cast<float>(m_viewport9.Height) * 0.5 * (1.0f - h.y * out.rhw);
      out.sz = m_viewport9.MinZ + h.z * out.rhw * (m_viewport9.MaxZ - m_viewport9.MinZ);

      out.color = in.color;
      out.specular = in.specular;
      out.tu = in.tu;
      out.tv = in.tv;
    }

    return D3D_OK;
  }

}
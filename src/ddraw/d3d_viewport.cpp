#include "d3d_viewport.h"

#include "../util/sync/sync_scoped.h"

#include "ddraw_common_interface.h"
#include "ddraw_common_surface.h"

#include "d3d_common_material.h"

#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

#include "ddraw/ddraw_surface.h"
#include "ddraw4/ddraw4_surface.h"

#include <vector>
#include <algorithm>

namespace dxvk {

  using D3DDeviceLock = sync::ScopedDeviceGuard;

  D3DViewport::D3DViewport(IUnknown* pParent)
    : DDrawChildObject<IUnknown, IDirect3DViewport3>(pParent) {
  }

  D3DViewport::~D3DViewport() {
    // Dissasociate every bound light from this viewport
    for (auto& light : m_lights) {
      light->SetViewport(nullptr);
    }
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Because viewport interfaces are clean extensions of one another,
    // we can implement all of them within a single backing object,
    // which actually mirrors the native implementation and works
    // around some game patches/mods hooking the viewport vtable
    if (likely(riid == __uuidof(IUnknown)
            || riid == __uuidof(IDirect3DViewport)
            || riid == __uuidof(IDirect3DViewport2)
            || riid == __uuidof(IDirect3DViewport3))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3DViewport::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "The IDirect3DViewport3::Initialize method is not implemented."
  HRESULT STDMETHODCALLTYPE D3DViewport::Initialize(IDirect3D* d3d) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::GetViewport(D3DVIEWPORT* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_isViewportSet))
      return D3DERR_VIEWPORTDATANOTSET;

    data->dwX      = m_viewport9.X;
    data->dwY      = m_viewport9.Y;
    data->dwWidth  = m_viewport9.Width;
    data->dwHeight = m_viewport9.Height;
    data->dvMinZ   = m_viewport9.MinZ;
    data->dvMaxZ   = m_viewport9.MaxZ;
    data->dvScaleX = m_legacyScale.x * (float)data->dwWidth / 2.0f;
    data->dvScaleY = m_legacyScale.y * (float)data->dwHeight / 2.0f;
    // Don't compact these because precision issues can affect the outcome
    data->dvMaxX   = 2.0f / m_legacyScale.x * (1.0f + (m_legacyClip.x + 1.0f) / -2.0f); // dvClipX + dvClipWidth
    data->dvMaxY   = 2.0f / m_legacyScale.y * (m_legacyClip.y - 1.0f) / -2.0f;          // dvClipY

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::SetViewport(D3DVIEWPORT* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_commonD3DDevice == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    // These validations aren't performed at all on viewports bound to a D3D3 device
    if (m_commonD3DDevice->GetD3D3Device() == nullptr) {
      // Use the full surface rect, since it is surface version agnostic
      const RECT* surfRect = m_commonD3DDevice->GetCommonRenderTarget()->GetFullSurfaceRect();
      // D3D6/5 will fail when setting a viewport that's outside
      // of the current render target, though that works in D3D9
      HRESULT hr = ValidateViewportRT(data->dwX, data->dwY, data->dwWidth, data->dwHeight,
                                      surfRect->right, surfRect->bottom);
      if (unlikely(FAILED(hr)))
        return hr;
    }

    // The docs state: "The method ignores the values in the dvMaxX,
    // dvMaxY, dvMinZ, and dvMaxZ members.", which appears correct
    m_viewport9.X      = data->dwX;
    m_viewport9.Y      = data->dwY;
    m_viewport9.Width  = data->dwWidth;
    m_viewport9.Height = data->dwHeight;
    m_viewport9.MinZ   = 0.0f;
    m_viewport9.MaxZ   = 1.0f;

    m_legacyScale.x = 2.0f * data->dvScaleX / (float)data->dwWidth;
    m_legacyScale.y = 2.0f * data->dvScaleY / (float)data->dwHeight;
    m_legacyScale.z = 1.0f;
    m_legacyClip.x = 0.0f;
    m_legacyClip.y = 0.0f;
    m_legacyClip.z = 0.0f;

    m_isViewportSet   = true;
    // Also dirty the legacy projection
    m_dirtyProjection = true;

    if (m_isCurrentViewport)
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA* data, DWORD flags, DWORD* offscreen) {
    if (unlikely(m_commonD3DDevice == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    if (unlikely(data == nullptr || offscreen == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DTRANSFORMDATA)))
      return DDERR_INVALIDPARAMS;

    if (unlikely((flags & (D3DTRANSFORM_CLIPPED | D3DTRANSFORM_UNCLIPPED)) == 0))
      return DDERR_INVALIDPARAMS;

    const bool clipped = (flags & D3DTRANSFORM_CLIPPED) && !(flags & D3DTRANSFORM_UNCLIPPED);

    if (clipped)
      *offscreen = std::numeric_limits<uint32_t>::max();
    else
      *offscreen = 0;

    // When vertex_count = 0, native apparently returns success even
    // when data->lpIn/data->lpOut are null, otherwise crash
    if (unlikely(vertex_count == 0))
      return D3D_OK;

    if (unlikely(data->dwInSize < sizeof(D3DLVERTEX) || data->dwOutSize < sizeof(D3DTLVERTEX)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->lpIn == nullptr || data->lpOut == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ensure transform states aren't modified in flight
    D3DDeviceLock lock6, lock5, lock3;
    D3D6Device* d3d6Device = m_commonD3DDevice->GetD3D6Device();
    if (d3d6Device != nullptr)
      lock6 = d3d6Device->LockDevice();
    D3D5Device* d3d5Device = m_commonD3DDevice->GetD3D5Device();
    if (d3d5Device != nullptr)
      lock5 = d3d5Device->LockDevice();
    D3D3Device* d3d3Device = m_commonD3DDevice->GetD3D3Device();
    if (d3d3Device != nullptr)
      lock3 = d3d3Device->LockDevice();

    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    // Temporarily activate this viewport, if not already active
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_isCurrentViewport) {
      d3d9Device->GetViewport(&currentViewport9);
      d3d9Device->SetViewport(&m_viewport9);
    }

    D3DMATRIX world9, view9, projection9;
    d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world9);
    d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_VIEW), &view9);
    d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_PROJECTION), &projection9);

    // Precalculate a few static viewport factors, to save on per-vertex cycles
    const float viewport9HalfWidth  = static_cast<float>(m_viewport9.Width)  * 0.5f;
    const float viewport9HalfHeight = static_cast<float>(m_viewport9.Height) * 0.5f;
    const float viewport9ZDelta     = m_viewport9.MaxZ - m_viewport9.MinZ;

    const D3DMATRIX* correction = GetLegacyProjectionMatrix(0);

    const Matrix4 wv = MatrixD3DTo4(&view9) * MatrixD3DTo4(&world9);
    const Matrix4 wvp = correction == nullptr ? MatrixD3DTo4(&projection9) * wv
                                              : MatrixD3DTo4(correction) * MatrixD3DTo4(&projection9) * wv;

    for (DWORD t = 0; t < vertex_count; t++) {
      // Docs says input is always D3DLVERTEX and output D3DTLVERTEX.
      // But they can have arbitrary stride set by application and defined via dwInSize/dwOutSize.
      D3DLVERTEX& in = *(reinterpret_cast<D3DLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpIn) + data->dwInSize * t));
      D3DTLVERTEX& out = *(reinterpret_cast<D3DTLVERTEX*>(reinterpret_cast<uint8_t*>(data->lpOut) + data->dwOutSize * t));

      const Vector4 h = wvp * Vector4({in.x, in.y, in.z, 1.0f});

      D3DHVERTEX* outH = data->lpHOut;
      if (outH != nullptr && clipped) {
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
      out.sx = m_viewport9.X + viewport9HalfWidth * (h.x * out.rhw + 1.0f);
      out.sy = m_viewport9.Y + viewport9HalfHeight * (1.0f - h.y * out.rhw);
      out.sz = m_viewport9.MinZ + h.z * out.rhw * viewport9ZDelta;

      out.color = in.color;
      out.specular = in.specular;
      out.tu = in.tu;
      out.tv = in.tv;
    }

    // Restore the previously active viewport
    if (!m_isCurrentViewport) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    return D3D_OK;
  }

  // Docs state: "The IDirect3DViewport3::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3DViewport::LightElements(DWORD element_count, D3DLIGHTDATA* data) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::SetBackground(D3DMATERIALHANDLE hMat) {
    if (unlikely(m_materialHandle == hMat))
      return D3D_OK;

    if (likely(hMat)) {
      D3DCommonMaterial* commonMaterial = D3DCommonInterface::GetCommonMaterialFromHandle(hMat);
      if (unlikely(commonMaterial == nullptr))
        return DDERR_INVALIDPARAMS;
    }

    // Cache only the set material handle, as its color can
    // change after it is set (get it on Clear directly)
    m_materialHandle = hMat;
    m_isMaterialSet = hMat ? true : false;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::GetBackground(D3DMATERIALHANDLE* material, BOOL* valid) {
    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    *material = m_materialHandle;
    *valid = m_isMaterialSet;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::SetBackgroundDepth(IDirectDrawSurface* surface) {
    if (unlikely(surface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3DViewport::SetBackgroundDepth: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    const bool isBackgroundClear = surface == nullptr;
    m_backgroundDepth = isBackgroundClear ? nullptr : reinterpret_cast<DDrawSurface*>(surface);
    m_isBackgroundDepthSet = isBackgroundClear ? false : true;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::GetBackgroundDepth(IDirectDrawSurface** surface, BOOL* valid) {
    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *surface = reinterpret_cast<IDirectDrawSurface*>(m_backgroundDepth.ptr());
    *valid = m_isBackgroundDepthSet;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::Clear(DWORD count, D3DRECT* rects, DWORD flags) {
    // Early D3D viewport fast skip
    if (unlikely(!count || !rects))
      return D3D_OK;

    if (unlikely(m_commonD3DDevice == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    const bool clearRenderTarget = flags & D3DCLEAR_TARGET;
    const bool clearDepthStencil = flags & D3DCLEAR_ZBUFFER;
    DDrawCommonSurface* rt = nullptr;
    DDrawCommonSurface* ds = nullptr;

    if (clearRenderTarget) {
      rt = m_commonD3DDevice->GetCommonRenderTarget();
      if (likely(rt != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !rt->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          rt->InitializeOrUploadD3D9();
        }
      }
    }
    if (clearDepthStencil) {
      ds = m_commonD3DDevice->GetCommonDepthStencil();
      if (likely(ds != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !ds->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          ds->InitializeOrUploadD3D9();
        }
      }
    }

    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_isCurrentViewport) {
      d3d9Device->GetViewport(&currentViewport9);
      d3d9Device->SetViewport(&m_viewport9);
    }

    static constexpr D3DCOLOR defaultColor = D3DCOLOR_ARGB(0, 0, 0, 0);
    D3DCommonMaterial* commonMaterial = D3DCommonInterface::GetCommonMaterialFromHandle(m_materialHandle);
    D3DCOLOR clearColor = commonMaterial != nullptr ? commonMaterial->GetMaterialColor() : defaultColor;

    // TODO: Account for any set background depth surface, though in practice
    // it does not appear to matter even in the one game which uses it (Powerslide)
    HRESULT hr = d3d9Device->Clear(count, rects, flags, clearColor, 1.0f, 0u);

    // Restore the previously active viewport
    if (!m_isCurrentViewport) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    // Can fail in D3D9 only in case of a missing depth stencil surface
    if (unlikely(FAILED(hr))) {
      // Fix up expected return codes
      return D3DERR_ZBUFFER_NOTPRESENT;
    }

    if (clearRenderTarget && rt != nullptr)
      rt->UnDirtyDDrawSurface();
    if (clearDepthStencil && ds != nullptr)
      ds->UnDirtyDDrawSurface();

    m_commonD3DDevice->UpdateSurfaceDirtyTracking(clearRenderTarget, clearDepthStencil, false);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::AddLight(IDirect3DLight* light) {
    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(d3dLight->HasViewport()))
      return D3DERR_LIGHTHASVIEWPORT;

    // No need to check if the light is already attached, since
    // if that's the case it will have a set viewport above
    d3dLight->SetViewport(this);
    m_lights.push_back(d3dLight);

    if (m_commonD3DDevice != nullptr && m_isCurrentViewport)
      ApplyAndActivateLight(d3dLight);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::DeleteLight(IDirect3DLight* light) {
    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(!d3dLight->HasViewport()))
      return DDERR_INVALIDPARAMS;

    auto it = std::find(m_lights.begin(), m_lights.end(), d3dLight);
    if (likely(it != m_lights.end())) {
      // Ensure the light is deactivated before deleting it
      if (m_commonD3DDevice != nullptr && m_isCurrentViewport && d3dLight->IsActive())
        DeactivateLight(d3dLight);
      d3dLight->SetViewport(nullptr);
      m_lights.erase(it);
    } else {
      Logger::warn("D3DViewport::DeleteLight: Light not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::NextLight(IDirect3DLight* lpDirect3DLight, IDirect3DLight** lplpDirect3DLight, DWORD flags) {
    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    if (flags & D3DNEXT_HEAD) {
      if (likely(m_lights.size() > 0))
        *lplpDirect3DLight = m_lights.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DLight == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(m_lights.size() > 0))
        Logger::warn("D3DViewport::NextLight: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_lights.size() > 0))
        *lplpDirect3DLight = m_lights.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::GetViewport2(D3DVIEWPORT2* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_isViewportSet))
      return D3DERR_VIEWPORTDATANOTSET;

    data->dwX          = m_viewport9.X;
    data->dwY          = m_viewport9.Y;
    data->dwWidth      = m_viewport9.Width;
    data->dwHeight     = m_viewport9.Height;
    data->dvMinZ       = m_viewport9.MinZ;
    data->dvMaxZ       = m_viewport9.MaxZ;

    data->dvClipWidth  = 2.0f / m_legacyScale.x;
    data->dvClipHeight = 2.0f / m_legacyScale.y;
    data->dvClipX      = data->dvClipWidth  * (m_legacyClip.x + 1.0f) / -2.0f;
    data->dvClipY      = data->dvClipHeight * (m_legacyClip.y - 1.0f) / -2.0f;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::SetViewport2(D3DVIEWPORT2* data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_commonD3DDevice == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    // These validations aren't performed at all on viewports bound to a D3D3 device
    if (likely(m_commonD3DDevice->GetD3D3Device() == nullptr)) {
      // Use the full surface rect, since it is surface version agnostic
      const RECT* surfRect = m_commonD3DDevice->GetCommonRenderTarget()->GetFullSurfaceRect();
      // D3D6/5 will fail when setting a viewport that's outside
      // of the current render target, though that works in D3D9
      HRESULT hr = ValidateViewportRT(data->dwX, data->dwY, data->dwWidth, data->dwHeight,
                                      surfRect->right, surfRect->bottom);
      if (unlikely(FAILED(hr)))
        return hr;
    }

    m_viewport9.X      = data->dwX;
    m_viewport9.Y      = data->dwY;
    m_viewport9.Width  = data->dwWidth;
    m_viewport9.Height = data->dwHeight;
    m_viewport9.MinZ   = 0.0f;
    m_viewport9.MaxZ   = 1.0f;

    m_legacyScale.x = 2.0f / data->dvClipWidth;
    m_legacyScale.y = 2.0f / data->dvClipHeight;
    m_legacyScale.z = 1.0f / (data->dvMaxZ - data->dvMinZ);
    m_legacyClip.x = -2.0f * data->dvClipX / data->dvClipWidth - 1.0f;
    m_legacyClip.y = -2.0f * data->dvClipY / data->dvClipHeight + 1.0f;
    m_legacyClip.z = -data->dvMinZ / (data->dvMaxZ - data->dvMinZ);

    m_isViewportSet   = true;
    // Also dirty the legacy projection
    m_dirtyProjection = true;

    if (m_isCurrentViewport)
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::SetBackgroundDepth2(IDirectDrawSurface4* surface) {
    if (unlikely(surface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3DViewport::SetBackgroundDepth2: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    const bool isBackgroundClear = surface == nullptr;
    m_backgroundDepth4 = isBackgroundClear ? nullptr : reinterpret_cast<DDraw4Surface*>(surface);
    m_isBackgroundDepth4Set = isBackgroundClear ? false : true;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::GetBackgroundDepth2(IDirectDrawSurface4** surface, BOOL* valid) {
    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *surface = reinterpret_cast<IDirectDrawSurface4*>(m_backgroundDepth4.ptr());
    *valid = m_isBackgroundDepth4Set;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3DViewport::Clear2(DWORD count, D3DRECT* rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil) {
    // Early D3D viewport fast skip
    if (unlikely(!count || !rects))
      return D3D_OK;

    if (unlikely(m_commonD3DDevice == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    const bool clearRenderTarget = flags & D3DCLEAR_TARGET;
    const bool clearDepthStencil = (flags & D3DCLEAR_ZBUFFER) || (flags & D3DCLEAR_STENCIL);
    DDrawCommonSurface* rt = nullptr;
    DDrawCommonSurface* ds = nullptr;

    if (clearRenderTarget) {
      rt = m_commonD3DDevice->GetCommonRenderTarget();
      if (likely(rt != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !rt->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          rt->InitializeOrUploadD3D9();
        }
      }
    }
    if (clearDepthStencil) {
      ds = m_commonD3DDevice->GetCommonDepthStencil();
      if (likely(ds != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !ds->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          ds->InitializeOrUploadD3D9();
        }
      }
    }

    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_isCurrentViewport) {
      d3d9Device->GetViewport(&currentViewport9);
      d3d9Device->SetViewport(&m_viewport9);
    }

    HRESULT hr = d3d9Device->Clear(count, rects, flags, color, z, stencil);

    // Restore the previously active viewport
    if (!m_isCurrentViewport) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    // Can fail in D3D9 only in case of a missing depth stencil surface
    if (unlikely(FAILED(hr))) {
      // Fix up expected return codes
      if (flags & D3DCLEAR_ZBUFFER) {
        return D3DERR_ZBUFFER_NOTPRESENT;
      } else {
        return D3DERR_STENCILBUFFER_NOTPRESENT;
      }
    }

    if (clearRenderTarget && rt != nullptr)
      rt->UnDirtyDDrawSurface();
    if (clearDepthStencil && ds != nullptr)
      ds->UnDirtyDDrawSurface();

    m_commonD3DDevice->UpdateSurfaceDirtyTracking(clearRenderTarget, clearDepthStencil, false);

    return D3D_OK;
  }

  void D3DViewport::ApplyViewport() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    HRESULT hr = d3d9Device->SetViewport(&m_viewport9);
    if (unlikely(FAILED(hr)))
      Logger::err("D3DViewport: Failed to set the D3D9 viewport");
  }

  void D3DViewport::DeactivateLights() {
    for (auto& light : m_lights) {
      if (light->IsActive())
        DeactivateLight(light.ptr());
    }
  }

  void D3DViewport::DeactivateLight(D3DLight* light) {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    const DWORD light9Index = light->GetIndex();

    HRESULT hr = d3d9Device->LightEnable(light9Index, FALSE);
    if (unlikely(FAILED(hr)))
      Logger::err("D3DViewport::DeactivateLight: Failed D3D9 LightEnable call");
  }

  void D3DViewport::ApplyAndActivateLights() {
    for (auto& light : m_lights)
      ApplyAndActivateLight(light.ptr());
  }

  void D3DViewport::ApplyAndActivateLight(D3DLight* light) {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    const DWORD light9Index = light->GetIndex();
    HRESULT hr = d3d9Device->SetLight(light9Index, light->GetD3D9Light());
    if (unlikely(FAILED(hr)))
      Logger::err("D3DViewport::ApplyAndActivateLight: Failed D3D9 SetLight call");

    if (light->IsActive()) {
      hr = d3d9Device->LightEnable(light9Index, TRUE);
      if (unlikely(FAILED(hr)))
        Logger::err("D3DViewport::ApplyAndActivateLight: Failed D3D9 LightEnable call (TRUE)");
    } else {
      hr = d3d9Device->LightEnable(light9Index, FALSE);
      if (unlikely(FAILED(hr)))
        Logger::err("D3DViewport::ApplyAndActivateLight: Failed D3D9 LightEnable call (FALSE)");
    }
  }

}

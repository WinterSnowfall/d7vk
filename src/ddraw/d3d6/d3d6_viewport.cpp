#include "d3d6_viewport.h"

#include "../ddraw4/ddraw4_surface.h"

#include "d3d6_device.h"
#include "d3d6_material.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D6Viewport::s_viewportCount = 0;

  D3D6Viewport::D3D6Viewport(Com<IDirect3DViewport3>&& proxyViewport, D3D6Interface* pParent)
    : DDrawWrappedObject<D3D6Interface, IDirect3DViewport3, IUnknown>(pParent, std::move(proxyViewport), nullptr) {
    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D6Viewport: Created a new viewport nr. [[3-", m_viewportCount, "]]"));
  }

  D3D6Viewport::~D3D6Viewport() {
    Logger::debug(str::format("D3D6Viewport: Viewport nr. [[3-", m_viewportCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D6Interface, IDirect3DViewport3, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DViewport3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Viewport::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D6Viewport::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  // Docs state: "The IDirect3DViewport3::Initialize method is not implemented."
  HRESULT STDMETHODCALLTYPE D3D6Viewport::Initialize(IDirect3D *d3d) {
    Logger::debug(">>> D3D6Viewport::Initialize");
    return DDERR_UNSUPPORTED;
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetViewport(D3DVIEWPORT *data) {
    Logger::warn("<<< D3D6Viewport::GetViewport: Proxy");
    return m_proxy->GetViewport(data);
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::warn("<<< D3D6Viewport::SetViewport: Proxy");
    return m_proxy->SetViewport(data);
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    Logger::debug("<<< D3D6Viewport::TransformVertices: Proxy");
    return m_proxy->TransformVertices(vertex_count, data, flags, offscreen);
  }

  // Docs state: "The IDirect3DViewport3::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D6Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    Logger::warn(">>> D3D6Viewport::LightElements");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    Logger::debug(">>> D3D6Viewport::SetBackground");

    m_material = hMat;
    // TODO: Look up the material and make it valid

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug(">>> D3D6Viewport::GetBackground");

    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    *material = m_material;
    // TODO: Get the validity from the material or FALSE if it's not found
    *valid = TRUE;

    return D3D_OK;
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackgroundDepth(IDirectDrawSurface *surface) {
    Logger::warn("<<< D3D6Viewport::SetBackgroundDepth: Proxy");
    return m_proxy->SetBackgroundDepth(surface);
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid) {
    Logger::warn("<<< D3D6Viewport::SetBackgroundDepth: Proxy");
    return m_proxy->GetBackgroundDepth(surface, valid);
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::Clear(DWORD count, D3DRECT *rects, DWORD flags) {
    Logger::debug("<<< D3D6Viewport::Clear: Proxy");

    HRESULT hr = m_proxy->Clear(count, rects, flags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(m_device == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    D3D6Device* d3d6Device = m_parent->GetLastUsedDevice();
    if (likely(d3d6Device != nullptr)) {
      HRESULT hr9 = d3d6Device->GetD3D9()->Clear(count, rects, flags, 0, 0.0f, 0);
      if (unlikely(FAILED(hr9)))
        Logger::err("D3D6Viewport::Clear: Failed D3D9 Clear call");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::AddLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D6Viewport::AddLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D6Light* light6 = reinterpret_cast<D3D6Light*>(light);

    auto it = std::find(m_lights.begin(), m_lights.end(), light6);
    if (unlikely(it != m_lights.end())) {
      Logger::warn("D3D6Viewport::AddLight: Pre-existing light found");
    } else {
      m_lights.push_back(light6);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::warn(">>> D3D6Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D6Light* light6 = reinterpret_cast<D3D6Light*>(light);

    auto it = std::find(m_lights.begin(), m_lights.end(), light6);
    if (likely(it != m_lights.end())) {
      m_lights.erase(it);
    } else {
      Logger::warn("D3D6Viewport::DeleteLight: Light not found");
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::NextLight(IDirect3DLight *ref, IDirect3DLight **light, DWORD flags) {
    Logger::warn("<<< D3D6Viewport::NextLight: Proxy");
    // TODO:
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D6Viewport::GetViewport2");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    *data = m_viewport;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D6Viewport::SetViewport2");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    m_viewport = *data;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackgroundDepth2(IDirectDrawSurface4 *surface) {
    Logger::debug(">>> D3D6Viewport::SetBackgroundDepth2");

    // TODO: Look up the depth and make it valid
    m_materialDepth = reinterpret_cast<DDraw4Surface*>(surface);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid) {
    Logger::debug(">>> D3D6Viewport::GetBackgroundDepth2");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_materialDepth;
    // TODO: Get the validity from the surface or FALSE if it's not found
    *valid = TRUE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::Clear2(DWORD count, D3DRECT *rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil) {
    Logger::debug("<<< D3D6Viewport::Clear2: Proxy");

    HRESULT hr = m_proxy->Clear2(count, rects, flags, color, z, stencil);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(m_device == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    D3D6Device* d3d6Device = m_parent->GetLastUsedDevice();
    if (likely(d3d6Device != nullptr)) {
      HRESULT hr9 = d3d6Device->GetD3D9()->Clear(count, rects, flags, color, z, stencil);
      if (unlikely(FAILED(hr9)))
        Logger::err("D3D6Viewport::Clear2: Failed D3D9 Clear call");
    }

    return D3D_OK;
  }

}

#include "d3d6_viewport.h"

#include "../ddraw4/ddraw4_surface.h"

#include "d3d6_light.h"
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

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    // Ignore the dvScaleX, dvScaleY, dvMaxX and dvMaxY values for now
    data->dwX      = m_viewport.dwX;
    data->dwY      = m_viewport.dwY;
    data->dwWidth  = m_viewport.dwWidth;
    data->dwHeight = m_viewport.dwHeight;
    data->dvScaleX = 0;
    data->dvScaleY = 0;
    data->dvMaxX   = 0;
    data->dvMaxY   = 0;
    data->dvMinZ   = m_viewport.dvMinZ;
    data->dvMaxZ   = m_viewport.dvMaxZ;

    return D3D_OK;
  }

  // Legacy call, technically should not be very common
  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::warn("<<< D3D6Viewport::SetViewport: Proxy");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    // Ignore the dvScaleX, dvScaleY, dvMaxX and dvMaxY values for now
    m_viewport.dwX      = data->dwX;
    m_viewport.dwY      = data->dwY;
    m_viewport.dwWidth  = data->dwWidth;
    m_viewport.dwHeight = data->dwHeight;
    m_viewport.dvMinZ   = data->dvMinZ;
    m_viewport.dvMaxZ   = data->dvMaxZ;

    if (likely(m_device != nullptr))
      ApplyViewport();

    return D3D_OK;
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

    if (unlikely(m_materialHandle == hMat))
      return D3D_OK;

    D3D6Material* material = m_parent->GetMaterialFromHandle(hMat);

    if (unlikely(material == nullptr))
      return DDERR_INVALIDPARAMS;

    m_materialHandle = hMat;
    m_materialIsSet = TRUE;

    if (likely(m_device != nullptr))
      ApplyMaterial();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug(">>> D3D6Viewport::GetBackground");

    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(m_materialIsSet))
      *material = m_materialHandle;
    *valid = m_materialIsSet;

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
      HRESULT hr9 = d3d6Device->GetD3D9()->Clear(count, rects, flags, 0, 1.0f, 0);
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

    if (likely(m_device != nullptr))
      ApplyAndActivateLights();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D6Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D6Light* light6 = reinterpret_cast<D3D6Light*>(light);

    auto it = std::find(m_lights.begin(), m_lights.end(), light6);
    if (likely(it != m_lights.end())) {
      m_lights.erase(it);
    } else {
      Logger::warn("D3D6Viewport::DeleteLight: Light not found");
    }

    if (likely(m_device != nullptr))
      ApplyAndActivateLights();

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

    if (likely(m_device != nullptr))
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackgroundDepth2(IDirectDrawSurface4 *surface) {
    Logger::warn(">>> D3D6Viewport::SetBackgroundDepth2");

    m_materialDepth = reinterpret_cast<DDraw4Surface*>(surface);
    m_materialDepthIsSet = TRUE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid) {
    Logger::warn(">>> D3D6Viewport::GetBackgroundDepth2");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(m_materialDepthIsSet))
      *surface = m_materialDepth;
    *valid = m_materialDepthIsSet;

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

  HRESULT D3D6Viewport::ApplyViewport() {
    Logger::debug("D3D6Viewport: Applying viewport to D3D9");

    d3d9::D3DVIEWPORT9 viewport9;
    viewport9.X = m_viewport.dwX;
    viewport9.Y = m_viewport.dwY;
    viewport9.Width  = m_viewport.dwWidth;
    viewport9.Height = m_viewport.dwHeight;
    viewport9.MinZ = m_viewport.dvMinZ;
    viewport9.MaxZ = m_viewport.dvMaxZ;

    HRESULT hr = m_device->GetD3D9()->SetViewport(&viewport9);
    if(unlikely(FAILED(hr)))
      Logger::err("D3D6Viewport: Failed to set the D3D9 viewport");

    return hr;
  }

  HRESULT D3D6Viewport::ApplyMaterial() {
    if (!m_materialIsSet)
      return D3D_OK;

    Logger::debug("D3D6Viewport: Applying material to D3D9");

    D3DMATERIAL material;
    m_parent->GetMaterialFromHandle(m_materialHandle)->GetMaterial(&material);

    d3d9::D3DMATERIAL9 material9;
    material9.Diffuse  = material.dcvDiffuse;
    material9.Ambient  = material.dcvAmbient;
    material9.Specular = material.dcvSpecular;
    material9.Emissive = material.dcvEmissive;
    material9.Power    = material.dvPower;

    HRESULT hr = m_device->GetD3D9()->SetMaterial(&material9);
    if(unlikely(FAILED(hr)))
      Logger::err("D3D6Viewport: Failed to set the D3D9 material");

    return hr;
  }

  HRESULT D3D6Viewport::ApplyAndActivateLights() {
    Logger::debug("D3D6Viewport: Applying lights to D3D9");

    uint32_t lightcounter = 0;

    D3DLIGHT2 currentLight;
    currentLight.dwSize = sizeof(D3DLIGHT2);

    d3d9::D3DLIGHT9 light9;

    for (auto light: m_lights) {
      light->GetLight(reinterpret_cast<D3DLIGHT*>(&currentLight));

      const bool noSpecular = currentLight.dwFlags & D3DLIGHT_NO_SPECULAR;

      light9.Type         = d3d9::D3DLIGHTTYPE(currentLight.dltType);
      light9.Diffuse      = currentLight.dcvColor;
      light9.Specular     = noSpecular ? D3DCOLORVALUE({0, 0, 0, 0}) : currentLight.dcvColor;
      light9.Ambient      = currentLight.dcvColor;
      light9.Position     = currentLight.dvPosition;
      light9.Direction    = currentLight.dvDirection;
      light9.Range        = currentLight.dvRange;
      light9.Falloff      = currentLight.dvFalloff;
      light9.Attenuation0 = currentLight.dvAttenuation0;
      light9.Attenuation1 = currentLight.dvAttenuation1;
      light9.Attenuation2 = currentLight.dvAttenuation2;
      light9.Theta        = currentLight.dvTheta;
      light9.Phi          = currentLight.dvPhi;

      HRESULT hr = m_device->GetD3D9()->SetLight(lightcounter, &light9);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Viewport::Clear2: Failed D3D9 SetLight call");
      } else {
        if (currentLight.dwFlags & D3DLIGHT_ACTIVE) {
          m_device->GetD3D9()->LightEnable(lightcounter, TRUE);
        } else {
          // TODO: Clear and inactivate all exising lights on viewport reset
          m_device->GetD3D9()->LightEnable(lightcounter, FALSE);
        }

        lightcounter++;
      }
    }

    return D3D_OK;
  }

}

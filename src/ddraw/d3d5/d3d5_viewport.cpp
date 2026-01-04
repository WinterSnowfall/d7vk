#include "d3d5_viewport.h"

#include "../ddraw_surface.h"

#include "d3d5_light.h"
#include "d3d5_device.h"
#include "d3d5_material.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D5Viewport::s_viewportCount = 0;

  D3D5Viewport::D3D5Viewport(Com<IDirect3DViewport2>&& proxyViewport, D3D5Interface* pParent)
    : DDrawWrappedObject<D3D5Interface, IDirect3DViewport2, IUnknown>(pParent, std::move(proxyViewport), nullptr) {
    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D5Viewport: Created a new viewport nr. [[2-", m_viewportCount, "]]"));
  }

  D3D5Viewport::~D3D5Viewport() {
    // Dissasociate every bound light from this viewport
    for (auto light : m_lights) {
      light->SetViewport(nullptr);
    }

    Logger::debug(str::format("D3D5Viewport: Viewport nr. [[2-", m_viewportCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D5Interface, IDirect3DViewport2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DViewport2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Viewport::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D5Viewport::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy viewport interfaces
    if (unlikely(riid == __uuidof(IDirect3DViewport))) {
      Logger::warn("D3D5Viewport::QueryInterface: Query for legacy IDirect3DViewport");
      // Revenant uses this QueryInterface call as a poor man's ref increment,
      // and does absolutely nothing with the object. Since this isn't used at
      // all in other contexts, make this a global hack of sorts, for now.
      *ppvObject = ref(this);
      return S_OK;
      //return m_proxy->QueryInterface(riid, ppvObject);
    }

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  // Docs state: "The IDirect3DViewport2::Initialize method is not implemented."
  HRESULT STDMETHODCALLTYPE D3D5Viewport::Initialize(LPDIRECT3D lpDirect3D) {
    Logger::debug(">>> D3D5Viewport::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::GetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D5Viewport::GetViewport");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_viewportIsSet))
      return D3DERR_VIEWPORTDATANOTSET;

    data->dwX      = m_viewport.dwX;
    data->dwY      = m_viewport.dwY;
    data->dwWidth  = m_viewport.dwWidth;
    data->dwHeight = m_viewport.dwHeight;
    data->dvMinZ   = m_viewport.dvMinZ;
    data->dvMaxZ   = m_viewport.dvMaxZ;
    // D3DVIEWPORT specific
    data->dvMaxX   = m_viewport.dvMaxX;
    data->dvMaxY   = m_viewport.dvMaxY;
    data->dvScaleX = m_viewport.dvScaleX;
    data->dvScaleY = m_viewport.dvScaleY;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D5Viewport::SetViewport");

    HRESULT hr = m_proxy->SetViewport(data);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_device == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    // TODO: Check viewport dimensions against the currently set RT

    // The docs state: "The method ignores the values in the dvMaxX, dvMaxY,
    // dvMinZ, and dvMaxZ members.", which appears correct.
    m_viewport9.X      = m_viewport.dwX      = data->dwX;
    m_viewport9.Y      = m_viewport.dwY      = data->dwY;
    m_viewport9.Width  = m_viewport.dwWidth  = data->dwWidth;
    m_viewport9.Height = m_viewport.dwHeight = data->dwHeight;
    m_viewport9.MinZ   = m_viewport.dvMinZ   = 0.0f;
    m_viewport9.MaxZ   = m_viewport.dvMaxZ   = 1.0f;
    // D3DVIEWPORT specific
    m_viewport.dvScaleX = data->dvScaleX;
    m_viewport.dvScaleY = data->dvScaleY;
    m_viewport.dvMaxX   = 1.0f;
    m_viewport.dvMaxY   = 1.0f;

    m_viewportIsSet = TRUE;

    if (m_isCurrentViewport)
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    Logger::debug("<<< D3D5Viewport::TransformVertices: Proxy");
    return m_proxy->TransformVertices(vertex_count, data, flags, offscreen);
  }

  // Docs state: "The IDirect3DViewport2::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D5Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    Logger::warn(">>> D3D6Viewport::LightElements");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    Logger::debug(">>> D3D5Viewport::SetBackground");

    if (unlikely(m_materialHandle == hMat))
      return D3D_OK;

    D3D5Material* material5 = m_parent->GetMaterialFromHandle(hMat);

    if (unlikely(material5 == nullptr))
      return DDERR_INVALIDPARAMS;

    m_materialHandle = hMat;
    m_materialIsSet = TRUE;

    // The viewport will be set to the diffuse values of
    // the material/background when cleared. It is not used
    // in any other way as far as I can tell, certainly
    // not as a standard D3D9 material (see D3DLIGHTSTATE_MATERIAL).
    D3DMATERIAL material;
    material5->GetMaterial(&material);
    m_backgroundColor = D3DCOLOR_RGBA(static_cast<BYTE>(material.dcvDiffuse.r * 255.0f),
                                      static_cast<BYTE>(material.dcvDiffuse.g * 255.0f),
                                      static_cast<BYTE>(material.dcvDiffuse.b * 255.0f),
                                      static_cast<BYTE>(material.dcvDiffuse.a * 255.0f));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug(">>> D3D5Viewport::GetBackground");

    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(m_materialIsSet))
      *material = m_materialHandle;
    *valid = m_materialIsSet;

    return D3D_OK;
  }

  // One could speculate this was meant to set a z-buffer depth value
  // to be used during clears, perhaps, similarly to SetBackground(),
  // however it has not seen any practical use in the wild
  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetBackgroundDepth(IDirectDrawSurface *surface) {
    Logger::warn("!!! D3D5Viewport::SetBackgroundDepth: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid) {
    Logger::warn("!!! D3D5Viewport::SetBackgroundDepth: Stub");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *valid = FALSE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::Clear(DWORD count, D3DRECT *rects, DWORD flags) {
    Logger::debug("<<< D3D5Viewport::Clear: Proxy");

    // Fast skip
    if (unlikely(!count && rects))
      return D3D_OK;

    HRESULT hr = m_proxy->Clear(count, rects, flags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(m_device == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_isCurrentViewport) {
      D3D5Viewport* currentViewport = m_device->GetCurrentViewportInternal();
      if (currentViewport != nullptr) {
        currentViewport9 = currentViewport->GetD3D9Viewport();
      } else {
        m_device->GetD3D9()->GetViewport(&currentViewport9);
      }
      m_device->GetD3D9()->SetViewport(&m_viewport9);
    }

    HRESULT hr9 = m_device->GetD3D9()->Clear(count, rects, flags, m_backgroundColor, 1.0f, 0);
    if (unlikely(FAILED(hr9)))
      Logger::err("D3D6Viewport::Clear: Failed D3D9 Clear call");

    // Restore the previously active viewport
    if (!m_isCurrentViewport) {
      m_device->GetD3D9()->SetViewport(&currentViewport9);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::AddLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D5Viewport::AddLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D5Light* light5 = reinterpret_cast<D3D5Light*>(light);

    if (unlikely(light5->HasViewport()))
      return D3DERR_LIGHTHASVIEWPORT;

    auto it = std::find(m_lights.begin(), m_lights.end(), light5);
    if (unlikely(it != m_lights.end())) {
      Logger::warn("D3D5Viewport::AddLight: Pre-existing light found");
    } else {
      m_lights.push_back(light5);
      light5->SetViewport(this);
    }

    if (m_device != nullptr && m_isCurrentViewport)
      ApplyAndActivateLight(light5->GetIndex(), light5);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D5Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D5Light* light5 = reinterpret_cast<D3D5Light*>(light);

    auto it = std::find(m_lights.begin(), m_lights.end(), light5);
    if (likely(it != m_lights.end())) {
      const DWORD light5Index = light5->GetIndex();
      if (m_device != nullptr && m_isCurrentViewport && light5->IsActive()) {
        Logger::debug(str::format("D3D5Viewport: Disabling light nr. ", light5Index));
        m_device->GetD3D9()->LightEnable(light5Index, FALSE);
      }
      m_lights.erase(it);
      light5->SetViewport(nullptr);
    } else {
      Logger::warn("D3D5Viewport::DeleteLight: Light not found");
    }

    return D3D_OK;
  }

  // TODO:
  HRESULT STDMETHODCALLTYPE D3D5Viewport::NextLight(IDirect3DLight *ref, IDirect3DLight **light, DWORD flags) {
    Logger::warn("!!! D3D5Viewport::NextLight: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::GetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D5Viewport::GetViewport2");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_viewportIsSet))
      return D3DERR_VIEWPORTDATANOTSET;

    data->dwX      = m_viewport2.dwX;
    data->dwY      = m_viewport2.dwY;
    data->dwWidth  = m_viewport2.dwWidth;
    data->dwHeight = m_viewport2.dwHeight;
    data->dvMinZ   = m_viewport2.dvMinZ;
    data->dvMaxZ   = m_viewport2.dvMaxZ;
    // D3DVIEWPORT2 specific
    data->dvClipX      = m_viewport2.dvClipX;
    data->dvClipY      = m_viewport2.dvClipY;
    data->dvClipWidth  = m_viewport2.dvClipWidth;
    data->dvClipHeight = m_viewport2.dvClipHeight;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D5Viewport::SetViewport2");

    HRESULT hr = m_proxy->SetViewport2(data);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_device == nullptr))
      return D3DERR_VIEWPORTHASNODEVICE;

    // TODO: Check viewport dimensions against the currently set RT

    m_viewport9.X      = m_viewport2.dwX      = data->dwX;
    m_viewport9.Y      = m_viewport2.dwY      = data->dwY;
    m_viewport9.Width  = m_viewport2.dwWidth  = data->dwWidth;
    m_viewport9.Height = m_viewport2.dwHeight = data->dwHeight;
    // TODO: Check if they should also be 0.0f & 1.0f here
    m_viewport9.MinZ   = m_viewport2.dvMinZ   = data->dvMinZ;
    m_viewport9.MaxZ   = m_viewport2.dvMaxZ   = data->dvMaxZ;
    // D3DVIEWPORT2 specific
    m_viewport2.dvClipX      = data->dvClipX;
    m_viewport2.dvClipY      = data->dvClipY;
    m_viewport2.dvClipWidth  = data->dvClipWidth;
    m_viewport2.dvClipHeight = data->dvClipHeight;

    m_viewportIsSet = TRUE;

    if (m_isCurrentViewport)
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT D3D5Viewport::ApplyViewport() {
    if (!m_viewportIsSet)
      return D3D_OK;

    Logger::debug("D3D5Viewport: Applying viewport to D3D9");

    HRESULT hr = m_device->GetD3D9()->SetViewport(&m_viewport9);
    if(unlikely(FAILED(hr)))
      Logger::err("D3D5Viewport: Failed to set the D3D9 viewport");

    return hr;
  }

  HRESULT D3D5Viewport::ApplyAndActivateLights() {
    if (!m_lights.size())
      return D3D_OK;

    Logger::debug("D3D5Viewport: Applying lights to D3D9");

    for (auto light5: m_lights)
      ApplyAndActivateLight(light5->GetIndex(), light5);

    return D3D_OK;
  }

  HRESULT D3D5Viewport::ApplyAndActivateLight(DWORD index, D3D5Light* light5) {
    HRESULT hr = m_device->GetD3D9()->SetLight(index, light5->GetD3D9Light());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Viewport: Failed D3D9 SetLight call");
    } else {
      if (light5->IsActive()) {
        Logger::debug(str::format("D3D5Viewport: Enabling light nr. ", index));
        m_device->GetD3D9()->LightEnable(index, TRUE);
      } else {
        Logger::debug(str::format("D3D5Viewport: Disabling light nr. ", index));
        m_device->GetD3D9()->LightEnable(index, FALSE);
      }
    }

    return hr;
  }

}

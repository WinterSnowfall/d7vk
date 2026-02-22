#include "d3d5_viewport.h"

#include "d3d5_light.h"
#include "d3d5_device.h"
#include "d3d5_material.h"

#include "../ddraw/ddraw_surface.h"

#include "../d3d6/d3d6_viewport.h"
#include "../d3d3/d3d3_viewport.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D5Viewport::s_viewportCount = 0;

  D3D5Viewport::D3D5Viewport(D3DCommonViewport* commonViewport, Com<IDirect3DViewport2>&& proxyViewport, D3D5Interface* pParent)
    : DDrawWrappedObject<D3D5Interface, IDirect3DViewport2, IUnknown>(pParent, std::move(proxyViewport), nullptr)
    , m_commonViewport ( commonViewport ) {

    if (m_commonViewport == nullptr)
      m_commonViewport = new D3DCommonViewport();

    if (m_commonViewport->GetOrigin() == nullptr)
      m_commonViewport->SetOrigin(this);

    m_commonViewport->SetD3D5Viewport(this);

    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D5Viewport: Created a new viewport nr. [[2-", m_viewportCount, "]]"));
  }

  D3D5Viewport::~D3D5Viewport() {
    // Dissasociate every bound light from this viewport
    for (auto light : m_lights) {
      light->SetViewport(nullptr);
    }

    m_commonViewport->SetD3D5Viewport(nullptr);

    Logger::debug(str::format("D3D5Viewport: Viewport nr. [[2-", m_viewportCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D5Interface, IDirect3DViewport2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DViewport2))
      return this;

    throw DxvkError("D3D5Viewport::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy viewport interfaces
    if (unlikely(riid == __uuidof(IDirect3DViewport))) {
      if (m_commonViewport->GetD3D3Viewport() != nullptr) {
        Logger::debug("D3D6Viewport::QueryInterface: Query for existing IDirect3DViewport");
        return m_commonViewport->GetD3D3Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D5Viewport::QueryInterface: Query for IDirect3DViewport");

      Com<IDirect3DViewport> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Viewport(m_commonViewport.ptr(), std::move(ppvProxyObject), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DViewport3))) {
      if (m_commonViewport->GetD3D6Viewport() != nullptr) {
        Logger::debug("D3D6Viewport::QueryInterface: Query for existing IDirect3DViewport3");
        return m_commonViewport->GetD3D6Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D5Viewport::QueryInterface: Query for IDirect3DViewport3");

      Com<IDirect3DViewport3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D6Viewport(m_commonViewport.ptr(), std::move(ppvProxyObject), nullptr));

      return S_OK;
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
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::GetViewport: Proxy");
      return m_proxy->SetViewport(data);
    }

    Logger::debug(">>> D3D5Viewport::GetViewport");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->IsViewportSet()))
      return D3DERR_VIEWPORTDATANOTSET;

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    data->dwX      = viewport9->X;
    data->dwY      = viewport9->Y;
    data->dwWidth  = viewport9->Width;
    data->dwHeight = viewport9->Height;
    data->dvMinZ   = viewport9->MinZ;
    data->dvMaxZ   = viewport9->MaxZ;
    // Docs state: "Values of the D3DVALUE type describing the maximum and minimum
    // nonhomogeneous coordinates of x, y, and z. Again, the relevant coordinates
    // are the nonhomogeneous coordinates that result from the perspective division."
    data->dvMaxX   = 1.0f;
    data->dvMaxY   = 1.0f;
    data->dvScaleX = 1.0f;
    data->dvScaleY = 1.0f;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetViewport(D3DVIEWPORT *data) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::SetViewport: Proxy");
      return m_proxy->SetViewport(data);
    }

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

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    // The docs state: "The method ignores the values in the dvMaxX, dvMaxY,
    // dvMinZ, and dvMaxZ members.", which appears correct.
    viewport9->X      = data->dwX;
    viewport9->Y      = data->dwY;
    viewport9->Width  = data->dwWidth;
    viewport9->Height = data->dwHeight;
    viewport9->MinZ   = 0.0f;
    viewport9->MaxZ   = 1.0f;

    m_commonViewport->MarkViewportAsSet();

    if (m_commonViewport->IsCurrentViewport())
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    Logger::debug("<<< D3D5Viewport::TransformVertices: Proxy");
    return m_proxy->TransformVertices(vertex_count, data, flags, offscreen);
  }

  // Docs state: "The IDirect3DViewport2::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D5Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    Logger::warn(">>> D3D5Viewport::LightElements");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::SetBackground: Proxy");
      return m_proxy->SetBackground(hMat);
    }

    Logger::debug(">>> D3D5Viewport::SetBackground");

    if (unlikely(m_commonViewport->GetMaterialHandle() == hMat))
      return D3D_OK;

    D3D5Material* material5 = m_parent->GetMaterialFromHandle(hMat);

    if (unlikely(material5 == nullptr))
      return DDERR_INVALIDPARAMS;

    m_commonViewport->MarkMaterialAsSet();

    m_commonViewport->SetMaterialHandle(hMat);
    // The viewport will be set to the diffuse values of
    // the material/background when cleared. It is not used
    // in any other way as far as I can tell, certainly
    // not as a standard D3D9 material (see D3DLIGHTSTATE_MATERIAL).
    m_commonViewport->SetBackgroundColor(material5->GetMaterialColor());

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::GetBackground: Proxy");
      return m_proxy->GetBackground(material, valid);
    }

    Logger::debug(">>> D3D5Viewport::GetBackground");

    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    if (likely(m_commonViewport->IsMaterialSet()))
      *material = m_commonViewport->GetMaterialHandle();
    *valid = m_commonViewport->IsMaterialSet();

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
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::Clear: Proxy");
      return m_proxy->Clear(count, rects, flags);
    }

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
    if (!m_commonViewport->IsCurrentViewport()) {
      D3D5Viewport* currentViewport = m_device->GetCurrentViewportInternal();
      if (currentViewport != nullptr) {
        currentViewport9 = *currentViewport->GetCommonViewport()->GetD3D9Viewport();
      } else {
        m_device->GetD3D9()->GetViewport(&currentViewport9);
      }
      m_device->GetD3D9()->SetViewport(m_commonViewport->GetD3D9Viewport());
    }

    HRESULT hr9 = m_device->GetD3D9()->Clear(count, rects, flags, m_commonViewport->GetBackgroundColor(), 1.0f, 0.0f);
    if (unlikely(FAILED(hr9)))
      Logger::err("D3D5Viewport::Clear: Failed D3D9 Clear call");

    // Restore the previously active viewport
    if (!m_commonViewport->IsCurrentViewport()) {
      m_device->GetD3D9()->SetViewport(&currentViewport9);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::AddLight(IDirect3DLight *light) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::AddLight: Proxy");
      D3D5Light* light5 = reinterpret_cast<D3D5Light*>(light);
      return m_proxy->AddLight(light5->GetProxied());
    }

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

    if (m_device != nullptr && m_commonViewport->IsCurrentViewport())
      ApplyAndActivateLight(light5->GetIndex(), light5);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::DeleteLight(IDirect3DLight *light) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::DeleteLight: Proxy");
      D3D5Light* light5 = reinterpret_cast<D3D5Light*>(light);
      return m_proxy->DeleteLight(light5->GetProxied());
    }

    Logger::debug(">>> D3D5Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D5Light* light5 = reinterpret_cast<D3D5Light*>(light);

    auto it = std::find(m_lights.begin(), m_lights.end(), light5);
    if (likely(it != m_lights.end())) {
      const DWORD light5Index = light5->GetIndex();
      if (m_device != nullptr && m_commonViewport->IsCurrentViewport() && light5->IsActive()) {
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
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::SetViewport2: Proxy");
      return m_proxy->GetViewport2(data);
    }

    Logger::debug(">>> D3D5Viewport::GetViewport2");

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->IsViewportSet()))
      return D3DERR_VIEWPORTDATANOTSET;

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    data->dwX          = viewport9->X;
    data->dwY          = viewport9->Y;
    data->dwWidth      = viewport9->Width;
    data->dwHeight     = viewport9->Height;
    data->dvMinZ       = viewport9->MinZ;
    data->dvMaxZ       = viewport9->MaxZ;
    // Docs state: "In most cases, dvClipX is set to -1.0 and dvClipY is set
    // to the inverse of the viewport's aspect ratio on the target surface,
    // which can be calculated by dividing the dwHeight member by dwWidth.
    // Similarly, the dvClipWidth member is typically 2.0 and dvClipHeight is
    // set to twice the aspect ratio set in dwClipY. The dvMinZ and dvMaxZ
    // are usually set to 0.0 and 1.0."
    data->dvClipX      = -1.0f;
    data->dvClipY      = -1.0f * (static_cast<float>(viewport9->Height) / 
                                  static_cast<float>(viewport9->Width));
    data->dvClipWidth  =  2.0f;
    data->dvClipHeight = -2.0f * data->dvClipY;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Viewport::SetViewport2(D3DVIEWPORT2 *data) {
    if (unlikely(m_parent != nullptr && m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Viewport::SetViewport2: Proxy");
      return m_proxy->SetViewport2(data);
    }

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

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    viewport9->X      = data->dwX;
    viewport9->Y      = data->dwY;
    viewport9->Width  = data->dwWidth;
    viewport9->Height = data->dwHeight;
    viewport9->MinZ   = data->dvMinZ;
    viewport9->MaxZ   = data->dvMaxZ;

    m_commonViewport->MarkViewportAsSet();

    if (m_commonViewport->IsCurrentViewport())
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT D3D5Viewport::ApplyViewport() {
    if (!m_commonViewport->IsViewportSet())
      return D3D_OK;

    Logger::debug("D3D5Viewport: Applying viewport to D3D9");

    HRESULT hr = m_device->GetD3D9()->SetViewport(m_commonViewport->GetD3D9Viewport());
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
      HRESULT hrLE;
      if (light5->IsActive()) {
        Logger::debug(str::format("D3D5Viewport: Enabling light nr. ", index));
        hrLE = m_device->GetD3D9()->LightEnable(index, TRUE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D5Viewport: Failed D3D9 LightEnable call (TRUE)");
      } else {
        Logger::debug(str::format("D3D5Viewport: Disabling light nr. ", index));
        hrLE = m_device->GetD3D9()->LightEnable(index, FALSE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D5Viewport: Failed D3D9 LightEnable call (FALSE)");
      }
    }

    return hr;
  }

}

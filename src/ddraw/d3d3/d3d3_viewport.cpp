#include "d3d3_viewport.h"

#include "d3d3_device.h"

#include "../d3d_common_light.h"
#include "../d3d_common_material.h"

#include "../ddraw/ddraw_surface.h"

#include "../d3d5/d3d5_viewport.h"
#include "../d3d6/d3d6_viewport.h"

#include <algorithm>

namespace dxvk {

  uint32_t D3D3Viewport::s_viewportCount = 0;

  D3D3Viewport::D3D3Viewport(D3DCommonViewport* commonViewport, Com<IDirect3DViewport>&& proxyViewport, D3D3Interface* pParent)
    : DDrawWrappedObject<D3D3Interface, IDirect3DViewport, IUnknown>(pParent, std::move(proxyViewport), nullptr)
    , m_commonViewport ( commonViewport ) {

    if (m_commonViewport == nullptr)
      m_commonViewport = new D3DCommonViewport(m_parent->GetCommonD3DInterface());

    if (m_commonViewport->GetOrigin() == nullptr)
      m_commonViewport->SetOrigin(this);

    m_commonViewport->SetD3D3Viewport(this);

    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D3Viewport: Created a new viewport nr. [[1-", m_viewportCount, "]]"));
  }

  D3D3Viewport::~D3D3Viewport() {
    // Dissasociate every bound light from this viewport
    for (auto light : m_lights) {
      light->SetViewport3(nullptr);
    }

    m_commonViewport->SetD3D3Viewport(nullptr);

    Logger::debug(str::format("D3D3Viewport: Viewport nr. [[1-", m_viewportCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D3Interface, IDirect3DViewport, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DViewport))
      return this;

    throw DxvkError("D3D3Viewport::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D3Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DViewport2))) {
      if (m_commonViewport->GetD3D5Viewport() != nullptr) {
        Logger::debug("D3D3Viewport::QueryInterface: Query for existing IDirect3DViewport2");
        return m_commonViewport->GetD3D5Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D3Viewport::QueryInterface: Query for IDirect3DViewport2");

      Com<IDirect3DViewport2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D5Viewport(m_commonViewport.ptr(), std::move(ppvProxyObject), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DViewport3))) {
      if (m_commonViewport->GetD3D6Viewport() != nullptr) {
        Logger::debug("D3D3Viewport::QueryInterface: Query for existing IDirect3DViewport3");
        return m_commonViewport->GetD3D6Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D3Viewport::QueryInterface: Query for IDirect3DViewport3");

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
  HRESULT STDMETHODCALLTYPE D3D3Viewport::Initialize(LPDIRECT3D lpDirect3D) {
    Logger::debug(">>> D3D3Viewport::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D3Viewport::GetViewport");

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

    data->dvMaxX   = 1.0f;
    data->dvMaxY   = 1.0f;
    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    data->dvScaleX = legacyScale->x * (float)data->dwWidth / 2.0f;
    data->dvScaleY = legacyScale->y * (float)data->dwHeight / 2.0f;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D3Viewport::SetViewport");

    HRESULT hr = m_proxy->SetViewport(data);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    // TODO: Check viewport dimensions against the currently set RT,
    // and perform some sanity checks (positive, non-zero dimensions)

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    // The docs state: "The method ignores the values in the dvMaxX, dvMaxY,
    // dvMinZ, and dvMaxZ members.", which appears correct.
    viewport9->X      = data->dwX;
    viewport9->Y      = data->dwY;
    viewport9->Width  = data->dwWidth;
    viewport9->Height = data->dwHeight;
    viewport9->MinZ   = 0.0f;
    viewport9->MaxZ   = 1.0f;

    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    legacyScale->x = 2.0f * data->dvScaleX / (float)data->dwWidth;
    legacyScale->y = 2.0f * data->dvScaleY / (float)data->dwHeight;
    legacyScale->z = 1.0f;
    D3DVECTOR* legacyClip = m_commonViewport->GetLegacyClip();
    legacyClip->x = 0.0f;
    legacyClip->y = 0.0f;
    legacyClip->z = 0.0f;

    m_commonViewport->MarkViewportAsSet();

    if (m_commonViewport->IsCurrentViewport())
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    Logger::warn("<<< D3D3Viewport::TransformVertices: Proxy");
    return m_proxy->TransformVertices(vertex_count, data, flags, offscreen);
  }

  // Docs state: "The IDirect3DViewport::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    Logger::warn(">>> D3D3Viewport::LightElements");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    // Workaround: Revenant sets the background on a IDirect3DViewport viewport
    if (unlikely(m_commonViewport->GetD3D6Viewport() != nullptr)) {
      Logger::debug(">>> D3D3Viewport::SetBackground");
      return m_commonViewport->GetD3D6Viewport()->SetBackground(hMat);
    }

    Logger::debug(">>> D3D3Viewport::SetBackground");

    if (unlikely(m_commonViewport->GetMaterialHandle() == hMat))
      return D3D_OK;

    D3DCommonMaterial* commonMaterial = m_commonViewport->GetCommonD3DInterface()->GetCommonMaterialFromHandle(hMat);

    if (unlikely(commonMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    m_commonViewport->MarkMaterialAsSet();

    // Cache only the set material handle, as its color can
    // change after it is set (get it on Clear directly)
    m_commonViewport->SetMaterialHandle(hMat);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug(">>> D3D3Viewport::GetBackground");

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
  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetBackgroundDepth(IDirectDrawSurface *surface) {
    Logger::warn("!!! D3D3Viewport::SetBackgroundDepth: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid) {
    Logger::warn("!!! D3D3Viewport::SetBackgroundDepth: Stub");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *valid = FALSE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::Clear(DWORD count, D3DRECT *rects, DWORD flags) {
    Logger::debug("<<< D3D3Viewport::Clear: Proxy");

    // Fast skip
    if (unlikely(!count && rects))
      return D3D_OK;

    HRESULT hr = m_proxy->Clear(count, rects, flags);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetD3D9Device();

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_commonViewport->IsCurrentViewport()) {
      D3D3Viewport* currentViewport = m_commonViewport->GetCurrentD3D3Viewport();
      if (currentViewport != nullptr) {
        currentViewport9 = *currentViewport->GetCommonViewport()->GetD3D9Viewport();
      } else {
        d3d9Device->GetViewport(&currentViewport9);
      }
      d3d9Device->SetViewport(m_commonViewport->GetD3D9Viewport());
    }

    static constexpr D3DCOLOR defaultColor = D3DCOLOR_RGBA(0, 0, 0, 0);
    D3DMATERIALHANDLE handle = m_commonViewport->GetMaterialHandle();
    D3DCommonMaterial* commonMaterial = m_commonViewport->GetCommonD3DInterface()->GetCommonMaterialFromHandle(handle);
    D3DCOLOR clearColor = commonMaterial != nullptr ? commonMaterial->GetMaterialColor() : defaultColor;

    HRESULT hr9 = d3d9Device->Clear(count, rects, flags, clearColor, 1.0f, 0u);
    if (unlikely(FAILED(hr9)))
      Logger::err("D3D3Viewport::Clear: Failed D3D9 Clear call");

    // Restore the previously active viewport
    if (!m_commonViewport->IsCurrentViewport()) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::AddLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D3Viewport::AddLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(d3dLight->HasViewport()))
      return D3DERR_LIGHTHASVIEWPORT;

    if (m_commonViewport->HasDevice()) {
      Logger::debug("D3D3Viewport::AddLight: Enabling device legacy light model");
      m_commonViewport->EnableLegacyLights(d3dLight->IsD3DLight2());
    }

    // No need to check if the light is already attached, since
    // if that's the case it will have a set viewport above
    m_lights.push_back(d3dLight);
    d3dLight->SetViewport3(this);

    if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport())
      ApplyAndActivateLight(d3dLight->GetIndex(), d3dLight);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D3Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(!d3dLight->HasViewport()))
      return DDERR_INVALIDPARAMS;

    auto it = std::find(m_lights.begin(), m_lights.end(), d3dLight);
    if (likely(it != m_lights.end())) {
      const DWORD lightIndex = d3dLight->GetIndex();
      if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport() && d3dLight->IsActive()) {
        Logger::debug(str::format("D3D3Viewport: Disabling light nr. ", lightIndex));
        m_commonViewport->GetD3D9Device()->LightEnable(lightIndex, FALSE);
      }
      m_lights.erase(it);
      d3dLight->SetViewport3(nullptr);
    } else {
      Logger::warn("D3D3Viewport::DeleteLight: Light not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::NextLight(IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD flags) {
    Logger::debug(">>> D3D3Viewport::NextLight");

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
        Logger::warn("D3D3Viewport::NextLight: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_lights.size() > 0))
        *lplpDirect3DLight = m_lights.back().ref();
    }

    return D3D_OK;
  }

  HRESULT D3D3Viewport::ApplyViewport() {
    if (!m_commonViewport->IsViewportSet())
      return D3D_OK;

    Logger::debug("D3D3Viewport: Applying viewport to D3D9");

    HRESULT hr = m_commonViewport->GetD3D9Device()->SetViewport(m_commonViewport->GetD3D9Viewport());
    if(unlikely(FAILED(hr)))
      Logger::err("D3D3Viewport: Failed to set the D3D9 viewport");

    return hr;
  }

  HRESULT D3D3Viewport::ApplyAndActivateLights() {
    if (!m_lights.size())
      return D3D_OK;

    Logger::debug("D3D3Viewport: Applying lights to D3D9");

    for (auto light: m_lights)
      ApplyAndActivateLight(light->GetIndex(), light.ptr());

    return D3D_OK;
  }

  HRESULT D3D3Viewport::ApplyAndActivateLight(DWORD index, D3DLight* light) {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetD3D9Device();

    HRESULT hr = d3d9Device->SetLight(index, light->GetD3D9Light());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D3Viewport: Failed D3D9 SetLight call");
    } else {
      HRESULT hrLE;
      if (light->IsActive()) {
        Logger::debug(str::format("D3D3Viewport: Enabling light nr. ", index));
        hrLE = d3d9Device->LightEnable(index, TRUE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D3Viewport: Failed D3D9 LightEnable call (TRUE)");
      } else {
        Logger::debug(str::format("D3D3Viewport: Disabling light nr. ", index));
        hrLE = d3d9Device->LightEnable(index, FALSE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D3Viewport: Failed D3D9 LightEnable call (FALSE)");
      }
    }

    return hr;
  }

}

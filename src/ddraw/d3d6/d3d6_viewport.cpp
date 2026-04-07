#include "d3d6_viewport.h"

#include "d3d6_device.h"

#include "../d3d_light.h"
#include "../d3d_common_material.h"

#include "../ddraw4/ddraw4_surface.h"

#include "../d3d5/d3d5_viewport.h"
#include "../d3d3/d3d3_viewport.h"

#include <vector>
#include <algorithm>

namespace dxvk {

  uint32_t D3D6Viewport::s_viewportCount = 0;

  D3D6Viewport::D3D6Viewport(
        D3DCommonViewport* commonViewport,
        Com<IDirect3DViewport3>&& proxyViewport,
        D3D6Interface* pParent)
    : DDrawWrappedObject<D3D6Interface, IDirect3DViewport3, IUnknown>(pParent, std::move(proxyViewport), nullptr)
    , m_commonViewport ( commonViewport ) {

    if (m_commonViewport == nullptr)
      m_commonViewport = new D3DCommonViewport(m_parent->GetCommonD3DInterface());

    m_commonViewport->SetD3D6Viewport(this);

    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D6Viewport: Created a new viewport nr. [[3-", m_viewportCount, "]]"));
  }

  D3D6Viewport::~D3D6Viewport() {
    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    // Dissasociate every bound light from this viewport
    for (auto light : lights) {
      light->SetViewport6(nullptr);
    }

    m_commonViewport->SetD3D6Viewport(nullptr);

    Logger::debug(str::format("D3D6Viewport: Viewport nr. [[3-", m_viewportCount, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy viewport interfaces
    if (unlikely(riid == __uuidof(IDirect3DViewport))) {
      if (m_commonViewport->GetD3D3Viewport() != nullptr) {
        Logger::debug("D3D6Viewport::QueryInterface: Query for existing IDirect3DViewport");
        return m_commonViewport->GetD3D3Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D6Viewport::QueryInterface: Query for IDirect3DViewport");

      Com<IDirect3DViewport> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D3Viewport(m_commonViewport.ptr(), std::move(ppvProxyObject), nullptr));

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DViewport2))) {
      if (m_commonViewport->GetD3D5Viewport() != nullptr) {
        Logger::debug("D3D6Viewport::QueryInterface: Query for existing IDirect3DViewport2");
        return m_commonViewport->GetD3D5Viewport()->QueryInterface(riid, ppvObject);
      }

      Logger::debug("D3D6Viewport::QueryInterface: Query for IDirect3DViewport2");

      Com<IDirect3DViewport2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new D3D5Viewport(m_commonViewport.ptr(), std::move(ppvProxyObject), nullptr));

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

  // Docs state: "The IDirect3DViewport3::Initialize method is not implemented."
  HRESULT STDMETHODCALLTYPE D3D6Viewport::Initialize(IDirect3D *d3d) {
    Logger::debug(">>> D3D6Viewport::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D6Viewport::GetViewport");

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

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::debug(">>> D3D6Viewport::SetViewport");

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

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug(">>> D3D6Viewport::GetBackground");

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
  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackgroundDepth(IDirectDrawSurface *surface) {
    Logger::warn("!!! D3D6Viewport::SetBackgroundDepth: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid) {
    Logger::warn("!!! D3D6Viewport::SetBackgroundDepth: Stub");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *valid = FALSE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::Clear(DWORD count, D3DRECT *rects, DWORD flags) {
    Logger::debug("<<< D3D6Viewport::Clear: Proxy");

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
      D3D6Viewport* currentViewport = m_commonViewport->GetCurrentD3D6Viewport();
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
      Logger::err("D3D6Viewport::Clear: Failed D3D9 Clear call");

    // Restore the previously active viewport
    if (!m_commonViewport->IsCurrentViewport()) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::AddLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D6Viewport::AddLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(d3dLight->HasViewport()))
      return D3DERR_LIGHTHASVIEWPORT;

    if (m_commonViewport->HasDevice()) {
      Logger::debug("D3D6Viewport::AddLight: Enabling device legacy light model");
      m_commonViewport->EnableLegacyLights(d3dLight->IsD3DLight2());
    }

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();
    // No need to check if the light is already attached, since
    // if that's the case it will have a set viewport above
    lights.push_back(d3dLight);
    d3dLight->SetViewport6(this);

    if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport())
      ApplyAndActivateLight(d3dLight->GetIndex(), d3dLight);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::debug(">>> D3D6Viewport::DeleteLight");

    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(!d3dLight->HasViewport()))
      return DDERR_INVALIDPARAMS;

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    auto it = std::find(lights.begin(), lights.end(), d3dLight);
    if (likely(it != lights.end())) {
      const DWORD lightIndex = d3dLight->GetIndex();
      if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport() && d3dLight->IsActive()) {
        Logger::debug(str::format("D3D6Viewport: Disabling light nr. ", lightIndex));
        m_commonViewport->GetD3D9Device()->LightEnable(lightIndex, FALSE);
      }
      lights.erase(it);
      d3dLight->SetViewport6(nullptr);
    } else {
      Logger::warn("D3D6Viewport::DeleteLight: Light not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::NextLight(IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD flags) {
    Logger::debug(">>> D3D6Viewport::NextLight");

    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    if (flags & D3DNEXT_HEAD) {
      if (likely(lights.size() > 0))
        *lplpDirect3DLight = lights.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DLight == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(lights.size() > 0))
        Logger::warn("D3D6Viewport::NextLight: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(lights.size() > 0))
        *lplpDirect3DLight = lights.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D6Viewport::GetViewport2");

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

    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    data->dvClipWidth  = 2.0f / legacyScale->x;
    data->dvClipHeight = 2.0f / legacyScale->y;
    D3DVECTOR* legacyClip = m_commonViewport->GetLegacyClip();
    data->dvClipX      = data->dvClipWidth  * (legacyClip->x + 1.0f) / -2.0f;
    data->dvClipY      = data->dvClipHeight * (legacyClip->y - 1.0f) / -2.0f;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetViewport2(D3DVIEWPORT2 *data) {
    Logger::debug(">>> D3D6Viewport::SetViewport2");

    HRESULT hr = m_proxy->SetViewport2(data);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT2)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    // TODO: Check viewport dimensions against the currently set RT,
    // and perform some sanity checks (positive, non-zero dimensions)

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    viewport9->X      = data->dwX;
    viewport9->Y      = data->dwY;
    viewport9->Width  = data->dwWidth;
    viewport9->Height = data->dwHeight;
    viewport9->MinZ   = 0.0f;
    viewport9->MaxZ   = 1.0f;

    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    legacyScale->x = 2.0f / data->dvClipWidth;
    legacyScale->y = 2.0f / data->dvClipHeight;
    legacyScale->z = 1.0f / (data->dvMaxZ - data->dvMinZ);
    D3DVECTOR* legacyClip = m_commonViewport->GetLegacyClip();
    legacyClip->x = -2.0f * data->dvClipX / data->dvClipWidth - 1.0f;
    legacyClip->y = -2.0f * data->dvClipY / data->dvClipHeight + 1.0f;
    legacyClip->z = -data->dvMinZ / (data->dvMaxZ - data->dvMinZ);

    m_commonViewport->MarkViewportAsSet();

    if (m_commonViewport->IsCurrentViewport())
      ApplyViewport();

    return D3D_OK;
  }

  // One could speculate this was meant to set a z-buffer depth value
  // to be used during clears, perhaps, similarly to SetBackground(),
  // however it has not seen any practical use in the wild
  HRESULT STDMETHODCALLTYPE D3D6Viewport::SetBackgroundDepth2(IDirectDrawSurface4 *surface) {
    Logger::warn("!!! D3D6Viewport::SetBackgroundDepth2: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid) {
    Logger::warn("!!! D3D6Viewport::GetBackgroundDepth2: Stub");

    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *valid = FALSE;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Viewport::Clear2(DWORD count, D3DRECT *rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil) {
    Logger::debug("<<< D3D6Viewport::Clear2: Proxy");

    // Fast skip
    if (unlikely(!count && rects))
      return D3D_OK;

    HRESULT hr = m_proxy->Clear2(count, rects, flags, color, z, stencil);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetD3D9Device();

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_commonViewport->IsCurrentViewport()) {
      D3D6Viewport* currentViewport = m_commonViewport->GetCurrentD3D6Viewport();
      if (currentViewport != nullptr) {
        currentViewport9 = *currentViewport->GetCommonViewport()->GetD3D9Viewport();
      }  else {
        d3d9Device->GetViewport(&currentViewport9);
      }
      d3d9Device->SetViewport(m_commonViewport->GetD3D9Viewport());
    }

    HRESULT hr9 = d3d9Device->Clear(count, rects, flags, color, z, stencil);

    // Restore the previously active viewport
    if (!m_commonViewport->IsCurrentViewport()) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    if (unlikely(FAILED(hr9))) {
      Logger::err("D3D6Viewport::Clear2: Failed D3D9 Clear call");
      // Unlike Clear(), Clear2() is supposed to error out on
      // z/stencil clears and no z/stencil attachments
      return hr;
    }

    return D3D_OK;
  }

  HRESULT D3D6Viewport::ApplyViewport() {
    if (!m_commonViewport->IsViewportSet())
      return D3D_OK;

    Logger::debug("D3D6Viewport: Applying viewport to D3D9");

    HRESULT hr = m_commonViewport->GetD3D9Device()->SetViewport(m_commonViewport->GetD3D9Viewport());
    if(unlikely(FAILED(hr)))
      Logger::err("D3D6Viewport: Failed to set the D3D9 viewport");

    return hr;
  }

  HRESULT D3D6Viewport::ApplyAndActivateLights() {
    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    if (!lights.size())
      return D3D_OK;

    Logger::debug("D3D6Viewport: Applying lights to D3D9");

    for (auto light: lights)
      ApplyAndActivateLight(light->GetIndex(), light.ptr());

    return D3D_OK;
  }

  HRESULT D3D6Viewport::ApplyAndActivateLight(DWORD index, D3DLight* light) {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetD3D9Device();

    HRESULT hr = d3d9Device->SetLight(index, light->GetD3D9Light());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Viewport: Failed D3D9 SetLight call");
    } else {
      HRESULT hrLE;
      if (light->IsActive()) {
        Logger::debug(str::format("D3D6Viewport: Enabling light nr. ", index));
        hrLE = d3d9Device->LightEnable(index, TRUE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D6Viewport: Failed D3D9 LightEnable call (TRUE)");
      } else {
        Logger::debug(str::format("D3D6Viewport: Disabling light nr. ", index));
        hrLE = d3d9Device->LightEnable(index, FALSE);
        if (unlikely(FAILED(hrLE)))
          Logger::err("D3D6Viewport: Failed D3D9 LightEnable call (FALSE)");
      }
    }

    return hr;
  }

}

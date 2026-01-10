#include "d3d3_viewport.h"

namespace dxvk {

  uint32_t D3D3Viewport::s_viewportCount = 0;

  D3D3Viewport::D3D3Viewport(Com<IDirect3DViewport>&& proxyViewport, D3D3Interface* pParent, IUnknown* origin)
    : DDrawWrappedObject<D3D3Interface, IDirect3DViewport, IUnknown>(pParent, std::move(proxyViewport), nullptr)
    , m_origin ( origin ) {
    if (m_origin != nullptr)
      m_origin->AddRef();

    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D3Viewport: Created a new viewport nr. [[1-", m_viewportCount, "]]"));
  }

  D3D3Viewport::~D3D3Viewport() {
    if (m_origin != nullptr)
      m_origin->Release();

    Logger::debug(str::format("D3D3Viewport: Viewport nr. [[1-", m_viewportCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<D3D3Interface, IDirect3DViewport, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DViewport)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D3Viewport::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D3Viewport::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D3Viewport::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DViewport2))) {
      Logger::warn("D3D3Viewport::QueryInterface: Query for IDirect3DViewport2");
      m_proxy->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DViewport3))) {
      Logger::warn("D3D3Viewport::QueryInterface: Query for IDirect3DViewport3");
      m_proxy->QueryInterface(riid, ppvObject);
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
    Logger::debug("<<< D3D3Viewport::GetViewport: Proxy");
    return m_proxy->GetViewport(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetViewport(D3DVIEWPORT *data) {
    Logger::debug("<<< D3D3Viewport::SetViewport: Proxy");
    return m_proxy->SetViewport(data);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    Logger::debug("<<< D3D3Viewport::TransformVertices: Proxy");
    return m_proxy->TransformVertices(vertex_count, data, flags, offscreen);
  }

  // Docs state: "The IDirect3DViewport::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    Logger::warn(">>> D3D3Viewport::LightElements");
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    Logger::debug("<<< D3D3Viewport::SetBackground: Proxy");
    return m_proxy->SetBackground(hMat);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    Logger::debug("<<< D3D3Viewport::GetBackground: Proxy");
    return m_proxy->GetBackground(material, valid);
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
    return m_proxy->Clear(count, rects, flags);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::AddLight(IDirect3DLight *light) {
    Logger::debug("<<< D3D3Viewport::AddLight: Proxy");
    return m_proxy->AddLight(light);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::DeleteLight(IDirect3DLight *light) {
    Logger::debug("<<< D3D3Viewport::DeleteLight: Proxy");
    return m_proxy->DeleteLight(light);
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::NextLight(IDirect3DLight *ref, IDirect3DLight **light, DWORD flags) {
    Logger::warn("!!! D3D3Viewport::NextLight: Stub");
    return D3D_OK;
  }

}

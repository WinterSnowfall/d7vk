#include "d3d6_interface.h"

#include "../ddraw_interface.h"

namespace dxvk {

  uint32_t D3D6Interface::s_intfCount = 0;

  D3D6Interface::D3D6Interface(Com<IDirect3D3>&& d3d6IntfProxy, DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirect3D3, d3d9::IDirect3D9>(pParent, std::move(d3d6IntfProxy), std::move(d3d9::Direct3DCreate9(D3D_SDK_VERSION))) {
    // Get the bridge interface to D3D9.
    if (unlikely(FAILED(m_d3d9->QueryInterface(__uuidof(IDxvkD3D8InterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    // TODO: Have a D3D6 equivalent
    m_bridge->EnableD3D6CompatibilityMode();

    m_d3d7Options = D3D7Options(*m_bridge->GetConfig());

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D6Interface: Created a new interface nr. ((3-", m_intfCount, "))"));
  }

  D3D6Interface::~D3D6Interface() {
    Logger::debug(str::format("D3D6Interface: Interface nr. ((3-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D6Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D6Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirect3D3, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D3)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D6Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(m_d3d7Options.legacyQueryInterface)) {
      // Some games query for legacy d3d interfaces
      if (unlikely(riid == __uuidof(IDirect3D)
                || riid == __uuidof(IDirect3D2))) {
        Logger::warn("D3D6Interface::QueryInterface: Query for legacy IDirect3D");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
      // Some games query for legacy ddraw interfaces
      if (unlikely(riid == __uuidof(IDirectDraw)
                || riid == __uuidof(IDirectDraw2)
                || riid == __uuidof(IDirectDraw4))) {
        Logger::warn("D3D6Interface::QueryInterface: Query for legacy IDirectDraw");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
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

  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug("<<< D3D6Interface::EnumDevices: Proxy");
    return m_proxy->EnumDevices(lpEnumDevicesCallback, lpUserArg);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D6Interface::CreateLight: Proxy");
    return m_proxy->CreateLight(lplpDirect3DLight, pUnkOuter);
  }
    
  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D6Interface::CreateMaterial: Proxy");
    return m_proxy->CreateMaterial(lplpDirect3DMaterial, pUnkOuter);
  }
    
  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D6Interface::CreateViewport: Proxy");
    return m_proxy->CreateViewport(lplpD3DViewport, pUnkOuter);
  }
    
  HRESULT STDMETHODCALLTYPE D3D6Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug("<<< D3D6Interface::FindDevice: Proxy");
    return m_proxy->FindDevice(lpD3DFDS, lpD3DFDR);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter) {
    Logger::warn("<<< D3D6Interface::CreateDevice: Proxy");
    return m_proxy->CreateDevice(rclsid, lpDDS, lplpD3DDevice, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateVertexBuffer(LPD3DVERTEXBUFFERDESC lpVBDesc, LPDIRECT3DVERTEXBUFFER *lpD3DVertexBuffer, DWORD dwFlags, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D6Interface::CreateVertexBuffer: Proxy");
    return m_proxy->CreateVertexBuffer(lpVBDesc, lpD3DVertexBuffer, dwFlags, pUnkOuter);
  }

  // Total Club Manager 2003 uses a D3D6 interface to query for supported Z buffer formats,
  // so report what we know is supported by D3D9, otherwise the game will error out on startup
  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext) {
    Logger::debug(">>> D3D6Interface::EnumZBufferFormats");

    if (unlikely(lpEnumCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    // There are just 3 supported depth stencil formats to worry about
    // in D3D9, so let's just enumerate them liniarly, for better clarity

    DDPIXELFORMAT depthFormat = GetZBufferFormat(d3d9::D3DFMT_D16);
    HRESULT hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24X8);
    hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24S8);
    hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr == D3DENUMRET_CANCEL))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::EvictManagedTextures() {
    Logger::debug("<<< D3D6Interface::EvictManagedTextures:  Proxy");
    return m_proxy->EvictManagedTextures();
  }

}
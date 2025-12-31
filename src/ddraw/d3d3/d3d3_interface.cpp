#include "d3d3_interface.h"

#include "../ddraw_interface.h"

namespace dxvk {

  uint32_t D3D3Interface::s_intfCount = 0;

  D3D3Interface::D3D3Interface(Com<IDirect3D>&& d3d3IntfProxy, DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>(pParent, std::move(d3d3IntfProxy), nullptr) {

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D3Interface: Created a new interface nr. ((1-", m_intfCount, "))"));
  }

  D3D3Interface::~D3D3Interface() {
    Logger::debug(str::format("D3D3Interface: Interface nr. ((1-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D3Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D3Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::debug("D3D3Interface::QueryInterface: Query for IDirectDraw");
      return m_parent->QueryInterface(riid, ppvObject);
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

  HRESULT STDMETHODCALLTYPE D3D3Interface::Initialize(REFIID riid) {
    Logger::debug("<<< D3D3Interface::Initialize: Proxy");
    return m_proxy->Initialize(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug("<<< D3D3Interface::EnumDevices: Proxy");
    return m_proxy->EnumDevices(lpEnumDevicesCallback, lpUserArg);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D3Interface::CreateLight: Proxy");
    return m_proxy->CreateLight(lplpDirect3DLight, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateMaterial(LPDIRECT3DMATERIAL *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D3Interface::CreateMaterial: Proxy");
    return m_proxy->CreateMaterial(lplpDirect3DMaterial, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateViewport(LPDIRECT3DVIEWPORT *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D3Interface::CreateViewport: Proxy");
    return m_proxy->CreateViewport(lplpD3DViewport, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug("<<< D3D3Interface::FindDevice: Proxy");
    return m_proxy->FindDevice(lpD3DFDS, lpD3DFDR);
  }

}
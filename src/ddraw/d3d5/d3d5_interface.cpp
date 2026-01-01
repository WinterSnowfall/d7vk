#include "d3d5_interface.h"

#include "../ddraw_surface.h"

#include "../ddraw2/ddraw2_interface.h"

namespace dxvk {

  uint32_t D3D5Interface::s_intfCount = 0;

  D3D5Interface::D3D5Interface(Com<IDirect3D2>&& d3d5IntfProxy, DDraw2Interface* pParent)
    : DDrawWrappedObject<DDraw2Interface, IDirect3D2, d3d9::IDirect3D9>(pParent, std::move(d3d5IntfProxy), nullptr) {

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D5Interface: Created a new interface nr. ((2-", m_intfCount, "))"));
  }

  D3D5Interface::~D3D5Interface() {
    Logger::debug(str::format("D3D5Interface: Interface nr. ((2-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw2Interface, IDirect3D2, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Interface::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("D3D5Interface::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Some games query for legacy d3d interfaces
    if (unlikely(riid == __uuidof(IDirect3D))) {
      Logger::warn("D3D5Interface::QueryInterface: Query for legacy IDirect3D");
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Some games query for ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      Logger::debug("D3D5Interface::QueryInterface: Query for IDirectDraw");
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

  HRESULT STDMETHODCALLTYPE D3D5Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    Logger::debug("<<< D3D5Interface::EnumDevices: Proxy");
    return m_proxy->EnumDevices(lpEnumDevicesCallback, lpUserArg);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D5Interface::CreateLight: Proxy");
    return m_proxy->CreateLight(lplpDirect3DLight, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateMaterial(LPDIRECT3DMATERIAL2 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D5Interface::CreateMaterial: Proxy");
    return m_proxy->CreateMaterial(lplpDirect3DMaterial, pUnkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateViewport(LPDIRECT3DVIEWPORT2 *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug("<<< D3D5Interface::CreateViewport: Proxy");
    return m_proxy->CreateViewport(lplpD3DViewport, pUnkOuter);
  }

  // Minimal implementation which should suffice in most cases
  HRESULT STDMETHODCALLTYPE D3D5Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug("<<< D3D5Interface::FindDevice: Proxy");
    return m_proxy->FindDevice(lpD3DFDS, lpD3DFDR);
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE lpDDS, LPDIRECT3DDEVICE2 *lplpD3DDevice) {
    Logger::debug("<<< D3D5Interface::CreateDevice: Proxy");

    Logger::warn("D3D5Interface::CreateDevice: Use of unsupported D3D5 device");

    if (likely(m_parent->IsWrappedSurface(lpDDS))) {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDS);
      return m_proxy->CreateDevice(rclsid, ddrawSurface->GetProxied(), lplpD3DDevice);
    } else {
      return m_proxy->CreateDevice(rclsid, lpDDS, lplpD3DDevice);
    }
  }

}
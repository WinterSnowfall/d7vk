#include "d3d3_interface.h"

#include "d3d3_light.h"
#include "d3d3_material.h"
#include "d3d3_viewport.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  uint32_t D3D3Interface::s_intfCount = 0;

  D3D3Interface::D3D3Interface(Com<IDirect3D>&& d3d3IntfProxy, DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>(pParent, std::move(d3d3IntfProxy), nullptr) {
    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D3Interface: Created a new interface nr. ((1-", m_intfCount, "))"));
  }

  D3D3Interface::~D3D3Interface() {
    if (m_parent->GetCommonInterface()->GetD3D3Interface() == this)
      m_parent->GetCommonInterface()->SetD3D3Interface(nullptr);

    Logger::debug(str::format("D3D3Interface: Interface nr. ((1-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::AddRef() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->AddRef();
      else
        return m_parent->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::Release() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_parent->GetCommonInterface()->GetOrigin();
      if (likely(origin != nullptr))
        return origin->Release();
      else
        return m_parent->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirect3D, d3d9::IDirect3D9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3D))
      return this;

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
    // Deathtrap Dungeon queries for IDirect3D2... not sure if this ever worked
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      Logger::warn("D3D3Interface::QueryInterface: Query for IDirect3D2");
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
    Logger::debug(">>> D3D3Interface::CreateLight");

    Com<IDirect3DLight> lplpDirect3DLightProxy;
    HRESULT hr = m_proxy->CreateLight(&lplpDirect3DLightProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpDirect3DLight);

    *lplpDirect3DLight = ref(new D3D3Light(std::move(lplpDirect3DLightProxy), this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateMaterial(LPDIRECT3DMATERIAL *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D3Interface::CreateMaterial");

    Com<IDirect3DMaterial> lplpDirect3DMaterialProxy;
    HRESULT hr = m_proxy->CreateMaterial(&lplpDirect3DMaterialProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpDirect3DMaterial);

    *lplpDirect3DMaterial = ref(new D3D3Material(std::move(lplpDirect3DMaterialProxy), this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateViewport(LPDIRECT3DVIEWPORT *lplpD3DViewport, IUnknown *pUnkOuter) {
    Logger::debug(">>> D3D3Interface::CreateViewport");

    Com<IDirect3DViewport> lplpD3DViewportProxy;
    HRESULT hr = m_proxy->CreateViewport(&lplpD3DViewportProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D3Viewport(nullptr, std::move(lplpD3DViewportProxy), this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    Logger::debug("<<< D3D3Interface::FindDevice: Proxy");
    return m_proxy->FindDevice(lpD3DFDS, lpD3DFDR);
  }

}
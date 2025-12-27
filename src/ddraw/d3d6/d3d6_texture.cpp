#include "d3d6_texture.h"

namespace dxvk {

  uint32_t D3D6Texture::s_texCount = 0;

  D3D6Texture::D3D6Texture(Com<IDirect3DTexture2>&& proxyTexture, DDraw4Surface* pParent)
    : DDrawWrappedObject<DDraw4Surface, IDirect3DTexture2, IUnknown>(pParent, std::move(proxyTexture), nullptr) {
    m_texCount = ++s_texCount;

    Logger::debug(str::format("D3D6Texture: Created a new texture nr. [[2-", m_texCount, "]]"));
  }

  D3D6Texture::~D3D6Texture() {
    Logger::debug(str::format("D3D6Texture: Texture nr. [[2-", m_texCount, "]] bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDrawSurface4
  ULONG STDMETHODCALLTYPE D3D6Texture::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDrawSurface4
  ULONG STDMETHODCALLTYPE D3D6Texture::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw4Surface, IDirect3DTexture2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DTexture2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D6Texture::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D6Texture::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D6Texture::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D6Texture::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirect3DTexture");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl)
              || riid == __uuidof(IDirectDrawColorControl))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirectDrawSurface3");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirectDrawSurface4");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug("D3D6Texture::QueryInterface: Query for IDirectDrawSurface7");
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

  HRESULT STDMETHODCALLTYPE D3D6Texture::GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle) {
    Logger::warn("<<< D3D6Texture::GetHandle: Proxy");
    return m_proxy->GetHandle(lpDirect3DDevice2, lpHandle);
  }

  HRESULT STDMETHODCALLTYPE D3D6Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    Logger::warn("<<< D3D6Texture::PaletteChanged: Proxy");
    return m_proxy->PaletteChanged(dwStart, dwCount);
  }

  HRESULT STDMETHODCALLTYPE D3D6Texture::Load(LPDIRECT3DTEXTURE2 lpD3DTexture2) {
    Logger::debug("<<< D3D6Texture::Load: Proxy");

    Com<D3D6Texture> d3d6Texture = static_cast<D3D6Texture*>(lpD3DTexture2);

    HRESULT hr = m_proxy->Load(d3d6Texture->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    d3d6Texture->GetParent()->DirtyMipMaps();

    return hr;
  }

}

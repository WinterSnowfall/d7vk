#include "d3d5_texture.h"

namespace dxvk {

  uint32_t D3D5Texture::s_texCount = 0;

  D3D5Texture::D3D5Texture(Com<IDirect3DTexture2>&& proxyTexture, DDrawSurface* pParent, D3DTEXTUREHANDLE handle)
    : DDrawWrappedObject<DDrawSurface, IDirect3DTexture2, IUnknown>(pParent, std::move(proxyTexture), nullptr)
    , m_textureHandle ( handle ) {
    m_texCount = ++s_texCount;

    Logger::debug(str::format("D3D5Texture: Created a new texture nr. [[2-", m_texCount, "]]"));
  }

  D3D5Texture::~D3D5Texture() {
    Logger::debug(str::format("D3D5Texture: Texture nr. [[2-", m_texCount, "]] bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDrawSurface
  ULONG STDMETHODCALLTYPE D3D5Texture::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDrawSurface
  ULONG STDMETHODCALLTYPE D3D5Texture::Release() {
    return m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirect3DTexture2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DTexture2)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D5Texture::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D5Texture::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Texture::QueryInterface");

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

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle) {
    if (unlikely(m_parent->GetOptions()->proxySetTexture)) {
      Logger::debug("<<< D3D5Texture::GetHandle: Proxy");
      return m_proxy->GetHandle(lpDirect3DDevice2, lpHandle);
    }

    Logger::debug(">>> D3D5Texture::GetHandle");

    if(unlikely(lpDirect3DDevice2 == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpHandle = m_textureHandle;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    Logger::warn("<<< D3D5Texture::PaletteChanged: Proxy");
    return m_proxy->PaletteChanged(dwStart, dwCount);
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::Load(LPDIRECT3DTEXTURE2 lpD3DTexture2) {
    Logger::debug("<<< D3D5Texture::Load: Proxy");

    Com<D3D5Texture> d3d5Texture = static_cast<D3D5Texture*>(lpD3DTexture2);

    HRESULT hr = m_proxy->Load(d3d5Texture->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    d3d5Texture->GetParent()->GetCommonSurface()->DirtyMipMaps();

    return hr;
  }

}

#include "d3d3_texture.h"

#include "../ddraw/ddraw_surface.h"

#include "d3d3_device.h"

namespace dxvk {

  uint32_t D3D3Texture::s_texCount = 0;

  D3D3Texture::D3D3Texture(Com<IDirect3DTexture>&& proxyTexture, DDrawSurface* pParent)
    : DDrawWrappedObject<DDrawSurface, IDirect3DTexture, IUnknown>(pParent, std::move(proxyTexture), nullptr) {
    m_parent->AddRef();

    m_texCount = ++s_texCount;

    Logger::debug(str::format("D3D3Texture: Created a new texture nr. [[1-", m_texCount, "]]"));
  }

  D3D3Texture::~D3D3Texture() {
    m_parent->Release();

    Logger::debug(str::format("D3D3Texture: Texture nr. [[1-", m_texCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirect3DTexture, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DTexture)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("D3D3Texture::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("D3D3Texture::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D3Texture::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirect3DTexture");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl)
              || riid == __uuidof(IDirectDrawColorControl))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawSurface3");
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

  HRESULT STDMETHODCALLTYPE D3D3Texture::GetHandle(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DTEXTUREHANDLE lpHandle) {
    Logger::debug("<<< D3D3Texture::GetHandle: Proxy");

    if(unlikely(lpDirect3DDevice == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Device* d3d3Device = static_cast<D3D3Device*>(lpDirect3DDevice);
    return m_proxy->GetHandle(d3d3Device->GetProxied(), lpHandle);
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    Logger::warn("<<< D3D3Texture::PaletteChanged: Proxy");
    return m_proxy->PaletteChanged(dwStart, dwCount);
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::Load(LPDIRECT3DTEXTURE lpD3DTexture) {
    Logger::debug("<<< D3D3Texture::Load: Proxy");

    Com<D3D3Texture> d3d3Texture = static_cast<D3D3Texture*>(lpD3DTexture);
    return m_proxy->Load(d3d3Texture->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPDIRECTDRAWSURFACE lpDDSurface) {
    Logger::debug("<<< D3D3Texture::Initialize: Proxy");

    if(unlikely(lpDirect3DDevice == nullptr || lpDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Device* d3d3Device = static_cast<D3D3Device*>(lpDirect3DDevice);
    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSurface);
    return m_proxy->Initialize(d3d3Device->GetProxied(), ddrawSurface->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::Unload() {
    Logger::debug("<<< D3D3Texture::Unload: Proxy");
    return m_proxy->Unload();
  }

}

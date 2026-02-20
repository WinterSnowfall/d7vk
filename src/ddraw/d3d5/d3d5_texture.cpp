#include "d3d5_texture.h"

namespace dxvk {

  uint32_t D3D5Texture::s_texCount = 0;

  D3D5Texture::D3D5Texture(Com<IDirect3DTexture2>&& proxyTexture, DDrawSurface* pParent, D3DTEXTUREHANDLE handle)
    : DDrawWrappedObject<DDrawSurface, IDirect3DTexture2, IUnknown>(pParent, std::move(proxyTexture), nullptr) {
    m_parent->AddRef();

    m_commonTex = new D3DCommonTexture(m_parent->GetCommonSurface(), handle);

    m_texCount = ++s_texCount;

    Logger::debug(str::format("D3D5Texture: Created a new texture nr. [[2-", m_texCount, "]]"));
  }

  D3D5Texture::~D3D5Texture() {
    m_parent->Release();

    Logger::debug(str::format("D3D5Texture: Texture nr. [[2-", m_texCount, "]] bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirect3DTexture2, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DTexture2))
      return this;

    throw DxvkError("D3D5Texture::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> D3D5Texture::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug("D3D5Texture::QueryInterface: Query for IDirect3DTexture");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl)
              || riid == __uuidof(IDirectDrawColorControl))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug("D3D5Texture::QueryInterface: Query for IDirectDrawSurface");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug("D3D5Texture::QueryInterface: Query for IDirectDrawSurface2");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug("D3D5Texture::QueryInterface: Query for IDirectDrawSurface3");
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
    if (unlikely(m_parent->GetOptions()->proxiedExecuteBuffers)) {
      Logger::debug("<<< D3D5Texture::GetHandle: Proxy");
      return m_proxy->GetHandle(lpDirect3DDevice2, lpHandle);
    }

    Logger::debug(">>> D3D5Texture::GetHandle");

    if(unlikely(lpDirect3DDevice2 == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DTEXTUREHANDLE texHandle = m_commonTex->GetTextureHandle();

    if (unlikely(!texHandle)) {
      Logger::warn("D3D5Texture::GetHandle: Null handle returned");
      return m_proxy->GetHandle(lpDirect3DDevice2, lpHandle);
    }

    *lpHandle = texHandle;

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

    if (likely(!m_parent->GetOptions()->apitraceMode)) {
      m_parent->GetCommonSurface()->DirtyMipMaps();
    } else {
      m_parent->InitializeOrUploadD3D9();
    }

    return hr;
  }

}

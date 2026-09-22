#include "d3d3_texture.h"

#include "../ddraw_common_surface.h"

#include "../ddraw/ddraw_surface.h"

namespace dxvk {

  D3D3Texture::D3D3Texture(
        D3DCommonTexture* commonTex,
        DDrawCommonSurface* commonSurf,
        Com<IDirect3DTexture>&& proxyTexture,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirect3DTexture>(pParent, std::move(proxyTexture))
    , m_commonTex ( commonTex ) {
    if (m_commonTex == nullptr)
      m_commonTex = new D3DCommonTexture(commonSurf);

    m_commonTex->SetD3D3Texture(this);
  }

  D3D3Texture::~D3D3Texture() {
    m_commonTex->SetD3D3Texture(nullptr);
  }

  // Interlocked refcount with the parent IDirectDrawSurface
  ULONG STDMETHODCALLTYPE D3D3Texture::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDrawSurface
  ULONG STDMETHODCALLTYPE D3D3Texture::Release() {
    return m_parent->Release();
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IDirect3DTexture))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Texture::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::GetHandle(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DTEXTUREHANDLE lpHandle) {
    if (unlikely(lpDirect3DDevice == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonTex->GetHandleCommon(lpHandle);
  }

  // Docs state: "This method only affects the legacy ramp device.
  // For all other devices, this method takes no action and returns D3D_OK."
  HRESULT STDMETHODCALLTYPE D3D3Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::Load(LPDIRECT3DTEXTURE lpD3DTexture) {
    Com<D3D3Texture> d3d3Texture = static_cast<D3D3Texture*>(lpD3DTexture);

    // Fast skip
    if (unlikely(d3d3Texture == this))
      return D3D_OK;

    // Note: Will not work if IDirect3DTexture is queried directly
    // from IDirectDrawSurface4, though that shouldn't happen in practice
    DDrawSurface* parentSurf = d3d3Texture->GetCommonTexture()->GetDDSurface();
    if (likely(parentSurf != nullptr)) {
      parentSurf->DownloadSurfaceData();
    } else {
      Logger::warn("D3D3Texture::Load: Failed to download parent surface");
    }

    HRESULT hr = m_proxy->Load(d3d3Texture->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* commonSurf = m_commonTex->GetCommonSurface();

    hr = commonSurf->RefreshSurfaceDescripton(true);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D3Texture::Load: Failed to refresh surface description");
      return hr;
    }

    commonSurf->DirtyDDrawSurface();

    return D3D_OK;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the Direct3DTexture object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Texture::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPDIRECTDRAWSURFACE lpDDSurface) {
    return DDERR_ALREADYINITIALIZED;
  }

  // Nothing to do here, this isn't managed texture unloading
  HRESULT STDMETHODCALLTYPE D3D3Texture::Unload() {
    return D3D_OK;
  }

}

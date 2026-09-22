#include "d3d5_texture.h"

#include "../ddraw_common_surface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw4/ddraw4_surface.h"

namespace dxvk {

  D3D5Texture::D3D5Texture(
        D3DCommonTexture* commonTex,
        DDrawCommonSurface* commonSurf,
        Com<IDirect3DTexture2>&& proxyTexture,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirect3DTexture2>(pParent, std::move(proxyTexture))
    , m_commonTex  ( commonTex ) {
    if (m_commonTex == nullptr)
      m_commonTex = new D3DCommonTexture(commonSurf);

    m_commonTex->SetD3D5Texture(this);
  }

  D3D5Texture::~D3D5Texture() {
    m_commonTex->SetD3D5Texture(nullptr);
  }

  // Interlocked refcount with the parent IDirectDrawSurface/IDirectDrawSurface4
  ULONG STDMETHODCALLTYPE D3D5Texture::AddRef() {
    return m_parent->AddRef();
  }

  // Interlocked refcount with the parent IDirectDrawSurface/IDirectDrawSurface4
  ULONG STDMETHODCALLTYPE D3D5Texture::Release() {
    return m_parent->Release();
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
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

    if (likely(riid == __uuidof(IDirect3DTexture2))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D5Texture::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle) {
    if (unlikely(lpDirect3DDevice2 == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonTex->GetHandleCommon(lpHandle);
  }

  // Docs state: "This method only affects the legacy ramp device.
  // For all other devices, this method takes no action and returns D3D_OK."
  HRESULT STDMETHODCALLTYPE D3D5Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::Load(LPDIRECT3DTEXTURE2 lpD3DTexture2) {
    Com<D3D5Texture> d3d5Texture = static_cast<D3D5Texture*>(lpD3DTexture2);

    // Fast skip
    if (unlikely(d3d5Texture == this))
      return D3D_OK;

    // IDirect3DTexture2 is guaranteed to have a IDirectDrawSurface4 parent,
    // because we create and cache one ourselves on creation if it doesn't exist
    DDraw4Surface* parentSurf4 = d3d5Texture->GetCommonTexture()->GetDD4Surface();
    if (likely(parentSurf4 != nullptr)) {
      parentSurf4->DownloadSurfaceData();
    } else {
      Logger::warn("D3D5Texture::Load: Failed to download parent surface");
    }

    HRESULT hr = m_proxy->Load(d3d5Texture->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* commonSurf = m_commonTex->GetCommonSurface();

    hr = commonSurf->RefreshSurfaceDescripton(true);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D5Texture::Load: Failed to refresh surface description");
      return hr;
    }

    commonSurf->DirtyDDrawSurface();

    return D3D_OK;
  }

}

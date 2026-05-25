#include "d3d3_texture.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "../ddraw/ddraw_surface.h"

namespace dxvk {

  uint32_t D3D3Texture::s_texCount = 0;

  D3D3Texture::D3D3Texture(
        DDrawCommonSurface* commonSurf,
        Com<IDirect3DTexture>&& proxyTexture,
        IUnknown* pParent,
        D3DTEXTUREHANDLE handle)
    : DDrawWrappedObject<IUnknown, IDirect3DTexture>(pParent, std::move(proxyTexture))
    , m_commonIntf ( commonSurf->GetCommonInterface() ) {
    m_commonTex = new D3DCommonTexture(commonSurf, handle);

    m_texCount = ++s_texCount;

    Logger::debug(str::format("D3D3Texture: Created a new texture nr. [[1-", m_texCount, "]]"));
  }

  D3D3Texture::~D3D3Texture() {
    m_commonIntf->ReleaseTextureHandle(m_commonTex->GetTextureHandle());

    Logger::debug(str::format("D3D3Texture: Texture nr. [[1-", m_texCount, "]] bites the dust"));
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
    Logger::debug(">>> D3D3Texture::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirect3DTexture");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawGammaControl");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawColorControl");
      return E_NOINTERFACE;
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
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawSurface4");
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug("D3D3Texture::QueryInterface: Query for IDirectDrawSurface7");
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

  // Frogger gets texture handles from IDirect3DTexture objects and uses them in calls on a IDirect3DDevice2
  HRESULT STDMETHODCALLTYPE D3D3Texture::GetHandle(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DTEXTUREHANDLE lpHandle) {
    Logger::debug(">>> D3D3Texture::GetHandle");

    if(unlikely(lpDirect3DDevice == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpHandle = m_commonTex->GetTextureHandle();

    return D3D_OK;
  }

  // Docs state: "This method only affects the legacy ramp device.
  // For all other devices, this method takes no action and returns D3D_OK."
  HRESULT STDMETHODCALLTYPE D3D3Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    Logger::debug(">>> D3D3Texture::PaletteChanged");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Texture::Load(LPDIRECT3DTEXTURE lpD3DTexture) {
    Logger::debug("<<< D3D3Texture::Load: Proxy");

    Com<D3D3Texture> d3d3Texture = static_cast<D3D3Texture*>(lpD3DTexture);

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
    HRESULT hrDesc = commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hrDesc)))
      Logger::err("D3D3Texture::Load: Failed to refresh surface description");
    commonSurf->DirtyDDrawSurface();

    return hr;
  }

  // Docs state: "Returns DDERR_ALREADYINITIALIZED because the Direct3DTexture object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Texture::Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPDIRECTDRAWSURFACE lpDDSurface) {
    Logger::debug(">>> D3D3Texture::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  // Nothing to do here, this isn't managed texture unloading
  HRESULT STDMETHODCALLTYPE D3D3Texture::Unload() {
    Logger::debug(">>> D3D3Texture::Unload");
    return D3D_OK;
  }

}

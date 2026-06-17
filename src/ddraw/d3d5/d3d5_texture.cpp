#include "d3d5_texture.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw4/ddraw4_surface.h"

namespace dxvk {

  uint32_t D3D5Texture::s_texCount = 0;

  D3D5Texture::D3D5Texture(
        DDrawCommonSurface* commonSurf,
        Com<IDirect3DTexture2>&& proxyTexture,
        IUnknown* pParent,
        D3DTEXTUREHANDLE handle,
        bool isD3D6Texture)
    : DDrawWrappedObject<IUnknown, IDirect3DTexture2>(pParent, std::move(proxyTexture))
    , m_commonIntf ( commonSurf->GetCommonInterface() ) {
    m_commonTex = new D3DCommonTexture(commonSurf, handle);

    // D3D5Texture is shared between D3D5/6, however textures used in a D3D6 context
    // typically come from an IDirectDrawSurface4 parent. This isn't a hard requirement,
    // but is true in the vast majority of cases, so use the distinction for logging purposes.
    if (isD3D6Texture)
      m_objectType = "D3D6Texture";

    m_texCount = ++s_texCount;

    Logger::debug(str::format(m_objectType, ": Created a new texture nr. [[2-", m_texCount, "]]"));
  }

  D3D5Texture::~D3D5Texture() {
    Logger::debug(str::format(m_objectType, ": Texture nr. [[2-", m_texCount, "]] bites the dust"));
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
    Logger::debug(str::format(">>> ", m_objectType, "::QueryInterface"));

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirect3DTexture"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawGammaControl))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawGammaControl"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawColorControl"));
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawSurface"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawSurface2"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawSurface3"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawSurface4"));
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      Logger::debug(str::format(m_objectType, "::QueryInterface: Query for IDirectDrawSurface7"));
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IDirect3DTexture2))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn(str::format(m_objectType, "::QueryInterface: Unknown interface query"));
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle) {
    Logger::debug(str::format(">>> ", m_objectType, "::GetHandle"));

    if(unlikely(lpDirect3DDevice2 == nullptr || lpHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpHandle = m_commonTex->GetTextureHandle();

    return D3D_OK;
  }

  // Docs state: "This method only affects the legacy ramp device.
  // For all other devices, this method takes no action and returns D3D_OK."
  HRESULT STDMETHODCALLTYPE D3D5Texture::PaletteChanged(DWORD dwStart, DWORD dwCount) {
    Logger::debug(str::format(">>> ", m_objectType, "::PaletteChanged"));
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Texture::Load(LPDIRECT3DTEXTURE2 lpD3DTexture2) {
    Logger::debug(str::format("<<< ", m_objectType, "::Load: Proxy"));

    Com<D3D5Texture> d3d5Texture = static_cast<D3D5Texture*>(lpD3DTexture2);

    //IDirect3DTexture2 can have either a IDirectDrawSurface4 or a IDirectDrawSurface parent
    DDraw4Surface* parentSurf4 = d3d5Texture->GetCommonTexture()->GetDD4Surface();
    if (parentSurf4 != nullptr) {
      parentSurf4->DownloadSurfaceData();
    } else {
      DDrawSurface* parentSurf = d3d5Texture->GetCommonTexture()->GetDDSurface();
      if (likely(parentSurf != nullptr)) {
        parentSurf->DownloadSurfaceData();
      } else {
        Logger::warn(str::format(m_objectType, "::Load: Failed to download parent surface"));
      }
    }

    HRESULT hr = m_proxy->Load(d3d5Texture->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* commonSurf = m_commonTex->GetCommonSurface();

    HRESULT hrDesc = commonSurf->RefreshSurfaceDescripton();
    if (unlikely(FAILED(hrDesc)))
      Logger::err(str::format(m_objectType, "::Load: Failed to refresh surface description"));
    commonSurf->DirtyDDrawSurface();

    return hr;
  }

}

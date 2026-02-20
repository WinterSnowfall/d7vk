#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_texture.h"

#include "../ddraw/ddraw_surface.h"

namespace dxvk {

  class D3D5Texture final : public DDrawWrappedObject<DDrawSurface, IDirect3DTexture2, IUnknown> {

  public:

    D3D5Texture(Com<IDirect3DTexture2>&& proxyTexture, DDrawSurface* pParent, D3DTEXTUREHANDLE handle);

    ~D3D5Texture();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle);

    HRESULT STDMETHODCALLTYPE PaletteChanged(DWORD dwStart, DWORD dwCount);

    HRESULT STDMETHODCALLTYPE Load(LPDIRECT3DTEXTURE2 lpD3DTexture2);

    D3DCommonTexture* GetCommonTexture() const {
      return m_commonTex.ptr();
    }

  private:

    static uint32_t       s_texCount;
    uint32_t              m_texCount = 0;

    Com<D3DCommonTexture> m_commonTex;

  };

}

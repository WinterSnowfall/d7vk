#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class DDrawSurface;

  class D3D3Texture final : public DDrawWrappedObject<DDrawSurface, IDirect3DTexture, IUnknown> {

  public:

    D3D3Texture(Com<IDirect3DTexture>&& proxyTexture, DDrawSurface* pParent);

    ~D3D3Texture();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetHandle(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DTEXTUREHANDLE lpHandle);

    HRESULT STDMETHODCALLTYPE PaletteChanged(DWORD dwStart, DWORD dwCount);

    HRESULT STDMETHODCALLTYPE Load(LPDIRECT3DTEXTURE lpD3DTexture);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPDIRECTDRAWSURFACE lpDDSurface);

    HRESULT STDMETHODCALLTYPE Unload();

  private:

    static uint32_t   s_texCount;
    uint32_t          m_texCount = 0;

  };

}

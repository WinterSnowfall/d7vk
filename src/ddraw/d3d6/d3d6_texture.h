#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "ddraw4/ddraw4_surface.h"

namespace dxvk {

  class D3D6Texture final : public DDrawWrappedObject<DDraw4Surface, IDirect3DTexture2, IUnknown> {

  public:

    D3D6Texture(Com<IDirect3DTexture2>&& proxyTexture, DDraw4Surface* pParent);

    ~D3D6Texture();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle);

    HRESULT STDMETHODCALLTYPE PaletteChanged(DWORD dwStart, DWORD dwCount);

    HRESULT STDMETHODCALLTYPE Load(LPDIRECT3DTEXTURE2 lpD3DTexture2);

  private:

    static uint32_t  s_texCount;
    uint32_t         m_texCount = 0;

  };

}

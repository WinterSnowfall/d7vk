#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"

namespace dxvk {

  class DDrawSurface;

  /**
  * \brief Minimal D3D3 device implementation
  */
  class D3D3Device final : public DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9> {

  public:
    D3D3Device(Com<IDirect3DDevice>&& d3d3DeviceProxy, DDrawSurface* pParent);

    ~D3D3Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport *ref, IDirect3DViewport **viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D **d3d);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc);

    HRESULT STDMETHODCALLTYPE CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter);

    HRESULT STDMETHODCALLTYPE Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect);

    HRESULT STDMETHODCALLTYPE GetPickRecords(DWORD *count, D3DPICKRECORD *records);

    HRESULT STDMETHODCALLTYPE CreateMatrix(D3DMATRIXHANDLE *matrix);

    HRESULT STDMETHODCALLTYPE SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle);

  private:

    static uint32_t       s_deviceCount;
    uint32_t              m_deviceCount = 0;

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "ddraw4_interface.h"

#include "../d3d6/d3d6_device.h"

#include <unordered_map>

namespace dxvk {

  class DDraw7Surface;

  /**
  * \brief IDirectDrawSurface4 interface implementation
  */
  class DDraw4Surface final : public DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4, d3d9::IDirect3DSurface9> {

  public:

    DDraw4Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw4Surface* pParentSurf,
        IUnknown* origin,
        bool isChildObject);

    ~DDraw4Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface);

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS2 lpDDSCaps);

    HRESULT STDMETHODCALLTYPE GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette);

    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE IsLost();

    HRESULT STDMETHODCALLTYPE Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    HRESULT STDMETHODCALLTYPE Restore();

    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG lX, LONG lY);

    HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);

    HRESULT STDMETHODCALLTYPE Unlock(LPRECT lpSurfaceData);

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    const D3DOptions* GetOptions() const {
      return m_commonIntf->GetOptions();
    }

    D3D6Device* GetD3D6Device() {
      RefreshD3D6Device();
      return m_d3d6Device;
    }

    d3d9::IDirect3DTexture9* GetD3D9Texture() const {
      return m_texture.ptr();
    }

    DDraw4Surface* GetAttachedDepthStencil() const {
      return m_depthStencil.ptr();
    }

    void SetAttachedDepthStencil(DDraw4Surface* ds) {
      ds->SetParentSurface(this);
      m_depthStencil = ds;
    }

    void SetParentSurface(DDraw4Surface* surface) {
      m_parentSurf = surface;
    }

    void ClearParentSurface() {
      m_parentSurf = nullptr;
    }

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeOrUploadD3D9();

  private:

    inline HRESULT InitializeD3D9(const bool initRT);

    inline HRESULT UploadSurfaceData();

    inline void RefreshD3D6Device() {
      D3D6Device* d3d6Device = m_commonIntf->GetD3D6Device();
      if (unlikely(m_d3d6Device != d3d6Device)) {
        // Check if the device has been recreated and reset all D3D9 resources
        if (unlikely(m_d3d6Device != nullptr)) {
          Logger::debug("DDraw4Surface: Device context has changed, clearing all D3D9 resources");
          m_texture = nullptr;
          m_d3d9 = nullptr;
        }
        m_d3d6Device = d3d6Device;
      }
    }

    inline void ListSurfaceDetails() const {
      const char* type = "generic surface";

      if (m_commonSurf->IsFrontBuffer())                type = "front buffer";
      else if (m_commonSurf->IsBackBuffer())            type = "back buffer";
      else if (m_commonSurf->IsTextureMip())            type = "texture mipmap";
      else if (m_commonSurf->IsTexture())               type = "texture";
      else if (m_commonSurf->IsDepthStencil())          type = "depth stencil";
      else if (m_commonSurf->IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (m_commonSurf->IsOverlay())               type = "overlay";
      else if (m_commonSurf->Is3DSurface())             type = "render target";
      else if (m_commonSurf->IsPrimarySurface())        type = "primary surface";
      else if (m_commonSurf->IsNotKnown())              type = "unknown";

      const DDSURFACEDESC2* desc2 = m_commonSurf->GetDesc2();

      Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", m_surfCount, "]]:"));
      Logger::debug(str::format("   Type:        ", type));
      Logger::debug(str::format("   Dimensions:  ", desc2->dwWidth, "x", desc2->dwHeight));
      Logger::debug(str::format("   Format:      ", m_commonSurf->GetD3D9Format()));
      Logger::debug(str::format("   IsComplex:   ", m_commonSurf->IsComplex() ? "yes" : "no"));
      Logger::debug(str::format("   HasMipMaps:  ", desc2->dwMipMapCount ? "yes" : "no"));
      Logger::debug(str::format("   HasColorKey: ", m_commonSurf->HasColorKey() ? "yes" : "no"));
      Logger::debug(str::format("   IsAttached:  ", m_parentSurf != nullptr ? "yes" : "no"));
      if (m_commonSurf->IsFrontBuffer())
        Logger::debug(str::format("   BackBuffers: ", desc2->dwBackBufferCount));
    }

    bool             m_isChildObject = true;

    static uint32_t  s_surfCount;
    uint32_t         m_surfCount = 0;

    Com<DDrawCommonSurface>             m_commonSurf;
    DDrawCommonInterface*               m_commonIntf = nullptr;

    DDraw4Surface*                      m_parentSurf = nullptr;

    IUnknown*                           m_origin     = nullptr;

    D3D6Device*                         m_d3d6Device = nullptr;

    Com<d3d9::IDirect3DTexture9>        m_texture;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw4Surface>                  m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface4*, Com<DDraw4Surface, false>> m_attachedSurfaces;

  };

}

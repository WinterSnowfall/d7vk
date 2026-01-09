#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

#include "../d3d5/d3d5_interface.h"

#include <unordered_map>

namespace dxvk {

  class D3D5Device;
  class D3D5Texture;
  class DDrawSurface;

  /**
  * \brief DirectDraw interface implementation
  */
  class DDrawInterface final : public DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown> {

  public:
    DDrawInterface(
      DDrawCommonInterface* commonIntf,
      Com<IDirectDraw>&& proxyIntf,
      IUnknown* origin,
      bool needsInitialization);

    ~DDrawInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Compact();

    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface);

    HRESULT STDMETHODCALLTYPE EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback);

    HRESULT STDMETHODCALLTYPE EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE FlipToGDISurface();

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes);

    HRESULT STDMETHODCALLTYPE GetGDISurface(LPDIRECTDRAWSURFACE *lplpGDIDDSurface);

    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(LPDWORD lpdwFrequency);

    HRESULT STDMETHODCALLTYPE GetScanLine(LPDWORD lpdwScanLine);

    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(LPBOOL lpbIsInVB);

    HRESULT STDMETHODCALLTYPE Initialize(GUID* lpGUID);

    HRESULT STDMETHODCALLTYPE RestoreDisplayMode();

    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hWnd, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP);

    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);

    D3D5Texture* GetTextureFromHandle(D3DTEXTUREHANDLE handle);

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf.ptr();
    }

    D3D5Interface* GetD3D5Interface() const {
      return m_d3d5Intf.ptr();
    }

    D3D5Device* GetD3D5Device() const {
      return m_d3d5Intf != nullptr ? m_d3d5Intf->GetLastUsedDevice() : nullptr;
    }

    D3DTEXTUREHANDLE GetNextTextureHandle() {
      return ++m_textureHandle;
    }

    void EmplaceTexture(D3D5Texture* texture, D3DTEXTUREHANDLE handle) {
      m_textures.emplace(std::piecewise_construct,
                         std::forward_as_tuple(handle),
                         std::forward_as_tuple(texture));
    }

    DDrawSurface* GetLastDepthStencil() const {
      return m_lastDepthStencil;
    }

  private:

    bool                      m_needsInitialization = false;
    bool                      m_isInitialized = false;

    static uint32_t           s_intfCount;
    uint32_t                  m_intfCount  = 0;

    Com<DDrawCommonInterface> m_commonIntf;

    IUnknown*                 m_origin = nullptr;

    Com<D3D5Interface, false> m_d3d5Intf;

    DDrawSurface*             m_lastDepthStencil = nullptr;

    D3DTEXTUREHANDLE          m_textureHandle = 0;
    std::unordered_map<D3DTEXTUREHANDLE, D3D5Texture*> m_textures;

  };

}
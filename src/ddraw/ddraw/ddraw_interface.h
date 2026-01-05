#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"
#include "ddraw_options.h"

#include "../ddraw_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

#include "../ddraw2/ddraw2_interface.h"

#include "../d3d5/d3d5_interface.h"

#include <vector>
#include <unordered_map>

namespace dxvk {

  class D3D5Device;
  class D3D5Texture;
  class DDraw4Interface;
  class DDraw7Interface;
  class DDrawSurface;

  /**
  * \brief Minimal IDirectDraw interface implementation for IDirectDraw7 QueryInterface calls
  */
  class DDrawInterface final : public DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown> {

  public:
    DDrawInterface(DDrawCommonInterface* commonIntf, Com<IDirectDraw>&& proxyIntf, DDraw7Interface* origin);

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

    bool IsWrappedSurface(IDirectDrawSurface* surface) const;

    void AddWrappedSurface(IDirectDrawSurface* surface);

    void RemoveWrappedSurface(IDirectDrawSurface* surface);

    D3D5Texture* GetTextureFromHandle(D3DTEXTUREHANDLE handle);

    D3D5Device* GetD3D5Device() const {
      return m_d3d5Intf != nullptr ? m_d3d5Intf->GetLastUsedDevice() : nullptr;
    }

    // As an exception, DDrawInterface has its standalone options and
    // its own D3D9 interface and interface bridge to get them
    const D3DOptions* GetOptions() const {
      return &m_options;
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

    DDraw2Interface* GetDDraw2Interface() const {
      return m_intf2;
    }

    void ClearDDraw2Interface() {
      m_intf2 = nullptr;
    }

    DDraw4Interface* GetDDraw4Interface() const {
      return m_intf4;
    }

    void ClearDDraw4Interface() {
      m_intf4 = nullptr;
    }

    DWORD GetCooperativeLevel() const {
      return m_commonIntf->GetCooperativeLevel();
    }

    HWND GetHWND() const {
      return m_commonIntf->GetHWND();
    }

    DDrawModeSize GetModeSize() const {
      return m_modeSize;
    }

    void SetWaitForVBlank(bool waitForVBlank) {
      m_waitForVBlank = waitForVBlank;
    }

    bool GetWaitForVBlank() const {
      return m_waitForVBlank;
    }

  private:

    inline bool IsLegacyInterface() const {
      return m_origin != nullptr;
    }

    static uint32_t            s_intfCount;
    uint32_t                   m_intfCount  = 0;

    Com<DDrawCommonInterface>  m_commonIntf;

    D3DOptions                 m_options;

    DDraw7Interface*           m_origin = nullptr;
    DDraw4Interface*           m_intf4  = nullptr;
    DDraw2Interface*           m_intf2  = nullptr;

    Com<D3D5Interface, false>  m_d3d5Intf;

    bool                       m_waitForVBlank = true;
    DDrawModeSize              m_modeSize = { };

    DDrawSurface*              m_lastDepthStencil = nullptr;

    std::vector<DDrawSurface*> m_surfaces;

    D3DTEXTUREHANDLE           m_textureHandle = 0;
    std::unordered_map<D3DTEXTUREHANDLE, D3D5Texture*> m_textures;

  };

}
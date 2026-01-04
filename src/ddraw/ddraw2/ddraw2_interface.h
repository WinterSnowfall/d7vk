#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_format.h"

#include "../d3d9/d3d9_bridge.h"

#include <vector>

namespace dxvk {

  class D3D5Device;
  class DDrawInterface;
  class DDraw4Interface;
  class DDraw7Interface;
  class DDrawSurface;

  /**
  * \brief DirectDraw2 interface implementation
  */
  class DDraw2Interface final : public DDrawWrappedObject<DDrawInterface, IDirectDraw2, IUnknown> {

  public:
    DDraw2Interface(Com<IDirectDraw2>&& proxyIntf, DDrawInterface* pParent, DDraw7Interface* origin);

    ~DDraw2Interface();

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

    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree);

    DDrawSurface* GetLastDepthStencil() const {
      return m_lastDepthStencil;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    void SetCooperativeLevel(DWORD cooperativeLevel) {
      m_cooperativeLevel = cooperativeLevel;
    }

    DDrawModeSize GetModeSize() const {
      return m_modeSize;
    }

    HWND GetHWND() const {
      return m_hwnd;
    }

    void SetHWND(HWND hwnd) {
      m_hwnd = hwnd;
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

    DDraw7Interface*           m_origin = nullptr;
    DDraw4Interface*           m_intf4  = nullptr;

    HWND                       m_hwnd       = nullptr;

    bool                       m_waitForVBlank = true;

    DWORD                      m_cooperativeLevel = 0;
    DDrawModeSize              m_modeSize = { };

    DDrawSurface*              m_lastDepthStencil = nullptr;

  };

}
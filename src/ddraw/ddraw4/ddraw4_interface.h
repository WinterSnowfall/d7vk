#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_format.h"

#include "../d3d9/d3d9_bridge.h"

#include "../d3d6/d3d6_interface.h"

#include <vector>

namespace dxvk {

  class D3D6Device;
  class DDraw7Interface;
  class DDraw4Surface;

  /**
  * \brief Minimal IDirectDraw4 interface implementation for IDirectDraw7 QueryInterface calls
  */
  class DDraw4Interface final : public DDrawWrappedObject<IUnknown, IDirectDraw4, IUnknown> {

  public:
    DDraw4Interface(Com<IDirectDraw4>&& proxyIntf, DDraw7Interface* origin);

    ~DDraw4Interface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Compact();

    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE4 *lplpDDSurface, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE DuplicateSurface(LPDIRECTDRAWSURFACE4 lpDDSurface, LPDIRECTDRAWSURFACE4 *lplpDupDDSurface);

    HRESULT STDMETHODCALLTYPE EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback);

    HRESULT STDMETHODCALLTYPE EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE FlipToGDISurface();

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes);

    HRESULT STDMETHODCALLTYPE GetGDISurface(LPDIRECTDRAWSURFACE4 *lplpGDIDDSurface);

    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(LPDWORD lpdwFrequency);

    HRESULT STDMETHODCALLTYPE GetScanLine(LPDWORD lpdwScanLine);

    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(LPBOOL lpbIsInVB);

    HRESULT STDMETHODCALLTYPE Initialize(GUID* lpGUID);

    HRESULT STDMETHODCALLTYPE RestoreDisplayMode();

    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hWnd, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree);

    HRESULT STDMETHODCALLTYPE GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE4 *pSurf);

    HRESULT STDMETHODCALLTYPE RestoreAllSurfaces();

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();

    HRESULT STDMETHODCALLTYPE GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags);

    bool IsWrappedSurface(IDirectDrawSurface4* surface) const;

    void AddWrappedSurface(IDirectDrawSurface4* surface);

    void RemoveWrappedSurface(IDirectDrawSurface4* surface);

    D3D6Interface* GetD3D7Interface() const {
      return m_d3d6Intf.ptr();
    }

    D3D6Device* GetD3D6Device() const {
      return m_d3d6Intf->GetLastUsedDevice();
    }

    const D3DOptions* GetOptions() const {
      return m_d3d6Intf->GetOptions();
    }

    DDraw4Surface* GetLastDepthStencil() const {
      return m_lastDepthStencil;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
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

    inline bool IsLegacyInterface() {
      return m_origin != nullptr;
    }

    static uint32_t             s_intfCount;
    uint32_t                    m_intfCount  = 0;

    DDraw7Interface*            m_origin = nullptr;

    Com<D3D6Interface,   false> m_d3d6Intf;

    HWND                        m_hwnd       = nullptr;

    bool                        m_waitForVBlank = true;

    DWORD                       m_cooperativeLevel = 0;
    DDrawModeSize               m_modeSize = { };

    DDraw4Surface*              m_lastDepthStencil = nullptr;

    std::vector<DDraw4Surface*> m_surfaces;

  };

}
#pragma once

#include "d3d7_include.h"
#include "d3d7_interface.h"
#include "../d3d9/d3d9_bridge.h"
#include "ddraw7_wrapped_object.h"

namespace dxvk {

  struct ModeSize {
    DWORD width  = 0;
    DWORD height = 0;
  };

  class D3D7Device;
  class DDraw7Surface;

  /**
  * \brief DirectDraw7 interface implementation
  */
  class DDraw7Interface final : public DDrawWrappedObject<IUnknown, IDirectDraw7, IUnknown> {

  public:
    DDraw7Interface(Com<IDirectDraw7>&& proxyIntf);

    ~DDraw7Interface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Compact();

    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 *lplpDDSurface, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE DuplicateSurface(LPDIRECTDRAWSURFACE7 lpDDSurface, LPDIRECTDRAWSURFACE7 *lplpDupDDSurface);

    HRESULT STDMETHODCALLTYPE EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback);

    HRESULT STDMETHODCALLTYPE EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE FlipToGDISurface();

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes);

    HRESULT STDMETHODCALLTYPE GetGDISurface(LPDIRECTDRAWSURFACE7 *lplpGDIDDSurface);

    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(LPDWORD lpdwFrequency);

    HRESULT STDMETHODCALLTYPE GetScanLine(LPDWORD lpdwScanLine);

    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(LPBOOL lpbIsInVB);

    HRESULT STDMETHODCALLTYPE Initialize(GUID* lpGUID);

    HRESULT STDMETHODCALLTYPE RestoreDisplayMode();

    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hWnd, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree);

    HRESULT STDMETHODCALLTYPE GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *pSurf);

    HRESULT STDMETHODCALLTYPE RestoreAllSurfaces();

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();

    HRESULT STDMETHODCALLTYPE GetDeviceIdentifier(LPDDDEVICEIDENTIFIER2 pDDDI, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE StartModeTest(LPSIZE pModes, DWORD dwNumModes, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE EvaluateMode(DWORD dwFlags, DWORD* pTimeout);

    bool IsWrappedSurface(IDirectDrawSurface7* surface) const;

    void AddWrappedSurface(IDirectDrawSurface7* surface);

    void RemoveWrappedSurface(IDirectDrawSurface7* surface);

    D3D7Interface* GetD3D7Interface() const {
      return m_d3d7Intf.ptr();
    }

    D3D7Device* GetD3D7Device() const {
      return m_d3d7Intf->GetLastUsedDevice();
    }

    const D3D7Options* GetOptions() const {
      return m_d3d7Intf->GetOptions();
    }

    DDraw7Surface* GetLastDepthStencil() const {
      return m_lastDepthStencil;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    ModeSize GetModeSize() const {
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

    static uint32_t             s_intfCount;
    uint32_t                    m_intfCount  = 0;

    Com<D3D7Interface, false>   m_d3d7Intf;

    HWND                        m_hwnd       = nullptr;

    bool                        m_waitForVBlank = true;

    DWORD                       m_cooperativeLevel = 0;
    ModeSize                    m_modeSize;

    DDraw7Surface*              m_lastDepthStencil = nullptr;

    std::vector<DDraw7Surface*> m_surfaces;

  };

}
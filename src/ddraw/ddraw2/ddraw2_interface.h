#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include <vector>

namespace dxvk {

  class D3D5Interface;
  class DDrawSurface;
  class DDrawInterface;
  class DDraw7Interface;

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

    bool IsWrappedSurface(IDirectDrawSurface* surface) const;

    void AddWrappedSurface(IDirectDrawSurface* surface);

    void RemoveWrappedSurface(IDirectDrawSurface* surface);

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    void SetCooperativeLevel(DWORD cooperativeLevel) {
      m_cooperativeLevel = cooperativeLevel;
    }

    HWND GetHWND() const {
      return m_hwnd;
    }

    void SetHWND(HWND hwnd) {
      m_hwnd = hwnd;
    }

  private:

    inline bool IsLegacyInterface() {
      return m_origin != nullptr;
    }

    static uint32_t             s_intfCount;
    uint32_t                    m_intfCount  = 0;

    DDraw7Interface*            m_origin = nullptr;

    Com<D3D5Interface,   false> m_d3d5Intf;

    HWND                        m_hwnd       = nullptr;

    DWORD                       m_cooperativeLevel = 0;

    std::vector<DDrawSurface*> m_surfaces;

  };

}
#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class D3D6Interface;
  class DDraw7Interface;

  /**
  * \brief Minimal IDirectDraw2 interface implementation for IDirectDraw7 QueryInterface calls
  */
  class DDraw2Interface final : public DDrawWrappedObject<IUnknown, IDirectDraw2, IUnknown> {

  public:
    DDraw2Interface(Com<IDirectDraw2>&& proxyIntf, DDraw7Interface* origin);

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

  private:

    inline bool IsLegacyInterface() {
      return m_origin != nullptr;
    }

    static uint32_t             s_intfCount;
    uint32_t                    m_intfCount  = 0;

    DDraw7Interface*            m_origin = nullptr;

    Com<D3D6Interface,   false> m_d3d6Intf;

  };

}
#pragma once

#include "ddraw_include.h"
#include "ddraw_wrapped_object.h"

#include "ddraw2/ddraw2_interface.h"
#include "ddraw4/ddraw4_interface.h"

namespace dxvk {

  class D3D6Interface;
  class DDraw7Interface;

  /**
  * \brief Minimal IDirectDraw interface implementation for IDirectDraw7 QueryInterface calls
  */
  class DDrawInterface final : public DDrawWrappedObject<IUnknown, IDirectDraw, IUnknown> {

  public:
    DDrawInterface(Com<IDirectDraw>&& proxyIntf, DDraw7Interface* origin);

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

  private:

    inline bool IsLegacyInterface() {
      return m_origin != nullptr;
    }

    static uint32_t             s_intfCount;
    uint32_t                    m_intfCount  = 0;

    HWND                        m_hwnd       = nullptr;

    DWORD                       m_cooperativeLevel = 0;

    DDraw7Interface*            m_origin = nullptr;

    Com<D3D6Interface,   false> m_d3d6Intf;

  };

}
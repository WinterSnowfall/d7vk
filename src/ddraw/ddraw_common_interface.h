#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"

#include <vector>

namespace dxvk {

  class D3D3Interface;

  class DDraw7Interface;
  class DDraw4Interface;
  class DDraw2Interface;
  class DDrawInterface;

  class DDrawSurface;
  class DDraw4Surface;
  class DDraw7Surface;

  class D3D7Device;
  class D3D6Device;
  class D3D5Device;

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface(const D3DOptions& options);

    ~DDrawCommonInterface();

    bool IsWrappedSurface(IDirectDrawSurface* surface) const;

    void AddWrappedSurface(IDirectDrawSurface* surface);

    void RemoveWrappedSurface(IDirectDrawSurface* surface);

    bool IsWrappedSurface(IDirectDrawSurface2* surface) const;

    void AddWrappedSurface(IDirectDrawSurface2* surface);

    void RemoveWrappedSurface(IDirectDrawSurface2* surface);

    bool IsWrappedSurface(IDirectDrawSurface3* surface) const;

    void AddWrappedSurface(IDirectDrawSurface3* surface);

    void RemoveWrappedSurface(IDirectDrawSurface3* surface);

    bool IsWrappedSurface(IDirectDrawSurface4* surface) const;

    void AddWrappedSurface(IDirectDrawSurface4* surface);

    void RemoveWrappedSurface(IDirectDrawSurface4* surface);

    bool IsWrappedSurface(IDirectDrawSurface7* surface) const;

    void AddWrappedSurface(IDirectDrawSurface7* surface);

    void RemoveWrappedSurface(IDirectDrawSurface7* surface);

    d3d9::IDirect3DDevice9* GetD3D9Device();

    d3d9::D3DMULTISAMPLE_TYPE GetMultiSampleType();

    d3d9::D3DPRESENT_PARAMETERS GetPresentParameters();

    HRESULT ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params);

    bool IsCurrentRenderTarget(DDrawSurface* surface) const;

    bool IsCurrentRenderTarget(DDraw4Surface* surface) const;

    bool IsCurrentRenderTarget(DDraw7Surface* surface) const;

    bool IsCurrentDepthStencil(DDrawSurface* surface) const;

    bool IsCurrentDepthStencil(DDraw4Surface* surface) const;

    bool IsCurrentDepthStencil(DDraw7Surface* surface) const;

    void SetFlipRTSurfaceAndFlags(IUnknown* surf, DWORD flags) {
      m_flipRTSurf  = surf;
      m_flipRTFlags = flags;
    }

    IUnknown* GetFlipRTSurface() const {
      return m_flipRTSurf;
    }

    DWORD GetFlipRTFlags() const {
      return m_flipRTFlags;
    }

    bool HasDrawn() const {
      // Returning true here means we skip all proxied back buffer blits,
      // whereas returning false means we allow all proxied back buffer blits
      return m_options.backBufferGuard == D3DBackBufferGuard::Strict   ? true :
             m_options.backBufferGuard == D3DBackBufferGuard::Disabled ? false : m_hasDrawn;
    }

    void UpdateDrawTracking() {
      if (unlikely(!m_hasDrawn))
        m_hasDrawn = true;
    }

    void ResetDrawTracking() {
      if (likely(m_options.backBufferGuard != D3DBackBufferGuard::Strict))
        m_hasDrawn = false;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    void SetAdapterIdentifier(const d3d9::D3DADAPTER_IDENTIFIER9& adapterIdentifier9) {
      m_adapterIdentifier9 = adapterIdentifier9;
    }

    const d3d9::D3DADAPTER_IDENTIFIER9* GetAdapterIdentifier() const {
      return &m_adapterIdentifier9;
    }

    const D3DOptions* GetOptions() const {
      return &m_options;
    }

    void SetWaitForVBlank(bool waitForVBlank) {
      m_waitForVBlank = waitForVBlank;
    }

    bool GetWaitForVBlank() const {
      return m_waitForVBlank;
    }

    void SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
      m_hWnd = hWnd;
      m_cooperativeLevel = dwFlags;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    HWND GetHWND() const {
      return m_hWnd;
    }

    DDrawModeSize* GetModeSize() {
      return &m_modeSize;
    }

    void SetD3D3Interface(D3D3Interface* d3d3Intf) {
      m_d3d3Intf = d3d3Intf;
    }

    D3D3Interface* GetD3D3Interface() const {
      return m_d3d3Intf;
    }

    void SetDD7Interface(DDraw7Interface* intf7) {
      m_intf7 = intf7;
    }

    DDraw7Interface* GetDD7Interface() const {
      return m_intf7;
    }

    void SetDD4Interface(DDraw4Interface* intf4) {
      m_intf4 = intf4;
    }

    DDraw4Interface* GetDD4Interface() const {
      return m_intf4;
    }

    void SetDD2Interface(DDraw2Interface* intf2) {
      m_intf2 = intf2;
    }

    DDraw2Interface* GetDD2Interface() const {
      return m_intf2;
    }

    void SetDDInterface(DDrawInterface* intf) {
      m_intf = intf;
    }

    DDrawInterface* GetDDInterface() const {
      return m_intf;
    }

    void SetD3D7Device(D3D7Device* device7) {
      m_device7 = device7;
    }

    D3D7Device* GetD3D7Device() const {
      return m_device7;
    }

    void SetD3D6Device(D3D6Device* device6) {
      m_device6 = device6;
    }

    D3D6Device* GetD3D6Device() const {
      return m_device6;
    }

    void SetD3D5Device(D3D5Device* device5) {
      m_device5 = device5;
    }

    D3D5Device* GetD3D5Device() const {
      return m_device5;
    }

  private:

    // Track draw state on the common interface, since the back buffer
    // guard should protect against global blits, not device specific ones
    bool                              m_hasDrawn           = false;
    bool                              m_waitForVBlank      = true;

    DWORD                             m_cooperativeLevel   = 0;

    HWND                              m_hWnd               = nullptr;

    DDrawModeSize                     m_modeSize           = { };

    IUnknown*                         m_flipRTSurf         = nullptr;
    DWORD                             m_flipRTFlags        = 0;

    d3d9::D3DADAPTER_IDENTIFIER9      m_adapterIdentifier9 = { };

    D3DOptions                        m_options;

    D3D3Interface*                    m_d3d3Intf           = nullptr;

    // Track all possible last used D3D devices
    D3D7Device*                       m_device7            = nullptr;
    D3D6Device*                       m_device6            = nullptr;
    D3D5Device*                       m_device5            = nullptr;

    // Track all possible instance versions of the same object
    DDraw7Interface*                  m_intf7              = nullptr;
    DDraw4Interface*                  m_intf4              = nullptr;
    DDraw2Interface*                  m_intf2              = nullptr;
    DDrawInterface*                   m_intf               = nullptr;

    std::vector<IDirectDrawSurface7*> m_surfaces7;
    std::vector<IDirectDrawSurface4*> m_surfaces4;
    std::vector<IDirectDrawSurface3*> m_surfaces3;
    std::vector<IDirectDrawSurface2*> m_surfaces2;
    std::vector<IDirectDrawSurface*>  m_surfaces;

  };

}
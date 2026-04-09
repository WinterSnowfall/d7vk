#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"

#include <vector>
#include <unordered_map>

namespace dxvk {

  class D3DCommonDevice;
  class D3DCommonTexture;

  class DDrawCommonSurface;

  class D3D3Interface;

  class DDraw7Interface;
  class DDraw4Interface;
  class DDraw2Interface;
  class DDrawInterface;

  class DDrawSurface;

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

    DDrawSurface* GetSurfaceFromTextureHandle(D3DTEXTUREHANDLE handle) const;

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
      return m_d3dOptions.backBufferGuard == D3DBackBufferGuard::Strict   ? true :
             m_d3dOptions.backBufferGuard == D3DBackBufferGuard::Disabled ? false : m_hasDrawn;
    }

    void MarkAsInitialized() {
      m_isInitialized = true;
    }

    bool IsInitialized() const {
      return m_isInitialized;
    }

    void UpdateDrawTracking() {
      if (unlikely(!m_hasDrawn))
        m_hasDrawn = true;
    }

    void ResetDrawTracking() {
      if (likely(m_d3dOptions.backBufferGuard != D3DBackBufferGuard::Strict))
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
      return &m_d3dOptions;
    }

    void SetWaitForVBlank(bool waitForVBlank) {
      m_waitForVBlank = waitForVBlank;
    }

    bool GetWaitForVBlank() const {
      return m_waitForVBlank;
    }

    void SetPrimarySurface(DDrawCommonSurface* ps) {
      m_ps = ps;
    }

    DDrawCommonSurface* GetPrimarySurface() {
      return m_ps;
    }

    void SetRenderTarget(DDrawCommonSurface* rt) {
      m_rt = rt;
    }

    DDrawCommonSurface* GetRenderTarget() {
      return m_rt;
    }

    void SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
      m_hWnd = hWnd;
      m_cooperativeLevel = dwFlags;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    bool IsCooperativeLevelSet() const {
      return (m_cooperativeLevel & DDSCL_NORMAL) ||
             (m_cooperativeLevel & DDSCL_EXCLUSIVE);
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

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

    void SetCommonD3DDevice(D3DCommonDevice* commonD3DDevice) {
      m_commonD3DDevice = commonD3DDevice;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    D3DTEXTUREHANDLE GetNextTextureHandle() {
      return ++m_textureHandle;
    }

    void EmplaceTexture(D3DCommonTexture* commonTex, D3DTEXTUREHANDLE handle) {
      m_textures.emplace(std::piecewise_construct,
                         std::forward_as_tuple(handle),
                         std::forward_as_tuple(commonTex));
    }

    void ReleaseTextureHandle(D3DTEXTUREHANDLE handle) {
      auto textureIter = m_textures.find(handle);

      if (likely(textureIter != m_textures.end()))
        m_textures.erase(textureIter);
    }

  private:

    bool                              m_isInitialized      = false;
    // Track draw state on the common interface, since the back buffer
    // guard should protect against global blits, not device specific ones
    bool                              m_hasDrawn           = false;
    bool                              m_waitForVBlank      = true;

    DWORD                             m_cooperativeLevel   = 0;

    DDrawCommonSurface*               m_ps                 = nullptr;
    DDrawCommonSurface*               m_rt                 = nullptr;
    HWND                              m_hWnd               = nullptr;
    DDrawModeSize                     m_modeSize           = { };

    IUnknown*                         m_flipRTSurf         = nullptr;
    DWORD                             m_flipRTFlags        = 0;

    d3d9::D3DADAPTER_IDENTIFIER9      m_adapterIdentifier9 = { };

    D3DOptions                        m_d3dOptions;

    D3D3Interface*                    m_d3d3Intf           = nullptr;

    // Track all possible instance versions of the same object
    DDraw7Interface*                  m_intf7              = nullptr;
    DDraw4Interface*                  m_intf4              = nullptr;
    DDraw2Interface*                  m_intf2              = nullptr;
    DDrawInterface*                   m_intf               = nullptr;

    // Track the origin surface, as in the DDraw surface
    // that gets created through a DirectDrawCreate(Ex) call
    IUnknown*                         m_origin             = nullptr;

    D3DCommonDevice*                  m_commonD3DDevice    = nullptr;

    std::vector<IDirectDrawSurface7*> m_surfaces7;
    std::vector<IDirectDrawSurface4*> m_surfaces4;
    std::vector<IDirectDrawSurface3*> m_surfaces3;
    std::vector<IDirectDrawSurface2*> m_surfaces2;
    std::vector<IDirectDrawSurface*>  m_surfaces;

    std::atomic<D3DTEXTUREHANDLE>     m_textureHandle = 0;
    std::unordered_map<D3DTEXTUREHANDLE, D3DCommonTexture*> m_textures;

  };

}
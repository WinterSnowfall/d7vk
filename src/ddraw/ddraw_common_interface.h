#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"
#include "ddraw_format.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../d3d9/d3d9_bridge.h"

namespace dxvk {

  struct DDrawModeSize {
    DWORD width;
    DWORD height;
  };

  class D3DCommonDevice;
  class D3DCommonTexture;

  class DDrawCommonSurface;

  class D3D3Interface;

  class DDraw7Interface;
  class DDraw4Interface;
  class DDraw2Interface;
  class DDrawInterface;

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface(const D3DOptions& options);

    ~DDrawCommonInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3D3Interface* GetOrCreateD3D3Interface();

    static bool IsWrappedSurface(IDirectDrawSurface* surface);

    static void AddWrappedSurface(IDirectDrawSurface* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface* surface);

    static bool IsWrappedSurface(IDirectDrawSurface2* surface);

    static void AddWrappedSurface(IDirectDrawSurface2* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface2* surface);

    static bool IsWrappedSurface(IDirectDrawSurface3* surface);

    static void AddWrappedSurface(IDirectDrawSurface3* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface3* surface);

    static bool IsWrappedSurface(IDirectDrawSurface4* surface);

    static void AddWrappedSurface(IDirectDrawSurface4* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface4* surface);

    static bool IsWrappedSurface(IDirectDrawSurface7* surface);

    static void AddWrappedSurface(IDirectDrawSurface7* surface);

    static void RemoveWrappedSurface(IDirectDrawSurface7* surface);

    static D3DCommonTexture* GetCommonTextureFromTextureHandle(D3DTEXTUREHANDLE handle);

    static D3DTEXTUREHANDLE GetNextTextureHandle() {
      return ++s_textureHandle;
    }

    static void EmplaceTexture(D3DTEXTUREHANDLE handle, D3DCommonTexture* commonTex) {
      s_textures.emplace(std::piecewise_construct,
                         std::forward_as_tuple(handle),
                         std::forward_as_tuple(commonTex));
    }

    static void ReleaseTextureHandle(D3DTEXTUREHANDLE handle) {
      auto textureIter = s_textures.find(handle);

      if (likely(textureIter != s_textures.end()))
        s_textures.erase(textureIter);
    }

    void MarkAsInitialized() {
      m_isInitialized = true;
    }

    bool IsInitialized() const {
      return m_isInitialized;
    }

    void SetWaitForVBlank(bool waitForVBlank) {
      m_waitForVBlank = waitForVBlank;
    }

    bool GetWaitForVBlank() const {
      return m_waitForVBlank;
    }

    void UpdateIn8BitDisplayMode(bool in8BitDisplayMode) {
      m_old8BitDisplayMode = m_in8BitDisplayMode;
      m_in8BitDisplayMode = in8BitDisplayMode;
    }

    bool In8BitDisplayMode() const {
      return m_in8BitDisplayMode;
    }

    bool WasIn8BitDisplayMode() const {
      return m_old8BitDisplayMode;
    }

    void SetCommonD3DDevice(D3DCommonDevice* commonD3DDevice) {
      m_commonD3DDevice = commonD3DDevice;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    void SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
      m_hWnd = hWnd;
      m_skipHWNDOverride = m_hWnd != nullptr;
      m_cooperativeLevel = dwFlags;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    bool IsCooperativeLevelSet() const {
      return (m_cooperativeLevel & DDSCL_NORMAL) ||
             (m_cooperativeLevel & DDSCL_EXCLUSIVE);
    }

    // Used to hook the HWND during SetClipper calls,
    // or during SetHWnd calls on the clipper itself,
    // and use that on device creation if no other
    // HWND is specified through SetCooperativeLevel
    void SetHWND(HWND hWnd) {
      if (unlikely(!m_skipHWNDOverride))
        m_hWnd = hWnd;
    }

    HWND GetHWND() const {
      return m_hWnd;
    }

    void SetPrimarySurface(DDrawCommonSurface* ps) {
      m_ps = ps;
    }

    DDrawCommonSurface* GetPrimarySurface() {
      return m_ps;
    }

    DDrawModeSize* GetModeSize() {
      return &m_modeSize;
    }

    const d3d9::D3DADAPTER_IDENTIFIER9* GetAdapterIdentifier() {
      // We will always have a valid D3D9 interface at this point
      if (unlikely(m_adapterIdentifier9.DeviceIdentifier == GUID_NULL))
        m_d3d9Intf->GetAdapterIdentifier(0, 0, &m_adapterIdentifier9);

      return &m_adapterIdentifier9;
    }

    const std::vector<DDSURFACEDESC2>& GetDisplayModes() {
      if (!m_displayModes.size())
        CacheDisplayModes();

      return m_displayModes;
    }

    const D3DOptions* GetOptions() const {
      return &m_d3dOptions;
    }

    void SetD3D9Interface(Com<d3d9::IDirect3D9>&& d3d9Intf) {
      m_d3d9Intf = std::move(d3d9Intf);
    }

    d3d9::IDirect3D9* GetD3D9Interface() const {
      return m_d3d9Intf.ptr();
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

  private:

    inline void CacheDisplayModes() {
      // Enumerate the 32-bit color modes first, followed by 16-bit color modes
      static d3d9::D3DFORMAT adapterFormats[2] = { d3d9::D3DFMT_X8R8G8B8, d3d9::D3DFMT_R5G6B5 };

      std::vector<DDSURFACEDESC2> displayModes8Bit;

      for (auto& adapterformat : adapterFormats) {
        uint32_t lastWidth = 0u;
        uint32_t lastHeight = 0u;

        // We will always have a valid D3D9 interface at this point
        uint32_t modeCount = m_d3d9Intf->GetAdapterModeCount(0, adapterformat);

        for (uint32_t modeIndex = 0; modeIndex < modeCount; modeIndex++) {
          d3d9::D3DDISPLAYMODE mode;
          m_d3d9Intf->EnumAdapterModes(0, adapterformat, modeIndex, &mode);

          // We get an ordered list here from D3D9, desceding by refresh rate for each mode,
          // so we can simply ignore refresh rate differences as they aren't relevant for DDraw,
          // as well as any resolutions with only below 60 Hz refresh rates
          if (mode.RefreshRate >= 60 && (lastWidth != mode.Width || lastHeight != mode.Height)) {
            lastWidth = mode.Width;
            lastHeight = mode.Height;

            //Logger::debug(str::format("DDrawCommonInterface::CacheDisplayModes: Format: ", mode.Format));
            //Logger::debug(str::format("DDrawCommonInterface::CacheDisplayModes: Width: ", mode.Width));
            //Logger::debug(str::format("DDrawCommonInterface::CacheDisplayModes: Height: ", mode.Height));
            //Logger::debug(str::format("DDrawCommonInterface::CacheDisplayModes: RefreshRate: ", mode.RefreshRate));

            if (m_d3dOptions.forceLegacyModeFilter &&
              (mode.Width  > ddrawCaps::MaxTextureDimensionLegacy
            || mode.Height > ddrawCaps::MaxTextureDimensionLegacy)) {
              static bool s_modeDimensionsWarningShown;

              if (!std::exchange(s_modeDimensionsWarningShown, true))
                Logger::info("DDrawCommonInterface::CacheDisplayModes: Masking display modes with exceeding dimensions");

              continue;
            }

            if (m_d3dOptions.forceLegacyAspectRatio &&
                !IsLegacyAspectRatioMode(mode.Width, mode.Height)) {
              static bool s_modeAspectRatioWarningShown;

              if (!std::exchange(s_modeAspectRatioWarningShown, true))
                Logger::warn("DDrawCommonInterface::CacheDisplayModes: Masking display modes with non-legacy aspect ratio");

              continue;
            }

            DDSURFACEDESC2 modeDesc = { };
            modeDesc.dwSize = sizeof(DDSURFACEDESC2);
            modeDesc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
            modeDesc.dwWidth = mode.Width;
            modeDesc.dwHeight = mode.Height;
            modeDesc.ddpfPixelFormat = GetPixelFormatForD3D9AdapterFormat(adapterformat);

            m_displayModes.push_back(modeDesc);

            // Also construct a list of fake 8-bit P8 modes, mimicking real 16-bit modes
            if (!m_d3dOptions.mask8BitModes && mode.Format == d3d9::D3DFMT_R5G6B5) {
              DDSURFACEDESC2 modeDesc8Bit = { };
              modeDesc8Bit.dwSize = sizeof(DDSURFACEDESC2);
              modeDesc8Bit.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
              modeDesc8Bit.dwWidth = mode.Width;
              modeDesc8Bit.dwHeight = mode.Height;
              modeDesc8Bit.ddpfPixelFormat = GetPixelFormatForD3D9AdapterFormat(d3d9::D3DFMT_P8);

              displayModes8Bit.push_back(modeDesc8Bit);
            }
          }
        }
      }

      if (!m_d3dOptions.mask8BitModes) {
        // Append the fake 8-bit modes at the end of the display mode list
        for (auto& displayMode8Bit : displayModes8Bit)
          m_displayModes.push_back(displayMode8Bit);
      } else {
        Logger::warn("DDrawCommonInterface::CacheDisplayModes: Masking 8-bit display modes");
      }
    }

    bool                              m_isInitialized      = false;
    bool                              m_waitForVBlank      = true;
    bool                              m_skipHWNDOverride   = false;

    bool                              m_in8BitDisplayMode  = false;
    // We also need to track the old state for RestoreDisplayMode()
    bool                              m_old8BitDisplayMode = false;

    D3DCommonDevice*                  m_commonD3DDevice    = nullptr;

    HWND                              m_hWnd               = nullptr;
    DWORD                             m_cooperativeLevel   = 0u;

    DDrawCommonSurface*               m_ps                 = nullptr;
    DDrawModeSize                     m_modeSize           = { };

    d3d9::D3DADAPTER_IDENTIFIER9      m_adapterIdentifier9 = { };
    std::vector<DDSURFACEDESC2>       m_displayModes;

    D3DOptions                        m_d3dOptions;

    // We need to keep a D3D9 interface around in order to get access
    // to adapter as well as mode information for the DDraw endpoints
    Com<d3d9::IDirect3D9>             m_d3d9Intf;

    D3D3Interface*                    m_d3d3Intf           = nullptr;

    DDraw7Interface*                  m_intf7              = nullptr;
    DDraw4Interface*                  m_intf4              = nullptr;
    DDraw2Interface*                  m_intf2              = nullptr;
    DDrawInterface*                   m_intf               = nullptr;

    // Track the origin surface, as in the DDraw surface
    // that gets created through a DirectDrawCreate(Ex) call
    IUnknown*                         m_origin             = nullptr;

    // Tests have indicated that once created, texture handles are shared across
    // all devices and DDraw interfaces, regardless of their relation
    static std::atomic<D3DTEXTUREHANDLE> s_textureHandle;
    static inline std::unordered_map<D3DTEXTUREHANDLE, D3DCommonTexture*> s_textures;

    // Keep wrapped surface tracking shared between all DDraw interfaces as tests
    // as well as some games (e.g. GTA 2) depend on wrapped surface lookups across
    // unrelated DDraw interface objects... yes, really...
    static inline std::unordered_set<IDirectDrawSurface7*> s_surfaces7;
    static inline std::unordered_set<IDirectDrawSurface4*> s_surfaces4;
    static inline std::unordered_set<IDirectDrawSurface3*> s_surfaces3;
    static inline std::unordered_set<IDirectDrawSurface2*> s_surfaces2;
    static inline std::unordered_set<IDirectDrawSurface*>  s_surfaces;

  };

}

#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"

#include <vector>

namespace dxvk {

  class DDraw7Interface;
  class DDraw4Interface;
  class DDraw2Interface;
  class DDrawInterface;

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface(D3DOptions options);

    ~DDrawCommonInterface();

    bool IsWrappedSurface(IDirectDrawSurface* surface) const;

    void AddWrappedSurface(IDirectDrawSurface* surface);

    void RemoveWrappedSurface(IDirectDrawSurface* surface);

    bool IsWrappedSurface(IDirectDrawSurface4* surface) const;

    void AddWrappedSurface(IDirectDrawSurface4* surface);

    void RemoveWrappedSurface(IDirectDrawSurface4* surface);

    bool IsWrappedSurface(IDirectDrawSurface7* surface) const;

    void AddWrappedSurface(IDirectDrawSurface7* surface);

    void RemoveWrappedSurface(IDirectDrawSurface7* surface);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
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
      m_hwnd = hWnd;
      m_cooperativeLevel = dwFlags;
    }

    DWORD GetCooperativeLevel() const {
      return m_cooperativeLevel;
    }

    HWND GetHWND() const {
      return m_hwnd;
    }

    DDrawModeSize* GetModeSize() {
      return &m_modeSize;
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

  private:

    bool                              m_waitForVBlank    = true;

    DWORD                             m_cooperativeLevel = 0;

    HWND                              m_hwnd             = nullptr;

    DDrawModeSize                     m_modeSize = { };

    D3DOptions                        m_options;

    // Track all possible instance versions of the same object
    DDraw7Interface*                  m_intf7            = nullptr;
    DDraw4Interface*                  m_intf4            = nullptr;
    DDraw2Interface*                  m_intf2            = nullptr;
    DDrawInterface*                   m_intf             = nullptr;

    std::vector<IDirectDrawSurface7*> m_surfaces7;
    std::vector<IDirectDrawSurface4*> m_surfaces4;
    std::vector<IDirectDrawSurface*>  m_surfaces;

  };

}
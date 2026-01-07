#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"

namespace dxvk {

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface(D3DOptions options);

    ~DDrawCommonInterface();

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

  private:

    bool          m_waitForVBlank    = true;

    DWORD         m_cooperativeLevel = 0;

    HWND          m_hwnd             = nullptr;

    DDrawModeSize m_modeSize = { };

    D3DOptions    m_options;

  };

}
#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class DDrawCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonInterface();

    ~DDrawCommonInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
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

  private:

    DWORD m_cooperativeLevel = 0;

    HWND  m_hwnd             = nullptr;

  };

}
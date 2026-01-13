#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class DDrawClipper final : public DDrawWrappedObject<IUnknown, IDirectDrawClipper, IUnknown> {

  public:

    DDrawClipper(
          Com<IDirectDrawClipper>&& clipperProxy,
          IUnknown* pParent);

    ~DDrawClipper();

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetClipList(LPRECT lpRect, LPRGNDATA lpClipList, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE IsClipListChanged(BOOL *lpbChanged);

    HRESULT STDMETHODCALLTYPE SetClipList(LPRGNDATA lpClipList, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetHWnd(DWORD dwFlags, HWND hWnd);

    HRESULT STDMETHODCALLTYPE GetHWnd(HWND *lphWnd);

  };

}
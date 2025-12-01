#pragma once

#include "d3d7_include.h"
#include "ddraw7_interface.h"
#include "ddraw7_wrapped_object.h"

namespace dxvk {

  class DDraw7Clipper final : public DDrawWrappedObject<DDraw7Interface, IDirectDrawClipper, IUnknown> {

  public:

    DDraw7Clipper(
          Com<IDirectDrawClipper>&& clipperProxy,
          DDraw7Interface* pParent);

    ~DDraw7Clipper();

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetClipList(LPRECT lpRect, LPRGNDATA lpClipList, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE IsClipListChanged(BOOL *lpbChanged);

    HRESULT STDMETHODCALLTYPE SetClipList(LPRGNDATA lpClipList, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetHWnd(DWORD dwFlags, HWND hWnd);

    HRESULT STDMETHODCALLTYPE GetHWnd(HWND *lphWnd);

  };

}
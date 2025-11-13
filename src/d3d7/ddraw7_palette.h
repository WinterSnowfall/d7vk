#pragma once

#include "d3d7_include.h"
#include "ddraw7_interface.h"
#include "ddraw7_wrapped_object.h"

namespace dxvk {

  class DDraw7Palette final : public DDrawWrappedObject<DDraw7Interface, IDirectDrawPalette, IUnknown> {

  public:

    DDraw7Palette(
          Com<IDirectDrawPalette>&& paletteProxy,
          DDraw7Interface* pParent);

    ~DDraw7Palette();

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDWORD lpdwCaps);

    HRESULT STDMETHODCALLTYPE GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries);

    HRESULT STDMETHODCALLTYPE SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries);

  };

}
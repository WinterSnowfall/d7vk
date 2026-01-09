#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

namespace dxvk {

  class DDrawInterface;

  class DDrawPalette final : public DDrawWrappedObject<DDrawInterface, IDirectDrawPalette, IUnknown> {

  public:

    DDrawPalette(
          Com<IDirectDrawPalette>&& paletteProxy,
          DDrawInterface* pParent);

    ~DDrawPalette();

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDWORD lpdwCaps);

    HRESULT STDMETHODCALLTYPE GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries);

    HRESULT STDMETHODCALLTYPE SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries);

  };

}
#include "ddraw_palette.h"

namespace dxvk {

  DDrawPalette::DDrawPalette(
        Com<IDirectDrawPalette>&& paletteProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawPalette, IUnknown>(pParent, std::move(paletteProxy), nullptr) {
    if (m_parent != nullptr)
      m_parent->AddRef();

    Logger::debug("DDrawPalette: Created a new palette");
  }

  DDrawPalette::~DDrawPalette() {
    if (m_parent != nullptr)
      m_parent->Release();

    Logger::debug("DDrawPalette: A palette bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDrawPalette, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawPalette))
      return this;

    throw DxvkError("DDrawPalette::QueryInterface: Unknown interface query");
  }

  // Docs state: "Because the DirectDrawPalette object is initialized when
  // it is created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDrawPalette::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable) {
    Logger::debug(">>> DDrawPalette::Initialize");
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::GetCaps(LPDWORD lpdwCaps) {
    Logger::debug("<<< DDrawPalette::GetCaps: Proxy");
    return m_proxy->GetCaps(lpdwCaps);
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) {
    Logger::debug("<<< DDrawPalette::GetEntries: Proxy");
    return m_proxy->GetEntries(dwFlags, dwBase, dwNumEntries, lpEntries);
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) {
    Logger::debug("<<< DDrawPalette::SetEntries: Proxy");
    return m_proxy->SetEntries(dwFlags, dwStartingEntry, dwCount, lpEntries);
  }

}


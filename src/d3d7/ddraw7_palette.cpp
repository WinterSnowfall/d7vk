#include "ddraw7_palette.h"

namespace dxvk {

  DDraw7Palette::DDraw7Palette(
        Com<IDirectDrawPalette>&& paletteProxy,
        DDraw7Interface* pParent)
    : DDrawWrappedObject<DDraw7Interface, IDirectDrawPalette, IUnknown>(pParent, std::move(paletteProxy), nullptr) {
    m_parent->AddRef();

    Logger::debug("DDraw7Palette: Created a new palette");
  }

  DDraw7Palette::~DDraw7Palette() {
    Logger::debug("DDraw7Palette: A palette bites the dust");

    m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw7Interface, IDirectDrawPalette, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawPalette)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDraw7Palette::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    Logger::debug("DDraw7Palette::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Palette::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable) {
    Logger::debug("<<< DDraw7Palette::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, dwFlags, lpDDColorTable);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Palette::GetCaps(LPDWORD lpdwCaps) {
    Logger::debug("<<< DDraw7Palette::GetCaps: Proxy");
    return m_proxy->GetCaps(lpdwCaps);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Palette::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) {
    Logger::debug("<<< DDraw7Palette::GetEntries: Proxy");
    return m_proxy->GetEntries(dwFlags, dwBase, dwNumEntries, lpEntries);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Palette::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) {
    Logger::debug("<<< DDraw7Palette::SetEntries: Proxy");
    return m_proxy->SetEntries(dwFlags, dwStartingEntry, dwCount, lpEntries);
  }

}


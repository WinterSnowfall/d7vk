#include "ddraw_palette.h"

namespace dxvk {

  DDrawPalette::DDrawPalette(
        Com<IDirectDrawPalette>&& paletteProxy,
        DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawPalette, IUnknown>(pParent, std::move(paletteProxy), nullptr) {
    m_parent->AddRef();

    Logger::debug("DDrawPalette: Created a new palette");
  }

  DDrawPalette::~DDrawPalette() {
    m_parent->Release();

    Logger::debug("DDrawPalette: A palette bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDrawPalette, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawPalette)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDrawPalette::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDrawPalette::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawPalette::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable) {
    Logger::debug("<<< DDrawPalette::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, dwFlags, lpDDColorTable);
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


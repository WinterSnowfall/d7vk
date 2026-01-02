#include "ddraw_clipper.h"

namespace dxvk {

  DDrawClipper::DDrawClipper(
        Com<IDirectDrawClipper>&& clipperProxy,
        DDrawInterface* pParent)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawClipper, IUnknown>(pParent, std::move(clipperProxy), nullptr) {
    if (likely(m_parent != nullptr))
      m_parent->AddRef();

    Logger::debug("DDrawClipper: Created a new clipper");
  }

  DDrawClipper::~DDrawClipper() {
    if (likely(m_parent != nullptr))
      m_parent->Release();

    Logger::debug("DDrawClipper: A clipper bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawInterface, IDirectDrawClipper, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawClipper)) {
      if (unlikely(m_forwardToProxy)) {
        Logger::debug("DDrawClipper::QueryInterface: Forwarding interface query to proxied object");
        // Hack: Return the proxied interface, as some applications need
        // to use an unwrapped object in relation with external modules
        void* ppvObject = nullptr;
        HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
        if (likely(SUCCEEDED(hr)))
          return reinterpret_cast<IUnknown*>(ppvObject);
      }
      return this;
    }

    throw DxvkError("DDrawClipper::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags) {
    Logger::debug("<<< DDrawClipper::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::GetClipList(LPRECT lpRect, LPRGNDATA lpClipList, LPDWORD lpdwSize) {
    Logger::debug("<<< DDrawClipper::GetClipList: Proxy");
    return m_proxy->GetClipList(lpRect, lpClipList, lpdwSize);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::IsClipListChanged(BOOL *lpbChanged) {
    Logger::debug("<<< DDrawClipper::IsClipListChanged: Proxy");
    return m_proxy->IsClipListChanged(lpbChanged);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::SetClipList(LPRGNDATA lpClipList, DWORD dwFlags) {
    Logger::debug("<<< DDrawClipper::SetClipList: Proxy");
    return m_proxy->SetClipList(lpClipList, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::SetHWnd(DWORD dwFlags, HWND hWnd) {
    Logger::debug("<<< DDrawClipper::SetHWnd: Proxy");
    return m_proxy->SetHWnd(dwFlags, hWnd);
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::GetHWnd(HWND *lphWnd) {
    Logger::debug("<<< DDrawClipper::GetHWnd: Proxy");
    return m_proxy->GetHWnd(lphWnd);
  }

}


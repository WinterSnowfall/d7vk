#include "ddraw7_clipper.h"

namespace dxvk {

  DDraw7Clipper::DDraw7Clipper(
        Com<IDirectDrawClipper>&& clipperProxy,
        DDraw7Interface* pParent)
    : DDrawWrappedObject<DDraw7Interface, IDirectDrawClipper, IUnknown>(pParent, std::move(clipperProxy), nullptr) {
    m_parent->AddRef();

    Logger::debug("DDraw7Clipper: Created a new clipper");
  }

  DDraw7Clipper::~DDraw7Clipper() {
    Logger::debug("DDraw7Clipper: A clipper bites the dust");

    m_parent->Release();
  }

  template<>
  IUnknown* DDrawWrappedObject<DDraw7Interface, IDirectDrawClipper, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawClipper)) {
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

    Logger::debug("DDraw7Clipper::QueryInterface: Forwarding interface query to parent");
    return m_parent->GetInterface(riid);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Clipper::Initialize: Proxy");
    return m_proxy->Initialize(lpDD, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::GetClipList(LPRECT lpRect, LPRGNDATA lpClipList, LPDWORD lpdwSize) {
    Logger::debug("<<< DDraw7Clipper::GetClipList: Proxy");
    return m_proxy->GetClipList(lpRect, lpClipList, lpdwSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::IsClipListChanged(BOOL *lpbChanged) {
    Logger::debug("<<< DDraw7Clipper::IsClipListChanged: Proxy");
    return m_proxy->IsClipListChanged(lpbChanged);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::SetClipList(LPRGNDATA lpClipList, DWORD dwFlags) {
    Logger::debug("<<< DDraw7Clipper::SetClipList: Proxy");
    return m_proxy->SetClipList(lpClipList, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::SetHWnd(DWORD dwFlags, HWND hWnd) {
    Logger::debug("<<< DDraw7Clipper::SetHWnd: Proxy");
    return m_proxy->SetHWnd(dwFlags, hWnd);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Clipper::GetHWnd(HWND *lphWnd) {
    Logger::debug("<<< DDraw7Clipper::GetHWnd: Proxy");
    return m_proxy->GetHWnd(lphWnd);
  }

}


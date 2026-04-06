#include "ddraw_clipper.h"

namespace dxvk {

  DDrawClipper::DDrawClipper(
        Com<IDirectDrawClipper>&& clipperProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawClipper, IUnknown>(pParent, std::move(clipperProxy), nullptr) {
    Logger::debug("DDrawClipper: Created a new clipper");
  }

  DDrawClipper::~DDrawClipper() {
    Logger::debug("DDrawClipper: A clipper bites the dust");
  }

  template<>
  IUnknown* DDrawWrappedObject<IUnknown, IDirectDrawClipper, IUnknown>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirectDrawClipper))
      return this;

    throw DxvkError("DDrawClipper::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags) {
    Logger::debug(">>> DDrawClipper::Initialize");

    if (unlikely(m_isInitialized))
      return DDERR_ALREADYINITIALIZED;

    m_isInitialized = true;

    return DD_OK;
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


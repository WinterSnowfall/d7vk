#include "ddraw_clipper.h"

namespace dxvk {

  DDrawClipper::DDrawClipper(
        DDrawCommonInterface* commonIntf,
        Com<IDirectDrawClipper>&& clipperProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawClipper>(pParent, std::move(clipperProxy))
    , m_commonIntf ( commonIntf ) {
    Logger::debug("DDrawClipper: Created a new clipper");
  }

  DDrawClipper::~DDrawClipper() {
    Logger::debug("DDrawClipper: A clipper bites the dust");
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags) {
    Logger::debug(">>> DDrawClipper::Initialize");

    if (unlikely(m_isInitialized))
      return DDERR_ALREADYINITIALIZED;

    m_isInitialized = true;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::QueryInterface(REFIID riid, void** ppvObject) {
    Logger::debug(">>> DDrawClipper::QueryInterface");

    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    try {
      *ppvObject = ref(this->GetInterface(riid));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::warn(e.message());
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }
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

    HRESULT hr = m_proxy->SetHWnd(dwFlags, hWnd);

    // The Eschalon: Book I launcher creates a device before attaching the clipper to
    // the primary surface, but right after it sets a HWND on the newly created clipper
    if (likely(SUCCEEDED(hr) && m_commonIntf != nullptr)) {
      // Only set the cached hWnd if it's absent
      if (unlikely(m_commonIntf->GetHWND() == nullptr)) {
        Logger::debug("DDrawClipper::SetHWnd: Caching passed hWnd");
        m_commonIntf->SetHWND(hWnd);
      }
    }

    return hr;
  }

  HRESULT STDMETHODCALLTYPE DDrawClipper::GetHWnd(HWND *lphWnd) {
    Logger::debug("<<< DDrawClipper::GetHWnd: Proxy");
    return m_proxy->GetHWnd(lphWnd);
  }

}


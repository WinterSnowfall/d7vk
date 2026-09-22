#include "ddraw_class_factory.h"

namespace dxvk {

  DDrawClassFactory::DDrawClassFactory(FunctionType createInstance)
    : m_createInstance ( createInstance ) {
  }

  DDrawClassFactory::~DDrawClassFactory() {
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (likely(riid == __uuidof(IUnknown)
            || riid == __uuidof(IClassFactory))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawClassFactory::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    return m_createInstance(pUnkOuter, riid, ppvObject);
  }

  // Allegedly used for cacheing/keeping queried objects in memory. Docs state:
  // "Most clients do not need to call this method. It is provided only for those clients
  //  that require special performance in creating multiple instances of their objects."
  HRESULT STDMETHODCALLTYPE DDrawClassFactory::LockServer(BOOL fLock) {
    return S_OK;
  }

}
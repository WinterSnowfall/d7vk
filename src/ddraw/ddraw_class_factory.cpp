#include "ddraw_class_factory.h"

namespace dxvk {

  DDrawClassFactory::DDrawClassFactory(FunctionType createInstance)
    : m_createInstance ( createInstance ) {
  }

  HRESULT STDMETHODCALLTYPE DDrawClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
    Logger::debug(">>> DDrawClassFactory::CreateInstance");

    return m_createInstance(pUnkOuter, riid, ppvObject);
  }

}
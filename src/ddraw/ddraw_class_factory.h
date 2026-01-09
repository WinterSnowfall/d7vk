#pragma once

#include "ddraw_include.h"

namespace dxvk {

  class DDrawClassFactory : public ComObjectClamp<IClassFactory> {

  using FunctionType = HRESULT(&)(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);

  public:

    DDrawClassFactory(FunctionType createInstance);

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) {
      Logger::warn(">>> DDrawClassFactory::LockServer: Stub");
      return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      Logger::debug(">>> DDrawClassFactory::QueryInterface");

      if (unlikely(ppvObject == nullptr))
        return E_POINTER;

      InitReturnPtr(ppvObject);

      if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      return E_NOINTERFACE;
    }

  private:

    FunctionType m_createInstance;

  };

}
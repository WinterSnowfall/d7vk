#pragma once

#include "d3d7_include.h"

namespace dxvk {

  template <typename D3D9Type, typename DDrawType>
  class DDrawWrappedObject : public ComObjectClamp<DDrawType> {

  public:

    using D3D9 = D3D9Type;
    using DDraw = DDrawType;

    DDrawWrappedObject(Com<D3D9>&& object, Com<DDraw>&& proxiedIntf)
      : m_d3d9  ( std::move(object) )
      , m_proxy ( std::move(proxiedIntf) ) {
    }

    D3D9* GetD3D9() {
      return m_d3d9.ptr();
    }

    void SetD3D9(Com<D3D9>&& object) {
      m_d3d9 = std::move(object);
    }

    DDraw* GetProxied() {
      return m_proxy.ptr();
    }

    // For cases where the object may be null.
    static D3D9* GetD3D9Nullable(DDrawWrappedObject* self) {
      if (unlikely(self == NULL)) {
        return NULL;
      }
      return self->m_d3d9.ptr();
    }

    template <typename T>
    static D3D9* GetD3D9Nullable(Com<T>& self) {
      return GetD3D9Nullable(self.ptr());
    }

    virtual IUnknown* GetInterface(REFIID riid) {
      if (riid == __uuidof(IUnknown))
        return this;
      if (riid == __uuidof(DDraw))
        return this;

      throw DxvkError("DDrawWrappedObject::QueryInterface: Unknown interface query");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      if (unlikely(ppvObject == nullptr))
        return E_POINTER;

      *ppvObject = nullptr;

      // TODO: Uncomment once things are in a decent state
      // in order to enable surface gamma and color control
      if (riid == __uuidof(IDirectDrawColorControl)
       || riid == __uuidof(IDirectDrawGammaControl)) {
        return E_NOINTERFACE;
        //return m_proxy->QueryInterface(riid, ppvObject);
      }

      try {
        *ppvObject = ref(this->GetInterface(riid));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::warn(e.message());
        Logger::warn(str::format(riid));
        return E_NOINTERFACE;
      }
    }

  protected:

    Com<D3D9>  m_d3d9;
    Com<DDraw> m_proxy;

  };

}
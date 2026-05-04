#pragma once

#include "ddraw_include.h"

namespace dxvk {

  template <typename ParentType, typename DDrawType>
  class DDrawWrappedObject : public ComObjectClamp<DDrawType> {

  public:

    using Parent = ParentType;
    using DDraw = DDrawType;

    DDrawWrappedObject(Parent* parent, Com<DDraw>&& proxiedIntf)
      : m_parent ( parent )
      , m_proxy ( std::move(proxiedIntf) ) {
    }

    void UpdateParent(Parent* parent) {
      m_parent = parent;
    }

    Parent* GetParent() const {
      return m_parent;
    }

    DDraw* GetProxied() const {
      return m_proxy.ptr();
    }

    IUnknown* GetInterface(REFIID riid) {
      if (riid == __uuidof(IUnknown))
        return this;
      if (riid == __uuidof(DDraw))
        return this;

      throw DxvkError("DDrawWrappedObject::QueryInterface: Unknown interface query");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      Logger::debug(">>> DDrawWrappedObject::QueryInterface");

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

  protected:

    Parent*    m_parent = nullptr;

    Com<DDraw> m_proxy;

  };

}
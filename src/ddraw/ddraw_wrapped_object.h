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

  protected:

    Parent*    m_parent = nullptr;

    Com<DDraw> m_proxy;

  };

}
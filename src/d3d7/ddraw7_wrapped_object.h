#pragma once

#include "d3d7_include.h"

namespace dxvk {

  template <typename ParentType, typename DDrawType, typename D3D9Type>
  class DDrawWrappedObject : public ComObjectClamp<DDrawType> {

  public:

    using Parent = ParentType;
    using DDraw = DDrawType;
    using D3D9 = D3D9Type;

    DDrawWrappedObject(Parent* parent, Com<DDraw>&& proxiedIntf, Com<D3D9>&& object)
      : m_parent ( parent )
      , m_d3d9  ( std::move(object) )
      , m_proxy ( std::move(proxiedIntf) ) {
    }

    Parent* GetParent() const {
      return m_parent;
    }

    DDraw* GetProxied() const {
      return m_proxy.ptr();
    }

    D3D9* GetD3D9() const {
      return m_d3d9.ptr();
    }

    void SetD3D9(Com<D3D9>&& object) {
      m_d3d9 = std::move(object);
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

    void SetForwardToProxy(bool forwardToProxy) {
      m_forwardToProxy = forwardToProxy;
    }

    // Implemented/specialized in all of the invididual wrapped
    // object types, due to the need of hierarchical forwarding
    virtual IUnknown* GetInterface(REFIID riid);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (unlikely(ppvObject == nullptr))
        return E_POINTER;

      *ppvObject = nullptr;

      // Some games query for the legacy ddraw interface
      if (unlikely(riid == __uuidof(IDirectDraw))) {
        Logger::warn("QueryInterface: Query for legacy IDirectDraw");
        return m_proxy->QueryInterface(riid, ppvObject);
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

    bool       m_forwardToProxy = false;

    Parent*    m_parent = nullptr;

    Com<D3D9>  m_d3d9;
    Com<DDraw> m_proxy;

  };

}
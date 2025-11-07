#pragma once

#include "d3d7_include.h"
#include "ddraw7_gamma.h"

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

    virtual IUnknown* GetInterface(REFIID riid) {
      if (riid == __uuidof(IUnknown))
        return this;
      if (riid == __uuidof(DDraw)) {
        if (unlikely(m_forwardToProxy)) {
          Logger::debug("DDrawWrappedObject::QueryInterface: Forwarding interface query to proxied object");
          // Hack: Return the proxied interface, as some applications need
          // to use an unwarpped object in relation with external modules
          void* ppvObject = nullptr;
          HRESULT hr = m_proxy->QueryInterface(riid, &ppvObject);
          if (likely(SUCCEEDED(hr)))
            return reinterpret_cast<IUnknown*>(ppvObject);
        }
        return this;
      }

      if (likely(m_parent != nullptr)) {
        Logger::debug("DDrawWrappedObject::QueryInterface: Forwarding interface query to parent");
        return m_parent->GetInterface(riid);
      }

      throw DxvkError("DDrawWrappedObject::QueryInterface: Unknown interface query");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      if (unlikely(ppvObject == nullptr))
        return E_POINTER;

      *ppvObject = nullptr;

      // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
      if (riid == __uuidof(IDirectDrawGammaControl)) {
        void* d3d7GammaProxiedVoid = nullptr;
        // This can never reasonably fail
        m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &d3d7GammaProxiedVoid);
        Com<IDirectDrawGammaControl> d3d7GammaProxied = static_cast<IDirectDrawGammaControl*>(d3d7GammaProxiedVoid);
        *ppvObject = ref(new DDraw7GammaControl(std::move(d3d7GammaProxied)));
        return S_OK;
      }
      if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
        return m_proxy->QueryInterface(riid, ppvObject);
      }
      // Some games query the legacy ddraw interface from the new one
      if (unlikely(riid == __uuidof(IDirectDraw))) {
        Logger::warn("QueryInterface: Query for legacy IDirectDraw");
        return m_proxy->QueryInterface(riid, ppvObject);
      }
      // Some games query legacy ddraw surfaces from the new one
      if (unlikely(riid == __uuidof(IDirectDrawSurface)
                || riid == __uuidof(IDirectDrawSurface4))) {
        Logger::warn("QueryInterface: Query for legacy IDirectDrawSurface");
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

    Parent*    m_parent = nullptr;

    Com<D3D9>  m_d3d9;
    Com<DDraw> m_proxy;

  private:

    bool       m_forwardToProxy = false;

  };

}
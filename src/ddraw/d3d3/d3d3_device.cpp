#include "d3d3_device.h"

#include "../ddraw/ddraw_surface.h"

#include "../d3d5/d3d5_device.h"

#include "d3d3_viewport.h"

namespace dxvk {

  uint32_t D3D3Device::s_deviceCount = 0;

  D3D3Device::D3D3Device(
      Com<IDirect3DDevice>&& d3d5DeviceProxy,
      DDrawSurface* pParent,
      D3D5Device* origin)
    : DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9>(pParent, std::move(d3d5DeviceProxy), nullptr)
    , m_origin ( origin ) {
    m_deviceCount = ++s_deviceCount;

    Logger::debug(str::format("D3D3Device: Created a new device nr. ((1-", m_deviceCount, "))"));
  }

  D3D3Device::~D3D3Device() {
    Logger::debug(str::format("D3D3Device: Device nr. ((1-", m_deviceCount, ")) bites the dust"));
  }

  template<>
  IUnknown* DDrawWrappedObject<DDrawSurface, IDirect3DDevice, d3d9::IDirect3DDevice9>::GetInterface(REFIID riid) {
    if (riid == __uuidof(IUnknown))
      return this;
    if (riid == __uuidof(IDirect3DDevice))
      return this;

    throw DxvkError("D3D3Device::QueryInterface: Unknown interface query");
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      if (likely(m_parent != nullptr)) {
        Logger::debug("D3D3Device::QueryInterface: Query for IDirect3DDevice 2");
        m_parent->QueryInterface(riid, ppvObject);
      }

      Logger::warn("D3D3Device::QueryInterface: Query for IDirect3DDevice 2");
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

  HRESULT STDMETHODCALLTYPE D3D3Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    Logger::debug("<<< D3D3Device::GetCaps: Proxy");
    return m_proxy->GetCaps(hal_desc, hel_desc);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2) {
    Logger::warn("!!! D3D3Device::SwapTextureHandles: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetStats(D3DSTATS *stats) {
    Logger::debug("<<< D3D3Device::GetStats: Proxy");
    return m_proxy->GetStats(stats);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::AddViewport(IDirect3DViewport *viewport) {
    if (unlikely(m_origin != nullptr)) {
      Logger::debug(">>> D3D3Device::AddViewport: Forwarded");
      return m_origin->AddViewport(reinterpret_cast<IDirect3DViewport2*>(viewport));
    }

    Logger::debug("<<< D3D3Device::AddViewport: Proxy");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    return m_proxy->AddViewport(d3d3Viewport->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteViewport(IDirect3DViewport *viewport) {
    if (unlikely(m_origin != nullptr)) {
      Logger::debug(">>> D3D3Device::DeleteViewport: Forwarded");
      return m_origin->DeleteViewport(reinterpret_cast<IDirect3DViewport2*>(viewport));
    }

    Logger::debug("<<< D3D3Device::DeleteViewport: Proxy");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    return m_proxy->DeleteViewport(d3d3Viewport->GetProxied());
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::NextViewport(IDirect3DViewport *ref, IDirect3DViewport **viewport, DWORD flags) {
    Logger::warn("!!! D3D3Device::NextViewport: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx) {
    Logger::debug("<<< D3D3Device::EnumTextureFormats: Proxy");
    return m_proxy->EnumTextureFormats(cb, ctx);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::BeginScene() {
    Logger::debug("<<< D3D3Device::BeginScene: Proxy");
    return m_proxy->BeginScene();
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::EndScene() {
    Logger::debug("<<< D3D3Device::EndScene: Proxy");
    return m_proxy->EndScene();
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetDirect3D(IDirect3D **d3d) {
    Logger::warn("<<< D3D3Device::GetDirect3D: Proxy");
    return m_proxy->GetDirect3D(d3d);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc) {
    Logger::debug("<<< D3D3Device::Initialize: Proxy");
    return m_proxy->Initialize(d3d, lpGUID, desc);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter) {
    Logger::warn("<<< D3D3Device::CreateExecuteBuffer: Proxy");
    return m_proxy->CreateExecuteBuffer(desc, buffer, pkOuter);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags) {
    Logger::warn("<<< D3D3Device::Execute: Proxy");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    return m_proxy->Execute(buffer, d3d3Viewport->GetProxied(), flags);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect) {
    Logger::warn("<<< D3D3Device::Pick: Proxy");

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    D3D3Viewport* d3d3Viewport = static_cast<D3D3Viewport*>(viewport);
    return m_proxy->Pick(buffer, d3d3Viewport->GetProxied(), flags, rect);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetPickRecords(DWORD *count, D3DPICKRECORD *records) {
    Logger::debug("<<< D3D3Device::GetPickRecords: Proxy");
    return m_proxy->GetPickRecords(count, records);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::CreateMatrix(D3DMATRIXHANDLE *matrix) {
    Logger::debug("<<< D3D3Device::CreateMatrix: Proxy");
    return m_proxy->CreateMatrix(matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug("<<< D3D3Device::SetMatrix: Proxy");
    return m_proxy->SetMatrix(handle, matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix) {
    Logger::debug("<<< D3D3Device::GetMatrix: Proxy");
    return m_proxy->GetMatrix(handle, matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D3Device::DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle) {
    Logger::debug("<<< D3D3Device::GetMatrix: Proxy");
    return m_proxy->DeleteMatrix(D3DMatHandle);
  }

}